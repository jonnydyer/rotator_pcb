#include <Arduino.h>
#include <SPIFFS.h>
#include <ESP32Encoder.h>
#include <driver/mcpwm.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "wifi_manager.h"
#include "neopixel.h"
#include "rotator.h"
#include "main.h"

#define USER_LED_PIN 12
#define M1A_PIN 15
#define M1B_PIN 16
#define M2A_PIN 38
#define M2B_PIN 39
#define E1A_PIN 17
#define E1B_PIN 18
#define E2A_PIN 40
#define E2B_PIN 41
#define NEOPIX_PIN 13

// MCPWM configuration
#define MCPWM_FREQ 20000     // PWM frequency in Hz
#define MCPWM_RESOLUTION 16   // PWM resolution (16-bit = 0-65535)
#define MCPWM_TIMER_M1 MCPWM_TIMER_0
#define MCPWM_TIMER_M2 MCPWM_TIMER_1
#define MCPWM_UNIT MCPWM_UNIT_0

// Function prototypes
void setup_pins();
void setup_serial();
void setup_quadrature_encoders();
void setup_mcpwm();
void setup_timers();
void setup_spiffs();
void set_motor1_speed(float speed);
void set_motor2_speed(float speed);
void disable_motors();
void toggle_led(void* arg);
void update_encoder_status(void* arg);
void update_motion_control(void* arg);
void check_auto_rotation(void* arg);
void send_debug_data_timer(void* arg);
float generate_trapezoidal_profile(int32_t current_position, int32_t target_position, float current_velocity, float max_speed, float acceleration, unsigned long dt_ms);

// Global variables
ESP32Encoder encoder1;
ESP32Encoder encoder2;

// ESP Timer handles
esp_timer_handle_t led_timer;
esp_timer_handle_t encoder_timer;
esp_timer_handle_t motion_control_timer;
esp_timer_handle_t auto_rotation_timer;
esp_timer_handle_t debug_timer;

// Variables for velocity calculation
volatile int32_t last_encoder_count = 0;
volatile float encoder_velocity = 0; // counts per second
volatile unsigned long last_velocity_calc_time = 0;

// LED state
volatile bool led_state = false;

// Motion control variables
volatile bool motion_active = false;
volatile int32_t target_position = 0;
volatile float max_velocity = DEFAULT_MAX_SPEED;
volatile float acceleration = DEFAULT_ACCELERATION;
volatile float current_setpoint_velocity = 0;
volatile unsigned long last_motion_update_time = 0;

// Motion control parameters (module-level variables)
static uint32_t motion_position_hysteresis = DEFAULT_POSITION_HYSTERESIS;
static float motion_max_speed = DEFAULT_MAX_SPEED;
static float motion_acceleration = DEFAULT_ACCELERATION;
static float motion_vel_loop_p = DEFAULT_VEL_LOOP_P;
static float motion_vel_loop_i = DEFAULT_VEL_LOOP_I;
static float motion_vel_loop_d = DEFAULT_VEL_LOOP_D;
static float motion_vel_filter_persistence = DEFAULT_VEL_FILTER_PERSISTENCE;
static float motion_spd_err_persistence = DEFAULT_SPD_ERR_PERSISTENCE;
static int32_t full_revolution_count = 0;  // Full revolution in encoder counts for unwrapping

// Debug variables for WebSocket streaming
float debug_speed_error = 0.0f;
float debug_speed_error_integral = 0.0f;
float debug_speed_error_derivative = 0.0f;

void setup() {
  // Initialize the system
  setup_pins();
  disable_motors();
  setup_serial();
  
  // Setup file system and configuration
  setup_spiffs();
  loadConfiguration();
  
  // Initialize motion control parameters from configuration
  setMotionControlConfig(config.position_hysteresis, config.max_speed, config.acceleration,
                        config.vel_loop_p, config.vel_loop_i, config.vel_loop_d,
                        config.vel_filter_persistence, config.spd_err_persistence);
  
  // Initialize calibration-based parameters
  updateMotionControlCalibration();
  
  // Initialize components
  setup_quadrature_encoders();
  setupNeoPixel();
  setup_mcpwm();
  setup_timers();
  setupRotator();
  
  // Start WiFi and web server
  startWiFiAP();
  setupCaptivePortal();
  setupWebServer();
  setupOTA();
  
  //delay(500);
  
  // Log system information
  log_i("Total heap: %d", ESP.getHeapSize());
  log_i("Free heap: %d", ESP.getFreeHeap());
  log_i("Total PSRAM: %d", ESP.getPsramSize());
  log_i("Free PSRAM: %d", ESP.getFreePsram());
  log_i("SPIFFS size: %d", SPIFFS.totalBytes());
  log_i("SPIFFS used: %d", SPIFFS.usedBytes()); 
  log_i("Flash chip size: %d", ESP.getFlashChipSize());
  log_i("Flash chip real size: %d", ESP.getFlashChipSize());
  log_i("Flash chip speed: %d", ESP.getFlashChipSpeed()); 
  
  log_i("Rotator ready!");
}

void setup_spiffs() {
  if (!SPIFFS.begin(true)) {
    log_e("SPIFFS mount failed");
  } else {
    log_i("SPIFFS mounted successfully");
  }
}

void setup_mcpwm() {
  // Motor 1 MCPWM configuration
  mcpwm_gpio_init(MCPWM_UNIT, MCPWM0A, M1A_PIN);
  mcpwm_gpio_init(MCPWM_UNIT, MCPWM0B, M1B_PIN);
  
  // Motor 2 MCPWM configuration
  mcpwm_gpio_init(MCPWM_UNIT, MCPWM1A, M2A_PIN);
  mcpwm_gpio_init(MCPWM_UNIT, MCPWM1B, M2B_PIN);
  
  // Set MCPWM parameters
  mcpwm_config_t pwm_config;
  pwm_config.frequency = MCPWM_FREQ;
  pwm_config.cmpr_a = 0;
  pwm_config.cmpr_b = 0;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  
  // Configure MCPWM timers
  mcpwm_init(MCPWM_UNIT, MCPWM_TIMER_M1, &pwm_config);
  mcpwm_init(MCPWM_UNIT, MCPWM_TIMER_M2, &pwm_config);
  
  log_i("MCPWM initialized");
}

void setup_timers() {
  // LED blink timer
  esp_timer_create_args_t led_timer_config = {};
  led_timer_config.callback = &toggle_led;
  led_timer_config.name = "led_timer";
  
  ESP_ERROR_CHECK(esp_timer_create(&led_timer_config, &led_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(led_timer, LED_BLINK_INTERVAL_MS * 1000));
  
  // Encoder update timer
  esp_timer_create_args_t encoder_timer_config = {};
  encoder_timer_config.callback = &update_encoder_status;
  encoder_timer_config.name = "encoder_timer";
  
  ESP_ERROR_CHECK(esp_timer_create(&encoder_timer_config, &encoder_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(encoder_timer, ENCODER_UPDATE_INTERVAL_MS * 1000));
  
  // Motion control timer
  esp_timer_create_args_t motion_timer_config = {};
  motion_timer_config.callback = &update_motion_control;
  motion_timer_config.name = "motion_control_timer";
  
  ESP_ERROR_CHECK(esp_timer_create(&motion_timer_config, &motion_control_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(motion_control_timer, MOTION_CONTROL_INTERVAL_MS * 1000));
  
  // Auto rotation check timer
  esp_timer_create_args_t auto_rotation_timer_config = {};
  auto_rotation_timer_config.callback = &check_auto_rotation;
  auto_rotation_timer_config.name = "auto_rotation_timer";
  
  ESP_ERROR_CHECK(esp_timer_create(&auto_rotation_timer_config, &auto_rotation_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(auto_rotation_timer, AUTO_ROTATION_CHECK_INTERVAL_MS * 1000));
  
  // Debug data streaming timer
  esp_timer_create_args_t debug_timer_config = {};
  debug_timer_config.callback = &send_debug_data_timer;
  debug_timer_config.name = "debug_timer";
  
  ESP_ERROR_CHECK(esp_timer_create(&debug_timer_config, &debug_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(debug_timer, DEBUG_SEND_INTERVAL_MS * 1000));
  
  log_i("Timers initialized");
}

void loop() {
  // Process DNS requests for captive portal
  handleDNS();
  
  // Main loop is now mostly empty as the work is done by timers and async handlers
  delay(10); // Short delay to prevent watchdog timeouts
}

void setup_pins() {
  pinMode(USER_LED_PIN, OUTPUT);       // User LED for debugging
  pinMode(NEOPIX_PIN, OUTPUT);         // NeoPixel LED Data output
  
  // Encoder pins
  pinMode(E1A_PIN, INPUT);             // Encoder 1 A
  pinMode(E1B_PIN, INPUT);             // Encoder 1 B
  pinMode(E2A_PIN, INPUT);             // Encoder 2 A
  pinMode(E2B_PIN, INPUT);             // Encoder 2 B
}

void setup_serial() {
  Serial.begin(460800);
  //while (!Serial) {
  //  ;  // wait for serial port to connect. Needed for native USB port only
  //}
  Serial.setDebugOutput(true);
  log_i("Serial initialized");
}

void setup_quadrature_encoders() {
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  
  // Encoder 1
  encoder1.attachFullQuad(E1A_PIN, E1B_PIN);
  encoder1.setCount(0);
  
  // Encoder 2
  encoder2.attachFullQuad(E2A_PIN, E2B_PIN);
  encoder2.setCount(0);
  
  last_velocity_calc_time = millis();
  last_motion_update_time = millis();
  
  log_i("Encoders initialized");
}

void IRAM_ATTR toggle_led(void* arg) {
  led_state = !led_state;
  digitalWrite(USER_LED_PIN, led_state);
}

void IRAM_ATTR update_encoder_status(void* arg) {
  int32_t current_count = encoder1.getCount();
  unsigned long current_time = millis();
  unsigned long time_diff = current_time - last_velocity_calc_time;
  static float last_encoder_velocity = 0;
  
  // Calculate velocity in counts per second
  if (time_diff > 0) {
    encoder_velocity = (1 - motion_vel_filter_persistence) * ((float)(current_count - last_encoder_count) * 1000.0) / time_diff + last_encoder_velocity * motion_vel_filter_persistence;
    last_encoder_velocity = encoder_velocity;
  }
  
  // Update values for next calculation
  last_encoder_count = current_count;
  last_velocity_calc_time = current_time;
}

void IRAM_ATTR update_motion_control(void* arg) {
  if (!motion_active) {
    return;
  }

  static float speed_error_integral = 0;
  static float speed_error_previous = 0;
  static float last_target_velocity = 0;
  static float last_speed_deriv_err = 0;
  int32_t current_position = encoder1.getCount();
  unsigned long current_time = millis();
  unsigned long dt_ms = current_time - last_motion_update_time;
  
  // Check if we've reached target position with hysteresis
  if (abs(current_position - target_position) <= motion_position_hysteresis) {
    // We've reached the target position, stop the motor
    set_motor1_speed(0);
    motion_active = false;
    
    // Reset PID control variables
    speed_error_integral = 0;
    speed_error_previous = 0;
    last_target_velocity = 0;
    last_speed_deriv_err = 0;
    
    // Reset debug variables
    debug_speed_error = 0.0f;
    debug_speed_error_integral = 0.0f;
    debug_speed_error_derivative = 0.0f;
    
    // Check if encoder unwrapping is needed
    if (full_revolution_count > 0) {
      int32_t distance_from_zero = abs(current_position - config.pos_0_degrees);
      
      log_i("Unwrap check: pos=%d, pos_0=%d, dist=%d, full_rev=%d", 
            current_position, config.pos_0_degrees, distance_from_zero, full_revolution_count);
      
      if (distance_from_zero > full_revolution_count) {
        // Determine unwrap direction
        int32_t unwrapped_position;
        if (current_position > config.pos_0_degrees) {
          // Wrapped in positive direction
          unwrapped_position = current_position - full_revolution_count;
        } else {
          // Wrapped in negative direction
          unwrapped_position = current_position + full_revolution_count;
        }
        
        encoder1.setCount(unwrapped_position);
        log_i("Encoder unwrapped: %d -> %d (full_rev: %d)", 
              current_position, unwrapped_position, full_revolution_count);
        
        // Update current_position for the log message below
        current_position = unwrapped_position;
      }
    } else {
      log_w("Unwrap skipped: full_revolution_count is %d", full_revolution_count);
    }
    
    log_i("Target position reached: %d (current: %d)", target_position, current_position);
    return;
  }
  
  // Generate velocity using trapezoidal profile
  float target_velocity = generate_trapezoidal_profile(
    current_position, 
    target_position, 
    last_target_velocity, 
    motion_max_speed, 
    motion_acceleration, 
    dt_ms
  );
  last_target_velocity = target_velocity;
  
  // PID controller for velocity
  float speed_error = target_velocity - encoder_velocity;
  speed_error_integral += speed_error * dt_ms / 1000.0;
  float speed_error_derivative = (1.0f - motion_spd_err_persistence) * (speed_error - speed_error_previous) / (dt_ms / 1000.0) + motion_spd_err_persistence * last_speed_deriv_err;
  last_speed_deriv_err = speed_error_derivative;
  speed_error_previous = speed_error;
  float motor_speed = constrain(motion_vel_loop_p * speed_error + motion_vel_loop_i * speed_error_integral + motion_vel_loop_d * speed_error_derivative, -1.0f, 1.0f);

  // Update debug variables for WebSocket streaming
  debug_speed_error = speed_error;
  debug_speed_error_integral = speed_error_integral;
  debug_speed_error_derivative = speed_error_derivative;

  // Log performance data (uncomment for debugging)
  // log_d("speed_err:%.3e,speed_int:%.1f,speed_deriv:%.3e,pwm_cmd:%.3f,target_vel:%.3f,encoder_cnt:%d,encoder_vel:%.3f,loop_time:%d",
  //       speed_error, speed_error_integral, speed_error_derivative, motor_speed, 
  //       target_velocity, encoder1.getCount(), encoder_velocity, dt_ms);

  // Apply motor speed
  set_motor1_speed(-motor_speed);
  
  // Update timing for next cycle
  last_motion_update_time = current_time;
}

void IRAM_ATTR check_auto_rotation(void* arg) {
  processAutoRotation();
}

float get_encoder_velocity() {
  return encoder_velocity;
}

/**
 * Initiates a motion to a target position using a trapezoidal velocity profile
 */
void move_to_position(int32_t position) {
  // Set motion parameters
  target_position = position;
  
  // Reset motion control timing
  last_motion_update_time = millis();
  
  // Activate motion control
  motion_active = true;
  
  log_i("Starting motion to position %d, max speed: %.2f, accel: %.2f", 
              position, motion_max_speed, motion_acceleration);
}

/**
 * Generates a trapezoidal velocity profile for smooth motion
 * Returns the target velocity at this point in time
 */
float generate_trapezoidal_profile(int32_t current_position, int32_t target_position, 
                                  float current_velocity, float max_speed, 
                                  float acceleration, unsigned long dt_ms) {
  // Calculate distance to target
  float distance_remaining = target_position - current_position;
  
  // Determine direction
  float direction = distance_remaining > 0 ? 1.0f : -1.0f;
  distance_remaining = abs(distance_remaining);
  
  // Calculate the distance needed to decelerate to stop
  float decel_distance = (current_velocity * current_velocity) / (1.0f * acceleration);
  
  float target_velocity;
  
  // Check if we need to start decelerating
  if (distance_remaining <= decel_distance) {
    // Deceleration phase
    target_velocity = max(0.0f, abs(current_velocity) - (acceleration * dt_ms / 1000.0f));
  } else {
    // Acceleration or constant velocity phase
    if (abs(current_velocity) < max_speed) {
      // Acceleration phase
      target_velocity = min(max_speed, abs(current_velocity) + (acceleration * dt_ms / 1000.0f));
    } else {
      // Constant velocity phase
      target_velocity = max_speed;
    }
  }
  
  // Apply direction
  return target_velocity * direction;
}

/**
 * Sets motor 1 speed using a float value between -1.0 and 1.0
 * where -1.0 is full reverse, 0.0 is stop, and 1.0 is full forward
 */
void set_motor1_speed(float speed) {
  // Constrain speed to valid range
  speed = constrain(speed, -1.0f, 1.0f);
  
  if (speed > 0) {
    // Forward direction: M1A high, M1B PWM
    mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_A, 100.0);
    mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_B, 100.0 - (speed * 100.0));
    mcpwm_set_duty_type(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty_type(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
  } else if (speed < 0) {
    // Reverse direction: M1A PWM, M1B high
    mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_B, 100.0);
    mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_A, 100.0 - (abs(speed) * 100.0));
    mcpwm_set_duty_type(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty_type(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
  } else {
    // Stop: Both outputs low (coast)
    mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_A, 0);
    mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_B, 0);
    mcpwm_set_duty_type(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty_type(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
  }
}

/**
 * Sets motor 2 speed using a float value between -1.0 and 1.0
 * where -1.0 is full reverse, 0.0 is stop, and 1.0 is full forward
 */
void set_motor2_speed(float speed) {
  // Constrain speed to valid range
  speed = constrain(speed, -1.0f, 1.0f);
  
  if (speed > 0) {
    // Forward direction: M2A high, M2B PWM
    mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_A, 100.0);
    mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_B, 100.0 - (speed * 100.0));
    mcpwm_set_duty_type(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty_type(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
  } else if (speed < 0) {
    // Reverse direction: M2A PWM, M2B high
    mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_B, 100.0);
    mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_A, 100.0 - (abs(speed) * 100.0));
    mcpwm_set_duty_type(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty_type(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
  } else {
    // Stop: Both outputs low (coast)
    mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_A, 0);
    mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_B, 0);
    mcpwm_set_duty_type(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty_type(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
  }
}

void disable_motors() {
  // Set both motor outputs to LOW (coast mode)
  mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_A, 0);
  mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M1, MCPWM_OPR_B, 0);
  mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_A, 0);
  mcpwm_set_duty(MCPWM_UNIT, MCPWM_TIMER_M2, MCPWM_OPR_B, 0);
}

void IRAM_ATTR send_debug_data_timer(void* arg) {
  sendDebugData();
}

/**
 * Get motion control configuration
 */
void getMotionControlConfig(uint32_t& position_hysteresis, float& max_speed, float& acceleration,
                           float& vel_loop_p, float& vel_loop_i, float& vel_loop_d,
                           float& vel_filter_persistence, float& spd_err_persistence) {
    position_hysteresis = motion_position_hysteresis;
    max_speed = motion_max_speed;
    acceleration = motion_acceleration;
    vel_loop_p = motion_vel_loop_p;
    vel_loop_i = motion_vel_loop_i;
    vel_loop_d = motion_vel_loop_d;
    vel_filter_persistence = motion_vel_filter_persistence;
    spd_err_persistence = motion_spd_err_persistence;
}

/**
 * Set motion control configuration
 */
void setMotionControlConfig(uint32_t position_hysteresis, float max_speed, float acceleration,
                           float vel_loop_p, float vel_loop_i, float vel_loop_d,
                           float vel_filter_persistence, float spd_err_persistence) {
    motion_position_hysteresis = position_hysteresis;
    motion_max_speed = max_speed;
    motion_acceleration = acceleration;
    motion_vel_loop_p = vel_loop_p;
    motion_vel_loop_i = vel_loop_i;
    motion_vel_loop_d = vel_loop_d;
    motion_vel_filter_persistence = vel_filter_persistence;
    motion_spd_err_persistence = spd_err_persistence;
    
    log_i("Motion control config updated: hysteresis=%u, max_speed=%.1f, accel=%.1f", 
          position_hysteresis, max_speed, acceleration);
    log_i("PID gains updated: P=%.2e, I=%.2e, D=%.2e", vel_loop_p, vel_loop_i, vel_loop_d);
    log_i("Filter paramters updated: velocity filter =%.2f, speed error filter=%.2f", vel_filter_persistence, spd_err_persistence);
}

/**
 * Set full revolution count for encoder unwrapping
 */
void setFullRevolutionCount(int32_t full_revolution) {
    full_revolution_count = full_revolution;
    log_i("Full revolution count set to: %d", full_revolution_count);
}

int32_t get_current_position() {
  return encoder1.getCount();
}

MotionControlInfo get_motion_control_info() {
  MotionControlInfo info;
  info.motion_active = motion_active;
  info.target_position = target_position;
  info.speed_error = debug_speed_error;
  info.speed_error_integral = debug_speed_error_integral;
  info.speed_error_derivative = debug_speed_error_derivative;
  info.velocity = encoder_velocity;
  return info;
}

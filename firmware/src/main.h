#ifndef MAIN_H
#define MAIN_H

#include <ESP32Encoder.h>

// Timer configuration
#define LED_BLINK_INTERVAL_MS 250
#define ENCODER_UPDATE_INTERVAL_MS 10
#define MOTION_CONTROL_INTERVAL_MS 10
#define AUTO_ROTATION_CHECK_INTERVAL_MS 1000
#define DEBUG_SEND_INTERVAL_MS 100       // 10Hz debug data streaming
#define MAX_MOTOR_PWM_DUTY_CYCLE 1.0f

// System state enumeration
enum SystemState {
    SYSTEM_BOOTING,           // Very fast blink during startup
    SYSTEM_WIFI_AP_MODE,      // Fast blink - looking for connection
    SYSTEM_WIFI_CONNECTING,   // Medium blink - attempting connection
    SYSTEM_WIFI_CONNECTED,    // Slow blink - connected and ready
    SYSTEM_WIFI_FAILED,       // Very fast blink - connection failed
    SYSTEM_ERROR,             // Solid on - system error
};

// Structure for motion control information
struct MotionControlInfo {
    bool motion_active;
    int64_t target_position;
    float velocity;
    float speed_error;
    float speed_error_integral;
    float speed_error_derivative;
    float pwm_control_out;
};

// Getter function declarations
int64_t get_current_position();
float get_encoder_velocity();
bool is_motion_active(void);
MotionControlInfo get_motion_control_info();

// Motion control configuration functions
void getMotionControlConfig(uint32_t& position_hysteresis, float& max_speed, float& acceleration,
                           float& vel_loop_p, float& vel_loop_i, float& vel_loop_d,
                           float& vel_filter_persistence, float& spd_err_persistence);
void setMotionControlConfig(uint32_t position_hysteresis, float max_speed, float acceleration,
                           float vel_loop_p, float vel_loop_i, float vel_loop_d,
                           float vel_filter_persistence, float spd_err_persistence);
void setFullRevolutionCount(int32_t full_revolution);

// Function prototypes
void move_to_position(int64_t target_position);

void reset_motor_control();

// LED control functions
void setLEDBlinkRate(uint32_t interval_ms);
void updateLEDStatus();
SystemState determineSystemState();

#endif

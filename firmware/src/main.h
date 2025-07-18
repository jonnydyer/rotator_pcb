#ifndef MAIN_H
#define MAIN_H

#include <ESP32Encoder.h>

// Timer configuration
#define LED_BLINK_INTERVAL_MS 250
#define ENCODER_UPDATE_INTERVAL_MS 10
#define MOTION_CONTROL_INTERVAL_MS 10
#define AUTO_ROTATION_CHECK_INTERVAL_MS 1000
#define DEBUG_SEND_INTERVAL_MS 100       // 10Hz debug data streaming

// Structure for motion control information
struct MotionControlInfo {
    bool motion_active;
    int32_t target_position;
    float velocity;
    float speed_error;
    float speed_error_integral;
    float speed_error_derivative;
};

// Getter function declarations
int32_t get_current_position();
float get_encoder_velocity();
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
void move_to_position(int32_t target_position);

#endif

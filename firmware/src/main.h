#ifndef MAIN_H
#define MAIN_H

// Timer configuration
#define LED_BLINK_INTERVAL_MS 250
#define ENCODER_UPDATE_INTERVAL_MS 10
#define MOTION_CONTROL_INTERVAL_MS 10
#define AUTO_ROTATION_CHECK_INTERVAL_MS 1000

// Motion control parameters
#define POSITION_HYSTERESIS 20           // Encoder count hysteresis around target position
#define DEFAULT_MAX_SPEED 6000           // Default max speed in counts/second
#define DEFAULT_ACCELERATION 4000        // Default acceleration in counts/second²
#define VEL_LOOP_P 3e-5 
#define VEL_LOOP_I 6e-3
#define VEL_LOOP_D -2e-8
#define VEL_FILTER_PERSISTENCE 0.0
#define SPD_ERR_PERSISTENCE 0.0

void move_to_position(int32_t target_position, float max_speed = DEFAULT_MAX_SPEED, float acceleration = DEFAULT_ACCELERATION);

#endif

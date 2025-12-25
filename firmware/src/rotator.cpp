#include "rotator.h"
#include "neopixel.h"
#include "main.h"


// Global rotator state
volatile bool auto_rotation_active = false;
volatile unsigned long last_rotation_time = 0;
volatile int32_t full_revolution_count = 0;

/**
 * Setup the rotator subsystem
 */
void setupRotator() {
    // Initialize the rotation timer
    last_rotation_time = millis();

    // Set initial color based on current position
    int currentAngle = positionToAngle(get_current_position());
    setNeoPixelForAngle(currentAngle);

    log_i("Rotator initialized. Current angle: %d degrees", currentAngle);
}

/**
 * Helper: Normalize any encoder position to [0, full_rotation_count)
 * Handles arbitrary positive and negative values
 */
static int64_t normalizePosition(int64_t position) {
    if (config.full_rotation_count <= 0) {
        log_e("full_rotation_count not calibrated!");
        return 0;
    }

    int64_t normalized;

    if (position >= 0) {
        // Positive case
        uint64_t full_rotations = position / config.full_rotation_count;
        normalized = position - (full_rotations * config.full_rotation_count);
    } else {
        // Negative case
        uint64_t full_rotations = (-position) / config.full_rotation_count;
        normalized = position + ((full_rotations + 1) * config.full_rotation_count);
    }

    // Ensure result is in [0, full_rotation_count)
    if (normalized >= config.full_rotation_count) {
        normalized = 0;
    }

    return normalized;
}

/**
 * Convert angle (0, 90, 180, 270) to encoder count offset
 */
int64_t angleToPositionOffset(int angle) {
    if (config.full_rotation_count <= 0) {
        log_e("full_rotation_count not calibrated!");
        return 0;
    }
    return (int64_t)angle * config.full_rotation_count / 360;
}

/**
 * Convert any encoder position to angle in degrees [0, 359]
 */
int positionToAngle(int64_t position) {
    if (config.full_rotation_count <= 0) {
        log_e("full_rotation_count not calibrated!");
        return 0;
    }

    int64_t normalized = normalizePosition(position);

    // Convert to angle in degrees
    int angle_degrees = (int)(normalized * 360 / config.full_rotation_count);

    return angle_degrees;
}

/**
 * Calculate signed circular distance from one position to another
 * Positive = forward rotation, Negative = backward rotation
 * Always returns the shortest path
 */
int64_t calculateSignedCircularDistance(int64_t from_position, int64_t to_position) {
    if (config.full_rotation_count <= 0) {
        log_e("full_rotation_count not calibrated!");
        return 0;
    }

    // Normalize both positions to [0, full_rotation_count)
    int64_t from_norm = normalizePosition(from_position);
    int64_t to_norm = normalizePosition(to_position);

    // Calculate direct distance
    int64_t direct_distance = to_norm - from_norm;

    // Calculate alternate distance (going the other way around)
    int64_t alternate_distance;
    if (direct_distance > 0) {
        alternate_distance = direct_distance - config.full_rotation_count;
    } else {
        alternate_distance = direct_distance + config.full_rotation_count;
    }

    // Return shortest distance (signed)
    if (llabs(direct_distance) <= llabs(alternate_distance)) {
        return direct_distance;
    } else {
        return alternate_distance;
    }
}

/**
 * Calculate the minimum circular distance between two encoder positions
 * Accounts for the circular nature of the encoder
 * DEPRECATED: Use calculateSignedCircularDistance() instead
 */
int64_t calculateCircularDistance(int64_t count, int64_t target_pos) {
    return llabs(calculateSignedCircularDistance(count, target_pos));
}


/**
 * Rotate to a specific angle (0, 90, 180, 270 degrees)
 * Takes the shortest path to the target angle
 */
void rotateToAngle(int angle) {
    int64_t currentPosition = get_current_position();

    // Convert target angle to position offset
    int64_t targetOffset = angleToPositionOffset(angle);

    // Calculate signed circular distance (shortest path)
    int64_t distance = calculateSignedCircularDistance(currentPosition, targetOffset);

    // Final target = current position + shortest distance
    int64_t finalTarget = currentPosition + distance;

    log_i("Rotating to %d°, encoder: %lld -> %lld (distance: %lld)",
          angle, currentPosition, finalTarget, distance);

    // Command the motor to move to the target position
    move_to_position(finalTarget);

    // Set the NeoPixel color based on the angle
    setNeoPixelForAngle(angle);

    // Update timing for auto-rotation
    last_rotation_time = millis();
}

/**
 * Process auto-rotation logic
 * Should be called periodically to check if it's time to rotate
 */
void processAutoRotation() {
    MotionControlInfo motionInfo = get_motion_control_info();
    if (!config.auto_rotation_enabled || motionInfo.motion_active) {
        return;
    }
    
    unsigned long currentTime = millis();
    unsigned long elapsedTime = (currentTime - last_rotation_time) / 1000; // in seconds
    
    if (elapsedTime >= config.rotation_interval) {
        log_i("Auto-rotation triggered after %u seconds", elapsedTime);
        moveToNextPosition();
    }
}

/**
 * Move to the next 90-degree position in sequence
 */
void moveToNextPosition() {
    int nextAngle;

    int currentAngle = positionToAngle(get_current_position());

    // Round to nearest 90-degree increment
    int currentAngleSnapped = ((currentAngle + 45) / 90) * 90;
    if (currentAngleSnapped >= 360) currentAngleSnapped = 0;

    if(config.auto_rotate_forward){
        nextAngle = (currentAngleSnapped + 90) % 360;
    }
    else{
        nextAngle = (currentAngleSnapped + 360 - 90) % 360;
    }

    rotateToAngle(nextAngle);
}

/**
 * Set the NeoPixel color based on the current angle
 */
void setNeoPixelForAngle(int angle) {
    uint32_t color;
    
    switch (angle) {
        case 0:
            color = config.color_0;
            break;
        case 90:
            color = config.color_90;
            break;
        case 180:
            color = config.color_180;
            break;
        case 270:
            color = config.color_270;
            break;
        default:
            return;
    }
    
    // Set the NeoPixel color
    setNeoPixelColor(color);
}

/**
 * Update motion control calibration parameters
 * Calculates full revolution count from calibration data
 */
void updateMotionControlCalibration() {
    // Calculate full revolution from calibration data using 270° span
    full_revolution_count = abs(config.pos_270_degrees - config.pos_0_degrees) * 4 / 3;
    
    log_i("Motion control calibration updated - Full revolution: %d counts", full_revolution_count);
} 
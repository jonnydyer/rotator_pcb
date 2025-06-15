#include "rotator.h"
#include "neopixel.h"
#include "main.h"


// Global rotator state
volatile bool auto_rotation_active = false;
volatile unsigned long last_rotation_time = 0;

/**
 * Setup the rotator subsystem
 */
void setupRotator() {
    // Initialize the rotation timer
    last_rotation_time = millis();
    
    // Set initial color based on current position
    int currentAngle = calculateCurrentAngle();
    setNeoPixelForAngle(currentAngle);
    
    log_i("Rotator initialized. Current angle: %d degrees", currentAngle);
}

/**
 * Calculate the current angular position based on encoder count
 */
int calculateCurrentAngle() {
    int32_t count = encoder1.getCount();
    
    // Find the closest angle
    int32_t dist_0 = abs(count - config.pos_0_degrees);
    int32_t dist_90 = abs(count - config.pos_90_degrees);
    int32_t dist_180 = abs(count - config.pos_180_degrees);
    int32_t dist_270 = abs(count - config.pos_270_degrees);
    
    if (dist_0 <= dist_90 && dist_0 <= dist_180 && dist_0 <= dist_270) {
        return 0;
    } else if (dist_90 <= dist_0 && dist_90 <= dist_180 && dist_90 <= dist_270) {
        return 90;
    } else if (dist_180 <= dist_0 && dist_180 <= dist_90 && dist_180 <= dist_270) {
        return 180;
    } else {
        return 270;
    }
}

/**
 * Rotate to a specific angle (0, 90, 180, 270 degrees)
 * Takes the shortest path to the target angle
 */
void rotateToAngle(int angle) {
    int currentAngle = calculateCurrentAngle();
    int32_t targetPosition;
    
    // Determine target position
    switch (angle) {
        case 0:
            targetPosition = config.pos_0_degrees;
            break;
        case 90:
            targetPosition = config.pos_90_degrees;
            break;
        case 180:
            targetPosition = config.pos_180_degrees;
            break;
        case 270:
            targetPosition = config.pos_270_degrees;
            break;
        default:
            log_e("Invalid angle: %d", angle);
            return;
    }
    
    int32_t currentPosition = encoder1.getCount();
    
    // Determine the shortest path
    // Calculate full 360 revolution in encoder counts
    int32_t fullRevolution = (config.pos_270_degrees - config.pos_0_degrees) * 4/3;
    if (fullRevolution < 0) fullRevolution = -fullRevolution;
    
    // Calculate distances in both directions
    int32_t directDistance = targetPosition - currentPosition;
    int32_t alternateDistance = (directDistance > 0) ? 
                                directDistance - fullRevolution : 
                                directDistance + fullRevolution;
    
    // Choose the shortest path
    int32_t finalTarget = (abs(directDistance) <= abs(alternateDistance)) ? 
                          targetPosition : 
                          (directDistance > 0) ? targetPosition - fullRevolution : targetPosition + fullRevolution;
    
    log_i("Rotating from %d° to %d°, encoder: %d -> %d", 
          currentAngle, angle, currentPosition, finalTarget);
    
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
    if (!config.auto_rotation_enabled || motion_active) {
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
    int currentAngle = calculateCurrentAngle();
    int nextAngle = (currentAngle + 90) % 360;
    
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
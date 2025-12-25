#ifndef ROTATOR_H
#define ROTATOR_H

#include <Arduino.h>
#include <ESP32Encoder.h>
#include "config.h"

// External declarations
extern ESP32Encoder encoder1;
extern volatile bool motion_active;

// Function prototypes
void setupRotator();
void rotateToAngle(int angle);
void processAutoRotation();
void moveToNextPosition();
void setNeoPixelForAngle(int angle);
void updateMotionControlCalibration();

// Helper functions for angle/position conversion
int64_t angleToPositionOffset(int angle);
int positionToAngle(int64_t position);
int64_t calculateSignedCircularDistance(int64_t from_position, int64_t to_position);

// DEPRECATED: Use calculateSignedCircularDistance() instead
int64_t calculateCircularDistance(int64_t count, int64_t target_pos);

// Rotator state
extern volatile bool auto_rotation_active;
extern volatile unsigned long last_rotation_time;

#endif // ROTATOR_H 
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
int calculateCurrentAngle();
int32_t calculateCircularDistance(int32_t count, int32_t target_pos);
void rotateToAngle(int angle);
void processAutoRotation();
void moveToNextPosition();
void setNeoPixelForAngle(int angle);
void updateMotionControlCalibration();

// Rotator state
extern volatile bool auto_rotation_active;
extern volatile unsigned long last_rotation_time;

#endif // ROTATOR_H 
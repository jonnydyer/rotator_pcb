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
void rotateToAngle(int angle);
void processAutoRotation();
void moveToNextPosition();
void setNeoPixelForAngle(int angle);

// Rotator state
extern volatile bool auto_rotation_active;
extern volatile unsigned long last_rotation_time;

#endif // ROTATOR_H 
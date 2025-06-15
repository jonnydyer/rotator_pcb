#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

// Pin definitions
#define NEOPIX_PIN 13

// Function prototypes
void setupNeoPixel();
void setNeoPixelColor(uint32_t color);
void setNeoPixelBrightness(uint8_t brightness);
uint32_t convertRGB(uint8_t r, uint8_t g, uint8_t b);

// Global NeoPixel instance
extern Adafruit_NeoPixel neopixel;

#endif // NEOPIXEL_H 
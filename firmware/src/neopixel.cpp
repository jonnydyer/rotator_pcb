#include "neopixel.h"

// Create NeoPixel instance - 1 pixel on specified pin
Adafruit_NeoPixel neopixel(1, NEOPIX_PIN, NEO_GRB + NEO_KHZ800);

/**
 * Initialize the NeoPixel
 */
void setupNeoPixel() {
    neopixel.begin();
    neopixel.setBrightness(50); // Set to 50% brightness
    neopixel.clear();
    neopixel.show();
    log_i("NeoPixel initialized");
}

/**
 * Set the NeoPixel color using a 32-bit packed color value
 */
void setNeoPixelColor(uint32_t color) {
    neopixel.setPixelColor(0, color);
    neopixel.show();
}

/**
 * Set the NeoPixel brightness (0-255)
 */
void setNeoPixelBrightness(uint8_t brightness) {
    neopixel.setBrightness(brightness);
    neopixel.show();
}

/**
 * Convert RGB values to a 32-bit packed color value
 */
uint32_t convertRGB(uint8_t r, uint8_t g, uint8_t b) {
    return neopixel.Color(r, g, b);
} 
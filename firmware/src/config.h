#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

// Default WiFi settings
#define DEFAULT_AP_SSID "RotatorAP"
#define DEFAULT_AP_PASSWORD "rotator1234"

// Motor positions in encoder counts

#define POS_0_DEGREES 0
#define POS_90_DEGREES 39375  // Assuming 39375 counts for 90 degree rotation
#define POS_180_DEGREES 78750
#define POS_270_DEGREES 118125

// NEOPIXEL settings
#define DEFAULT_COLOR_0 0x00FF00    // Green
#define DEFAULT_COLOR_90 0xFF0000   // Red
#define DEFAULT_COLOR_180 0x0000FF  // Blue
#define DEFAULT_COLOR_270 0xFFFF00  // Yellow

// Timer settings
#define DEFAULT_ROTATION_INTERVAL 60 // seconds between auto-rotation

// Configuration file path
#define CONFIG_FILE "/config.json"

// Structure to hold all configuration data
struct RotatorConfig {
    // WiFi settings
    char ap_ssid[32];
    char ap_password[64];
    
    // Motor position calibration (encoder counts)
    int32_t pos_0_degrees;
    int32_t pos_90_degrees;
    int32_t pos_180_degrees;
    int32_t pos_270_degrees;
    
    // Color settings for each position
    uint32_t color_0;
    uint32_t color_90;
    uint32_t color_180;
    uint32_t color_270;
    
    // Rotation settings
    uint32_t rotation_interval; // seconds
    bool auto_rotation_enabled;
};

// Global configuration object
extern RotatorConfig config;

// Function prototypes
bool loadConfiguration();
bool saveConfiguration();
void resetToDefaultConfig();

#endif // CONFIG_H 
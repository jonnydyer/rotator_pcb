#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

// Default WiFi settings
#define DEFAULT_AP_SSID "RotatorAP"
#define DEFAULT_AP_PASSWORD "rotator1234"

// Default WiFi client settings
#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASSWORD ""
#define DEFAULT_WIFI_CLIENT_ENABLED false
#define DEFAULT_WIFI_CONNECTION_TIMEOUT 5 // seconds
#define DEFAULT_MDNS_NAME "" // Will be set to "rotator-XXXX" where XXXX is last 4 MAC digits

// Motor positions in encoder counts

#define POS_0_DEGREES 0
#define POS_90_DEGREES 39375  // Assuming 39375 counts for 90 degree rotation
#define POS_180_DEGREES 78750
#define POS_270_DEGREES 118125
#define FULL_ROTATION_COUNT 157500

// NEOPIXEL settings
#define DEFAULT_COLOR_0 0x00FF00    // Green
#define DEFAULT_COLOR_90 0xFF0000   // Red
#define DEFAULT_COLOR_180 0x0000FF  // Blue
#define DEFAULT_COLOR_270 0xFFFF00  // Yellow

// Timer settings
#define DEFAULT_ROTATION_INTERVAL 60 // seconds between auto-rotation

// Motion control default values
#define DEFAULT_POSITION_HYSTERESIS 20
#define DEFAULT_MAX_SPEED 4000.0f
#define DEFAULT_ACCELERATION 2000.0f
#define DEFAULT_VEL_LOOP_P 3e-5f
#define DEFAULT_VEL_LOOP_I 6e-3f
#define DEFAULT_VEL_LOOP_D -2e-8f
#define DEFAULT_VEL_FILTER_PERSISTENCE 0.2f
#define DEFAULT_SPD_ERR_PERSISTENCE 0.2f

// Configuration file path
#define CONFIG_FILE "/config.json"

// Structure to hold all configuration data
struct RotatorConfig {
    // WiFi AP settings
    char ap_ssid[32];
    char ap_password[64];
    
    // WiFi client settings
    char wifi_ssid[32];
    char wifi_password[64];
    bool wifi_client_enabled;
    uint32_t wifi_connection_timeout; // seconds
    char mdns_name[32]; // mDNS hostname
    
    // Motor position calibration (encoder counts)
    int32_t pos_0_degrees;
    int32_t pos_90_degrees;
    int32_t pos_180_degrees;
    int32_t pos_270_degrees;

    int32_t full_rotation_count;
    
    // Color settings for each position
    uint32_t color_0;
    uint32_t color_90;
    uint32_t color_180;
    uint32_t color_270;
    
    // Rotation settings
    uint32_t rotation_interval; // seconds
    bool auto_rotation_enabled;
    
    // Motion control parameters
    uint32_t position_hysteresis;
    float max_speed;
    float acceleration;
    float vel_loop_p;
    float vel_loop_i;
    float vel_loop_d;
    float vel_filter_persistence;
    float spd_err_persistence;
};

// Global configuration object
extern RotatorConfig config;

// Function prototypes
bool loadConfiguration();
bool saveConfiguration();
void resetToDefaultConfig();
void generateMDNSName();

#endif // CONFIG_H 
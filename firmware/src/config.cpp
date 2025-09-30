#include "config.h"
#include "main.h"
#include "rotator.h"
#include <WiFi.h>

// Global configuration instance
RotatorConfig config;

/**
 * Reset configuration to factory defaults
 */
void resetToDefaultConfig() {
    // WiFi AP settings
    strncpy(config.ap_ssid, DEFAULT_AP_SSID, sizeof(config.ap_ssid));
    strncpy(config.ap_password, DEFAULT_AP_PASSWORD, sizeof(config.ap_password));
    
    // WiFi client settings
    strncpy(config.wifi_ssid, DEFAULT_WIFI_SSID, sizeof(config.wifi_ssid));
    strncpy(config.wifi_password, DEFAULT_WIFI_PASSWORD, sizeof(config.wifi_password));
    config.wifi_client_enabled = DEFAULT_WIFI_CLIENT_ENABLED;
    config.wifi_connection_timeout = DEFAULT_WIFI_CONNECTION_TIMEOUT;
    
    // Generate mDNS name from MAC address
    generateMDNSName();
    
    // Motor positions
    config.pos_0_degrees = POS_0_DEGREES;
    config.pos_90_degrees = POS_90_DEGREES;
    config.pos_180_degrees = POS_180_DEGREES;
    config.pos_270_degrees = POS_270_DEGREES;
    
    // NeoPixel colors
    config.color_0 = DEFAULT_COLOR_0;
    config.color_90 = DEFAULT_COLOR_90;
    config.color_180 = DEFAULT_COLOR_180;
    config.color_270 = DEFAULT_COLOR_270;
    
    // Rotation settings
    config.rotation_interval = DEFAULT_ROTATION_INTERVAL;
    config.auto_rotation_enabled = false;
    
    // Motion control parameters
    config.position_hysteresis = DEFAULT_POSITION_HYSTERESIS;
    config.max_speed = DEFAULT_MAX_SPEED;
    config.acceleration = DEFAULT_ACCELERATION;
    config.vel_loop_p = DEFAULT_VEL_LOOP_P;
    config.vel_loop_i = DEFAULT_VEL_LOOP_I;
    config.vel_loop_d = DEFAULT_VEL_LOOP_D;
    config.vel_filter_persistence = DEFAULT_VEL_FILTER_PERSISTENCE;
    config.spd_err_persistence = DEFAULT_SPD_ERR_PERSISTENCE;
    
    // Save to file
    saveConfiguration();
    
    // Update runtime motion control parameters
    setMotionControlConfig(config.position_hysteresis, config.max_speed, config.acceleration,
                          config.vel_loop_p, config.vel_loop_i, config.vel_loop_d,
                          config.vel_filter_persistence, config.spd_err_persistence);
    
    // Update calibration-based parameters
    updateMotionControlCalibration();
}

/**
 * Load configuration from SPIFFS
 */
bool loadConfiguration() {
    if (!SPIFFS.exists(CONFIG_FILE)) {
        log_i("Configuration file not found, creating default");
        resetToDefaultConfig();
        return false;
    }
    
    File file = SPIFFS.open(CONFIG_FILE, "r");
    if (!file) {
        log_e("Failed to open config file for reading");
        return false;
    }
    
    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        log_e("Failed to parse config file: %s", error.c_str());
        return false;
    }
    
    // WiFi AP settings
    strlcpy(config.ap_ssid, 
            doc["ap_ssid"] | DEFAULT_AP_SSID, 
            sizeof(config.ap_ssid));
    strlcpy(config.ap_password, 
            doc["ap_password"] | DEFAULT_AP_PASSWORD, 
            sizeof(config.ap_password));
    
    // WiFi client settings
    strlcpy(config.wifi_ssid, 
            doc["wifi_ssid"] | DEFAULT_WIFI_SSID, 
            sizeof(config.wifi_ssid));
    strlcpy(config.wifi_password, 
            doc["wifi_password"] | DEFAULT_WIFI_PASSWORD, 
            sizeof(config.wifi_password));
    config.wifi_client_enabled = doc["wifi_client_enabled"] | DEFAULT_WIFI_CLIENT_ENABLED;
    config.wifi_connection_timeout = doc["wifi_connection_timeout"] | DEFAULT_WIFI_CONNECTION_TIMEOUT;
    
    // mDNS name - generate if not set or empty
    const char* saved_mdns = doc["mdns_name"];
    if (saved_mdns && strlen(saved_mdns) > 0) {
        strlcpy(config.mdns_name, saved_mdns, sizeof(config.mdns_name));
    } else {
        generateMDNSName();
    }
    
    // Motor positions
    config.pos_0_degrees = doc["pos_0_degrees"] | POS_0_DEGREES;
    config.pos_90_degrees = doc["pos_90_degrees"] | POS_90_DEGREES;
    config.pos_180_degrees = doc["pos_180_degrees"] | POS_180_DEGREES;
    config.pos_270_degrees = doc["pos_270_degrees"] | POS_270_DEGREES;
    
    // NeoPixel colors
    config.color_0 = doc["color_0"] | DEFAULT_COLOR_0;
    config.color_90 = doc["color_90"] | DEFAULT_COLOR_90;
    config.color_180 = doc["color_180"] | DEFAULT_COLOR_180;
    config.color_270 = doc["color_270"] | DEFAULT_COLOR_270;
    
    // Rotation settings
    config.rotation_interval = doc["rotation_interval"] | DEFAULT_ROTATION_INTERVAL;
    config.auto_rotation_enabled = doc["auto_rotation_enabled"] | false;
    
    // Motion control parameters
    config.position_hysteresis = doc["position_hysteresis"] | DEFAULT_POSITION_HYSTERESIS;
    config.max_speed = doc["max_speed"] | DEFAULT_MAX_SPEED;
    config.acceleration = doc["acceleration"] | DEFAULT_ACCELERATION;
    config.vel_loop_p = doc["vel_loop_p"] | DEFAULT_VEL_LOOP_P;
    config.vel_loop_i = doc["vel_loop_i"] | DEFAULT_VEL_LOOP_I;
    config.vel_loop_d = doc["vel_loop_d"] | DEFAULT_VEL_LOOP_D;
    config.vel_filter_persistence = doc["vel_filter_persistence"] | DEFAULT_VEL_FILTER_PERSISTENCE;
    config.spd_err_persistence = doc["spd_err_persistence"] | DEFAULT_SPD_ERR_PERSISTENCE;
    
    log_i("Configuration loaded successfully");
    return true;
}

/**
 * Save configuration to SPIFFS
 */
bool saveConfiguration() {
    StaticJsonDocument<1536> doc;
    
    // WiFi AP settings
    doc["ap_ssid"] = config.ap_ssid;
    doc["ap_password"] = config.ap_password;
    
    // WiFi client settings
    doc["wifi_ssid"] = config.wifi_ssid;
    doc["wifi_password"] = config.wifi_password;
    doc["wifi_client_enabled"] = config.wifi_client_enabled;
    doc["wifi_connection_timeout"] = config.wifi_connection_timeout;
    doc["mdns_name"] = config.mdns_name;
    
    // Motor positions
    doc["pos_0_degrees"] = config.pos_0_degrees;
    doc["pos_90_degrees"] = config.pos_90_degrees;
    doc["pos_180_degrees"] = config.pos_180_degrees;
    doc["pos_270_degrees"] = config.pos_270_degrees;
    
    // NeoPixel colors
    doc["color_0"] = config.color_0;
    doc["color_90"] = config.color_90;
    doc["color_180"] = config.color_180;
    doc["color_270"] = config.color_270;
    
    // Rotation settings
    doc["rotation_interval"] = config.rotation_interval;
    doc["auto_rotation_enabled"] = config.auto_rotation_enabled;
    
    // Motion control parameters
    doc["position_hysteresis"] = config.position_hysteresis;
    doc["max_speed"] = config.max_speed;
    doc["acceleration"] = config.acceleration;
    doc["vel_loop_p"] = config.vel_loop_p;
    doc["vel_loop_i"] = config.vel_loop_i;
    doc["vel_loop_d"] = config.vel_loop_d;
    doc["vel_filter_persistence"] = config.vel_filter_persistence;
    doc["spd_err_persistence"] = config.spd_err_persistence;
    
    File file = SPIFFS.open(CONFIG_FILE, "w");
    if (!file) {
        log_e("Failed to open config file for writing");
        return false;
    }
    
    if (serializeJson(doc, file) == 0) {
        log_e("Failed to write config to file");
        file.close();
        return false;
    }
    
    file.close();
    log_i("Configuration saved successfully");
    return true;
}

/**
 * Generate mDNS name from MAC address
 * Format: "rotator-XXXX" where XXXX is the last 4 hex digits of MAC
 */
void generateMDNSName() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    
    // Get last 4 bytes of MAC address
    snprintf(config.mdns_name, sizeof(config.mdns_name), 
             "rotator-%02X%02X", mac[4], mac[5]);
    
    log_i("Generated mDNS name: %s", config.mdns_name);
} 
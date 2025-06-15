#include "config.h"

// Global configuration instance
RotatorConfig config;

/**
 * Reset configuration to factory defaults
 */
void resetToDefaultConfig() {
    // WiFi settings
    strncpy(config.ap_ssid, DEFAULT_AP_SSID, sizeof(config.ap_ssid));
    strncpy(config.ap_password, DEFAULT_AP_PASSWORD, sizeof(config.ap_password));
    
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
    
    // Save to file
    saveConfiguration();
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
    
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        log_e("Failed to parse config file: %s", error.c_str());
        return false;
    }
    
    // WiFi settings
    strlcpy(config.ap_ssid, 
            doc["ap_ssid"] | DEFAULT_AP_SSID, 
            sizeof(config.ap_ssid));
    strlcpy(config.ap_password, 
            doc["ap_password"] | DEFAULT_AP_PASSWORD, 
            sizeof(config.ap_password));
    
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
    
    log_i("Configuration loaded successfully");
    return true;
}

/**
 * Save configuration to SPIFFS
 */
bool saveConfiguration() {
    StaticJsonDocument<1024> doc;
    
    // WiFi settings
    doc["ap_ssid"] = config.ap_ssid;
    doc["ap_password"] = config.ap_password;
    
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
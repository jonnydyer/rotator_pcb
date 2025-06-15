#include "wifi_manager.h"
#include "main.h"

// Global web server instance
AsyncWebServer webServer(80);
DNSServer dnsServer;

// For tracking if OTA is in progress
bool otaInProgress = false;

/**
 * Start WiFi in Access Point mode
 */
bool startWiFiAP() {
    WiFi.mode(WIFI_AP);
    
    log_i("Starting AP with SSID: %s", config.ap_ssid);
    if (WiFi.softAP(config.ap_ssid, config.ap_password)) {
        log_i("AP started successfully");
        log_i("AP IP address: %s", WiFi.softAPIP().toString().c_str());
        return true;
    } else {
        log_e("Failed to start AP");
        return false;
    }
}

/**
 * Setup captive portal DNS server
 */
void setupCaptivePortal() {
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    
    // Add handler for captive portal detection
    webServer.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    
    webServer.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    
    webServer.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    
    webServer.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    
    webServer.on("/portal", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    
    log_i("Captive portal DNS server started");
}

/**
 * Process DNS requests for captive portal
 */
void handleDNS() {
    dnsServer.processNextRequest();
}

/**
 * Setup the web server routes and handlers
 */
void setupWebServer() {
    // Serve the root index page
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        log_i("Root http access");
        request->send(SPIFFS, "/index.html", "text/html");
    });
    
    // API endpoint for getting current status
    webServer.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        StaticJsonDocument<256> doc;  // Smaller document for just status
        
        doc["currentPosition"] = encoder1.getCount();
        doc["currentAngle"] = calculateCurrentAngle();
        doc["autoRotationEnabled"] = config.auto_rotation_enabled;
        
        // Current color based on angle
        int currentAngle = calculateCurrentAngle();
        switch(currentAngle) {
            case 0: doc["currentColor"] = config.color_0; break;
            case 90: doc["currentColor"] = config.color_90; break;
            case 180: doc["currentColor"] = config.color_180; break;
            case 270: doc["currentColor"] = config.color_270; break;
        }
        
        serializeJson(doc, *response);
        log_i("Status API access");
        request->send(response);
    });
    
    // API endpoint for getting configuration
    webServer.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        StaticJsonDocument<512> doc;
        
        // Position calibration
        doc["pos_0_degrees"] = config.pos_0_degrees;
        doc["pos_90_degrees"] = config.pos_90_degrees;
        doc["pos_180_degrees"] = config.pos_180_degrees;
        doc["pos_270_degrees"] = config.pos_270_degrees;
        
        // Colors
        doc["color_0"] = config.color_0;
        doc["color_90"] = config.color_90;
        doc["color_180"] = config.color_180;
        doc["color_270"] = config.color_270;
        
        // WiFi settings
        doc["ap_ssid"] = config.ap_ssid;
        doc["ap_password"] = config.ap_password;
        
        // Rotation settings
        doc["rotation_interval"] = config.rotation_interval;
        
        serializeJson(doc, *response);
        log_i("Config API access");
        request->send(response);
    });
    
    // API endpoint for updating settings
    AsyncCallbackJsonWebHandler* settingsHandler = new AsyncCallbackJsonWebHandler("/api/settings", 
        [](AsyncWebServerRequest *request, JsonVariant &json) {
            JsonObject jsonObj = json.as<JsonObject>();
            
            // WiFi settings if present
            if (jsonObj.containsKey("ap_ssid")) {
                strlcpy(config.ap_ssid, jsonObj["ap_ssid"], sizeof(config.ap_ssid));
            }
            
            if (jsonObj.containsKey("ap_password")) {
                strlcpy(config.ap_password, jsonObj["ap_password"], sizeof(config.ap_password));
            }
            
            // Position calibration if present
            if (jsonObj.containsKey("pos_0_degrees")) {
                config.pos_0_degrees = jsonObj["pos_0_degrees"];
            }
            
            if (jsonObj.containsKey("pos_90_degrees")) {
                config.pos_90_degrees = jsonObj["pos_90_degrees"];
            }
            
            if (jsonObj.containsKey("pos_180_degrees")) {
                config.pos_180_degrees = jsonObj["pos_180_degrees"];
            }
            
            if (jsonObj.containsKey("pos_270_degrees")) {
                config.pos_270_degrees = jsonObj["pos_270_degrees"];
            }
            
            // Colors if present
            if (jsonObj.containsKey("color_0")) {
                config.color_0 = jsonObj["color_0"];
            }
            
            if (jsonObj.containsKey("color_90")) {
                config.color_90 = jsonObj["color_90"];
            }
            
            if (jsonObj.containsKey("color_180")) {
                config.color_180 = jsonObj["color_180"];
            }
            
            if (jsonObj.containsKey("color_270")) {
                config.color_270 = jsonObj["color_270"];
            }
            
            // Rotation settings if present
            if (jsonObj.containsKey("rotation_interval")) {
                config.rotation_interval = jsonObj["rotation_interval"];
            }
            
            if (jsonObj.containsKey("auto_rotation_enabled")) {
                config.auto_rotation_enabled = jsonObj["auto_rotation_enabled"];
            }
            
            // Save the updated configuration
            saveConfiguration();
            
            request->send(200, "text/plain", "Settings updated");
        }
    );
    webServer.addHandler(settingsHandler);
    
    // API endpoint for commanding a rotation
    webServer.on("/api/rotate", HTTP_POST, [](AsyncWebServerRequest *request) {
        log_i("Rotate API access");
        if (!request->hasParam("angle", true)) {
            request->send(400, "text/plain", "Missing 'angle' parameter");
            return;
        }
        
        int angle = request->getParam("angle", true)->value().toInt();
        if (angle != 0 && angle != 90 && angle != 180 && angle != 270) {
            request->send(400, "text/plain", "Angle must be 0, 90, 180, or 270");
            return;
        }
        
        // Command the rotation
        rotateToAngle(angle);
        request->send(200, "text/plain", "Rotation commanded");
    });
    
    // API endpoint for setting the current position as the new zero reference point
    webServer.on("/api/set-zero", HTTP_POST, [](AsyncWebServerRequest *request) {
        log_i("Set Zero API access");
        
        // Get current encoder position
        int32_t currentPosition = encoder1.getCount();
        
        // Calculate offset to subtract from all positions
        int32_t offset = currentPosition;
        
        // Update all calibration positions by subtracting the offset
        config.pos_0_degrees -= offset;
        config.pos_90_degrees -= offset;
        config.pos_180_degrees -= offset;
        config.pos_270_degrees -= offset;
        
        // Save the updated configuration
        saveConfiguration();
        
        log_i("Zero position set. Offset applied: %d", offset);
        log_i("New positions - 0°: %d, 90°: %d, 180°: %d, 270°: %d", 
              config.pos_0_degrees, config.pos_90_degrees, 
              config.pos_180_degrees, config.pos_270_degrees);
        
        request->send(200, "text/plain", "Zero position set successfully");
    });
    
    // API endpoint for going to a specific encoder position
    webServer.on("/api/goto", HTTP_POST, [](AsyncWebServerRequest *request) {
        log_i("Goto API access");
        if (!request->hasParam("position", true)) {
            request->send(400, "text/plain", "Missing 'position' parameter");
            return;
        }
        
        int32_t targetPosition = request->getParam("position", true)->value().toInt();
        
        // Basic safety check - limit to reasonable range
        if (abs(targetPosition) > 2000000) {
            request->send(400, "text/plain", "Position out of safe range (±100000)");
            return;
        }
        
        // Command the movement using default speed and acceleration
        move_to_position(targetPosition);
        
        log_i("Commanded movement to position: %d", targetPosition);
        request->send(200, "text/plain", "Movement commanded");
    });
    
    // Endpoint for resetting to default settings
    webServer.on("/api/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        log_i("Reset API Access");
        resetToDefaultConfig();
        request->send(200, "text/plain", "Settings reset to defaults");
    });
    
    // Start the web server
    webServer.begin();
    log_i("Web server started");
}

/**
 * Setup OTA update handler
 */
void setupOTA() {
    // OTA update endpoint
    webServer.on("/update", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            bool shouldReboot = !Update.hasError();
            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", 
                shouldReboot ? "Update complete, rebooting..." : "Update failed!");
            response->addHeader("Connection", "close");
            request->send(response);
            
            if (shouldReboot) {
                delay(1000);
                ESP.restart();
            }
        },
        [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
            // Handle the actual OTA update
            if (!index) {
                log_i("OTA update started: %s", filename.c_str());
                otaInProgress = true;
                
                // Check if the update is possible
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    log_e("OTA update not possible: %s", Update.errorString());
                    request->send(400, "text/plain", "OTA update not possible");
                    return;
                }
            }
            
            // Write the data to flash
            if (Update.write(data, len) != len) {
                log_e("OTA update error: %s", Update.errorString());
                return;
            }
            
            // When the update is complete
            if (final) {
                if (Update.end(true)) {
                    log_i("OTA update successful");
                } else {
                    log_e("OTA update failed: %s", Update.errorString());
                }
                otaInProgress = false;
            }
        }
    );
    
    log_i("OTA update handler setup complete");
}

#include "wifi_manager.h"
#include "main.h"
#include "web_ui.h"  // Include the compiled HTML

// Global web server instance
AsyncWebServer webServer(80);
AsyncWebSocket debugWebSocket("/ws/debug");
DNSServer dnsServer;

// For tracking if OTA is in progress
bool otaInProgress = false;

// Debug WebSocket state
bool debugStreamActive = false;
unsigned long lastDebugSend = 0;

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
 * Handle WebSocket events for debug interface
 */
void onDebugWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            log_i("Debug WebSocket client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
            break;
            
        case WS_EVT_DISCONNECT:
            log_i("Debug WebSocket client #%u disconnected", client->id());
            // If no clients connected, stop debug streaming
            if (debugWebSocket.count() == 0) {
                debugStreamActive = false;
                log_i("Debug streaming stopped - no clients connected");
            }
            break;
            
        case WS_EVT_DATA: {
            AwsFrameInfo *info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                // Parse incoming message
                String message = String((char*)data, len);
                log_i("Debug WebSocket received: %s", message.c_str());
                
                if (message == "start") {
                    debugStreamActive = true;
                    lastDebugSend = 0; // Force immediate send
                    log_i("Debug streaming started");
                } else if (message == "stop") {
                    debugStreamActive = false;
                    log_i("Debug streaming stopped");
                }
            }
            break;
        }
        
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

/**
 * Setup WebSocket handlers
 */
void setupWebSockets() {
    debugWebSocket.onEvent(onDebugWebSocketEvent);
    webServer.addHandler(&debugWebSocket);
    log_i("Debug WebSocket handler setup complete");
}

/**
 * Send debug data to connected WebSocket clients
 * This should be called from the debug timer at 10Hz
 */
void sendDebugData() {
    if (!debugStreamActive || debugWebSocket.count() == 0) {
        return;
    }
    
    unsigned long currentTime = millis();
    if (currentTime - lastDebugSend < DEBUG_SEND_INTERVAL_MS) {
        return;
    }
    
    // Get motion control information
    MotionControlInfo motionInfo = get_motion_control_info();
    
    // Create JSON debug data
    StaticJsonDocument<256> doc;
    doc["timestamp"] = currentTime;
    doc["currentPosition"] = get_current_position();
    doc["currentVelocity"] = motionInfo.velocity;
    doc["targetPosition"] = motionInfo.target_position;
    doc["motionActive"] = motionInfo.motion_active;
    
    // Add real motion control debug data
    doc["speedError"] = motionInfo.speed_error;
    doc["errorIntegral"] = motionInfo.speed_error_integral;  
    doc["errorDerivative"] = motionInfo.speed_error_derivative;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    debugWebSocket.textAll(jsonString);
    lastDebugSend = currentTime;
}

/**
 * Setup the web server routes and handlers
 */
void setupWebServer() {
    // Serve the root index page from compiled HTML (PROGMEM)
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        log_i("Root http access");
        request->send(200, "text/html", html_index);
    });
    
    // API endpoint for getting current status
    webServer.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        StaticJsonDocument<256> doc;  // Smaller document for just status
        
        doc["currentPosition"] = get_current_position();
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
        StaticJsonDocument<768> doc;  // Increased size for motion control params
        
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
        
        // Motion control parameters
        doc["position_hysteresis"] = config.position_hysteresis;
        doc["max_speed"] = config.max_speed;
        doc["acceleration"] = config.acceleration;
        doc["vel_loop_p"] = config.vel_loop_p;
        doc["vel_loop_i"] = config.vel_loop_i;
        doc["vel_loop_d"] = config.vel_loop_d;
        doc["vel_filter_persistence"] = config.vel_filter_persistence;
        doc["spd_err_persistence"] = config.spd_err_persistence;
        
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
            
            // Motion control parameters if present
            if (jsonObj.containsKey("position_hysteresis")) {
                config.position_hysteresis = jsonObj["position_hysteresis"];
            }
            
            if (jsonObj.containsKey("max_speed")) {
                config.max_speed = jsonObj["max_speed"];
            }
            
            if (jsonObj.containsKey("acceleration")) {
                config.acceleration = jsonObj["acceleration"];
            }
            
            if (jsonObj.containsKey("vel_loop_p")) {
                config.vel_loop_p = jsonObj["vel_loop_p"];
            }
            
            if (jsonObj.containsKey("vel_loop_i")) {
                config.vel_loop_i = jsonObj["vel_loop_i"];
            }
            
            if (jsonObj.containsKey("vel_loop_d")) {
                config.vel_loop_d = jsonObj["vel_loop_d"];
            }
            
            if (jsonObj.containsKey("vel_filter_persistence")) {
                config.vel_filter_persistence = jsonObj["vel_filter_persistence"];
            }
            
            if (jsonObj.containsKey("spd_err_persistence")) {
                config.spd_err_persistence = jsonObj["spd_err_persistence"];
            }
            
            // Save the updated configuration
            saveConfiguration();
            
            // Update runtime motion control parameters
            setMotionControlConfig(config.position_hysteresis, config.max_speed, config.acceleration,
                                  config.vel_loop_p, config.vel_loop_i, config.vel_loop_d,
                                  config.vel_filter_persistence, config.spd_err_persistence);
            
            // Update calibration-based parameters
            updateMotionControlCalibration();
            
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
        int32_t currentPosition = get_current_position();
        
        // Calculate offset to subtract from all positions
        int32_t offset = currentPosition - config.pos_0_degrees;
        
        // Update all calibration positions by subtracting the offset
        config.pos_0_degrees = currentPosition;
        config.pos_90_degrees += offset;
        config.pos_180_degrees += offset;
        config.pos_270_degrees += offset;
        
        // Save the updated configuration
        saveConfiguration();
        
        // Update calibration-based parameters
        updateMotionControlCalibration();
        
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
    
    // Setup WebSocket handlers
    setupWebSockets();
    
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

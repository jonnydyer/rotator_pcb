#include "wifi_manager.h"
#include "main.h"
#include "web_ui.h"  // Include the compiled HTML
#include "ESPmDNS.h"

// Global web server instance
AsyncWebServer webServer(80);
AsyncWebSocket debugWebSocket("/ws/debug");
DNSServer dnsServer;

// For tracking if OTA is in progress
bool otaInProgress = false;

// Debug WebSocket state
bool debugStreamActive = false;
unsigned long lastDebugSend = 0;

// WiFi state management
WiFiState currentWiFiState = WIFI_DISCONNECTED;

/**
 * Start WiFi in Access Point mode
 */
bool startWiFiAP() {
    WiFi.mode(WIFI_AP);
    
    log_i("Starting AP with SSID: %s", config.ap_ssid);
    if (WiFi.softAP(config.ap_ssid, config.ap_password)) {
        log_i("AP started successfully");
        log_i("AP IP address: %s", WiFi.softAPIP().toString().c_str());
        
        // Start mDNS
        startMDNS();
        
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

        doc["full_rotation_count"] = config.full_rotation_count;
        
        // Colors
        doc["color_0"] = config.color_0;
        doc["color_90"] = config.color_90;
        doc["color_180"] = config.color_180;
        doc["color_270"] = config.color_270;

        // WiFi settings
        doc["ap_ssid"] = config.ap_ssid;
        doc["ap_password"] = config.ap_password;
        doc["mdns_name"] = config.mdns_name;
        
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
            
            if (jsonObj.containsKey("mdns_name")) {
                strlcpy(config.mdns_name, jsonObj["mdns_name"], sizeof(config.mdns_name));
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

            if (jsonObj.containsKey("full_rotation_count")) {
                config.full_rotation_count = jsonObj["full_rotation_count"];
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
    
    // WiFi management API endpoints
    // Scan for available networks
    webServer.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        log_i("WiFi scan API access");
        
        // Check if scan is already running
        if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
            request->send(409, "text/plain", "Scan already in progress");
            return;
        }
        
        // Start async scan
        WiFi.scanNetworks(true);
        
        // Return immediately with scan started message
        request->send(202, "text/plain", "Scan started");
    });
    
    // Get scan results
    webServer.on("/api/wifi/scan-results", HTTP_GET, [](AsyncWebServerRequest *request) {
        log_i("WiFi scan results API access");
        
        int n = WiFi.scanComplete();
        
        if (n == WIFI_SCAN_RUNNING) {
            request->send(202, "text/plain", "Scan in progress");
            return;
        }
        
        if (n == WIFI_SCAN_FAILED) {
            request->send(500, "text/plain", "Scan failed");
            return;
        }
        
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        StaticJsonDocument<2048> doc;
        JsonArray networks = doc.createNestedArray("networks");
        
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                JsonObject network = networks.createNestedObject();
                network["ssid"] = WiFi.SSID(i);
                network["rssi"] = WiFi.RSSI(i);
                network["encryption"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured";
            }
        }
        
        serializeJson(doc, *response);
        request->send(response);
    });
    
    // Test WiFi connection
    webServer.on("/api/wifi/test", HTTP_POST, [](AsyncWebServerRequest *request) {
        log_i("WiFi test API access");
        
        if (!request->hasParam("ssid", true) || !request->hasParam("password", true)) {
            request->send(400, "text/plain", "Missing ssid or password parameter");
            return;
        }
        
        String ssid = request->getParam("ssid", true)->value();
        String password = request->getParam("password", true)->value();
        
        if (testWiFiConnection(ssid.c_str(), password.c_str())) {
            request->send(200, "text/plain", "Connection test successful");
        } else {
            request->send(400, "text/plain", "Connection test failed");
        }
    });
    
    // Connect to WiFi and save credentials
    webServer.on("/api/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *request) {
        log_i("WiFi connect API access");
        
        if (!request->hasParam("ssid", true) || !request->hasParam("password", true)) {
            request->send(400, "text/plain", "Missing ssid or password parameter");
            return;
        }
        
        String ssid = request->getParam("ssid", true)->value();
        String password = request->getParam("password", true)->value();
        
        // Test connection first
        if (!testWiFiConnection(ssid.c_str(), password.c_str())) {
            request->send(400, "text/plain", "Connection test failed");
            return;
        }
        
        // Save credentials
        strlcpy(config.wifi_ssid, ssid.c_str(), sizeof(config.wifi_ssid));
        strlcpy(config.wifi_password, password.c_str(), sizeof(config.wifi_password));
        config.wifi_client_enabled = true;
        
        // Save configuration
        if (saveConfiguration()) {
            request->send(200, "text/plain", "WiFi credentials saved successfully");
        } else {
            request->send(500, "text/plain", "Failed to save configuration");
        }
    });
    
    // Disconnect and clear WiFi credentials
    webServer.on("/api/wifi/disconnect", HTTP_POST, [](AsyncWebServerRequest *request) {
        log_i("WiFi disconnect API access");
        
        // Clear credentials
        strlcpy(config.wifi_ssid, "", sizeof(config.wifi_ssid));
        strlcpy(config.wifi_password, "", sizeof(config.wifi_password));
        config.wifi_client_enabled = false;
        
        // Save configuration
        if (saveConfiguration()) {
            // Switch to AP mode
            switchToAPMode();
            request->send(200, "text/plain", "WiFi disconnected and credentials cleared");
        } else {
            request->send(500, "text/plain", "Failed to save configuration");
        }
    });
    
    // Get WiFi status
    webServer.on("/api/wifi/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        log_i("WiFi status API access");
        
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        StaticJsonDocument<256> doc;
        
        doc["state"] = (int)getWiFiState();
        doc["status"] = getWiFiStatus();
        doc["ssid"] = config.wifi_ssid;
        doc["client_enabled"] = config.wifi_client_enabled;
        doc["mdns_name"] = config.mdns_name;
        
        if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED) {
            doc["ip"] = WiFi.localIP().toString();
        } else if (WiFi.getMode() == WIFI_AP) {
            doc["ip"] = WiFi.softAPIP().toString();
        }
        
        serializeJson(doc, *response);
        request->send(response);
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

/**
 * Initialize WiFi based on configuration
 * Attempts client connection first, falls back to AP mode
 */
bool initializeWiFi() {
    // Check if client mode is enabled and credentials exist
    if (config.wifi_client_enabled && strlen(config.wifi_ssid) > 0) {
        log_i("Attempting WiFi client connection to: %s", config.wifi_ssid);
        currentWiFiState = WIFI_CONNECTING_CLIENT;
        
        if (startWiFiClient()) {
            currentWiFiState = WIFI_CONNECTED_CLIENT;
            log_i("WiFi client connected successfully");
            return true;
        } else {
            currentWiFiState = WIFI_CONNECTION_FAILED;
            log_w("WiFi client connection failed, falling back to AP mode");
        }
    }
    
    // Fall back to AP mode
    log_i("Starting WiFi in AP mode");
    currentWiFiState = WIFI_CONNECTING_AP;
    
    if (startWiFiAP()) {
        currentWiFiState = WIFI_CONNECTED_AP;
        log_i("WiFi AP started successfully");
        return true;
    } else {
        currentWiFiState = WIFI_CONNECTION_FAILED;
        log_e("Failed to start WiFi AP");
        return false;
    }
}

/**
 * Start WiFi in client mode
 */
bool startWiFiClient() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifi_ssid, config.wifi_password);
    
    // Wait for connection with timeout
    unsigned long startTime = millis();
    unsigned long timeout = config.wifi_connection_timeout * 1000; // Convert to milliseconds
    
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < timeout) {
        delay(500);
        log_d("Connecting to WiFi...");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        log_i("WiFi client connected to: %s", config.wifi_ssid);
        log_i("IP address: %s", WiFi.localIP().toString().c_str());
        
        // Start mDNS
        startMDNS();
        
        return true;
    } else {
        log_e("WiFi client connection failed");
        WiFi.disconnect();
        return false;
    }
}


/**
 * Test WiFi connection with given credentials
 */
bool testWiFiConnection(const char* ssid, const char* password) {
    log_i("Testing WiFi connection to: %s", ssid);
    
    // Save current mode
    WiFiMode_t currentMode = WiFi.getMode();
    
    // Switch to STA mode for testing
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid, password);
    
    // Wait for connection with shorter timeout for testing
    unsigned long startTime = millis();
    unsigned long timeout = config.wifi_connection_timeout * 1000; 
    
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < timeout) {
        delay(500);
    }
    
    bool connected = (WiFi.status() == WL_CONNECTED);
    
    if (connected) {
        log_i("WiFi test connection successful");
        WiFi.disconnect();
    } else {
        log_w("WiFi test connection failed");
    }
    
    // Restore previous mode
    WiFi.mode(currentMode);
    
    return connected;
}

/**
 * Switch to AP mode
 */
void switchToAPMode() {
    log_i("Switching to AP mode");
    WiFi.disconnect();
    delay(100);
    
    if (startWiFiAP()) {
        currentWiFiState = WIFI_CONNECTED_AP;
        setupCaptivePortal();
        log_i("Switched to AP mode successfully");
    } else {
        currentWiFiState = WIFI_CONNECTION_FAILED;
        log_e("Failed to switch to AP mode");
    }
}

/**
 * Switch to client mode
 */
void switchToClientMode() {
    log_i("Switching to client mode");
    
    if (startWiFiClient()) {
        currentWiFiState = WIFI_CONNECTED_CLIENT;
        log_i("Switched to client mode successfully");
    } else {
        currentWiFiState = WIFI_CONNECTION_FAILED;
        log_e("Failed to switch to client mode");
        // Fall back to AP mode
        switchToAPMode();
    }
}

/**
 * Start mDNS service
 */
void startMDNS() {
    if (MDNS.begin(config.mdns_name)) {
        log_i("mDNS responder started with hostname: %s.local", config.mdns_name);
        
        // Add HTTP service
        MDNS.addService("http", "tcp", 80);
        MDNS.addServiceTxt("http", "tcp", "device", "rotator");
        MDNS.addServiceTxt("http", "tcp", "version", "1.0");
    } else {
        log_e("Error setting up mDNS responder");
    }
}

/**
 * Get current WiFi state
 */
WiFiState getWiFiState() {
    return currentWiFiState;
}

/**
 * Get WiFi status string for display
 */
String getWiFiStatus() {
    switch (currentWiFiState) {
        case WIFI_DISCONNECTED:
            return "Disconnected";
        case WIFI_CONNECTING_CLIENT:
            return "Connecting to WiFi...";
        case WIFI_CONNECTED_CLIENT:
            return "Connected to " + String(config.wifi_ssid) + " (" + WiFi.localIP().toString() + ")";
        case WIFI_CONNECTING_AP:
            return "Starting AP...";
        case WIFI_CONNECTED_AP:
            return "AP Mode (" + WiFi.softAPIP().toString() + ")";
        case WIFI_CONNECTION_FAILED:
            return "Connection Failed";
        default:
            return "Unknown";
    }
}

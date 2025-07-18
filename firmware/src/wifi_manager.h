#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <Update.h>
#include "config.h"
#include "rotator.h"
#include "neopixel.h"
#include "main.h"

// Constants
const int DNS_PORT = 53;

// WARNING: Performance Consideration
// --------------------------------
// When serving static files, avoid serving the entire SPIFFS filesystem at the root path ("/").
// Instead, serve specific directories or files to prevent performance issues:
// 
// BAD:  webServer.serveStatic("/", SPIFFS, "/");  // Serves entire filesystem at root
// GOOD: webServer.serveStatic("/", SPIFFS, "/www");  // Serves only the www directory
// 
// Serving at root path causes:
// 1. Every HTTP request checks SPIFFS first
// 2. Increased filesystem operations
// 3. Delayed TCP event processing
// 4. "coalescing polls" and "throttling" warnings in AsyncTCP
// 5. Overall reduced web server performance

// Enums and types
enum WiFiMode {
    AP_MODE,
    STA_MODE
};

// Function declarations
bool startWiFiAP();
void setupCaptivePortal();
void handleDNS();
void setupWebServer();
void setupWebSockets();
void setupOTA();
void handleWiFiEvents();
void sendDebugData();
void onDebugWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

// External declarations
extern AsyncWebServer webServer;
extern AsyncWebSocket debugWebSocket;
extern DNSServer dnsServer;
extern bool otaInProgress;
extern bool debugStreamActive;

// External function declarations from rotator.h and config.h
extern int calculateCurrentAngle();
extern void rotateToAngle(int angle);
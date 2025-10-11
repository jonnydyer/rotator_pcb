# ESP32 Rotator PCB Firmware

A sophisticated motor control system for precise rotational positioning with web interface and real-time debugging capabilities.

# IMPORTANT FABRICATION NOTES

1. The latest rev design requires a belt circumference of 9.5"


## Features

### Core Functionality
- **Precise Position Control**: Quadrature encoder feedback with PID velocity control
- **Trapezoidal Motion Profiles**: Smooth acceleration/deceleration for optimal motion
- **Multi-Position Calibration**: Support for 0°, 90°, 180°, and 270° positions
- **Automatic Rotation**: Configurable timed rotation sequences
- **Visual Feedback**: NeoPixel LED indication for current position

### Web Interface
- **Captive Portal**: Easy WiFi setup and configuration
- **Real-time Control**: Web-based motor control and position commands
- **Configuration Management**: Persistent settings with web-based editing
- **OTA Updates**: Over-the-air firmware updates

### Debug & Monitoring
- **WebSocket Streaming**: Real-time debug data at 10Hz
- **PID Visualization**: Live monitoring of control loop parameters
- **Position Tracking**: Current vs target position plotting
- **Test Interface**: Standalone WebSocket test page for development

## Hardware Requirements

- **ESP32 Development Board**
- **Quadrature Encoder** (connected to pins 17/18)
- **Motor Driver** (MCPWM controlled via pins 15/16)
- **NeoPixel LED** (pin 13)
- **User LED** (pin 12)

## Quick Start

### 1. Build and Upload
```bash
cd firmware
pio run --target upload
pio run --target uploadfs
```

### 2. Connect to WiFi
- Connect to "RotatorAP" network (password: "rotator1234")
- Navigate to http://192.168.4.1

### 3. Calibrate Positions
- Use "Set Zero" to establish reference position
- Manually position to 90°, 180°, 270° and update calibration

### 4. Debug Interface
- Enable debug streaming in web interface
- Use WebSocket test page for development: `websocket_test.html`

## Architecture

### Module Organization
```
main.cpp          - Core control loops and hardware abstraction
wifi_manager.cpp  - Network, web server, and WebSocket handling  
rotator.cpp       - High-level rotation logic and angle calculations
config.cpp        - Configuration persistence and management
neopixel.cpp      - LED control and visual feedback
```

### Timer Architecture
- **Motion Control**: 10ms PID control loop
- **Encoder Reading**: 10ms position sensing
- **Debug Streaming**: 100ms WebSocket data transmission
- **Auto Rotation**: 1000ms rotation sequence checking
- **LED Blink**: 250ms status indication

### Data Flow
```
Encoder → Position Sensing → Motion Control → Motor Output
    ↓
Debug Variables → WebSocket Timer → Web Interface
```

## Configuration

### Default Settings
- **WiFi AP**: "RotatorAP" / "rotator1234"
- **Max Speed**: 6000 counts/second
- **Acceleration**: 4000 counts/second²
- **Position Hysteresis**: ±20 counts

### Calibration Process
1. Power on system
2. Manually position to desired 0° reference
3. Use "Set Zero" command via web interface
4. Physically rotate to other positions and update calibration values

## Development

### Coding Guidelines
See [CODING_GUIDELINES.md](CODING_GUIDELINES.md) for detailed patterns and style rules established for this project.

### Key Patterns
- **Data Encapsulation**: Use getter functions instead of global variable access
- **Timer Separation**: Dedicated timers for each functional area
- **WebSocket Decoupling**: Debug streaming independent of control loops
- **Module Boundaries**: Clean interfaces between functional modules

### Debug Tools
- **Serial Logging**: Detailed system events and performance data
- **WebSocket Streaming**: Real-time PID parameters and position data
- **Test Interface**: Standalone WebSocket test page for development

### Performance Monitoring
```cpp
// Example debug output format
{
  "timestamp": 12345,
  "currentPosition": 1000,
  "targetPosition": 2000,
  "motionActive": true,
  "speedError": 0.123,
  "errorIntegral": 0.456,
  "errorDerivative": 0.789
}
```

## API Reference

### REST Endpoints
- `GET /api/status` - Current system status
- `GET /api/config` - Configuration settings
- `POST /api/settings` - Update configuration
- `POST /api/rotate?angle=90` - Command rotation
- `POST /api/goto?position=1000` - Go to encoder position
- `POST /api/set-zero` - Set current position as zero reference

### WebSocket Interface
- **Endpoint**: `/ws/debug`
- **Commands**: `"start"`, `"stop"`
- **Data Rate**: 10Hz when active
- **Format**: JSON with timestamp, position, and PID data

## Troubleshooting

### Common Issues
1. **Motor not moving**: Check MCPWM pin connections and power supply
2. **Position drift**: Verify encoder wiring and quadrature signals
3. **Web interface not loading**: Ensure SPIFFS upload completed successfully
4. **WebSocket connection fails**: Check IP address and firewall settings

### Debug Information
- Monitor serial output at 460800 baud for system logs
- Use WebSocket test page to verify real-time data streaming
- Check heap usage and performance metrics in serial output

## License

This project is developed for educational and research purposes. See individual component licenses for specific terms.

## Contributing

When contributing to this project, please follow the established patterns documented in [CODING_GUIDELINES.md](CODING_GUIDELINES.md).

---

**Project Status**: Active Development  
**Last Updated**: 2024  
**ESP32 Framework**: Arduino/ESP-IDF via PlatformIO 
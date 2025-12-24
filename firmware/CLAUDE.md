# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-based motor control system for precise rotational positioning with web interface, PID velocity control, and real-time WebSocket debugging. Hardware: Adafruit Feather ESP32-S3 with quadrature encoders, MCPWM motor drivers, and NeoPixel status LEDs.

## Build Commands

### Build and Upload
```bash
pio run --target upload        # Upload firmware
pio run --target uploadfs      # Upload SPIFFS filesystem (rarely needed)
```

### Serial Monitor
```bash
pio device monitor             # Monitor at 460800 baud with exception decoder
```

### Clean Build
```bash
pio run --target clean && pio run
```

## HTML UI Development

The web UI is **embedded into firmware**, not served from SPIFFS:

1. Edit `data/index.html`
2. Run `python3 html_to_header.py` to generate `src/web_ui.h`
3. Build and upload firmware

See `README_HTML_EMBEDDING.md` for details. The HTML is compiled as PROGMEM for monolithic OTA updates.

## Architecture

### Module Structure

The codebase follows strict module boundaries with data encapsulation:

- **main.cpp**: Hardware abstraction (encoders, MCPWM, timers), motion control, PID velocity loop
- **wifi_manager.cpp**: Network stack, AsyncWebServer, WebSocket debug streaming, captive portal, OTA updates
- **rotator.cpp**: High-level rotation logic, angle-to-position conversion, auto-rotation sequences
- **config.cpp**: JSON configuration persistence to SPIFFS
- **neopixel.cpp**: LED visual feedback for current position

**Key principle**: Use getter functions instead of direct global variable access. See `get_current_position()`, `get_motion_control_info()`, `getMotionControlConfig()` in main.h:36-47.

### Timer Architecture (esp_timer)

Five independent periodic timers, all in microseconds:

- **LED blink** (250ms): System status indication
- **Encoder reading** (10ms): Velocity calculation via differentiation
- **Motion control** (10ms): PID velocity loop, trapezoidal profile generation
- **Auto-rotation** (1000ms): Timed rotation sequence checking
- **Debug streaming** (100ms): WebSocket data transmission at 10Hz

All timer callbacks marked `IRAM_ATTR` for ISR execution.

### Motion Control Flow

```
Encoder → Velocity Calculation → Trapezoidal Profile → PID Velocity Loop → MCPWM Output
   ↓                                                           ↓
   └─────────────────────────────────────────────────────────→ Position Error Check
```

**Trapezoidal motion profile**: Generates smooth acceleration/deceleration with configurable max speed and acceleration. PID controls velocity, not position directly.

**Encoder unwrapping**: After reaching target, automatically unwraps encoder position if beyond full_rotation_count (see main.cpp:358-384).

### Data Flow Pattern

Motion control variables in main.cpp are accessed through getters (main.h:36-39):
- `get_current_position()` - Current encoder count
- `is_motion_active()` - Motion state
- `get_motion_control_info()` - Complete motion state for WebSocket streaming

This prevents tight coupling between modules and race conditions.

### Configuration System

`RotatorConfig` struct (config.h:50-90) persists to `/config.json` in SPIFFS:

**WiFi**: AP credentials, client SSID/password, mDNS name
**Calibration**: Position values for 0°/90°/180°/270° in encoder counts, full rotation count
**Motion control**: PID gains (P/I/D), max speed, acceleration, position hysteresis, filter parameters
**Auto-rotation**: Interval, enabled state, direction
**NeoPixel**: Colors for each position

Call `saveConfiguration()` after changes, `loadConfiguration()` at boot.

### WebSocket Debug Streaming

Endpoint: `/ws/debug`
Commands: `"start"` / `"stop"`
Rate: 10Hz (100ms timer)

JSON format:
```json
{
  "timestamp": 12345,
  "currentPosition": 1000,
  "targetPosition": 2000,
  "motionActive": true,
  "velocity": 150.5,
  "speedError": 0.123,
  "errorIntegral": 0.456,
  "errorDerivative": 0.789,
  "pwmOut": 25.3
}
```

Streaming controlled by `debugStreamActive` flag. Debug variables updated in motion control ISR, sent in separate timer to decouple control loop from network.

## Hardware Configuration

**Board**: Adafruit Feather ESP32-S3 (16MB flash, PSRAM)
**Framework**: Arduino via PlatformIO

### Pin Assignments (main.cpp:13-22)

- **Motor 1**: M1A=15, M1B=16 (MCPWM0)
- **Motor 2**: M2A=38, M2B=39 (MCPWM1)
- **Encoder 1**: E1A=17, E1B=18 (quadrature)
- **Encoder 2**: E2A=40, E2B=41 (quadrature)
- **NeoPixel**: 13
- **User LED**: 12 (system status blink)

### MCPWM Motor Control

20kHz PWM, sign-magnitude drive (one pin 100%, other PWM'd). See `set_motor1_speed()` (main.cpp:502-525) for implementation.

## Default Calibration Values

From config.h:19-25:

- **0°**: 0 counts
- **90°**: 7389 counts
- **180°**: 14778 counts
- **270°**: 22166 counts
- **Full rotation**: 29555 counts
- **Belt circumference**: 9.5" (per README)

## REST API Endpoints

Defined in wifi_manager.cpp:

- `GET /api/status` - Current position, motion state, WiFi info
- `GET /api/config` - Full configuration JSON
- `POST /api/settings` - Update configuration (JSON body)
- `POST /api/rotate?angle=90` - Rotate to angle (0/90/180/270)
- `POST /api/goto?position=1000` - Move to raw encoder position
- `POST /api/set-zero` - Set current position as 0° reference
- `POST /api/calibrate?pos=[0|90|180|270]` - Update position calibration
- `GET /api/wifi/scan` - Scan WiFi networks
- `POST /api/wifi/test` - Test WiFi credentials
- `POST /update` - OTA firmware upload

## Important Build Flags

From platformio.ini:27-36:

- `CORE_DEBUG_LEVEL=5` - Full debug logging
- `ARDUINO_USB_CDC_ON_BOOT=1` - USB CDC for serial
- `BOARD_HAS_PSRAM=1` - Enable PSRAM
- `CONFIG_ASYNC_TCP_RUNNING_CORE=1` - Force AsyncTCP to core 1
- `ASYNCWEBSERVER_REGEX=1` - Enable regex routes

## Key Technical Patterns

1. **PID operates on velocity, not position**: Motion control generates velocity setpoints via trapezoidal profile, PID tracks velocity (main.cpp:390-407).

2. **Circular unwrapping logic**: After motion complete, checks if encoder is >full_rotation_count from zero reference and resets to equivalent position (main.cpp:358-384).

3. **AsyncWebServer static file serving**: **NEVER** serve SPIFFS at root path (`serveStatic("/", SPIFFS, "/")`). This causes "coalescing polls" and throttling. Serve specific paths only. See wifi_manager.h:20-33 for detailed warning.

4. **Filter persistence parameters**: Both velocity and speed error use exponential filters with configurable persistence (0.0-1.0). Higher = more smoothing (config.h:43-44).

5. **Position hysteresis**: Motion considered complete when within ±hysteresis counts (default 5). Prevents oscillation around target (config.h:37).

## Serial Debug Output

460800 baud, ESP32 exception decoder enabled. Log levels via `log_i()`, `log_w()`, `log_e()` macros.

Key performance data in motion control loop (commented out by default, main.cpp:415-417):
```cpp
log_d("speed_err:%.3e,speed_int:%.1f,speed_deriv:%.3e,pwm_cmd:%.3f,target_vel:%.3f,encoder_cnt:%d,encoder_vel:%.3f,loop_time:%d", ...)
```

## Common Development Tasks

### Tuning PID Parameters

1. Modify `DEFAULT_VEL_LOOP_P/I/D` in config.h:40-42
2. Or adjust via web UI `/api/settings` with `vel_loop_p`, `vel_loop_i`, `vel_loop_d`
3. Monitor via WebSocket debug stream

### Calibrating New Hardware

1. Power on, use web UI to manually position to 0°
2. POST to `/api/set-zero`
3. Rotate to 90°, POST to `/api/calibrate?pos=90`
4. Repeat for 180°, 270°, full rotation
5. Values persist to SPIFFS

### Modifying Motion Profile

- **Max speed** (counts/sec): `config.max_speed` (default 2000)
- **Acceleration** (counts/sec²): `config.acceleration` (default 1000)
- **Position hysteresis**: `config.position_hysteresis` (default 5)

Update via `/api/settings` POST or modify defaults in config.h:37-39.

## Known Issues / Quirks

1. **Unwrap warning on first boot**: If `full_rotation_count` not calibrated, unwrap skipped (main.cpp:383).

2. **Motion error protection**: If position error increases beyond hysteresis during motion, motion stops with warning (main.cpp:317-336). Usually indicates mechanical binding or encoder failure.

3. **WiFi mode switching**: System prioritizes client mode if `wifi_client_enabled=true`. Falls back to AP mode on timeout (wifi_manager.cpp).

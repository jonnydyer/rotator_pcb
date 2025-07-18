# HTML UI Embedding in Firmware

## Overview

The HTML UI is now compiled directly into the firmware binary instead of being served from SPIFFS. This enables monolithic OTA updates that include both firmware and UI changes.

## Benefits

1. **Monolithic OTA Updates**: Single firmware binary contains both code and UI
2. **Faster Serving**: HTML served from flash memory instead of file system
3. **Simplified Deployment**: No need to manage separate SPIFFS uploads
4. **Reduced Complexity**: One less thing to go wrong during updates

## File Structure

- `data/index.html` - Source HTML file (still maintained for editing)
- `src/web_ui.h` - Generated C++ header with HTML as PROGMEM string
- `html_to_header.py` - Conversion script that generates the header file

## Workflow

### Modifying the HTML UI

1. Edit `data/index.html` as needed
2. Run the conversion script:
   ```bash
   python3 html_to_header.py
   ```
3. Build and upload the firmware:
   ```bash
   pio run --target upload
   ```

### Automatic Conversion (Future Enhancement)

The `platformio.ini` includes `extra_scripts = pre:html_to_header.py` for automatic conversion, but this needs debugging. For now, run the script manually when the HTML changes.

## Technical Details

### HTML to Header Conversion

The `html_to_header.py` script:
- Reads `data/index.html`
- Escapes special characters for C++ string literals
- Generates `src/web_ui.h` with:
  - `html_index[]` - PROGMEM string constant
  - `html_index_size` - Size of the HTML content

### Web Server Changes

In `wifi_manager.cpp`:
- Added `#include "web_ui.h"`
- Changed root handler from `request->send(SPIFFS, "/index.html", "text/html")` to `request->send(200, "text/html", html_index)`

### Memory Usage

- HTML content (~60KB) is stored in flash memory (PROGMEM)
- No impact on RAM usage
- Slight increase in firmware size but well within limits

## SPIFFS Usage

SPIFFS is still used for:
- JSON configuration files (`config.json`)
- Any other data files that need runtime modification

Only the HTML UI has been moved to compiled firmware.

## Troubleshooting

### Build Errors

If you get `web_ui.h: No such file or directory`:
1. Run `python3 html_to_header.py` manually
2. Ensure the script completed successfully
3. Verify `src/web_ui.h` was created

### HTML Changes Not Reflected

1. Ensure you ran the conversion script after modifying `data/index.html`
2. Clean and rebuild: `pio run --target clean && pio run`
3. Upload the new firmware

### Script Issues

The conversion script requires Python 3 and should work on any system. If it fails:
1. Check file permissions
2. Verify `data/index.html` exists and is readable
3. Ensure `src/` directory exists and is writable 
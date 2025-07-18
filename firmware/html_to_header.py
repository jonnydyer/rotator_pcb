#!/usr/bin/env python3
"""
Convert HTML file to C++ header file for embedding in firmware
"""

import os
import sys

def escape_string(s):
    """Escape string for C++ string literal"""
    # Replace backslashes first to avoid double escaping
    s = s.replace('\\', '\\\\')
    # Replace quotes
    s = s.replace('"', '\\"')
    # Replace newlines with literal \n
    s = s.replace('\n', '\\n')
    # Replace carriage returns
    s = s.replace('\r', '\\r')
    return s

def html_to_header(html_file, header_file):
    """Convert HTML file to C++ header file"""
    
    # Read the HTML file
    try:
        with open(html_file, 'r', encoding='utf-8') as f:
            html_content = f.read()
    except FileNotFoundError:
        print(f"Error: HTML file '{html_file}' not found")
        return False
    except Exception as e:
        print(f"Error reading HTML file: {e}")
        return False
    
    # Escape the content for C++ string literal
    escaped_content = escape_string(html_content)
    
    # Generate header file content
    header_content = f"""#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>

// HTML content for the web UI (stored in flash memory)
const char html_index[] PROGMEM = "{escaped_content}";

// Size of the HTML content
const size_t html_index_size = {len(html_content)};

#endif // WEB_UI_H
"""
    
    # Write the header file
    try:
        with open(header_file, 'w', encoding='utf-8') as f:
            f.write(header_content)
        print(f"Successfully converted '{html_file}' to '{header_file}'")
        print(f"HTML size: {len(html_content)} bytes")
        return True
    except Exception as e:
        print(f"Error writing header file: {e}")
        return False

def main():
    # Default paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    html_file = os.path.join(script_dir, 'data', 'index.html')
    header_file = os.path.join(script_dir, 'src', 'web_ui.h')
    
    # Allow command line arguments to override paths
    if len(sys.argv) >= 2:
        html_file = sys.argv[1]
    if len(sys.argv) >= 3:
        header_file = sys.argv[2]
    
    print(f"Converting HTML to header file...")
    print(f"Input:  {html_file}")
    print(f"Output: {header_file}")
    
    success = html_to_header(html_file, header_file)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main() 
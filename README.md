# Timbre Automático 🛎️

An automatic bell system for schools built with ESP32 that allows configuring and managing bell schedules through a web interface.

## Features

* **Multiple Schedule Configurations**: Support for three different bell schedules that can be switched on demand
*  **Web Interface**: Easy-to-use responsive web interface for configuring and managing bell schedules
*  **Manual Control**: Emergency bell trigger and system enable/disable functionality
*  **Time Synchronization**: NTP synchronization and manual time adjustment
*  **Authentication**: Protected web interface with username and password
*  **Audio Support**: Uses DFPlayer Mini module to play different sounds
*  **Offline Operation**: Falls back to Access Point mode if WiFi connection fails

## Hardware Requirements

*   ESP32 microcontroller board
*   DFPlayer Mini MP3 player module
*   Relay module for controlling the physical bell
*   Push buttons for manual control
*   MicroSD card for storing audio files
*   Power supply

## Pin Configuration

*   Relay Pin: GPIO 18
*   Emergency Button: GPIO 3
*   Disable Button: GPIO 2
*   DFPlayer RX: GPIO 20
*   DFPlayer TX: GPIO 19

## Software Dependencies

*   [ESP32 Arduino Core](vscode-file://vscode-app/private/var/folders/cj/5rfl5xmn2v56tgv0nwf5tlk00000gn/T/AppTranslocation/8DA65265-D99D-4BCC-8D98-9BAE63DCDA0D/d/Visual%20Studio%20Code.app/Contents/Resources/app/out/vs/code/electron-sandbox/workbench/workbench.html)
*   [ESPAsyncWebServer](vscode-file://vscode-app/private/var/folders/cj/5rfl5xmn2v56tgv0nwf5tlk00000gn/T/AppTranslocation/8DA65265-D99D-4BCC-8D98-9BAE63DCDA0D/d/Visual%20Studio%20Code.app/Contents/Resources/app/out/vs/code/electron-sandbox/workbench/workbench.html)
*   [ArduinoJson](vscode-file://vscode-app/private/var/folders/cj/5rfl5xmn2v56tgv0nwf5tlk00000gn/T/AppTranslocation/8DA65265-D99D-4BCC-8D98-9BAE63DCDA0D/d/Visual%20Studio%20Code.app/Contents/Resources/app/out/vs/code/electron-sandbox/workbench/workbench.html)
*   [DFRobotDFPlayerMini](vscode-file://vscode-app/private/var/folders/cj/5rfl5xmn2v56tgv0nwf5tlk00000gn/T/AppTranslocation/8DA65265-D99D-4BCC-8D98-9BAE63DCDA0D/d/Visual%20Studio%20Code.app/Contents/Resources/app/out/vs/code/electron-sandbox/workbench/workbench.html)
*   [ESP32Time](vscode-file://vscode-app/private/var/folders/cj/5rfl5xmn2v56tgv0nwf5tlk00000gn/T/AppTranslocation/8DA65265-D99D-4BCC-8D98-9BAE63DCDA0D/d/Visual%20Studio%20Code.app/Contents/Resources/app/out/vs/code/electron-sandbox/workbench/workbench.html)

## Setup Instructions

1.  Clone this repository
1.  Create a [credentials.h](vscode-file://vscode-app/private/var/folders/cj/5rfl5xmn2v56tgv0nwf5tlk00000gn/T/AppTranslocation/8DA65265-D99D-4BCC-8D98-9BAE63DCDA0D/d/Visual%20Studio%20Code.app/Contents/Resources/app/out/vs/code/electron-sandbox/workbench/workbench.html) file in the [main](vscode-file://vscode-app/private/var/folders/cj/5rfl5xmn2v56tgv0nwf5tlk00000gn/T/AppTranslocation/8DA65265-D99D-4BCC-8D98-9BAE63DCDA0D/d/Visual%20Studio%20Code.app/Contents/Resources/app/out/vs/code/electron-sandbox/workbench/workbench.html) directory with your WiFi and authentication settings:

``#define SSID "your\_wifi\_ssid"``
``#define PASSWORD "your\_wifi\_password"``
``#define HTTP\_USERNAME "your\_username"``
``#define HTTP\_PASSWORD "your\_password"``

     
3.  Upload the contents of the [data](vscode-file://vscode-app/private/var/folders/cj/5rfl5xmn2v56tgv0nwf5tlk00000gn/T/AppTranslocation/8DA65265-D99D-4BCC-8D98-9BAE63DCDA0D/d/Visual%20Studio%20Code.app/Contents/Resources/app/out/vs/code/electron-sandbox/workbench/workbench.html) folder to SPIFFS using the Arduino IDE "ESP32 Sketch Data Upload" tool
4.  Compile and upload the main code to your ESP32

## Web Interface

The web interface provides the following functionality:

*   Real-time clock display
*   Bell system status toggle
*   Active schedule selection
*   Manual bell trigger
*   System time adjustment
*   Schedule configuration with customizable bell times
*   Support for special bell types (break time, end of day)

## Default Access

*   Web Interface URL: http://\[ESP32\_IP\_ADDRESS\]
*   Default Username: admin
*   Default Password: admin

## License

This project is licensed under the MIT License.

## Author

Designed by Daniel Sanchez © 2025
# ESP32-CAM Video Streaming Platform

A lightweight video streaming solution built with ESP32-CAM that provides real-time MJPEG streaming over HTTP with Telegram notifications.

## 🌟 Features

- **Live Video Streaming**: Access real-time camera feed from any browser or MJPEG-compatible application
- **Telegram Integration**: Automatically sends camera IP address to a designated Telegram chat
- **Low Latency**: Optimized for minimal delay in video transmission
- **Responsive Design**: Simple web interface that works on desktop and mobile browsers
- **Easy Configuration**: Simple setup for WiFi and Telegram credentials

## 📋 Requirements

### Hardware
- ESP32-CAM development board (AI-Thinker model recommended)
- FTDI programmer or similar USB-to-Serial adapter for uploading code (CP2102 in my case)

### Software
- PlatformIO
- Required libraries:
  - `esp32` (core)
  - `esp32-camera`
  - `HTTPClient`

## 🔧 Installation

### Using Arduino IDE
1. Install the ESP32 board support package
2. Install the ESP32 Camera library
3. Create a `config.h` file with your WiFi and Telegram credentials (see Configuration section)
4. Connect your ESP32-CAM to the FTDI programmer (see wiring instructions below)
5. Select "AI Thinker ESP32-CAM" from the boards menu
6. Upload the sketch

### Using PlatformIO
1. Clone this repository
2. Create a `config.h` file in the `src` directory
3. Connect your ESP32-CAM to the programmer
4. Run `platformio run --target upload`

## 📝 Configuration

Create a `config.cpp` file with the following contents:

```cpp
#ifndef CONFIG_H
#define CONFIG_H

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Telegram credentials
const char* tg_bot_token = "YOUR_TELEGRAM_BOT_TOKEN";
const char* tg_chat_id = "YOUR_TELEGRAM_CHAT_ID";

#endif // CONFIG_H
```

## 📡 Wiring Instructions for Programming

To put the ESP32-CAM in programming mode, connect:

| ESP32-CAM | CP2102          |
|-----------|-----------------|
| 5V        | VCC (5V)        |
| GND       | GND             |
| U0RXD (GPIO3) | TXD         |
| U0TXD (GPIO1) | RXD         |
| GPIO0+GND |                 |

Important: GPIO0 <-> GND are used only during programming camera. You should connect on camera both of them and after that you will be able to upload new soft on it

Programming procedure:
1. Connect GPIO0 to GND
2. Press the RST button
3. Release the RST button
4. Upload the code
5. After uploading, disconnect GPIO0 from GND and press RST again

## 🚀 Usage

1. After uploading the code, the ESP32-CAM will:
   - Connect to your WiFi network
   - Send its IP address to your Telegram chat
   - Start the video streaming server

2. Access the video stream:
   - Open a web browser and navigate to `http://[ESP32-CAM_IP]/`
   - Or access the stream directly at `http://[ESP32-CAM_IP]/stream`

3. The stream can be embedded in other applications using:
   ```html
   <img src="http://[ESP32-CAM_IP]/stream" width="640" height="480">
   ```

## 📝 How It Works

- The ESP32-CAM initializes the camera module and connects to WiFi
- It sends the camera's IP address to a specified Telegram chat
- An HTTP server is started to handle web requests
- The root path (`/`) serves a simple HTML page with the video embedded
- The `/stream` endpoint provides a Motion JPEG (MJPEG) stream
- The main loop keeps the system running and handles client connections

## 🔌 Power Considerations

- The ESP32-CAM can be power hungry, especially during WiFi transmission
- For stable operation, use a good quality 5V power supply capable of delivering at least 500mA
- Using a USB power bank can provide portable operation

## 🔒 Security Considerations

- This implementation does not include authentication or encryption
- The video stream is accessible to anyone with the IP address
- For enhanced security, consider:
  - Implementing HTTP Basic Authentication
  - Using ESP32's HTTPS capabilities
  - Running on a secured local network

## 🛠️ Troubleshooting

- **Camera initialization fails**: Check camera connections, try reducing frame size
- **WiFi won't connect**: Verify credentials, ensure signal strength is adequate
- **Telegram messages not sending**: Check bot token and chat ID, ensure internet connectivity
- **Stream is slow or laggy**: Reduce resolution, improve WiFi signal, lower JPEG quality
- **ESP32 keeps crashing**: Use a more powerful power supply, reduce frame rate

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 🙏 Acknowledgements

- ESP32 Camera library developers
- Arduino and PlatformIO communities
- AI-Thinker for the ESP32-CAM module

---

Feel free to contribute to this project by submitting issues or pull requests!

# Get info about cammera and see in real time it's logs
    platformio device monitor -b 115200

# Upload firmware to device:
    platformio run --target upload

# Init project for specific board:
    platformio init --board esp32cam
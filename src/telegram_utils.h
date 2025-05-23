#ifndef TELEGRAM_UTILS_H
#define TELEGRAM_UTILS_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"

// Function to send a photo from the ESP32-CAM to Telegram
bool sendPhotoToTelegram(const char* tg_bot_token, const char* tg_chat_id);

// Function to send a text message to Telegram
bool sendMessageToTelegram(const char* tg_bot_token, const char* tg_chat_id, const char* message);

// Add more functions here if needed

#endif // TELEGRAM_UTILS_H
#include "telegram_utils.h"
#include <HTTPClient.h>

bool sendPhotoToTelegram(const char* tg_bot_token, const char* tg_chat_id) {
    camera_fb_t * fb = NULL;
    bool success = false;
    
    // Capture photo
    Serial.println("Capturing photo");
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return false;
    }
    
    Serial.println("Photo captured, size: " + String(fb->len) + " bytes");
    
    // Create secure WiFi client
    WiFiClientSecure client;
    
    // IMPORTANT: Skip certificate validation - necessary for ESP32 to connect to HTTPS
    client.setInsecure();
    
    Serial.println("Connecting to api.telegram.org...");
    
    // Connect to Telegram API server (HTTPS)
    if (!client.connect("api.telegram.org", 443)) {
      Serial.println("Connection failed");
      esp_camera_fb_return(fb);
      return false;
    }
    
    Serial.println("Connected to api.telegram.org");
    
    // Construct the URL path (not the full URL with protocol)
    String path = "/bot";
    path += tg_bot_token;
    path += "/sendPhoto?chat_id=";
    path += tg_chat_id;
    
    // Create a boundary for multipart/form-data
    String boundary = "ESP32CAM-";
    boundary += String(millis());
    
    // Construct form data for the photo
    String head = "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"photo\"; filename=\"esp32cam.jpg\"\r\n";
    head += "Content-Type: image/jpeg\r\n\r\n";
    
    String tail = "\r\n--" + boundary + "--\r\n";
    
    // Calculate content length
    uint32_t imageLen = fb->len;
    uint32_t headLen = head.length();
    uint32_t tailLen = tail.length();
    uint32_t totalLen = headLen + imageLen + tailLen;
    
    // Send HTTP headers for HTTPS
    client.print("POST " + path + " HTTP/1.1\r\n");
    client.print("Host: api.telegram.org\r\n");
    client.print("User-Agent: ESP32-CAM\r\n");
    client.print("Content-Length: " + String(totalLen) + "\r\n");
    client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
    client.print("Connection: close\r\n\r\n");
    
    // Send form data header
    client.print(head);
    
    // Send JPEG data in chunks
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    size_t chunkSize = 1024;
    
    Serial.println("Sending photo data...");
    
    for (size_t i = 0; i < fbLen; i += chunkSize) {
      size_t currentChunkSize = min(chunkSize, fbLen - i);
      size_t bytesWritten = client.write(fbBuf + i, currentChunkSize);
      
      if (bytesWritten != currentChunkSize) {
        Serial.println("Failed to send all bytes in chunk");
      }
      
      // A small delay can help with stability
      delay(1);
      
      // Print progress every ~100KB
      if (i % (chunkSize * 100) == 0) {
        Serial.printf("Sent %d bytes of %d\n", i, fbLen);
      }
    }
    
    // Send form data footer
    client.print(tail);
    Serial.println("Photo data sent, waiting for response...");
    
    // Wait for the server's response
    unsigned long timeout = millis() + 10000;  // 10 second timeout
    while (client.available() == 0) {
      if (millis() > timeout) {
        Serial.println("Response timeout");
        client.stop();
        esp_camera_fb_return(fb);
        return false;
      }
      delay(100);
    }
    
    // Read the response
    Serial.println("Reading response...");
    String responseStatus = client.readStringUntil('\n');
    Serial.println("Status: " + responseStatus);
    
    // Skip HTTP headers
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        // End of headers
        break;
      }
    }
    
    // Read response body
    String response = "";
    while (client.available()) {
      char c = client.read();
      response += c;
    }
    
    Serial.println("Response body: " + response);
    
    // Check for success (basic check)
    success = response.indexOf("\"ok\":true") > 0;
    
    // Clean up
    client.stop();
    esp_camera_fb_return(fb);
    
    Serial.println(success ? "Photo sent successfully!" : "Failed to send photo");
    
    return success;
  }

bool sendMessageToTelegram(const char* tg_bot_token, const char* tg_chat_id, const char *message)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected, cannot send message");
    return false;
  }

  Serial.println("Preparing to send message to Telegram");

  HTTPClient http;
  char url[150];
  snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage", tg_bot_token);

  Serial.println("Beginning HTTP connection");
  if (!http.begin(url))
  {
    Serial.println("Failed to begin HTTP connection");
    return false;
  }

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  char postData[200];
  snprintf(postData, sizeof(postData), "chat_id=%s&text=%s&parse_mode=HTML",
           tg_chat_id, message);

  Serial.println("Sending HTTP POST request");
  int httpResponseCode = http.POST(postData);

  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    http.end();
    return true;
  }
  else
  {
    Serial.print("Error on HTTP request. Error code: ");
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }
}
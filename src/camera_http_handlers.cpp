#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"          // Disable brownout problems
#include "soc/rtc_cntl_reg.h" // Disable brownout problems
#include "esp_http_server.h"
#include <HTTPClient.h>
#include "config.h"
#include <libb64/cencode.h>
#include "telegram_utils.h"

esp_err_t index_handler(httpd_req_t *req)
{
  httpd_resp_set_type(req, "text/html");
  String page = "<html><head><title>ESP32-CAM Stream</title></head><body>";
  page += "<h1>ESP32-CAM VideoStream</h1>";
  page += "<img src='/stream' width='640' height='480'>";
  page += "</body></html>";
  return httpd_resp_send(req, &page[0], page.length());
}

esp_err_t stream_handler(httpd_req_t *req)
{
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[64];

  // Set headers for MJPEG stream - modified for better compatibility
  static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace; boundary=123456789000000000000987654321";
  static const char *_STREAM_BOUNDARY = "\r\n--123456789000000000000987654321\r\n";
  static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

  httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  httpd_resp_set_hdr(req, "Pragma", "no-cache");
  httpd_resp_set_hdr(req, "Expires", "0");
  httpd_resp_set_hdr(req, "Connection", "close");

  Serial.println("Stream requested");

  while (true)
  {
    // Get frame from camera
    fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("Camera frame capture failed");
      res = ESP_FAIL;
    }
    else
    {
      // Use frame buffer directly if it's JPEG format
      if (fb->format == PIXFORMAT_JPEG)
      {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }

      // Send MJPEG part header
      if (res == ESP_OK)
      {
        size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
        res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
      }

      // Send JPEG data
      if (res == ESP_OK)
      {
        res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        if (res == ESP_OK)
        {
          Serial.printf("Frame sent: %u bytes\n", _jpg_buf_len);
        }
      }

      // Send boundary between MJPEG parts
      if (res == ESP_OK)
      {
        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
      }

      // Return frame buffer
      esp_camera_fb_return(fb);

      // Check if client disconnected
      if (res != ESP_OK)
      {
        Serial.println("Client disconnected or streaming error");
        break;
      }
    }
  }

  return res;
}

// Add this handler function
esp_err_t capture_handler(httpd_req_t *req) {
  Serial.println("Capture photo request received");
  bool success = sendPhotoToTelegram(tg_bot_token, tg_chat_id);
  
  // Return response
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  
  char response[100];
  if (success) {
    snprintf(response, sizeof(response), "{\"success\":true,\"message\":\"Photo captured and sent to Telegram\"}");
  } else {
    snprintf(response, sizeof(response), "{\"success\":false,\"message\":\"Failed to capture or send photo\"}");
  }
  
  return httpd_resp_send(req, response, strlen(response));
}

esp_err_t health_handler(httpd_req_t *req) {
    Serial.println("Health request received"); 
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "OK", 2);
  }
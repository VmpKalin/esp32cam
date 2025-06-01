
#include "camera_http_server.h"

// Variable to store HTTP server
httpd_handle_t camera_httpd = NULL;

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

  Logger::getInstance().info("Stream requested");

  while (true)
  {
    // Get frame from camera
    fb = esp_camera_fb_get();
    if (!fb)
    {
      Logger::getInstance().error("Camera frame capture failed");
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
        Logger::getInstance().error("Client disconnected or streaming error");
        break;
      }
    }
  }

  return res;
}

// Add this handler function
esp_err_t capture_handler(httpd_req_t *req)
{
  Logger::getInstance().info("Capture photo request received");
  bool success = sendPhotoToTelegram(tg_bot_token, tg_chat_id);

  // Return response
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char response[100];
  if (success)
  {
    snprintf(response, sizeof(response), "{\"success\":true,\"message\":\"Photo captured and sent to Telegram\"}");
  }
  else
  {
    snprintf(response, sizeof(response), "{\"success\":false,\"message\":\"Failed to capture or send photo\"}");
  }

  return httpd_resp_send(req, response, strlen(response));
}

esp_err_t health_handler(httpd_req_t *req)
{
  Logger::getInstance().info("Health request received");
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, "OK", 2);
}

void startHttpServer()
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Increase buffer size to handle larger headers
  config.max_uri_handlers = 16;
  config.max_resp_headers = 16;
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.stack_size = 10240; // Increase stack size
  config.recv_wait_timeout = 10;
  config.send_wait_timeout = 10;

  config.server_port = 80;

  // Configure main page handler
  httpd_uri_t index_uri = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = index_handler,
      .user_ctx = NULL};

  // Configure stream handler
  httpd_uri_t stream_uri = {
      .uri = "/stream",
      .method = HTTP_GET,
      .handler = stream_handler,
      .user_ctx = NULL};

  httpd_uri_t shot_uri = {
      .uri = "/shot",
      .method = HTTP_GET,
      .handler = capture_handler,
      .user_ctx = NULL};

  httpd_uri_t health_uri = {
      .uri = "/health",
      .method = HTTP_GET,
      .handler = health_handler,
      .user_ctx = NULL};

  // Start HTTP server
  Logger::getInstance().info("Webserver start");
  if (httpd_start(&camera_httpd, &config) == ESP_OK)
  {
    httpd_register_uri_handler(camera_httpd, &health_uri);
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    httpd_register_uri_handler(camera_httpd, &shot_uri);
  }
}
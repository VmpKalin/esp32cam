#ifndef CAMERA_HTTP_HANDLERS_H
#define CAMERA_HTTP_HANDLERS_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "esp_http_server.h"  // Add this to define httpd_req_t

// Remove 'static' from declarations
esp_err_t index_handler(httpd_req_t *req);
esp_err_t stream_handler(httpd_req_t *req);
esp_err_t capture_handler(httpd_req_t *req);
esp_err_t health_handler(httpd_req_t *req);

#endif
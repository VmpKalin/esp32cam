#ifndef CAMERA_HTTP_HANDLERS_H
#define CAMERA_HTTP_HANDLERS_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "soc/soc.h"          // Disable brownout problems
#include "soc/rtc_cntl_reg.h" // Disable brownout problems
#include "esp_http_server.h"
#include <HTTPClient.h>
#include "config.h"
#include <libb64/cencode.h>
#include "telegram_utils.h"
#include "logger.h"

void startHttpServer();

esp_err_t index_handler(httpd_req_t *req);
esp_err_t stream_handler(httpd_req_t *req);
esp_err_t capture_handler(httpd_req_t *req);
esp_err_t health_handler(httpd_req_t *req);

#endif
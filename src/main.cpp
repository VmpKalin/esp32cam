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
#include "camera_http_handlers.h"
#include "telegram_utils.h"

// Camera settings for ESP32-CAM AI-THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Variable to store HTTP server
httpd_handle_t camera_httpd = NULL;

void startCameraServer()
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Increase buffer size to handle larger headers
  config.max_uri_handlers = 16;
  config.max_resp_headers = 16;
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.stack_size = 10240; // Increase stack size
  config.recv_wait_timeout = 10;
  config.send_wait_timeout = 10;
  
  // Important: increase max header size
  // config.max_header_len = 1024; // Default is 512 bytes

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
    .user_ctx = NULL
  };
  
  // Start HTTP server
  Serial.println("Webserver start");
  if (httpd_start(&camera_httpd, &config) == ESP_OK)
  {
    httpd_register_uri_handler(camera_httpd, &health_uri);
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    httpd_register_uri_handler(camera_httpd, &shot_uri);
  }
}

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector

  Serial.begin(115200);
  Serial.setDebugOutput(false);

  // Camera initialization
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Quality and frame size
  if (psramFound())
  {
    config.frame_size = FRAMESIZE_VGA; // 640x480
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }
  else
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Camera initialization
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Issue with camera initialization: 0x%x", err);
    return;
  }

  Serial.println("Camera initialized successfully");

  // Add camera sensor settings adjustment
  sensor_t *s = esp_camera_sensor_get();
  if (s)
  {
    // Set camera parameters
    s->set_framesize(s, FRAMESIZE_VGA);      // 640x480
    s->set_quality(s, 10);                   // 0-63, lower is higher quality
    s->set_brightness(s, 0);                 // -2 to 2
    s->set_contrast(s, 0);                   // -2 to 2
    s->set_saturation(s, 0);                 // -2 to 2
    s->set_special_effect(s, 0);             // 0 = no effect
    s->set_whitebal(s, 1);                   // 1 = enable auto white balance
    s->set_awb_gain(s, 1);                   // 1 = enable AWB gain
    s->set_wb_mode(s, 0);                    // 0 = auto mode
    s->set_exposure_ctrl(s, 1);              // 1 = enable auto exposure
    s->set_aec2(s, 1);                       // 1 = enable auto exposure (AEC DSP)
    s->set_ae_level(s, 0);                   // -2 to 2
    s->set_aec_value(s, 300);                // 0 to 1200
    s->set_gain_ctrl(s, 1);                  // 1 = enable auto gain control
    s->set_agc_gain(s, 0);                   // 0 to 30
    s->set_gainceiling(s, (gainceiling_t)0); // 0 to 6
    s->set_bpc(s, 1);                        // 1 = enable black pixel correction
    s->set_wpc(s, 1);                        // 1 = enable white pixel correction
    s->set_raw_gma(s, 1);                    // 1 = enable gamma correction
    s->set_lenc(s, 1);                       // 1 = enable lens correction
    s->set_hmirror(s, 0);                    // 0 = disable horizontal mirror
    s->set_dcw(s, 1);                        // 1 = enable downsize
    s->set_colorbar(s, 0);                   // 0 = disable color bar test
    // For vertical flip (180 degree rotation)
    s->set_vflip(s, 1); // 1 = enable vertical flip

    // // For horizontal mirror
    // s->set_hmirror(s, 1); // 1 = enable horizontal mirror

    // // For a 180 degree rotation, enable both
    // s->set_vflip(s, 1);
    // s->set_hmirror(s, 1);

    Serial.println("Camera sensor settings adjusted");
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi was connected");
  // Output IP address
  Serial.print("ESP32 Camera ip: http://");
  Serial.println(WiFi.localIP());

  delay(1000);

  // Try to send IP address via Telegram
  char ipMessage[100];
  snprintf(ipMessage, sizeof(ipMessage), "Camera IP: http://%s", WiFi.localIP().toString().c_str());
  if (!sendMessageToTelegram(tg_bot_token, tg_chat_id, ipMessage))
  {
    Serial.println("Failed to send Telegram message, but continuing anyway");
  }

  // Start web server for streaming
  startCameraServer();
}

void loop()
{
  // Main loop does nothing - web server handles everything
  delay(10000);
}
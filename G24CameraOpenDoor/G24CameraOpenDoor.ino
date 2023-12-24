cpp#include <ArduinoWebsockets.h>
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "camera_index.h"
#include "Arduino.h"
#include "fd_forward.h"
#include "fr_forward.h"
#include "fr_flash.h"

const char* ssid = "DTQM";
const char* password = "66668888";

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

using namespace websockets;
WebsocketsServer socket_server;

static inline mtmn_config_t app_mtmn_config()
{
   mtmn_config_t mtmn_config = {0};
   mtmn_config.type = FAST;
   mtmn_config.min_face = 80;
   mtmn_config.pyramid = 0.707;
   mtmn_config.pyramid_times = 4;
   mtmn_config.p_threshold.score = 0.6;
   mtmn_config.p_threshold.nms = 0.7;
   mtmn_config.p_threshold.candidate_number = 20;
   mtmn_config.r_threshold.score = 0.7;
   mtmn_config.r_threshold.nms = 0.7;
   mtmn_config.r_threshold.candidate_number = 10;
   mtmn_config.o_threshold.score = 0.7;
   mtmn_config.o_threshold.nms = 0.7;
   mtmn_config.o_threshold.candidate_number = 1;
   return mtmn_config;
}
mtmn_config_t mtmn_config = app_mtmn_config();

face_id_name_list st_face_list;
static dl_matrix3du_t *aligned_face = NULL;

httpd_handle_t camera_httpd = NULL;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  digitalWrite(relay_pin, LOW);
  pinMode(relay_pin, OUTPUT);

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
  //init with high specs to pre-allocate larger buffers
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  app_httpserver_init();
  app_facenet_main();
  socket_server.listen(82);

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

void loop() {
  socket_server.listen(82);
  WebsocketsClient client = socket_server.accept();
  if (client.available()) {
    WebsocketsMessage msg = client.receive();
    if (msg.is_text()) {
      String command = msg.data();
      if (command == "stream_handler") {
        stream_handler();
      } else if (command == "face_handler") {
        face_handler();
      } else if (command == "door_handler") {
        door_handler();
      }
    }
  }
  client.disconnect();
}

void app_httpserver_init() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    httpd_register_uri_handler(camera_httpd, &face_uri);
    httpd_register_uri_handler(camera_httpd, &door_uri);
  }
}

void app_facenet_main() {
  face_id_name_list_init(&st_face_list);
  fr_flash_init(&st_face_list);
  fr_flash_load(&st_face_list);
}

void app_facenet_deinit() {
  fr_flash_save(&st_face_list);
  face_id_name_list_deinit(&st_face_list);
}

esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, (const char *)index_html_gz, index_html_gz_len);
  return ESP_OK;
}

esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
  res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];
  dl_matrix3du_t *image_matrix = NULL;
  bool detected = false;
  int face_id = 0;
  int64_t fr_start = esp_timer_get_time();
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  detected = detect_face(&mtmn_config, fb->buf, fb->width, fb->height, &face_id, part_buf);
  if (detected) {
    image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
    if (!image_matrix) {
      Serial.println("dl_matrix3du_alloc failed");
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item);
    if (align_face(&mtmn_config, image_matrix, aligned_face) == ESP_OK) {
      fr_forward(&st_face_list, aligned_face, &face_id);
    }
    dl_matrix3du_free(image_matrix);
  }
  _jpg_buf_len = fb->len;
  _jpg_buf = fb->buf;
  res = httpd_resp_set_type(req, "image/jpeg");
  if (res == ESP_OK) {
    res = httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=stream.jpg");
  }
  if (res == ESP_OK) {
    res = httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  }
  if (res == ESP_OK) {
    res = httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET");
  }
  if (res == ESP_OK) {
    res = httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
  }
  if (res == ESP_OK) {
    res = httpd_resp_send(req, (const char *)_jpg_buf, _jpg_buf_len);
  }
  esp_camera_fb_return(fb);
  int64_t fr_end = esp_timer_get_time();
  Serial.printf("Face Recognition Time: %lldms\n", (fr_end - fr_start) / 1000);
  return res;
}

esp_err_t face_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char variable[32] = {0,};
  char value[32] = {0,};
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char *)malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
          httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
        if (strcmp(variable, "name") == 0) {
          strncpy(st_face_list.id_name_list[st_face_list.num].name, value, sizeof(st_face_list.id_name_list[st_face_list.num].name));
          Serial.printf("Set name: %s\n", st_face_list.id_name_list[st_face_list.num].name);
        } else if (strcmp(variable, "id") == 0) {
          st_face_list.id_name_list[st_face_list.num].id = atoi(value);
          Serial.printf("Set id: %d\n", st_face_list.id_name_list[st_face_list.num].id);
        }
      }
    }
    free(buf);
  }
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, (const char *)face_html_gz, face_html_gz_len);
  return ESP_OK;
}

esp_err_t door_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;
  char variable[32] = {0,};
  char value[32] = {0,};
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char *)malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
          httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
        if (strcmp(variable, "action") == 0) {
          if (strcmp(value, "open") == 0) {
            digitalWrite(relay_pin, HIGH);
            delay(1000);
            digitalWrite(relay_pin, LOW);
            Serial.println("Door opened");
          }
        }
      }
    }
    free(buf);
  }
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, (const char *)door_html_gz, door_html_gz_len);
  return ESP_OK;
}

#include <Arduino.h>

#include "esp_camera.h"
#include "esp_http_server.h"

#include <WiFi.h>
#include "../../robot_PTZ/.pio/build/esp32cam/ESP32Servo/src/ESP32Servo.h"

#include <Wire.h>
#include "../.pio/libdeps/esp32cam/BH1750/src/BH1750.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"


// wifi
#define AP_ssid "ESP_PTZ"
#define PART_BOUNDARY "gc0p4Jq0M2Yt08jU534c0p"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
IPAddress local_IP;
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

//https://blog.kalan.dev/2021-03-13-html-form-data

// servo
#define level_servo_pin GPIO_NUM_12
#define vertical_servo_pin GPIO_NUM_4
#define servo_left_W 30
#define servo_stop_W 90
#define servo_right_W 119
Servo level_servo, vertical_servo;
int16_t vertical_val;
uint8_t servo_move_val = 20;

// BH1750
#define MAX_LUX 150
#define I2C_SDA GPIO_NUM_15
#define I2C_SCL GPIO_NUM_13
BH1750 lightMeter;

// light
#define light_pin GPIO_NUM_2

// camera
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


// http

esp_err_t camera_handler(httpd_req_t *req) {
    camera_config_t config_camera;

    config_camera.ledc_channel = LEDC_CHANNEL_0;
    config_camera.ledc_timer = LEDC_TIMER_0;
    config_camera.pin_d0 = Y2_GPIO_NUM;
    config_camera.pin_d1 = Y3_GPIO_NUM;
    config_camera.pin_d2 = Y4_GPIO_NUM;
    config_camera.pin_d3 = Y5_GPIO_NUM;
    config_camera.pin_d4 = Y6_GPIO_NUM;
    config_camera.pin_d5 = Y7_GPIO_NUM;
    config_camera.pin_d6 = Y8_GPIO_NUM;
    config_camera.pin_d7 = Y9_GPIO_NUM;
    config_camera.pin_xclk = XCLK_GPIO_NUM;
    config_camera.pin_pclk = PCLK_GPIO_NUM;
    config_camera.pin_vsync = VSYNC_GPIO_NUM;
    config_camera.pin_href = HREF_GPIO_NUM;
    config_camera.pin_sscb_sda = SIOD_GPIO_NUM;
    config_camera.pin_sscb_scl = SIOC_GPIO_NUM;
    config_camera.pin_pwdn = PWDN_GPIO_NUM;
    config_camera.pin_reset = RESET_GPIO_NUM;
    config_camera.xclk_freq_hz = 20000000;
    config_camera.pixel_format = PIXFORMAT_JPEG;

    if(psramFound()){
        config_camera.frame_size = FRAMESIZE_VGA;
        config_camera.jpeg_quality = 10;
        config_camera.fb_count = 2;
    } else {
        config_camera.frame_size = FRAMESIZE_SVGA;
        config_camera.jpeg_quality = 12;
        config_camera.fb_count = 1;
    }
    esp_err_t err = esp_camera_init(&config_camera);
    if (err != ESP_OK) {
        //Serial.printf("Camera init failed with error 0x%x", err);
        return  ESP_FAIL;
    }

    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }
//https://ithelp.ithome.com.tw/articles/10300809?sc=rss.iron

    sensor_t * s = esp_camera_sensor_get();
    s->set_vflip(s, 1);
    s->set_framesize(s, FRAMESIZE_VGA);
    s->set_quality(s, 10);
    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            //Serial.println("Camera capture failed");
            res = ESP_FAIL;
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if(_jpg_buf){
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK){
            break;
        }
    }
    return res;
}
static esp_err_t command_handler(httpd_req_t *req) {

    char content[32] = {0,};

    size_t recv_size = min(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }


    if (!strcmp(content, "cmd=top")) {
        vertical_val += servo_move_val;
        vertical_servo.write(vertical_val);
    }
    else if (!strcmp(content, "cmd=bottom")) {
        vertical_val -= servo_move_val;
        vertical_servo.write(vertical_val);
    }
    else if (!strcmp(content, "cmd=leftT")) {
        level_servo.write(servo_left_W);
    }
    else if (!strcmp(content, "cmd=leftF")) {
        level_servo.write(servo_stop_W);
    }
    else if (!strcmp(content, "cmd=rightT")) {
        level_servo.write(servo_right_W);
    }
    else if (!strcmp(content, "cmd=rightF")) {
        level_servo.write(servo_stop_W);
    }
    else if (!strcmp(content, "cmd=init")) {
        vertical_servo.write(0);
    }
    else if (!strcmp(content, "cmd=attackT")) {
        digitalWrite(light_pin, HIGH);
    }
    else if (!strcmp(content, "cmd=attackF")) {
        digitalWrite(light_pin, LOW);
    }
    //Serial.println(content);

    return ESP_OK;
}
static esp_err_t light_handler(httpd_req_t *req) {


    while (true) {

        char data[4];
        float lux = lightMeter.readLightLevel();
        if (lux > MAX_LUX){
            data[0] = '1';
        }
        else {
            data[0] = '0';
        }
        //Serial.println(lux);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, data, strlen(data));
    }


    return ESP_OK;
}



void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // init
    //Serial.begin(115200);

    //Wire.begin(I2C_SDA, I2C_SCL);
    //lightMeter.begin();

    WiFi.softAP(AP_ssid);

    IPAddress AP_LOCAL_IP(192, 168, 1, 160);
    IPAddress AP_GATEWAY_IP(192, 168, 1, 4);
    IPAddress AP_NETWORK_MASK(255, 255, 255, 0);
    if (!WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY_IP, AP_NETWORK_MASK)) {
        //Serial.println("AP Config Failed");
        return;
    }

    level_servo.attach(level_servo_pin);
    vertical_servo.attach(vertical_servo_pin);


    //Serial.println("");
    local_IP = WiFi.softAPIP();
    //Serial.println(local_IP);

    // light
    pinMode(light_pin, OUTPUT);


    // http
    httpd_config_t config_http = HTTPD_DEFAULT_CONFIG();
    config_http.server_port = 80;
    config_http.stack_size = 8096;

    httpd_uri_t camera_uri = {
            .uri      = "/camera",
            .method   = HTTP_GET,
            .handler  = camera_handler,
            .user_ctx = NULL
    };
    httpd_uri_t command_uri = {
            .uri      = "/command",
            .method   = HTTP_POST,
            .handler  = command_handler,
            .user_ctx = NULL
    };
    httpd_uri_t light_uri = {
            .uri      = "/light",
            .method   = HTTP_GET,
            .handler  = light_handler,
            .user_ctx = NULL
    };


    if (httpd_start(&camera_httpd, &config_http) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &command_uri);
        //httpd_register_uri_handler(camera_httpd, &light_uri);

    }

    config_http.server_port += 1 ;
    config_http.ctrl_port += 1 ;

    if (httpd_start(&stream_httpd, &config_http) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &camera_uri);
    }


}


void loop() {
}
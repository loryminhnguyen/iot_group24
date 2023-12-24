import websockets
import esp_http_server
import esp_timer
import esp_camera
import camera_index
import Arduino
import fd_forward
import fr_forward
import fr_flash

ssid = "ĐTQM"
password = "66668888"
ENROLL_CONFIRM_TIMES = 5
FACE_ID_SAVE_NUMBER = 7

CAMERA_MODEL_AI_THINKER

camera_pins = camera_pins

socket_server = websockets.WebsocketsServer()
fb = None
current_millis = None
last_detected_millis = 0
relay_pin = 4  # pin 12 can also be used
door_opened_millis = 0
interval = 5000  # open lock for ... milliseconds
face_recognised = False


def app_facenet_main():
    app_mtmn_config = mtmn_config()
    mtmn_config = app_mtmn_config
    st_face_list = face_id_name_list()
    aligned_face = None
    camera_httpd = None

    def __init__(self):
        self.camera_httpd = None

    def app_httpserver_init():
        config = esp_http_server.httpd_config_t()
        camera_httpd = esp_http_server.httpd_start(config)
        index_uri = esp_http_server.httpd_uri_t()
        esp_http_server.httpd_register_uri_handler(camera_httpd, index_uri)

    def setup():
        Serial.begin(115200)
        Serial.setDebugOutput(True)
        Serial.println()
        digitalWrite(relay_pin, LOW)
        pinMode(relay_pin, OUTPUT)
        config = camera_config_t()
        config.ledc_channel = LEDC_CHANNEL_0
        config.ledc_timer = LEDC_TIMER_0
        config.pin_d0 = Y2_GPIO_NUM
        config.pin_d1 = Y3_GPIO_NUM
        config.pin_d2 = Y4_GPIO_NUM
        config.pin_d3 = Y5_GPIO_NUM
        config.pin_d4 = Y6_GPIO_NUM
        config.pin_d5 = Y7_GPIO_NUM
        config.pin_d6 = Y8_GPIO_NUM
        config.pin_d7 = Y9_GPIO_NUM
        config.pin_xclk = XCLK_GPIO_NUM
        config.pin_pclk = PCLK_GPIO_NUM
        config.pin_vsync = VSYNC_GPIO_NUM
        config.pin_href = HREF_GPIO_NUM
        config.pin_sscb_sda = SIOD_GPIO_NUM
        config.pin_sscb_scl = SIOC_GPIO_NUM
        config.pin_pwdn = PWDN_GPIO_NUM
        config.pin_reset = RESET_GPIO_NUM
        config.xclk_freq_hz = 20000000
        config.pixel_format = PIXFORMAT_JPEG
        if psramFound():
            config.frame_size = FRAMESIZE_UXGA
            config.jpeg_quality = 10
            config.fb_count = 2
        else:
            config.frame_size = FRAMESIZE_SVGA
            config.jpeg_quality = 12
            config.fb_count = 1
        if CAMERA_MODEL_ESP_EYE:
            pinMode(13, INPUT_PULLUP)
            pinMode(14, INPUT_PULLUP)
        esp_err_t err = esp_camera_init(config)
        if err != ESP_OK:
            Serial.printf("Camera init failed with error 0x%x", err)
            return
        sensor_t * s = esp_camera_sensor_get()
        s->set_framesize(s, FRAMESIZE_QVGA)
        if CAMERA_MODEL_M5STACK_WIDE:
            s->set_vflip(s, 1)
            s->set_hmirror(s, 1)
        WiFi.begin(ssid, password)
        while WiFi.status() != WL_CONNECTED:
            delay(500)
            Serial.print(".")
        Serial.println("")
        Serial.println("WiFi connected")
        app_httpserver_init()
        app_facenet_main()
        socket_server.listen(82)
        Serial.print("Camera Ready! Use 'http://")
        Serial.print(WiFi.localIP())
        Serial.println("' to connect")

    def index_handler(req):
        httpd_resp_set_type(req, "text/html")
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip")
        return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len)

    def send_face_list(client):
        client.send("delete_faces")
        head = st_face_list.head
        add_face = [64]
        for i in range(st_face_list.count):
            sprintf(add_face, "listface:%s", head->id_name)
            client.send(add_face)
            head = head->next

    def delete_all_faces(client):
        delete_face_all_in_flash_with_name(&st_face_list)
        client.send("delete_faces")

    def handle_message(client, msg):
        if msg.data() == "stream":
            g_state = START_STREAM
            client.send("STREAMING")
        if msg.data() == "detect":
            g_state = START_DETECT
            client.send("DETECTING")
        if msg.data().substring(0, 8) == "capture:":
            g_state = START_ENROLL
            person = [FACE_ID_SAVE_NUMBER * ENROLL_NAME_LEN]
            msg.data().substring(8).toCharArray(person, sizeof(person))
            memcpy(st_name.enroll_name, person, strlen(person) + 1)
            client.send("CAPTURING")
        if msg.data() == "recognise":
            g_state = START_RECOGNITION
            client.send("RECOGNISING")
        if msg.data().substring(0, 7) == "remove:":
            person = [ENROLL_NAME_LEN * FACE_ID_SAVE_NUMBER]
            msg.data().substring(7).toCharArray(person, sizeof(person))
            delete_face_id_in_flash_with_name(&st_face_list, person)
            send_face_list(client)
        if msg.data() == "delete_all":
            delete_all_faces(client)

    def open_door(client):
        if digitalRead(relay_pin) == LOW:
            digitalWrite(relay_pin, HIGH)
            Serial.println("Door Unlocked")
            client.send("door_open")
            door_opened_millis = millis()

    def loop():
        client = socket_server.accept()
        client.onMessage(handle_message)
        image_matrix = dl_matrix3du_alloc(1, 320, 240, 3)
        out_res = http_img_process_result()
        out_res.image = image_matrix->item
        send_face_list(client)
        client.send("STREAMING")
        while client.available():
            client.poll()
            if millis() - interval > door_opened_millis:
                digitalWrite(relay_pin, LOW)
            fb = esp_camera_fb_get()
            if g_state == START_DETECT or g_state == START_ENROLL or g_state == START_RECOGNITION:
                out_res.net_boxes = NULL
                out_res.face_id = NULL
                fmt2rgb888(fb->buf, fb->len, fb->format, out_res.image)
                out_res.net_boxes = face_detect(image_matrix, &mtmn_config)
                if out_res.net_boxes:
                    if align_face(out_res.net_boxes, image_matrix, aligned_face) == ESP_OK:
                        out_res.face_id = get_face_id(aligned_face)
                        last_detected_millis = millis()
                        if g_state == START_DETECT:
                            client.send("FACE DETECTED")
                        if g_state == START_ENROLL:
                            left_sample_face = do_enrollment(&st_face_list, out_res.face_id)
                            enrolling_message = [64]
                            sprintf(enrolling_message, "SAMPLE NUMBER %d FOR %s", ENROLL_CONFIRM_TIMES - left_sample_face, st_name.enroll_name)
                            client.send(enrolling_message)
                            if left_sample_face == 0:
                                ESP_LOGI(TAG, "Enrolled Face ID: %s", st_face_list.tail->id_name)
                                g_state = START_STREAM
                                captured_message = [64]
                                sprintf(captured_message, "FACE CAPTURED FOR %s", st_face_list.tail->id_name)
                                client.send(captured_message)
                                send_face_list(client)
                        if g_state == START_RECOGNITION and (st_face_list.count > 0):
                            f = recognize_face_with_name(&st_face_list, out_res.face_id)
                            if f:
                                recognised_message = [64]
                                sprintf(recognised_message, "DOOR OPEN FOR %s", f->id_name)
                                open_door(client)
                                client.send(recognised_message)
                            else:
                                client.send("FACE NOT RECOGNISED")
                        dl_matrix3d_free(out_res.face_id)
                else:
                    if g_state != START_DETECT:
                        client.send("NO FACE DETECTED")
                if g_state == START_DETECT and millis() - last_detected_millis > 500:
                    client.send("DETECTING")
            client.sendBinary((const char *)fb->buf, fb->len)
            esp_camera_fb_return(fb)
            fb = NULL



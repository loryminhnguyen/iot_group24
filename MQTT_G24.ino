#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>

const char* ssid = "Phong802";
const char* password = "20021997";
const char* mqtt_server = "broker.hivemq.com"; // Thay đổi thành MQTT broker của bạn

const char* fan_control_topic = "/home/fan/control";
const char* light_control_topic = "/home/light/control";
const char* heater_control_topic = "/home/heater/control";
const char* temp_topic = "/home/sensor/temperature";
const char* hum_topic = "/home/sensor/humidity";

const int FAN_PIN = 2;     // Thay đổi thành chân kết nối relay của quạt
const int LIGHT_PIN = 5;   // Thay đổi thành chân kết nối relay của bóng đèn
const int HEATER_PIN = 4;  // Thay đổi thành chân kết nối relay của máy sưởi
const int DHT_PIN = 15;    // Thay đổi thành chân kết nối cảm biến DHT22

WiFiClient espClient;
PubSubClient client(espClient);
DHTesp dht;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("Message arrived [" + String(topic) + "] " + message);

  // Kiểm tra chủ đề và thực hiện các hành động tương ứng
  if (String(topic) == fan_control_topic) {
    if (message == "ON") {
      digitalWrite(FAN_PIN, HIGH); // Bật quạt
    } else if (message == "OFF") {
      digitalWrite(FAN_PIN, LOW); // Tắt quạt
    }
  } else if (String(topic) == light_control_topic) {
    if (message == "ON") {
      digitalWrite(LIGHT_PIN, HIGH); // Bật bóng đèn
    } else if (message == "OFF") {
      digitalWrite(LIGHT_PIN, LOW); // Tắt bóng đèn
    }
  } else if (String(topic) == heater_control_topic) {
    if (message == "ON") {
      digitalWrite(HEATER_PIN, HIGH); // Bật máy sưởi
    } else if (message == "OFF") {
      digitalWrite(HEATER_PIN, LOW); // Tắt máy sưởi
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("Connected to MQTT broker");
      client.subscribe(fan_control_topic);
      client.subscribe(light_control_topic);
      client.subscribe(heater_control_topic);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(HEATER_PIN, OUTPUT);

  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  dht.setup(DHT_PIN, DHTesp::DHT22);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  // Đọc dữ liệu từ cảm biến và gửi lên MQTT broker
  TempAndHumidity data = dht.getTempAndHumidity();
  String temp = String(data.temperature, 2);
  String hum = String(data.humidity, 1);

  client.publish(temp_topic, temp.c_str());
  client.publish(hum_topic, hum.c_str());

  Serial.print("Temperature: ");
  Serial.println(temp);
  Serial.print("Humidity: ");
  Serial.println(hum);
  

  // Thêm các hành động khác ở đây nếu cần
}

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <time.h>  // Them thu vien time.h de lay thoi gian

// Cau hinh WiFi va MQTT
const char* ssid = "Redmi 12";
const char* password = "88888888";
const char* mqtt_server = "192.168.231.46";  // Dia chi IP Mosquitto MQTT Broker

// Cau hinh cam bien DHT
#define DPIN 4        // (D2)
#define DTYPE DHT11   // Xac dinh loai cam bien DHT 11

DHT dht(DPIN, DTYPE);

// Cau hinh cam bien anh sang 
#define ANALOG_INPUT_PIN A0  // Chan analog (A0 tren ESP8266)
#define DIGITAL_INPUT_PIN 5  // Chan digital (D1)

// Cau hinh chan LED
#define LED_PIN_1 14  // Chan D5
#define LED_PIN_2 12  // Chan D6
#define LED_PIN_3 13  // Chan D7
#define LED_PIN_0 16  // Chan D0 

// Cau hinh MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (100)
char msg[MSG_BUFFER_SIZE];

bool warningLedBlink = false;  // Trang thai nhap nhay cua LED canh bao
unsigned long previousMillis = 0;
const long interval = 500;  // delay 0.5 

// Cau hinh mui gio 
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;  // GMT+7 (Chinh sua theo mui gio cua ban)
const int daylightOffset_sec = 0;

// Ham lay thoi gian hien tai
String getFormattedTime() {
  time_t now = time(nullptr);
  if (now < 0) { // Kiểm tra thời gian hợp lệ
    return "N/A"; // Trả về N/A nếu thời gian chưa được đồng bộ
  }
 
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);  
  char buffer[30];
  strftime(buffer, 30, "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

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

  // Cau hinh de lay thoi gian
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Waiting for time sync...");
 
  // Thêm thời gian chờ cho đến khi có thời gian hợp lệ
  for (int i = 0; i < 10; i++) {
    if (time(nullptr) > 0) {
      break; // Thoát nếu thời gian hợp lệ
    }
    delay(1000);
    Serial.print(".");
  }
 
  Serial.println("\nTime synchronized");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
 
  // Chuyen payload thanh chuoi
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
 
  Serial.println(message);

  // Bat tat LED tuong ung voi tin nhan
  if (message == "led1-on") {
    digitalWrite(LED_PIN_1, HIGH);  // Bat LED 1
  } else if (message == "led1-off") {
    digitalWrite(LED_PIN_1, LOW);   // Tat LED 1
  } else if (message == "led2-on") {
    digitalWrite(LED_PIN_2, HIGH);  // Bat LED 2
  } else if (message == "led2-off") {
    digitalWrite(LED_PIN_2, LOW);   // Tat LED 2
  } else if (message == "led3-on") {
    digitalWrite(LED_PIN_3, HIGH);  // Bat LED 3
  } else if (message == "led3-off") {
    digitalWrite(LED_PIN_3, LOW);   // Tat LED 3
  } else if (message == "ON") {
    digitalWrite(LED_PIN_1, HIGH);
    digitalWrite(LED_PIN_2, HIGH);
    digitalWrite(LED_PIN_3, HIGH);
  } else if (message == "OFF") {
    digitalWrite(LED_PIN_1, LOW);
    digitalWrite(LED_PIN_2, LOW);
    digitalWrite(LED_PIN_3, LOW);
  } else if (message == "led4-on") {
    warningLedBlink = true;  // Bat che do nhap nhay LED canh bao
  } else if (message == "led4-off") {
    warningLedBlink = false;  // Tat che do nhap nhay LED canh bao
    digitalWrite(LED_PIN_0, LOW);  // Tat LED canh bao
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Ket noi voi ten nguoi dung va mat khau
    if (client.connect(clientId.c_str(), "tutran", "1234")) {
      Serial.println("connected");
      client.subscribe("in");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);  // Khoi tao chan LED
  pinMode(DIGITAL_INPUT_PIN, INPUT);  // Khoi tao chan digital
  pinMode(LED_PIN_1, OUTPUT);  // Khoi tao chan LED 1
  pinMode(LED_PIN_2, OUTPUT);  // Khoi tao chan LED 2
  pinMode(LED_PIN_3, OUTPUT);  // Khoi tao chan LED 3
  pinMode(LED_PIN_0, OUTPUT);  // Khoi tao chan LED canh bao
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 2003);
  client.setCallback(callback);
  dht.begin();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 5000) {  // Chu ky phat 5 giay
    lastMsg = now;

    // Doc du lieu tu cam bien DHT
    float tc = dht.readTemperature(false);  // Doc nhiet do (°C)
    float hu = dht.readHumidity();          // Doc do am

    // Doc gia tri tu cac chan analog va digital
    int analogValue = analogRead(ANALOG_INPUT_PIN); // Doc gia tri tu chan A0 (cam bien anh sang)
    int digitalValue = digitalRead(DIGITAL_INPUT_PIN); // Doc gia tri tu GPIO5 (chan digital)

    // Kiem tra loi doc cam bien DHT
    if (isnan(tc) || isnan(hu)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    // Lay thoi gian thuc tu NTP
    String currentTime = getFormattedTime();

    //  luong mua
    int rainfall = random(0, 101);

    // Phat du lieu nhiet do, do am, cam bien va thoi gian
    snprintf(msg, MSG_BUFFER_SIZE, "Thoigian: %s, Nhietdo: %.2f C, Doam: %.0f%%, Anhsang: %d Lux, Luongmua: %d mm", currentTime.c_str(), tc, hu, analogValue, rainfall);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("out", msg);
  }

  // Xu ly nhap nhay LED canh bao
  if (warningLedBlink) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      int state = digitalRead(LED_PIN_0);
      digitalWrite(LED_PIN_0, !state);  // Dao trang thai LED
    }
  }
}

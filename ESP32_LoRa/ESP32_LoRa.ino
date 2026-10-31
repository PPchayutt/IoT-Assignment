#include <SPI.h>
#include <LoRa.h>
#include <PubSubClient.h>

#if defined(ESP32)
  #include <WiFi.h>
  #define SCK_PIN   4
  #define MISO_PIN  5
  #define MOSI_PIN  6
  #define NSS_PIN   7
  #define RST_PIN   3
  #define DIO0_PIN  2
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #define NSS_PIN   15  // D8
  #define RST_PIN   16  // D0
  #define DIO0_PIN  5   // D1
#endif

// ==========================================
// 1. ตั้งค่า Wi-Fi บ้าน และ MQTT Broker
// ==========================================
const char* ssid = "Rattanamanee";
const char* password = "0816868621";

const char* mqtt_server = "mqtt-dashboard.com";
const int mqtt_port = 1883;

const char* TOPIC_UPLINK = "lora/node1/data";
const char* TOPIC_DOWNLINK = "lora/node1/cmd";

#define LORA_FREQ 923E6

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// จำคำสั่งสถานะไฟล่าสุดที่สั่งมาจากเว็บหรือ AI Chat
String targetLoraLed = "";

void sendLoRaDownlink(String cmd) {
  LoRa.beginPacket();
  LoRa.print(cmd);
  LoRa.endPacket();
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim();
  msg.toUpperCase();
  targetLoraLed = msg;

  Serial.print("📥 [MQTT -> LoRa Downlink] Sending: ");
  Serial.println(targetLoraLed);

  // ยิงคลื่นวิทยุ LoRa ส่งคำสั่งไปหา Arduino ทันที 2 ครั้งติดกันกันพลาด
  sendLoRaDownlink(targetLoraLed);
  delay(120);
  sendLoRaDownlink(targetLoraLed);
}

void setup_wifi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
}

void reconnect_mqtt() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT Broker...");
    String clientId = "LoRaGW-Home-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("Connected!");
      mqttClient.subscribe(TOPIC_DOWNLINK);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 3 seconds");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== Starting Single-Channel LoRa Gateway (923 MHz) ===");

#if defined(ESP32)
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, NSS_PIN);
#else
  SPI.begin();
#endif

  LoRa.setPins(NSS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("❌ Starting LoRa RFM95W failed!");
    while (1) { delay(1000); }
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSyncWord(0x34);
  Serial.println("✅ LoRa RFM95W Initialized OK at 923 MHz!");

  setup_wifi();
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(onMqttMessage);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnect_mqtt();
  }
  mqttClient.loop();

  // คอยดักฟังคลื่นวิทยุ LoRa จากบอร์ด Arduino Uno R4
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    Serial.println("----------------------------------------");
    Serial.println("📡 [LoRa Uplink Received] " + incoming);

    // ถ้าระบบเคยสั่งเปิด/ปิดไฟไว้ แต่บอร์ด Arduino ยังไม่ได้เปลี่ยนสถานะตาม (เช่น ติดอ่าน DHT11 ตอนส่งรอบแรก)
    // ให้ส่งคำสั่งซ้ำในจังหวะ RX1 Window (หลังรับ Uplink 150ms) ทันที
    if (targetLoraLed == "ON" && incoming.indexOf("\"lora_led\":\"OFF\"") != -1) {
      delay(150);
      Serial.println("🔄 [RX1 Retry] Re-sending ON to LoRa Node...");
      sendLoRaDownlink("ON");
    } else if (targetLoraLed == "OFF" && incoming.indexOf("\"lora_led\":\"ON\"") != -1) {
      delay(150);
      Serial.println("🔄 [RX1 Retry] Re-sending OFF to LoRa Node...");
      sendLoRaDownlink("OFF");
    } else {
      // ส่งข้อมูลขึ้น MQTT Broker เมื่อสถานะตรงกันแล้ว
      mqttClient.publish(TOPIC_UPLINK, incoming.c_str());
      Serial.println("☁️ Forwarded to MQTT Topic: " + String(TOPIC_UPLINK));
    }
  }
}
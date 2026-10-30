#include <SPI.h>
#include <LoRa.h>
#include <PubSubClient.h>

// ตรวจสอบรุ่นบอร์ดอัตโนมัติ (ใช้ได้ทั้ง ESP32-C3 และ ESP8266)
#if defined(ESP32)
#include <WiFi.h>
// ขาสำหรับ ESP32-C3 SuperMini
#define SCK_PIN 4
#define MISO_PIN 5
#define MOSI_PIN 6
#define NSS_PIN 7
#define RST_PIN 3
#define DIO0_PIN 2
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
// ขาสำหรับ ESP8266 NodeMCU
#define NSS_PIN 15  // D8
#define RST_PIN 16  // D0
#define DIO0_PIN 5  // D1
#endif

// ==========================================
// 1. ตั้งค่า Wi-Fi บ้าน และ MQTT Broker
// ==========================================
const char* ssid = "Rattanamanee";
const char* password = "0816868621";

const char* mqtt_server = "mqtt-dashboard.com";
const int mqtt_port = 1883;

const char* TOPIC_UPLINK = "lora/node1/data";   // Topic ส่งค่าเซนเซอร์ไปให้ Raspberry Pi MCP
const char* TOPIC_DOWNLINK = "lora/node1/cmd";  // Topic รับคำสั่งจาก AI Chat ส่งกลับไปหา LoRa Node

// ความถี่คลื่น LoRa 923 MHz (ตรงกับเสา 923 MHz ของคุณ)
#define LORA_FREQ 923E6

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ฟังก์ชันเมื่อมีคำสั่งจาก MQTT (เช่น สั่งจาก AI Chat) ให้ส่งคลื่น LoRa กลับไปหา Node
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.print("[MQTT -> LoRa Downlink] Sending: ");
  Serial.println(msg);

  // ส่งคลื่นวิทยุ LoRa กลับไปยัง End Node
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
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

  // 1. เริ่มต้นการทำงานของโมดูล RFM95W
#if defined(ESP32)
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, NSS_PIN);
#else
  SPI.begin();
#endif

  LoRa.setPins(NSS_PIN, RST_PIN, DIO0_PIN);

  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("❌ Starting LoRa RFM95W failed! กรุณาเช็กสายไฟ SPI และจุดบัดกรี");
    while (1) { delay(1000); }
  }

  // ตั้งค่า Spreading Factor และ SyncWord ให้ตรงกับฝั่งส่ง
  LoRa.setSpreadingFactor(7);
  LoRa.setSyncWord(0x34);
  Serial.println("✅ LoRa RFM95W Initialized OK at 923 MHz!");

  // 2. เชื่อมต่อ Wi-Fi และ MQTT
  setup_wifi();
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(onMqttMessage);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnect_mqtt();
  }
  mqttClient.loop();

  // 3. คอยดักฟังคลื่นวิทยุ LoRa จากบอร์ด Arduino Uno R4
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    Serial.println("----------------------------------------");
    Serial.println("📡 [LoRa Uplink Received]");
    Serial.println("Payload: " + incoming);
    Serial.println("RSSI: " + String(rssi) + " dBm | SNR: " + String(snr) + " dB");

    // ส่งข้อมูลขึ้น MQTT Broker ไปยัง Topic lora/node1/data ทันที
    mqttClient.publish(TOPIC_UPLINK, incoming.c_str());
    Serial.println("☁️ Forwarded to MQTT Topic: " + String(TOPIC_UPLINK));
  }
}
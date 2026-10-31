#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>

// 1. ตั้งค่าเซนเซอร์ DHT11 ที่ขา D4 และไฟ LED บนบอร์ด
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ใช้ไฟ LED บนบอร์ด Uno R4 (หรือต่อ LED เพิ่มที่ขา D7 ก็ได้)
#define LORA_LED_PIN LED_BUILTIN

// 2. ขาของแผ่น Cytron LoRa Shield
#define NSS_PIN   10
#define RST_PIN   9
#define DIO0_PIN  2

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 5000; // ส่งค่าทุก 5 วินาที
String loraLedStatus = "OFF";

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  pinMode(LORA_LED_PIN, OUTPUT);
  digitalWrite(LORA_LED_PIN, LOW);

  dht.begin();

  LoRa.setPins(NSS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(923E6)) {
    Serial.println("หาบอร์ด LoRa ไม่เจอ! ลองขยับแผ่น Shield ให้แน่นขึ้น");
    while (1);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSyncWord(0x34);
  Serial.println("=== LoRa End Node พร้อมรับ-ส่งข้อมูลแล้ว! ===");
}

void loop() {
  // ส่วนที่ 1: ส่งค่าอุณหภูมิ ความชื้น และสถานะไฟของ LoRa Node ทุก 5 วินาที (ไม่ใช้ delay เพื่อไม่ให้บล็อกการรับคำสั่ง)
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      t = 0.0;
      h = 0.0;
    }

    char payload[80];
    snprintf(payload, sizeof(payload), "{\"temp\":%.1f,\"humid\":%.1f,\"lora_led\":\"%s\"}", t, h, loraLedStatus.c_str());

    Serial.print("📡 กำลังส่งข้อมูล Uplink: ");
    Serial.println(payload);

    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();
  }

  // ส่วนที่ 2: คอยดักฟังคำสั่ง Downlink ที่ส่งมาจาก AI Chat / MQTT ผ่าน Gateway
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String cmd = "";
    while (LoRa.available()) {
      cmd += (char)LoRa.read();
    }
    cmd.trim();
    Serial.print("📥 ได้รับคำสั่ง Downlink จาก AI/MQTT: ");
    Serial.println(cmd);

    if (cmd.equalsIgnoreCase("ON") || cmd.equalsIgnoreCase("LORA_ON")) {
      digitalWrite(LORA_LED_PIN, HIGH);
      loraLedStatus = "ON";
      Serial.println("💡 สั่งเปิดไฟ LED บนบอร์ด LoRa Node แล้ว!");
    } else if (cmd.equalsIgnoreCase("OFF") || cmd.equalsIgnoreCase("LORA_OFF")) {
      digitalWrite(LORA_LED_PIN, LOW);
      loraLedStatus = "OFF";
      Serial.println("🌑 สั่งปิดไฟ LED บนบอร์ด LoRa Node แล้ว!");
    }
  }
}
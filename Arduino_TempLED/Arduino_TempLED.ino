#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>

// 1. ตั้งค่าเซนเซอร์ DHT11 ที่ขา D4
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ใช้ขา A0 ควบคุมไฟ LED (แยกฝั่งจากขา LoRa/SPI เด็ดขาด 100%)
#define LORA_LED_PIN A0

// 2. ขาของแผ่น Cytron LoRa Shield
#define NSS_PIN 10
#define RST_PIN 9
#define DIO0_PIN 2

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 5000;  // ส่งค่าทุก 5 วินาที
String loraLedStatus = "OFF";
float lastTemp = 0.0;
float lastHumid = 0.0;

void sendUplinkPayload() {
  char payload[80];
  snprintf(payload, sizeof(payload), "{\"temp\":%.1f,\"humid\":%.1f,\"lora_led\":\"%s\"}", lastTemp, lastHumid, loraLedStatus.c_str());

  Serial.print("📡 กำลังส่งข้อมูล Uplink: ");
  Serial.println(payload);

  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000)
    ;

  // ตั้งค่าขา A0 เป็น Output และทดสอบเปิดไฟสว่าง 1 วินาทีตอนเริ่มบอร์ดแล้วดับลง
  pinMode(LORA_LED_PIN, OUTPUT);
  digitalWrite(LORA_LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LORA_LED_PIN, LOW);

  dht.begin();

  LoRa.setPins(NSS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(923E6)) {
    Serial.println("❌ หาบอร์ด LoRa ไม่เจอ! ลองกดแผ่น Cytron Shield ลงบนบอร์ด Arduino ให้แน่นขึ้น");
    while (1)
      ;
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSyncWord(0x34);
  Serial.println("✅ LoRa End Node พร้อมรับ-ส่งข้อมูลแล้ว (LED Pin A0)!");
}

void loop() {
  // ส่วนที่ 1: อ่านค่า DHT11 และส่ง Uplink ทุก 5 วินาที
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      lastTemp = t;
      lastHumid = h;
    }

    sendUplinkPayload();
  }

  // ส่วนที่ 2: คอยดักฟังคำสั่ง Downlink จาก AI Chat / Web Dashboard
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String cmd = "";
    while (LoRa.available()) {
      cmd += (char)LoRa.read();
    }
    cmd.trim();
    Serial.print("📥 ได้รับคำสั่ง Downlink: ");
    Serial.println(cmd);

    if (cmd.equalsIgnoreCase("ON") || cmd.equalsIgnoreCase("LORA_ON")) {
      digitalWrite(LORA_LED_PIN, HIGH);
      loraLedStatus = "ON";
      Serial.println("💡 สั่งเปิดไฟ LED (ขา A0) บน LoRa Node แล้ว!");
      delay(50);
      sendUplinkPayload();
      lastSendTime = millis();
    } else if (cmd.equalsIgnoreCase("OFF") || cmd.equalsIgnoreCase("LORA_OFF")) {
      digitalWrite(LORA_LED_PIN, LOW);
      loraLedStatus = "OFF";
      Serial.println("🌑 สั่งปิดไฟ LED (ขา A0) บน LoRa Node แล้ว!");
      delay(50);
      sendUplinkPayload();
      lastSendTime = millis();
    }
  }
}
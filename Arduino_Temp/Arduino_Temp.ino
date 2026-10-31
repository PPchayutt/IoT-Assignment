#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>

// ตั้งค่าเซนเซอร์ DHT11 ที่ต่อไว้ตรงขา D4
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ขาที่บังคับของแผ่น Cytron LoRa Shield
#define NSS_PIN 10
#define RST_PIN 9
#define DIO0_PIN 2

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000)
    ;

  dht.begin();  // เปิดใช้งานเซนเซอร์ DHT11

  // ตั้งค่าให้บอร์ดส่งคลื่น LoRa 923 MHz
  LoRa.setPins(NSS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(923E6)) {
    Serial.println("หาบอร์ด LoRa ไม่เจอ! ลองขยับแผ่น Shield ให้แน่นขึ้น");
    while (1)
      ;
  }

  // ตั้งรหัสผ่านคลื่นวิทยุให้ตรงกับ Gateway
  LoRa.setSpreadingFactor(7);
  LoRa.setSyncWord(0x34);
  Serial.println("พร้อมส่งข้อมูลแล้ว!");
}

void loop() {
  // อ่านค่าจากเซนเซอร์ DHT11
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // สร้างข้อความแบบ JSON เพื่อเตรียมส่ง
  char payload[60];
  snprintf(payload, sizeof(payload), "{\"temp\":%.1f,\"humid\":%.1f}", t, h);

  // ปริ้นโชว์บนหน้าจอคอม
  Serial.print("กำลังส่งข้อมูล: ");
  Serial.println(payload);

  // ยิงคลื่นวิทยุ LoRa ออกไปหา Gateway
  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();

  delay(5000);  // รอ 5 วินาทีแล้วค่อยส่งใหม่
}
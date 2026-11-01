cat << 'EOF' > ~/Documents/GitHub/IoT-Assignment/README.md
# 🤖 IoT Smart Control & Monitoring System via AI Chat Box (MCP Server)

โปรเจกต์ระบบ **Internet of Things (IoT)** แบบครบวงจร สำหรับการอ่านค่าเซนเซอร์และควบคุมอุปกรณ์ฮาร์ดแวร์ทั้งแบบต่อสายตรง (Local GPIO) และแบบไร้สายระยะไกลผ่านคลื่นวิทยุ **LoRa (923 MHz)** โดยเชื่อมต่อเข้ากับ **AI Chat Box (เช่น ChatGPT)** ผ่านโปรโตคอล **Model Context Protocol (MCP)** และแสดงผลแบบเรียลไทม์บน **Web Dashboard** ด้วย **REST API**

![System Architecture](./IoT%20AI%20Chat%20Control%20Architecture.png)

---

## 🌟 คุณสมบัติหลักของระบบ (Key Features & Assignment Checklist)

1. **AI Chat Box Integration (MCP over HTTP/JSON):** สั่งเปิด-ปิดไฟ และสอบถามค่าอุณหภูมิ ความชื้น ระยะทางผ่านภาษาธรรมชาติบน AI Chat Box ที่เชื่อมต่อกับ MCP Server
2. **Raspberry Pi MCP Server:** รันเซิร์ฟเวอร์ด้วย `FastMCP` บนบอร์ด Raspberry Pi พร้อมเปิด Secure Tunnel (`tunnl.gg`) ให้ AI จากภายนอกเข้าถึงได้
3. **4 Custom MCP Tools:**
   - `read_pi_sensors` : อ่านค่าระยะทางจากเซนเซอร์ Ultrasonic และสถานะอุปกรณ์บน Raspberry Pi
   - `control_pi_devices` : สั่งเปิด-ปิดหลอดไฟ LED บนบอร์ด Raspberry Pi
   - `read_from_mqtt` : อ่านค่าอุณหภูมิ ความชื้น และสถานะไฟล่าสุดที่ส่งมาจาก LoRa Node ผ่าน MQTT Broker
   - `publish_to_mqtt` : ส่งคำสั่งควบคุมผ่าน MQTT Broker ไปยัง LoRa Node หรือ Raspberry Pi
4. **End-to-End LoRa Network (923 MHz):** รับ-ส่งข้อมูลแบบสองทิศทาง (Two-Way Uplink & Downlink) ระหว่างบอร์ด **Arduino UNO R4 WiFi (LoRa End Node)** และ **ESP32-C3 (Single-Channel LoRa Gateway)** พร้อมกลไก RX1 Window Retry
5. **MQTT Broker Integration:** ใช้ `mqtt-dashboard.com` (HiveMQ) เป็นตัวกลางแลกเปลี่ยนข้อมูลระหว่าง LoRa Network และ Raspberry Pi
6. **Interactive Web Dashboard:** หน้าเว็บแสดงผลค่า Temperature (°C), Humidity (%), Distance (cm), สวิตช์ควบคุมไฟ LED ทั้ง 2 ฝั่ง และกราฟเส้น **Live Data** อัปเดตอัตโนมัติทุก 2 วินาที
7. **RESTful API Architecture:** เชื่อมต่อระหว่าง Backend บน Raspberry Pi (Flask) กับ Frontend Web Dashboard ด้วย HTTP REST API

---

## 📂 โครงสร้างโปรเจกต์ (Project Structure)

```text
IoT-Assignment/
├── Arduino_TempLED/
│   └── Arduino_TempLED.ino       # โค้ดฝั่ง LoRa End Node (Arduino UNO R4 WiFi + Cytron LoRa Shield + DHT11 + LED)
├── ESP32_LoRa/
│   └── ESP32_LoRa.ino            # โค้ดฝั่ง Single-Channel LoRa Gateway (ESP32-C3 + RFM95W 923 MHz <-> MQTT)
├── iot-mcp/
│   ├── requirements.txt          # รายการไลบรารี Python สำหรับ Raspberry Pi
│   ├── server.py                 # โค้ดหลักรัน MCP Server (Port 8000), MQTT Client และ Flask REST API (Port 5000)
│   └── templates/
│       └── index.html            # หน้าเว็บ IoT Smart Dashboard (HTML5 + Chart.js + Fetch REST API)
├── IoT AI Chat Control Architecture.png
├── LICENSE
└── README.md

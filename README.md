cat << 'EOF' > ~/Documents/GitHub/IoT-Assignment/README.md
# IoT Assignment: สร้างระบบ IoT ควบคุม/อ่านค่า ผ่าน AI Chat box (MCP Server)

![Architecture](./IoT%20AI%20Chat%20Control%20Architecture.png)

## โครงสร้างโปรเจกต์ (Project Structure)
* **`iot-mcp/`** : โค้ดฝั่ง **Raspberry Pi (MCP Server & Web Dashboard)**
  * `server.py` : MCP Server (FastMCP) พร้อม 4 Tools (`read_pi_sensors`, `control_pi_devices`, `read_from_mqtt`, `publish_to_mqtt`), เชื่อมต่อ MQTT Broker และให้บริการ Flask REST API
  * `templates/index.html` : หน้าเว็บ Dashboard แสดงค่า Temperature, Humidity, Distance (cm), กราฟ Live Data และปุ่มควบคุม LED ผ่าน REST API
* **`Arduino_Temp/`** : โค้ดฝั่ง **LoRa End Node** (Arduino UNO R4 WiFi + Cytron LoRa Shield RFM95 + เซนเซอร์ DHT11) ส่งค่าผ่านคลื่นความถี่ 923 MHz และรับคำสั่ง Downlink
* **`ESP32_LoRa/`** : โค้ดฝั่ง **Single-Channel LoRa Gateway** (ESP32-C3 + โมดูล RFM95W 923 MHz) รับข้อมูลจาก LoRa Node ส่งต่อขึ้น MQTT Broker (`mqtt-dashboard.com`)

## วิธีรันระบบบน Raspberry Pi
```bash
cd iot-mcp
pip install -r requirements.txt
python3 server.py

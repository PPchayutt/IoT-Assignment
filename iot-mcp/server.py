import time
import json
import threading
import RPi.GPIO as GPIO
import paho.mqtt.client as mqtt
from flask import Flask, jsonify, request, render_template
from flask_cors import CORS
from mcp.server.fastmcp import FastMCP
from mcp.server.transport_security import TransportSecuritySettings

# ==========================================
# 1. ตั้งค่าขา GPIO บนบอร์ด Raspberry Pi (ส่วนที่ 3 Local Devices)
# ==========================================
LED_PIN_14 = 14   # ขา GPIO 14 (Physical Pin 8)
LED_PIN_4 = 4     # ขา GPIO 4 (Physical Pin 7)
TRIG_PIN = 23     # ขา TRIG ของ Ultrasonic HC-SR04 (Pin 16)
ECHO_PIN = 24     # ขา ECHO ของ Ultrasonic HC-SR04 (Pin 18)

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)
GPIO.setup(LED_PIN_14, GPIO.OUT)
GPIO.setup(LED_PIN_4, GPIO.OUT)
GPIO.setup(TRIG_PIN, GPIO.OUT)
GPIO.setup(ECHO_PIN, GPIO.IN)

GPIO.output(LED_PIN_14, GPIO.LOW)
GPIO.output(LED_PIN_4, GPIO.LOW)

# ตัวแปรเก็บสถานะทั้งหมดของระบบ
system_state = {
    "temperature": 26.5,
    "humidity": 62.0,
    "distance": 120.0,
    "led_status": "OFF",
    "lora_led_status": "OFF",
    "lora_raw_payload": "Waiting for LoRa data...",
    "history": []
}

# ==========================================
# 2. ฟังก์ชันอ่านค่าเซนเซอร์บน Raspberry Pi (Ultrasonic)
# ==========================================
def read_ultrasonic_distance():
    """วัดระยะทางจากเซนเซอร์ Ultrasonic HC-SR04 บน Raspberry Pi"""
    try:
        GPIO.output(TRIG_PIN, False)
        time.sleep(0.02)
        GPIO.output(TRIG_PIN, True)
        time.sleep(0.00001)
        GPIO.output(TRIG_PIN, False)

        timeout = time.time() + 0.06
        pulse_start = time.time()
        while GPIO.input(ECHO_PIN) == 0:
            pulse_start = time.time()
            if pulse_start > timeout:
                return system_state["distance"]

        pulse_end = time.time()
        while GPIO.input(ECHO_PIN) == 1:
            pulse_end = time.time()
            if pulse_end > timeout:
                return system_state["distance"]

        pulse_duration = pulse_end - pulse_start
        distance = round(pulse_duration * 17150, 1)
        if 2.0 <= distance <= 400.0:
            system_state["distance"] = distance
    except Exception:
        pass
    return system_state["distance"]

def set_pi_led_hardware(state_str: str):
    """สั่งเปิด-ปิดไฟ LED บนบอร์ด Raspberry Pi และอัปเดตสถานะ"""
    if state_str == "ON":
        GPIO.output(LED_PIN_14, GPIO.HIGH)
        GPIO.output(LED_PIN_4, GPIO.HIGH)
        system_state["led_status"] = "ON"
    else:
        GPIO.output(LED_PIN_14, GPIO.LOW)
        GPIO.output(LED_PIN_4, GPIO.LOW)
        system_state["led_status"] = "OFF"

# ==========================================
# 3. ตั้งค่าการเชื่อมต่อ MQTT Broker (ส่วนที่ 4 MQTT Broker)
# ==========================================
MQTT_BROKER = "mqtt-dashboard.com"
MQTT_PORT = 1883

TOPIC_PI_TEMP = "pi/sensor/temperature"
TOPIC_PI_HUMID = "pi/sensor/humidity"
TOPIC_PI_LED = "pi/device/led"
TOPIC_LORA_DATA = "lora/node1/data"
TOPIC_LORA_CMD = "lora/node1/cmd"

def on_connect(client, userdata, flags, reason_code, properties):
    print("✅ Connected to MQTT Broker!")
    client.subscribe(TOPIC_LORA_DATA)
    client.subscribe(TOPIC_PI_LED)

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode("utf-8", errors="ignore").strip()
    print(f"📡 [MQTT Received] {topic} -> {payload}")

    if topic == TOPIC_LORA_DATA:
        system_state["lora_raw_payload"] = payload
        try:
            data = json.loads(payload)
            if "temp" in data:
                system_state["temperature"] = float(data["temp"])
            if "humid" in data:
                system_state["humidity"] = float(data["humid"])
            if "lora_led" in data:
                system_state["lora_led_status"] = str(data["lora_led"])
        except Exception:
            pass
    elif topic == TOPIC_PI_LED:
        if payload.upper() in ("ON", "OFF"):
            set_pi_led_hardware(payload.upper())

mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="raspi-mcp-67070115")
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
mqtt_client.loop_start()

def background_sensor_updater():
    while True:
        read_ultrasonic_distance()
        timestamp = time.strftime("%H:%M:%S")
        system_state["history"].append({
            "time": timestamp,
            "temperature": system_state["temperature"],
            "humidity": system_state["humidity"],
            "distance": system_state["distance"]
        })
        if len(system_state["history"]) > 15:
            system_state["history"].pop(0)

        mqtt_client.publish(TOPIC_PI_TEMP, str(system_state["temperature"]))
        mqtt_client.publish(TOPIC_PI_HUMID, str(system_state["humidity"]))
        time.sleep(5)

threading.Thread(target=background_sensor_updater, daemon=True).start()

# ==========================================
# 4. สร้าง MCP Server และ Tools ทั้ง 4 ตัว (ส่วนที่ 2 MCP Server)
# ==========================================
TUNNL_DOMAIN = "pure-canyon-ccffaed0.tunnl.gg"

mcp = FastMCP(
    "RaspberryPi-IoT-MCP",
    host="0.0.0.0",
    port=8000,
    transport_security=TransportSecuritySettings(
        enable_dns_rebinding_protection=True,
        allowed_hosts=[
            "127.0.0.1:8000",
            "localhost:8000",
            "0.0.0.0:8000",
            TUNNL_DOMAIN,
            f"{TUNNL_DOMAIN}:*"
        ],
        allowed_origins=[
            f"https://{TUNNL_DOMAIN}/",
            f"https://{TUNNL_DOMAIN}",
            "https://chatgpt.com"
        ]
    )
)

@mcp.tool()
def read_pi_sensors() -> str:
    """Tool: Read Pi Sensors - อ่านค่าเซนเซอร์และสถานะอุปกรณ์จากบอร์ด Raspberry Pi (ระยะทาง Ultrasonic, อุณหภูมิ, ความชื้น, สถานะไฟ LED และลิงก์หน้าเว็บ Dashboard)"""
    dist = read_ultrasonic_distance()
    return json.dumps({
        "distance_cm": dist,
        "temperature_c": system_state["temperature"],
        "humidity_percent": system_state["humidity"],
        "pi_led_status": system_state["led_status"],
        "web_dashboard_info": "ดูกราฟและควบคุมผ่านหน้าเว็บได้ที่ http://<IP-ของ-Raspberry-Pi>:5000"
    }, ensure_ascii=False)

@mcp.tool()
def control_pi_devices(action: str) -> str:
    """Tool: Control Pi Devices - สั่งเปิดหรือปิดไฟ LED บนบอร์ด Raspberry Pi (ส่งค่า action เป็น 'on' หรือ 'off')"""
    cmd = action.strip().upper()
    if cmd not in ("ON", "OFF"):
        return "Error: กรุณาระบุ action เป็น 'on' หรือ 'off'"
    set_pi_led_hardware(cmd)
    mqtt_client.publish(TOPIC_PI_LED, cmd, qos=1)
    return f"สั่ง {cmd} ไฟ LED บนบอร์ด Raspberry Pi เรียบร้อยแล้วค่ะ"

@mcp.tool()
def read_from_mqtt() -> str:
    """Tool: Read from MQTT - อ่านค่าข้อมูลล่าสุดจาก MQTT Broker รวมถึงอุณหภูมิ ความชื้น และสถานะไฟจาก LoRa Node"""
    return json.dumps({
        "topic": TOPIC_LORA_DATA,
        "lora_temperature_c": system_state["temperature"],
        "lora_humidity_percent": system_state["humidity"],
        "lora_led_status": system_state["lora_led_status"],
        "pi_led_status": system_state["led_status"],
        "raw_payload": system_state["lora_raw_payload"]
    }, ensure_ascii=False)

@mcp.tool()
def publish_to_mqtt(topic: str, message: str) -> str:
    """Tool: Publish to MQTT - ส่งคำสั่งหรือข้อความไปยัง MQTT Broker
    - หากต้องการสั่งเปิด/ปิดไฟที่ LoRa Node ให้ใช้ topic='lora/node1/cmd' และ message='ON' หรือ 'OFF'
    - หากต้องการสั่งเปิด/ปิดไฟที่ Raspberry Pi ให้ใช้ topic='pi/device/led' และ message='ON' หรือ 'OFF'"""
    topic = topic.strip()
    message = message.strip()
    info = mqtt_client.publish(topic, message, qos=1)
    info.wait_for_publish(timeout=5)

    if topic == TOPIC_PI_LED and message.upper() in ("ON", "OFF"):
        set_pi_led_hardware(message.upper())
    elif topic == TOPIC_LORA_CMD and message.upper() in ("ON", "OFF"):
        system_state["lora_led_status"] = message.upper()

    return f"Publish ข้อความ '{message}' ไปยัง MQTT Topic '{topic}' สำเร็จแล้วค่ะ"

# ==========================================
# 5. สร้าง REST API และ Web Dashboard (ส่วนที่ 7 Web Dashboard)
# ==========================================
app = Flask(__name__)
CORS(app)

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/sensors", methods=["GET"])
def api_get_sensors():
    read_ultrasonic_distance()
    return jsonify(system_state)

@app.route("/api/led", methods=["POST"])
def api_control_led():
    data = request.get_json(silent=True) or {}
    action = data.get("action", "")
    msg = control_pi_devices(action)
    return jsonify({"status": "success", "led_status": system_state["led_status"], "message": msg})

@app.route("/api/lora/led", methods=["POST"])
def api_control_lora_led():
    data = request.get_json(silent=True) or {}
    action = data.get("action", "").strip().upper()
    msg = publish_to_mqtt(TOPIC_LORA_CMD, action)
    return jsonify({"status": "success", "lora_led_status": system_state["lora_led_status"], "message": msg})

def run_flask():
    app.run(host="0.0.0.0", port=5000, debug=False, use_reloader=False)

if __name__ == "__main__":
    threading.Thread(target=run_flask, daemon=True).start()
    print("🌐 Web Dashboard & REST API running on http://0.0.0.0:5000")
    print(f"🤖 MCP Server running on https://{TUNNL_DOMAIN}/mcp")
    mcp.run(transport="streamable-http")

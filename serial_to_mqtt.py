"""
PC-side monitoring client
--------------------------
Reads temperature readings sent by the Arduino over USB serial, publishes
each one to an MQTT broker on the VPS, and prints them to the console in
real time.

Serial protocol expected from the Arduino (one line per reading):
    TEMP:23.45

MQTT:
    Topic   : see MQTT_TOPIC below
    Payload : the temperature as plain text, e.g. "23.45"
    QoS     : 0 (fire-and-forget, fine for a periodic sensor feed)

Install dependencies:
    pip install pyserial paho-mqtt
"""

import sys
import time
from datetime import datetime

import serial
import paho.mqtt.client as mqtt

# ----------------------------------------------------------------------
# CONFIGURATION - edit these for your exam/lab setup
# ----------------------------------------------------------------------
SERIAL_PORT = "COM17"          # Windows e.g. "COM3" | Linux/Mac e.g. "/dev/ttyACM0"
SERIAL_BAUD = 9600

CANDIDATE_NAME = "Ganwa Anne Laure"    # used to build a unique MQTT topic per candidate
MQTT_BROKER_HOST = "157.173.101.159"
MQTT_BROKER_PORT = 1883
MQTT_TOPIC = "exam/emg19/temperature"
MQTT_USERNAME = None          # set to a string if the broker requires auth
MQTT_PASSWORD = None

RECONNECT_DELAY_S = 3


def build_mqtt_client():
    client = mqtt.Client(client_id=f"{CANDIDATE_NAME}-pc-client")
    if MQTT_USERNAME:
        client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)

    def on_connect(c, userdata, flags, rc):
        if rc == 0:
            print(f"[mqtt] connected to {MQTT_BROKER_HOST}:{MQTT_BROKER_PORT}")
        else:
            print(f"[mqtt] connection failed, rc={rc}")

    def on_disconnect(c, userdata, rc):
        print("[mqtt] disconnected, will retry in background")

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT, keepalive=60)
    client.loop_start()  # handles reconnects on a background thread
    return client


def open_serial_connection():
    while True:
        try:
            ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=2)
            print(f"[serial] connected to {SERIAL_PORT} at {SERIAL_BAUD} baud")
            return ser
        except serial.SerialException as exc:
            print(f"[serial] could not open {SERIAL_PORT} ({exc}), retrying...")
            time.sleep(RECONNECT_DELAY_S)


def parse_temperature_line(line: str):
    """Returns a float temperature from a 'TEMP:23.45' line, or None if malformed."""
    line = line.strip()
    if not line.startswith("TEMP:"):
        return None
    try:
        return float(line.split("TEMP:", 1)[1])
    except ValueError:
        return None


def main():
    mqtt_client = build_mqtt_client()
    ser = open_serial_connection()

    print(f"[info] publishing to MQTT topic: {MQTT_TOPIC}")
    print("[info] waiting for temperature readings (Ctrl+C to stop)...\n")
    print(f"{'time':<10} {'temperature (C)':<18} {'mqtt status'}")
    print("-" * 45)

    try:
        while True:
            try:
                raw_line = ser.readline().decode("utf-8", errors="ignore")
            except serial.SerialException:
                print("[serial] connection lost, reopening...")
                ser = open_serial_connection()
                continue

            if not raw_line:
                continue  # read timeout with no data yet, just loop again

            temperature = parse_temperature_line(raw_line)
            if temperature is None:
                continue  # ignore boot messages / partial lines

            result = mqtt_client.publish(MQTT_TOPIC, payload=f"{temperature:.2f}", qos=0)
            status = "sent" if result.rc == mqtt.MQTT_ERR_SUCCESS else f"error({result.rc})"

            timestamp = datetime.now().strftime("%H:%M:%S")
            print(f"{timestamp:<10} {temperature:<18.2f} {status}")

    except KeyboardInterrupt:
        print("\n[info] stopping...")
    finally:
        ser.close()
        mqtt_client.loop_stop()
        mqtt_client.disconnect()


if __name__ == "__main__":
    main()

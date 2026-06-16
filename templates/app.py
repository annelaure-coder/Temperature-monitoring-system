from flask import Flask, render_template, jsonify
import paho.mqtt.client as mqtt

app = Flask(__name__)

latest_temperature = "0.00"

BROKER = "157.173.101.159"
PORT = 1883
TOPIC = "exam/emg19/temperature"

def on_message(client, userdata, msg):
    global latest_temperature
    latest_temperature = msg.payload.decode()

client = mqtt.Client()
client.on_message = on_message

client.connect(BROKER, PORT)
client.subscribe(TOPIC)
client.loop_start()

@app.route("/")
def home():
    return render_template("index.html")

@app.route("/temperature")
def temperature():
    return jsonify({
        "temperature": latest_temperature
    })

if __name__ == "__main__":
    app.run(debug=True)
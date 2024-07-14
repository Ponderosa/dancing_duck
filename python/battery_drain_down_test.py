import json
import time
import paho.mqtt.client as mqtt

# MQTT server details
mqtt_server = "192.168.5.6"
mqtt_port = 1883
mqtt_topic = "dancing_duck/devices/1/command/motor"

# Array of JSON messages with different duty cycles
messages = [
    {"duty_right": 0.750, "duty_left": 0.0, "dur_ms": 10000},
    {"duty_right": 0.750, "duty_left": 0.750, "dur_ms": 5000},
    {"duty_right": 0.0, "duty_left": 0.750, "dur_ms": 10000},
    {"duty_right": 1.0, "duty_left": 1.0, "dur_ms": 2000},
]
message_jsons = [json.dumps(msg) for msg in messages]

# MQTT client setup
client = mqtt.Client()

def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))

# Attach the connect callback
client.on_connect = on_connect

# Connect to the MQTT server
client.connect(mqtt_server, mqtt_port, 60)

# Loop to send the messages in sequence every 2 seconds
try:
    index = 0
    while True:
        client.publish(mqtt_topic, message_jsons[index])
        print(f"Published message: {message_jsons[index]}")
        index = (index + 1) % len(message_jsons)  # Cycle through the messages
        time.sleep(15)
except KeyboardInterrupt:
    print("Stopping the script.")

# Disconnect from the MQTT server
client.disconnect()
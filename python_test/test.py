import json
import time
import paho.mqtt.client as mqtt

# MQTT server details
mqtt_server = "192.168.5.6"
mqtt_port = 1883
mqtt_topic = "DuckCommand"

# Array of JSON messages with different duty cycles
messages = [
    {"motor_1_duty_cycle": 750, "motor_2_duty_cycle": 0, "duration_s": 10},
    {"motor_1_duty_cycle": 750, "motor_2_duty_cycle": 750, "duration_s": 5},
    {"motor_1_duty_cycle": 0, "motor_2_duty_cycle": 750, "duration_s": 10},
    {"motor_1_duty_cycle": 999, "motor_2_duty_cycle": 999, "duration_s": 2},
    #{"motor_1_duty_cycle": 0, "motor_2_duty_cycle": 800, "duration_s": 2},
    #{"motor_1_duty_cycle": 800, "motor_2_duty_cycle": 0, "duration_s": 2},
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
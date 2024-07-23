import json
import time
import random
from typing import List, Dict
import paho.mqtt.client as mqtt

class DuckCoordinator:
    def __init__(self, config_file: str):
        self.config = self.load_config(config_file)
        self.mqtt_client = self.setup_mqtt()
        self.ducks = [f"duck_{i}" for i in range(1, 21)]  # 20 ducks
        self.current_mode = "Float"
        self.last_dock_time = time.time()
        self.force_dock = False

    def load_config(self, config_file: str) -> Dict:
        with open(config_file, 'r') as f:
            return json.load(f)

    def setup_mqtt(self) -> mqtt.Client:
        client = mqtt.Client()
        client.on_connect = self.on_connect
        client.on_message = self.on_message
        client.connect(self.config['mqtt_broker'], 1883, 60)
        client.loop_start()
        return client

    def on_connect(self, client, userdata, flags, rc):
        print(f"Connected with result code {rc}")
        client.subscribe("ducks/force_dock")

    def on_message(self, client, userdata, msg):
        if msg.topic == "ducks/force_dock":
            print("Received force dock command")
            self.force_dock = True

    def send_mqtt_message(self, topic: str, message: str):
        self.mqtt_client.publish(topic, message)

    def float_mode(self):
        print("Entering Float mode")
        # No commands sent in float mode

    def independent_dance(self):
        print("Entering Independent Dance mode")
        for duck in self.ducks:
            dance_routine = random.choice(self.config['dance_routines'])
            for move in dance_routine:
                self.send_mqtt_message(f"ducks/{duck}/dance", json.dumps(move))
                time.sleep(random.uniform(0.5, 1.5))  # Time between moves

    def synchronized_dance(self):
        print("Entering Synchronized Dance mode")
        dance_routine = random.choice(self.config['dance_routines'])
        for move in dance_routine:
            for duck in self.ducks:
                self.send_mqtt_message(f"ducks/{duck}/dance", json.dumps(move))
            time.sleep(random.uniform(0.5, 1.5))  # Time between moves

    def return_to_dock(self):
        print("Returning to dock")
        for duck in self.ducks:
            self.send_mqtt_message(f"ducks/{duck}/dock", "return")
        self.force_dock = False  # Reset the force_dock flag

    def run(self):
        while True:
            if self.force_dock or time.time() - self.last_dock_time > 5400:  # 90 minutes or forced
                self.return_to_dock()
                self.last_dock_time = time.time()
                time.sleep(300)  # Wait 5 minutes after docking
                continue

            mode_duration = random.uniform(
                self.config['mode_duration_min'],
                self.config['mode_duration_max']
            )

            if self.current_mode == "Float":
                self.float_mode()
                self.current_mode = "Independent Dance"
            elif self.current_mode == "Independent Dance":
                self.independent_dance()
                self.current_mode = "Synchronized Dance"
            elif self.current_mode == "Synchronized Dance":
                self.synchronized_dance()
                self.current_mode = "Float"

            # Check for force_dock during the mode execution
            start_time = time.time()
            while time.time() - start_time < mode_duration:
                if self.force_dock:
                    break
                time.sleep(1)

if __name__ == "__main__":
    coordinator = DuckCoordinator("config.json")
    coordinator.run()
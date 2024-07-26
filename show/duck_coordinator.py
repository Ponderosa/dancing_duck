import json
import time
import random
from typing import List, Dict
import paho.mqtt.client as mqtt

class DuckCoordinator:
    MODE_MAP = {
        "motor": 0,
        "point": 1,
        "swim": 2,
        "return_to_dock": 3,
        "float": 4
    }

    def __init__(self, config_file: str):
        self.config = self.load_config(config_file)
        self.mqtt_client = self.setup_mqtt()
        self.ducks = self.config['device_ids']
        self.current_mode = "Synchronized Dance"
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
        client.subscribe("dancing_duck/coordinator/return_to_dock")

    def on_message(self, client, userdata, msg):
        if msg.topic == "dancing_duck/coordinator/return_to_dock":
            print("Received force dock command")
            self.force_dock = True

    def round_floats(self, obj, decimal_places=2):
        if isinstance(obj, float):
            return round(obj, decimal_places)
        elif isinstance(obj, dict):
            return {k: self.round_floats(v, decimal_places) for k, v in obj.items()}
        elif isinstance(obj, (list, tuple)):
            return [self.round_floats(x, decimal_places) for x in obj]
        return obj

    def send_mqtt_message(self, device_id: int, message: Dict):
        topic = f"dancing_duck/devices/{device_id}/command/motor"
        wire_message = message.copy()
        
        # Convert named mode to number
        wire_message['type'] = self.MODE_MAP[wire_message['type']]
        
        if wire_message['type'] == 2:  # Swim mode
            wire_message['Kp'] = self.config['Kp']
            wire_message['Kd'] = self.config['Kd']

        # Apply float rounding to the entire message
        rounded_message = self.round_floats(wire_message, decimal_places=2)
        
        print(topic)
        print(rounded_message)
        self.mqtt_client.publish(topic, json.dumps(rounded_message))

    def return_to_dock(self):
        print("Returning to dock")
        self.force_dock = False  # Reset the force_dock flag
        self.last_dock_time = time.time()
        dock_message = {"type": "return_to_dock", "dur_ms": self.confg['time_to_swim_to_dock_s'] * 1000}
        for duck in self.ducks:
            self.send_mqtt_message(duck, dock_message)

    def independent_dance(self):
        print("Entering Independent Dance mode")
        for duck in self.ducks:
            dance_routine = random.choice(self.config['dance_routines']).copy()
            self.execute_dance_routine(dance_routine['moves'])
            for move in dance_routine['moves']:
                if 'heading' in move and move['heading'] == 'random':
                    move['heading'] = random.uniform(0.0, 360.0)
                self.send_mqtt_message(duck, move)

    def synchronized_dance(self):
        print("Entering Synchronized Dance mode")
        dance_routine = random.choice(self.config['dance_routines']).copy()
        for move in dance_routine['moves']:
            if 'heading' in move and move['heading'] == 'random':
                move['heading'] = random.uniform(0.0, 360.0)
            for duck in self.ducks:
                self.send_mqtt_message(duck, move)

    def run(self):
        while True:
            # Return to Dock if timeout or force message received
            if self.force_dock or time.time() - self.last_dock_time > self.config['dock_interval_s']:
                self.current_mode = "Return to Dock"

            # 4 Swarm Modes - Float, Independent Dance, Synchronized Dance, Return to Dock
            if self.current_mode == "Return to Dock":
                self.return_to_dock()
                mode_duration = self.config['time_at_dock_s']
                self.current_mode = "Synchronized Dance"
            elif self.current_mode == "Float":
                print("Entering Float mode")
                mode_duration = random.uniform(
                    self.config['float_mode_duration_min_s'],
                    self.config['float_mode_duration_max_s']
                )
                self.current_mode = "Independent Dance"
            elif self.current_mode == "Independent Dance":
                self.independent_dance()
                mode_duration = random.uniform(
                    self.config['dance_mode_duration_min_s'],
                    self.config['dance_mode_duration_max_s']
                )
                self.current_mode = "Synchronized Dance"
            elif self.current_mode == "Synchronized Dance":
                self.synchronized_dance()
                mode_duration = random.uniform(
                    self.config['dance_mode_duration_min_s'],
                    self.config['dance_mode_duration_max_s']
                )
                self.current_mode = "Float"

            # Wait for dance/float to complete
            start_time = time.time()
            print("Mode Duration: " + str(mode_duration))
            while time.time() - start_time < mode_duration:
                if self.force_dock:
                    break
                time.sleep(1)

if __name__ == "__main__":
    coordinator = DuckCoordinator("config.json")
    time.sleep(2)
    coordinator.run()
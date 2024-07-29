import json
import copy
import time
import random
from itertools import cycle
from time import sleep
from typing import List, Dict
import paho.mqtt.client as mqtt


class DuckCoordinator:
    MODE_MAP = {"motor": 0, "point": 1, "swim": 2, "float": 3, "return_to_dock": 4}

    def __init__(self, config_file: str):
        self.config = self.load_config(config_file)
        self.mqtt_client = self.setup_mqtt()
        self.ducks = self.config["device_ids"]
        self.current_mode = "Float"
        self.last_dock_time = time.time()
        self.force_dock = False
        self.mode_cycles = cycle(self.config["mode_cycle"])

    def load_config(self, config_file: str) -> Dict:
        with open(config_file, "r") as f:
            return json.load(f)

    def setup_mqtt(self) -> mqtt.Client:
        client = mqtt.Client()
        client.on_connect = self.on_connect
        client.on_message = self.on_message
        client.connect(self.config["mqtt_broker"], 1883, 60)
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

    def send_mqtt_message(self, topic: str, message: Dict):
        print(topic + ":" + str(message))
        self.mqtt_client.publish(topic, json.dumps(message))

    def send_mqtt_motor_command(self, device_id: int, message: Dict):
        topic = f"dancing_duck/devices/{device_id}/command/motor"

        wire_message = message.copy()

        # Convert named mode to number
        wire_message["type"] = self.MODE_MAP[wire_message["type"]]

        if wire_message["type"] == 2:  # Swim mode
            wire_message["Kp"] = self.config["Kp"]
            wire_message["Kd"] = self.config["Kd"]

        # Apply float rounding to the entire message
        rounded_message = self.round_floats(wire_message, decimal_places=2)
        self.send_mqtt_message(topic, rounded_message)

    def send_mqtt_motor_stop(self, device_id: int):
        topic = f"dancing_duck/devices/{device_id}/command/motor_stop"
        self.send_mqtt_message(topic, "")

    def return_to_dock(self):
        self.force_dock = False  # Reset the force_dock flag
        self.last_dock_time = time.time()
        dock_message = {
            "type": "return_to_dock",
            "dur_ms": self.config["time_to_swim_to_dock_s"] * 1000,
        }
        for duck in self.ducks:
            self.send_mqtt_motor_command(duck, dock_message)

    def float(self):
        if self.config["send_motor_stop_on_float"]:
            for duck in self.ducks:
                self.send_mqtt_motor_stop(duck)

    def get_dance_duration_s(self, dance_routine) -> int:
        ret_val = 0
        for move in dance_routine["moves"]:
            ret_val += move["dur_ms"]
        return ret_val / 1000

    def independent_dance(self) -> int:
        longest_dance_duration = 0
        for duck in self.ducks:
            dance_routine = copy.deepcopy(random.choice(self.config["dance_routines"]))
            print(dance_routine["name"])
            for move in dance_routine["moves"]:
                if "heading" in move and move["heading"] == "random":
                    move["heading"] = random.uniform(0.0, 360.0)
                self.send_mqtt_motor_command(duck, move)
                sleep(self.config["mqtt_delay_per_duck_s"])
            dance_duration = self.get_dance_duration_s(dance_routine)
            if dance_duration > longest_dance_duration:
                longest_dance_duration = dance_duration
        return longest_dance_duration

    def synchronized_dance(self) -> int:
        dance_routine = copy.deepcopy(random.choice(self.config["dance_routines"]))
        print(dance_routine["name"])
        for move in dance_routine["moves"]:
            if "heading" in move and move["heading"] == "random":
                move["heading"] = random.uniform(0.0, 360.0)
            for duck in self.ducks:
                self.send_mqtt_motor_command(duck, move)
            sleep(self.config["mqtt_delay_per_duck_s"])
        return self.get_dance_duration_s(dance_routine)

    def run(self):
        while True:
            # Return to Dock if timeout or force message received
            if self.force_dock or (
                self.config["enable_dock_timeout"]
                and time.time() - self.last_dock_time > self.config["dock_interval_s"]
            ):
                self.current_mode = "Return to Dock"

            # 4 Swarm Modes - Return to Dock, Float, Independent Dance, Synchronized Dance
            if self.current_mode == "Return to Dock":
                print("Returning to dock")
                self.return_to_dock()
                mode_duration = self.config["time_at_dock_s"]
                self.current_mode = "Synchronized Dance"
            elif self.current_mode == "Float":
                print("Entering Float Mode")
                self.float()
                mode_duration = random.uniform(
                    self.config["float_mode_duration_min_s"],
                    self.config["float_mode_duration_max_s"],
                )
                self.current_mode = next(self.mode_cycles)
            elif self.current_mode == "Independent Dance":
                print("Entering Independent Dance Mode")
                mode_duration = self.independent_dance()
                self.current_mode = next(self.mode_cycles)
            elif self.current_mode == "Synchronized Dance":
                print("Entering Synchronized Dance Mode")
                mode_duration = self.synchronized_dance()
                self.current_mode = next(self.mode_cycles)

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

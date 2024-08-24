import time
import sys
import paho.mqtt.client as mqtt


class DuckCoordinator:
    def __init__(self, mqtt_broker="localhost"):
        self.start_time = time.time()
        try:
            self.mqtt_client = self.setup_mqtt(mqtt_broker)
        except Exception as e:
            print(f"Error setting up MQTT client: {e}")
            sys.exit(1)

    def setup_mqtt(self, broker):
        try:
            client = mqtt.Client()
            client.on_connect = self.on_connect
            client.connect(broker, 1883, 60)
            client.loop_start()
            return client
        except Exception as e:
            print(f"Failed to connect to MQTT broker at {broker}: {e}")
            raise

    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print(f"Successfully connected to MQTT broker with result code {rc}")
        else:
            print(f"Failed to connect to MQTT broker. Return code: {rc}")

    def send_mqtt_message(self, topic, message):
        try:
            # Add null terminator to the message
            null_terminated_message = message + "\0"
            result = self.mqtt_client.publish(
                topic, null_terminated_message.encode("utf-8")
            )
            if result.rc != 0:
                print(f"Failed to send message. Return code: {result.rc}")
            else:
                print(f"Sent message to {topic}: {message}")
        except Exception as e:
            print(f"Error sending MQTT message: {e}")

    def run(self):
        print("Starting Duck Coordinator...")
        try:
            while True:
                elapsed_time_ms = int((time.time() - self.start_time) * 1000)
                self.send_mqtt_message(
                    "dancing_duck/all_devices/command/set_time", str(elapsed_time_ms)
                )
                time.sleep(10)
        except Exception as e:
            print(f"An error occurred during execution: {e}")
        finally:
            print("Disconnecting from MQTT broker...")
            self.mqtt_client.disconnect()
            print("Duck Coordinator shut down.")


if __name__ == "__main__":
    try:
        coordinator = DuckCoordinator("192.168.42.2")
        coordinator.run()
    except Exception as e:
        print(f"Failed to initialize or run Duck Coordinator: {e}")
        sys.exit(1)

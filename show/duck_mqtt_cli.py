import argparse
import json
import paho.mqtt.client as mqtt
import os
import sys
import time
from enum import IntEnum


class MotorCommandType(IntEnum):
    MOTOR = 0
    POINT = 1
    SWIM = 2
    FLOAT = 3


def load_config(config_file="config.json"):
    try:
        with open(config_file, "r") as f:
            config = json.load(f)

        # List of required config parameters
        required_params = [
            "device_ids",
            "Kp",
            "Kd",
            "dock_heading_degrees",
            "time_to_swim_to_dock_s",
        ]

        # Check for missing parameters
        missing_params = [param for param in required_params if param not in config]
        if missing_params:
            raise ValueError(
                f"Config file is missing the following required parameters: {', '.join(missing_params)}"
            )

        # Validate device_ids
        device_ids = config["device_ids"]
        if not device_ids or not all(
            isinstance(id, int) and id > 0 for id in device_ids
        ):
            print(
                "Warning: device_ids should be a non-empty list of positive integers."
            )

        # Validate numeric parameters
        numeric_params = ["Kp", "Kd", "dock_heading_degrees", "time_to_swim_to_dock_s"]
        for param in numeric_params:
            if not isinstance(config[param], (int, float)) or config[param] < 0:
                raise ValueError(f"'{param}' must be a non-negative number.")

        return config
    except FileNotFoundError:
        print(f"Config file '{config_file}' not found. Please ensure it exists.")
        sys.exit(1)
    except json.JSONDecodeError:
        print(f"Error decoding '{config_file}'. Please ensure it's valid JSON.")
        sys.exit(1)
    except Exception as e:
        print(f"Error in config file: {str(e)}")
        sys.exit(1)


def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("Connected to MQTT broker")
        client.connected_flag = True
    else:
        print(f"Connection failed with code {reason_code}")
        client.connected_flag = False


def on_publish(client, userdata, mid, reason_code=None, properties=None):
    pass  # Do nothing, effectively removing the MID printout


def create_motor_message(motor_type, config, **kwargs):
    if motor_type == MotorCommandType.MOTOR:
        return {
            "type": motor_type,
            "duty_right": kwargs.get("duty_right"),
            "duty_left": kwargs.get("duty_left"),
            "dur_ms": kwargs.get("dur_ms"),
        }
    elif motor_type == MotorCommandType.POINT:
        return {
            "type": motor_type,
            "heading": kwargs.get("heading"),
            "dur_ms": kwargs.get("dur_ms"),
        }
    elif motor_type == MotorCommandType.SWIM:
        return {
            "type": motor_type,
            "Kp": kwargs.get("Kp") or config.get("Kp"),
            "Kd": kwargs.get("Kd") or config.get("Kd"),
            "heading": kwargs.get("heading"),
            "dur_ms": kwargs.get("dur_ms"),
        }
    elif motor_type == MotorCommandType.FLOAT:
        return {"type": motor_type, "dur_ms": kwargs.get("dur_ms")}
    else:
        raise ValueError(f"Unknown motor command type: {motor_type}")


def create_return_message(config):
    return {
        "type": MotorCommandType.SWIM,
        "Kp": config.get("Kp"),
        "Kd": config.get("Kd"),
        "heading": config["dock_heading_degrees"],
        "dur_ms": int(
            config["time_to_swim_to_dock_s"] * 1000
        ),  # Convert seconds to milliseconds
    }


def send_command(client, device_id, command, config, **kwargs):
    topic = f"dancing_duck/devices/{device_id}/command/{command}"

    if command in ["calibrate", "dance", "stop_all", "reset"]:
        message = None  # No message for these commands
    elif command == "launch":
        message = json.dumps(
            {"launch_time": kwargs.get("launch_time"), "heading": kwargs.get("heading")}
        )
    elif command == "motor":
        motor_type = kwargs.pop("motor_type", None)  # Remove motor_type from kwargs
        if motor_type is None:
            raise ValueError("Motor type is required for motor command")
        message = json.dumps(create_motor_message(motor_type, config, **kwargs))
    elif command == "return":
        message = json.dumps(create_return_message(config))
    else:
        raise ValueError(f"Unknown command: {command}")

    # Print the topic and message
    print(f"Publishing to topic: {topic}")
    print(f"Message content: {message}")

    qos = 0
    result = client.publish(topic, message, qos=qos)
    result.wait_for_publish()
    print(f"{command.capitalize()} command sent to device: {device_id}")
    if command == "return":
        print(f"Return heading: {config['dock_heading_degrees']} degrees")
        print(f"Return duration: {config['time_to_swim_to_dock_s']} seconds")


def load_broker_ip():
    original_dir = os.getcwd()
    os.chdir("..")

    broker_ip = None
    files_to_check = ["wifi_secret.sh", "wifi_example.sh"]
    for file_name in files_to_check:
        if os.path.exists(file_name):
            print(f"Checking {file_name} for BROKER_IP_ADDRESS...")
            with open(file_name, "r") as f:
                for line in f:
                    if "BROKER_IP_ADDRESS=" in line:
                        parts = line.split("=", 1)
                        if len(parts) == 2:
                            broker_ip = parts[1].strip().strip('"')
                            print(f"Found BROKER_IP_ADDRESS: {broker_ip}")
                            break
            if broker_ip:
                break
        else:
            print(f"File {file_name} not found.")

    os.chdir(original_dir)

    if not broker_ip:
        print("Error: BROKER_IP_ADDRESS not found in wifi_secret.sh or wifi_example.sh")
        print("Please ensure one of these files exists and contains a line like:")
        print(
            'export BROKER_IP_ADDRESS="192.168.1.100" or BROKER_IP_ADDRESS=192.168.1.100'
        )
        sys.exit(1)

    return broker_ip


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Send MQTT commands to dancing duck devices"
    )
    parser.add_argument(
        "device", help="Device ID to send command to, or 'all' for all devices"
    )
    parser.add_argument(
        "command",
        choices=[
            "calibrate",
            "launch",
            "dance",
            "stop_all",
            "reset",
            "motor",
            "return",
        ],
        help="Command to send",
    )

    parser.add_argument(
        "--launch-time",
        type=float,
        metavar="S",
        help="Launch time (required for launch command)",
    )
    parser.add_argument(
        "--heading",
        type=float,
        metavar="DEGREES",
        help="Heading (required for launch, POINT, SWIM)",
    )
    parser.add_argument(
        "--broker", metavar="IP", help="MQTT broker IP address (optional)"
    )

    # Motor command arguments
    parser.add_argument(
        "--motor-type",
        type=int,
        choices=[0, 1, 2, 3],
        metavar="TYPE",
        help="Motor command type (0:MOTOR, 1:POINT, 2:SWIM, 3:FLOAT)",
    )
    parser.add_argument(
        "--duty-right",
        type=float,
        metavar="DUTY",
        help="Right motor duty cycle (for MOTOR type)",
    )
    parser.add_argument(
        "--duty-left",
        type=float,
        metavar="DUTY",
        help="Left motor duty cycle (for MOTOR type)",
    )
    parser.add_argument(
        "--Kp", type=float, metavar="GAIN", help="Proportional gain (for SWIM type)"
    )
    parser.add_argument(
        "--Kd", type=float, metavar="GAIN", help="Derivative gain (for SWIM type)"
    )
    parser.add_argument(
        "--dur-ms",
        type=int,
        metavar="MS",
        help="Duration (required for all motor commands)",
    )

    return parser.parse_args()


def validate_device(device):
    if device.lower() == "all":
        return "all"
    try:
        return int(device)
    except ValueError:
        raise argparse.ArgumentTypeError("Device must be a positive integer or 'all'")


def validate_arguments(args, config):
    if args.command == "launch" and (args.launch_time is None or args.heading is None):
        raise ValueError(
            "Launch command requires both --launch-time and --heading arguments"
        )
    elif args.command in ["calibrate", "dance", "stop_all", "reset", "return"] and any(
        [
            args.launch_time,
            args.heading,
            args.motor_type,
            args.duty_right,
            args.duty_left,
            args.Kp,
            args.Kd,
            args.dur_ms,
        ]
    ):
        raise ValueError(
            f"{args.command.capitalize()} command does not accept additional arguments"
        )
    elif args.command == "motor":
        if args.motor_type is None:
            raise ValueError("Motor command requires --motor-type argument")
        if args.motor_type == MotorCommandType.MOTOR and (
            args.duty_right is None or args.duty_left is None or args.dur_ms is None
        ):
            raise ValueError(
                "MOTOR type requires --duty-right, --duty-left, and --dur-ms arguments"
            )
        if args.motor_type == MotorCommandType.POINT and (
            args.heading is None or args.dur_ms is None
        ):
            raise ValueError("POINT type requires --heading and --dur-ms arguments")
        if args.motor_type == MotorCommandType.SWIM:
            if args.heading is None or args.dur_ms is None:
                raise ValueError("SWIM type requires --heading and --dur-ms arguments")
            if (args.Kp is None and "Kp" not in config) or (
                args.Kd is None and "Kd" not in config
            ):
                raise ValueError(
                    "SWIM type requires Kp and Kd values. Provide them as arguments or in the config file."
                )
        if args.motor_type == MotorCommandType.FLOAT and args.dur_ms is None:
            raise ValueError("FLOAT type requires --dur-ms argument")


def main():
    args = parse_arguments()
    config = load_config()

    try:
        device = validate_device(args.device)
        validate_arguments(args, config)

        broker_ip = args.broker if args.broker else load_broker_ip()
        device_ids = config.get("device_ids", [])

        if device == "all" and not device_ids:
            print("No valid devices found in config. Exiting.")
            return

        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        client.on_connect = on_connect
        client.on_publish = on_publish
        client.connected_flag = False

        client.connect_async(broker_ip, 1883, 60)
        client.loop_start()

        # Wait for connection
        timeout = 10  # 10 seconds timeout
        start_time = time.time()
        while not client.connected_flag:
            time.sleep(0.1)
            if time.time() - start_time > timeout:
                raise Exception("Connection timeout")

        command_args = {
            k: v for k, v in vars(args).items() if k not in ["device", "command"]
        }

        if device == "all":
            for device_id in device_ids:
                send_command(client, device_id, args.command, config, **command_args)
        else:
            send_command(client, device, args.command, config, **command_args)

    except Exception as e:
        print(f"Error: {e}")

    finally:
        if "client" in locals():
            client.loop_stop()
            client.disconnect()


if __name__ == "__main__":
    main()

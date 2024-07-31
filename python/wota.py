import sys
import os
import subprocess
import paho.mqtt.client as mqtt
import paramiko
import time
import argparse


def get_mac_ip_from_ap(ssh_user, ssh_password, ap_ip):
    print(f"Connecting to AP at {ap_ip} via SSH...")
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

    try:
        client.connect(ap_ip, username=ssh_user, password=ssh_password)

        print("Retrieving MAC/IP relationships...")

        # Get DHCP leases (using sudo)
        print("\nExecuting: sudo cat /var/lib/NetworkManager/dnsmasq-wlan0.leases")
        stdin, stdout, stderr = client.exec_command(
            "sudo cat /var/lib/NetworkManager/dnsmasq-wlan0.leases"
        )
        stdin.write(f"{ssh_password}\n")
        stdin.flush()
        dhcp_leases = stdout.read().decode()
        print(dhcp_leases)

        client.close()

        # Process the outputs to get MAC/IP relationships
        mac_ip_dict = {}

        # Process DHCP leases
        for line in dhcp_leases.split("\n"):
            if line:
                parts = line.split()
                if len(parts) >= 5:
                    mac, ip = parts[1].lower(), parts[2]
                    mac_ip_dict[mac] = ip

        return mac_ip_dict

    except Exception as e:
        print(f"Error connecting to AP or retrieving information: {str(e)}")
        return None


def perform_firmware_update(ip_address, firmware_path, max_retries=3, retry_delay=5):
    print("Performing firmware update...")
    update_command = f"serial-flash tcp:{ip_address}:4242 {firmware_path}"

    for attempt in range(max_retries):
        try:
            subprocess.run(update_command, shell=True, check=True)
            print("Firmware update successful!")
            return True
        except subprocess.CalledProcessError as e:
            print(f"Attempt {attempt + 1} failed: {e}")
            if attempt < max_retries - 1:
                print(f"Retrying in {retry_delay} seconds...")
                time.sleep(retry_delay)
            else:
                print("Max retries reached. Firmware update failed.")
                return False


# Set up argument parser
parser = argparse.ArgumentParser(description="OTA firmware update script")
parser.add_argument("device_id", type=int, help="Device ID for the update")
parser.add_argument("--broker", type=str, help="IP address of the broker (optional)")
args = parser.parse_args()

device_id = args.device_id
provided_broker_ip = args.broker

# Store the current working directory
original_dir = os.getcwd()

# Change to the parent directory to run build.sh
os.chdir("..")

# Run build script
build_command = f"./build.sh {device_id}"
try:
    print(f"Running build for device {device_id}...")
    subprocess.run(build_command, shell=True, check=True)
    print(f"Build completed successfully for device {device_id}")
except subprocess.CalledProcessError:
    print(f"Error: Build failed for device {device_id}")
    os.chdir(original_dir)  # Change back to the original directory
    sys.exit(1)

# Change back to the original directory
os.chdir(original_dir)

# Load MAC addresses and SSH credentials
try:
    from mac_secret import mac_addresses, ssh_user, ssh_password
except ImportError:
    print("Error: mac_secret.py not found or missing required variables.")
    sys.exit(1)

# Get MAC address for the given device ID
if device_id not in mac_addresses:
    print(f"Error: Device ID {device_id} not found in MAC address dictionary.")
    sys.exit(1)

mac_address = mac_addresses[device_id]

# Use provided broker IP or load from file
if provided_broker_ip:
    broker_ip = provided_broker_ip
    print(f"Using provided broker IP: {broker_ip}")
else:
    # Load MQTT broker IP address from wifi_secret.sh or wifi_example.sh
    broker_ip = None
    files_to_check = ["../wifi_secret.sh", "../wifi_example.sh"]
    for file_name in files_to_check:
        if os.path.exists(file_name):
            print(f"Checking {file_name} for BROKER_IP_ADDRESS...")
            with open(file_name, "r") as f:
                content = f.read()
                for line in content.split("\n"):
                    if "BROKER_IP_ADDRESS=" in line:
                        broker_ip = line.split("=")[1].strip().strip('"')
                        print(f"Found BROKER_IP_ADDRESS: {broker_ip}")
                        break
            if broker_ip:
                break
        else:
            print(f"File {file_name} not found.")

    if not broker_ip:
        print("Error: BROKER_IP_ADDRESS not found in wifi_secret.sh or wifi_example.sh")
        print("Please ensure one of these files exists and contains a line like:")
        print(
            'BROKER_IP_ADDRESS="192.168.1.100" or export BROKER_IP_ADDRESS=192.168.1.100'
        )
        sys.exit(1)

# Use broker IP for both SSH and MQTT
ap_ip = broker_ip

# Get MAC/IP relationships from AP
mac_ip_dict = get_mac_ip_from_ap(ssh_user, ssh_password, ap_ip)

print(mac_ip_dict)

if not mac_ip_dict:
    print("Failed to retrieve MAC/IP relationships from AP.")
    sys.exit(1)

# Find IP address for the given MAC address
mac_address = mac_address.lower()  # Convert the target MAC to lowercase
ip_address = mac_ip_dict.get(mac_address)

if not ip_address:
    print(f"Error: IP address not found for MAC address {mac_address}")
    print("Available MAC/IP pairs:")
    for mac, ip in mac_ip_dict.items():
        print(f"{mac}: {ip}")
    sys.exit(1)

print(f"Found IP address: {ip_address}")

# Send MQTT command to prepare device for update
print("Sending MQTT command to prepare device for update...")
client = mqtt.Client()
client.connect(broker_ip, 1883, 60)
topic = f"dancing_duck/devices/{device_id}/command/bootloader"
message = "B"
client.publish(topic, message)
client.disconnect()

# Add a 5-second delay after MQTT connection is closed
print("Waiting for 15 seconds...")
time.sleep(15)

# Perform firmware update using serial-flash
firmware_path = os.path.abspath("../build/dancing_duck.elf")
print(f"Firmware path: {firmware_path}")
if not os.path.exists(firmware_path):
    print(f"Error: Firmware file not found at {firmware_path}")
    sys.exit(1)

if perform_firmware_update(ip_address, firmware_path):
    print("Firmware update process completed successfully.")
else:
    print("Firmware update process failed after multiple attempts.")

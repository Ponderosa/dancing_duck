import sys
import os
import subprocess
import paho.mqtt.client as mqtt
import paramiko
import time
import argparse
import platform
import ipaddress
from concurrent.futures import ThreadPoolExecutor, as_completed


def get_subnet_from_ip(ip_address):
    ip = ipaddress.IPv4Address(ip_address)
    network = ipaddress.IPv4Network(f"{ip}/24", strict=False)
    return str(network)


def ping_ip(ip):
    ping_command = f"ping -c 1 -W 1 {ip} >/dev/null 2>&1"
    return subprocess.run(ping_command, shell=True).returncode == 0


def get_mac_ip_from_subnet(subnet):
    print(f"Scanning subnet {subnet}...")

    is_wsl = "microsoft-standard" in platform.uname().release.lower()
    if is_wsl:
        print("Execution environment: Windows Subsystem for Linux (WSL)")
    else:
        print("Execution environment: Native Linux")

    def run_command(command):
        result = subprocess.run(command, shell=True, capture_output=True, text=True)
        return result.stdout

    # Parallel pinging
    network = ipaddress.IPv4Network(subnet, strict=False)
    with ThreadPoolExecutor(max_workers=50) as executor:
        futures = [executor.submit(ping_ip, str(ip)) for ip in network.hosts()]
        for future in as_completed(futures):
            pass  # We don't need to do anything with the result, just wait for completion

    # Get ARP table
    mac_ip_dict = {}
    if is_wsl:
        arp_output = run_command('wsl.exe powershell.exe "arp -a"')
        # Process ARP output
        for line in arp_output.split("\n"):
            parts = line.split()
            if len(parts) >= 3:
                ip = parts[0]
                mac = parts[1].replace("-", ":").lower()
                if mac != "<incomplete>":
                    mac_ip_dict[mac] = ip
    else:
        time.sleep(10)
        arp_output = run_command("arp -a")
        print(arp_output)
        # Process ARP output
        for line in arp_output.split("\n"):
            parts = line.split()
            if len(parts) >= 4:
                ip = parts[1].replace("(", "").replace(")", "")
                mac = parts[3].replace("-", ":").lower()
                if mac != "<incomplete>":
                    mac_ip_dict[mac] = ip

    return mac_ip_dict


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

# Generate subnet from broker IP
subnet = get_subnet_from_ip(broker_ip)
print(f"Generated subnet: {subnet}")

# Get MAC/IP relationships from subnet
mac_ip_dict = get_mac_ip_from_subnet(subnet)

print("MAC/IP relationships:")
for mac, ip in mac_ip_dict.items():
    print(f"{mac}: {ip}")

if not mac_ip_dict:
    print("Failed to retrieve MAC/IP relationships from subnet.")
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

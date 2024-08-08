#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Update and upgrade the system
echo "Updating and upgrading the system..."
apt update && apt upgrade -y

# WiFi Credentials
echo "Loading WiFi Credentials"
if [ -f "../wifi_secret.sh" ]; then
  source ./../wifi_secret.sh
  echo "wifi_secret.sh loaded"
elif [ -f "../wifi_example.sh" ]; then
  source ./../wifi_example.sh
  echo "wifi_example.sh loaded"
else
  echo "No Credentials Found"
fi

# Network Manager - Add Hotspot to get NTP
nmcli device wifi connect $HOTSPOT_SSID password $HOTSPOT_PASSWORD

# # Network Manager - Uncomment to have RPi4 be AP
# # There is probably a better way than this: see hotspot
# # https://www.raspberrypi.com/documentation/computers/configuration.html
# nmcli con delete DUCK-AP
# nmcli con add type wifi ifname wlan0 mode ap con-name DUCK-AP ssid $WIFI_SSID autoconnect true
# nmcli con modify DUCK-AP wifi.band bg
# nmcli con modify DUCK-AP wifi.channel 3
# nmcli con modify DUCK-AP wifi-sec.key-mgmt wpa-psk
# nmcli con modify DUCK-AP wifi-sec.psk $WIFI_PASSWORD
# nmcli con modify DUCK-AP ipv4.method shared ipv4.address $BROKER_IP_ADDRESS/24
# nmcli con modify DUCK-AP ipv6.method disabled
# nmcli con up DUCK-AP

# Mosquitto
systemctl disable mosquitto.service
apt install -y mosquitto mosquitto-clients
mosquitto_config="/etc/mosquitto/conf.d/duck.conf"
rm "$mosquitto_config"
touch "$mosquitto_config"
echo "listener 1883 0.0.0.0" | tee -a "$mosquitto_config"
echo "allow_anonymous true" | tee -a "$mosquitto_config"
systemctl enable mosquitto.service

# Database and Visualization
echo "Uninstalling InfluxDB 2, InfluxDB CLI, Telegraf, and Grafana..."

# Uninstall InfluxDB 2
if command_exists influxd; then
    sudo systemctl stop influxdb
    sudo apt remove --purge influxdb2 -y
    sudo rm -rf /var/lib/influxdb /etc/influxdb
fi

# Uninstall InfluxDB CLI
if command_exists influx; then
    sudo apt remove --purge influxdb2-cli -y
fi

# Uninstall Telegraf
if command_exists telegraf; then
    sudo systemctl stop telegraf
    sudo apt remove --purge telegraf -y
    sudo rm -rf /etc/telegraf /var/log/telegraf
fi

# Uninstall Grafana
if command_exists grafana-server; then
    sudo systemctl stop grafana-server
    sudo apt remove --purge grafana -y
    sudo rm -rf /var/lib/grafana /etc/grafana
fi

echo "Uninstallation complete. Now reinstalling..."

# Update package lists
sudo apt update

# Install InfluxDB 2
echo "Installing InfluxDB 2..."
wget -q https://repos.influxdata.com/influxdata-archive_compat.key
echo '393e8779c89ac8d958f81f942f9ad7fb82a25e133faddaf92e15b16e6ac9ce4c influxdata-archive_compat.key' | sha256sum -c && cat influxdata-archive_compat.key | gpg --dearmor | sudo tee /etc/apt/trusted.gpg.d/influxdata-archive_compat.gpg > /dev/null
echo 'deb [signed-by=/etc/apt/trusted.gpg.d/influxdata-archive_compat.gpg] https://repos.influxdata.com/debian stable main' | sudo tee /etc/apt/sources.list.d/influxdata.list
sudo apt update
sudo apt install influxdb2 -y

# Install InfluxDB CLI
echo "Installing InfluxDB CLI..."
sudo apt install influxdb2-cli -y

# Install Telegraf
echo "Installing Telegraf..."
sudo apt install telegraf -y

# Install Grafana
echo "Installing Grafana..."
wget -q -O - https://packages.grafana.com/gpg.key | gpg --dearmor | sudo tee /usr/share/keyrings/grafana.gpg > /dev/null
echo "deb [signed-by=/usr/share/keyrings/grafana.gpg] https://packages.grafana.com/oss/deb stable main" | sudo tee /etc/apt/sources.list.d/grafana.list
sudo apt update
sudo apt install grafana -y

# Start services
sudo systemctl start influxdb
sudo systemctl start telegraf
sudo systemctl start grafana-server

# Enable services to start on boot
sudo systemctl enable influxdb
sudo systemctl enable telegraf
sudo systemctl enable grafana-server

echo "Installation complete. InfluxDB 2, InfluxDB CLI, Telegraf, and Grafana have been reinstalled."
echo "Please configure InfluxDB and Grafana as needed."
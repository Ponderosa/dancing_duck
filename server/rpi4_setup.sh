#!/bin/bash

# Upgrade Device Packages
apt update
apt upgrade -y

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

# Network Manager
nmcli con delete DUCK-AP
nmcli con add type wifi ifname wlan0 mode ap con-name DUCK-AP ssid $WIFI_SSID autoconnect false
nmcli con modify DUCK-AP wifi.band bg
nmcli con modify DUCK-AP wifi.channel 11
nmcli con modify DUCK-AP wifi-sec.key-mgmt wpa-psk
nmcli con modify DUCK-AP wifi-sec.psk $WIFI_PASSWORD
nmcli con modify DUCK-AP ipv4.method shared ipv4.address $BROKER_IP_ADDRESS/24
nmcli con modify DUCK-AP ipv6.method disabled
nmcli con up DUCK-AP

# Mosquitto
systemctl disable mosquitto.service
apt install -y mosquitto mosquitto-clients
mosquitto_config="/etc/mosquitto/conf.d/duck.conf"
rm "$mosquitto_config"
touch "$mosquitto_config"
echo "listener 1883 0.0.0.0" | tee -a "$mosquitto_config"
echo "allow_anonymous true" | tee -a "$mosquitto_config"
systemctl enable mosquitto.service


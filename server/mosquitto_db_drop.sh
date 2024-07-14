#!/bin/bash

systemctl stop mosquitto.service
rm /var/lib/mosquitto/mosquitto.db
systemctl start mosquitto.service
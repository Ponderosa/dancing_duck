# Server Setup Instructions for Raspberry Pi 4

1. Image bookworm onto RPi4
     - No Wifi Configuration
     - Enable SSH for Ethernet debugging
1. Clone repo to RPi4
1. Ensure `wifi_secret.sh` is properly defined 
1. Run `sudo rpi4_setup.sh`
1. Run `influx setup`
1. Copy telgraf.conf to /etc/telegraf/telegraf.conf
1. Add secrets to telegraf.conf from `influx auth list` - This will not be available again once you close your session!
1. Restart telgraf service `systemctl restart telegraf`
1. Setup Grafana by going to 192.179.1.1:3000
1. Add new data source to Grafana
1. For InfluxDB Details add the organization and the token - Do not worry about the Auth section
1. Grafana dashboards shall be committed to git via JSON
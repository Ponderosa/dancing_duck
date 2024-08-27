# Bumbershoot 2024 Runbook

## Show

### Hook up phone to network
- Get wifi network and password from team lead once on site.
- Turn off 5G on phone (you will not have internet access or receive calls during this time)
- Visit http://192.168.42.2:3000/d/cdtmyrxqlji80b/duck-dance-party?orgId=1
- Get username and password from lead.
- Once done checking dashboards, re-enable 5G so you can operate your phone normally. 

### Stuck Duck
- If possible check propellers for fouling such as feathers, refuse, or others.
  - Gently remove fouling
  - Double check drain is secure plugged in
  - Return to water by pushing the duck off towards the center
- Check battery voltage for particular duck in Grafana dashboards
  - Disable 5G
  - Visit http://192.168.42.2:3000/d/cdtmyrxqlji80b/duck-dance-party?orgId=1
  - Scroll to "Duck - System" row
  - Select proper device
  - If below 4V (device 1 battery sense doesn't work)
    - Retrieve duck if posssible, remove batteries, and contatct team lead for debug
- If we are not receiving metrics from duck for more than 15 minutes and duck is not moving for more than 5 minutes
  -  Retrieve duck if possible, remove batteries, and contact team lead for debug

### Wind clumping
1. Enable wind correction with CLI
  1. Identify wind direction
  2. `python3 duck_mqtt_cli.py wind_on -d <direction of wind in degrees> -t <time in seconds to correct for wind> -i <interval in seconds>`
  3. Try using launch time for duration and 5 to 15 minutes for interval

## Setup
### Pre-show Preperation
1. Take a picture of each duck with respective boat for debugging
1. Enable hotspot on phone to synchronize rpi4 clock 
1. Enable power to command station
1. Wait for wi-fi to initialize
1. Log into Grafana to check rpi4 metrics
1. Calibrate and launch all ducks
1. SSH into server and start show
   1. `cd workspace/dancing_duck/show`
   1. `./run_show.sh`

### Duck Launch
1. Add fresh batteries to boat and secure battery cover
1. Setup boat on calibration platform
1. Use CLI to send calibration command 
   1. `cd show`
   2. `python3 duck_mqtt_cli.py device <n> calibrate`
   3. Rotate boat slowly on calibration platform
1. Attach duck decoy to boat
1. Launch Duck
   1. Place duck in water
   1. Identify heading towards center of pool
   1. `python3 duck_mqtt_cli.py device <n> launch --heading <0-359> --launch_time <10-100>`
   1. Adjust launch time on next duck to approximately get to center of pool

### Teardown
1. Remove all batteries from all duck and place each set of batteries in it's own baggie.

# Dancing Duck

This repos contains all software and tools for the Dancing Duck installation (title subject to change) for the Bumbershoot 2024 Festival. 

- Duck Firmware (Basic OS and networking foundation created and tested)
- Coordinator (Basic system design discussed)
- Monitor (No progress)

The design consists of ~20 motorized ducks that can be controlled to perform coordinated dance moves in the water. The ducks are decoys with an RC hobby boat mounted to the bottom. There are two DC brushed motor, each attached to a propeller. A RP2040(Pico W) with a FreeRTOS, MQTT stack will be implmented on a custom PCBA. It will be able to interface with the Coordinator MQTT broker and control the motors with a TI 83xx driver chip. Also included is a magnetometer to help ducks point in desired directions and to return to dock when needed.  

# Duck Firmware
## Build Instructions
```
$ sudo apt update
$ sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib build-essential
$ cd ~/workspace (or wherever you want to clone to)
$ git clone --recurse-submodules https://github.com/Ponderosa/dancing_duck.git
$ git submodule update --init --recursive
$ ./build.sh
```
For further Pico information, please see the getting started link below.

## Flashing Instructions
### J-Link
1. Download and install latest package https://www.segger.com/downloads/jlink/ (Top section)
2. Solder 3 pins to Debug port on Pico and wire to SWD interface on J-Link.
3. Wire up hard reset pin to J-Link (Labeled "Run" on Pico)
4. Use flash.sh, ex `$ JLinkExe -ip 192.168.4.151 flash.sh`

### OpenOCD
Follow the getting started with Pico guide below.

## Debugging

### Serial Port
printf() is currently outputting to UART0 (GP0, GP1) at 3.3V. The UART is set to 115200-8-N-1. 

### GDB
Included is a .vscode/launch.json to use with the VSCode plugin Cortex-Debug by marus25. Please feel free to add more configurations, as you probably don't have the same J-Link IP address I do. 

Getting ARM GDB to run properly on Linux can be challenging. You may need to install older versions of libncurses and libncursesw

For my Ubuntu build this did the trick
```
sudo apt-get install libncurses5
sudo apt-get install libncursesw5
```

## Pico Documentation
https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html
https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf
https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf
https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf

# Coordinator
1. Install https://mosquitto.org/download/ on RPi4.
2. edit `etc/mosquitto/conf.d/mosquitto.conf` and add 
```
allow_anonymous true
listener 1883 0.0.0.0
```
3. Run https://mqttx.app/ for monitoring.
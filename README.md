# Dancing Duck

This repo contains all software and tools for the Dancing Duck installation (title subject to change) for the Bumbershoot 2024 Festival. 

- Duck Firmware (Foundational work complete - RTOS, WiFi, MQTT Messaging, JSON interpretation, and Motor Control)
- Coordinator (Mosquitto is functionally passing motor command messages to duck)
- Monitor (No progress)

The design consists of ~20 motorized ducks that can be controlled to perform coordinated dance moves in the water. The ducks are decoys with an RC hobby boat mounted to the bottom. There are two DC brushed motor, each attached to a propeller. A custom PCBA will be manufactured that will feature the rPi Pico and a TI DRV33xx chip. The multicore RP2040 will be running SMP FreeRTOS and include LwIP and MQTT stacks. It will be able to interface with the Coordinator MQTT broker to receive motor commands, which will interface with a TI 83xx driver chip. Also included is a magnetometer to help ducks point in desired directions and to return to dock when needed.  

The ducks will be controlled via MQTT text based instructiones (JSON, G-Code, Something hand rolled?) being issued from the RPi4 Coordinator. The Coordinator will be running a broker, likely Mosquitto. Logging and monitoring infrastructure will be running on the Coordinator and may be accessed via a touchscreen mounted directly to the RPi4 or an external laptop known as the Monitor. 

# Duck Firmware
## Build Instructions
```
$ sudo apt update
$ sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib build-essential
$ cd ~/workspace (or wherever you want to clone to)
$ git clone --recurse-submodules https://github.com/Ponderosa/dancing_duck.git
$ cd dancing_duck
$ git submodule update --init --recursive
$ cp wifi_example.sh wifi_secret.sh
$ vim wifi_secret.sh (add desired access point and MQTT broker information here)
$ ./build.sh
```
Note: Please be careful not to commit any private wifi AP information.

For further Pico information, please see the getting started link below.

## Flashing Instructions
### Copy to mass storage via USB
The Pico can load firmware images by copying the U2F output file. It needs to put into mass storage mode first. To do this hold down the button on the pico while plugging in the USB cable. More instructions can be found in the getting started manual linked below.

### J-Link
1. Download and install latest package https://www.segger.com/downloads/jlink/ (Top section)
2. Solder 3 pins to Debug port on Pico and wire to SWD interface on J-Link.
3. Wire up hard reset pin to J-Link (Labeled "Run" on Pico)
4. Use flash.sh, ex `$ JLinkExe -ip 192.168.4.151 flash.sh`

### OpenOCD
Follow the getting started with Pico guide below.

## Debugging

### Serial Port
printf() is currently outputting to UART0 (GP0, GP1) at 3.3V. The UART is set to 115200-8-N-1. A lot of useful information is being streamed to this port. It is highly recommended to utilize this debug feature before venturing into GDB. 

Todo: Add build configuration file that can switch to USB CDC printf(). Or just move to USB CDC printf(). More details on CMakeLists modifications here: https://deepbluembedded.com/raspberry-pi-pico-serial-usb-c-sdk-serial-print-monitor/

### GDB
Since the rp2040 is dual core, care must be taken in properly attaching to the device. 

Included is a .vscode/launch.json to use with the VSCode plugin Cortex-Debug by marus25. Please feel free to add more configurations, as you probably don't have the same J-Link IP address I do. To get this to work start the Debug session directly from VSCode. You may need to reset the device one or two times with the left most reset button that appears in the new toolbar in the center top of VSCode. Once you do that, you should be able to use breakpoints and step through the code. 

Getting ARM GDB to run properly on Linux can be challenging. You may need to install older versions of libncurses and libncursesw

For my Ubuntu build this did the trick
```
sudo apt-get install libncurses5
sudo apt-get install libncursesw5
```

## Pico Documentation
- https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html
- https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf
- https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf
- https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf

# Coordinator
1. Install https://mosquitto.org/download/ on RPi4.
2. edit `etc/mosquitto/conf.d/mosquitto.conf` and add 
```
allow_anonymous true
listener 1883 0.0.0.0
```
3. Run https://mqttx.app/ on any machine in the network for monitoring.

# MQTT Topics WIP
```
dancing_duck/
├── devices/
│   ├── {device_id}/
│   │   ├── sensors/
│   │   │   ├── temperature
│   │   │   ├── battery_voltage
│   │   │   └── ...
│   │   ├── metrics/
│   │   │   ├── freertos/
│   │   │   │   ├── heap
│   │   │   │   ├── tasks
│   │   │   │   └── ...
│   │   │   └── lwip
│   │   └── logs/
│   │   |   ├── info
│   │   |   ├── warning
│   │   |   └── error
│   │   └── command/
│   │   |   ├── motor
│   │   |   ├── navigate
│   │   |   ├── return_to_dock
│   │   |   ├── set_test_mode
│   │   |   └── ...
│   │   └── quack
│   └── ...
└── all_devices/
    ├── status
    └── command
        └── uart_tx
```
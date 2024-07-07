cmake_minimum_required(VERSION 3.20)

# Board and Hardware Settings
set(PICO_PLATFORM rp2040)
set(PICO_BOARD pico_w) # pico or pico_w

# Include SDKs and Libraries
include(lib/pico-sdk/external/pico_sdk_import.cmake)
include(lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# Project Setup
project(app C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize SDK
pico_sdk_init()

# Source Files
add_executable(dancing_duck 
  src/main.c 
  src/blink/blink.c 
  src/commanding/commanding.c
  src/magnetometer/lis2mdl.c
  src/magnetometer/magnetometer.c
  src/motor/motor.c
  src/publish/publish.c
  src/watchdog/watchdog.c
  src/wifi/wifi.c 
  src/wifi/mqtt/mqtt.c
  lib/cJSON/cJSON.c
  ${PICO_LWIP_CONTRIB_PATH}/apps/ping/ping.c
  ${PICO_LWIP_CONTRIB_PATH}/apps/socket_examples/socket_examples.c
)

# Include Directories
target_include_directories(dancing_duck PRIVATE 
  src
  src/blink
  src/commanding
  src/magnetometer
  src/motor
  src/publish
  src/watchdog
  src/wifi
  src/wifi/mqtt
  lib/cJSON
  ${PICO_LWIP_CONTRIB_PATH}/apps/ping
  ${PICO_LWIP_CONTRIB_PATH}/apps/socket_examples
)

# Compile Definitions
target_compile_definitions(dancing_duck PRIVATE
  DUCK_ID_NUM=${DUCK_ID_NUM}
  CYW43_HOST_NAME=\"Duck_${DUCK_ID_NUM}\"
  WIFI_SSID=\"${WIFI_SSID}\"
  WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
  MQTT_BROKER_IP_A=${BROKER_IP_BYTE_A}
  MQTT_BROKER_IP_B=${BROKER_IP_BYTE_B}
  MQTT_BROKER_IP_C=${BROKER_IP_BYTE_C}
  MQTT_BROKER_IP_D=${BROKER_IP_BYTE_D}
  NO_SYS=0            # Override lwipopts.h
  LWIP_SOCKET=1       # Override lwipopts.h
  PING_USE_SOCKETS=1
)

#Compile Options
target_compile_options(dancing_duck PRIVATE -Wall -Wextra)

# Link Libraries
target_link_libraries(dancing_duck 
  pico_stdlib 
  hardware_i2c
  hardware_pwm 
  pico_cyw43_arch_lwip_sys_freertos 
  FreeRTOS-Kernel 
  FreeRTOS-Kernel-Heap4 
  pico_lwip_mqtt
  picowota_reboot
)

# Wireless OTA
set(PICOWOTA_WIFI_SSID ${WIFI_SSID})
set(PICOWOTA_WIFI_PASS ${WIFI_PASSWORD})
set(PICOWOTA_WIFI_AP 0)
add_subdirectory(lib/picowota)

# Add post-build command to print memory usage
add_custom_command(TARGET dancing_duck
                   POST_BUILD
                   COMMAND arm-none-eabi-size --format=GNU --totals ${CMAKE_CURRENT_BINARY_DIR}/dancing_duck.elf
                   COMMAND echo "Memory Usage Details:"
                   COMMAND arm-none-eabi-size --format=sysv -x ${CMAKE_CURRENT_BINARY_DIR}/dancing_duck.elf
                   COMMENT "Printing memory usage details")

# Add linker flag to print memory usage
target_link_options(dancing_duck PRIVATE -Wl,--print-memory-usage)

# Extra Outputs
pico_add_extra_outputs(dancing_duck)

# Wireless OTA build
picowota_build_combined(dancing_duck)
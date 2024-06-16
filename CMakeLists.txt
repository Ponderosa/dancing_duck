cmake_minimum_required(VERSION 3.20)

# Include SDKs and Libraries
include(lib/pico-sdk/external/pico_sdk_import.cmake)
include(lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# Board and Hardware Settings
set(PICO_PLATFORM rp2040)
set(PICO_BOARD pico_w) # pico or pico_w

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
  src/motor/motor.c
  src/wifi/wifi.c 
  lib/cJSON/cJSON.c
  ${PICO_LWIP_CONTRIB_PATH}/apps/ping/ping.c
  ${PICO_LWIP_CONTRIB_PATH}/apps/socket_examples/socket_examples.c
)

# Include Directories
target_include_directories(dancing_duck PRIVATE 
  src
  src/blink
  src/motor
  src/wifi
  lib/cJSON
  ${PICO_LWIP_CONTRIB_PATH}/apps/ping
  ${PICO_LWIP_CONTRIB_PATH}/apps/socket_examples
)

# Compile Definitions
target_compile_definitions(dancing_duck PRIVATE
  WIFI_SSID=\"${WIFI_SSID}\"
  WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
  MQTT_BROKER_IP_A=${BROKER_IP_BYTE_A}
  MQTT_BROKER_IP_B=${BROKER_IP_BYTE_B}
  MQTT_BROKER_IP_C=${BROKER_IP_BYTE_C}
  MQTT_BROKER_IP_D=${BROKER_IP_BYTE_D}
  NO_SYS=0            # don't want NO_SYS (generally this would be in your lwipopts.h)
  LWIP_SOCKET=1       # we need the socket API (generally this would be in your lwipopts.h)
  PING_USE_SOCKETS=1
)

#Compile Options
target_compile_options(dancing_duck PRIVATE -Wall -Wextra)

# Link Libraries
target_link_libraries(dancing_duck 
  pico_stdlib 
  hardware_pwm 
  pico_cyw43_arch_lwip_sys_freertos 
  FreeRTOS-Kernel 
  FreeRTOS-Kernel-Heap4 
  pico_lwip_mqtt
)

# Extra Outputs
pico_add_extra_outputs(dancing_duck)

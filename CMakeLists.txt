cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include(lib/pico-sdk/external/pico_sdk_import.cmake)

# Pull in FreeRTOS
include(lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

#Pull in coreMQTT-Agent
# add_compile_definitions(MQTT_AGENT_DO_NOT_USE_CUSTOM_CONFIG)
# add_compile_definitions(MQTT_DO_NOT_USE_CUSTOM_CONFIG)
# include(lib/coreMQTT-Agent/mqttAgentFilePaths.cmake)
# include(lib/coreMQTT-Agent/source/dependency/coreMQTT/mqttFilePaths.cmake)

# settings for the board and hardware used
set(PICO_PLATFORM rp2040)
set(PICO_BOARD pico_w) # pico or pico_w

# setup project
project(app C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the rp2040 SDK
pico_sdk_init()

# add exe
add_executable(dancing_duck 
  src/main.c 
  src/blink/blink.c 
  #src/mqtt/mqtt_agent.c
  src/wifi/wifi.c 
  ${PICO_LWIP_CONTRIB_PATH}/apps/ping/ping.c
  ${PICO_LWIP_CONTRIB_PATH}/apps/socket_examples/socket_examples.c
)

target_include_directories(dancing_duck PRIVATE 
  src
  src/blink
  #src/mqtt
  src/wifi
  ${PICO_LWIP_CONTRIB_PATH}/apps/ping
  ${PICO_LWIP_CONTRIB_PATH}/apps/socket_examples
)

target_compile_definitions(dancing_duck PRIVATE
  WIFI_SSID=\"${WIFI_SSID}\"
  WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
  NO_SYS=0            # don't want NO_SYS (generally this would be in your lwipopts.h)
  LWIP_SOCKET=1       # we need the socket API (generally this would be in your lwipopts.h)
  PING_USE_SOCKETS=1
)

# add Libraries
#add_library(coremqtt-agent STATIC ${MQTT_AGENT_SOURCES})
#target_include_directories(coremqtt-agent PUBLIC ${MQTT_AGENT_INCLUDE_PUBLIC_DIRS} ${MQTT_INCLUDE_PUBLIC_DIRS})
#add_library(coremqtt STATIC ${MQTT_SOURCES})
#target_include_directories(coremqtt PUBLIC ${MQTT_INCLUDE_PUBLIC_DIRS})
 
# pull in common dependencies
target_link_libraries(dancing_duck pico_stdlib pico_cyw43_arch_lwip_sys_freertos FreeRTOS-Kernel FreeRTOS-Kernel-Heap4 pico_lwip_mqtt)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(dancing_duck)

cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include(lib/pico-sdk/external/pico_sdk_import.cmake)

# Pull in FreeRTOS
include(lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

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
  src/wifi/wifi.c 
  ${PICO_LWIP_CONTRIB_PATH}/apps/ping/ping.c
  ${PICO_LWIP_CONTRIB_PATH}/apps/socket_examples/socket_examples.c
)

target_include_directories(dancing_duck PRIVATE 
  src
  src/blink
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

# pull in common dependencies
target_link_libraries(dancing_duck pico_stdlib pico_cyw43_arch_lwip_sys_freertos FreeRTOS-Kernel FreeRTOS-Kernel-Heap4 pico_lwip_mqtt)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(dancing_duck)

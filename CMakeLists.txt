cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include(lib/pico-sdk/external/pico_sdk_import.cmake)

# Pull in FreeRTOS
include(lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# settings for the board and hardware used
set(PICO_PLATFORM rp2040)
set(PICO_BOARD pico_w) # pico or pico_w

# set build type. Without 'Debug', it won't generate line information if using build subfolders.
set(CMAKE_BUILD_TYPE Debug)

project(app C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(dancing_duck src/main.c)

target_include_directories(dancing_duck PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(dancing_duck PRIVATE src)
 
# pull in common dependencies
target_link_libraries(dancing_duck pico_stdlib pico_cyw43_arch_none FreeRTOS-Kernel FreeRTOS-Kernel-Heap4)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(dancing_duck)

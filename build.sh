#!/bin/bash

# Initialize environment variables
export DANCING_DUCK_HOME=$(pwd)
export BUILD_TYPE=Debug
export PICO_SDK_PATH=$PWD/lib/pico-sdk
export FREERTOS_KERNEL_PATH=$PWD/lib/FreeRTOS-Kernel

# Print the initialized environment variables
echo "Project Home: $DANCING_DUCK_HOME"
echo "Build Type: $BUILD_TYPE"

# Create the build directory if it doesn't exist
BUILD_DIR=$DANCING_DUCK_HOME/build
if [ ! -d "$BUILD_DIR" ]; then
  echo "Creating build directory at $BUILD_DIR"
  mkdir -p "$BUILD_DIR"
fi

# WiFi Credentials
echo "Loading WiFi Credentials"
if [ -f "wifi_secret.sh" ]; then
  source ./wifi_secret.sh
  echo "wifi_secret.sh loaded"
elif [ -f "wifi_example.sh" ]; then
  source ./wifi_example.sh
  echo "wifi_example.sh loaded"
else
  echo "No Credentials Found"
fi

# Navigate to the build directory
cd "$BUILD_DIR"

# Run CMake
echo "Running CMake..."
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DWIFI_SSID="$WIFI_SSID" \
      -DWIFI_PASSWORD="$WIFI_PASSWORD" \
      -DBROKER_IP_BYTE_A="$BROKER_IP_BYTE_A" \
      -DBROKER_IP_BYTE_B="$BROKER_IP_BYTE_B" \
      -DBROKER_IP_BYTE_C="$BROKER_IP_BYTE_C" \
      -DBROKER_IP_BYTE_D="$BROKER_IP_BYTE_D" \
      ..

# Run Make
echo "Running Make..."
make

# Return to the project home directory
cd "$DANCING_DUCK_HOME"
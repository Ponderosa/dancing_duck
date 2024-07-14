#!/bin/bash

# Check if the first argument is provided
if [ -z "$1" ]; then
  echo "Error: No argument provided."
  exit 1
fi

# Check if the first argument is a number
if ! [[ "$1" =~ ^[0-9]+$ ]]; then
  echo "Error: Argument is not a number."
  exit 1
fi

# Check if the number is between 0 and 100
if [ "$1" -lt 0 ] || [ "$1" -gt 100 ]; then
  echo "Error: Argument is not between 0 and 100."
  exit 1
fi

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
      -DDUCK_ID_NUM=$1 \
      -DWIFI_SSID="$WIFI_SSID" \
      -DWIFI_PASSWORD="$WIFI_PASSWORD" \
      -DWIFI_SSID_ALT="$WIFI_SSID_ALT" \
      -DWIFI_PASSWORD_ALT="$WIFI_PASSWORD_ALT" \
      -DBROKER_IP_BYTE_A="$BROKER_IP_BYTE_A" \
      -DBROKER_IP_BYTE_B="$BROKER_IP_BYTE_B" \
      -DBROKER_IP_BYTE_C="$BROKER_IP_BYTE_C" \
      -DBROKER_IP_BYTE_D="$BROKER_IP_BYTE_D" \
      -DBROKER_IP_BYTE_A_ALT="$BROKER_IP_BYTE_A_ALT" \
      -DBROKER_IP_BYTE_B_ALT="$BROKER_IP_BYTE_B_ALT" \
      -DBROKER_IP_BYTE_C_ALT="$BROKER_IP_BYTE_C_ALT" \
      -DBROKER_IP_BYTE_D_ALT="$BROKER_IP_BYTE_D_ALT" \
      ..

# Run Make
echo "Running Make..."
make

# Return to the project home directory
cd "$DANCING_DUCK_HOME"
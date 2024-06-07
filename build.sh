#!/bin/bash

# Initialize environment variables
export DANCING_DUCK_HOME=$(pwd)
export BUILD_TYPE=Release
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

# Navigate to the build directory
cd "$BUILD_DIR"

# Run CMake
echo "Running CMake..."
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# Run Make
echo "Running Make..."
make

# Return to the project home directory
cd "$DANCING_DUCK_HOME"

echo "Build completed!"

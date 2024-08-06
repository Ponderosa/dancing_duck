#!/bin/bash

# Configuration
PYTHON_SCRIPT="duck_coordinator.py"
LOG_FILE="duck_coordinator.log"

# Function to start the Python script
start_script() {
    echo "Starting Duck Coordinator..."
    python3 -u "$PYTHON_SCRIPT" >> "$LOG_FILE" 2>&1 &
    PID=$!
    echo "Duck Coordinator started with PID: $PID"
}

# Main loop
while true; do
    if ! pgrep -f "$PYTHON_SCRIPT" > /dev/null; then
        echo "Duck Coordinator is not running. Starting it..."
        start_script
    fi
    sleep 60  # Check every 60 seconds
done
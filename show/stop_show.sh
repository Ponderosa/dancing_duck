#!/bin/bash

# Configuration
PYTHON_SCRIPT="duck_coordinator.py"
WATCHDOG_SCRIPT="run_duck_coordinator.sh"

# Function to stop a process
stop_process() {
    local process_name=$1
    local pid=$(pgrep -f "$process_name")
    if [ -n "$pid" ]; then
        echo "Stopping $process_name (PID: $pid)..."
        kill $pid
        sleep 2
        if kill -0 $pid 2>/dev/null; then
            echo "$process_name did not stop gracefully. Forcing..."
            kill -9 $pid
        fi
        echo "$process_name stopped."
    else
        echo "$process_name is not running."
    fi
}

# Stop the watchdog script first
stop_process "$WATCHDOG_SCRIPT"

# Give the watchdog a moment to fully stop
sleep 2

# Now stop the Python script
stop_process "$PYTHON_SCRIPT"

echo "All processes have been stopped."
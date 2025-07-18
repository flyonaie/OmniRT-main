#!/bin/bash

# Function to start a process
start_process() {
    local cfg_file=$1
    local process_name=$2
    local log_file="./logs/${process_name}.log"
    
    # Create logs directory if it doesn't exist
    mkdir -p logs
    
    # Use exec -a to set process name
    exec -a "$process_name" ./omnirt_main --cfg_file_path="$cfg_file" > "$log_file" 2>&1 &
    echo "Started $process_name with config $cfg_file, log file: $log_file"
}

# Start multiple processes with different names and configs
start_process "./cfg/process1.yaml" "omnirt_pub1"
start_process "./cfg/process2.yaml" "omnirt_pub2"
start_process "./cfg/process3.yaml" "omnirt_sub1"
start_process "./cfg/process4.yaml" "omnirt_sub2"

# Print process status
echo -e "\nProcess status:"
ps aux | grep "omnirt_[ps]"

echo -e "\nUse 'ps aux | grep omnirt_' to check process status"
echo "Check logs in ./logs/ directory"

# Wait for all background processes
wait

echo "All processes completed"

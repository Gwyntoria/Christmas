#!/bin/sh

# Find the process ID (PID) of the 'loto_conf.sh' process
PID=$(ps aux | grep '[l]oto_conf.sh' | awk '{print $1}')

# If the PID was found, kill the process
if [ ! -z "$PID" ]; then
    echo "Killing process 'loto_conf.sh', PID=$PID"
    kill $PID
else
    echo "Process 'loto_conf.sh' is not running"
fi


PID=$(ps aux | grep '[l]oto_rtmp' | awk '{print $1}')

if [ ! -z "$PID" ]; then
    echo "Killing process 'loto_rtmp', PID=$PID"
    kill $PID
else
    echo "Process 'loto_rtmp' is not running"
fi
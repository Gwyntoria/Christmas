#!/bin/sh

usage() {
    echo "Usage: $1 <tftp server address>"
}


if [ $# -eq 0 ]; then
    usage $0
    exit 1
fi

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

# echo "Update push.config"
# tftp -g -r push.config $1

echo "Update loto_rtmp"
tftp -g -r loto_rtmp $1
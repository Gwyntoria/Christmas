#!/bin/sh

usage() {
    echo "Usage: $1 <tftp server address>"
}


if [ $# -eq 0 ]; then
    usage $0
    exit 1
fi

# kill processes
/etc/init.d/kill_process.sh

sleep 1

# echo "Update push.config"
# tftp -g -r push.config $1

echo "Update loto_rtmp"
tftp -g -r loto_rtmp $1

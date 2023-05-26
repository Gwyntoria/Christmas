#!/bin/sh

usage() {
    echo "Usage: $1 <tftp server address>"
}


if [ $# -eq 0 ]; then
    usage $0
    exit 1
fi

# kill processes
/root/scripts/kill_process.sh

sleep 1

# echo "=== Update push.conf ==="
# tftp -g -r push.conf $1

echo "=== Update loto_rtmp ==="
# cp loto_rtmp loto_rtmp_back
tftp -g -r loto_rtmp $1

chmod 777 loto_rtmp

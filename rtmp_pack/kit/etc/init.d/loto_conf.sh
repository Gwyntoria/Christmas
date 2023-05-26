#!/bin/sh

echo "########## Network Configuring ##########"
ifconfig eth0 10.0.0.80 netmask 255.255.252.0
route add default gw 10.0.1.1

telnetd &

# echo "########## Set System Time #############"
# /root/scripts/time_conf.sh

echo "########## Run RTMP App #################"
/root/loto_rtmp

sleep 20

reboot
# /root/scripts/check_process.sh

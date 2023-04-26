#!/bin/sh

echo "########## Network Configuring ##########"
ifconfig eth0 10.0.0.80 netmask 255.255.252.0
route add default gw 10.0.1.1

telnetd &

# echo "########## Set System Time #############"
# /etc/init.d/time_conf.sh

echo "########## Run RTMP App #################"
/root/loto_rtmp

/etc/init.d/check_process.sh

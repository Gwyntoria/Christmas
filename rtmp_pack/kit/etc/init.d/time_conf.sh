#!/bin/sh

echo "########## Run ntpdate #############"
ntpdate -u ntp1.aliyun.com 
hwclock -w      # Set hardware clock from system time

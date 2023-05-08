#!/bin/sh
# supervisor process

process_name=loto_rtmp
# if [ -z $process_name ]; then
#     echo "Input parameter is empty."
#     return 0
# fi

while true; do
    p_num=$(ps -e | grep "$process_name" | grep -v "grep" | wc -l)
    # echo "p_num = $p_num"
    # sleep 10

    if [ $p_num==0 ]; then
        # run ntpdate
        # /etc/init.d/time_conf.sh

        echo "########## Reboot #############"
        reboot

        break
    else
        PID=$(ps aux | grep '[l]oto_rtmp' | awk '{print $1}')
        echo "PID=$PID"
    fi
done

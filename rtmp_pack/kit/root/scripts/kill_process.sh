#!/bin/sh

list_process=("loto_conf.sh" "loto_rtmp")

for process in "${list_process[@]}"; do
    pid=$(ps aux | grep "$process" | grep -v "grep" | awk '{print $1}')

    if [ ! -z "$pid" ]; then
        echo "Killing process $process, pid=$pid"
        kill $pid
    else
        echo "Process $process is not running"
    fi
done

# # loto_conf.sh
# pid=$(ps aux | grep '[l]oto_conf.sh' | awk '{print $1}')

# # If the pid was found, kill the process
# if [ ! -z "$pid" ]; then
#     echo "Killing process 'loto_conf.sh', pid=$pid"
#     kill $pid
# else
#     echo "Process 'loto_conf.sh' is not running"
# fi

#!/bin/sh

WORK_DIR=$(pwd)
# echo "WORK_DIR=$WORK_DIR"
KIT_DIR=$WORK_DIR/kit
# echo "KIT_DIR=$KIT_DIR"

echo "===== 1. /root ====="
mv $KIT_DIR/root/loto_rtmp /root
chmod 777 /root/loto_rtmp
mv $KIT_DIR/root/push.conf /root
mv $KIT_DIR/root/update.sh /root

if [ -d "/root/res" ]; then
    rm -rf /root/res
    mv $KIT_DIR/root/res/ /root
else
    mv $KIT_DIR/root/res/ /root
fi

if [ -d "/root/scripts" ]; then
    rm -rf /root/scripts
    mv $KIT_DIR/root/scripts/ /root
    chmod 777 /root/scripts/*
else
    mv $KIT_DIR/root/scripts/ /root
    chmod 777 /root/scripts/*
fi

echo "===== 2. /etc ====="
mv $KIT_DIR/etc/localtime /etc
mv $KIT_DIR/etc/resolv.conf /etc

echo "===== 3. /bin ====="
mv $KIT_DIR/bin/ntpdate /bin

echo "===== 4. /usr/lib ====="
# rtmp lib
mv $KIT_DIR/lib/rtmp_lib/* /usr/lib/
rm $KIT_DIR/lib/rtmp_lib -rf

# opus lib
tar xzf $KIT_DIR/lib/opus_lib/libopus.tgz -C $KIT_DIR/lib/opus_lib/
mv $KIT_DIR/lib/opus_lib/lib/*.so* /usr/lib/
rm $KIT_DIR/lib/opus_lib/ -rf

# echo "===== 5. /var ====="
# tar xzf $KIT_DIR/spool.tgz -C /var
# rm $KIT_DIR/spool.tgz

echo "===== 6. /etc/init.d ====="
mv $KIT_DIR/etc/init.d/* /etc/init.d/
rm $KIT_DIR/etc/init.d/ -rf

echo "===== install complete ====="
echo "Please modify some essential files"
echo "1. rcS:           the value of argument 'osmem' of program 'load3516dv300'"
echo "2. loto_conf.sh:  IP address and so on"
echo "3. push.conf:   device_num, push_url, requested_url, profile and so on"

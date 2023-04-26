# README

## USAGE

1. Execute 'install.sh' directly to install `loto_rtmp`
2. **If necessary**，switch to `/etc/init.d`, and moodify the contents of file `rcS` or `loto_conf.sh`
    - `rcS`: Modify the arguments of `./load3516dv300`, like `-osmem` amd `-total`
    - `loto_conf.sh`: Modify the settings regarding IP addresses
    - `loto_conf.sh`: Modify whether to start `loto_rtmp` automatically on startup, as well as process monitoring script for `loto_rtmp`

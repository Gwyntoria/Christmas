#!/bin/sh

BASE=${PWD}
HOST=arm-himix200-linux
HI_CC=${HOST}-gcc
INSTALL_PATH=${BASE}/arm/curl

# echo "#################### UNINSTALL ####################"
# make uninstall
# make clean

echo "#################### CONFIGURE ####################"
env LDFLAGS=-R/home/karl/Desktop/Library/openssl-1.0.1p/arm/ssl/lib ./configure CC=arm-himix200-linux-gcc --prefix=${INSTALL_PATH} --host=${HOST} --with-ssl=/home/karl/Desktop/Library/openssl-1.0.1p/arm/ssl/

#!/bin/sh

rm -rf build
mkdir build
cd build
ARCH=hisi3516dv300.cmake
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../$ARCH ..
make


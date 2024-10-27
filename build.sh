#!/bin/bash

rm -rf build
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_CLANG=1 ..
make

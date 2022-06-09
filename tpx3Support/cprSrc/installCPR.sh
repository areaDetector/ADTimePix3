#!/bin/bash

git clone https://github.com/libcpr/cpr
cd cpr
mkdir build
cd build
cmake ..
cmake --build .
cp cpr_generated_includes/cpr/cprver.h ../include/cpr/.

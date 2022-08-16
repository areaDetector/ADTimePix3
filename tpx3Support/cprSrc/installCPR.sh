#!/bin/bash

git clone https://github.com/libcpr/cpr
cd cpr
git checkout tags/1.9.1 -b v1.9.1-branch
mkdir build
cd build
cmake ..
cmake --build .
cp cpr_generated_includes/cpr/cprver.h ../include/cpr/.

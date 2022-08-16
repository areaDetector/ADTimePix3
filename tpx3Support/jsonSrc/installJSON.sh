#!/bin/bash

git clone https://github.com/nlohmann/json
cd json
git checkout tags/v3.11.2 -b v3.11.2-branch
mkdir build
cd build
cmake ..
cmake --build .


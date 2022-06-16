#!/bin/bash

git clone https://github.com/nlohmann/json
cd json
mkdir build
cd build
cmake ..
cmake --build .


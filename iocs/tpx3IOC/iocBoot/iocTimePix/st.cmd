#!/bin/bash

export LD_LIBRARY_PATH=../../../../tpx3Support/cprSrc/cpr/build/lib:$LD_LIBRARY_PATH

../../bin/linux-x86_64/tpx3App st_base.cmd

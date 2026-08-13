#!/bin/bash
# Timepix3 IOC launcher — creates profile autosave dirs, then runs st_base.cmd.
cd "$(dirname "$0")"
mkdir -p autosave/tpx3 autosave/mpx3
exec ../../bin/linux-x86_64/tpx3App st_base.cmd "$@"

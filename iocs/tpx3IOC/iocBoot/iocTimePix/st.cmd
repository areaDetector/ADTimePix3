#!/bin/bash
# Timepix3 IOC launcher — creates profile autosave dirs, then runs profiles/tpx3/st.cmd.
cd "$(dirname "$0")"
mkdir -p autosave/tpx3 autosave/mpx3 autosave/tpx4
exec ../../bin/linux-x86_64/tpx3App profiles/tpx3/st.cmd "$@"

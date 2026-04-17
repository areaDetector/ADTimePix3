# Detector initialization: single source of truth for channel/config PVs.
# Source from st_base.cmd after iocInit(); safe to re-run from iocsh after reconnect.
#
# Two-phase layout (reduces 409/log spam when SERVAL is up but detector is off):
#   1) init_detector_paths.cmd — TCP paths, templates, BPC/DACS *paths* only (no SERVAL push).
#   2) init_detector_hw.cmd    — WriteData and mode PVs that call fileWriter/sendConfiguration.
#
# Offline detector at boot: in st_base.cmd, use only:
#     < init_detector_paths.cmd
# When hardware is ready:
#     dbpf $(PREFIX)cam1:RefreshConnection 1
#     < init_detector_hw.cmd
# Or merge both into one st_base.cmd line once the beamline always has a detector at IOC start.
#
# Site files (e.g. BL10): keep local path/calibration edits in a copy of init_detector_paths.cmd
# and put mask / WriteBPC / DetOrient / DAC writes in init_detector_hw.cmd (or init_detector_hw_bl10.cmd).

< init_detector_paths.cmd
< init_detector_hw.cmd

# Push detector/channel configuration to SERVAL (HTTP). Expect409 / errors if the
# detector is not connected — IOC should stay up; fix hardware, then re-run this file
# or dbpf $(PREFIX)cam1:ApplyConfig 1 after RefreshConnection.
#
# Beamline: merge extra dbpf blocks into this file or profiles/tpx3/init/hw_site.cmd.

epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:WriteData","1")   # fileWriter() + getServer(); push file-output channel config to SERVAL (not the same as BPC/DACS)

# BPC / DACS "calibration" PVs: driver uploadBPC() / uploadDACS() -- HTTP GET to SERVAL
#   /config/load?format=pixelconfig|dacs&file=<BPCFilePath><BPCFileName> (paths in init/paths.cmd).
# SERVAL must be up; file paths must be readable on the *SERVAL host* (URL is resolved there). If uploads fail,
# check cam1:HttpCode, cam1:WriteFileMessage, and that RefreshConnection / detector readiness match your site.
epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:WriteBPCFile","1")  # Instruct SERVAL to load BPC from disk (see paths above)
epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:WriteDACSFile","1")  # Instruct SERVAL to load DACS from disk (see paths above)

# Simplify startup / acquisition mode (touches driver → may hit SERVAL)
dbpf("$(PREFIX)cam1:ImageMode","2")     # areaDetector Continuous
dbpf("$(PREFIX)cam1:TriggerMode","5")   # Timepix3 CONTINUOUS
dbpf("$(PREFIX)Pva1:EnableCallbacks","1")     # PVA1 plugin
dbpf("$(PREFIX)Stats1:EnableCallbacks","1")   # STATS1 — cursor profiles for PVA preview (ProfileStats)
dbpf("$(PREFIX)Stats5:EnableCallbacks","1")   # STATS5 plugin
dbpf("$(PREFIX)cam1:NumImages","1000000000")  # Pseudo-unlimited; use 0 for true unlimited on hardware

# Enable NDStats row/column profiles for Phoebus (ADCore NDStatsProfiles.template).
# Requires stats_profiles.cmd before iocInit (see profiles/tpx3/st.cmd).
dbpf("$(PREFIX)$(STATS_PROF_R)StatsProfInit_.PROC","1")

#dbpf("$(PREFIX)cam1:Health.SCAN","I/O Intr")   # Do not scan

# Push Medipix3 channel configuration and calibration to SERVAL.
# BPC/DACS paths are set in init_detector_paths_mpx3.cmd ($(ADTIMEPIX)/vendor/mpx3/).
# Upload requires SERVAL up and files readable on the SERVAL host.

epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:WriteData","1")

epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:WriteBPCFile","1")
epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:WriteDACSFile","1")

epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:ImageMode","2")
dbpf("$(PREFIX)cam1:TriggerMode","5")
# Dual-threshold (BothCounters=Yes): Serval rejects CONTINUOUS — use TriggerMode 4
# (AUTOTRIGSTART_TIMERSTOP), PrvImgThs 0,1, WriteData, and TriggerPeriod >= ~0.15 s.
dbpf("$(PREFIX)Pva1:EnableCallbacks","1")
dbpf("$(PREFIX)Pva2:EnableCallbacks","1")
dbpf("$(PREFIX)Stats5:EnableCallbacks","1")
dbpf("$(PREFIX)cam1:NumImages","1000000000")

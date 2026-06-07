# Push Medipix3 channel configuration to SERVAL. Skips BPC/DACS upload — MPX3 uses
# different pixel config size and DAC names (Threshold[0..7], etc.). Load calibration
# from site-specific files once available.

epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:WriteData","1")

epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:ImageMode","2")
dbpf("$(PREFIX)cam1:TriggerMode","5")
dbpf("$(PREFIX)Pva1:EnableCallbacks","1")
dbpf("$(PREFIX)Stats5:EnableCallbacks","1")
dbpf("$(PREFIX)cam1:NumImages","1000000000")

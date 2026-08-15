# Push Medipix3 calibration and Erik-validated dual-threshold acquisition defaults to SERVAL.
# BPC/DACS paths: init_detector_paths_mpx3.cmd ($(ADTIMEPIX)/vendor/mpx3/).
# Recipe: documentation/medipix3/integration.md (BothCounters, AutoTrgSt_TmrSp, 4 triggers @ 0.5 s).

epicsThreadSleep(2)

# Detector + acquisition (each dbpf may call initAcquisition and PUT /detector/config)
dbpf("$(PREFIX)cam1:BiasVolt","100")
dbpf("$(PREFIX)cam1:GainMode","1")          # HGM — calibrated ~10 keV (Accos dual-threshold profile)
dbpf("$(PREFIX)cam1:PixelDepth","2")   # mbbo index 2 = 12-bit (TWVL 12); OpenAPI also allows 1, 6, 24
dbpf("$(PREFIX)cam1:ImageMode","1")          # Multiple (finite triggers)
dbpf("$(PREFIX)cam1:TriggerMode","4")       # AutoTrgSt_TmrSp — required with BothCounters (not Continuous)
dbpf("$(PREFIX)cam1:BothCounters","1")
dbpf("$(PREFIX)cam1:NumImages","4")
dbpf("$(PREFIX)cam1:AcquirePeriod","0.5")
dbpf("$(PREFIX)cam1:AcquireTime","0.495")

epicsThreadSleep(1)
dbpf("$(PREFIX)cam1:WriteBPCFile","1")
epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:WriteDACSFile","1")

# Push destination (PrvImgThs 0,1 from init_detector_paths_mpx3.cmd) after detector config
epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:WriteData","1")

dbpf("$(PREFIX)Pva1:EnableCallbacks","1")
dbpf("$(PREFIX)Pva2:EnableCallbacks","1")
dbpf("$(PREFIX)Pva3:EnableCallbacks","1")
dbpf("$(PREFIX)Pva4:EnableCallbacks","1")
dbpf("$(PREFIX)Pva5:EnableCallbacks","1")
dbpf("$(PREFIX)Pva6:EnableCallbacks","1")
dbpf("$(PREFIX)Stats5:EnableCallbacks","1")

# Enable NDStats row/column profiles for Phoebus (ADCore NDStatsProfiles.template).
# Requires < $(ADCORE)/iocBoot/stats_profiles.cmd before iocInit (see st_mpx3.cmd).
dbpf("$(PREFIX)$(STATS_PROF_R)StatsProfInit_.PROC","1")

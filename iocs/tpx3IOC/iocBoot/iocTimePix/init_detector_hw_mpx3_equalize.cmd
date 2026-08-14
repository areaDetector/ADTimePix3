# Erik ASI first hardware checkout (June 2026) — equalization, single threshold.
# See documentation/medipix3/integration.md § "ASI hardware checkout".
# Not for dual-counter / IXS band-pass testing (use init_detector_hw_mpx3.cmd instead).
#
# Threshold DAC ~50-90: set via Accos equalization or chip DAC / .dacs before acquire.
# GainMode SLGM (super-low): Erik Aug 2026 — frame tests without calibration (noise-hit pattern).

epicsThreadSleep(2)

dbpf("$(PREFIX)cam1:BiasVolt","100")
dbpf("$(PREFIX)cam1:Polarity","0")          # Positive (Si 300 um — see delivery sheet)
dbpf("$(PREFIX)cam1:GainMode","3")          # SLGM — super-low gain (uncalibrated frame / equalization tests)
dbpf("$(PREFIX)cam1:PixelDepth","12")
dbpf("$(PREFIX)cam1:ChargeSumming","0")
dbpf("$(PREFIX)cam1:Colour","0")
dbpf("$(PREFIX)cam1:BothCounters","0")
dbpf("$(PREFIX)cam1:PrvImgThs","0")
dbpf("$(PREFIX)cam1:PrvImg1Ths","0")
dbpf("$(PREFIX)cam1:ImageMode","1")         # Multiple (finite triggers)
dbpf("$(PREFIX)cam1:TriggerMode","4")       # AutoTrgSt_TmrSp
dbpf("$(PREFIX)cam1:NumImages","20")
dbpf("$(PREFIX)cam1:AcquireTime","0.495")   # 495 ms shutter high
dbpf("$(PREFIX)cam1:AcquirePeriod","0.5")   # includes ~5 ms shutter down

epicsThreadSleep(1)
dbpf("$(PREFIX)cam1:WriteBPCFile","1")
epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:WriteDACSFile","1")

epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:WriteData","1")

dbpf("$(PREFIX)Pva1:EnableCallbacks","1")
dbpf("$(PREFIX)Pva3:EnableCallbacks","1")

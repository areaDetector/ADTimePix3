# IXS dual-threshold band-pass (T0 - T1) — legacy NDPluginProcess init.
# Prefer driver NDArray addr 11/12 (wired in st_mpx3.cmd); use this only with the
# commented plugin block in st_mpx3.cmd (not together with driver Pva5/Pva6).

epicsThreadSleep(1)

# --- Frame difference (8088 preview): threshold 0 minus threshold 1 ---
dbpf("$(PREFIX)procFrameDiff:NDArrayPort","Image1")
dbpf("$(PREFIX)procFrameDiff:NDArrayAddress","0")
dbpf("$(PREFIX)procFrameDiff:EnableBackground","1")
dbpf("$(PREFIX)procFrameDiff:EnableArrayCallbacks","1")
dbpf("$(PREFIX)procFrameDiff:DataTypeOut","4")
dbpf("$(PREFIX)imageDiff1:EnableCallbacks","1")
dbpf("$(PREFIX)Pva5:EnableCallbacks","1")

# --- Integrated difference (8089 preview): integrated T0 minus integrated T1 ---
dbpf("$(PREFIX)procIntDiff:NDArrayPort","ImageInt1")
dbpf("$(PREFIX)procIntDiff:NDArrayAddress","0")
dbpf("$(PREFIX)procIntDiff:EnableBackground","1")
dbpf("$(PREFIX)procIntDiff:EnableArrayCallbacks","1")
dbpf("$(PREFIX)procIntDiff:DataTypeOut","4")
dbpf("$(PREFIX)imageIntDiff1:EnableCallbacks","1")
dbpf("$(PREFIX)Pva6:EnableCallbacks","1")

# DataTypeOut=4 -> NDInt32 (signed per-pixel T0-T1). Tune clipping/scale in proc* screens if needed.

# Manual one-shot background capture (if auto trig records are disabled):
#   caput $(PREFIX)procFrameDiff:SaveTh1BgSeq 1
#   caput $(PREFIX)procIntDiff:SaveIntTh1BgSeq 1

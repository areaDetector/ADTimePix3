# Phase C: configure HDF5 for full-rate Image[] demux (TCP 8086).
# Requires plugins in st_mpx3.cmd: HDFImgT0 (addr 1), HDFImgT1 (addr 13).
#
# NDFile Stream Capture needs ≥1 NDArray for dimensions before Capture=1.
# Sequence:
#   mkdir -p /tmp/mpx3_hdf
#   < init_detector_img_mpx3.cmd
#   < init_detector_hdf5_img_mpx3.cmd     # this file (does NOT arm Capture)
#   dbpf("$(PREFIX)cam1:Acquire","1")     # latch dimensions on HDF plugins
#   # wait until idle / frames arrive, then:
#   < init_detector_hdf5_img_mpx3_arm.cmd
#   dbpf("$(PREFIX)cam1:Acquire","1")     # stream writes while Capture=1
#
# Do not drive Phoebus from Pva7/Pva8 at high rate — this script disables them.
# Port 8087 (Image[1] integrated companion) is deferred — revisit later.
#
# Default output directory (edit this line for NVMe, etc.):
epicsEnvSet("MPX3_HDF_PATH", "/tmp/mpx3_hdf")
# Create the directory on the host before Capture if it does not exist.

# Disable live PVA on full-rate Image (keep Preview Pva1–Pva6 for OP)
dbpf("$(PREFIX)Pva7:EnableCallbacks","0")
dbpf("$(PREFIX)Pva8:EnableCallbacks","0")
dbpf("$(PREFIX)imageImg1:EnableCallbacks","0")
dbpf("$(PREFIX)imageImgTh1:EnableCallbacks","0")

# HDF5 T0 (NDArray addr 1) and T1 (addr 13) — config only; do not Capture yet
# FileTemplate is required (autosave may leave it empty → H5Fcreate "invalid file name")
dbpf("$(PREFIX)HDFImgT0:FilePath","$(MPX3_HDF_PATH)/")
dbpf("$(PREFIX)HDFImgT0:FileName","img_t0")
dbpf("$(PREFIX)HDFImgT0:FileTemplate","%s%s_%3.3d.h5")
dbpf("$(PREFIX)HDFImgT0:FileNumber","0")
dbpf("$(PREFIX)HDFImgT0:AutoIncrement","1")
dbpf("$(PREFIX)HDFImgT0:FileWriteMode","2")
dbpf("$(PREFIX)HDFImgT0:NumCapture","100")
dbpf("$(PREFIX)HDFImgT0:EnableCallbacks","1")

dbpf("$(PREFIX)HDFImgT1:FilePath","$(MPX3_HDF_PATH)/")
dbpf("$(PREFIX)HDFImgT1:FileName","img_t1")
dbpf("$(PREFIX)HDFImgT1:FileTemplate","%s%s_%3.3d.h5")
dbpf("$(PREFIX)HDFImgT1:FileNumber","0")
dbpf("$(PREFIX)HDFImgT1:AutoIncrement","1")
dbpf("$(PREFIX)HDFImgT1:FileWriteMode","2")
dbpf("$(PREFIX)HDFImgT1:NumCapture","100")
dbpf("$(PREFIX)HDFImgT1:EnableCallbacks","1")

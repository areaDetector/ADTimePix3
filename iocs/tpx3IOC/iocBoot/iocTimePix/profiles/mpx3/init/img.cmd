# Opt-in: enable Medipix3 full-rate Image[] TCP (port 8086) and push destination to Serval.
# Paths/format/thresholds come from profiles/mpx3/init/paths.cmd (WriteImg stays 0 there).
#
# Usage (iocsh after IOC is up):
#   < profiles/mpx3/init/img.cmd
#
# Keep Preview 8088/8089 on for operator feedback. Do not drive Phoebus from full-rate Image.
# Next acquire starts imgWorker when WriteImg=1, path is tcp://, and ImgAccumulationEnable=1.
# For HDF5 soak (Phase C): < profiles/mpx3/init/hdf5_img.cmd after this file.
# Port 8087 (optional Image[1] integrated companion) is deferred — revisit later.
#
# To disable again:
#   dbpf("$(PREFIX)cam1:WriteImg","0")
#   dbpf("$(PREFIX)cam1:WriteData","1")

dbpf("$(PREFIX)cam1:ImgFilePath","tcp://listen@localhost:8086")
dbpf("$(PREFIX)cam1:ImgFileFmt","3")
dbpf("$(PREFIX)cam1:ImgFileMode","0")
dbpf("$(PREFIX)cam1:ImgThs","0,1")
dbpf("$(PREFIX)cam1:ImgAccumulationEnable","1")
dbpf("$(PREFIX)cam1:WriteImg","1")
dbpf("$(PREFIX)cam1:WriteData","1")

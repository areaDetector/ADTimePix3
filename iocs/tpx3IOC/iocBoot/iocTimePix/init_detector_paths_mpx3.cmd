# Medipix3 channel/path defaults (frame + two-layer preview). Safe when detector is offline.
# Reference: ADMediPix3 configs/serval/serval_mpx3.json

dbpf("$(PREFIX)cam1:RawFilePath","file:/media/nvme/raw")
dbpf("$(PREFIX)cam1:RawFileTemplate","raw%MdHms_")
dbpf("$(PREFIX)cam1:WriteRaw","0")

dbpf("$(PREFIX)cam1:Raw1FilePath","file:/media/nvme/raw1")
dbpf("$(PREFIX)cam1:Raw1FileTemplate","raw%MdHms_")
dbpf("$(PREFIX)cam1:WriteRaw1","0")

# Main image channel: count mode, file output disabled by default
dbpf("$(PREFIX)cam1:ImgFilePath","file:/media/nvme/img")
dbpf("$(PREFIX)cam1:ImgFileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:ImgFileFmt","0")
dbpf("$(PREFIX)cam1:ImgFileMode","0")
dbpf("$(PREFIX)cam1:ImgIntgSize","0")
dbpf("$(PREFIX)cam1:ImgStpOnDskLim","1")
dbpf("$(PREFIX)cam1:ImgQueueSize","1024")
dbpf("$(PREFIX)cam1:WriteImg","0")

dbpf("$(PREFIX)cam1:Img1FilePath","file:/media/nvme/img1")
dbpf("$(PREFIX)cam1:Img1FileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:Img1FileFmt","0")
dbpf("$(PREFIX)cam1:Img1FileMode","0")
dbpf("$(PREFIX)cam1:WriteImg1","0")

# Two-layer preview: counter 0 (frame) and counter 1 (running sum)
dbpf("$(PREFIX)cam1:PrvImgFilePath","tcp://listen@localhost:8088")
dbpf("$(PREFIX)cam1:PrvImgFileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:PrvImgFileFmt","3")
dbpf("$(PREFIX)cam1:PrvImgFileMode","0")
dbpf("$(PREFIX)cam1:PrvImgIntgSize","1")
dbpf("$(PREFIX)cam1:PrvImgIntgMode","0")
dbpf("$(PREFIX)cam1:PrvStpOnDskLim","0")
dbpf("$(PREFIX)cam1:PrvImgQueueSize","16")
dbpf("$(PREFIX)cam1:WritePrvImg","1")

dbpf("$(PREFIX)cam1:PrvImg1FilePath","tcp://listen@localhost:8089")
dbpf("$(PREFIX)cam1:PrvImg1FileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:PrvImg1FileFmt","3")
dbpf("$(PREFIX)cam1:PrvImg1FileMode","0")
dbpf("$(PREFIX)cam1:PrvImg1IntgSize","-1")
dbpf("$(PREFIX)cam1:PrvImg1IntgMode","0")
dbpf("$(PREFIX)cam1:Prv1StpOnDskLim","0")
dbpf("$(PREFIX)cam1:PrvImg1QueueSize","16")
dbpf("$(PREFIX)cam1:WritePrvImg1","1")

dbpf("$(PREFIX)cam1:PrvPeriod","0.05")
dbpf("$(PREFIX)cam1:PrvSmplgMode","0")

# ToF histogram preview is out of scope for Medipix3 v1
dbpf("$(PREFIX)cam1:WritePrvHst","0")

# Calibration paths — site-specific; point at Medipix3 files when available
dbpf("$(PREFIX)cam1:BPCFilePath","$(ADTIMEPIX)/vendor/")
dbpf("$(PREFIX)cam1:BPCFileName","tpx3-demo.bpc")
dbpf("$(PREFIX)cam1:DACSFilePath","$(ADTIMEPIX)/vendor/")
dbpf("$(PREFIX)cam1:DACSFileName","tpx3-demo.dacs")

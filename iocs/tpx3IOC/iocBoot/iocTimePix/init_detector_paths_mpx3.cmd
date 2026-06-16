# Medipix3 channel/path defaults (preview TCP 8088; dual-threshold PrvImgThs 0,1). Safe when detector is offline.
# Reference: ADMediPix3 configs/serval/serval_mpx3.json; Erik Accos recipe — documentation/medipix3/integration.md

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

# Frame preview on TCP 8088; integrated preview on TCP 8089 (prvImg1Worker in driver).
dbpf("$(PREFIX)cam1:PrvImgFilePath","tcp://listen@localhost:8088")
dbpf("$(PREFIX)cam1:PrvImgFileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:PrvImgFileFmt","3")
dbpf("$(PREFIX)cam1:PrvImgFileMode","0")
dbpf("$(PREFIX)cam1:PrvImgIntgSize","1")
dbpf("$(PREFIX)cam1:PrvImgIntgMode","0")
dbpf("$(PREFIX)cam1:PrvStpOnDskLim","0")
dbpf("$(PREFIX)cam1:PrvImgQueueSize","16")
# Dual threshold (BothCounters): thresholds 0 and 1 only on one preview channel
dbpf("$(PREFIX)cam1:PrvImgThs","0,1")
dbpf("$(PREFIX)cam1:WritePrvImg","1")

# Second preview layer: integrated-from-measurement-start on TCP 8089
dbpf("$(PREFIX)cam1:PrvImg1FilePath","tcp://listen@localhost:8089")
dbpf("$(PREFIX)cam1:PrvImg1FileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:PrvImg1FileFmt","3")
dbpf("$(PREFIX)cam1:PrvImg1FileMode","0")
dbpf("$(PREFIX)cam1:PrvImg1IntgSize","-1")
dbpf("$(PREFIX)cam1:PrvImg1IntgMode","2")
dbpf("$(PREFIX)cam1:Prv1StpOnDskLim","0")
dbpf("$(PREFIX)cam1:PrvImg1QueueSize","16")
dbpf("$(PREFIX)cam1:PrvImg1Ths","0,1")
dbpf("$(PREFIX)cam1:WritePrvImg1","1")

dbpf("$(PREFIX)cam1:PrvPeriod","0.5")
dbpf("$(PREFIX)cam1:PrvSmplgMode","0")

# ToF histogram preview is out of scope for Medipix3 v1
dbpf("$(PREFIX)cam1:WritePrvHst","0")

# Medipix3 calibration (paths must end with / — driver concatenates path + filename).
# Files must be readable on the SERVAL host at the resolved absolute path.
dbpf("$(PREFIX)cam1:BPCFilePath","$(ADTIMEPIX)/vendor/mpx3/")
dbpf("$(PREFIX)cam1:BPCFileName","eq-01.bpc")
dbpf("$(PREFIX)cam1:DACSFilePath","$(ADTIMEPIX)/vendor/mpx3/")
dbpf("$(PREFIX)cam1:DACSFileName","eq-01.dacs")

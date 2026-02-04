# Detector initialization: single source of truth for channel/config PVs.
# Source this file from st_base.cmd after iocInit(); can be re-run from iocsh
# (e.g. after reconnect) to re-apply the same init. Driver applies these PVs
# to SERVAL when WriteData=1 (or when ApplyConfig is triggered).

dbpf("$(PREFIX)cam1:RawFilePath","file:/media/nvme/raw")    # tcp://listen@localhost:8085 for stream
dbpf("$(PREFIX)cam1:RawFileTemplate","raw%MdHms_")
dbpf("$(PREFIX)cam1:WriteRaw","0")   # Select raw disk write, or stream

dbpf("$(PREFIX)cam1:Raw1FilePath","tcp://listen@localhost:8085")    # tcp://listen@localhost:8085 for stream
dbpf("$(PREFIX)cam1:Raw1FileTemplate","raw%MdHms_")
dbpf("$(PREFIX)cam1:WriteRaw1","0")   # Select raw disk write, or stream

dbpf("$(PREFIX)cam1:ImgFilePath","tcp://listen@localhost:8087")
dbpf("$(PREFIX)cam1:ImgFileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:ImgFileFmt","3")    # jsonimage (TCP streaming)
dbpf("$(PREFIX)cam1:ImgFileMode","1")   # tot
dbpf("$(PREFIX)cam1:ImgIntgSize","1")   # Integration size: 1
dbpf("$(PREFIX)cam1:ImgIntgMode","0")   # sum (for IntegrationSize=1)
dbpf("$(PREFIX)cam1:ImgStpOnDskLim","0")   # false
dbpf("$(PREFIX)cam1:ImgQueueSize","1024")   # Queue size: 1024
dbpf("$(PREFIX)cam1:WriteImg","0")   # Select img disk write

dbpf("$(PREFIX)cam1:Img1FilePath","file:/media/nvme/img1")
dbpf("$(PREFIX)cam1:Img1FileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:Img1FileFmt","0")    # tiff
dbpf("$(PREFIX)cam1:Img1FileMode","1")   # tot
dbpf("$(PREFIX)cam1:Img1IntgMode","1")   # average
dbpf("$(PREFIX)cam1:Img1StpOnDskLim","1")   # true
dbpf("$(PREFIX)cam1:WriteImg1","0")   # Select img disk write

dbpf("$(PREFIX)cam1:PrvImgFilePath","tcp://listen@localhost:8089")
dbpf("$(PREFIX)cam1:PrvImgFileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:PrvImgFileFmt","3")    # 3: jsonimage (TCP streaming)
dbpf("$(PREFIX)cam1:PrvImgFileMode","1")   # tot
dbpf("$(PREFIX)cam1:PrvImgIntgSize","1")   # Sum 1 frame, -1,0,1,..,32
dbpf("$(PREFIX)cam1:PrvImgIntgMode","0")   # 0=sum, 1=average
dbpf("$(PREFIX)cam1:PrvStpOnDskLim","0")   # false
dbpf("$(PREFIX)cam1:PrvImgQueueSize","160")   # Increase from 16 to 160
dbpf("$(PREFIX)cam1:PrvPeriod","0.5")	# Preview 2 Hz, 1.0=once per second
dbpf("$(PREFIX)cam1:WritePrvImg","1")   # Select Preview write

dbpf("$(PREFIX)cam1:PrvImg1FilePath","file:/media/nvme/prv")
dbpf("$(PREFIX)cam1:PrvImg1FileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:PrvImg1FileFmt","0")    # 0: tiff (60 fps), 2: png (37 fps)
dbpf("$(PREFIX)cam1:PrvImg1FileMode","1")   # tot
dbpf("$(PREFIX)cam1:PrvImg1IntgMode","0")   # sum
dbpf("$(PREFIX)cam1:Prv1StpOnDskLim","0")   # false
dbpf("$(PREFIX)cam1:WritePrvImg1","0")   # Select preview disk write

# Histogram
dbpf("$(PREFIX)cam1:PrvHstFilePath","tcp://listen@localhost:8451")
dbpf("$(PREFIX)cam1:PrvHstFileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:PrvHstFileFmt","4")    # jsonhisto
dbpf("$(PREFIX)cam1:PrvHstFileMode","3")   # tof
dbpf("$(PREFIX)cam1:PrvHstIntgSize","0")   # Size=0; does not work for -1,1..32?
dbpf("$(PREFIX)cam1:PrvHstIntgMode","0")   # 0=sum; 1=average
dbpf("$(PREFIX)cam1:PrvHstNumBins","16000")      # 16000 bins, 16000 x 1 [us] = 16 [ms]
dbpf("$(PREFIX)cam1:PrvHstBinWidth","0.000001")  # 1us, bin width in sec
dbpf("$(PREFIX)cam1:PrvHstOffset","0.0")         # Offset in sec
dbpf("$(PREFIX)cam1:PrvHstStpOnDskLim","0")   # false
dbpf("$(PREFIX)cam1:WritePrvHst","0")   # Select histogram stream

epicsThreadSleep(2)
dbpf("$(PREFIX)cam1:WriteData","1")   # Write selected data files to disk and stream to socket

# One Chip calibration
#dbpf("$(PREFIX)cam1:BPCFilePath","/epics/support/areaDetector/ADTimePix3/vendor/oneChip")
#dbpf("$(PREFIX)cam1:BPCFileName","eq.bpc")       # load BPC calibration
#dbpf("$(PREFIX)cam1:DACSFilePath","/epics/support/areaDetector/ADTimePix3/vendor/oneChip")
#dbpf("$(PREFIX)cam1:DACSFileName","eq.dacs")     # load DACS calibration
# Four chip calibration
dbpf("$(PREFIX)cam1:BPCFilePath","/epics/support/areaDetector/ADTimePix3/vendor/")
dbpf("$(PREFIX)cam1:BPCFileName","tpx3-demo.bpc")       # load BPC calibration
dbpf("$(PREFIX)cam1:DACSFilePath","/epics/support/areaDetector/ADTimePix3/vendor/")
dbpf("$(PREFIX)cam1:DACSFileName","tpx3-demo.dacs")     # load DACS calibration

# Simplify startup
dbpf("$(PREFIX)cam1:ImageMode","2")     # areaDetector Continuous
dbpf("$(PREFIX)cam1:TriggerMode","5")   # Timepix3 CONTINUOUS
dbpf("$(PREFIX)Pva1:EnableCallbacks","1")     # PVA1 plugin
dbpf("$(PREFIX)Stats5:EnableCallbacks","1")   # STATS5 plugin
dbpf("$(PREFIX)cam1:NumImages","1000000000")  # Pseudo-unlimited maximum number of images, 2147483647 = 2^31 - 1; Physical detector unlimited set to 0

#dbpf("$(PREFIX)cam1:Health.SCAN","I/O Intr")   # Do not scan

#!../../bin/linux-x86_64/tpx3App

< envPaths

< unique.cmd


epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")
errlogInit(20000)

#epicsThreadSleep(20)
dbLoadDatabase("$(ADTIMEPIX)/iocs/tpx3IOC/dbd/tpx3App.dbd")
tpx3App_registerRecordDeviceDriver(pdbbase)




# epicsEnvSet("SERVER_URL", "http://localhost:8080")
epicsEnvSet("SERVER_URL", "http://localhost:8081")
epicsEnvSet("PREFIX", "TPX3-TEST:")
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db:$(ADTIMEPIX)/db")


# If searching for device by product ID put "" or empty string for serial number
ADTimePixConfig("$(PORT)", "$(SERVER_URL)", 0, 0, 0, 0)
epicsThreadSleep(2)

asynSetTraceIOMask($(PORT), 0, 2)
#asynSetTraceMask($(PORT),0,0xff)

#dbLoadRecords("$(ADCORE)/db/ADBase.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/TimePix3Base.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/ADTimePix3.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP0,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP1,PORT=$(PORT),ADDR=1,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP2,PORT=$(PORT),ADDR=2,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP3,PORT=$(PORT),ADDR=3,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/File.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Server.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Measurement.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Dashboard.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")

# One chip mask, below 4-chip mask
#dbLoadRecords("$(ADTIMEPIX)/db/MaskBPC.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1,TYPE=Int32,FTVL=LONG,NELEMENTS=65536")
dbLoadRecords("$(ADTIMEPIX)/db/MaskBPC.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1,TYPE=Int32,FTVL=LONG,NELEMENTS=262144")

dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr0,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr1,PORT=$(PORT),ADDR=1,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr2,PORT=$(PORT),ADDR=2,TIMEOUT=1")

#
# Create a standard arrays plugin, set it to get data from Driver.
#int NDStdArraysConfigure(const char *portName, int queueSize, int blockingCallbacks, const char *NDArrayPort, int NDArrayAddr, int maxBuffers, size_t maxMemory,
#                          int priority, int stackSize, int maxThreads)
NDStdArraysConfigure("Image1", 3, 0, "$(PORT)", 0)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=20000000")

#
# Load all other plugins using commonPlugins.cmd
< $(ADCORE)/iocBoot/commonPlugins.cmd
#

set_requestfile_path("$(ADTIMEPIX)/tpx3App/Db")

#asynSetTraceMask($(PORT),0,0x09)
#asynSetTraceMask($(PORT),0,0x11)
iocInit()

dbpf("$(PREFIX)cam1:RawFilePath","file:/media/nvme/raw")    # tcp://localhost:8085 for stream
dbpf("$(PREFIX)cam1:RawFileTemplate","raw%MdHms_")
dbpf("$(PREFIX)cam1:WriteRaw","0")   # Select raw disk write, or stream

dbpf("$(PREFIX)cam1:ImgFilePath","file:/media/nvme/img")
dbpf("$(PREFIX)cam1:ImgFileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:ImgFileFmt","0")    # tiff
dbpf("$(PREFIX)cam1:ImgFileMode","1")   # tot
dbpf("$(PREFIX)cam1:ImgIntgMode","1")   # average
dbpf("$(PREFIX)cam1:StpOnDskLim","1")   # true
dbpf("$(PREFIX)cam1:WriteImg","0")   # Select img disk write

dbpf("$(PREFIX)cam1:PrvImgFilePath","http://localhost")
dbpf("$(PREFIX)cam1:PrvImgFileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:PrvImgFileFmt","2")    # png
dbpf("$(PREFIX)cam1:PrvImgFileMode","1")   # tot
dbpf("$(PREFIX)cam1:PrvImgIntgMode","1")   # average
dbpf("$(PREFIX)cam1:PrvStpOnDskLim","0")   # false
dbpf("$(PREFIX)cam1:PrvPeriod","1.0")	# Preview once per second
dbpf("$(PREFIX)cam1:WritePrvImg","1")   # Select preview stream

dbpf("$(PREFIX)cam1:PrvImg1FilePath","file:/media/nvme/prv")
dbpf("$(PREFIX)cam1:PrvImg1FileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:PrvImg1FileFmt","2")    # png
dbpf("$(PREFIX)cam1:PrvImg1FileMode","1")   # tot
dbpf("$(PREFIX)cam1:PrvImg1IntgMode","0")   # sum
dbpf("$(PREFIX)cam1:Prv1StpOnDskLim","0")   # false
dbpf("$(PREFIX)cam1:WritePrvImg1","0")   # Select preview disk write

dbpf("$(PREFIX)cam1:PrvHstFilePath","tcp://listen@localhost:8451")
dbpf("$(PREFIX)cam1:PrvHstFileTemplate","f%MdHms_")
dbpf("$(PREFIX)cam1:PrvHstFileFmt","4")    # jsonhisto
dbpf("$(PREFIX)cam1:PrvHstFileMode","3")   # tof
dbpf("$(PREFIX)cam1:PrvHstIntgSize","0")   # Size=0; does not work for -1,1..32?
dbpf("$(PREFIX)cam1:PrvHstIntgMode","0")   # sum
dbpf("$(PREFIX)cam1:PrvStpOnDskLim","0")   # false
dbpf("$(PREFIX)cam1:PrvHstNumBins","10")   # 10
dbpf("$(PREFIX)cam1:PrvHstBinWidth","0.0001")   # 100 us
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
dbpf("$(PREFIX)cam1:ImageMode","2")     # areaDetector Continous
dbpf("$(PREFIX)cam1:TriggerMode","5")   # Timepix3 CONTINOUS
dbpf("$(PREFIX)Pva1:EnableCallbacks","1")     # PVA1 plugin
dbpf("$(PREFIX)Stats5:EnableCallbacks","1")   # STATS5 plugin

#dbpf("$(PREFIX)cam1:Health.SCAN","I/O Intr")   # Do not scan

# save things every thirty seconds
create_monitor_set("auto_settings.req", 30, "P=$(PREFIX)")

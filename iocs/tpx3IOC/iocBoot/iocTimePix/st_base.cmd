#!../../bin/linux-x86_64/tpx3App

< envPaths

< unique.cmd


epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")
errlogInit(20000)

#epicsThreadSleep(20)
dbLoadDatabase("$(ADTIMEPIX)/iocs/tpx3IOC/dbd/tpx3App.dbd")
tpx3App_registerRecordDeviceDriver(pdbbase)




epicsEnvSet("SERVER_URL", "http://localhost:8080")
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
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP1,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP2,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Chips.template","P=$(PREFIX),R=cam1:,C=CHIP3,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/File.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Server.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Measurement.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
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

dbpf("$(PREFIX)cam1:RawFilePath","/media/nvme/raw")
dbpf("$(PREFIX)cam1:RawFileTemplate","raw%Hms_")

dbpf("$(PREFIX)cam1:ImgFilePath","/media/nvme/img")
dbpf("$(PREFIX)cam1:ImgFileTemplate","f%Hms_")
dbpf("$(PREFIX)cam1:ImgFileFmt","0")    # tiff
dbpf("$(PREFIX)cam1:ImgFileMode","1")   # tot
dbpf("$(PREFIX)cam1:ImgIntgMode","1")   # average
dbpf("$(PREFIX)cam1:StpOnDskLim","1")   # true

dbpf("$(PREFIX)cam1:PrvImgFilePath","/media/nvme/prv")
dbpf("$(PREFIX)cam1:PrvImgFileTemplate","f%Hms_")
dbpf("$(PREFIX)cam1:PrvImgFileFmt","2")    # png
dbpf("$(PREFIX)cam1:PrvImgFileMode","1")   # tot
dbpf("$(PREFIX)cam1:PrvImgIntgMode","1")   # average
dbpf("$(PREFIX)cam1:PrvStpOnDskLim","0")   # false

dbpf("$(PREFIX)cam1:PrvImg1FilePath","http://localhost")
dbpf("$(PREFIX)cam1:PrvImg1FileFmt","0")    # tiff
dbpf("$(PREFIX)cam1:PrvImg1FileMode","1")   # tot
dbpf("$(PREFIX)cam1:PrvImg1IntgMode","1")   # average
dbpf("$(PREFIX)cam1:Prv1StpOnDskLim","0")   # false

dbpf("$(PREFIX)cam1:PrvHstFilePath","/media/nvme/hst")
dbpf("$(PREFIX)cam1:PrvHstFileTemplate","f%Hms_")
dbpf("$(PREFIX)cam1:PrvHstFileFmt","4")    # jsonhisto
dbpf("$(PREFIX)cam1:PrvHstFileMode","1")   # tot
dbpf("$(PREFIX)cam1:PrvHstIntgMode","1")   # average
dbpf("$(PREFIX)cam1:PrvStpOnDskLim","0")   # false

#dbpf("$(PREFIX)cam1:Health.SCAN","I/O Intr")   # Do not scan

# save things every thirty seconds
create_monitor_set("auto_settings.req", 30, "P=$(PREFIX)")

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
dbLoadRecords("$(ADTIMEPIX)/db/Dashboard.template","P=$(PREFIX),R=cam1:,S=Stats5:,PORT=$(PORT),ADDR=0,TIMEOUT=1")

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

# Detector initialization: single source of truth (init_detector.cmd).
# Re-run from iocsh after reconnect: < init_detector.cmd
< init_detector.cmd

# save things every thirty seconds
create_monitor_set("auto_settings.req", 30, "P=$(PREFIX)")

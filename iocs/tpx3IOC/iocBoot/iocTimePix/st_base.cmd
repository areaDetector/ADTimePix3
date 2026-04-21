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
< load_chips.cmd
dbLoadRecords("$(ADTIMEPIX)/db/File.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
# Keep Server waveforms (ImgImageData/Frame/SumNFrames NELM) aligned with detector PixCount.
dbLoadRecords("$(ADTIMEPIX)/db/Server.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1,MAX_PIXELS=$(MASK_BPC_NELEMENTS)")
dbLoadRecords("$(ADTIMEPIX)/db/Measurement.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Dashboard.template","P=$(PREFIX),R=cam1:,S=Stats5:,PORT=$(PORT),ADDR=0,TIMEOUT=1")

# MaskBPC.template: NELEMENTS=$(MASK_BPC_NELEMENTS) — set in unique.cmd (table: 65536 / 262144 / 524288).
# Must be >= PixCount or mask DB load fails.
dbLoadRecords("$(ADTIMEPIX)/db/MaskBPC.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1,TYPE=Int32,FTVL=LONG,NELEMENTS=$(MASK_BPC_NELEMENTS)")

# VDD/AVDD rails: ADDR 0–2 = first SPIDR board (3 rails); ADDR 3–5 = second board when Health[1] present.
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr0,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr1,PORT=$(PORT),ADDR=1,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr2,PORT=$(PORT),ADDR=2,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr3,PORT=$(PORT),ADDR=3,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr4,PORT=$(PORT),ADDR=4,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr5,PORT=$(PORT),ADDR=5,TIMEOUT=1")

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

# Sum of last N frames
#NDFileTIFFConfigure("FileTIFFSum", 30, 0, "TPX3", 2)
#dbLoadRecords("NDFileTIFF.template", "P=TPX3-TEST:,R=TIFFSum:,PORT=FileTIFFSum,ADDR=0,TIMEOUT=1,NDARRAY_PORT=TPX3")
# Sum of N frames
#NDFileTIFFConfigure("FileTIFFSumN", 30, 0, "TPX3", 3)
#dbLoadRecords("NDFileTIFF.template", "P=TPX3-TEST:,R=TIFFSumN:,PORT=FileTIFFSumN,ADDR=0,TIMEOUT=1,NDARRAY_PORT=TPX3")

# HDF5 for running sum (ImgImageData on TPX3 addr 2)
# dbLoadRecords must be one line: iocsh does not join continued lines into the second argument.
#NDFileHDF5Configure("FileHDFSum", 30, 0, "TPX3", 2)
#dbLoadRecords("NDFileHDF5.template", "P=TPX3-TEST:,R=HDFSum:,PORT=FileHDFSum,ADDR=0,TIMEOUT=1,NDARRAY_PORT=TPX3,XMLSIZE=2048")

# HDF5 for sum-of-N frames (ImgImageSumNFrames on TPX3 addr 3)
#NDFileHDF5Configure("FileHDFSumN", 30, 0, "TPX3", 3)
#dbLoadRecords("NDFileHDF5.template", "P=TPX3-TEST:,R=HDFSumN:,PORT=FileHDFSumN,ADDR=0,TIMEOUT=1,NDARRAY_PORT=TPX3,XMLSIZE=2048")

set_requestfile_path("$(ADTIMEPIX)/tpx3App/Db")

iocInit()

# Detector initialization: single source of truth (init_detector.cmd).
# Re-run from iocsh after reconnect: < init_detector.cmd
< init_detector.cmd

# Set trace mask for the driver to show ERROR | FLOW messages
#asynSetTraceMask($(PORT),0,0x09)
#asynSetTraceMask($(PORT),0,0x11)

# Set trace mask for FileTIFF1 to show ERROR | FLOW messages
#asynSetTraceMask("FileTIFF1", 0, 0x9)   # ERROR | FLOW
#asynSetTraceIOMask("FileTIFF1", 0, 0x0)

# Enable ERROR | FLOW | DRIVERIO
#asynSetTraceMask("FileTIFF1", 0, 0xB)
#asynSetTraceIOMask("FileTIFF1", 0, 0x0)

# ERROR+FLOW+DRIVERIO:
#asynSetTraceMask("FileTIFF1", 0, 0x3)
#asynSetTraceIOMask("FileTIFF1", 0, 0x0)

# save things every thirty seconds
create_monitor_set("auto_settings.req", 30, "P=$(PREFIX)")

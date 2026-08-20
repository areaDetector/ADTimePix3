# Shared ADTimePix3 IOC core: driver, cam1 DB, chips, MaskBPC, preview Image1.
# Requires: unique.cmd loaded (PORT, PREFIX, SERVER_URL, MASK_BPC_NELEMENTS, …).
# Include from profiles/<family>/st.cmd after that profile's unique.cmd.

epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")
errlogInit(20000)

dbLoadDatabase("$(ADTIMEPIX)/iocs/tpx3IOC/dbd/tpx3App.dbd")
tpx3App_registerRecordDeviceDriver(pdbbase)

epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db:$(ADTIMEPIX)/db")

ADTimePixConfig("$(PORT)", "$(SERVER_URL)", 0, 0, 0, 0)
epicsThreadSleep(2)

asynSetTraceIOMask($(PORT), 0, 2)
#asynSetTraceMask($(PORT),0,0xff)

dbLoadRecords("$(ADTIMEPIX)/db/TimePix3Base.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/ADTimePix3.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
< load_chips.cmd
dbLoadRecords("$(ADTIMEPIX)/db/File.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
# Keep Server waveforms (ImgImageData/Frame/SumNFrames NELM) aligned with detector PixCount.
dbLoadRecords("$(ADTIMEPIX)/db/Server.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1,MAX_PIXELS=$(MASK_BPC_NELEMENTS)")
dbLoadRecords("$(ADTIMEPIX)/db/Measurement.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Dashboard.template","P=$(PREFIX),R=cam1:,S=Stats5:,PORT=$(PORT),ADDR=0,TIMEOUT=1")

# MaskBPC.template: NELEMENTS=$(MASK_BPC_NELEMENTS) — set in profile unique.cmd.
dbLoadRecords("$(ADTIMEPIX)/db/MaskBPC.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1,TYPE=Int32,FTVL=LONG,NELEMENTS=$(MASK_BPC_NELEMENTS)")

# VDD/AVDD rails: ADDR 0–2 = first SPIDR board (3 rails); ADDR 3–5 = second board when Health[1] present.
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr0,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr1,PORT=$(PORT),ADDR=1,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr2,PORT=$(PORT),ADDR=2,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr3,PORT=$(PORT),ADDR=3,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr4,PORT=$(PORT),ADDR=4,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr5,PORT=$(PORT),ADDR=5,TIMEOUT=1")

NDStdArraysConfigure("Image1", 3, 0, "$(PORT)", 0)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=20000000")

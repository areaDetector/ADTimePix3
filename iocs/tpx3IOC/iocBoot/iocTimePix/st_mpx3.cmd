#!../../bin/linux-x86_64/tpx3App

< envPaths

< unique_mpx3.cmd

epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")
errlogInit(20000)

dbLoadDatabase("$(ADTIMEPIX)/iocs/tpx3IOC/dbd/tpx3App.dbd")
tpx3App_registerRecordDeviceDriver(pdbbase)

epicsEnvSet("SERVER_URL", "http://localhost:8081")
epicsEnvSet("PREFIX", "MPX3-TEST:")
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db:$(ADTIMEPIX)/db")

ADTimePixConfig("$(PORT)", "$(SERVER_URL)", 0, 0, 0, 0)
epicsThreadSleep(2)

asynSetTraceIOMask($(PORT), 0, 2)

# Driver trace (uncomment for IOC console diagnostics during preview TCP / acquire):
# asynSetTraceMask($(PORT), 0, 8)    # ASYN_TRACEIO_DRIVER — "Processed PrvImg/PrvImg1 frame", jsonimage headers (PrvImgLogHeaders)
# asynSetTraceMask($(PORT), 0, 4)    # ASYN_TRACE_FLOW — acquire start/stop, port rotation
# asynSetTraceMask($(PORT), 0, 3)    # ERROR | WARNING — HTTP failures, trigger guards
# asynSetTraceMask($(PORT), 0, 255)  # all trace levels (verbose)

dbLoadRecords("$(ADTIMEPIX)/db/TimePix3Base.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/ADTimePix3.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
< load_chips.cmd
dbLoadRecords("$(ADTIMEPIX)/db/File.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Server.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1,MAX_PIXELS=$(MASK_BPC_NELEMENTS)")
dbLoadRecords("$(ADTIMEPIX)/db/Measurement.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/Dashboard.template","P=$(PREFIX),R=cam1:,S=Stats5:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/MaskBPC.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1,TYPE=Int32,FTVL=LONG,NELEMENTS=$(MASK_BPC_NELEMENTS)")

dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr0,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr1,PORT=$(PORT),ADDR=1,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr2,PORT=$(PORT),ADDR=2,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr3,PORT=$(PORT),ADDR=3,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr4,PORT=$(PORT),ADDR=4,TIMEOUT=1")
dbLoadRecords("$(ADTIMEPIX)/db/OperatingVoltage.template","P=$(PREFIX),R=cam1:,C=Pwr5,PORT=$(PORT),ADDR=5,TIMEOUT=1")

NDStdArraysConfigure("Image1", 3, 0, "$(PORT)", 0)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=20000000")

# MPX3 dual threshold: PrvImg thresholdID=1 -> NDArray address 8 on driver port
NDStdArraysConfigure("ImageTh1", 3, 0, "$(PORT)", 8)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=imageTh1:,PORT=ImageTh1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=20000000")

# MPX3 dual threshold: PVA for NDArray addr 8 (imageTh1 / threshold 1)
NDPvaConfigure("PVA2", $(QSIZE), 0, "$(PORT)", 8, "$(PREFIX)Pva2:Image", 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDPva.template", "P=$(PREFIX),R=Pva2:, PORT=PVA2,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

# IXS / dual-threshold band-pass on frame preview (TCP 8088): T0-T1 -> NDArray addr 9 (Pva5).
# Clip to max(0,T0-T1) when cam1:PrvImgThreshDiffClip=On (default); signed T0-T1 when Off.
NDStdArraysConfigure("ImageDiff1", 3, 0, "$(PORT)", 9)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=imageDiff1:,PORT=ImageDiff1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int32,FTVL=LONG,NELEMENTS=20000000")

NDPvaConfigure("PVA5", $(QSIZE), 0, "$(PORT)", 9, "$(PREFIX)Pva5:Image", 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDPva.template", "P=$(PREFIX),R=Pva5:, PORT=PVA5,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

# PrvImg1 integrated preview (TCP 8089): thresholdID=0 -> addr 10, thresholdID=1 -> addr 11
NDStdArraysConfigure("ImageInt1", 3, 0, "$(PORT)", 10)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=imageInt1:,PORT=ImageInt1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=20000000")

NDStdArraysConfigure("ImageIntTh1", 3, 0, "$(PORT)", 11)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=imageIntTh1:,PORT=ImageIntTh1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=20000000")

NDPvaConfigure("PVA3", $(QSIZE), 0, "$(PORT)", 10, "$(PREFIX)Pva3:Image", 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDPva.template", "P=$(PREFIX),R=Pva3:, PORT=PVA3,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

NDPvaConfigure("PVA4", $(QSIZE), 0, "$(PORT)", 11, "$(PREFIX)Pva4:Image", 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDPva.template", "P=$(PREFIX),R=Pva4:, PORT=PVA4,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

# Integrated band-pass (TCP 8089): T0-T1 -> NDArray addr 12 (Pva6)
NDStdArraysConfigure("ImageIntDiff1", 3, 0, "$(PORT)", 12)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=imageIntDiff1:,PORT=ImageIntDiff1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int32,FTVL=LONG,NELEMENTS=20000000")

NDPvaConfigure("PVA6", $(QSIZE), 0, "$(PORT)", 12, "$(PREFIX)Pva6:Image", 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDPva.template", "P=$(PREFIX),R=Pva6:, PORT=PVA6,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

# Full-rate Image[] demux (TCP 8086): thresholdID=0 -> addr 1, thresholdID=1 -> addr 13.
# Use for archive/HDF5 at detector rates; disable PVA callbacks for high-rate runs.
NDStdArraysConfigure("ImageImg1", 3, 0, "$(PORT)", 1)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=imageImg1:,PORT=ImageImg1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=20000000")

NDStdArraysConfigure("ImageImgTh1", 3, 0, "$(PORT)", 13)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=imageImgTh1:,PORT=ImageImgTh1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=20000000")

NDPvaConfigure("PVA7", $(QSIZE), 0, "$(PORT)", 1, "$(PREFIX)Pva7:Image", 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDPva.template", "P=$(PREFIX),R=Pva7:, PORT=PVA7,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

NDPvaConfigure("PVA8", $(QSIZE), 0, "$(PORT)", 13, "$(PREFIX)Pva8:Image", 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDPva.template", "P=$(PREFIX),R=Pva8:, PORT=PVA8,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

# HDF5 archive for Image[] demux (fixed NDArrayAddress at configure — preferred over runtime switch).
# Queue 100: larger than Preview PVA; raise further for high-Hz soak. Enable via init_detector_hdf5_img_mpx3.cmd.
NDFileHDF5Configure("FileHDFImgT0", 100, 0, "$(PORT)", 1)
dbLoadRecords("$(ADCORE)/db/NDFileHDF5.template", "P=$(PREFIX),R=HDFImgT0:,PORT=FileHDFImgT0,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),XMLSIZE=2048")

NDFileHDF5Configure("FileHDFImgT1", 100, 0, "$(PORT)", 13)
dbLoadRecords("$(ADCORE)/db/NDFileHDF5.template", "P=$(PREFIX),R=HDFImgT1:,PORT=FileHDFImgT1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),XMLSIZE=2048")

# ---------------------------------------------------------------------------
# Legacy IXS band-pass via NDPluginProcess (optional; driver addr 9/12 preferred):
# Uncomment plugin ports below AND init_ixs_thresh_diff.cmd at bottom (conflicts with driver Pva5/Pva6 wiring).
#
# dbLoadRecords("$(ADCORE)/db/NDProcess.template", "P=$(PREFIX),R=procFrameDiff:,PORT=FRMDIFF,ADDR=0,TIMEOUT=1,NDARRAY_PORT=Image1")
# dbLoadRecords("ixs_thresh_diff.template", "P=$(PREFIX)")
#
# NDProcessConfigure("INTDIFF", $(QSIZE), 0, "ImageInt1", 0, 0, 0)
# dbLoadRecords("$(ADCORE)/db/NDProcess.template", "P=$(PREFIX),R=procIntDiff:,PORT=INTDIFF,ADDR=0,TIMEOUT=1,NDARRAY_PORT=ImageInt1")
#
# NDStdArraysConfigure("ImageDiff1", 3, 0, "FRMDIFF", 0)
# dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=imageDiff1:,PORT=ImageDiff1,ADDR=0,NDARRAY_PORT=FRMDIFF,TIMEOUT=1,TYPE=Int32,FTVL=LONG,NELEMENTS=20000000")
# NDStdArraysConfigure("ImageIntDiff1", 3, 0, "INTDIFF", 0)
# dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=imageIntDiff1:,PORT=ImageIntDiff1,ADDR=0,NDARRAY_PORT=INTDIFF,TIMEOUT=1,TYPE=Int32,FTVL=LONG,NELEMENTS=20000000")
#
# NDPvaConfigure("PVA5", $(QSIZE), 0, "FRMDIFF", 0, "$(PREFIX)Pva5:Image", 0, 0, 0)
# dbLoadRecords("$(ADCORE)/db/NDPva.template", "P=$(PREFIX),R=Pva5:, PORT=PVA5,ADDR=0,TIMEOUT=1,NDARRAY_PORT=FRMDIFF")
# NDPvaConfigure("PVA6", $(QSIZE), 0, "INTDIFF", 0, "$(PREFIX)Pva6:Image", 0, 0, 0)
# dbLoadRecords("$(ADCORE)/db/NDPva.template", "P=$(PREFIX),R=Pva6:, PORT=PVA6,ADDR=0,TIMEOUT=1,NDARRAY_PORT=INTDIFF")
#
# Optional ADCompVision (build ADCOMPVISION into support, set RELEASE.local):
# NDCVConfigure("IXSCV1", $(QSIZE), 0, "$(PORT)", 0, 0, 0, 0, 0, $(MAX_THREADS=5))
# dbLoadRecords("$(ADCOMPVISION)/db/NDCV.template", "P=$(PREFIX),R=ixsCv1:,PORT=IXSCV1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")
# set_requestfile_path("$(ADCOMPVISION)/db")
# ---------------------------------------------------------------------------

< $(ADCORE)/iocBoot/commonPlugins.cmd
< autosave_mpx3.cmd

< $(ADCORE)/iocBoot/stats_profiles.cmd

set_requestfile_path("$(ADTIMEPIX)/tpx3App/Db")

iocInit()

< init_detector_mpx3.cmd

create_monitor_set("auto_settings_mpx3.req", 30, "P=$(PREFIX)")

# IXS T0-T1 band-pass plugins (uncomment plugin block in st_mpx3.cmd first):
# < init_ixs_thresh_diff.cmd

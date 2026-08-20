# Medipix3-only NDStdArrays / PVA / HDF5 plugins (dual threshold, IXS band-pass, Image[] demux).
# Include from profiles/mpx3/st.cmd after common/st_core.cmd.

# MPX3 dual threshold: PrvImg thresholdID=1 -> NDArray address 8 on driver port
NDStdArraysConfigure("ImageTh1", 3, 0, "$(PORT)", 8)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=imageTh1:,PORT=ImageTh1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=20000000")

NDPvaConfigure("PVA2", $(QSIZE), 0, "$(PORT)", 8, "$(PREFIX)Pva2:Image", 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDPva.template", "P=$(PREFIX),R=Pva2:, PORT=PVA2,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

# IXS / dual-threshold band-pass on frame preview (TCP 8088): T0-T1 -> NDArray addr 9 (Pva5).
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
NDStdArraysConfigure("ImageImg1", 3, 0, "$(PORT)", 1)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=imageImg1:,PORT=ImageImg1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=20000000")

NDStdArraysConfigure("ImageImgTh1", 3, 0, "$(PORT)", 13)
dbLoadRecords("$(ADCORE)/db/NDStdArrays.template", "P=$(PREFIX),R=imageImgTh1:,PORT=ImageImgTh1,ADDR=0,NDARRAY_PORT=$(PORT),TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=20000000")

NDPvaConfigure("PVA7", $(QSIZE), 0, "$(PORT)", 1, "$(PREFIX)Pva7:Image", 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDPva.template", "P=$(PREFIX),R=Pva7:, PORT=PVA7,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

NDPvaConfigure("PVA8", $(QSIZE), 0, "$(PORT)", 13, "$(PREFIX)Pva8:Image", 0, 0, 0)
dbLoadRecords("$(ADCORE)/db/NDPva.template", "P=$(PREFIX),R=Pva8:, PORT=PVA8,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

# HDF5 archive for Image[] demux. Enable via profiles/mpx3/init/hdf5_img.cmd.
NDFileHDF5Configure("FileHDFImgT0", 100, 0, "$(PORT)", 1)
dbLoadRecords("$(ADCORE)/db/NDFileHDF5.template", "P=$(PREFIX),R=HDFImgT0:,PORT=FileHDFImgT0,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),XMLSIZE=2048")

NDFileHDF5Configure("FileHDFImgT1", 100, 0, "$(PORT)", 13)
dbLoadRecords("$(ADCORE)/db/NDFileHDF5.template", "P=$(PREFIX),R=HDFImgT1:,PORT=FileHDFImgT1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),XMLSIZE=2048")

# ---------------------------------------------------------------------------
# Legacy IXS band-pass via NDPluginProcess (optional; driver addr 9/12 preferred):
# Uncomment plugin ports below AND profiles/mpx3/init/ixs_thresh_diff.cmd (conflicts with Pva5/Pva6).
#
# dbLoadRecords("$(ADCORE)/db/NDProcess.template", "P=$(PREFIX),R=procFrameDiff:,PORT=FRMDIFF,ADDR=0,TIMEOUT=1,NDARRAY_PORT=Image1")
# dbLoadRecords("profiles/mpx3/ixs_thresh_diff.template", "P=$(PREFIX)")
# ...

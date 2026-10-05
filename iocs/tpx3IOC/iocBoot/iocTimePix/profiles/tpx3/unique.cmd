#
# Unique file with all parameters that change in the IOC
#

# Set this to the folder for support.
# Two supported formats: 
# base and support in one directory (must edit envPaths)
# base inside support
epicsEnvSet("SUPPORT_DIR", "../../../../..")

# Maintainer
epicsEnvSet("ENGINEER",                 "K. Gofron")

# IOC Information
epicsEnvSet("PORT",                     "TPX3")
epicsEnvSet("IOC",                      "iocADTimePix")

epicsEnvSet("EPICS_CA_AUTO_ADDR_LIST",  "NO")
epicsEnvSet("EPICS_CA_ADDR_LIST",       "255.255.255.0")
epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES", "6000000")

# PV and IOC Name configs
epicsEnvSet("HOSTNAME",                 "localhost")
epicsEnvSet("IOCNAME",                  "tpx3")

# Imag and data size
epicsEnvSet("QSIZE",                    "30")
epicsEnvSet("NCHANS",                   "2048")
epicsEnvSet("HIST_SIZE",                "4096")
# Image / NDStats profile maximum dimensions. These are waveform capacities;
# smaller detector geometries publish their actual lengths through NORD.
epicsEnvSet("XSIZE",                    "1024")
epicsEnvSet("YSIZE",                    "512")
# Serval URL and PV prefix (override per site/beamline).
epicsEnvSet("SERVER_URL", "http://localhost:8081")
# Calibration path policy: permissive by default for portable/community deployments.
# This variable accepts exactly one absolute directory, not a comma-separated list.
# Restrictive root examples (choose one by replacing "/"; one subtree is supported):
#   "$(ADTIMEPIX)/vendor"          module-supplied calibration tree
#   "/opt/adtimepix/calibration"  site calibration tree
#   "/data/detectors/tpx3"        detector-specific calibration tree
epicsEnvSet("ADTIMEPIX_CALIBRATION_ROOT", "/")
# Destination policy: permit any supported file/TCP/HTTP base by default.
# Restrictive examples (replace the permissive list):
#   "file:/data/detector/*,tcp://listen@localhost:8088"
#   "tcp://connect@example.invalid:9000,http://example.invalid:8080/output"
epicsEnvSet("ADTIMEPIX_DESTINATION_ALLOWLIST", "file:/*,tcp://*,http://*")
epicsEnvSet("PREFIX", "TPX3-TEST:")

# --- Mask BPC waveform size (MaskBPC.template NELEMENTS) ---
# Must be >= detector PixCount (mask upload / compare / PixelConfigDiff). Too small: DB load fails, PVs invalid.
#
#   Chips   Typical mosaic   Image (px)   PixCount = NELEMENTS
#   -----   --------------   ----------   -----------------------
#     1     1 × 1            256 × 256            65536
#     4     2 × 2            512 × 512           262144
#     8     4 × 2            1024 × 512          524288
#
# Pick exactly one active line for your detector (comment the others):
#epicsEnvSet("MASK_BPC_NELEMENTS", "65536") # 1 chip 256×256
#epicsEnvSet("MASK_BPC_NELEMENTS", "262144") # 4 chips 512×512
epicsEnvSet("MASK_BPC_NELEMENTS", "524288") # 8 chips 1024×512
epicsEnvSet("NDTYPE",                   "Int16")  #'Int8' (8bit B/W, Color) | 'Int16' (16bit B/W)
epicsEnvSet("NDFTVL",                   "SHORT") #'UCHAR' (8bit B/W, Color) | 'SHORT' (16bit B/W)
epicsEnvSet("CBUFFS",                   "500")

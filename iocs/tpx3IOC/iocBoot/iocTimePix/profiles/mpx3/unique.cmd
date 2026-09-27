#
# Medipix3 IOC profile — unique parameters (see unique.cmd for Timepix3 defaults).
#

# Set this to the folder for support.
epicsEnvSet("SUPPORT_DIR", "../../../../..")

epicsEnvSet("ENGINEER",                 "K. Gofron")

# IOC Information
epicsEnvSet("PORT",                     "MPX3")
epicsEnvSet("IOC",                      "iocADTimePix")

epicsEnvSet("EPICS_CA_AUTO_ADDR_LIST",  "NO")
epicsEnvSet("EPICS_CA_ADDR_LIST",       "255.255.255.0")
epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES", "6000000")

epicsEnvSet("HOSTNAME",                 "localhost")
epicsEnvSet("IOCNAME",                  "mpx3")

epicsEnvSet("QSIZE",                    "30")
epicsEnvSet("NCHANS",                   "2048")
epicsEnvSet("HIST_SIZE",                "4096")
epicsEnvSet("XSIZE",                    "512")
epicsEnvSet("YSIZE",                    "512")
epicsEnvSet("NELMT",                    "262144")

# Medipix3 2x2 quad: 512×512, PixCount 262144
epicsEnvSet("MASK_BPC_NELEMENTS", "262144")
epicsEnvSet("NDTYPE",                   "Int16")
epicsEnvSet("NDFTVL",                   "SHORT")
epicsEnvSet("CBUFFS",                   "500")

epicsEnvSet("SERVER_URL", "http://localhost:8081")
# Calibration path policy: permissive by default for portable/community deployments.
# This variable accepts exactly one absolute directory, not a comma-separated list.
# Restrictive root examples (choose one by replacing "/"; one subtree is supported):
#   "$(ADTIMEPIX)/vendor"          module-supplied calibration tree
#   "/opt/adtimepix/calibration"  site calibration tree
#   "/data/detectors/mpx3"        detector-specific calibration tree
epicsEnvSet("ADTIMEPIX_CALIBRATION_ROOT", "/")
# Destination policy: permit any supported file/TCP/HTTP base by default.
# Restrictive examples (replace the permissive list):
#   "file:/data/detector/*,tcp://listen@localhost:8088"
#   "tcp://connect@example.invalid:9000,http://example.invalid:8080/output"
epicsEnvSet("ADTIMEPIX_DESTINATION_ALLOWLIST", "file:/*,tcp://*,http://*")
epicsEnvSet("PREFIX", "MPX3-TEST:")

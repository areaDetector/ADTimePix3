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

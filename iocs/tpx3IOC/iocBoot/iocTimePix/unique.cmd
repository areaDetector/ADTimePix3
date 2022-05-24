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
epicsEnvSet("XSIZE",                    "256")
epicsEnvSet("YSIZE",                    "256")
epicsEnvSet("NELMT",                    "65536")
epicsEnvSet("NDTYPE",                   "Int16")  #'Int8' (8bit B/W, Color) | 'Int16' (16bit B/W)
epicsEnvSet("NDFTVL",                   "SHORT") #'UCHAR' (8bit B/W, Color) | 'SHORT' (16bit B/W)
epicsEnvSet("CBUFFS",                   "500")

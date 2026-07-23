# stats_profiles.cmd — include after commonPlugins.cmd in areaDetector st.cmd
#
# Prerequisites (normally already set for areaDetector):
#   $(PREFIX)  IOC prefix, e.g. BL11A:cam1:
#   $(XSIZE)   max image width  (NDStats row profile length)
#   $(YSIZE)   max image height (NDStats column profile length)
#
# Set BOB_ADET to the bob ADet tree before sourcing this file, e.g.:
#   epicsEnvSet("BOB_ADET", "/epics/GUI/SNS/bob/ADet")
#
# $(R) defaults to Stats1: — change if profiles use another NDStats instance
#   (must match Phoebus macro ProfileStats on the camera screen).

epicsEnvSet("STATS_PROF_R", "Stats1:")
epicsEnvSet("BOB_ADET", "/epics/GUI/SNS/bob/ADet")
dbLoadRecords("$(BOB_ADET)/R3-11/common/db/NDStatsProfiles.db", "P=$(PREFIX), R=$(STATS_PROF_R), XNELM=$(XSIZE), YNELM=$(YSIZE)")

# After iocInit, enable profile computation (or dbpf manually once):
#   dbpf "$(PREFIX)StatsProfInit_.PROC" 1

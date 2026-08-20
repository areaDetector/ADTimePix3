#!../../bin/linux-x86_64/tpx3App

< envPaths

< profiles/mpx3/unique.cmd
< common/st_core.cmd
< profiles/mpx3/plugins_mpx3.cmd

# Driver trace (uncomment for IOC console diagnostics during preview TCP / acquire):
# asynSetTraceMask($(PORT), 0, 8)    # ASYN_TRACEIO_DRIVER
# asynSetTraceMask($(PORT), 0, 4)    # ASYN_TRACE_FLOW
# asynSetTraceMask($(PORT), 0, 3)    # ERROR | WARNING
# asynSetTraceMask($(PORT), 0, 255)  # all trace levels (verbose)

< $(ADCORE)/iocBoot/commonPlugins.cmd
< profiles/mpx3/autosave.cmd

< $(ADCORE)/iocBoot/stats_profiles.cmd

set_requestfile_path("$(ADTIMEPIX)/tpx3App/Db")

iocInit()

< profiles/mpx3/init/detector.cmd

set_requestfile_path("profiles/mpx3")
create_monitor_set("auto_settings.req", 30, "P=$(PREFIX)")

# Legacy IXS NDPluginProcess band-pass (uncomment plugins_mpx3 legacy block first):
# < profiles/mpx3/init/ixs_thresh_diff.cmd

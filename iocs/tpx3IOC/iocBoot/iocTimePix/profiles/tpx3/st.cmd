#!../../bin/linux-x86_64/tpx3App

< envPaths

< profiles/tpx3/unique.cmd
< common/st_core.cmd

< $(ADCORE)/iocBoot/commonPlugins.cmd
< profiles/tpx3/autosave.cmd

# Phoebus row/column profile axes (NDStatsProfiles Cal:*.AVAL).
# Requires synApps calc; StatsProfInit_ in profiles/tpx3/init/hw.cmd.
< $(ADCORE)/iocBoot/stats_profiles.cmd

set_requestfile_path("$(ADTIMEPIX)/tpx3App/Db")

iocInit()

# Detector init: profiles/tpx3/init/detector.cmd (paths + hw + RefreshConnection).
< profiles/tpx3/init/detector.cmd

# Basename only: save_restore derives .sav name from req path (must stay under ./autosave/tpx3/).
set_requestfile_path("profiles/tpx3")
create_monitor_set("auto_settings.req", 30, "P=$(PREFIX)")

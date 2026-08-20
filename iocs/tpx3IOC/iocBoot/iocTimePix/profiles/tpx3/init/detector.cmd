# Detector initialization: single source of truth for channel/config PVs.
# Sourced from profiles/tpx3/st.cmd after iocInit(); safe to re-run from iocsh.
#
# Two-phase layout (reduces 409/log spam when SERVAL is up but detector is off):
#   1) init/paths.cmd — TCP paths, templates, BPC/DACS *paths* only (no SERVAL push).
#   2) init/hw.cmd    — WriteData and mode PVs that call fileWriter/sendConfiguration.
#
# Offline detector at boot: in profiles/tpx3/st.cmd, use only:
#     < profiles/tpx3/init/paths.cmd
# When hardware is ready:
#     dbpf $(PREFIX)cam1:RefreshConnection 1
#     < profiles/tpx3/init/hw.cmd
#
# Site overlays: profiles/tpx3/init/paths_site.cmd, init/hw_site.cmd (gitignored or local).

< profiles/tpx3/init/paths.cmd
< profiles/tpx3/init/hw.cmd

dbpf("$(PREFIX)cam1:RefreshConnection","1")

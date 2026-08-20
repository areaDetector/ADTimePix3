# Medipix3 detector initialization (paths + hardware push).
# Full-rate Image TCP is configured in paths (8086) but WriteImg=0.
# To enable Image[] after startup: < profiles/mpx3/init/img.cmd

< profiles/mpx3/init/paths.cmd
< profiles/mpx3/init/hw.cmd

dbpf("$(PREFIX)cam1:RefreshConnection","1")

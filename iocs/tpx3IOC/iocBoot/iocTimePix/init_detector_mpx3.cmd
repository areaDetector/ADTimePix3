# Medipix3 detector initialization (paths + hardware push).
# Full-rate Image TCP is configured in paths (8086) but WriteImg=0.
# To enable Image[] after startup: < init_detector_img_mpx3.cmd
< init_detector_paths_mpx3.cmd
< init_detector_hw_mpx3.cmd

# Driver connects before iocInit; Measurement mbbi defaults to 0 until first sync.
dbpf("$(PREFIX)cam1:RefreshConnection","1")

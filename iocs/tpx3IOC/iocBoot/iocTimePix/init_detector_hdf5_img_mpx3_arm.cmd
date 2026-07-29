# Arm HDF5 Stream Capture after plugins have latched dimensions (≥1 Image frame).
# Run after init_detector_hdf5_img_mpx3.cmd and one Acquire that delivered Img frames.
#
#   < init_detector_hdf5_img_mpx3_arm.cmd
#   dbpf("$(PREFIX)cam1:Acquire","1")

dbpf("$(PREFIX)HDFImgT0:Capture","1")
dbpf("$(PREFIX)HDFImgT1:Capture","1")

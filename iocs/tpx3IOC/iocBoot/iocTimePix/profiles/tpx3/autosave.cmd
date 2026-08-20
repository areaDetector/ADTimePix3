# Timepix3 autosave (PREFIX=TPX3-TEST:). Include after commonPlugins.cmd, before iocInit.
# .sav files store literal PV names; do not share ./autosave with Medipix3 (MPX3-TEST:).
# save_restore does not mkdir; st.cmd creates profile dirs under ./autosave/.
set_savefile_path("./autosave/tpx3")
set_pass0_restoreFile("auto_settings.sav")
set_pass1_restoreFile("auto_settings.sav")

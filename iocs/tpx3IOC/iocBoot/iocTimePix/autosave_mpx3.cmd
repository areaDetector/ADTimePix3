# Medipix3 autosave (PREFIX=MPX3-TEST:). Include after commonPlugins.cmd, before iocInit.
# .sav files store literal PV names; do not share ./autosave with Timepix3 (TPX3-TEST:).
# save_restore does not mkdir; st.cmd creates profile dirs for Timepix3 startup.
set_savefile_path("./autosave/mpx3")
set_pass0_restoreFile("auto_settings.sav")
set_pass1_restoreFile("auto_settings.sav")

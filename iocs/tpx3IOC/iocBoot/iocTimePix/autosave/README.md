# Autosave directories

Detector profiles share this boot folder but use **separate** autosave subdirectories under `./autosave/`:

| Profile   | Launcher     | Save path          | Autosave cmd              | Monitor set                         |
|-----------|--------------|--------------------|---------------------------|-------------------------------------|
| Timepix3  | `./st.cmd`   | `./autosave/tpx3/` | `profiles/tpx3/autosave.cmd` | `profiles/tpx3/auto_settings.req` |
| Medipix3  | `./st_mpx3.cmd` | `./autosave/mpx3/` | `profiles/mpx3/autosave.cmd` | `profiles/mpx3/auto_settings.req` |

`profiles/mpx3/auto_settings.req` adds `imageTh1:` and `Pva2:` (dual-threshold plugins). Timepix3 omits those PVs.

Legacy `profiles/tpx3/auto_settings_legacy.req` matches the old flat `auto_settings.req` name.

EPICS save_restore does **not** create directories. Launchers run `mkdir -p autosave/tpx3 autosave/mpx3 autosave/tpx4`.

On first run with an empty profile folder, `iocInit` may log “Can't open save file” once; after `create_monitor_set`, `auto_settings.sav` is written under the profile subdirectory.

See also `profiles/README.md` for the full boot layout.

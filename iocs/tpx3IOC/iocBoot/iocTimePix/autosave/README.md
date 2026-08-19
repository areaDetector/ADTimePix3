# Autosave directories

Timepix3 (`st.cmd` / `st_base.cmd`, `PREFIX=TPX3-TEST:`) and Medipix3 (`st_mpx3.cmd`, `PREFIX=MPX3-TEST:`) share this IOC boot folder but use **separate** autosave subdirectories:

| Profile   | Startup script | Save path          | Override cmd        | Monitor set              |
|-----------|----------------|--------------------|---------------------|--------------------------|
| Timepix3  | `st.cmd`       | `./autosave/tpx3/` | `autosave_tpx3.cmd` | `auto_settings_tpx3.req` |
| Medipix3  | `st_mpx3.cmd`  | `./autosave/mpx3/` | `autosave_mpx3.cmd` | `auto_settings_mpx3.req` |

`auto_settings_mpx3.req` adds `imageTh1:` and `Pva2:` (dual-threshold preview plugins loaded only in `st_mpx3.cmd`). Timepix3 uses `auto_settings_tpx3.req` without those PVs so `create_monitor_set` does not log connect failures.

Legacy `auto_settings.req` matches the Timepix3 profile; prefer the profile-specific files above.

EPICS save_restore does **not** create directories. Runtime `.sav` files and profile subdirs are gitignored.

**Directory creation:**

- **`st.cmd`** (Timepix3) runs `mkdir -p autosave/tpx3 autosave/mpx3` before the IOC starts.
- **`st_mpx3.cmd`** does not mkdir (iocsh has no portable `system()`). On a fresh clone, run once from this boot directory:

  ```bash
  mkdir -p autosave/mpx3 autosave/tpx3
  ```

  Or start Timepix3 via `./st.cmd` first — that creates both profile dirs. After that, `./st_mpx3.cmd` works and autosave persists locally.

On first run with an empty profile folder, `iocInit` may log “Can't open save file” once; that is normal. After `create_monitor_set` runs, `auto_settings.sav` is written under the profile subdirectory.

Legacy saves from a flat `./autosave/` layout can be copied into `tpx3/` or `mpx3/` by prefix. Runtime backups (`auto_settings.sav_*`) stay in the profile subdirectory once the IOC has run with the new layout.

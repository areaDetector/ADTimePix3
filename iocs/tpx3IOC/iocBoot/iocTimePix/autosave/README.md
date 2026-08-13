# Autosave directories

Timepix3 (`st.cmd` / `st_base.cmd`, `PREFIX=TPX3-TEST:`) and Medipix3 (`st_mpx3.cmd`, `PREFIX=MPX3-TEST:`) share this IOC boot folder but use **separate** autosave subdirectories:

| Profile   | Startup script | Save path          | Override cmd        |
|-----------|----------------|--------------------|---------------------|
| Timepix3  | `st.cmd`       | `./autosave/tpx3/` | `autosave_tpx3.cmd` |
| Medipix3  | `st_mpx3.cmd`  | `./autosave/mpx3/` | `autosave_mpx3.cmd` |

Autosave `.sav` files store **full PV names** (for example `MPX3-TEST:cam1:BinX`), not `$(PREFIX)` macros. If both profiles restore from the same file, `iocInit` logs thousands of `dbFindRecord ... failed` lines for the wrong prefix.

The shared `auto_settings.req` monitor set is fine for both profiles; it expands `P=$(PREFIX)` when `create_monitor_set` runs after startup.

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

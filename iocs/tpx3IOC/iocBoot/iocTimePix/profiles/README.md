# Detector family profiles (Option C layout)

One IOC app (`iocTimePix`) shares driver code; each **profile** is a self-contained startup package.

## Layout

```
iocBoot/iocTimePix/
  st.cmd                 → profiles/tpx3/st.cmd
  st_mpx3.cmd            → profiles/mpx3/st.cmd
  envPaths, load_chips.cmd
  common/
    st_core.cmd          # driver + cam1 DB + Image1 (all families)
  templates/
    hdf5/                # NDFileHDF5 hdf5_layout XML
    nexus/               # NDFileNexus NXroot templates (legacy; see templates/README.md)
  profiles/
    tpx3/                # Timepix3
    mpx3/                # Medipix3
    tpx4/                # (future) copy tpx3 skeleton + edit deltas
  autosave/
    tpx3/  mpx3/  tpx4/
```

Calibration files: `vendor/tpx3/`, `vendor/mpx3/`, `vendor/tpx4/` (see `vendor/README.md`).

Phoebus screens mirror this layout under `tpx3App/op/bob/` (see `tpx3App/op/bob/README.md`).

## Profile contract

Each `profiles/<family>/st.cmd` follows the same sequence:

1. `< envPaths`
2. `< profiles/<family>/unique.cmd` — PORT, PREFIX, SERVER_URL, mosaic, MASK_BPC_NELEMENTS
3. `< common/st_core.cmd`
4. `< profiles/<family>/plugins_*.cmd` — optional (MPX3 only today)
5. `< commonPlugins.cmd`, `< autosave.cmd`, `< stats_profiles.cmd`
6. `iocInit()`
7. `< init/detector.cmd`, then `set_requestfile_path("profiles/<family>")` and `create_monitor_set("auto_settings.req", …)` — **basename only** for the req file so `.sav` lands in `./autosave/<family>/`, not a nested `profiles/…` path.

## Init scripts (re-runnable from iocsh)

| File | Purpose |
|------|---------|
| `init/paths.cmd` | TCP paths, templates, BPC/DACS file paths (no SERVAL push) |
| `init/hw.cmd` | WriteData, BPC/DACS upload, TriggerMode, plugins |
| `init/detector.cmd` | paths + hw + RefreshConnection |

MPX3 optional: `init/img.cmd`, `init/hdf5_img.cmd`, `init/hdf5_img_arm.cmd`, `init/hw_equalize.cmd`.

## Adding TPX4

```bash
cp -r profiles/tpx3 profiles/tpx4
# Edit profiles/tpx4/unique.cmd (PORT, PREFIX, mosaic)
# Add profiles/tpx4/plugins_tpx4.cmd if needed
# Add st_tpx4.cmd launcher at boot root
mkdir -p autosave/tpx4
```

## Site overlays

Keep beamline-specific edits in gitignored files, e.g. `profiles/tpx3/init/paths_site.cmd`, and source them from a local wrapper — do not fork the whole profile in git.

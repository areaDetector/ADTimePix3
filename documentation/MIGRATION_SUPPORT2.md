# Developing ADTimePix3 in support2 with ADCore 3.14.0

When moving development to the **support2** tree with **ADCore 3.14.0**, use this as a tracking and migration checklist.

## Target environment

- **ADTimePix3:** `/epics/support2/areaDetector/ADTimePix3`
- **ADCore:** `/epics/support2/areaDetector/ADCore` (R3-14 / 3.14.0 or newer)
- **IOC startup:** `st.cmd` (which runs `st_base.cmd`)

## What does NOT need to be moved

1. **ADCore patches (SIGSEGV on exit)**  
   The fix is already in **ADCore 3.14.0**. Do **not** copy or re-apply the older support-tree ADCore changes (maxAddr_, nulling pArrays, registerDestroyingPool, NDArray::release check) into support2’s ADCore.

2. **support-tree ADCore**  
   `/epics/support/areaDetector/ADCore` was patched for older releases. With support2 you use unpatched ADCore 3.14.0. No need to track or move those patches.

3. **Autosave and runtime files**  
   `iocBoot/iocTimePix/autosave/*.sav` and similar are runtime-specific. Re-create or copy only if you need the same PV values; they are not part of the codebase to “migrate”.

4. **RELEASE paths**  
   Each tree has its own `configure/RELEASE` (and optional `RELEASE.local` / `RELEASE_LIBS_INCLUDE`). support2 should point to support2’s ADCore and dependencies. No need to copy support’s RELEASE files; just ensure support2’s RELEASE points to the support2 install you use.

## What to treat as single source of truth (support2)

- Do all **new development** in **support2** (driver, DB, screens, docs, IOC scripts).
- **Driver and app:** `tpx3App/src/`, `tpx3App/Db/`, `tpx3App/op/`, etc., in support2.
- **IOC scripts:** `iocs/tpx3IOC/iocBoot/iocTimePix/` (e.g. `st.cmd`, `st_base.cmd`, `init_detector.cmd`) in support2.
- **Documentation:** `README.md`, `RELEASE.md`, `documentation/*.md` in support2.

If you need to backport a fix to the **support** tree later, do it by cherry-pick or selective copy from support2, not the other way around.

## Features already present in both trees (no extra migration)

Both support and support2 ADTimePix3 already contain:

- **Connection management:** `checkConnection()`, `RefreshConnection` PV, connection poll thread, reconnect behavior (`fileWriter()` + `initAcquisition()` + `getServer()`).
- **Detector init:** `init_detector.cmd`, `ApplyConfig` PV, EPICS PVs as source of truth.
- **Destructor order:** Callback thread and connection poll stopped first; no `disconnect(pasynUserSelf)` in destructor.
- **Docs:** Same `documentation/` set, including `SIGSEGV_ON_EXIT.md` (noting ADCore 3.14.0+).

So you do **not** need to “move” these from support to support2; they are already there. Just ensure you are building and running from support2.

## Optional: what to copy only if you customized it

- **RELEASE.local / configure/RELEASE.local** – If you had support-specific paths or libs, replicate the same logic in support2’s `RELEASE.local` (with support2 paths).
- **Site-specific scripts** – Any custom `.cmd` or wrapper scripts you added under support’s IOC boot directory: copy or re-create under support2’s `iocs/tpx3IOC/iocBoot/iocTimePix/` if you still need them.
- **Autosave request files** – If you use custom `.req` or autosave request lists, ensure equivalent files exist in support2’s IOC boot or autosave area.

## Quick checklist

- [ ] support2 ADTimePix3 builds with support2 ADCore 3.14.0 (no ADCore patches applied).
- [ ] `st.cmd` (and thus `st_base.cmd`) runs and exits cleanly (no SIGSEGV on `exit`).
- [ ] RELEASE (and RELEASE.local if any) in support2 point to the intended support2 modules.
- [ ] All new changes are made in support2; support tree is only for legacy or backport if needed.
- [ ] Documentation in support2 is updated when you add or change features.

# Developing ADTimePix3 in support2 with ADCore 3.14.0

When moving development to the **support2** tree with **ADCore 3.14.0**, use this as a tracking and migration checklist.

## Target environment

- **ADTimePix3:** `/epics/support2/areaDetector/ADTimePix3`
- **ADCore:** `/epics/support2/areaDetector/ADCore` (R3-14 / 3.14.0 or newer)
- **IOC startup:** `st.cmd` (which runs `profiles/tpx3/st.cmd`)

## What does NOT need to be moved

1. **ADCore shutdown fix (SIGSEGV on exit)**
   Do not assume the ADCore version number alone includes the destroyed-pool shutdown fix. The unpatched R3-14 tree used with this support2 build does not contain the `registerDestroyingPool` / `isPoolDestroyed` implementation described in [SIGSEGV_ON_EXIT.md](SIGSEGV_ON_EXIT.md). Verify the actual ADCore source and perform an IOC exit test after acquisition; apply the reviewed ADCore fix if it is absent and the failure is reproducible.

2. **support-tree ADCore**
   Do not copy the entire patched legacy support-tree ADCore into support2. Point the release configuration at the intended support2 ADCore, then carry only a reviewed shutdown fix when the selected ADCore source does not already provide equivalent protection.

3. **Autosave and runtime files**
   `iocBoot/iocTimePix/autosave/*.sav` and similar are runtime-specific. Re-create or copy only if you need the same PV values; they are not part of the codebase to “migrate”.

4. **RELEASE paths**
   Each tree has its own `configure/RELEASE` (and optional `RELEASE.local` / `RELEASE_LIBS_INCLUDE`). support2 should point to support2’s ADCore and dependencies. No need to copy support’s RELEASE files; just ensure support2’s RELEASE points to the support2 install you use.

## What to treat as single source of truth (support2)

- Do all **new development** in **support2** (driver, DB, screens, docs, IOC scripts).
- **Driver and app:** `tpx3App/src/`, `tpx3App/Db/`, `tpx3App/op/`, etc., in support2.
- **IOC scripts:** `iocs/tpx3IOC/iocBoot/iocTimePix/` (e.g. `st.cmd`, `profiles/tpx3/st.cmd`, `profiles/tpx3/init/detector.cmd`) in support2.
- **Documentation:** `README.md`, `RELEASE.md`, `documentation/*.md` in support2.
- **Eight-chip / dual SPIDR:** See [documentation/8chip-migration.md](8chip-migration.md) (IOC `load_chips.cmd`, `MASK_BPC_NELEMENTS` in `unique.cmd`, driver health/boards, mask indexing). Ensure `unique.cmd` defines `MASK_BPC_NELEMENTS` so `MaskBPC.template` loads.

If you need to backport a fix to the **support** tree later, do it by cherry-pick or selective copy from support2, not the other way around.

## Features already present in both trees (no extra migration)

Both support and support2 ADTimePix3 already contain:

- **Connection management:** `checkConnection()`, `RefreshConnection` PV, and the connection poll thread. Automatic reconnect is read-only; use `ApplyConfig` or `WriteData` explicitly to restore a destination after Serval restart.
- **Detector init:** `profiles/tpx3/init/detector.cmd`, `ApplyConfig` PV, EPICS PVs as source of truth.
- **Destructor order:** Callback thread and connection poll stopped first; no `disconnect(pasynUserSelf)` in destructor.
- **Docs:** Same `documentation/` set, including `SIGSEGV_ON_EXIT.md`; verify the selected ADCore source rather than inferring shutdown-fix presence from its version number.

So you do **not** need to “move” these from support to support2; they are already there. Just ensure you are building and running from support2.

## Optional: what to copy only if you customized it

- **RELEASE.local / configure/RELEASE.local** – If you had support-specific paths or libs, replicate the same logic in support2’s `RELEASE.local` (with support2 paths).
- **Site-specific scripts** – Any custom `.cmd` or wrapper scripts you added under support’s IOC boot directory: copy or re-create under support2’s `iocs/tpx3IOC/iocBoot/iocTimePix/` if you still need them.
- **Autosave request files** – If you use custom `.req` or autosave request lists, ensure equivalent files exist in support2’s IOC boot or autosave area.

## Quick checklist

- [ ] support2 ADTimePix3 builds with the selected support2 ADCore; verify whether the shutdown fix is present and apply only the reviewed fix if required.
- [ ] `st.cmd` (and thus `profiles/tpx3/st.cmd`) runs and exits cleanly (no SIGSEGV on `exit`).
- [ ] `unique.cmd` sets **`MASK_BPC_NELEMENTS`** (65536 / 262144 / 524288) so `MaskBPC.template` loads; mask PVs connect.
- [ ] RELEASE (and RELEASE.local if any) in support2 point to the intended support2 modules.
- [ ] All new changes are made in support2; support tree is only for legacy or backport if needed.
- [ ] Documentation in support2 is updated when you add or change features.

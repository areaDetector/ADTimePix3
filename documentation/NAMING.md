# ADServal naming and legacy ADTimePix3

## Product vs module name

| Name | Meaning | Status |
|------|---------|--------|
| **ADServal** | Unified EPICS areaDetector driver for ASI pixel detectors on **Serval** (TimePix3, Medipix3; TimePix4 planned) | **Preferred** in release notes and new documentation |
| **ADTimePix3** | GitHub repository, EPICS support module directory, `$(ADTIMEPIX)` install path, library `libADTimePix`, C++ class `ADTimePix`, version macros `ADTIMEPIX_*` | **Legacy**; unchanged in builds and IOCs until a planned rename |

**R1-7-0** merged Medipix3 into the same driver binary. Documentation now describes that work under **ADServal**; operators still install and build **`ADTimePix3`** as today.

## What does not change (yet)

- `make` / `configure/RELEASE`: module name and `ADTIMEPIX` environment variable
- IOC macros: `$(ADTIMEPIX)`, `tpx3App/`, `tpx3Support/`
- EPICS records and PV layout (no rename for compatibility)
- C++ identifiers (`ADTimePix`, `ADTimePix3ServalHttp`, …)

Sites should keep existing startup scripts and support-module paths. Use **`st_mpx3.cmd`** only for Medipix3; **`profiles/tpx3/st.cmd`** / **`st.cmd`** for TimePix3.

## Planned migration

1. **Documentation** (current step): ADServal product name; ADTimePix3 as legacy module/repo label.
2. **Future release**: optional `ADSERVAL` env / module alias; thin **ADTimePix3** compatibility wrapper pointing at the same install tree.
3. **Later**: GitHub repository rename to `ADServal` with redirect; update DOE CODE citation when ready.

Details of each release: [RELEASE.md](../RELEASE.md).

## Other repositories (not this driver)

| Repository | Role |
|------------|------|
| [areaDetector/ADTimePix3](https://github.com/areaDetector/ADTimePix3) | **Active** unified driver (ADServal); shipped as ADTimePix3 |
| [areaDetector/ADTimePix](https://github.com/areaDetector/ADTimePix) | Unused placeholder (initial commit only, ~2018); **not** related to Serval |
| [kgofron/ADMediPix3](https://github.com/kgofron/ADMediPix3) | Early Medipix3 planning notes; integration landed in areaDetector/ADTimePix3 **R1-7-0** |

Earlier community attempts at a TimePix EPICS driver predated the current Serval-based code in **ADTimePix3**.

## Medipix3 integration history

Medipix3 support was developed on a feature branch and merged for **R1-7-0** (August 2026, driver **1.7.0**):

- **Upstream merge**: [areaDetector/ADTimePix3 PR #15](https://github.com/areaDetector/ADTimePix3/pull/15) (from [kgofron/ADTimePix3](https://github.com/kgofron/ADTimePix3), tag **R1-7-0**).
- **Development branch**: `medipix3-integration` on [kgofron/ADTimePix3](https://github.com/kgofron/ADTimePix3) (merged; branch removed after merge).
- **Local backup tree** (developer machine): `/epics/support2/areaDetector/ADTimePix3_mpx3.bkp` — snapshot of the integration work before/at merge; not part of the official release tree. Use **areaDetector/ADTimePix3** `master` and tag **R1-7-0** as the reference.

Operator and developer guide: [medipix3/integration.md](medipix3/integration.md).

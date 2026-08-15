# Medipix3 documentation

**ADServal** (unified driver; module **ADTimePix3**) Medipix3 (MPX3) support — merged **R1-7-0**, August 2026. Early planning: [ADMediPix3](https://github.com/kgofron/ADMediPix3). Integration history: [../NAMING.md](../NAMING.md).

Validated on the Medipix3 emulator and in a **first ASI hardware preview** (July 2026, via [ad-timepix3-deploy](https://github.com/kgofron/ad-timepix3-deploy)): real detector connection, dual-threshold previews after lowering chip-0 TH0/TH1 in `.dacs`. Equalization and dual-counter IXS band-pass on hardware are still to follow — see [integration.md](integration.md) § ASI hardware checkout. **Mask / BPC diff is not yet correct for MPX3** — see [integration.md](integration.md) § Open work (TODO).

| Document | Description |
|----------|-------------|
| [integration.md](integration.md) | IOC profile (`st_mpx3.cmd`), emulator workflow, **ASI hardware checkout**, calibration, Preview + **Image mode (8086)**, HDF5 soak, family TCP map, troubleshooting |
| [preview-dual-threshold.md](preview-dual-threshold.md) | ASI vendor notes, dual-threshold delivery, open questions, implementation plan |
| [screenshots/](screenshots/) | Phoebus OPI captures for integration docs |
| [COORDINATE_MAP.md](../COORDINATE_MAP.md) | Image `(i,j)` / BPC indexing; **top-left Y-origin** vs Phoebus / NDStats profiles |

**Local-only (gitignored):** [drafts/](drafts/) — saved Serval OpenAPI snapshots (`serval-openapi-<version>-build<N>.yaml`), email drafts, debug JSON. How to query live OpenAPI: [integration.md § Serval API reference](integration.md#serval-api-reference-openapi).

Shared driver topics (TCP streaming, NDArray addresses, masks) live in the [documentation index](../README.md).

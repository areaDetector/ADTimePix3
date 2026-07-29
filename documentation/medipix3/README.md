# Medipix3 documentation

Unified **ADTimePix3** driver support for Medipix3 (MPX3) on branch `medipix3-integration`. Planning and scope: [ADMediPix3](https://github.com/kgofron/ADMediPix3).

Validated on the Medipix3 emulator and in a **first ASI hardware preview** (July 2026, via [ad-timepix3-deploy](https://github.com/kgofron/ad-timepix3-deploy)): real detector connection, dual-threshold previews after lowering chip-0 TH0/TH1 in `.dacs`. Equalization and dual-counter IXS band-pass on hardware are still to follow — see [integration.md](integration.md) § ASI hardware checkout.

| Document | Description |
|----------|-------------|
| [integration.md](integration.md) | IOC profile (`st_mpx3.cmd`), emulator workflow, **ASI hardware checkout**, calibration, Preview + **Image mode (8086)**, family TCP map, troubleshooting |
| [preview-dual-threshold.md](preview-dual-threshold.md) | ASI vendor notes, dual-threshold delivery, open questions, implementation plan |
| [COORDINATE_MAP.md](../COORDINATE_MAP.md) | Image `(i,j)` / BPC indexing; **top-left Y-origin** vs Phoebus / NDStats profiles |

Shared driver topics (TCP streaming, NDArray addresses, masks) live in the [documentation index](../README.md).

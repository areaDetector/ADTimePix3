# Medipix3 documentation

**ADServal** (unified driver; module **ADTimePix3**) Medipix3 (MPX3) support — merged **R1-7-0**, August 2026. Early planning: [ADMediPix3](https://github.com/kgofron/ADMediPix3). Integration history: [../NAMING.md](../NAMING.md).

Validated on the Medipix3 emulator and in a **first ASI hardware preview** (July 2026, via [ad-timepix3-deploy](https://github.com/kgofron/ad-timepix3-deploy)): real detector connection, dual-threshold previews after lowering chip-0 TH0/TH1 in `.dacs`. The emulator validates the complete **131072-byte-per-chip** PixelConfig as big-endian 16-bit pixel words; a controlled bit-0 mask suppressed exactly the selected pixels in both counters. ASI subsequently confirmed that setting/clearing bit 0 masks/unmasks both counters while other word bits must be preserved, enabling family-aware MPX3 mask read/write/count/export. Physical post-equalization comparison, equalization, and dual-counter IXS band-pass validation are still to follow — see [integration.md](integration.md) § Open work (TODO).

ASI-provided corresponding BPC/Serval example data are available under
[`test/fixtures/mpx3/eq-02/`](../../test/fixtures/mpx3/eq-02/). ASI permits
redistribution as example/test configuration; the files are not default or
portable detector calibration. See the fixture README for hashes, provenance,
and license information.

| Document | Description |
|----------|-------------|
| [integration.md](integration.md) | IOC profile (`st_mpx3.cmd`), **operator reference** (PV map, TriggerMode, PipelineState, incompatibilities), emulator workflow, **ASI hardware checkout**, calibration, Preview + **Image mode (8086)**, HDF5 soak, family TCP map, troubleshooting |
| [preview-dual-threshold.md](preview-dual-threshold.md) | ASI vendor notes, dual-threshold delivery, open questions, implementation plan |
| [screenshots/](screenshots/) | Phoebus OPI captures for integration docs |
| [COORDINATE_MAP.md](../COORDINATE_MAP.md) | Image `(i,j)` / BPC indexing; **top-left Y-origin** vs Phoebus / NDStats profiles |

**Local-only (gitignored):** [drafts/](drafts/) — saved Serval OpenAPI snapshots (`serval-openapi-<version>-build<N>.yaml`), email drafts, debug JSON. How to query live OpenAPI: [integration.md § Serval API reference](integration.md#serval-api-reference-openapi).

Shared driver topics (TCP streaming, NDArray addresses, masks) live in the [documentation index](../README.md).

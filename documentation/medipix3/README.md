# Medipix3 documentation

Unified **ADTimePix3** driver support for Medipix3 (MPX3) on branch `medipix3-integration`. Planning and scope: [ADMediPix3](https://github.com/kgofron/ADMediPix3).

Validated on the Medipix3 emulator; physical hardware checkout is pending — see [integration.md](integration.md) § ASI hardware checkout.

| Document | Description |
|----------|-------------|
| [integration.md](integration.md) | IOC profile (`st_mpx3.cmd`), emulator workflow, **ASI hardware checkout**, calibration, preview TCP, troubleshooting |
| [preview-dual-threshold.md](preview-dual-threshold.md) | ASI vendor notes, dual-threshold delivery, open questions, implementation plan |

Shared driver topics (TCP streaming, NDArray addresses, masks) live in the [documentation index](../README.md).

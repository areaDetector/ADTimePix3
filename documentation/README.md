# ADServal documentation index

Technical notes for the unified **ADServal** areaDetector module (shipped as **ADTimePix3**): **TimePix3** and **Medipix3** on Serval; **TimePix4** planned. Naming: [NAMING.md](NAMING.md). Sphinx release docs and screenshots: [`docs/ADTimePix3/`](../docs/ADTimePix3/).

## Medipix3

Merged in **R1-7-0** (August 2026). See **[medipix3/](medipix3/)** and [NAMING.md](NAMING.md) (integration history).

| Document | Description |
|----------|-------------|
| [medipix3/integration.md](medipix3/integration.md) | MPX3 IOC profile, emulator, calibration, preview |
| [medipix3/preview-dual-threshold.md](medipix3/preview-dual-threshold.md) | Dual-threshold preview (vendor notes, plan) |

## Shared (both detector families)

| Document | Description |
|----------|-------------|
| [PROCESSED_IMAGE_FILE_SAVING.md](PROCESSED_IMAGE_FILE_SAVING.md) | NDArray addresses, file plugins, WriteProcessedImg/Hst |
| [TCP_PERFORMANCE_LIMITS.md](TCP_PERFORMANCE_LIMITS.md) | TCP/jsonimage streaming limits |
| [COORDINATE_MAP.md](COORDINATE_MAP.md) | BPC index ↔ image coordinates |
| [PIXELCONFIG_BPC_DIFF.md](PIXELCONFIG_BPC_DIFF.md) | Live PixelConfig vs on-disk BPC |
| [MASKED_PIXELS_JSON_AND_STREAMING.md](MASKED_PIXELS_JSON_AND_STREAMING.md) | Masked-pels JSON export |
| [PRVIMG_METADATA_RECOMMENDATIONS.md](PRVIMG_METADATA_RECOMMENDATIONS.md) | PrvImg frame metadata PVs |
| [PERFORMANCE_ANALYSIS.md](PERFORMANCE_ANALYSIS.md) | General performance notes |
| [8chip-migration.md](8chip-migration.md) | Eight-chip / dual-SPIDR (TPX3-origin; MPX3 may use 8-chip BPC) |

## Timepix3–oriented

| Document | Description |
|----------|-------------|
| [HISTOGRAM_PERFORMANCE_ANALYSIS.md](HISTOGRAM_PERFORMANCE_ANALYSIS.md) | ToF / PrvHst histogram performance |
| [PVA_FLICKERING_DIAGNOSIS.md](PVA_FLICKERING_DIAGNOSIS.md) | PVA flicker with histogram channel |
| [TimePix3_pipeline_48_64_96.svg](TimePix3_pipeline_48_64_96.svg) | Readout stack diagram (editable) |

## Project

| Document | Description |
|----------|-------------|
| [NAMING.md](NAMING.md) | ADServal vs legacy ADTimePix3; migration; repo history |
| [CONTRIBUTING.md](CONTRIBUTING.md) | License, REUSE, contribution notes |
| [MIGRATION_SUPPORT2.md](MIGRATION_SUPPORT2.md) | support2 migration checklist |
| [OPTIMIZATION_SUMMARY.md](OPTIMIZATION_SUMMARY.md) | Serval HTTP / fileWriter refactor history |
| [SIGSEGV_ON_EXIT.md](SIGSEGV_ON_EXIT.md) | IOC exit segfault notes |

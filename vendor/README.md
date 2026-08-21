# Calibration and vendor files (BPC / DACS)

Shipped and site-specific pixel configuration files. Paths are set in `profiles/<family>/init/paths.cmd` via **`BPCFilePath`** / **`DACSFilePath`** (must end with `/`).

## Layout

```
vendor/
  tpx3/
    1x1/          # 1 chip, 256×256 (65536 px) — eq.bpc, eq.dacs
    2x2/          # 4 chips, 512×512 (262144 px) — tpx3-demo.bpc, tpx3-demo.dacs (default TPX3 profile)
  mpx3/           # Medipix3 quad — eq-01.bpc, eq-01.dacs
  tpx4/           # (future) Timepix4 calibration
```

Mirror **`profiles/<family>/`** and **`MASK_BPC_NELEMENTS`** in `profiles/<family>/unique.cmd`.

## Runtime-generated files (same directory as calibration)

The driver writes next to **`BPCFilePath`**:

| File | Trigger | Naming |
|------|---------|--------|
| `mask.bpc` | Operator **MaskWrite** | `MaskFileName` (default `mask.bpc`) |
| `<stem>_masked_pels.json` | **RefreshPixelConfig** | stem = `BPCFileName` without `.bpc` |

Examples:

- TPX3 2×2: `tpx3/2x2/mask.bpc`, `tpx3/2x2/tpx3-demo_masked_pels.json`
- TPX3 1×1: `tpx3/1x1/mask.bpc`, `tpx3/1x1/eq_masked_pels.json`
- MPX3: `mpx3/mask.bpc`, `mpx3/eq-01_masked_pels.json`

`*.json` and `mask.bpc` are **gitignored** (operator/runtime artifacts). Reference exports may exist in the tree locally but are not required in git.

## Site calibration

Replace files under the appropriate mosaic folder or point `BPCFilePath` at beamline storage (e.g. `/media/nvme/cal/`). Files must be readable on the **SERVAL host** at the resolved absolute path.

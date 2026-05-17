# BPC file index and image coordinates (`mask_io.cpp`)

This note defines how **Binary Pixel Configuration (BPC)** byte indices relate to **global image** coordinates `(i, j)` in the ADTimePix3 driver. Implementation: `tpx3App/src/mask_io.cpp` (`pelIndex`, `bpc2ImgIndex`, `findChip`, `rowsCols`). PrvImg/Img TCP jsonimage streaming lives in `tpx3App/src/serval_stream.cpp` (`network_client.cpp`).

Related: [PIXELCONFIG_BPC_DIFF.md](PIXELCONFIG_BPC_DIFF.md), [MASKED_PIXELS_JSON_AND_STREAMING.md](MASKED_PIXELS_JSON_AND_STREAMING.md), [8chip-migration.md](8chip-migration.md).

Offline tools under `maskTpx3/xyChip` (e.g. **`check_bit.c`**) use the same **global `(i, j)`** convention as **`bpc2ImgIndex`** for masked-pel listings. Mask editing and **`PixelConfigDiff`** use **`pelIndex(i, j)`** instead.

## Geometry from the IOC

`rowsCols()` derives layout from Serval-backed PVs:

| Symbol | PV / source | Meaning |
|--------|-------------|---------|
| `rows` | `TPX3_NUM_ROWS` | Total image rows |
| `cols` | `yChips * chipPelWidth` | Total image columns |
| `xChips` | `TPX3_ROWLEN` | Chips per row in the mosaic |
| `yChips` | `numChips / xChips` | Chip rows |
| `w` | `chipPelWidth` = `rows / xChips` | Pixels per chip edge (often 256) |

**BPC file layout:** chip `c` occupies bytes `[c * w*w, (c+1) * w*w)`. Chip id in file order:

```text
chip = bpc_index / (w * w)     /* use integer division; not (index+1)/count */
```

**Intra-chip local coordinates** (masked-pels JSON, streaming):

```text
local = bpc_index - chip * w * w
lx = local % w
ly = local / w
```

## Image indexing conventions

| Index | Formula | Used by |
|-------|---------|---------|
| **Column `i`, row `j`** | Top-left origin; `i` increases right, `j` increases down | Mask PVs, NDArray, Phoebus displays |
| **Linear image** | `img_linear` from `bpc2ImgIndex` | Decode with mosaic **column stride**: `1`-chip: `w`; `4`-chip: `2*w`; `8`-chip: `xChips*w`. Then `i = img_linear % stride`, `j = img_linear / stride`. Mask PVs still use full `cols` from `rowsCols()` for `j*cols+i`. |
| **BPC file `k`** | `0 .. numChips*w*w - 1` | `TPX3_BPC_PEL`, disk `.bpc` |

## Which function to use

| Task | Function | Notes |
|------|----------|-------|
| Mask write, mask read, **`PixelConfigDiff`** | **`pelIndex(i, j)`** | Image → BPC byte; respects **`TPX3_DET_ORIENTATION`** |
| Masked-pels JSON **`i`/`j`**, cross-check with **`check_bit.c`** | **`bpc2ImgIndex(k, w)`** | BPC byte → linear image index → `(i,j)` |
| Which mosaic tile contains image pixel `(i,j)`? | **`findChip(i, j, &xChip, &yChip, &w)`** | `xChip = i / w`, `yChip = j / w` (chip grid only) |

**Important:** `pelIndex` and `bpc2ImgIndex` are **not** guaranteed inverses for every orientation. They match for many layouts (e.g. 1-chip UP, 4-chip UP) but **diverge** for some quad orientations (e.g. **LEFT**). The driver intentionally uses **`pelIndex`** on mask/PixelConfig paths so the mask editor and `.bpc` file stay consistent.

## Detector orientation (`TPX3_DET_ORIENTATION`)

| Value | Serval name |
|-------|-------------|
| 0 | UP |
| 1 | RIGHT |
| 2 | DOWN |
| 3 | LEFT |
| 4 | UP_MIRRORED |
| 5 | RIGHT_MIRRORED |
| 6 | DOWN_MIRRORED |
| 7 | LEFT_MIRRORED |

Set from Serval `Layout.DetectorOrientation` at connect.

## Single-chip (`numChips == 1`)

Let `w = chipPelWidth`, `k = bpc_index`.

**UP (0) — `bpc2ImgIndex`:**

```text
i = k % w
j = w - 1 - k / w
img_linear = i + w * j
```

**UP (0) — `pelIndex(i, j)`:**

```text
k = i + (w - 1 - j) * w
```

These are inverses.

## Quad 2×2 (`numChips == 4`)

Chip `c` is derived from `k` as above. Each orientation maps **intra-chip** `(lx, ly)` from `local = k - c*w*w` to global `(i,j)` with tile offsets; see `mask_io.cpp` branches.

**UP (0):** chip 0 BPC origin `k=0` maps to image column `i=w`, row `j=2*w-1` (upper-right tile in the 2×2 mosaic for the driver's tile numbering).

**LEFT (3):** chip 0 `k=0` maps to `(i,j) = (2*w-1, 0)` via `bpc2ImgIndex`, but **`pelIndex(2*w-1, 0)` is not 0** — use **`pelIndex`** for mask operations, not `bpc2ImgIndex`, on this path.

## Eight-chip mosaic (`numChips == 8`)

**Implemented only for orientation UP (0).** Rectangular grid from `rowsCols()`:

```text
X_CHIP = chip % xChips
Y_CHIP = chip / xChips
lx = local % w
ly = local / w
i = X_CHIP * w + lx
j = Y_CHIP * w + (w - 1 - ly)
img_linear = i + (xChips * w) * j
```

`pelIndex` uses the same mosaic with `k = chip * w*w + lx + (w - 1 - ly) * w`.

Other orientations: not implemented (`WARN`, returns `-1`).

## Test vectors

Golden cases (small `pel_width` for readability) live in:

```text
test/coordinate_map_vectors.json
```

Verify against the reference formulas (or future IOC test) with:

```bash
python3 test/verify_coordinate_map.py
```

The script reimplements the mapping rules above; it does **not** link the EPICS driver. When changing `mask_io.cpp`, update the JSON and script together.

## API summary (`ADTimePix` in `mask_io.cpp`)

```cpp
asynStatus rowsCols(int *rows, int *cols, int *xChips, int *yChips, int *chipPelWidth);
asynStatus findChip(int x, int y, int *xChip, int *yChip, int *width);

// Image (i,j) -> BPC byte index; -1 on error
int pelIndex(int i, int j);

// BPC byte index -> linear image index; decode i = idx % cols, j = idx / cols; -1 on error
int bpc2ImgIndex(int bpcIndexIn, int chipPelWidthIn);
```

There is **no** public `chipLocalToImage(chip, lx, ly)`; derive `k` from chip and local index, then call `bpc2ImgIndex`, or use `findChip` + `pelIndex` from image coordinates.

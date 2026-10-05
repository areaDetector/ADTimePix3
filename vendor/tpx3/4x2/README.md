# TPX3 4x2 synthetic demo calibration

This directory contains a deterministic **software/emulator fixture** for a
Timepix3 detector assembled from two adjacent 2x2 quads (four chips across and
two chips down, 1024x512 pixels). It is not detector-specific equalization data
and must not be treated as a qualified physical-hardware calibration.

Files:

- `tpx3-demo-4x2-synthetic.bpc`: 524288 bytes. Chips 0-3 are the four
  65536-byte blocks from `../2x2/tpx3-demo.bpc`; chips 4-7 repeat those blocks
  in the same order.
- `tpx3-demo-4x2-synthetic.dacs`: unique `[Chip0]` through `[Chip7]` sections.
  Chips 4-7 repeat the values from chips 0-3 respectively, with section names
  changed to the final chip numbers.

The two-quad UP image placement is:

```text
+---+---+---+---+
| 2 | 1 | 4 | 7 |
+---+---+---+---+
| 3 | 0 | 5 | 6 |
+---+---+---+---+
```

Generate the files or verify the checked-in copies from the repository root:

```bash
python3 test/generate_tpx3_4x2_demo_calibration.py
python3 test/generate_tpx3_4x2_demo_calibration.py --check
```

Provenance and deterministic checksums:

| File | SHA-256 |
|---|---|
| `../2x2/tpx3-demo.bpc` | `7c065b252dce0fc56aa991957eadaebfadb8c9e068072b17530dbb3775ef338b` |
| `../2x2/tpx3-demo.dacs` | `df3fba01f99d4080df518c6953ec4ebdfbf2974a6cb9a92d41adaf16687969a1` |
| `tpx3-demo-4x2-synthetic.bpc` | `6cd7f48d3833dc29934e0048ed611f4665efceed68c543dcf762a762277d474c` |
| `tpx3-demo-4x2-synthetic.dacs` | `00f08da4d06a876bf5d7d6286fd618da3d90810eeb552c29ba65e1c5f40aeec9` |

Replace both synthetic files with calibration generated for the actual eight
chips before physical use. Confirm BPC chip order, image placement, and mask
alignment on hardware.

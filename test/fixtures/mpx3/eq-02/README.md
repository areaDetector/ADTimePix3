# ASI MPX3 `eq-02` reference fixture

These files were supplied by ASI on 2026-09-24 as a corresponding Medipix3
Binary Pixel Configuration and Serval per-chip configuration response:

- `eq-02.bpc`
- `detector-chips.json`

On 2026-09-25, Erik Maddox confirmed on behalf of ASI that both attachments may
be redistributed as example files and test configuration. They are included
under `LicenseRef-ASI-Example-Redistribution`; see the sidecar metadata and
`LICENSES/LicenseRef-ASI-Example-Redistribution.txt`.

The files are reference/test data, not a default operational calibration. Do
not upload `eq-02.bpc` to an unrelated detector.

## Integrity and correspondence

SHA-256:

```text
e9bf9c3e9b812ff26d45538ddfb0a1f04cf79fcbdc2d4c38db3308e4b9a7452d  eq-02.bpc
5981b109fe475c24daed799c89650debb65c89e7ebd6134715d7e50fb4db5998  detector-chips.json
```

Validated locally:

- `eq-02.bpc` is 524,288 bytes: four 131,072-byte chip blocks.
- `detector-chips.json` contains four chip objects.
- Each Base64-decoded `PixelConfig` is 131,072 bytes.
- Each decoded `PixelConfig` matches the corresponding BPC chip block exactly.
- ASI confirmed that both representations use chip order 0, 1, 2, 3.
- The BPC contains 262,144 big-endian 16-bit pixel words.
- Mask bit 0 is set for 17 words: 4, 5, 5, and 3 in chips 0 through 3.
- Bit 11 and bits 12 through 15 are clear throughout this sample.

The fixture supports validation of packed MPX3 word decoding, chip-block order,
bit-preserving mask operations, and Serval/BPC comparison. ASI indicates that
Serval `PixelConfig` is converted from the BPC rather than read directly from
detector registers, so this correspondence is not hardware-register readback.
Synthetic vectors remain the primary unit-test coverage so the regular test
suite stays small and does not treat detector-specific calibration as portable.

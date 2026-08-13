# Medipix3 Phoebus screenshots

PNG captures for [integration.md](../integration.md) and Sphinx release docs (`docs/ADTimePix3/Screenshots/MediPix3/` when added).

| File | Phoebus screen | Notes |
|------|----------------|-------|
| `Mpx3_preview_monitor.png` | `Acquire/Mpx3PrvImgMonitor.bob` | Dual-threshold preview (8088/8089), T0/T1 and T0−T1 band (Pva5/Pva6) |
| `Mpx3_dest_writer.png` | `Acquire/Mpx3ServerFileWriter.bob` | Serval destination writer: PrvImg, Img[], WriteData |
| `Mpx3_img_monitor.png` | `Acquire/Mpx3ImgMonitor.bob` | Full-rate Image[] demux (8086), Pva7/Pva8 — low rate only |
| `Mpx3_detector_config.png` | `Detector/Mpx3DetectorConfig.bob` | Detector config: GainMode, BothCounters, trigger timing, IDelayConfig |

**Planned:** `Mpx3_main.png` (`MediPix3.bob`), `Mpx3_hdf_img_config.png`, acquire-active variants.

Reference in markdown (used in [integration.md](../integration.md)):

```markdown
![Preview monitor](screenshots/Mpx3_preview_monitor.png)
```

Bob files live under `tpx3App/op/bob/MediPix3/`.

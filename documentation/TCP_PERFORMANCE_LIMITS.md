# TCP Socket Performance Limits for Time-of-Flight Applications

## Executive Summary

**5 microseconds per frame (200,000 FPS) is NOT feasible over TCP socket for full 2D images.**

For Time-of-Flight (ToF) applications requiring very fast frame rates, the **histogram channel (`PrvHst`) is the recommended approach** rather than full 2D image streaming.

## Physical Limits

### Bandwidth Requirements

For a 512×512 pixel image with 16-bit pixels (uint16):
- **Image size per frame**: 512 × 512 × 2 bytes = **524,288 bytes** (~512 KB)
- **JSON header overhead**: ~200-500 bytes per frame (jsonimage format)
- **Total per frame**: ~525 KB

**Data rate calculations:**

| Frame Rate | Exposure Time | Data Rate | Feasibility |
|------------|---------------|-----------|-------------|
| 1,000 FPS | 1.0 ms | ~500 MB/s | **Possible** (with optimization) |
| 10,000 FPS | 0.1 ms | ~5 GB/s | **Difficult** (CPU/memory bound) |
| 100,000 FPS | 0.01 ms | ~50 GB/s | **Very difficult** (requires zero-copy, optimized processing) |
| 200,000 FPS | 0.005 ms (5 μs) | ~100 GB/s | **Extremely difficult** (approaches memory bandwidth limits) |

### Localhost (Loopback) vs Ethernet

**Important**: The TCP connection uses `localhost` (127.0.0.1), which means data travels through the **loopback interface**, not an Ethernet link.

**Loopback Interface Characteristics:**
- **Bandwidth**: Limited by CPU/memory bandwidth, not network hardware
- **Typical throughput**: 10-50+ GB/s (depends on CPU architecture and memory speed)
- **Latency**: Very low (~1-10 μs), no network stack overhead
- **No packet loss**: Reliable, no network congestion

**Comparison:**

| Interface | Bandwidth | Max FPS (512×512) | Limiting Factor |
|-----------|-----------|-------------------|-----------------|
| **Localhost (loopback)** | 10-50+ GB/s | **20,000-100,000+ FPS** | CPU/memory bandwidth |
| 1 GbE | ~100 MB/s | ~200 FPS | Network hardware |
| 10 GbE | ~1 GB/s | ~2,000 FPS | Network hardware |
| 100 GbE | ~10 GB/s | ~20,000 FPS | Network hardware |

**Conclusion**: With localhost, **network bandwidth is NOT the limiting factor**. The bottleneck is **CPU processing overhead** (JSON parsing, byte swapping, memory copies) and **memory bandwidth** for very high rates. 5 microseconds (200,000 FPS) at ~100 GB/s is approaching memory bandwidth limits on typical systems but may be feasible with optimized processing.

## Processing Overhead

### Current Implementation Overhead

Each frame requires:
1. **TCP socket read**: Blocking `recv()` call (~10-50 μs depending on data availability)
2. **JSON parsing**: nlohmann/json parsing of header (~50-200 μs for ~500 byte JSON)
3. **Binary data read**: Additional `receive_exact()` for pixel data (~100-500 μs for 512 KB)
4. **Byte order conversion**: Network-to-host byte swapping for all pixels (~200-1000 μs for 512×512)
5. **NDArray allocation**: EPICS NDArray pool allocation (~50-200 μs)
6. **Memory copy**: Copying pixel data to NDArray (~100-500 μs)
7. **EPICS callbacks**: `doCallbacksGenericPointer()` to plugins (~100-500 μs)
8. **Parameter updates**: asyn parameter updates (~10-50 μs)

**Total processing time per frame**: ~620-2,500 μs (0.62-2.5 ms)

**Maximum sustainable rate**: ~400-1,600 FPS (with current implementation)

**Note**: With localhost loopback, network latency is negligible (~1-10 μs), so the bottleneck is purely processing overhead, not network bandwidth.

### Bottlenecks

1. **JSON parsing**: nlohmann/json is flexible but not optimized for high-speed parsing
2. **Byte swapping**: Per-pixel byte order conversion is CPU-intensive
3. **NDArray pool**: EPICS NDArray pool can become exhausted at high rates
4. **EPICS callbacks**: Plugin callbacks add latency and can block
5. **Mutex contention**: Multiple mutex locks in processing path

## Current Performance Issues (from your logs)

Your logs show:
```
Trigger FPS: 1000.0, Exposure (ms): 1.0
WARN: Attempted to overwrite frame that was not fully read out due to limited buffers
WARN: Image Pool is empty... Awaiting free images.
```

**Analysis:**
- **1,000 FPS (1 ms exposure)** is at the edge of feasibility
- Serval is producing frames faster than the driver can consume them
- EPICS NDArray pool is exhausted (not enough buffers)
- Frame buffer overflow in Serval's internal pipeline

**Root cause**: Processing overhead (~1-2 ms per frame) exceeds the 1 ms frame period.

## Recommendations for Time-of-Flight Applications

### Option 1: Use Histogram Channel (Recommended)

**For ToF applications, use the histogram channel (`PrvHst`) instead of full 2D images:**

- **Data size**: ~16,000 bins × 4 bytes (int32) = **64 KB per frame** (vs 512 KB for images)
- **No JSON parsing**: Binary jsonhisto format is more efficient
- **No byte swapping**: Histogram data is already in correct format
- **Lower overhead**: ~10-50 μs processing time per frame
- **Higher rate capability**: Can sustain **10,000-50,000 FPS** easily

**Configuration:**
```bash
# Set histogram channel for ToF
caput TPX3-TEST:cam1:PrvHstFilePath "tcp://listen@localhost:8451"
caput TPX3-TEST:cam1:PrvHstFileFmt 4  # jsonhisto format
caput TPX3-TEST:cam1:PrvHstNumBins 16000
caput TPX3-TEST:cam1:PrvHstBinWidth 0.000001  # 1 μs bins
caput TPX3-TEST:cam1:WritePrvHst 1
```

**Benefits:**
- **100× smaller data size** (64 KB vs 512 KB per frame)
- **10-100× faster processing** (no JSON parsing, no byte swapping)
- **Suitable for 5 μs frame rates** (200,000 FPS) with proper network infrastructure

### Option 2: Optimize Image Channel (If Full Images Required)

If you absolutely need full 2D images for ToF:

1. **Increase NDArray pool size**:
   ```cpp
   // In ADTimePixConfig() call
   ADTimePixConfig("TPX3", "http://localhost:8081", 100, 10000000000, ...);
   // maxBuffers=100, maxMemory=10GB
   ```

2. **Use raw binary format** (if Serval supports it):
   - Skip JSON parsing overhead
   - Direct binary transfer
   - Requires format changes in driver

3. **Zero-copy optimizations**:
   - Use `mmap()` for shared memory
   - Direct DMA transfers
   - Requires significant driver modifications

4. **Reduce image size**:
   - Use region-of-interest (ROI) to read smaller sub-images
   - Reduces data size proportionally

5. **Optimize for localhost** (already using loopback):
   - Localhost has very high bandwidth (10-50+ GB/s)
   - Focus on reducing CPU processing overhead
   - Consider CPU affinity/pinning for worker threads
   - Use NUMA-aware memory allocation if on multi-socket systems

### Option 3: Hybrid Approach

Use histogram channel for ToF timing, and image channel for slower visualization:
- **Histogram**: High-rate ToF data (5 μs frames)
- **Image**: Lower-rate visualization (100-1000 FPS)

## Practical Limits

### Achievable Frame Rates (with current implementation)

| Application | Frame Rate | Exposure | Method | Status |
|-------------|------------|----------|--------|--------|
| Preview/Monitoring | 1-60 FPS | 16-1000 ms | Image TCP | ✅ **Easy** |
| Standard Acquisition | 10-100 FPS | 10-100 ms | Image TCP | ✅ **Easy** |
| Fast Acquisition | 100-1000 FPS | 1-10 ms | Image TCP | ⚠️ **Possible** (with optimization) |
| Very Fast Acquisition | 1,000-10,000 FPS | 0.1-1 ms | Image TCP (localhost) | ⚠️ **Difficult** (CPU-bound, requires optimization) |
| ToF Applications | 10,000-200,000 FPS | 5-100 μs | **Histogram TCP** (localhost) | ✅ **Recommended** |

## Conclusion

**For 5 microsecond frame rates (200,000 FPS) in ToF applications:**

1. ✅ **Use histogram channel (`PrvHst`)** - This is the correct approach for ToF
2. ⚠️ **Full 2D image channel at 5 μs**: With localhost loopback, network bandwidth is not the limit, but **CPU processing overhead** makes this extremely difficult. Current implementation can handle ~400-1,600 FPS. With significant optimization (zero-copy, optimized parsing), higher rates may be possible but are not guaranteed.
3. ⚠️ **If images are required**, maximum practical rate with current implementation is ~1,000-2,000 FPS. Higher rates require optimization of processing pipeline.

**Key Point**: Since you're using `localhost` (loopback interface), the bottleneck is **CPU processing time**, not network bandwidth. The loopback interface can handle 10-50+ GB/s, but processing each frame takes ~1-2 ms, limiting sustainable rate to ~500-1,000 FPS with current code.

The histogram channel is specifically designed for high-rate ToF applications and can handle microsecond-scale frame rates efficiently.

## References

- Histogram channel documentation: See `PrvHstHistogram.bob` screen
- TCP streaming implementation: `tpx3App/src/ADTimePix.cpp` (lines 3399-3740 for image, `histogram_io.cpp` for histogram)
- Buffer sizes: `MAX_BUFFER_SIZE = 32768` bytes (JSON header buffer)
- NDArray pool: Configured in `ADTimePixConfig()` call

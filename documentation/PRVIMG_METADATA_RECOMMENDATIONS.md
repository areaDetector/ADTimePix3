# PrvImg TCP Streaming Metadata Recommendations

## Overview

This document recommends which metadata from jsonimage TCP streaming should be exposed as EPICS PVs and whether a dedicated screen should be created for preview image monitoring.

## Available Metadata from jsonimage Header

The jsonimage format from Serval includes the following metadata in the JSON header:

```json
{
  "width": 256,
  "height": 256,
  "pixelFormat": "uint16",
  "frameNumber": 12345,
  "timeAtFrame": 1234567890.123456
}
```

### Currently Extracted (but not all exposed as PVs)

1. **width, height** ✅ - Already exposed as `ADSizeX`, `ADSizeY` (via `NDArraySizeX`, `NDArraySizeY`)
2. **pixelFormat** ✅ - Already exposed as `DataType` (via `NDDataType`)
3. **frameNumber** ⚠️ - Extracted but **NOT exposed as PV**
4. **timeAtFrame** ❌ - **NOT extracted or exposed**

### Comparison with tpx3image IOC

The `/epics/iocs/tpx3image` IOC exposes the following metadata PVs:

| PV Name | Type | Description | Status in ADTimePix3 |
|---------|------|-------------|---------------------|
| `TIME_AT_FRAME_NS` | Float64 | Timestamp at frame (nanoseconds) | ❌ Missing |
| `FRAME_NUMBER` | Int32 | Frame number from JSON | ❌ Missing |
| `FRAME_WIDTH` | Int32 | Width of current frame | ✅ Available as `NDArraySizeX` |
| `FRAME_HEIGHT` | Int32 | Height of current frame | ✅ Available as `NDArraySizeY` |
| `FRAME_PIXEL_FORMAT` | Int32 | Pixel format (0=UINT16, 1=UINT32) | ✅ Available as `NDDataType` |
| `ACQUISITION_RATE` | Float64 | Calculated frames per second | ❌ Missing (calculated) |

## Recommended Metadata PVs to Add

### High Priority (Direct from JSON)

1. **`TPX3_PRVIMG_FRAME_NUMBER`** (Int32)
   - **Source**: `frameNumber` from jsonimage header
   - **Use Case**: Frame tracking, frame loss detection, synchronization
   - **Current Status**: Extracted but not exposed
   - **Implementation**: Add parameter and database record

2. **`TPX3_PRVIMG_TIME_AT_FRAME`** (Float64)
   - **Source**: `timeAtFrame` from jsonimage header (nanoseconds)
   - **Use Case**: Precise timing, synchronization with other data streams
   - **Current Status**: Not extracted
   - **Implementation**: Extract from JSON, add parameter and database record

### Medium Priority (Calculated)

3. **`TPX3_PRVIMG_ACQUISITION_RATE`** (Float64)
   - **Source**: Calculated from frame number differences and timestamps
   - **Use Case**: Monitor actual frame rate, detect performance issues
   - **Current Status**: Not calculated
   - **Implementation**: Calculate in worker thread, similar to tpx3image
   - **Algorithm**: Track previous frame number and timestamp, calculate rate over sliding window

4. **`TPX3_PRVIMG_FRAME_LOSS`** (Int32) - Optional
   - **Source**: Calculated from frame number gaps
   - **Use Case**: Detect dropped frames, data quality monitoring
   - **Current Status**: Not tracked
   - **Implementation**: Compare expected vs received frame numbers

### Low Priority (Already Available via areaDetector)

- **Width/Height**: Already available as `NDArraySizeX`, `NDArraySizeY`
- **Pixel Format**: Already available as `NDDataType`
- **Timestamp**: Already available as `NDArrayTimeStamp`

## Screen Recommendation

### Should a Dedicated Screen Be Created?

**Recommendation: YES** - Create a dedicated preview image monitoring screen similar to tpx3image.

### Rationale

1. **Separation of Concerns**: Preview image monitoring is distinct from main acquisition control
2. **Real-time Monitoring**: High frame rates (200 fps) benefit from dedicated monitoring
3. **Metadata Visibility**: Frame number, timing, and rate information are valuable for diagnostics
4. **User Experience**: Similar to tpx3image screen, provides familiar interface for TCP streaming

### Screen Features (Based on tpx3image.bob)

#### Connection Section
- **Connection Status**: LED indicator for TCP connection
- **Host/Port Display**: Show current TCP connection details (extracted from `PrvImgFilePath`)
- **Connection State**: Connect/Disconnect button (if needed)

#### Frame Information Section
- **Frame Number**: Current frame number from jsonimage
- **Time at Frame**: Timestamp from Serval (nanoseconds)
- **Acquisition Rate**: Calculated frames per second
- **Frame Loss Indicator**: Warning if frames are dropped

#### Image Information Section
- **Image Dimensions**: Width × Height (from `NDArraySizeX`, `NDArraySizeY`)
- **Pixel Format**: Display format (UInt16/UInt32 from `NDDataType`)
- **Array Counter**: Current array counter (from `NDArrayCounter`)

#### Status Section
- **Status Message**: Connection/processing status
- **Error Count**: Number of processing errors
- **Processing Time**: Time to process each frame (optional)

### Screen Location

**Recommended Path**: `tpx3App/op/bob/Acquire/PrvImgMonitor.bob`

This keeps it with other acquisition screens and clearly identifies it as a preview image monitoring screen.

## Implementation Plan

### Phase 1: Add Metadata PVs

1. **Add Parameter Definitions** (`ADTimePix.h`)
   ```cpp
   #define ADTimePixPrvImgFrameNumberString     "TPX3_PRVIMG_FRAME_NUMBER"
   #define ADTimePixPrvImgTimeAtFrameString    "TPX3_PRVIMG_TIME_AT_FRAME"
   #define ADTimePixPrvImgAcqRateString        "TPX3_PRVIMG_ACQ_RATE"
   ```

2. **Add Member Variables** (`ADTimePix.h`)
   ```cpp
   int ADTimePixPrvImgFrameNumber;
   int ADTimePixPrvImgTimeAtFrame;
   int ADTimePixPrvImgAcqRate;
   
   // For rate calculation
   int prvImgPreviousFrameNumber_;
   double prvImgPreviousTimeAtFrame_;
   double prvImgAcquisitionRate_;
   std::deque<double> prvImgRateSamples_;
   double prvImgLastRateUpdateTime_;
   ```

3. **Extract and Set Metadata** (`ADTimePix.cpp::processPrvImgDataLine()`)
   - Extract `timeAtFrame` from JSON
   - Set `frameNumber` PV (already extracted)
   - Set `timeAtFrame` PV
   - Calculate and set `acquisitionRate` PV

4. **Add Database Records** (`Server.template` or new `PrvImgMetadata.template`)
   - `PrvImgFrameNumber_RBV` (longin)
   - `PrvImgTimeAtFrame_RBV` (ai, Float64, nanoseconds)
   - `PrvImgAcqRate_RBV` (ai, Float64, Hz)

### Phase 2: Create Monitoring Screen

1. **Create Screen File**: `tpx3App/op/bob/Acquire/PrvImgMonitor.bob`
2. **Screen Sections**:
   - Connection status and TCP info
   - Frame metadata (number, time, rate)
   - Image information
   - Status and errors
3. **Reference**: Use `tpx3image.bob` as template

## Benefits

### Technical Benefits
- **Frame Tracking**: Monitor frame sequence for synchronization
- **Timing Information**: Precise timestamps for data correlation
- **Performance Monitoring**: Real-time frame rate tracking
- **Diagnostics**: Frame loss detection for troubleshooting

### User Benefits
- **Visibility**: See what's happening with preview images
- **Debugging**: Easier to diagnose TCP streaming issues
- **Monitoring**: Track performance at high frame rates
- **Consistency**: Similar interface to tpx3image IOC

## Priority Assessment

### High Priority (Implement First)
- ✅ `TPX3_PRVIMG_FRAME_NUMBER` - Essential for frame tracking
- ✅ `TPX3_PRVIMG_TIME_AT_FRAME` - Important for timing/synchronization

### Medium Priority (Implement Second)
- ✅ `TPX3_PRVIMG_ACQUISITION_RATE` - Useful for performance monitoring
- ✅ Dedicated monitoring screen - Improves user experience

### Low Priority (Optional)
- ⚠️ `TPX3_PRVIMG_FRAME_LOSS` - Nice to have, but can be calculated from frame numbers

## Conclusion

**Recommendation**: Add the high-priority metadata PVs (`FRAME_NUMBER` and `TIME_AT_FRAME`) and create a dedicated monitoring screen. This provides essential frame tracking and timing information while maintaining consistency with the tpx3image IOC approach.

The acquisition rate calculation and screen can be added in a second phase if needed, but the frame number and timestamp are the most critical metadata for proper operation and debugging.

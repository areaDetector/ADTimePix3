# PVA Image Flickering Diagnosis Guide

## Issue
When histogram channel is enabled, the PVA image display (`pva://TPX3-TEST:Pva1:Image`) flickers.

**Important**: The flickering occurs in the **Preview image channel** (`TPX3-TEST:cam1:PrvImgFilePath`, using `tcp://listen@localhost:8089` and `http://localhost`), **not** in the actual image channel (`TPX3-TEST:cam1:ImgFilePath`). The PVA plugin is configured to listen to NDArray address 0, which corresponds to the PrvImg (preview) channel.

**Note**: This issue has been observed with the TimePix3 emulator (`tpx3Emulator-4.1.1-rc1.jar`). It may not occur with physical detectors. Testing with real hardware is recommended to confirm if this is emulator-specific.

## Root Cause Analysis

The PVA plugin is configured to listen to NDArray address 0 (PrvImg channel):
```
NDPvaConfigure("PVA1", $(QSIZE), 0, "$(PORT)", 0, $(PREFIX)Pva1:Image, 0, 0, 0)
```

The histogram channel (address 5) performs the following sequence:
1. Saves shared parameters (ADSizeX, ADSizeY, NDArraySizeX, NDArraySizeY, NDDataType, NDArraySize)
2. Sends histogram NDArray via callback (address 5)
3. Restores shared parameters to previous values
4. Calls `callParamCallbacks()` which triggers parameter updates

The parameter restoration and `callParamCallbacks()` may be causing the PVA plugin to see parameter changes that trigger unnecessary redraws, even though the actual NDArray data (address 0) hasn't changed.

## Diagnostic Steps

### 1. Monitor Parameter Changes
Monitor the shared parameters to see if they're changing when histogram is enabled:
```bash
camonitor TPX3-TEST:cam1:SizeX_RBV TPX3-TEST:cam1:SizeY_RBV TPX3-TEST:cam1:DataType_RBV
```
If these parameters are changing when histogram callbacks occur, that's likely the cause.

### 2. Check PVA Plugin Configuration
Verify which NDArray address the PVA plugin is listening to:
```bash
caget TPX3-TEST:Pva1:NDArrayAddress
```
Should be 0 (PrvImg channel).

### 3. Monitor NDArray Callback Rates
Check if histogram callbacks are interfering:
```bash
camonitor TPX3-TEST:cam1:ArrayCounter TPX3-TEST:cam1:ArrayRate_RBV
```
Verify that `ArrayRate_RBV` matches the PrvImg rate (not double).

### 4. Check Histogram Channel Status
Monitor histogram channel activity:
```bash
camonitor TPX3-TEST:cam1:PrvHstAccumulationEnable TPX3-TEST:cam1:PrvHstAcqRate_RBV
```

### 5. Test with Histogram Disabled
Disable histogram accumulation and verify flickering stops:
```bash
caput TPX3-TEST:cam1:PrvHstAccumulationEnable 0
```

### 6. Monitor Parameter Callback Frequency
Add debug output to see when `callParamCallbacks()` is being called:
- Check IOC console for parameter update messages
- Monitor parameter change rates

## Potential Solutions

### Solution 1: Don't Restore Parameters Immediately
The histogram channel could delay parameter restoration or avoid calling `callParamCallbacks()` after restoration.

### Solution 2: Use Selective Parameter Callbacks
Instead of `callParamCallbacks()` (which updates all parameters), use selective callbacks only for histogram-specific parameters.

### Solution 3: Move Parameter Restoration
Restore parameters before sending the NDArray callback, not after, to minimize the window where parameters are changed.

### Solution 4: Add Parameter Change Suppression
Add a flag to suppress parameter callbacks during histogram processing to prevent unnecessary updates.

## Recommended Fix

The issue is likely that `callParamCallbacks()` at line 761 in `histogram_io.cpp` is being called after parameter restoration, triggering updates to all parameters including those used by PVA.

**Fix**: Move the parameter restoration to happen BEFORE `callParamCallbacks()`, or better yet, only call `callParamCallbacks()` for histogram-specific parameters, not all parameters.

## Additional Findings

### Issue: Worker Thread Still Runs When Accumulation Disabled

The histogram worker thread continues to run and process data even when `PrvHstAccumulationEnable=0`. This means:
- TCP connection is still active
- Data is still being read and parsed
- Rate calculation still happens (explains why `PrvHstAcqRate_RBV` shows 5 Hz even when disabled)
- Only accumulation and NDArray callbacks are skipped

### Issue: ArrayCounter Undefined

`ArrayCounter` is undefined because:
- PrvImg channel no longer increments it (fixed to prevent double counting)
- Img channel increments it, but if Img channel isn't active, it stays at 0/undefined
- This is expected behavior - `ArrayRate_RBV` will be 0 when only PrvImg is active

### Potential Root Cause: Detector Emulator Behavior

The flickering might be caused by the detector emulator changing behavior when histogram channel is enabled in Serval, even if the driver's accumulation is disabled. The TCP connection is still active, so Serval might be sending data differently.

### Diagnostic Commands

1. **Check if worker thread is running:**
   ```bash
   # Monitor histogram connection status
   camonitor TPX3-TEST:cam1:PrvHstAccumulationEnable
   # Check if TCP connection is active (look for connection messages in IOC console)
   ```

2. **Check Serval configuration:**
   - Verify if histogram channel is enabled in Serval configuration
   - Check if enabling/disabling histogram in Serval affects image streaming

3. **Monitor NDArray callbacks:**
   ```bash
   # Check if histogram NDArrays are being sent (should be 0 when accumulation disabled)
   camonitor TPX3-TEST:cam1:NDArrayCounter
   ```

4. **Test with histogram completely disabled in Serval:**
   - Disable `WritePrvHst` in Serval configuration
   - Verify if flickering stops

5. **Check if worker thread is still running:**
   ```bash
   # Check IOC console for "PrvHst worker thread" messages
   # If thread is running, you'll see connection/processing messages
   # If disabled, thread should not be started (check acquireStart code)
   ```

6. **Monitor histogram NDArray callbacks:**
   ```bash
   # Histogram uses address 5, PVA uses address 0
   # Check if any NDArrays from address 5 are being received by PVA
   # This shouldn't happen, but worth checking
   ```

## Root Cause Hypothesis

Based on the observations:
1. Flickering occurs less frequently after fix (good progress)
2. Disabling `PrvHstAccumulationEnable` doesn't stop flickering
3. Size parameters don't change (our fix worked)
4. `PrvHstAcqRate_RBV` shows 5 Hz even when disabled (stale value or worker thread still running)

**Most Likely Cause**: The TimePix3 emulator (`tpx3Emulator-4.1.1-rc1.jar`) behavior changes when histogram channel is enabled in Serval configuration, even if the driver's accumulation is disabled. This could affect:
- Preview image data timing (PrvImg channel: `tcp://listen@localhost:8089`, `http://localhost`)
- Network bandwidth allocation
- Frame synchronization
- TCP connection handling

**Note**: This may be emulator-specific and might not occur with physical detectors. The flickering could be due to how the emulator handles multiple concurrent TCP streams (preview image + histogram channels). The main image channel (Img) is not affected.

**To Diagnose**:
1. Test with a physical detector to confirm if flickering occurs with real hardware
2. Check Serval/emulator logs/configuration to see if histogram channel affects image streaming
3. Monitor network traffic to see if histogram TCP connection affects image TCP connection
4. Test with histogram completely disabled in Serval (not just driver accumulation)
5. Check if flickering frequency correlates with histogram frame rate (5 Hz)

## PV Conflict Analysis

**No Duplicate PVs Found**: Investigation confirms that the standalone histogram IOC (`/epics/iocs/histogram/tpx3HistogramApp`) and ADTimePix3 driver do NOT have duplicate PVs that could cause conflicts:

- **Standalone Histogram IOC**: Uses `asynPortDriver` (not `ADDriver`), so it does NOT have areaDetector base parameters like `ADSizeX`, `NDArraySizeX`, `NDArraySizeY`, `NDDataType`, `NDArrayCounter`, `ArrayRate_RBV`, etc.
- **PV Names**: Standalone histogram IOC uses different PV names (e.g., `$(P)$(R)FRAME_COUNT`, `$(P)$(R)TOTAL_COUNTS`) with a different prefix, so there's no naming conflict.
- **NDArray Parameters**: The standalone histogram IOC doesn't set any areaDetector shared parameters that could affect ADTimePix3's image channels.

**Conclusion**: The flickering is NOT caused by duplicate PVs or parameter conflicts between the two IOCs. This definitively demonstrates that the issue originates from emulator-level behavior changes when the histogram channel is enabled in Serval configuration.

## Emulator-Level Issue Confirmation

**Important**: The flickering issue is likely related to the **TimePix3 emulator** (`tpx3Emulator-4.1.1-rc1.jar`), not necessarily Serval itself. When the histogram channel is enabled in Serval configuration (even if ADTimePix3's `PrvHstAccumulationEnable` is disabled), the emulator's behavior may change, affecting:
- Preview image data streaming timing/synchronization (PrvImg channel: `tcp://listen@localhost:8089`, `http://localhost`)
- Network bandwidth allocation between channels
- Frame delivery patterns
- TCP connection handling

**Channel-Specific**: The flickering affects the **Preview image channel** (PrvImg, address 0), not the main image channel (Img, address 1). The main image channel (`TPX3-TEST:cam1:ImgFilePath`) is not affected by this issue.

**This may not be an issue with physical detectors** - the flickering could be specific to emulator behavior when multiple TCP channels (preview image + histogram) are active simultaneously. The driver fixes (selective parameter callbacks, parameter preservation) have reduced flickering frequency but cannot eliminate it completely because the root cause appears to be in the emulator's data streaming behavior, not Serval or the driver.

**Testing Recommendation**: If possible, test with a physical detector to confirm whether flickering occurs with real hardware or is emulator-specific.

## Recommendations

1. **For Production Use**: If flickering is problematic, consider:
   - Disabling histogram channel in Serval configuration when not needed
   - Using the standalone histogram IOC instead of ADTimePix3's histogram feature
   - Testing with a physical detector to confirm if the issue is emulator-specific
   - Accepting the flickering as a known emulator limitation if it only occurs with the emulator

2. **For Emulator/Serval Developers**: This issue should be reported to emulator developers (or Serval developers if confirmed with physical detectors) as a potential improvement area for multi-channel TCP streaming behavior when histogram and image channels are enabled simultaneously.

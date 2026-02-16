# SIGSEGV on IOC Exit (exit / PrvHst TCP disconnected)

**Resolved in ADCore 3.14.0 and later.** If you use ADCore R3-14 or newer, this issue should no longer occur and no local patches are needed. The notes below apply when using older ADCore versions.

## What happens

When you type `exit` in the IOC shell, the process can abort with signal 11 (SIGSEGV) after messages like `PrvHst TCP disconnected`. The crash is in **ADCore**: `NDArrayPool::release()` is called after the driver and its pool have already been destroyed.

**Cause (short):** On exit, the pvAccess ServerContext is torn down (atexit). Its MonitorElements still hold NDArray-derived data. The custom deleter used by **ntndArrayConverter** (`freeNDArray`) calls `NDArray::release()` on that array. By then the detector driver’s **NDArrayPool** has already been deleted, so `release()` runs against freed memory → SIGSEGV.

## Fix (in ADCore)

The fix is in **ADCore**, not in ADTimePix3:

1. **asynNDArrayDriver** stores `maxAddr` in a member `maxAddr_`.
2. In **~asynNDArrayDriver()**, before `delete pNDArrayPoolPvt_`, the driver nulls **pNDArrayPool** on every entry in **pArrays[]**.

That way, when PVA later calls `array->release()`, **NDArray::release()** sees `pNDArrayPool == NULL` and returns without touching the (already deleted) pool.

**Files to change in ADCore:**

- `ADCore/ADApp/ADSrc/asynNDArrayDriver.h`: add `int maxAddr_;` (private).
- `ADCore/ADApp/ADSrc/asynNDArrayDriver.cpp`:
  - In the constructor initializer list: `maxAddr_(maxAddr)`.
  - In the destructor, before `delete this->pNDArrayPoolPvt_;`, add:
    - Loop `i = 0 .. maxAddr_-1`, and for each non-null `pArrays[i]` set `pArrays[i]->pNDArrayPool = NULL`.

## Next steps if you still see the crash

1. **Rebuild ADCore first**  
   In your ADCore source (the one you patched), run:
   ```bash
   cd /path/to/areaDetector/ADCore
   make clean
   make
   ```
2. **Then rebuild ADTimePix3 / tpx3App**  
   So the application links against the updated ADCore:
   ```bash
   cd /path/to/ADTimePix3
   make clean
   make
   ```
3. **Confirm you’re using the patched ADCore**  
   - Check `configure/RELEASE` (and any included files like `RELEASE_LIBS_INCLUDE`) so that `ADCORE` points to the directory where you applied the fix.  
   - If you have a system or other prebuilt ADCore, the IOC may still be linking to that; point to your patched build instead.
4. **If it still crashes**  
   Run under gdb again and capture a backtrace. Check:
   - Whether the fault is still in `NDArrayPool::release` (same as before).  
   - The `pArray` pointer in the crash frame: if it is **not** one of the driver’s `pArrays[0]` / `pArrays[1]`, then some other path (e.g. a plugin) is handing a different array to PVA; that would need a broader fix (e.g. in ntndArrayConverter or shutdown order).

## Is this important?

- **For production:** Yes. Clean shutdown without SIGSEGV avoids:
  - Core dumps and misleading “crash” reports.
  - Possible issues with process managers / restart logic that expect a clean exit code.
- **For development:** Exiting the IOC without a segfault makes debugging and restart cycles simpler and less noisy.

Recommend reporting the issue (and the ADCore destructor fix above) upstream to **areaDetector/ADCore** so it can be included in a future release for everyone.

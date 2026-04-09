# Working in Progress — Repository Reorganization

This document tracks the ongoing repository reorganization effort to unify platform-specific code (ESP32-D0, ESP32-S3, ESP32-P4) under a common vocabulary and clean architecture.

**Status: Phases 1–9 completed.**

---

## Current call path (reverse-engineered)

The two public entry points are `initled()` and `showPixels()`. Every other function is called from these two.  The description below follows the code top-down for all three targets (ESP32-D0, ESP32-S3, ESP32-P4).

### `initled()` path

There are currently many `initled()` overloads.  The canonical one — the lowest-level call that all others eventually reach — takes explicit component layout parameters:

```text
initled(leds, pinsq, sizes[], numStrips, channelsPerLight, pR, pG, pB, pW, pW2)   ← canonical / main
  │
  ├─ populate stripSize[], totalLeds
  └─ initLedImpl(leds, pinsq, numStrips, maxLength)
```

All other `initled()` variants are convenience wrappers that decode their arguments and delegate:

```text
initled(leds, pinsq, sizes[], numStrips, cArr)         — decodes cArr → pR/pG/pB/pW/pW2 inline
initled(leds, pinsq, numStrips, numLedPerStrip, cArr)  — uniform lengths; fills stripSize[], calls above
initled(pinsq, numStrips, numLedPerStrip, cArr)        — no leds pointer (set externally)
initled(Pixels pix, pinsq)                             — PixelsLib wrapper
```

The `cArr` decoder (a large switch-case) is duplicated across two of these overloads and is not related to hardware — it belongs in a utility helper, not the driver core.

`initLedImpl` does the actual work for all targets:

```text
initLedImpl(leds, pinsq, numStrips, numLedPerStrip)
  │
  ├─ set members: leds, saveleds, numLedPerStrip, numStrips, offsetDisplay, linewidth
  ├─ compute firstIndexPerOutput[]   (all targets)
  ├─ setShowDelay()                  (all targets)
  ├─ setBrightness(255)              (all targets; allocates + fills LUT tables)
  │
  ├─ [CONFIG_IDF_TARGET_ESP32P4]
  │     setPins(pinsq)              → stores pins[] only; PARLIO routes GPIO itself
  │     hwInit()                    → configures PARLIO TX unit (eager, same as ESP32/S3)
  │     initTransferBuffers()       → allocates PSRAM ping-pong waveform buffers (~328 KB each)
  │     return
  │
  └─ [ESP32 / ESP32-S3]
        setPins(pinsq)             → stores pins[]; routes GPIO through I2S signal matrix
        hwInit()                   → configures I2S/LCD_CAM registers + GDMA channel (S3)
                                     or allocates interrupt handler (ESP32)
        initTransferBuffers()      → allocates dmaBuffersTampon[] ring (ping-pong)
                                     and optionally dmaBuffersTransposed[] (FULL_DMA_BUFFER)
```

### `showPixels()` path

```text
showPixels()  /  showPixels(WAIT)  /  showPixels(NO_WAIT)  /  showPixels(newleds)  / …
  │
  ├─ waitDisplay()          — if isDisplaying: block on waitDisp semaphore until ISR clears it
  ├─ set leds, offsetDisplay, displayMode from arguments
  └─ showPixelsImpl()
       │
       ├─ [CONFIG_IDF_TARGET_ESP32P4]
       │     loadAndTranspose()  — bit-transpose raw LED data into ping-pong buffer
       │     hwStart()           — queue PARLIO transmit, swap buffers
       │     hwStop()            — wait for transmission complete
       │
       └─ [ESP32 / ESP32-S3]
             link DMA descriptor ring (dmaBuffersTampon)
             for buffNum 0 … nbDmaBuffer-2:
               loadAndTranspose(driver)        ← pre-fill ping-pong ring
               │  reads leds[], applies LUTs inline, calls transpose16x1Noinline2(), writes DMA buffer
             hwStart(dmaBuffersTampon[N])      ← arm and start DMA + I2S TX
             if WAIT: xSemaphoreTake(sem)      ← block until ISR signals completion

             ISR (interruptHandler) — fires per DMA buffer completion:
               loadAndTranspose(driver)        ← fill next buffer while previous transmits
               when all LEDs done:
                 hwStop(driver)                ← stop I2S TX, signal sem / waitDisp / semSync
```

### Summary: functions per layer (current state after Phase 5)

| Layer | ESP32-D0 | ESP32-S3 | ESP32-P4 |
|-------|----------|----------|----------|
| Public API | `initled`, `showPixels` | ← same | ← same |
| Colour decode | `applyColorArrangement()` | ← same | ← same |
| Common init | `initLedImpl` | ← same | ← same |
| GPIO routing | `setPins` (I2S matrix) | `setPins` (LCD_CAM signals) | `setPins` (store only) |
| HW peripheral init | `hwInit` | `hwInit` | `hwInit` |
| Buffer allocation | `initTransferBuffers` | `initTransferBuffers` | `initTransferBuffers` |
| Frame transpose | `loadAndTranspose` | `loadAndTranspose` | `loadAndTranspose` |
| LUT + wire-order | inline in `loadAndTranspose` | ← same | `rgbwBufferMapping` |
| HW start | `hwStart` | `hwStart` | `hwStart` |
| HW stop / ISR | `hwStop` + `interruptHandler` | ← same | `hwStop` (synchronous) |

---

## Desired decomposition

The goal is a clean three-target architecture where **all three targets use the same function names** for the same conceptual operations, and **utilities that are not part of the hardware driver** are moved to their own files.  The original `I2SClocklessLedDriver.h/.cpp` naming is the reference — P4 code adopts those names exactly, even where the underlying hardware is different.

### One canonical `initled()` ✅ done (Phase 2)

The main `initled()` is the one with explicit component layout — no `ColorArrangement` argument:

```cpp
// CANONICAL — all other initled() overloads call this one directly or indirectly
void initled(uint8_t* leds, uint8_t* pinsq, uint16_t* sizes, uint8_t numStrips,
             uint8_t channelsPerLight, uint8_t pR, uint8_t pG, uint8_t pB,
             uint8_t pW = UINT8_MAX, uint8_t pW2 = UINT8_MAX,
             bool extractWhiteFromRGB = false);
```

Convenience overloads that translate higher-level arguments into this form are utility wrappers.  They are still public API and stay in `I2SClocklessLedDriver.h`, but they are clearly secondary:

| Overload | What it does | Category |
|----------|-------------|----------|
| `initled(leds, pinsq, sizes[], numStrips, cArr)` | decodes `cArr` → component params, calls canonical | convenience |
| `initled(leds, pinsq, numStrips, numLedPerStrip, cArr)` | builds uniform `sizes[]`, calls `cArr` variant | convenience |
| `initled(pinsq, numStrips, numLedPerStrip, cArr)` | `leds = nullptr`, calls above | convenience |
| `initled(Pixels pix, pinsq)` | extracts buffer/sizes from `Pixels`, calls canonical | convenience |

### File layout after reorg

```text
src/
  I2SClocklessLedDriver.h      — class declaration + method declarations + dispatch shells
  │                               #includes the platform impl header after the class closes
  I2SClocklessLedDriver.cpp    — updateDriver(), deleteDriver()
  colorarrangement.h           — ColorArrangement enum + applyColorArrangement()  ✅ done
  esp32-d0s3_i2s_impl.h        — ESP32-D0 + ESP32-S3: out-of-class method bodies for
  │                               allocateDMABuffer, putdefaultones, hwStart, i2sReset,
  │                               hwStop (static), interruptHandler (static, two variants),
  │                               transpose16x1Noinline2 (static), loadAndTranspose (static)
  │                               S3/ESP32 branches kept via existing #ifdef guards  ✅ done (Phase 8)
  esp32-p4_parlio_impl.h       — ESP32-P4:  out-of-class method bodies  ✅ done (Phase 5)
  pixeltypes.h                 — unchanged
  framebuffer.h                — unchanged
  helper.h                     — unchanged
  HardwareSprite.h/.cpp        — unchanged
  main.cpp                     — dev sketch, unchanged
```

The `_impl.h` suffix signals "out-of-class method body definitions, included into the parent header after the class definition closes".  This is standard C++ practice for splitting large class implementations across files without changing the calling convention.

### Unified function naming (target state — after all phases complete)

Every platform uses the same function name and the same calling convention (class methods, no `driver*` argument).  Platform-specific `#ifdef` guards live inside the `_impl.h` files; `I2SClocklessLedDriver.h` sees only a single set of names.  Names are hardware-neutral — they describe the operation, not the peripheral.

| Concept | ESP32-D0 | ESP32-S3 | ESP32-P4 | Notes |
|---------|----------|----------|----------|-------|
| Configure HW peripheral | `hwInit()` | `hwInit()` | `hwInit()` | All: class method; P4: configures PARLIO unit eagerly in initled (Phase 4) |
| Allocate transfer buffers | `initTransferBuffers()` | `initTransferBuffers()` | `initTransferBuffers()` | All: class method; P4: PSRAM ping-pong buffers |
| Apply LUT + reorder channels | inline in `loadAndTranspose` | ← same | `rgbwBufferMapping()` | Phase 9: extract to shared method for all platforms |
| Compute wire-format buffer | `loadAndTranspose()` | `loadAndTranspose()` | `loadAndTranspose()` | All: class method after Phase 5 |
| Start hardware transfer | `hwStart()` | `hwStart()` | `hwStart()` | Phase 7: ESP32/S3 internalises buffer lookup; all platforms no-arg after Phase 7 |
| Stop hardware / signal done | `hwStop()` | `hwStop()` | `hwStop()` | All: class method after Phase 5 |
| GPIO routing | `setPins(pinsq)` | `setPins(pinsq)` | `setPins(pinsq)` | Already shared |
| Compute frame delay | `setShowDelay()` | `setShowDelay()` | `setShowDelay()` | Already shared |

### `initLedImpl` after all phases — fully unified skeleton

After Phases 4, 5, and 6 all three platforms share the same skeleton with no `#ifdef` in the body — only the method implementations differ (inside the `_impl.h` files):

```cpp
void initLedImpl(uint8_t* leds, uint8_t* pinsq, uint8_t numStrips, uint16_t numLedPerStrip) {
    // — common (all targets) ———————————————————————————
    set members (leds, numStrips, numLedPerStrip, offsets, …)
    compute firstIndexPerOutput[]
    setShowDelay()
    setBrightness(255)
    setPins(pinsq)
    hwInit()               // ESP32/S3: I2S/LCD_CAM + GDMA; P4: PARLIO TX unit (Phase 4)
    initTransferBuffers()  // ESP32/S3: DMA descriptor ring; P4: PSRAM ping-pong buffers
}
```

State after Phase 5 (unified `initLedImpl` tail — no `#ifdef` at call site; platform branching lives inside each method):

```cpp
    setPins(pinsq)
    hwInit()               // P4: PARLIO TX unit config; S3/ESP32: I2S/LCD_CAM registers
    initTransferBuffers()  // P4: PSRAM ping-pong alloc; S3/ESP32: DMA descriptor ring
    initSuccess = !initErrorOccurred && numStrips > 0 && numLedPerStrip > 0
```

### `showPixelsImpl` after all phases — platform leaves only where unavoidable

After Phase 5, the P4 block shrinks to the same logical shape as ESP32/S3.  The remaining `#ifdef` is structural — P4 is synchronous (no ISR), ESP32/S3 is asynchronous (DMA + ISR):

```cpp
void showPixelsImpl() {
    guard checks (enableDriver, initSuccess, leds != NULL)
#ifdef CONFIG_IDF_TARGET_ESP32P4
    loadAndTranspose()  // build waveform into active ping-pong buffer
    hwStart()           // swap buffers, chunk + transmit via PARLIO
    hwStop()            // wait for completion (synchronous on P4)
    isDisplaying = false
#else  // ESP32 / S3
    link DMA descriptor ring
    for nbDmaBuffer-1 buffers: loadAndTranspose()
    hwStart(dmaBuffersTampon[N])
    if WAIT: xSemaphoreTake(sem)
    // ISR fires loadAndTranspose() and finally hwStop() asynchronously
#endif
}
```

Note: the `if (::hwInit(this)) { return; }` warm-up guard that currently lives in the P4 path of `showPixelsImpl` is removed by Phase 4 (eager init moves to `initLedImpl`).

### What leaves `I2SClocklessLedDriver.h`

- **`ColorArrangement` enum and `switch(cArr)` decoder** ✅ (Phase 2): moved to `src/colorarrangement.h`.
- **P4 PARLIO method bodies** (Phase 5): move from `parlio_p4.cpp` to `esp32-p4_parlio_impl.h`; free-function declarations in `parlio_p4.h` replaced by class method declarations in the class body.
- **ESP32/S3 I2S method bodies** (Phase 6): `hwInit`, `initTransferBuffers`, `allocateDMABuffer`, `hwStart`, `hwStop`, `loadAndTranspose`, `interruptHandler`, `transpose16x1Noinline2` move to `i2s_esp32_impl.h` / `i2s_esp32s3_impl.h`.  The class retains the declarations.

### What stays in `I2SClocklessLedDriver.h`

- Class definition and all public member variables.
- The canonical `initled()` and all convenience overloads (public API surface).
- All `showPixels()` overloads (public API surface).
- `initLedImpl()`, `showPixelsImpl()` — thin dispatch shells after reorg.
- `setBrightness()`, `setGamma()`, `setPins()`, `setShowDelay()`, `setMapLed()` — act on class members only; no hardware dependency.
- `updateDriver()` / `deleteDriver()` declarations (implementations in `.cpp`).

---

## Virtual driver

The virtual driver multiplexes up to 120 LED strips on a single ESP32/ESP32-S3 using 74HC595 shift registers — 8 virtual strips per physical GPIO pin across up to 15 pins.  P4 support is planned for the future.  Source: [I2SClocklessVirtualLedDriver](https://github.com/hpwit/I2SClocklessVirtualLedDriver).

### How it fits the existing architecture

The virtual driver is **not a new target chip** — it is a **different hardware mode** of the same I2S peripheral on ESP32/S3.  It hooks into the same call chain at exactly two points:

| Hook point | Regular driver | Virtual driver |
|------------|---------------|----------------|
| `hwInit()` | Configure I2S for direct parallel output | Configure I2S for shift-register-encoded output (includes clock/latch timing) |
| `loadAndTranspose()` | Encode one LED column into 16-bit I2S words | Encode one LED column × `virtualStripsPerPin` into shift-register I2S words |

Everything else — `initled`, `showPixels`, `initTransferBuffers`, `hwStart`, `hwStop`, `setBrightness`, `updateDriver` — is identical.  The branching is `if (isVirtualDriver)` inside those two functions, not a new platform `#ifdef`.

### Public API intent

The goal is that the caller uses the **same `initled()` they already know**, just after setting three extra members:

```cpp
driver.isVirtualDriver    = true;
driver.clockPin           = 10;   // 74HC245 clock GPIO
driver.latchPin           = 11;   // 74HC245 latch GPIO
// virtualStripsPerPin defaults to 8; override if using a different shift register
driver.initled(leds, physicalPins, numPhysicalPins, numLedPerStrip, ORDER_GRB);
```

`initled()` detects `isVirtualDriver` and routes through the virtual `hwInit()` and `loadAndTranspose()` paths internally.  No new public function is needed.

### What is already in place

- `isVirtualDriver` flag on the class.
- Three reserved members: `virtualStripsPerPin` (default 0 = not configured), `clockPin`, `latchPin`.

### What will be added later

1. `src/virtual_driver_impl.h` — `I2SClocklessLedDriver::virtualHwInit()` and `I2SClocklessLedDriver::virtualLoadAndTranspose()`, following the same `_impl.h` out-of-class method pattern as Phase 5 (P4) and Phase 6 (ESP32/S3).
2. `hwInit()` / `loadAndTranspose()` branches: `if (isVirtualDriver) virtual…(this); else { /* existing */ }`.
3. Utilities (palette rendering, pixel pusher, scanline interrupts, dual-core helpers) — added after core virtual driver works; not part of `I2SClocklessLedDriver` itself.

No reorg is needed — the `if (isVirtualDriver)` branches fit cleanly into the `hwInit`/`loadAndTranspose` class method structure that Phase 5 (P4) and Phase 6 (ESP32/S3) produce.

---

## Phased implementation plan

Each phase is independently buildable and testable; no phase breaks the public API.

**Why renaming comes first:** Phase 1 renames the existing ESP32/S3 functions to hardware-neutral names.  Every later phase then writes code using the correct final names from day one — no double-rename, no transitional wrong-name step on P4, no extracted files that need immediate follow-up renaming.  The renaming decision is the vocabulary for the entire reorg.

### Phase 1 — Rename to hardware-neutral function names (ESP32/S3) ✅ done

*Goal:* Establish the final vocabulary before writing any new code.  Pure rename of existing ESP32/S3 functions — no logic change, no structural change.  All platforms will use these names after Phase 3.

| Old name (ESP32/S3) | Why semantically wrong across targets | New name | Notes |
|---------------------|---------------------------------------|----------|-------|
| `i2sInit()` | No I2S on P4; uses PARLIO | `hwInit()` | "hw" = hardware peripheral, neutral |
| `i2sStart()` | No I2S start register on P4 | `hwStart()` | Triggers DMA / PARLIO transfer |
| `i2sStop()` | No I2S stop register on P4 | `hwStop()` | Waits for / signals transfer completion |
| `initDMABuffers()` | P4 buffers are PSRAM waveform arrays, not DMA descriptors | `initTransferBuffers()` | Covers descriptor rings and flat waveform buffers |
| `i2sResetDma()` | ESP32-D0 only; meaningless on S3 and P4 | keep as ESP32-D0 internal | Only used inside `hwInit()` on ESP32-D0; no cross-platform call |
| `i2sResetFifo()` | Same | keep as ESP32-D0 internal | Same |

Names left unchanged because they describe the operation, not the hardware:

| Name | Rationale |
|------|-----------|
| `loadAndTranspose()` | Accurately describes bit-parallel transposition on all targets |
| `rgbwBufferMapping()` | Accurately describes LUT application and channel reordering |
| `setPins()` | Already hardware-neutral |
| `setShowDelay()` | Already hardware-neutral |

`dmaBuffersTampon` (member variable, ESP32/S3) is a candidate for `transferBuffers` but low priority — member variable renames touch more code and are deferred to Phase 5.

### Phase 2 — Establish canonical `initled()` and move `ColorArrangement` out ✅ done

*Goal:* The driver file contains hardware code only.  The `cArr` convenience layer is explicit and separate.

1. Create `src/colorarrangement.h`:
   - The `ColorArrangement` enum (same values, same names — no breaking change for existing users).
   - Free function `void applyColorArrangement(ColorArrangement cArr, uint8_t& channelsPerLight, uint8_t& pR, uint8_t& pG, uint8_t& pB, uint8_t& pW, uint8_t& pW2)` with the current switch-case body.
2. Promote `initled(leds, pinsq, sizes[], numStrips, channelsPerLight, pR, pG, pB, pW, pW2)` as the **canonical** form — add a prominent comment marking it as the primary entry point.
3. Replace the duplicated `switch(cArr)` blocks in the convenience overloads with `applyColorArrangement(…)`.
4. In `I2SClocklessLedDriver.h`: `#include "colorarrangement.h"`.

### Phase 3 — Unify P4 function names (using the Phase 1 vocabulary) ✅ done

*Goal:* `parlio_p4.h/.cpp` exposes `hwInit`, `initTransferBuffers`, `loadAndTranspose`, `hwStart`, `hwStop` — the same final names as the now-renamed ESP32/S3 code.  `hpwit` reads one set of names across all three platform files with no `i2s`/`parlio` prefix confusion.

| Change | From (current P4) | To |
|--------|-------------------|----|
| Monolithic show function | `show_parlio_p4(driver, pins, …)` | split into `hwInit`, `initTransferBuffers`, `loadAndTranspose`, `hwStart`, `hwStop` |
| Transpose pass | `create_transposed_led_output_optimized(…)` | `loadAndTranspose(driver)` |
| HW peripheral config | (inline in `show_parlio_p4`) | `hwInit(driver)` |
| Buffer allocation | (inline in `initLedImpl`) | `initTransferBuffers(driver)` |
| HW start (transmit) | (inline in `show_parlio_p4`) | `hwStart(driver)` |
| HW stop (wait done) | (inline in `show_parlio_p4`) | `hwStop(driver)` |
| Call site in `showPixelsImpl` | `show_parlio_p4(this, pins, …)` | `hwInit(this)` + `loadAndTranspose(this)` + `hwStart(this)` + `hwStop(this)` |
| Call site in `initLedImpl` (P4) | inline buffer alloc | `initTransferBuffers(this)` |

P4-specific class members keep their `p4` prefix (`p4TxUnit`, `p4Config`, `p4Buffer1/2`, `p4BufferActive`) — the prefix clearly marks them as PARLIO implementation details, not shared state.

### Phase 4 — Move P4 hardware init from `showPixels` to `initled` ✅ done

*Goal:* P4 follows the same eager-init pattern as ESP32/S3 — `initled` fully configures the hardware, `showPixels` only transmits.  The lazy-init warm-up guard is removed from `showPixelsImpl`.

Currently P4 defers `hwInit` to the first `showPixels` call (lazy) and skips that frame as a warm-up.  ESP32/S3 calls `hwInit()` eagerly in `initLedImpl`.

Changes:
1. In `initLedImpl` (P4 branch): add `::hwInit(this)` before `::initTransferBuffers(this)` — same position as ESP32/S3.
2. In `updateDriver()` (P4 branch): add `::hwInit(this)` after updating topology — mirrors how ESP32/S3 `updateDriver()` reconfigures I2S when strip count or LED count changes.
3. In `showPixelsImpl` (P4 branch): remove the `if (::hwInit(this)) { … return; }` warm-up guard.  P4's block reduces to: `::loadAndTranspose(this); ::hwStart(this); ::hwStop(this); isDisplaying = false;`
4. Remove members that only existed to support lazy init: `forceParlioReconfig` (if present), `p4LastOutputs`, `p4LastLedsPerOutput`, `p4LastPins` — all removed during Phase 4/5 implementation.

Result: `initLedImpl` and `updateDriver` become structurally identical across all three targets.  `showPixelsImpl` P4 branch is now three lines + `isDisplaying = false`, matching the logical shape of the ESP32/S3 path.

### Phase 5 — Convert P4 free functions to class methods ✅ done

*Goal:* P4 functions (`hwInit`, `initTransferBuffers`, `loadAndTranspose`, `hwStart`, `hwStop`) become class methods of `I2SClocklessLedDriver`.  All `driver->` dereferences become `this->`.  The `::` scope-resolution prefix in P4 call sites is removed.  This phase establishes the `_impl.h` class-method pattern on P4 first — Phase 6 then applies the same pattern to ESP32/S3.

Changes:
1. Add `#elif CONFIG_IDF_TARGET_ESP32P4` branches to the existing inline `hwInit()` and `initTransferBuffers()` class methods — P4 body inline alongside the existing S3/ESP32 branches.
2. Add P4-only method declarations to the class body (guarded by `#ifdef CONFIG_IDF_TARGET_ESP32P4`): `void loadAndTranspose()`, `void hwStart()`, `void hwStop()`.
3. Create `src/esp32-p4_parlio_impl.h` with `inline` out-of-class definitions for those three methods, plus all PARLIO helpers (`LedMatrixDetail` namespace, `rgbwBufferMapping`, `create_transposed_led_output_optimized`, `transmit_config`).
4. Replace `#include "parlio_p4.h"` in the P4 includes block with the `PARLIO_P4_BUFFER_BYTES` constant definition.
5. Add `#include "esp32-p4_parlio_impl.h"` at the bottom of `I2SClocklessLedDriver.h` inside `#ifdef CONFIG_IDF_TARGET_ESP32P4`.
6. Delete `parlio_p4.h` and `parlio_p4.cpp` — all content now lives in the header via inline branches and `esp32-p4_parlio_impl.h`.
7. In `showPixelsImpl`, `initLedImpl`, and `updateDriver`: replace `::hwInit(this)`, `::loadAndTranspose(this)`, etc. with plain `hwInit()`, `loadAndTranspose()`, etc.

Result: all three targets call the same names with the same syntax.  The `::` workaround introduced in Phase 3 is gone.  `I2SClocklessLedDriver.h` bottom section becomes (after Phase 8):
```cpp
#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32
  #include "esp32-d0s3_i2s_impl.h"       // ✅ Phase 8
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
  #include "esp32-p4_parlio_impl.h" // ✅ this phase
#endif
```

### Phase 6 — Unify `updateDriver()` ✅ done

*Goal:* Remove the P4-specific `#ifdef` block from `updateDriver()` so it mirrors `initLedImpl` — one common body, platform differences hidden inside `deleteDriver()`, `hwInit()`, and `initTransferBuffers()`.

Changes made:
1. `deleteDriver()` on ESP32/S3 now tears down hardware before freeing buffers:
   - S3: `gdma_disconnect(dmaChan)` + `gdma_del_channel(dmaChan)` — prevents the DMA engine from accessing freed memory; makes `hwInit()` safe to call again.
   - ESP32: `esp_intr_free(intrHandle)` — releases the I2S interrupt handler so it can be reinstalled.
2. `updateDriver()` is now a single unified body matching `initLedImpl`:
   ```text
   validate args
   compute newNumLedPerStrip          ← before deleteDriver sees the old value
   if isDisplaying: wait (semaphore)  ← no-op on P4; isDisplaying is always false there
   deleteDriver()                     ← all platforms: tears down HW + frees buffers
   update topology (common)
   setShowDelay() + setPins()
   hwInit()                           ← all platforms: reconfigures HW peripheral
   initTransferBuffers()              ← all platforms: allocates buffers
   setBrightness() + initSuccess
   ```
3. Fixed two pre-existing P4 bugs found during analysis: missing `setShowDelay()` call and inline `pins[i] = pinsq[i]` instead of `setPins(pinsq)`.

### Phase 7 — Align function signatures and variable names ✅ done

*Goal:* Every platform calls the same functions with the same signatures — no `#ifdef` at the call site and no argument-type differences.

#### `hwStart` unification

`hwStart` on ESP32/S3 now takes no argument. The body resolves the start-of-chain DMA buffer internally using the existing `transpose` flag:

```cpp
void hwStart() {
  #ifdef FULL_DMA_BUFFER
  I2SClocklessLedDriverDMABuffer* startBuffer = transpose ? transferBuffers[nbDmaBuffer] : dmaBuffersTransposed[0];
  #else
  I2SClocklessLedDriverDMABuffer* startBuffer = transferBuffers[nbDmaBuffer];
  #endif
  // ... S3/ESP32 DMA start code ...
}
```

Both call sites (`showPixelsImpl` normal path and `FULL_DMA_BUFFER` path) now call plain `hwStart()` with no argument and no `#ifdef` at the call site.

#### Variable name alignment

| Concept | Before | After | Notes |
|---------|--------|-------|-------|
| Ping-pong buffer array (ESP32/S3) | `dmaBuffersTampon[]` | `transferBuffers[]` | renamed throughout |
| Ping-pong buffers (P4) | `p4Buffer1`, `p4Buffer2` | unchanged | types differ; names converge conceptually |
| Active ping-pong pointer | `dmaBufferActive` (index) / `p4BufferActive` (ptr) | unchanged | types differ; documented analogy |
| `hwStart` argument | `DMABuffer*` on ESP32/S3, none on P4 | none on all platforms | buffer lookup moved inside `hwStart` |

### Phase 8 — Extract ESP32/S3 method bodies to platform impl headers ✅ done

*Goal:* `I2SClocklessLedDriver.h` shrinks to class declaration + method declarations + thin dispatch block.  The large ESP32/S3 function bodies move to a single `esp32-d0s3_i2s_impl.h` that is `#include`d back after the class definition closes, following the pattern established by Phase 5.

What was done:
1. Created `src/esp32-d0s3_i2s_impl.h` containing out-of-class definitions for both S3 and ESP32 (existing `#ifdef` guards separate the two): `allocateDMABuffer()`, `putdefaultones()`, `hwStart()`, `i2sReset()` as `inline I2SClocklessLedDriver::` methods; plus `hwStop()`, `interruptHandler()` (two variants), `transpose16x1Noinline2()`, `loadAndTranspose()` as static free functions.
2. Replaced inline bodies in the class body with bare declarations inside `#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32`.
3. Replaced the old `#ifndef CONFIG_IDF_TARGET_ESP32P4 … #else #include "esp32-p4_parlio_impl.h" #endif` dispatch with:
   ```cpp
   #if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32
     #include "esp32-d0s3_i2s_impl.h"
   #elif defined(CONFIG_IDF_TARGET_ESP32P4)
     #include "esp32-p4_parlio_impl.h"
   #endif
   ```
4. `hwInit()` and `initTransferBuffers()` kept in the class body (they have P4 branches and are smaller than the extracted functions).

Result: `I2SClocklessLedDriver.h` dropped from ~2100 lines to ~1660 lines; 458 lines extracted to `esp32-d0s3_i2s_impl.h`.

### Phase 9 — Common LUT application layer ✅ done

*Goal:* The per-pixel LUT+channel-reorder logic (`rgbwBufferMapping` on P4, inline code in `loadAndTranspose` on ESP32/S3) is written once as a shared method.

What was done:
1. Added `inline void rgbwBufferMapping(const uint8_t* src, uint8_t* dst) const` to the class body (no platform guard). Reads raw R,G,B[,W[,W2]] from `src`, applies white extraction, LUT tables, and channel reorder, writes mapped values to `dst[pR/pG/pB/pW/pW2]`.
2. In `esp32-d0s3_i2s_impl.h` `loadAndTranspose`: replaced the ~15-line inline LUT block with `driver->rgbwBufferMapping(poli, mapped)` + a loop copying `mapped[c]` into `secondPixel[c].bytes[i]`.
3. In `esp32-p4_parlio_impl.h`: removed `rgbwBufferMapping` static free function (now a shared class method); replaced its call in `create_transposed_led_output_optimized` with `driver->rgbwBufferMapping(...)`; dropped the now-unused `offsetR/G/B/W/W2` parameters from `create_transposed_led_output_optimized` and its call site.

Note: `rgbwBufferMapping` is `inline` so the compiler will typically inline it into `loadAndTranspose` (ISR context on ESP32/S3) with no call overhead. Benchmark before/after to confirm timing is unchanged.

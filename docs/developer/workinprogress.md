# Working in Progress — Repository Reorganization

This document tracks the ongoing repository reorganization effort to unify platform-specific code (ESP32-D0, ESP32-S3, ESP32-P4) under a common vocabulary and clean architecture.

**Status: Phases 1, 2, 3 completed. Phases 4, 5, 6 pending.**

---

## Current call path (reverse-engineered)

The two public entry points are `initled()` and `showPixels()`. Every other function is called from these two.  The description below follows the code top-down for all three targets (ESP32-D0, ESP32-S3, ESP32-P4).

### `initled()` path

There are currently many `initled()` overloads.  The canonical one — the lowest-level call that all others eventually reach — takes explicit component layout parameters:

```text
initled(leds, pinsq, sizes[], numStrips, nbComponents, pR, pG, pB, pW, pW2)   ← canonical / main
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
  │     initTransferBuffers()       → allocates PSRAM ping-pong waveform buffers (~328 KB each)
  │     return                       (no hwInit, no DMA descriptors)
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
       │     hwInit(this)            — lazy: only if topology changed
       │     loadAndTranspose(this)  — bit-transpose raw LED data into ping-pong buffer
       │     hwStart(this)           — queue PARLIO transmit, swap buffers
       │     hwStop(this)            — wait for transmission complete
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

### Summary: functions per layer (current state after Phase 3)

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
             uint8_t nbComponents, uint8_t pR, uint8_t pG, uint8_t pB,
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
  I2SClocklessLedDriver.h      — class + canonical initled + showPixels + dispatch shells
  I2SClocklessLedDriver.cpp    — updateDriver(), deleteDriver()
  colorarrangement.h           — ColorArrangement enum + applyColorArrangement()  ✅ done
  i2s_esp32.h                  — ESP32-D0:  hwInit, initTransferBuffers, loadAndTranspose,
  │                               hwStart, hwStop, interruptHandler  (Phase 4)
  i2s_esp32s3.h                — ESP32-S3:  same function names, different register code  (Phase 4)
  parlio_p4.h                  — ESP32-P4:  hwInit, initTransferBuffers, loadAndTranspose,
  │                               hwStart, hwStop  ✅ done (Phase 3)
  parlio_p4.cpp                — ESP32-P4:  implementation  ✅ done (Phase 3)
  pixeltypes.h                 — unchanged
  framebuffer.h                — unchanged
  helper.h                     — unchanged
  HardwareSprite.h/.cpp        — unchanged
  main.cpp                     — dev sketch, unchanged
```

### Unified function naming (target state — after all phases complete)

Every platform uses the same function name for the same conceptual step.  The `#ifdef` guards are inside the platform-specific files; `I2SClocklessLedDriver.h` sees only a single set of names.  Names are hardware-neutral so they describe the operation, not the peripheral (`hw` rather than `i2s` or `parlio`).

| Concept | ESP32-D0 | ESP32-S3 | ESP32-P4 | Notes |
|---------|----------|----------|----------|-------|
| Configure HW peripheral | `hwInit()` | `hwInit()` | `hwInit(driver)` | P4: configures PARLIO unit (lazy; only when topology changes) |
| Allocate transfer buffers | `initTransferBuffers()` | `initTransferBuffers()` | `initTransferBuffers(driver)` | P4: allocates PSRAM ping-pong waveform buffers |
| Apply LUT + reorder channels | inline in `loadAndTranspose` | ← same | `rgbwBufferMapping()` | Phase 6: extract to shared free function for all platforms |
| Compute wire-format buffer | `loadAndTranspose(driver)` | `loadAndTranspose(driver)` | `loadAndTranspose(driver)` | P4: direct free function |
| Start hardware transfer | `hwStart(buffer)` | `hwStart(buffer)` | `hwStart(driver)` | P4: PARLIO chunk+transmit; waits for previous frame first |
| Stop hardware / signal done | `hwStop(driver)` | `hwStop(driver)` | `hwStop(driver)` | P4: synchronous `wait_all_done`; no ISR needed |
| GPIO routing | `setPins(pinsq)` | `setPins(pinsq)` | `setPins(pinsq)` | Already shared; P4 stores pins only |
| Compute frame delay | `setShowDelay()` | `setShowDelay()` | `setShowDelay()` | Already shared |

### `initLedImpl` after reorg — common skeleton, platform-specific leaves

```cpp
void initLedImpl(uint8_t* leds, uint8_t* pinsq, uint8_t numStrips, uint16_t numLedPerStrip) {
    // — common —————————————————————————————————————————
    set members (leds, numStrips, numLedPerStrip, offsets, …)
    compute firstIndexPerOutput[]
    setShowDelay()
    setBrightness(255)
    setPins(pinsq)
    // — platform-specific ——————————————————————————————
#ifdef CONFIG_IDF_TARGET_ESP32P4
    initTransferBuffers(this)  // allocates PSRAM ping-pong waveform buffers
    // hwInit deferred to first showPixels — topology may change before first show
#else  // ESP32 / S3
    hwInit()
    initTransferBuffers()
#endif
}
```

### `showPixelsImpl` after reorg — common skeleton, platform-specific leaves

```cpp
void showPixelsImpl() {
    guard checks (enableDriver, initSuccess, leds != NULL)
#ifdef CONFIG_IDF_TARGET_ESP32P4
    if (hwInit(this)) {
      // PARLIO was reconfigured — skip this frame as warm-up
      isDisplaying = false;
      return;
    }
    loadAndTranspose(this)  // build waveform into active ping-pong buffer
    hwStart(this)           // swap buffers, chunk + transmit via PARLIO
    hwStop(this)            // wait for completion (synchronous on P4)
    isDisplaying = false
#else  // ESP32 / S3
    link DMA descriptor ring
    for nbDmaBuffer-1 buffers: loadAndTranspose(this)
    hwStart(dmaBuffersTampon[N])
    if WAIT: xSemaphoreTake(sem)
    // ISR fires loadAndTranspose() and finally hwStop() asynchronously
#endif
}
```

### What leaves `I2SClocklessLedDriver.h`

- **`ColorArrangement` enum and `switch(cArr)` decoder** ✅ (Phase 2): moved to `src/colorarrangement.h` as a free function `applyColorArrangement(cArr, &nbComponents, &pR, &pG, &pB, &pW, &pW2)`.  The convenience `initled(…, cArr)` overload calls it.
- **ESP32/S3 I2S implementation** (Phase 4, pending): `hwInit`, `initTransferBuffers`, `allocateDMABuffer`, `hwStart`, `hwStop`, `loadAndTranspose`, `interruptHandler`, `transpose16x1Noinline2` and all their register-level code move to `i2s_esp32.h` / `i2s_esp32s3.h`, mirroring `parlio_p4.h/.cpp`.

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

1. `src/virtual_driver.h` — `virtualHwInit(driver)` and `virtualLoadAndTranspose(driver)`, following the same free-function-taking-`driver*` pattern as `parlio_p4.cpp`.
2. `hwInit()` / `loadAndTranspose()` branches: `if (isVirtualDriver) virtual…(this); else { /* existing */ }`.
3. Utilities (palette rendering, pixel pusher, scanline interrupts, dual-core helpers) — added after core virtual driver works; not part of `I2SClocklessLedDriver` itself.

No reorg is needed — the `if (isVirtualDriver)` branches fit cleanly into the `hwInit`/`loadAndTranspose` structure that Phase 4 (Extract ESP32/S3 code) produces.

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
   - Free function `void applyColorArrangement(ColorArrangement cArr, uint8_t& nbComponents, uint8_t& pR, uint8_t& pG, uint8_t& pB, uint8_t& pW, uint8_t& pW2)` with the current switch-case body.
2. Promote `initled(leds, pinsq, sizes[], numStrips, nbComponents, pR, pG, pB, pW, pW2)` as the **canonical** form — add a prominent comment marking it as the primary entry point.
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

P4-specific class members keep their `p4` prefix (`p4TxUnit`, `p4Config`, `p4Buffer1/2`, `p4BufferActive`, `p4LastOutputs`, `p4LastLedsPerOutput`) — the prefix clearly marks them as PARLIO implementation details, not shared state.

### Phase 4 — Extract ESP32/S3 hardware code into separate headers (pending)

*Goal:* `I2SClocklessLedDriver.h` becomes a thin dispatch shell; all three platforms' implementation files sit side by side.  Because Phases 1–3 have established correct names, the extracted files are clean from the start.

1. Create `src/i2s_esp32s3.h` (under `#ifdef CONFIG_IDF_TARGET_ESP32S3`):
   - `hwInit()`, `initTransferBuffers()`, `allocateDMABuffer()`, `hwStart()`, `hwStop()`
   - `transpose16x1Noinline2()`, `loadAndTranspose()`
   - `interruptHandler()`
   - All S3-specific `#define`s and register-level code.
2. Create `src/i2s_esp32.h` with the equivalent ESP32-D0 code (including `i2sResetDma`, `i2sResetFifo` as ESP32-internal helpers).
3. Replace the large `#ifdef` bodies in `I2SClocklessLedDriver.h` with:
   ```cpp
   #ifdef CONFIG_IDF_TARGET_ESP32S3
     #include "i2s_esp32s3.h"
   #elif CONFIG_IDF_TARGET_ESP32
     #include "i2s_esp32.h"
   #elif CONFIG_IDF_TARGET_ESP32P4
     #include "parlio_p4.h"
   #endif
   ```
4. Functions that access state only via the `driver` pointer (e.g., `hwStop`) become free functions taking `I2SClocklessLedDriver*` — matching the existing pattern in `parlio_p4.cpp`.

### Phase 5 — Align variable names and buffer model (pending)

*Goal:* The same conceptual entity has the same name across all three targets.

| Concept | ESP32/S3 current | P4 current | Desired (all targets) |
|---------|-----------------|------------|-----------------------|
| Ping-pong buffer array | `dmaBuffersTampon[]` | `p4Buffer1`, `p4Buffer2` | `transferBuffers[]` or document analogy; types differ |
| Active ping-pong pointer | `dmaBufferActive` (uint8 index) | `p4BufferActive` (uint16_t*) | keep both; types differ (index vs pointer) |
| "Is a frame in flight?" | `isDisplaying` | `isDisplaying` | already unified |
| Max LEDs per strip | `numLedPerStrip` | `numLedPerStrip` | already unified |
| Per-strip sizes | `stripSize[]` | `stripSize[]` | already unified |
| Per-strip byte offsets | `firstIndexPerOutput[]` | `firstIndexPerOutput[]` | already unified |

Lower priority — the differences cause no bugs.  Rename only where it clearly aids readability.

### Phase 6 — Common LUT application layer (pending)

*Goal:* The per-pixel LUT+channel-reorder logic (`rgbwBufferMapping` on P4, inline code in `loadAndTranspose` on ESP32/S3) is written once as a shared free function.

`rgbwBufferMapping(driver, inputRGBW, outputWireOrder)` is already defined in `parlio_p4.cpp`.  ESP32/S3 `loadAndTranspose` should call it rather than duplicating the LUT indexing inline.  Requires ISR-path benchmarking on real hardware before committing, since `loadAndTranspose` runs in interrupt context on ESP32/S3 and any extra function-call overhead must be measured.

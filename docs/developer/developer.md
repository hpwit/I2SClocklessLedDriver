# Developer Guide

## Repository layout

| Path | Purpose |
|------|---------|
| `src/I2SClocklessLedDriver.h` | Full driver class + all static ISR/transpose functions |
| `src/I2SClocklessLedDriver.cpp` | `updateDriver()` / `deleteDriver()` implementations |
| `src/parlio_p4.h` | ESP32-P4 PARLIO driver — declaration (included by the main header under `CONFIG_IDF_TARGET_ESP32P4`) |
| `src/parlio_p4.cpp` | ESP32-P4 PARLIO driver — implementation (bit-transposition, DMA chunking, LUT mapping) |
| `src/pixeltypes.h` | `Pixel` struct and `Pixels` container (used when `USE_PIXELSLIB` is not set) |
| `src/framebuffer.h` | Simple double-buffer helper |
| `src/HardwareSprite.h/.cpp` | Hardware sprite overlay (opt-in with `HARDWARESPRITES 1`) |
| `src/helper.h` | Timing macros (`HOW_LONG`, `RUN_SKETCH_FOR`, `RUN_SKETCH_N_TIMES`) |
| `src/main.cpp` | Development sketch — excluded from library builds via `library.json` |

---

## DMA pipeline

Each LED bit is encoded as 3 I2S clock ticks: `100` = 0-bit, `110` = 1-bit. The I2S peripheral outputs 16 bits in parallel at each tick, so one tick drives one bit of all 16 strips simultaneously.

**Transposition** converts the per-strip linear layout (`strip0_led0, strip0_led1, …, strip1_led0, …`) into the parallel 16-bit I2S word format. `transpose16x1_noinline2()` in the header handles one 16-strip × 8-bit block using a bitwise in-place matrix transpose.

### Ping-pong mode (default)

Two small DMA buffers (`dmaBuffersTampon[0..N+1]`) are filled one at a time by the ISR. Each ISR call transposes and loads the next LED into the currently-idle buffer. This uses minimal RAM but requires an ISR call per LED column.

The ISR (`interruptHandler` on S3, `interruptHandler` on ESP32) is `IRAM_ATTR`-placed and only uses ISR-safe FreeRTOS primitives.

### Full DMA buffer mode (`FULL_DMA_BUFFER`)

`transposeAll()` pre-transposes the entire frame into `dmaBuffersTransposed[0..num_led_per_strip+1]` before the transfer starts. The I2S then runs through the entire chain without interrupts, freeing the CPU completely.

This also enables the `LOOP` display mode: the last DMA descriptor points back to the first, creating a continuous ring that the DMA/I2S hardware replays indefinitely.

---

## Platform branching

All hardware-specific code is guarded by the target defines injected by PlatformIO:

| Define | Peripheral used |
|--------|----------------|
| `CONFIG_IDF_TARGET_ESP32S3` | LCD_CAM + GDMA (`gdma_new_ahb_channel`) |
| `CONFIG_IDF_TARGET_ESP32` | I2S0 + `esp_intr_alloc` |
| `CONFIG_IDF_TARGET_ESP32P4` | PARLIO TX (`parlio_tx_unit`) — see [ESP32-P4 PARLIO driver](#esp32-p4-parlio-driver) |

---

## ESP32-P4 PARLIO driver

The ESP32-P4 does not have an I2S peripheral, so the parallel LED output is driven by the **PARLIO TX** (Parallel IO) hardware unit.  The implementation lives in `src/parlio_p4.h` / `src/parlio_p4.cpp` and is called transparently from the same `initled()` + `showPixels()` API.

### How it works

Instead of filling a DMA descriptor ring (ESP32/S3 approach), the P4 driver:

1. **Transposes** the raw `leds[]` byte buffer into a packed waveform buffer (`parallel_buffer_repacked`), applying brightness/gamma LUT tables for every channel in the same pass.
2. **Encodes** each LED bit as 4 clock cycles (`1000` = 0-bit, `1110` = 1-bit at 800 kHz × 4 = 3.2 MHz clock).  Both nibbles of a byte are looked up simultaneously via a 256-entry `waveform_cache[]`.
3. **Packs** the per-pin bits into the PARLIO data width (1/2/4/8/16-bit) using optimised `process_Nbit()` helpers in the `LedMatrixDetail` namespace.
4. **Chunks** the output into ≤65535-byte transfers to respect the PARLIO DMA hardware limit, queuing up to 4 chunks per frame.
5. **Ping-pongs** between two waveform buffers so the CPU can build the next frame while the PARLIO unit streams the current one.

### Variable strip lengths (padding — feature by @ewowi)

When strips have different lengths (`leds_per_output[]`), the transposition loop runs for `max_leds_per_output` positions.  Pins whose strip is shorter than the maximum are **zero-padded** — they output black (`0x00`) for the extra positions instead of transmitting stale data.

`first_index_per_output[]` tracks the byte-offset of each strip's first pixel in the flat `leds[]` buffer.

### RGBCCT warm-white support (feature by @ewowi)

A fifth colour channel (`offsetW2` / `pW2`) is supported for RGBCCT strips.  The warm-white channel uses the `white2Map` LUT (separate brightness/gamma curve from the cool-white `whiteMap`).

> **Bug fixed vs original parlio.cpp**: the original code applied `whiteMap` (cool-white LUT) to the warm-white channel; `parlio_p4.cpp` correctly uses `white2Map`.

### Attribution

The PARLIO approach was originally developed by **@troyhacks** and extended with variable-length padding and RGBCCT support by **@ewowi** in the [MoonModules/MoonLight](https://github.com/MoonModules/MoonLight) project.  It was adapted for standalone use (no MoonLight dependencies, explicit driver pointer instead of `extern ledsDriver`) and integrated into I2SClocklessLedDriver by **@ewowi**.

### `initLedImpl()` on P4

On ESP32-P4, `initLedImpl()` stores the pins in `pins[]` and calls `setBrightness()` to initialise the LUT tables, then allocates the two ping-pong waveform buffers (`p4Buffer1`, `p4Buffer2`) in PSRAM.  The PARLIO unit is created lazily on the first `showPixels()` call.

### `updateDriver()` on P4

There is no DMA transfer to quiesce.  `updateDriver()` updates the pin array, strip sizes, colour order, and brightness LUTs, then returns.  The PARLIO unit detects the topology change on the next `showPixels()` call and reconfigures.

### `deleteDriver()` on P4

`deleteDriver()` stops and deletes the PARLIO TX unit (if it was ever created) and frees the two ping-pong waveform buffers (`p4Buffer1`, `p4Buffer2`).  No I2S DMA descriptor rings are involved.

---

## IDF version branching

Remaining `#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)` checks all cover genuine API changes:

- **Header locations** — `<esp_private/gpio.h>` (≥ 5.5) vs `<driver/gpio.h>` (< 5.5)
- **GPIO mux function** (S3) — `gpio_iomux_output()` vs `gpio_iomux_out()`
- **GDMA alloc** (S3) — `gdma_new_ahb_channel()` vs `gdma_new_channel()` (`isr_cache_safe=true` crashes on 5.5+)

All other previously version-gated code (`NUM_STRIPS`, `__NB_DMA_BUFFER`, `__delay`, `DMABuffersTampon`, `updateDriver`, `deleteDriver`, transpose runtime check) is now unconditional — the 5.5+ design (runtime variables, dynamic allocation) is used on all supported IDF versions.

---

## Semaphores

Three FreeRTOS semaphores live on the driver object:

| Member | Purpose |
|--------|---------|
| `sem` | Blocks `showPixels(WAIT)` until transfer done |
| `semSync` | Frame-sync signal for `waitSync()` |
| `waitDisp` | Lazy-created; used by `showPixels(NO_WAIT)`, `waitDisplay()`, and `updateDriver()` to wait for an in-flight transfer before proceeding |

`wasWaitingtofinish` is a flag set by any caller that is about to block on `waitDisp`. The ISR checks the flag in `hwStop()` and only calls `xSemaphoreGiveFromISR` when a waiter is present, preventing spurious semaphore count accumulation.

All semaphore operations inside `i2sStop()` use `xSemaphoreGiveFromISR` + `portYIELD_FROM_ISR`, as required for ISR context.

---

## Memory layout (`FULL_DMA_BUFFER`)

```text
dmaBuffersTransposed[0]          — preamble (all zeros)
dmaBuffersTransposed[1..N]       — one buffer per LED column (transposed)
dmaBuffersTransposed[N+1]        — postamble (4× longer for reset timing)
```

In `LOOP` mode, `dmaBuffersTransposed[N+1]->next` points back to `dmaBuffersTransposed[0]`. `stopDisplayLoop()` sets that pointer to `NULL`.

---

## Adding a new target

1. Add a `[env:your-target]` section in `platformio.ini` with the matching `CONFIG_IDF_TARGET_*` build flag.
2. If the new target uses I2S/DMA: add a `hwInit()` branch guarded by the new define, an ISR handler, and a `hwStart()` branch — following the ESP32 or S3 pattern.
3. If the new target uses a different peripheral (like PARLIO on P4): create `src/parlio_<target>.h/.cpp`, declare the show function, include the header in the P4/top of `I2SClocklessLedDriver.h`, and add `#elif CONFIG_IDF_TARGET_<NEW>` branches in `setPins()`, `initLedImpl()`, and `showPixelsImpl()`.

---

## Build system

The project uses **PlatformIO**. Defined environments:

| Environment | Chip |
|-------------|------|
| `esp32dev` | ESP32 |
| `esp-wrover-kit` | ESP32 (WROVER with PSRAM) |
| `esp32-s3-devkitc-1` | ESP32-S3 |
| `esp32-p4` | ESP32-P4 |

Common platform: `pioarduino` (tracks upstream Espressif releases closely). Check `platformio.ini` for the pinned version.

```bash
pio run -e esp32-s3-devkitc-1          # build
pio run -e esp32-s3-devkitc-1 -t upload  # flash
pio device monitor                       # serial monitor
```

---

## Design decisions and known issues

### No more mutable globals (commit 54db938)

`gNbDmaBuffer` and `gNumStrips` were file-scope globals. With two `I2SClocklessLedDriver` instances they would share state and cause a data race. They were removed as follows:

- `gNbDmaBuffer` → class member `nbDmaBuffer` (default 6, not `volatile`). `volatile` is unnecessary because `updateDriver()` always waits for DMA to quiesce via semaphore before writing it; the ISR therefore never runs concurrently with a write.
- `gNumStrips` → parameter on `transpose16x1Noinline2()`; the ISR passes `driver->numStrips` (already `volatile`) directly.

### `Pixels` copy semantics — intentional asymmetry

The copy constructor produces a *non-owning view*: it copies `ledpointer` and sizes but clears `localLedPointer`, `mapFunction`, and `arguments`. `operator=` is deleted to prevent silent shallow copies that could outlive the source buffer. Do not treat the missing assignment operator as a defect.

### `HardwareSprite::reorder()` — caller precondition

`target` carries no size metadata. The bounds check (`pixelOffset >= 0 && pixelOffset < width * height`) only validates within the declared dimensions; if the `target` allocation is actually smaller than `width * height`, writes will overflow silently. Adding a `targetSize` parameter would just shift the error surface without preventing it. Document and enforce as a caller precondition: *the `target` buffer must hold at least `width * height` `uint16_t` elements.*

### `tools/patch_compile_db.py` — known issue with xtensa stubs

The script currently copies xtensa base headers into the live PlatformIO package tree (`~/.platformio/packages/framework-arduinoespressif32-libs/…/xtensa/`). This corrupts subsequent `pio run` builds with `'xthal_set_intset' was not declared in this scope` errors.

**Recovery:** `rm -rf ~/.platformio/packages/framework-arduinoespressif32-libs`

**Pending fix:** write stubs to `OUTPUT_DIR/xtensa_stubs/` and inject via `-isystem` instead of mutating system packages.

---

## CI

Two GitHub Actions workflows are provided:

- **`.github/workflows/build.yml`** — compiles all four PlatformIO environments on every push and pull request.
- **`.github/workflows/lint.yml`** — runs `cppcheck` and `clang-tidy` on every push and pull request.
- **`.github/workflows/docs.yml`** — builds and deploys this MkDocs site to GitHub Pages on every push to `main`.

To enable GitHub Pages deployment, go to **Settings → Pages** and set the source to the `gh-pages` branch.

### clang-tidy exit code handling

`clang-tidy` always exits non-zero when cross-compilation headers (xtensa, newlib, RISC-V) cause fatal errors on the host toolchain. The lint workflow therefore captures output via command substitution with `|| true` and only fails if the captured output contains violation lines anchored to `$(pwd)/src/`. Do **not** remove `|| true` or add `set -e` around the clang-tidy step — that would turn every cross-compilation header error into a CI failure.

---

## Repo reorg

### Current call path (reverse-engineered)

The two public entry points are `initled()` and `showPixels()`. Every other function is called from these two.  The description below follows the code top-down for all three targets (ESP32-D0, ESP32-S3, ESP32-P4).

#### `initled()` path

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
  │     allocate p4Buffer1/2       → PSRAM+DMA ping-pong waveform buffers (inline, ~328 KB each)
  │     forceParlioReconfig = true → PARLIO unit configured lazily on first show
  │     return                     (no i2sInit, no DMA descriptors)
  │
  └─ [ESP32 / ESP32-S3]
        setPins(pinsq)             → stores pins[]; routes GPIO through I2S signal matrix
        hwInit()                   → configures I2S/LCD_CAM registers + GDMA channel (S3)
                                     or allocates interrupt handler (ESP32)
        initTransferBuffers()      → allocates dmaBuffersTampon[] ring (ping-pong)
                                     and optionally dmaBuffersTransposed[] (FULL_DMA_BUFFER)
```

#### `showPixels()` path

```text
showPixels()  /  showPixels(WAIT)  /  showPixels(NO_WAIT)  /  showPixels(newleds)  / …
  │
  ├─ waitDisplay()          — if isDisplaying: block on waitDisp semaphore until ISR clears it
  ├─ set leds, offsetDisplay, displayMode from arguments
  └─ showPixelsImpl()
       │
       ├─ [CONFIG_IDF_TARGET_ESP32P4]
       │     show_parlio_p4(this, pins, …)           ← monolithic: init + transpose + start
       │       │
       │       ├─ lazy hardware init (only when topology changes):
       │       │     configure p4Config (data width, clock, GPIO map)
       │       │     parlio_new_tx_unit → p4TxUnit
       │       │     parlio_tx_unit_enable
       │       │     return early (first frame is a warm-up frame)
       │       │
       │       ├─ create_transposed_led_output_optimized(driver, leds, p4BufferActive, …)
       │       │     for each LED position (0 … numLedPerStrip-1):
       │       │       for each pin:
       │       │         rgbwBufferMapping() → apply LUTs, reorder to wire order, zero-pad short strips
       │       │       for each component (R/G/B/W/W2):
       │       │         transpose_32_slices() → 32 time-slice words (WS2812 bit encoding)
       │       │         process_Nbit()        → pack into output buffer (1/2/4/8/16-bit width)
       │       │
       │       ├─ parlio_tx_unit_wait_all_done → wait for previous frame to finish
       │       ├─ swap ping-pong buffers (p4Buffer1 ↔ p4Buffer2)
       │       └─ parlio_tx_unit_transmit (chunked into ≤65535-byte pieces)
       │     isDisplaying = false  (no ISR; reset here)
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

#### Summary: functions per layer (current state)

| Layer | ESP32-D0 | ESP32-S3 | ESP32-P4 |
|-------|----------|----------|----------|
| Public API | `initled`, `showPixels` | ← same | ← same |
| Colour decode | `switch(cArr)` inline in overload | ← same | ← same |
| Common init | `initLedImpl` | ← same | ← same |
| GPIO routing | `setPins` (I2S matrix) | `setPins` (LCD_CAM signals) | `setPins` (store only) |
| HW peripheral init | `hwInit` | `hwInit` | inline in `show_parlio_p4` (lazy) |
| Buffer allocation | `initTransferBuffers` | `initTransferBuffers` | inline in `initLedImpl` |
| Frame transpose | `loadAndTranspose` | `loadAndTranspose` | `create_transposed_led_output_optimized` |
| LUT + wire-order | inline in `loadAndTranspose` | ← same | `rgbwBufferMapping` |
| HW start | `hwStart` | `hwStart` | inline in `show_parlio_p4` |
| HW stop / ISR | `hwStop` + `interruptHandler` | ← same | none (synchronous) |

---

### Desired decomposition

The goal is a clean three-target architecture where **all three targets use the same function names** for the same conceptual operations, and **utilities that are not part of the hardware driver** are moved to their own files.  The original `I2SClocklessLedDriver.h/.cpp` naming is the reference — P4 code adopts those names exactly, even where the underlying hardware is different.

#### One canonical `initled()`

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

#### File layout after reorg

```text
src/
  I2SClocklessLedDriver.h      — class + canonical initled + showPixels + dispatch shells
  I2SClocklessLedDriver.cpp    — updateDriver(), deleteDriver()
  colorarrangement.h           — ColorArrangement enum + applyColorArrangement()  [NEW]
  i2s_esp32.h                  — ESP32-D0:  i2sInit, initDMABuffers, loadAndTranspose,
  │                               i2sStart, i2sStop, interruptHandler
  i2s_esp32s3.h                — ESP32-S3:  same function names, different register code
  parlio_p4.h                  — ESP32-P4:  same function names (i2sInit, initDMABuffers, …)
  parlio_p4.cpp                — ESP32-P4:  implementation
  pixeltypes.h                 — unchanged
  framebuffer.h                — unchanged
  helper.h                     — unchanged
  HardwareSprite.h/.cpp        — unchanged
  main.cpp                     — dev sketch, unchanged
```

#### Unified function naming (target state — after all phases complete)

Every platform uses the same function name for the same conceptual step.  The `#ifdef` guards are inside the platform-specific files; `I2SClocklessLedDriver.h` sees only a single set of names.  Names are hardware-neutral so they describe the operation, not the peripheral (`hw` rather than `i2s` or `parlio`).

| Concept | ESP32-D0 | ESP32-S3 | ESP32-P4 | Notes |
|---------|----------|----------|----------|-------|
| Configure HW peripheral | `hwInit()` | `hwInit()` | `hwInit(driver)` | P4: configures PARLIO unit (lazy; only when topology changes) |
| Allocate transfer buffers | `initTransferBuffers()` | `initTransferBuffers()` | `initTransferBuffers(driver)` | P4: allocates PSRAM ping-pong waveform buffers |
| Apply LUT + reorder channels | inline in `loadAndTranspose` | ← same | `rgbwBufferMapping()` | Phase 5: extract to shared free function for all platforms |
| Compute wire-format buffer | `loadAndTranspose(driver)` | `loadAndTranspose(driver)` | `loadAndTranspose(driver)` | P4: replaces `create_transposed_led_output_optimized` |
| Start hardware transfer | `hwStart(buffer)` | `hwStart(buffer)` | `hwStart(driver)` | P4: PARLIO chunk+transmit; waits for previous frame first |
| Stop hardware / signal done | `hwStop(driver)` | `hwStop(driver)` | `hwStop(driver)` | P4: synchronous `wait_all_done`; no ISR needed |
| GPIO routing | `setPins(pinsq)` | `setPins(pinsq)` | `setPins(pinsq)` | Already shared; P4 stores pins only |
| Compute frame delay | `setShowDelay()` | `setShowDelay()` | `setShowDelay()` | Already shared |

#### `initLedImpl` after reorg — common skeleton, platform-specific leaves

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

#### `showPixelsImpl` after reorg — common skeleton, platform-specific leaves

```cpp
void showPixelsImpl() {
    guard checks (enableDriver, initSuccess, leds != NULL)
#ifdef CONFIG_IDF_TARGET_ESP32P4
    hwInit(this)            // lazy: only if topology changed (outputs count or max LEDs)
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

#### What leaves `I2SClocklessLedDriver.h`

- **`ColorArrangement` enum and `switch(cArr)` decoder**: move to `src/colorarrangement.h` as a free function `applyColorArrangement(cArr, &nbComponents, &pR, &pG, &pB, &pW, &pW2)`.  The convenience `initled(…, cArr)` overload calls it.
- **ESP32/S3 I2S implementation** (Phase 4): `hwInit`, `initTransferBuffers`, `allocateDMABuffer`, `hwStart`, `hwStop`, `loadAndTranspose`, `interruptHandler`, `transpose16x1Noinline2` and all their register-level code move to `i2s_esp32.h` / `i2s_esp32s3.h`, mirroring `parlio_p4.h/.cpp`.

#### What stays in `I2SClocklessLedDriver.h`

- Class definition and all public member variables.
- The canonical `initled()` and all convenience overloads (public API surface).
- All `showPixels()` overloads (public API surface).
- `initLedImpl()`, `showPixelsImpl()` — thin dispatch shells after reorg.
- `setBrightness()`, `setGamma()`, `setPins()`, `setShowDelay()`, `setMapLed()` — act on class members only; no hardware dependency.
- `updateDriver()` / `deleteDriver()` declarations (implementations in `.cpp`).

---

### Virtual driver

The virtual driver multiplexes up to 120 LED strips on a single ESP32/ESP32-S3 using 74HC595 shift registers — 8 virtual strips per physical GPIO pin across up to 15 pins.  P4 support is planned for the future.  Source: [I2SClocklessVirtualLedDriver](https://github.com/hpwit/I2SClocklessVirtualLedDriver).

#### How it fits the existing architecture

The virtual driver is **not a new target chip** — it is a **different hardware mode** of the same I2S peripheral on ESP32/S3.  It hooks into the same call chain at exactly two points:

| Hook point | Regular driver | Virtual driver |
|------------|---------------|----------------|
| `hwInit()` | Configure I2S for direct parallel output | Configure I2S for shift-register-encoded output (includes clock/latch timing) |
| `loadAndTranspose()` | Encode one LED column into 16-bit I2S words | Encode one LED column × `virtualStripsPerPin` into shift-register I2S words |

Everything else — `initled`, `showPixels`, `initTransferBuffers`, `hwStart`, `hwStop`, `setBrightness`, `updateDriver` — is identical.  The branching is `if (isVirtualDriver)` inside those two functions, not a new platform `#ifdef`.

#### Public API intent

The goal is that the caller uses the **same `initled()` they already know**, just after setting three extra members:

```cpp
driver.isVirtualDriver    = true;
driver.clockPin           = 10;   // 74HC245 clock GPIO
driver.latchPin           = 11;   // 74HC245 latch GPIO
// virtualStripsPerPin defaults to 8; override if using a different shift register
driver.initled(leds, physicalPins, numPhysicalPins, numLedPerStrip, ORDER_GRB);
```

`initled()` detects `isVirtualDriver` and routes through the virtual `hwInit()` and `loadAndTranspose()` paths internally.  No new public function is needed.

#### What is already in place

- `isVirtualDriver` flag on the class.
- Three reserved members: `virtualStripsPerPin` (default 0 = not configured), `clockPin`, `latchPin`.

#### What will be added later

1. `src/virtual_driver.h` — `virtualHwInit(driver)` and `virtualLoadAndTranspose(driver)`, following the same free-function-taking-`driver*` pattern as `parlio_p4.cpp`.
2. `hwInit()` / `loadAndTranspose()` branches: `if (isVirtualDriver) virtual…(this); else { /* existing */ }`.
3. Utilities (palette rendering, pixel pusher, scanline interrupts, dual-core helpers) — added after core virtual driver works; not part of `I2SClocklessLedDriver` itself.

No reorg is needed — the `if (isVirtualDriver)` branches fit cleanly into the `hwInit`/`loadAndTranspose` structure that Phase 4 (Extract ESP32/S3 code) produces.

---

### Phased implementation plan

Each phase is independently buildable and testable; no phase breaks the public API.

**Why renaming comes first:** Phase 1 renames the existing ESP32/S3 functions to hardware-neutral names.  Every later phase then writes code using the correct final names from day one — no double-rename, no transitional wrong-name step on P4, no extracted files that need immediate follow-up renaming.  The renaming decision is the vocabulary for the entire reorg.

#### Phase 1 — Rename to hardware-neutral function names (ESP32/S3) ✅ done

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

#### Phase 2 — Establish canonical `initled()` and move `ColorArrangement` out ✅ done

*Goal:* The driver file contains hardware code only.  The `cArr` convenience layer is explicit and separate.  This phase is fully independent of hardware naming and can run in parallel with Phase 1.

1. Create `src/colorarrangement.h`:
   - The `ColorArrangement` enum (same values, same names — no breaking change for existing users).
   - Free function `void applyColorArrangement(ColorArrangement cArr, uint8_t& nbComponents, uint8_t& pR, uint8_t& pG, uint8_t& pB, uint8_t& pW, uint8_t& pW2)` with the current switch-case body.
2. Promote `initled(leds, pinsq, sizes[], numStrips, nbComponents, pR, pG, pB, pW, pW2)` as the **canonical** form — add a prominent comment marking it as the primary entry point.
3. Replace the duplicated `switch(cArr)` blocks in the convenience overloads with `applyColorArrangement(…)`.
4. In `I2SClocklessLedDriver.h`: `#include "colorarrangement.h"`.

#### Phase 3 — Unify P4 function names (using the Phase 1 vocabulary) ✅ done

*Goal:* `parlio_p4.h/.cpp` exposes `hwInit`, `initTransferBuffers`, `loadAndTranspose`, `hwStart`, `hwStop` — the same final names as the now-renamed ESP32/S3 code.  `hpwit` reads one set of names across all three platform files with no `i2s`/`parlio` prefix confusion.

| Change | From (current P4) | To |
|--------|-------------------|----|
| Monolithic show function | `show_parlio_p4(driver, pins, …)` | split into `hwInit`, `loadAndTranspose`, `hwStart`, `hwStop` |
| Transpose pass | `create_transposed_led_output_optimized(…)` | `loadAndTranspose(driver)` |
| HW peripheral config | (inline in `show_parlio_p4`) | `hwInit(driver)` |
| Buffer allocation | (inline in `initLedImpl`) | `initTransferBuffers(driver)` |
| HW start (transmit) | (inline in `show_parlio_p4`) | `hwStart(driver)` |
| HW stop (wait done) | (inline in `show_parlio_p4`) | `hwStop(driver)` |
| Call site in `showPixelsImpl` | `show_parlio_p4(this, pins, …)` | `hwInit(this)` + `loadAndTranspose(this)` + `hwStart(this)` + `hwStop(this)` |
| Call site in `initLedImpl` (P4) | inline buffer alloc | `initTransferBuffers(this)` |
| Update `parlio_p4.h` | one declaration | four declarations |

P4-specific class members keep their `p4` prefix (`p4TxUnit`, `p4Config`, `p4Buffer1/2`, `p4BufferActive`, `p4LastOutputs`, `p4LastLedsPerOutput`) — the prefix clearly marks them as PARLIO implementation details, not shared state.

#### Phase 4 — Extract ESP32/S3 hardware code into separate headers

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

#### Phase 5 — Align variable names and buffer model

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

#### Phase 6 — Common LUT application layer

*Goal:* The per-pixel LUT+channel-reorder logic (`rgbwBufferMapping` on P4, inline code in `loadAndTranspose` on ESP32/S3) is written once as a shared free function.

`rgbwBufferMapping(driver, inputRGBW, outputWireOrder)` is already defined in `parlio_p4.cpp`.  ESP32/S3 `loadAndTranspose` should call it rather than duplicating the LUT indexing inline.  Requires ISR-path benchmarking on real hardware before committing, since `loadAndTranspose` runs in interrupt context on ESP32/S3 and any extra function-call overhead must be measured.

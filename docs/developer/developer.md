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

Two small DMA buffers (`transferBuffers[0..N+1]`) are filled one at a time by the ISR. Each ISR call transposes and loads the next LED into the currently-idle buffer. This uses minimal RAM but requires an ISR call per LED column.

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
3. **Packs** the per-pin bits into the PARLIO data width (1/2/4/8/16-bit) using optimized `process_Nbit()` helpers in the `LedMatrixDetail` namespace.
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


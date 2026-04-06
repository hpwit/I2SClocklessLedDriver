# Developer Guide

## Repository layout

| Path | Purpose |
|------|---------|
| `src/I2SClocklessLedDriver.h` | Full driver class + all static ISR/transpose functions |
| `src/I2SClocklessLedDriver.cpp` | Global variable definitions; `updateDriver()` / `deleteDriver()` |
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
| `CONFIG_IDF_TARGET_ESP32P4` | AXI GDMA (virtual driver path only, physical not yet implemented) |

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

`wasWaitingtofinish` is a flag set by any caller that is about to block on `waitDisp`. The ISR checks the flag in `i2sStop()` and only calls `xSemaphoreGiveFromISR` when a waiter is present, preventing spurious semaphore count accumulation.

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
2. Add an `i2sInit()` branch in `I2SClocklessLedDriver.h` guarded by the new target define, initialising the appropriate DMA peripheral.
3. Add an ISR handler function for the new peripheral.
4. Add an `i2sStart()` branch to kick off the DMA transfer.
5. If the target uses a different GDMA variant (e.g., AXI GDMA on P4), add a matching `gdma_new_axi_channel` call in the `>= 5.5.0` branch.

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

## CI

Two GitHub Actions workflows are provided:

- **`.github/workflows/build.yml`** — compiles all four PlatformIO environments on every push and pull request.
- **`.github/workflows/docs.yml`** — builds and deploys this MkDocs site to GitHub Pages on every push to `main`.

To enable GitHub Pages deployment, go to **Settings → Pages** and set the source to the `gh-pages` branch.

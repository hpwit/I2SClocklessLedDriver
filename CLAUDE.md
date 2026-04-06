# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this project is

An ESP32 Arduino library that drives up to 16 parallel WS2812/WS2813/WS2815 (RGB) and SK6812 (RGBW) LED strips simultaneously using I2S + DMA — no CPU involvement during transmission.

## Build system

**PlatformIO** — build, flash, and monitor via the `pio` CLI.

```bash
# Build for a specific environment
pio run -e esp32-s3-devkitc-1

# Upload and open serial monitor
pio run -e esp32-s3-devkitc-1 -t upload && pio device monitor

# Build all environments
pio run
```

Defined environments in `platformio.ini`: `esp32dev`, `esp-wrover-kit`, `esp32-s3-devkitc-1`, `esp32-p4`.

Each environment injects `CONFIG_IDF_TARGET_ESP32`, `CONFIG_IDF_TARGET_ESP32S3`, or `CONFIG_IDF_TARGET_ESP32P4` as a build flag — all hardware-specific branching in the code uses these defines.

There are no automated tests; validation is done by flashing and observing LED output.

## Code structure

| File | Role |
|------|------|
| `src/I2SClocklessLedDriver.h` | Main header — contains the full `I2SClocklessLedDriver` class and all static ISR/transpose functions. Nearly all implementation lives here. |
| `src/I2SClocklessLedDriver.cpp` | Global variable definitions for `__NB_DMA_BUFFER` / `NUM_STRIPS`, plus `updateDriver()` and `deleteDriver()` implementations. |
| `src/hardwareSprite.h/.cpp` | Optional hardware sprite overlay (enabled with `#define HARDWARESPRITES 1`). |
| `src/framebuffer.h` | Simple double-buffer helper (`frameBuffer` class). |
| `src/helper.h` | Timing macros: `HOW_LONG`, `RUN_SKETCH_FOR`, `RUN_SKETCH_N_TIMES`. |
| `src/pixeltypes.h` | Minimal `Pixel`/`Pixels` types used when `USE_PIXELSLIB` is not defined. |
| `src/main.cpp` | Dev/test sketch (excluded from library build via `library.json`). Guarded by `#ifdef PLATFORM_VERSION`. |

## Key architecture

### I2S + DMA pipeline

The driver serialises LED colour data into 16-bit parallel I2S words (each bit of a colour byte becomes 3 ticks: `100`=0, `110`=1). Transposition converts the per-strip interleaved layout (`strip0_led0, strip1_led0, …`) into the parallel 16-bit format the I2S hardware expects.

Two modes:
- **Ping-pong DMA** (default): two small DMA buffers filled incrementally by the ISR (`interruptHandler`). Requires CPU on each ISR call but uses little RAM.
- **Full DMA buffer** (`#define FULL_DMA_BUFFER`): entire frame is transposed upfront into one large buffer; I2S runs autonomously. Enables `showPixelsFirstTranspose()`, `showPixelsFromBuffer()`, and `showPixelsFromBuffer(LOOP)`.

### IDF version branching

`NUM_STRIPS`, `__NB_DMA_BUFFER`, and `__delay` are always runtime variables (defined in `.cpp`, extern-declared in `.h`). `updateDriver()` / `deleteDriver()` are always available. The remaining `ESP_IDF_VERSION >= 5.5.0` checks cover genuine API differences only:

- **Header locations**: `<esp_private/gpio.h>` / `<esp_private/periph_ctrl.h>` (≥ 5.5) vs `<driver/gpio.h>` / `<driver/periph_ctrl.h>` (< 5.5).
- **GPIO mux function** (S3): `gpio_iomux_output()` (≥ 5.5) vs `gpio_iomux_out()` (< 5.5).
- **GDMA channel alloc** (S3): `gdma_new_ahb_channel()` without `isr_cache_safe` (≥ 5.5) vs `gdma_new_channel()` with `isr_cache_safe=true` (< 5.5, causes a cache crash on 5.5+).

### Platform branching

- `CONFIG_IDF_TARGET_ESP32S3` — uses LCD_CAM peripheral + GDMA. Main ISR path.
- `CONFIG_IDF_TARGET_ESP32` — uses I2S0 peripheral + `esp_intr_alloc`. Different register layout.
- `CONFIG_IDF_TARGET_ESP32P4` — uses **PARLIO TX** peripheral (`src/parlio_p4.h/.cpp`). No I2S/DMA; the PARLIO unit is configured lazily on the first `showPixels()` call. `initled()` and `showPixels(WAIT)` are the supported entry points; `FULL_DMA_BUFFER`/`LOOP` modes are not available.

### Semaphores

Three FreeRTOS semaphores on the driver object:
- `sem` — blocks `showPixels(WAIT)` until transfer done.
- `semSync` — frame-sync signal for `waitSync()`.
- `waitDisp` — lazy-created in `updateDriver()` to wait for an in-flight DMA to finish before reconfiguring. Released from ISR via `xSemaphoreGiveFromISR`.

The ISR (`i2sStop`, `interruptHandler`) is `IRAM_ATTR` and must only use ISR-safe FreeRTOS calls (`xSemaphoreGiveFromISR`, `portYIELD_FROM_ISR`).

## Compile-time options (set before `#include "I2SClocklessLedDriver.h"`)

| Define | Effect |
|--------|--------|
| `FULL_DMA_BUFFER` | Enable full pre-transposed DMA buffer |
| `ENABLE_HARDWARE_SCROLL` | Enable `OffsetDisplay` hardware scrolling |
| `USE_PIXELSLIB` | Use external PixelsLib types instead of built-in `pixeltypes.h` |
| `HARDWARESPRITES 1` | Enable hardware sprite overlay |
| `SNAKEPATTERN` | 0/1 — strip layout (default 1) |
| `ALTERNATEPATTERN` | 0/1 — alternate start sides (default 1) |
| `OVERCLOCK_1MHZ` / `OVERCLOCK_1_1MHZ` / `OVER_CLOCK_MAX` | Clock speed overrides (S3 only) |

## esp-idf / CMake usage

For esp-idf projects (non-Arduino), `CMakeLists.txt` registers the component with `idf_component_register`. Include path is `src/`, main source is `src/I2SClocklessLedDriver.cpp`.

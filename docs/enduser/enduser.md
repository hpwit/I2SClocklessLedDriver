# End User Guide

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
  https://github.com/hpwit/I2SClocklessLedDriver
```

### Arduino IDE

Download the repository as a ZIP and use **Sketch → Include Library → Add .ZIP Library**.

---

## Basic Setup

```cpp
#define NUMSTRIPS 8
#define NUM_LEDS_PER_STRIP 144
#include "I2SClocklessLedDriver.h"

I2SClocklessLedDriver driver;
uint8_t leds[NUMSTRIPS * NUM_LEDS_PER_STRIP * 3];  // 3 bytes per RGB pixel
uint8_t pins[NUMSTRIPS] = {2, 4, 5, 12, 13, 14, 15, 16};

void setup() {
  driver.initled(leds, pins, NUMSTRIPS, NUM_LEDS_PER_STRIP, ORDER_GRB);
  driver.setBrightness(128);
}

void loop() {
  // fill leds[] …
  driver.showPixels();
}
```

For **RGBW** strips (SK6812), use 4 bytes per pixel and `ORDER_GRBW`:

```cpp
uint8_t leds[NUMSTRIPS * NUM_LEDS_PER_STRIP * 4];
driver.initled(leds, pins, NUMSTRIPS, NUM_LEDS_PER_STRIP, ORDER_GRBW);
```

---

## Initialisation

### Uniform strip lengths

```cpp
driver.initled(leds, pins, num_strips, num_led_per_strip, colorarrangment);
```

### Variable strip lengths

```cpp
uint16_t lengths[3] = {200, 300, 100};
driver.initled(leds, pins, lengths, 3, ORDER_GRB);
```

The `leds` array must be large enough to hold the sum of all strip lengths × bytes-per-pixel.

### Color orders

| Constant | Type | Notes |
|----------|------|-------|
| `ORDER_GRB` | RGB | Most WS2812 variants |
| `ORDER_RGB` | RGB | |
| `ORDER_RBG` | RGB | |
| `ORDER_GBR` | RGB | |
| `ORDER_BRG` | RGB | |
| `ORDER_BGR` | RGB | |
| `ORDER_GRBW` | RGBW | SK6812 |
| `ORDER_RGBW` | RGBW | |
| `ORDER_RGBCCT` | RGBCCT | 5-channel tunable white |

---

## Setting Pixels

### Direct buffer write (fastest)

```cpp
// leds is laid out as: [strip0_led0_R, strip0_led0_G, strip0_led0_B, strip0_led1_R, …, strip1_led0_R, …]
leds[strip * NUM_LEDS_PER_STRIP * 3 + led * 3 + 0] = red;
leds[strip * NUM_LEDS_PER_STRIP * 3 + led * 3 + 1] = green;
leds[strip * NUM_LEDS_PER_STRIP * 3 + led * 3 + 2] = blue;
```

### Helper — RGB

```cpp
driver.setPixel(pos, red, green, blue);
```

`pos` is the flat index across all strips: `strip * NUM_LEDS_PER_STRIP + led`.

For RGBW drivers, `setPixel(pos, r, g, b)` automatically derives the white channel using `W = min(R,G,B)`.

### Helper — RGBW explicit

```cpp
driver.setPixel(pos, red, green, blue, white);
```

---

## Brightness & Gamma

```cpp
driver.setBrightness(128);            // 0–255
driver.setGamma(2.2f, 2.2f, 2.2f);   // per-channel gamma (RGB)
driver.setGamma(2.2f, 2.2f, 2.2f, 2.2f);  // RGBW
```

Gamma and brightness are applied together via lookup tables computed at call time.

---

## Showing Pixels

### Blocking (default)

```cpp
driver.showPixels();              // uses the buffer set in initled()
driver.showPixels(newBuffer);     // switch to a different buffer this frame
```

Both calls wait until the DMA transfer completes before returning.

### Non-blocking

```cpp
driver.showPixels(NO_WAIT);
// CPU is free here; the next showPixels(NO_WAIT) will wait for the
// previous one to finish before starting.
```

### With a hardware scroll offset

```cpp
OffsetDisplay off = driver.getDefaultOffset();
off.offsetx = 5;   // scroll 5 pixels in X
driver.showPixels(off);
driver.showPixels(NO_WAIT, off);
```

Requires `#define ENABLE_HARDWARE_SCROLL` before the include.

---

## Full DMA Buffer Mode

Add `#define FULL_DMA_BUFFER` before the include. This pre-transposes the entire frame into a large DMA buffer. The I2S then runs autonomously with no CPU involvement during transmission.

```cpp
#define FULL_DMA_BUFFER
#include "I2SClocklessLedDriver.h"
```

### Transpose then display

```cpp
driver.showPixelsFirstTranspose();           // transposes + displays, waits
driver.showPixelsFirstTranspose(NO_WAIT);    // transposes + displays, returns immediately
driver.showPixelsFirstTranspose(newBuffer);  // transposes newBuffer + displays
```

The transposition step (~1 ms for 256 LEDs) happens on the CPU; the DMA transmission (~18 ms for 256 LEDs at 800 kHz) runs independently.

### Direct DMA buffer write

Use `setPixelinBuffer()` / `setPixelinBufferByStrip()` to write pixels directly into the pre-transposed DMA buffer, bypassing the per-frame transposition:

```cpp
driver.setPixelinBuffer(pos, red, green, blue);
driver.setPixelinBuffer(pos, red, green, blue, white);  // RGBW
driver.setPixelinBufferByStrip(stripNumber, posOnStrip, red, green, blue);
```

!!! warning
    Do not mix `setPixelinBuffer` with `showPixelsFirstTranspose()` — the transpose will overwrite what you wrote directly.

Then display with:

```cpp
driver.showPixelsFromBuffer();           // display once, blocking
driver.showPixelsFromBuffer(NO_WAIT);    // display once, non-blocking
driver.showPixelsFromBuffer(LOOP);       // continuous loop, no CPU required
```

### Continuous loop

```cpp
driver.showPixelsFromBuffer(LOOP);
// CPU is now completely free; the DMA buffer is re-displayed on every frame

// To stop the loop:
driver.stopDisplayLoop();
```

### Frame sync

When using `LOOP`, use `waitSync()` to align your buffer writes to a frame boundary and avoid tearing:

```cpp
driver.waitSync();
// safe to update the DMA buffer now
driver.setPixelinBuffer(pos, r, g, b);
```

---

## Runtime Reconfiguration

`updateDriver()` allows changing the number of strips, strip lengths, or DMA buffer count at runtime without rebooting. It safely waits for any in-flight DMA transfer to complete first.

```cpp
uint8_t newPins[4] = {2, 4, 5, 12};
uint16_t newSizes[4] = {100, 200, 150, 300};
driver.updateDriver(newPins, newSizes, 4, /*dmaBuffer=*/6,
                    /*nb_components=*/3, /*p_r=*/1, /*p_g=*/0, /*p_b=*/2);
```

---

## Hardware Sprites

Requires `#define HARDWARESPRITES 1`.

```cpp
#define NBSPRITE 4
#define SPRITE_WIDTH 8
#define SPRITE_HEIGHT 8
#define HARDWARESPRITES 1
#include "I2SClocklessLedDriver.h"

hardwareSprite sprite;
sprite.posX = 10;
sprite.posY = 5;
sprite.displaySprite = true;
```

Each `hardwareSprite` writes into a pre-allocated segment of `_spritesleds[]`. A maximum of `NBSPRITE` sprites can be constructed.

---

## Using with FastLED

`CRGB` arrays are layout-compatible with the `uint8_t*` buffers this driver expects:

```cpp
CRGB leds[NUMSTRIPS * NUM_LEDS_PER_STRIP];
driver.initled((uint8_t*)leds, pins, NUMSTRIPS, NUM_LEDS_PER_STRIP, ORDER_GRB);
FastLED.setBrightness(128);  // note: use driver.setBrightness instead
```

---

## Compile-time Options

Set these **before** `#include "I2SClocklessLedDriver.h"`:

| Define | Default | Effect |
|--------|---------|--------|
| `FULL_DMA_BUFFER` | off | Enable full pre-transposed DMA buffer mode |
| `ENABLE_HARDWARE_SCROLL` | off | Enable `OffsetDisplay` hardware scrolling |
| `USE_PIXELSLIB` | off | Use external PixelsLib types |
| `HARDWARESPRITES 1` | 0 | Enable hardware sprite overlay |
| `NBSPRITE` | 8 | Maximum number of sprite objects |
| `SPRITE_WIDTH` | 20 | Sprite width in pixels |
| `SPRITE_HEIGHT` | 20 | Sprite height in pixels |
| `SNAKEPATTERN` | 1 | 1 = strips in snake layout |
| `ALTERNATEPATTERN` | 1 | 1 = strips start on alternate sides |
| `OVERCLOCK_1MHZ` | off | 1 MHz clock (S3 only) |
| `OVERCLOCK_1_1MHZ` | off | 1.1 MHz clock (S3 only) |
| `OVER_CLOCK_MAX` | off | ~1.12 MHz clock (S3 only) |

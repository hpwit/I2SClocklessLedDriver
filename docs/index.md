# I2SClocklessLedDriver

An ESP32 Arduino library that drives **up to 16 parallel LED strips** using I2S + DMA — with zero CPU usage during transmission.

Supported LED types:

- **RGB**: WS2812, WS2813, WS2815
- **RGBW**: SK6812
- **RGBCCT**: 5-channel tunable white

Supported targets: **ESP32**, **ESP32-S3**, **ESP32-P4** (partial).

---

## Why this library?

Most LED drivers block the CPU for the full duration of the transmission. This library offloads everything to the I2S peripheral and DMA hardware. The CPU is free to compute the next frame while the current one is being pushed to the strips.

When using the [full DMA buffer mode](enduser/enduser.md#full-dma-buffer-mode), the CPU is not involved at all during transmission — enabling a "video chip" style loop where the DMA buffer is continuously re-displayed without any CPU intervention.

---

## Quick links

- [End User Guide](enduser/enduser.md) — installation, wiring, API reference, examples
- [Developer Guide](developer/developer.md) — architecture, DMA pipeline, how to extend
- [GitHub Repository](https://github.com/hpwit/I2SClocklessLedDriver)

/**
    @title     I2SClocklessLedDriver
    @file      parlio_p4.h
    @repo      https://github.com/hpwit/I2SClocklessLedDriver
    @Authors   Original PARLIO implementation: @troyhacks (https://github.com/troyhacks)
               Extended by @ewowi (https://github.com/ewowi):
                 - Padding to max LEDs per output for unequal strip lengths
                 - RGBCCT (5-channel) support via offsetWhite2
               Integrated into I2SClocklessLedDriver by @ewowi
    @Copyright © 2026 Yves Bazin, troyhacks, ewowi
    @license   MIT License
**/

#pragma once

#ifdef CONFIG_IDF_TARGET_ESP32P4

#include <stdint.h>

/** Shared waveform-buffer size used by both the allocator (I2SClocklessLedDriver.h)
 *  and the capacity check (parlio_p4.cpp).
 *  Formula: max_leds × max_components × 32 ticks × max_data_width_bits / 8
 *           = 1024 × 5 × 32 × 16 / 8 = 327,680 bytes */
static constexpr uint32_t PARLIO_P4_BUFFER_BYTES = 1024u * 5u * 32u * 16u / 8u;

class I2SClocklessLedDriver;

/**
 * show_parlio_p4 — drive parallel LED strips on ESP32-P4 using the PARLIO peripheral.
 *
 * Equivalent to showPixels() on ESP32/S3 but uses the hardware Parallel IO peripheral
 * instead of I2S+DMA.  The parlio unit is configured lazily on the first call and
 * reconfigured automatically whenever the number of outputs or LEDs-per-output changes.
 *
 * @param driver          Pointer to the owning I2SClocklessLedDriver (for LUT tables).
 * @param parallelPins    GPIO pin numbers for each output (array length: outputs).
 * @param length          Total number of LEDs across all outputs.
 * @param buffer_in       Raw LED data buffer (components bytes per pixel, RGB-ordered input).
 * @param components      Bytes per pixel: 3 = RGB, 4 = RGBW, 5 = RGBCCT.
 * @param outputs         Number of parallel outputs (≤ SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH).
 * @param leds_per_output Per-output LED count (array length: outputs).
 * @param offsetR         Wire-order position of the Red channel.
 * @param offsetG         Wire-order position of the Green channel.
 * @param offsetB         Wire-order position of the Blue channel.
 * @param offsetW         Wire-order position of the White channel (UINT8_MAX = absent).
 * @param offsetW2        Wire-order position of the warm White channel (UINT8_MAX = absent).
 * @return 0 on success/setup, non-zero on error.
 */
uint8_t show_parlio_p4(I2SClocklessLedDriver* driver,
                       uint8_t* parallelPins, uint32_t length,
                       uint8_t* buffer_in, uint8_t components,
                       uint8_t outputs, uint16_t* leds_per_output,
                       uint8_t offsetR, uint8_t offsetG, uint8_t offsetB,
                       uint8_t offsetW, uint8_t offsetW2);

#endif  // CONFIG_IDF_TARGET_ESP32P4

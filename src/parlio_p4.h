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
 * initTransferBuffers — allocate the two ping-pong waveform buffers used by the
 * PARLIO driver.  Called once from initLedImpl() on ESP32-P4.
 * Sets driver->initErrorOccurred on failure.
 * @return true on success, false if allocation failed.
 */
bool initTransferBuffers(I2SClocklessLedDriver* driver);

/**
 * hwInit — configure (or reconfigure) the PARLIO TX unit.  Called from
 * initLedImpl() and updateDriver() on P4.  No-op if the topology (number of
 * outputs and LEDs-per-output) has not changed since the last call.
 */
void hwInit(I2SClocklessLedDriver* driver);

/**
 * loadAndTranspose — bit-transpose the raw LED buffer into the active ping-pong
 * waveform buffer, applying brightness/gamma LUT tables in the same pass.
 * Must only be called after hwInit() returns false.
 */
void loadAndTranspose(I2SClocklessLedDriver* driver);

/**
 * hwStart — compute DMA chunk descriptors from the current active waveform buffer,
 * swap the ping-pong buffers, then queue the chunks for non-blocking PARLIO TX.
 * After this call driver->p4BufferActive points to the idle buffer for the next frame.
 */
void hwStart(I2SClocklessLedDriver* driver);

/**
 * hwStop — block until the PARLIO TX unit has finished transmitting the current
 * frame.  Adds a short guard delay when the wait returned immediately (idle hardware).
 */
void hwStop(I2SClocklessLedDriver* driver);

#endif  // CONFIG_IDF_TARGET_ESP32P4

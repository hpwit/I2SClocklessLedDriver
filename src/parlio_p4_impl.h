/**
    @title     I2SClocklessLedDriver
    @file      parlio_p4_impl.h
    @repo      https://github.com/hpwit/I2SClocklessLedDriver
    @Authors   Original PARLIO implementation: @troyhacks (https://github.com/troyhacks)
               Extended by @ewowi (https://github.com/ewowi):
                 - Padding to max LEDs per output for unequal strip lengths
                 - RGBCCT (5-channel) support via offsetWhite2
               Integrated into I2SClocklessLedDriver by @ewowi
               Converted to class methods (Phase 5) by @ewowi
    @Copyright © 2026 Yves Bazin, troyhacks, ewowi
    @license   MIT License
**/

// Out-of-class definitions for I2SClocklessLedDriver P4 PARLIO methods.
// Included at the bottom of I2SClocklessLedDriver.h, after the class body.

#pragma once

#ifdef CONFIG_IDF_TARGET_ESP32P4

#include "esp_timer.h"
#include "esp_rom_sys.h"
#if HAS_PARLIO_DRIVER
#include "driver/parlio_tx.h"
#endif
#include "esp_attr.h"
#include "esp_err.h"
#include "portmacro.h"
#include "soc/soc_caps.h"
#include "esp_log.h"

#undef TAG
#define TAG "🐸P4"

static_assert(SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH <= 16,
              "parlio_p4_impl.h assumes max data width <= 16 (bit-packing/shift logic).");

// ---------------------------------------------------------------------------
// Bit-transposition helpers
// ---------------------------------------------------------------------------
namespace LedMatrixDetail {

/**
 * transpose_32_slices — for one colour component across all pins, produce 32
 * time-slice words (one per WS2812 clock tick) that encode the parallel output.
 */
inline void transpose_32_slices(uint32_t (&transposed_slices)[32],
                                 uint8_t* mappedBuffer,
                                 const uint8_t component_in_pixel,
                                 const uint32_t num_active_pins,
                                 const uint8_t COMPONENTS_PER_PIXEL,
                                 const uint32_t* waveform_cache) {
    memset(transposed_slices, 0, sizeof(uint32_t) * 32);

    for (uint32_t pin = 0; pin < num_active_pins; ++pin) {
        const uint32_t component_idx = (pin * COMPONENTS_PER_PIXEL) + component_in_pixel;
        const uint8_t  data_byte     = mappedBuffer[component_idx];
        const uint32_t waveform      = waveform_cache[data_byte];
        const uint32_t pin_bit       = (1u << pin);

        uint8_t b;

        b = waveform & 0xFF;
        if ((b >> 7) & 1) transposed_slices[0]  |= pin_bit;
        if ((b >> 6) & 1) transposed_slices[1]  |= pin_bit;
        if ((b >> 5) & 1) transposed_slices[2]  |= pin_bit;
        if ((b >> 4) & 1) transposed_slices[3]  |= pin_bit;
        if ((b >> 3) & 1) transposed_slices[4]  |= pin_bit;
        if ((b >> 2) & 1) transposed_slices[5]  |= pin_bit;
        if ((b >> 1) & 1) transposed_slices[6]  |= pin_bit;
        if ((b >> 0) & 1) transposed_slices[7]  |= pin_bit;

        b = (waveform >> 8) & 0xFF;
        if ((b >> 7) & 1) transposed_slices[8]  |= pin_bit;
        if ((b >> 6) & 1) transposed_slices[9]  |= pin_bit;
        if ((b >> 5) & 1) transposed_slices[10] |= pin_bit;
        if ((b >> 4) & 1) transposed_slices[11] |= pin_bit;
        if ((b >> 3) & 1) transposed_slices[12] |= pin_bit;
        if ((b >> 2) & 1) transposed_slices[13] |= pin_bit;
        if ((b >> 1) & 1) transposed_slices[14] |= pin_bit;
        if ((b >> 0) & 1) transposed_slices[15] |= pin_bit;

        b = (waveform >> 16) & 0xFF;
        if ((b >> 7) & 1) transposed_slices[16] |= pin_bit;
        if ((b >> 6) & 1) transposed_slices[17] |= pin_bit;
        if ((b >> 5) & 1) transposed_slices[18] |= pin_bit;
        if ((b >> 4) & 1) transposed_slices[19] |= pin_bit;
        if ((b >> 3) & 1) transposed_slices[20] |= pin_bit;
        if ((b >> 2) & 1) transposed_slices[21] |= pin_bit;
        if ((b >> 1) & 1) transposed_slices[22] |= pin_bit;
        if ((b >> 0) & 1) transposed_slices[23] |= pin_bit;

        b = (waveform >> 24) & 0xFF;
        if ((b >> 7) & 1) transposed_slices[24] |= pin_bit;
        if ((b >> 6) & 1) transposed_slices[25] |= pin_bit;
        if ((b >> 5) & 1) transposed_slices[26] |= pin_bit;
        if ((b >> 4) & 1) transposed_slices[27] |= pin_bit;
        if ((b >> 3) & 1) transposed_slices[28] |= pin_bit;
        if ((b >> 2) & 1) transposed_slices[29] |= pin_bit;
        if ((b >> 1) & 1) transposed_slices[30] |= pin_bit;
        if ((b >> 0) & 1) transposed_slices[31] |= pin_bit;
    }
}

inline void __attribute__((hot)) process_1bit(uint8_t* buffer, const uint32_t* transposed_slices) {
    uint32_t packed_word = 0;
    for (int i = 0; i < 32; ++i) {
        if (transposed_slices[i]) packed_word |= (1u << i);
    }
    reinterpret_cast<uint32_t*>(buffer)[0] = packed_word;
}

inline void __attribute__((hot)) process_2bit(uint8_t* buffer, const uint32_t* transposed_slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    uint32_t word0 = 0, word1 = 0;
    for (int i = 0; i < 16; ++i) word0 |= (transposed_slices[i]      << (i * 2));
    for (int i = 0; i < 16; ++i) word1 |= (transposed_slices[i + 16] << (i * 2));
    out[0] = word0;
    out[1] = word1;
}

inline void __attribute__((hot)) process_4bit(uint8_t* buffer, const uint32_t* transposed_slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    uint32_t word0 = 0, word1 = 0, word2 = 0, word3 = 0;
    for (int i = 0; i < 8; ++i) word0 |= (transposed_slices[i]      << (i * 4));
    for (int i = 0; i < 8; ++i) word1 |= (transposed_slices[i +  8] << (i * 4));
    for (int i = 0; i < 8; ++i) word2 |= (transposed_slices[i + 16] << (i * 4));
    for (int i = 0; i < 8; ++i) word3 |= (transposed_slices[i + 24] << (i * 4));
    out[0] = word0; out[1] = word1; out[2] = word2; out[3] = word3;
}

inline void __attribute__((hot)) process_8bit(uint8_t* buffer, const uint32_t* transposed_slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    for (int i = 0; i < 8; ++i) {
        const int base = i * 4;
        out[i] = (transposed_slices[base + 0])        |
                 (transposed_slices[base + 1] <<  8)  |
                 (transposed_slices[base + 2] << 16)  |
                 (transposed_slices[base + 3] << 24);
    }
}

inline void __attribute__((hot)) process_16bit(uint16_t* buffer, const uint32_t* transposed_slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    for (int i = 0; i < 16; ++i) {
        const int base = i * 2;
        out[i] = (transposed_slices[base + 0]) | (transposed_slices[base + 1] << 16);
    }
}

}  // namespace LedMatrixDetail

// ---------------------------------------------------------------------------
// Per-pixel colour mapping  (brightness + gamma LUTs, wire-order repack)
// ---------------------------------------------------------------------------

/**
 * rgbwBufferMapping — apply driver LUT tables and repack one pixel's colour
 * channels into wire order.
 */
static void rgbwBufferMapping(uint8_t* packetRGBChannel,
                               const uint8_t* lightsRGBChannel,
                               const uint8_t offsetRed,
                               const uint8_t offsetGreen,
                               const uint8_t offsetBlue,
                               const uint8_t offsetWhite,
                               const uint8_t offsetWhite2,
                               I2SClocklessLedDriver* driver) {
    uint8_t red   = lightsRGBChannel[0];
    uint8_t green = lightsRGBChannel[1];
    uint8_t blue  = lightsRGBChannel[2];

    if (offsetWhite != UINT8_MAX) {
        uint8_t white = lightsRGBChannel[3];
        if (driver->extractWhiteFromRGB && !white) {
            white  = MIN(MIN(red, green), blue);
            red   -= white;
            green -= white;
            blue  -= white;
        }
        packetRGBChannel[offsetWhite] = driver->whiteMap[white];
    }

    if (offsetWhite2 != UINT8_MAX) {
        packetRGBChannel[offsetWhite2] = driver->white2Map[lightsRGBChannel[4]];
    }

    packetRGBChannel[offsetRed]   = driver->redMap[red];
    packetRGBChannel[offsetGreen] = driver->greenMap[green];
    packetRGBChannel[offsetBlue]  = driver->blueMap[blue];
}

// ---------------------------------------------------------------------------
// Main transposition pass
// ---------------------------------------------------------------------------

/**
 * create_transposed_led_output_optimized — converts the raw LED buffer into the
 * bit-parallel waveform buffer consumed by the PARLIO DMA engine.
 */
static void create_transposed_led_output_optimized(
        I2SClocklessLedDriver* driver,
        const uint8_t*  input_buffer,
        uint16_t*       output_buffer,
        const uint16_t* pixels_per_pin,
        const uint32_t  num_active_pins,
        const uint8_t   COMPONENTS_PER_PIXEL,
        const uint8_t   offsetR, const uint8_t offsetG, const uint8_t offsetB,
        const uint8_t   offsetW, const uint8_t offsetW2) {

    static uint32_t waveform_cache[256];
    static bool     waveform_cache_initialized = false;

    static const uint16_t bitpatterns[16] = {
        0b1000100010001000, 0b1000100010001110,
        0b1000100011101000, 0b1000100011101110,
        0b1000111010001000, 0b1000111010001110,
        0b1000111011101000, 0b1000111011101110,
        0b1110100010001000, 0b1110100010001110,
        0b1110100011101000, 0b1110100011101110,
        0b1110111010001000, 0b1110111010001110,
        0b1110111011101000, 0b1110111011101110,
    };

    if (!waveform_cache_initialized) {
        for (int i = 0; i < 256; ++i) {
            const uint16_t p1 = bitpatterns[i >> 4];
            const uint16_t p2 = bitpatterns[i & 0x0F];
            waveform_cache[i] = (uint32_t(p2) << 16) | p1;
        }
        waveform_cache_initialized = true;
    }

    const uint16_t max_leds = driver->numLedPerStrip;

    const uint32_t WAVEFORM_WORDS_PER_PIXEL = COMPONENTS_PER_PIXEL * 32u;
    const uint32_t total_output_words = max_leds * WAVEFORM_WORDS_PER_PIXEL;
    if (total_output_words == 0) return;

    uint8_t bit_width;
    if      (num_active_pins <= 1)  bit_width = 1;
    else if (num_active_pins <= 2)  bit_width = 2;
    else if (num_active_pins <= 4)  bit_width = 4;
    else if (num_active_pins <= 8)  bit_width = 8;
    else                            bit_width = 16;

    const size_t total_bytes = (total_output_words * bit_width + 7) / 8;
    memset(output_buffer, 0, total_bytes);

    uint8_t* out_base_ptr = reinterpret_cast<uint8_t*>(output_buffer);

    for (uint32_t pixel_in_pin = 0; pixel_in_pin < max_leds; ++pixel_in_pin) {

        uint8_t mappedBuffer[COMPONENTS_PER_PIXEL * SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH];

        for (uint32_t pin = 0; pin < num_active_pins; ++pin) {
            const uint32_t pixel_idx     = driver->firstIndexPerOutput[pin] + pixel_in_pin;
            const uint32_t component_idx = pixel_idx * COMPONENTS_PER_PIXEL;

            if (pixel_in_pin < pixels_per_pin[pin]) {
                rgbwBufferMapping(&mappedBuffer[pin * COMPONENTS_PER_PIXEL],
                                  &input_buffer[component_idx],
                                  offsetR, offsetG, offsetB, offsetW, offsetW2,
                                  driver);
            } else {
                memset(&mappedBuffer[pin * COMPONENTS_PER_PIXEL], 0, COMPONENTS_PER_PIXEL);
            }
        }

        for (uint32_t component_in_pixel = 0; component_in_pixel < COMPONENTS_PER_PIXEL; ++component_in_pixel) {
            uint32_t transposed_slices[32];

            LedMatrixDetail::transpose_32_slices(transposed_slices, mappedBuffer,
                                                  component_in_pixel, num_active_pins,
                                                  COMPONENTS_PER_PIXEL, waveform_cache);

            const uint32_t component_start_word =
                (pixel_in_pin * WAVEFORM_WORDS_PER_PIXEL) + (component_in_pixel * 32u);
            uint8_t* current_out_ptr = out_base_ptr + (component_start_word * bit_width / 8u);

            switch (bit_width) {
            case  1: LedMatrixDetail::process_1bit(current_out_ptr, transposed_slices); break;
            case  2: LedMatrixDetail::process_2bit(current_out_ptr, transposed_slices); break;
            case  4: LedMatrixDetail::process_4bit(current_out_ptr, transposed_slices); break;
            case  8: LedMatrixDetail::process_8bit(current_out_ptr, transposed_slices); break;
            case 16: LedMatrixDetail::process_16bit(reinterpret_cast<uint16_t*>(current_out_ptr),
                                                     transposed_slices); break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// PARLIO transmit config — immutable after first use.
// ---------------------------------------------------------------------------

static const parlio_transmit_config_t transmit_config = {
    .idle_value = 0x00,
    .flags = {
        .queue_nonblocking  = 1,
        .loop_transmission  = 0,
    }
};

// ---------------------------------------------------------------------------
// I2SClocklessLedDriver P4 method bodies
// ---------------------------------------------------------------------------

inline bool __attribute__((hot)) I2SClocklessLedDriver::loadAndTranspose() {
    uint8_t outputs = numStrips;
    if (outputs > SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH)
        outputs = SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH;
    const uint16_t max_leds  = numLedPerStrip;
    const uint8_t  components = nbComponents;

    const uint32_t required_bytes =
        ((uint32_t)max_leds * components * 32u * p4Config.data_width + 7u) / 8u;
    if (required_bytes > PARLIO_P4_BUFFER_BYTES) {
        ESP_LOGE(TAG, "loadAndTranspose: buffer too small "
                      "(%u needed, %u allocated) for %u LEDs × %u ch × %u-bit — skipping frame",
                 (unsigned)required_bytes, (unsigned)PARLIO_P4_BUFFER_BYTES,
                 (unsigned)max_leds, (unsigned)components,
                 (unsigned)p4Config.data_width);
        return false;
    }

    create_transposed_led_output_optimized(
        this, leds, p4BufferActive,
        stripSize, outputs, components,
        pR, pG, pB, pW, pW2);
    return true;
}

inline void I2SClocklessLedDriver::hwStart() {
    const uint8_t  components = nbComponents;
    const uint16_t max_leds   = numLedPerStrip;

    const uint32_t bits_per_pixel   = components * 32u * p4Config.data_width;
    const uint32_t bytes_per_pixel  = (bits_per_pixel + 7u) / 8u;
    if (bytes_per_pixel == 0) {
        ESP_LOGE(TAG, "hwStart: bytes_per_pixel == 0 — skipping frame");
        return;
    }
    const uint32_t HW_MAX_BYTES       = p4Config.max_transfer_size;
    const uint16_t max_leds_per_chunk = HW_MAX_BYTES / bytes_per_pixel;
    const uint8_t  num_chunks         = (uint8_t)((max_leds + max_leds_per_chunk - 1u) / max_leds_per_chunk);
    const size_t   chunk_stride       = (size_t)max_leds_per_chunk * bytes_per_pixel;

    uint32_t leds_remaining = max_leds;
    const uint8_t* chunk_ptr = (const uint8_t*)p4BufferActive;

    // Swap ping-pong: next loadAndTranspose writes to the idle buffer.
    p4BufferActive = (p4BufferActive == p4Buffer1) ? p4Buffer2 : p4Buffer1;

    // Queue chunks for non-blocking PARLIO TX - compute and transmit on the fly
    for (uint8_t i = 0; i < num_chunks; ++i) {
        uint32_t leds_in_chunk = (leds_remaining < max_leds_per_chunk) ? leds_remaining : max_leds_per_chunk;
        uint32_t chunk_bits = leds_in_chunk * bits_per_pixel;
        
        if (chunk_bits > 0) {
            ESP_ERROR_CHECK(parlio_tx_unit_transmit(
                p4TxUnit, chunk_ptr, chunk_bits, &transmit_config));
        }
        
        chunk_ptr += chunk_stride;
        leds_remaining -= leds_in_chunk;
    }
}

inline void I2SClocklessLedDriver::hwStop() {
    int64_t before = esp_timer_get_time();
    ESP_ERROR_CHECK(parlio_tx_unit_wait_all_done(p4TxUnit, portMAX_DELAY));
    int64_t after = esp_timer_get_time();
    if (after - before < 50) esp_rom_delay_us(20);
}

#endif  // CONFIG_IDF_TARGET_ESP32P4

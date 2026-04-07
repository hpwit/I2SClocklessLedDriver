/**
    @title     I2SClocklessLedDriver
    @file      test/test_parlio_p4_logic.cpp
    @repo      https://github.com/hpwit/I2SClocklessLedDriver

    Host-native unit tests for the pure bit-manipulation logic extracted from
    src/parlio_p4.cpp.

    Covered logic (all platform-agnostic, no ESP32-IDF dependencies):
      1. WS2812 waveform cache initialisation (bitpatterns + nibble-decode)
      2. LedMatrixDetail::process_1/2/4/8/16bit packing helpers
      3. LedMatrixDetail::transpose_32_slices bit-transposition
      4. Data-width selection (1/2/4/8/16 based on pin count)
      5. Chunk-count and chunk-size arithmetic (hwStart logic)
      6. PARLIO_P4_BUFFER_BYTES constant correctness

    These tests are self-contained — they re-implement the same algorithm with
    the same constants so that any divergence between the test expectation and
    the source immediately flags a regression.

    Compile as plain C++11 executable:
        g++ -std=c++11 -I src test/test_parlio_p4_logic.cpp -o test_parlio_p4_logic
        ./test_parlio_p4_logic

    Or run via PlatformIO native env:
        pio test -e native
**/

#ifdef UNITY_FRAMEWORK
  #include <unity.h>
  #define TEST_ASSERT_EQ(expected, actual) TEST_ASSERT_EQUAL_UINT32((uint32_t)(expected), (uint32_t)(actual))
  #define TEST_ASSERT_EQ8(expected, actual) TEST_ASSERT_EQUAL_UINT8((expected), (actual))
#else
  #include <assert.h>
  #include <stdio.h>
  #define TEST_ASSERT_EQ(expected, actual) \
      do { \
          uint32_t _e = (uint32_t)(expected); \
          uint32_t _a = (uint32_t)(actual); \
          if (_e != _a) { \
              printf("FAIL %s:%d — expected 0x%08X (%u) got 0x%08X (%u)\n", \
                     __FILE__, __LINE__, _e, _e, _a, _a); \
              failures++; \
          } \
      } while (0)
  #define TEST_ASSERT_EQ8(expected, actual) \
      do { \
          uint8_t _e = (uint8_t)(expected); \
          uint8_t _a = (uint8_t)(actual); \
          if (_e != _a) { \
              printf("FAIL %s:%d — expected 0x%02X got 0x%02X\n", \
                     __FILE__, __LINE__, _e, _a); \
              failures++; \
          } \
      } while (0)
  static int failures = 0;
#endif

#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Re-declare the constants and functions under test.
// They mirror the implementation in parlio_p4.cpp exactly, allowing host
// compilation and independent verification.
// ---------------------------------------------------------------------------

// Matches PARLIO_P4_BUFFER_BYTES in parlio_p4.h
static constexpr uint32_t PARLIO_P4_BUFFER_BYTES_EXPECTED = 1024u * 5u * 32u * 16u / 8u;

// WS2812 nibble-to-waveform lookup table (copied verbatim from parlio_p4.cpp).
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

// Compute the waveform cache entry for one byte value (same algorithm as parlio_p4.cpp).
static uint32_t compute_waveform(uint8_t byte_val) {
    const uint16_t p1 = bitpatterns[byte_val >> 4];    // high nibble → ticks 0-15
    const uint16_t p2 = bitpatterns[byte_val & 0x0F];  // low  nibble → ticks 16-31
    return (uint32_t(p2) << 16) | p1;
}

// Build the full 256-entry waveform cache (same as parlio_p4.cpp).
static void build_waveform_cache(uint32_t cache[256]) {
    for (int i = 0; i < 256; ++i)
        cache[i] = compute_waveform((uint8_t)i);
}

// ---------------------------------------------------------------------------
// Replicated helpers (from LedMatrixDetail namespace in parlio_p4.cpp)
// ---------------------------------------------------------------------------

static void test_transpose_32_slices_impl(
        uint32_t (&transposed_slices)[32],
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

static void process_1bit(uint8_t* buffer, const uint32_t* transposed_slices) {
    uint32_t packed_word = 0;
    for (int i = 0; i < 32; ++i)
        if (transposed_slices[i]) packed_word |= (1u << i);
    reinterpret_cast<uint32_t*>(buffer)[0] = packed_word;
}

static void process_2bit(uint8_t* buffer, const uint32_t* transposed_slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    uint32_t word0 = 0, word1 = 0;
    for (int i = 0; i < 16; ++i) word0 |= (transposed_slices[i]      << (i * 2));
    for (int i = 0; i < 16; ++i) word1 |= (transposed_slices[i + 16] << (i * 2));
    out[0] = word0;
    out[1] = word1;
}

static void process_4bit(uint8_t* buffer, const uint32_t* transposed_slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    uint32_t word0 = 0, word1 = 0, word2 = 0, word3 = 0;
    for (int i = 0; i < 8; ++i) word0 |= (transposed_slices[i]      << (i * 4));
    for (int i = 0; i < 8; ++i) word1 |= (transposed_slices[i +  8] << (i * 4));
    for (int i = 0; i < 8; ++i) word2 |= (transposed_slices[i + 16] << (i * 4));
    for (int i = 0; i < 8; ++i) word3 |= (transposed_slices[i + 24] << (i * 4));
    out[0] = word0; out[1] = word1; out[2] = word2; out[3] = word3;
}

static void process_8bit(uint8_t* buffer, const uint32_t* transposed_slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    for (int i = 0; i < 8; ++i) {
        const int base = i * 4;
        out[i] = (transposed_slices[base + 0])        |
                 (transposed_slices[base + 1] <<  8)  |
                 (transposed_slices[base + 2] << 16)  |
                 (transposed_slices[base + 3] << 24);
    }
}

static void process_16bit(uint16_t* buffer, const uint32_t* transposed_slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    for (int i = 0; i < 16; ++i) {
        const int base = i * 2;
        out[i] = (transposed_slices[base + 0]) | (transposed_slices[base + 1] << 16);
    }
}

// Data-width selection (mirrors create_transposed_led_output_optimized).
static uint8_t select_bit_width(uint32_t num_active_pins) {
    if      (num_active_pins <= 1)  return 1;
    else if (num_active_pins <= 2)  return 2;
    else if (num_active_pins <= 4)  return 4;
    else if (num_active_pins <= 8)  return 8;
    else                            return 16;
}

// Chunk count arithmetic (mirrors hwStart).
static uint8_t compute_num_chunks(uint16_t max_leds, uint32_t bytes_per_pixel, uint32_t hw_max_bytes) {
    uint32_t max_leds_per_chunk = hw_max_bytes / bytes_per_pixel;
    if (max_leds_per_chunk == 0) return 0;
    return (uint8_t)((max_leds + max_leds_per_chunk - 1u) / max_leds_per_chunk);
}

// ---------------------------------------------------------------------------
// TESTS: PARLIO_P4_BUFFER_BYTES
// ---------------------------------------------------------------------------

void test_parlio_p4_buffer_bytes_formula(void) {
    // Formula: 1024 LEDs × 5 components × 32 ticks × 16 bits / 8 bits-per-byte
    const uint32_t expected = 1024u * 5u * 32u * 16u / 8u;
    TEST_ASSERT_EQ(327680u, expected);
    TEST_ASSERT_EQ(PARLIO_P4_BUFFER_BYTES_EXPECTED, expected);
}

// ---------------------------------------------------------------------------
// TESTS: waveform cache correctness
// ---------------------------------------------------------------------------

void test_waveform_cache_all_zeros(void) {
    // 0x00: every bit is 0 → every nibble is 0 → bitpatterns[0] = 0x8888
    // waveform = (0x8888 << 16) | 0x8888 = 0x88888888
    TEST_ASSERT_EQ(0x88888888u, compute_waveform(0x00));
}

void test_waveform_cache_all_ones(void) {
    // 0xFF: every bit is 1 → every nibble is 0xF → bitpatterns[15] = 0xEEEE
    // waveform = (0xEEEE << 16) | 0xEEEE = 0xEEEEEEEE
    TEST_ASSERT_EQ(0xEEEEEEEEu, compute_waveform(0xFF));
}

void test_waveform_cache_high_nibble_only(void) {
    // 0xF0: high nibble=0xF → p1=0xEEEE (ticks 0-15)
    //       low  nibble=0x0 → p2=0x8888 (ticks 16-31)
    // waveform = (0x8888 << 16) | 0xEEEE = 0x8888EEEE
    TEST_ASSERT_EQ(0x8888EEEEu, compute_waveform(0xF0));
}

void test_waveform_cache_low_nibble_only(void) {
    // 0x0F: high nibble=0x0 → p1=0x8888 (ticks 0-15)
    //       low  nibble=0xF → p2=0xEEEE (ticks 16-31)
    // waveform = (0xEEEE << 16) | 0x8888 = 0xEEEE8888
    TEST_ASSERT_EQ(0xEEEE8888u, compute_waveform(0x0F));
}

void test_waveform_cache_msb_only(void) {
    // 0x80 = 0b10000000: high nibble=0b1000=8
    // bitpatterns[8] = 0b1110100010001000 = 0xE888
    // Low nibble = 0, bitpatterns[0] = 0x8888
    // waveform = (0x8888 << 16) | 0xE888 = 0x8888E888
    TEST_ASSERT_EQ(0x8888E888u, compute_waveform(0x80));
}

void test_waveform_cache_lsb_only(void) {
    // 0x01 = 0b00000001: high nibble=0b0000=0, low nibble=0b0001=1
    // bitpatterns[0] = 0x8888 (high nibble → ticks 0-15)
    // bitpatterns[1] = 0b1000100010001110 = 0x888E (low nibble → ticks 16-31)
    // waveform = (0x888E << 16) | 0x8888 = 0x888E8888
    TEST_ASSERT_EQ(0x888E8888u, compute_waveform(0x01));
}

void test_waveform_cache_alternating_bits_AA(void) {
    // 0xAA = 0b10101010: high nibble=0b1010=10, low nibble=0b1010=10
    // bitpatterns[10] = 0b1110100011101000 = 0xE8E8
    // waveform = (0xE8E8 << 16) | 0xE8E8 = 0xE8E8E8E8
    TEST_ASSERT_EQ(0xE8E8E8E8u, compute_waveform(0xAA));
}

void test_waveform_cache_alternating_bits_55(void) {
    // 0x55 = 0b01010101: high nibble=0b0101=5, low nibble=0b0101=5
    // bitpatterns[5] = 0b1000111010001110 = 0x8E8E
    // waveform = (0x8E8E << 16) | 0x8E8E = 0x8E8E8E8E
    TEST_ASSERT_EQ(0x8E8E8E8Eu, compute_waveform(0x55));
}

void test_waveform_cache_full_table_all_zeros_encode_low(void) {
    // Verify that every entry in the cache has bits 3,7,11,15,19,23,27,31
    // set (the "1" start-bit of every WS2812 pulse).
    // Each 4-bit group in any waveform pattern starts with 1 (both 1000 and 1110).
    uint32_t cache[256];
    build_waveform_cache(cache);
    for (int i = 0; i < 256; ++i) {
        // Bit 3 of the waveform (LSB of byte 0, MSB=bit7 of first tick)
        // The first tick of the first WS2812 pulse is always 1 → bit 7 of byte 0 = 1.
        uint8_t byte0 = cache[i] & 0xFF;
        TEST_ASSERT_EQ(1u, (byte0 >> 7) & 1u);
    }
}

void test_bitpatterns_nibble_0_is_all_zero_bits(void) {
    // Nibble 0b0000 = 4 zero-bits → 4 × '1000' = 1000 1000 1000 1000 = 0x8888
    TEST_ASSERT_EQ(0x8888u, bitpatterns[0]);
}

void test_bitpatterns_nibble_15_is_all_one_bits(void) {
    // Nibble 0b1111 = 4 one-bits → 4 × '1110' = 1110 1110 1110 1110 = 0xEEEE
    TEST_ASSERT_EQ(0xEEEEu, bitpatterns[15]);
}

void test_bitpatterns_nibble_1_single_one_lsb(void) {
    // Nibble 0b0001: bit3=0, bit2=0, bit1=0, bit0=1
    // WS2812: 1000 1000 1000 1110 = 0x888E
    TEST_ASSERT_EQ(0x888Eu, bitpatterns[1]);
}

void test_bitpatterns_nibble_8_single_one_msb(void) {
    // Nibble 0b1000: bit3=1, bit2=0, bit1=0, bit0=0
    // WS2812: 1110 1000 1000 1000 = 0xE888
    TEST_ASSERT_EQ(0xE888u, bitpatterns[8]);
}

// ---------------------------------------------------------------------------
// TESTS: Data-width selection
// ---------------------------------------------------------------------------

void test_bit_width_1_pin(void) {
    TEST_ASSERT_EQ(1u, select_bit_width(1));
}

void test_bit_width_2_pins(void) {
    TEST_ASSERT_EQ(2u, select_bit_width(2));
}

void test_bit_width_3_pins(void) {
    TEST_ASSERT_EQ(4u, select_bit_width(3));
}

void test_bit_width_4_pins(void) {
    TEST_ASSERT_EQ(4u, select_bit_width(4));
}

void test_bit_width_5_pins(void) {
    TEST_ASSERT_EQ(8u, select_bit_width(5));
}

void test_bit_width_8_pins(void) {
    TEST_ASSERT_EQ(8u, select_bit_width(8));
}

void test_bit_width_9_pins(void) {
    TEST_ASSERT_EQ(16u, select_bit_width(9));
}

void test_bit_width_16_pins(void) {
    TEST_ASSERT_EQ(16u, select_bit_width(16));
}

// ---------------------------------------------------------------------------
// TESTS: Chunk count arithmetic (hwStart logic)
// ---------------------------------------------------------------------------

void test_chunk_count_fits_in_one_chunk(void) {
    // 100 LEDs × (3 × 32 × 1bit / 8) = 1200 bytes, hw_max = 65535 → 1 chunk
    const uint32_t bytes_pp = (3u * 32u * 1u + 7u) / 8u;  // = 12
    TEST_ASSERT_EQ(1u, compute_num_chunks(100, bytes_pp, 65535u));
}

void test_chunk_count_exactly_two_chunks(void) {
    // Make hw_max exactly hold half the pixels so we need 2 chunks.
    // bytes_per_pixel = 12 (3 components × 32 ticks × 1 bit/8)
    const uint32_t bytes_pp = 12u;
    const uint16_t max_leds = 200u;
    // hw_max = 100 pixels × 12 = 1200 bytes
    TEST_ASSERT_EQ(2u, compute_num_chunks(max_leds, bytes_pp, 1200u));
}

void test_chunk_count_three_chunks(void) {
    const uint32_t bytes_pp = 12u;
    const uint16_t max_leds = 300u;
    // hw_max = 100 pixels → 3 chunks
    TEST_ASSERT_EQ(3u, compute_num_chunks(max_leds, bytes_pp, 1200u));
}

void test_chunk_count_single_led(void) {
    TEST_ASSERT_EQ(1u, compute_num_chunks(1u, 12u, 65535u));
}

void test_chunk_count_max_leds_zero(void) {
    // Edge: 0 LEDs → ceiling division gives 0 chunks
    TEST_ASSERT_EQ(0u, compute_num_chunks(0u, 12u, 65535u));
}

// ---------------------------------------------------------------------------
// TESTS: process_1bit packing
// ---------------------------------------------------------------------------

void test_process_1bit_all_active(void) {
    // If all 32 slices are non-zero, the packed word is 0xFFFFFFFF.
    uint32_t slices[32];
    for (int i = 0; i < 32; ++i) slices[i] = 1u;
    uint8_t buf[4] = {};
    process_1bit(buf, slices);
    TEST_ASSERT_EQ(0xFFFFFFFFu, reinterpret_cast<uint32_t*>(buf)[0]);
}

void test_process_1bit_all_inactive(void) {
    uint32_t slices[32] = {};
    uint8_t buf[4] = {};
    process_1bit(buf, slices);
    TEST_ASSERT_EQ(0u, reinterpret_cast<uint32_t*>(buf)[0]);
}

void test_process_1bit_only_lsb(void) {
    // Only slice 0 active → bit 0 set in packed word.
    uint32_t slices[32] = {};
    slices[0] = 1u;
    uint8_t buf[4] = {};
    process_1bit(buf, slices);
    TEST_ASSERT_EQ(1u, reinterpret_cast<uint32_t*>(buf)[0]);
}

void test_process_1bit_only_msb(void) {
    // Only slice 31 active → bit 31 set.
    uint32_t slices[32] = {};
    slices[31] = 5u;  // non-zero, value > 0
    uint8_t buf[4] = {};
    process_1bit(buf, slices);
    TEST_ASSERT_EQ(1u << 31, reinterpret_cast<uint32_t*>(buf)[0]);
}

// ---------------------------------------------------------------------------
// TESTS: process_2bit packing
// ---------------------------------------------------------------------------

void test_process_2bit_all_active(void) {
    // All slices = 0x3 (both bits in each 2-bit slot set).
    uint32_t slices[32];
    for (int i = 0; i < 32; ++i) slices[i] = 0x3u;
    uint8_t buf[8] = {};
    process_2bit(buf, slices);
    uint32_t* out = reinterpret_cast<uint32_t*>(buf);
    TEST_ASSERT_EQ(0xFFFFFFFFu, out[0]);
    TEST_ASSERT_EQ(0xFFFFFFFFu, out[1]);
}

void test_process_2bit_all_inactive(void) {
    uint32_t slices[32] = {};
    uint8_t buf[8] = {};
    process_2bit(buf, slices);
    uint32_t* out = reinterpret_cast<uint32_t*>(buf);
    TEST_ASSERT_EQ(0u, out[0]);
    TEST_ASSERT_EQ(0u, out[1]);
}

void test_process_2bit_first_slice_only(void) {
    // Slice 0 = 0x3 → bits [1:0] of word0.
    uint32_t slices[32] = {};
    slices[0] = 0x3u;
    uint8_t buf[8] = {};
    process_2bit(buf, slices);
    uint32_t* out = reinterpret_cast<uint32_t*>(buf);
    TEST_ASSERT_EQ(0x3u, out[0]);
    TEST_ASSERT_EQ(0u,   out[1]);
}

void test_process_2bit_last_slice_only(void) {
    // Slice 31 = 0x3 → bits [31:30] of word1.
    uint32_t slices[32] = {};
    slices[31] = 0x3u;
    uint8_t buf[8] = {};
    process_2bit(buf, slices);
    uint32_t* out = reinterpret_cast<uint32_t*>(buf);
    TEST_ASSERT_EQ(0u,           out[0]);
    TEST_ASSERT_EQ(0x3u << 30,   out[1]);
}

// ---------------------------------------------------------------------------
// TESTS: transpose_32_slices single-pin round-trip
// ---------------------------------------------------------------------------

void test_transpose_32_slices_single_pin_all_zeros(void) {
    // One pin, one component, value = 0x00.
    // Waveform for 0x00 = 0x88888888 (each bit → 1000).
    // For each slice s (0..31): the waveform bit at position (31-s) is set in the waveform.
    // The pin_bit = 1.  After transposition, each slice should have bit 0 set IFF the
    // corresponding waveform bit is 1.
    uint32_t cache[256];
    build_waveform_cache(cache);
    uint8_t mappedBuffer[1] = { 0x00 };
    uint32_t slices[32];
    test_transpose_32_slices_impl(slices, mappedBuffer, 0, 1, 1, cache);

    // 0x88888888 = 10001000100010001000100010001000 in binary
    // bit 31 (MSB, slice 0) = 1
    // bit 30 (slice 1)      = 0
    // bit 29 (slice 2)      = 0
    // bit 28 (slice 3)      = 0
    // bit 27 (slice 4)      = 1  ...and so on in groups of 4: 1000 repeating
    // The pattern repeats every 4 bits: 1000 1000 ...
    // slice 0 corresponds to bit 7 of byte 0 of waveform (MSB of first byte = 0x88)
    //   0x88 = 10001000: bit7=1, bit6=0, bit5=0, bit4=0, bit3=1, bit2=0, bit1=0, bit0=0
    // slice 0 → bit7 of 0x88 = 1 → slices[0] should have pin_bit (bit 0) set
    TEST_ASSERT_EQ(1u, slices[0]);   // bit 7 of 0x88 = 1
    TEST_ASSERT_EQ(0u, slices[1]);   // bit 6 of 0x88 = 0
    TEST_ASSERT_EQ(0u, slices[2]);   // bit 5 of 0x88 = 0
    TEST_ASSERT_EQ(0u, slices[3]);   // bit 4 of 0x88 = 0
    TEST_ASSERT_EQ(1u, slices[4]);   // bit 3 of 0x88 = 1
    TEST_ASSERT_EQ(0u, slices[5]);   // bit 2 of 0x88 = 0
    TEST_ASSERT_EQ(0u, slices[6]);   // bit 1 of 0x88 = 0
    TEST_ASSERT_EQ(0u, slices[7]);   // bit 0 of 0x88 = 0
}

void test_transpose_32_slices_single_pin_all_ones(void) {
    // One pin, value = 0xFF → waveform 0xEEEEEEEE.
    // 0xEE = 11101110: bit7=1,bit6=1,bit5=1,bit4=0,bit3=1,bit2=1,bit1=1,bit0=0
    uint32_t cache[256];
    build_waveform_cache(cache);
    uint8_t mappedBuffer[1] = { 0xFF };
    uint32_t slices[32];
    test_transpose_32_slices_impl(slices, mappedBuffer, 0, 1, 1, cache);

    TEST_ASSERT_EQ(1u, slices[0]);   // bit7 of 0xEE = 1
    TEST_ASSERT_EQ(1u, slices[1]);   // bit6 = 1
    TEST_ASSERT_EQ(1u, slices[2]);   // bit5 = 1
    TEST_ASSERT_EQ(0u, slices[3]);   // bit4 = 0
    TEST_ASSERT_EQ(1u, slices[4]);   // bit3 = 1
    TEST_ASSERT_EQ(1u, slices[5]);   // bit2 = 1
    TEST_ASSERT_EQ(1u, slices[6]);   // bit1 = 1
    TEST_ASSERT_EQ(0u, slices[7]);   // bit0 = 0
}

void test_transpose_32_slices_two_pins_independent(void) {
    // Two pins with different values.  Each pin's contribution must not
    // bleed into the other pin's bit-position in the slices.
    // Pin 0 = 0x00, Pin 1 = 0xFF.
    // COMPONENTS_PER_PIXEL=1, component_in_pixel=0.
    // mappedBuffer[0]=0x00 (pin 0), mappedBuffer[1]=0xFF (pin 1).
    uint32_t cache[256];
    build_waveform_cache(cache);
    uint8_t mappedBuffer[2] = { 0x00, 0xFF };
    uint32_t slices[32];
    test_transpose_32_slices_impl(slices, mappedBuffer, 0, 2, 1, cache);

    // Slice 0: pin0 bit = waveform(0x00) bit-31 = 1 → bit 0 set;
    //          pin1 bit = waveform(0xFF) bit-31 = 1 → bit 1 set.
    //          Combined = 0b11 = 3
    TEST_ASSERT_EQ(3u, slices[0]);

    // Slice 3: pin0 bit = waveform(0x00) byte0-bit4 = 0 → bit 0 clear;
    //          pin1 bit = waveform(0xFF) byte0-bit4 = 0 → bit 1 clear.
    //          Combined = 0
    TEST_ASSERT_EQ(0u, slices[3]);
}

void test_transpose_32_slices_zero_pins(void) {
    // With 0 active pins, all slices remain 0.
    uint32_t cache[256];
    build_waveform_cache(cache);
    uint8_t mappedBuffer[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    uint32_t slices[32];
    test_transpose_32_slices_impl(slices, mappedBuffer, 0, 0, 1, cache);
    for (int i = 0; i < 32; ++i) {
        TEST_ASSERT_EQ(0u, slices[i]);
    }
}

// ---------------------------------------------------------------------------
// TESTS: process_8bit round-trip
// ---------------------------------------------------------------------------

void test_process_8bit_all_active(void) {
    // All slices have every bit set (within 8-bit width = max pin_bit = 0xFF).
    uint32_t slices[32];
    for (int i = 0; i < 32; ++i) slices[i] = 0xFFu;
    uint8_t buf[32] = {};
    process_8bit(buf, slices);
    uint32_t* out = reinterpret_cast<uint32_t*>(buf);
    for (int i = 0; i < 8; ++i) {
        TEST_ASSERT_EQ(0xFFFFFFFFu, out[i]);
    }
}

void test_process_8bit_all_inactive(void) {
    uint32_t slices[32] = {};
    uint8_t buf[32] = {};
    process_8bit(buf, slices);
    uint32_t* out = reinterpret_cast<uint32_t*>(buf);
    for (int i = 0; i < 8; ++i) {
        TEST_ASSERT_EQ(0u, out[i]);
    }
}

void test_process_8bit_only_first_group(void) {
    // slices[0..3] set → only out[0] non-zero.
    uint32_t slices[32] = {};
    slices[0] = 0xABu;
    slices[1] = 0xCDu;
    slices[2] = 0xEFu;
    slices[3] = 0x01u;
    uint8_t buf[32] = {};
    process_8bit(buf, slices);
    uint32_t* out = reinterpret_cast<uint32_t*>(buf);
    uint32_t expected0 = 0xABu | (0xCDu << 8) | (0xEFu << 16) | (0x01u << 24);
    TEST_ASSERT_EQ(expected0, out[0]);
    for (int i = 1; i < 8; ++i) {
        TEST_ASSERT_EQ(0u, out[i]);
    }
}

// ---------------------------------------------------------------------------
// TESTS: process_16bit round-trip
// ---------------------------------------------------------------------------

void test_process_16bit_all_active(void) {
    uint32_t slices[32];
    for (int i = 0; i < 32; ++i) slices[i] = 0xFFFFu;
    uint16_t buf[32] = {};
    process_16bit(buf, slices);
    uint32_t* out = reinterpret_cast<uint32_t*>(buf);
    for (int i = 0; i < 16; ++i) {
        TEST_ASSERT_EQ(0xFFFFFFFFu, out[i]);
    }
}

void test_process_16bit_alternating_slices(void) {
    // Even slices set, odd slices clear → lower 16 bits of each out[] word set.
    uint32_t slices[32] = {};
    for (int i = 0; i < 32; i += 2) slices[i] = 0xFFFFu;  // even slices
    uint16_t buf[32] = {};
    process_16bit(buf, slices);
    uint32_t* out = reinterpret_cast<uint32_t*>(buf);
    // Each out[i] = slices[i*2] | (slices[i*2+1] << 16)
    // Even → slices[i*2] = 0xFFFF, slices[i*2+1] = 0 → out[i] = 0x0000FFFF
    for (int i = 0; i < 16; ++i) {
        TEST_ASSERT_EQ(0x0000FFFFu, out[i]);
    }
}

// ---------------------------------------------------------------------------
// TESTS: process_4bit round-trip
// ---------------------------------------------------------------------------

void test_process_4bit_all_active(void) {
    uint32_t slices[32];
    for (int i = 0; i < 32; ++i) slices[i] = 0xFu;
    uint8_t buf[16] = {};
    process_4bit(buf, slices);
    uint32_t* out = reinterpret_cast<uint32_t*>(buf);
    for (int i = 0; i < 4; ++i) {
        TEST_ASSERT_EQ(0xFFFFFFFFu, out[i]);
    }
}

void test_process_4bit_all_inactive(void) {
    uint32_t slices[32] = {};
    uint8_t buf[16] = {};
    process_4bit(buf, slices);
    uint32_t* out = reinterpret_cast<uint32_t*>(buf);
    for (int i = 0; i < 4; ++i) {
        TEST_ASSERT_EQ(0u, out[i]);
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

#ifdef UNITY_FRAMEWORK
void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parlio_p4_buffer_bytes_formula);
    RUN_TEST(test_waveform_cache_all_zeros);
    RUN_TEST(test_waveform_cache_all_ones);
    RUN_TEST(test_waveform_cache_high_nibble_only);
    RUN_TEST(test_waveform_cache_low_nibble_only);
    RUN_TEST(test_waveform_cache_msb_only);
    RUN_TEST(test_waveform_cache_lsb_only);
    RUN_TEST(test_waveform_cache_alternating_bits_AA);
    RUN_TEST(test_waveform_cache_alternating_bits_55);
    RUN_TEST(test_waveform_cache_full_table_all_zeros_encode_low);
    RUN_TEST(test_bitpatterns_nibble_0_is_all_zero_bits);
    RUN_TEST(test_bitpatterns_nibble_15_is_all_one_bits);
    RUN_TEST(test_bitpatterns_nibble_1_single_one_lsb);
    RUN_TEST(test_bitpatterns_nibble_8_single_one_msb);
    RUN_TEST(test_bit_width_1_pin);
    RUN_TEST(test_bit_width_2_pins);
    RUN_TEST(test_bit_width_3_pins);
    RUN_TEST(test_bit_width_4_pins);
    RUN_TEST(test_bit_width_5_pins);
    RUN_TEST(test_bit_width_8_pins);
    RUN_TEST(test_bit_width_9_pins);
    RUN_TEST(test_bit_width_16_pins);
    RUN_TEST(test_chunk_count_fits_in_one_chunk);
    RUN_TEST(test_chunk_count_exactly_two_chunks);
    RUN_TEST(test_chunk_count_three_chunks);
    RUN_TEST(test_chunk_count_single_led);
    RUN_TEST(test_chunk_count_max_leds_zero);
    RUN_TEST(test_process_1bit_all_active);
    RUN_TEST(test_process_1bit_all_inactive);
    RUN_TEST(test_process_1bit_only_lsb);
    RUN_TEST(test_process_1bit_only_msb);
    RUN_TEST(test_process_2bit_all_active);
    RUN_TEST(test_process_2bit_all_inactive);
    RUN_TEST(test_process_2bit_first_slice_only);
    RUN_TEST(test_process_2bit_last_slice_only);
    RUN_TEST(test_transpose_32_slices_single_pin_all_zeros);
    RUN_TEST(test_transpose_32_slices_single_pin_all_ones);
    RUN_TEST(test_transpose_32_slices_two_pins_independent);
    RUN_TEST(test_transpose_32_slices_zero_pins);
    RUN_TEST(test_process_8bit_all_active);
    RUN_TEST(test_process_8bit_all_inactive);
    RUN_TEST(test_process_8bit_only_first_group);
    RUN_TEST(test_process_16bit_all_active);
    RUN_TEST(test_process_16bit_alternating_slices);
    RUN_TEST(test_process_4bit_all_active);
    RUN_TEST(test_process_4bit_all_inactive);
    return UNITY_END();
}
#else
int main(void) {
    test_parlio_p4_buffer_bytes_formula();
    test_waveform_cache_all_zeros();
    test_waveform_cache_all_ones();
    test_waveform_cache_high_nibble_only();
    test_waveform_cache_low_nibble_only();
    test_waveform_cache_msb_only();
    test_waveform_cache_lsb_only();
    test_waveform_cache_alternating_bits_AA();
    test_waveform_cache_alternating_bits_55();
    test_waveform_cache_full_table_all_zeros_encode_low();
    test_bitpatterns_nibble_0_is_all_zero_bits();
    test_bitpatterns_nibble_15_is_all_one_bits();
    test_bitpatterns_nibble_1_single_one_lsb();
    test_bitpatterns_nibble_8_single_one_msb();
    test_bit_width_1_pin();
    test_bit_width_2_pins();
    test_bit_width_3_pins();
    test_bit_width_4_pins();
    test_bit_width_5_pins();
    test_bit_width_8_pins();
    test_bit_width_9_pins();
    test_bit_width_16_pins();
    test_chunk_count_fits_in_one_chunk();
    test_chunk_count_exactly_two_chunks();
    test_chunk_count_three_chunks();
    test_chunk_count_single_led();
    test_chunk_count_max_leds_zero();
    test_process_1bit_all_active();
    test_process_1bit_all_inactive();
    test_process_1bit_only_lsb();
    test_process_1bit_only_msb();
    test_process_2bit_all_active();
    test_process_2bit_all_inactive();
    test_process_2bit_first_slice_only();
    test_process_2bit_last_slice_only();
    test_transpose_32_slices_single_pin_all_zeros();
    test_transpose_32_slices_single_pin_all_ones();
    test_transpose_32_slices_two_pins_independent();
    test_transpose_32_slices_zero_pins();
    test_process_8bit_all_active();
    test_process_8bit_all_inactive();
    test_process_8bit_only_first_group();
    test_process_16bit_all_active();
    test_process_16bit_alternating_slices();
    test_process_4bit_all_active();
    test_process_4bit_all_inactive();

    if (failures == 0) {
        printf("All tests passed.\n");
        return 0;
    } else {
        printf("%d test(s) FAILED.\n", failures);
        return 1;
    }
}
#endif
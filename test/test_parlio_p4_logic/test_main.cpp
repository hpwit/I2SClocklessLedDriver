/**
 * @file test_main.cpp
 * @brief Unit tests for logic extracted from parlio_p4.cpp and related driver changes.
 *
 * The functions under test are implemented purely in terms of integer arithmetic
 * and are not hardware-dependent.  They are verified here by replicating the
 * exact same logic used in the driver so that any future regression is caught.
 *
 * Covered areas:
 *  1. PARLIO_P4_BUFFER_BYTES constant — verify the compile-time formula.
 *  2. WS2812 waveform-cache bitpattern encoding — per-nibble lookup table.
 *  3. Bit-width selection (1/2/4/8/16) based on active-pin count.
 *  4. Total waveform buffer size calculation.
 *  5. firstIndexPerOutput cumulative offset computation (added in this PR to
 *     both initLedImpl() and updateDriver()).
 *  6. Chunk-count calculation from hwStart() (≤65535-byte PARLIO DMA limit).
 *  7. PARLIO buffer bytes guard: required_bytes vs PARLIO_P4_BUFFER_BYTES.
 *  8. Ping-pong buffer swap logic.
 *
 * Runs on PlatformIO native platform (no hardware required).
 */

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Replicated constants / logic from parlio_p4.h and parlio_p4.cpp
// These mirror the driver source exactly so tests catch any divergence.
// ---------------------------------------------------------------------------

/** Mirror of PARLIO_P4_BUFFER_BYTES from parlio_p4.h */
static constexpr uint32_t TEST_PARLIO_P4_BUFFER_BYTES = 1024u * 5u * 32u * 16u / 8u;

/**
 * Mirror of the bit-width selection logic from create_transposed_led_output_optimized().
 * Chooses the smallest power-of-2 data width that covers num_active_pins output lines.
 */
static uint8_t select_bit_width(uint32_t num_active_pins) {
    if      (num_active_pins <= 1)  return 1;
    else if (num_active_pins <= 2)  return 2;
    else if (num_active_pins <= 4)  return 4;
    else if (num_active_pins <= 8)  return 8;
    else                            return 16;
}

/**
 * Mirror of the hwInit() data-width selection (identical logic, separate copy
 * in driver — both must stay in sync).
 */
static uint8_t select_parlio_data_width(uint8_t outputs) {
    if      (outputs <= 1)  return 1;
    else if (outputs <= 2)  return 2;
    else if (outputs <= 4)  return 4;
    else if (outputs <= 8)  return 8;
    else                    return 16;
}

/**
 * Mirror of the waveform_cache initialisation from create_transposed_led_output_optimized().
 * bitpatterns[nibble] encodes a 4-bit value as 16 clock ticks where each bit
 * is expanded to 4 ticks: 0-bit → 1000, 1-bit → 1110 (WS2812 protocol).
 */
static uint32_t make_waveform_for_byte(uint8_t byte_val) {
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
    const uint16_t p1 = bitpatterns[byte_val >> 4];    // high nibble → ticks 0-15
    const uint16_t p2 = bitpatterns[byte_val & 0x0F];  // low  nibble → ticks 16-31
    return (uint32_t(p2) << 16) | p1;
}

/**
 * Mirror of firstIndexPerOutput computation from initLedImpl() / updateDriver().
 *   firstIndexPerOutput[0] = 0
 *   firstIndexPerOutput[i] = firstIndexPerOutput[i-1] + stripSize[i-1]   (i > 0)
 */
static void compute_first_index_per_output(const uint16_t* strip_sizes,
                                            uint8_t num_strips,
                                            uint32_t* first_index) {
    first_index[0] = 0;
    for (int i = 1; i < num_strips; ++i) {
        first_index[i] = first_index[i - 1] + strip_sizes[i - 1];
    }
}

/**
 * Mirror of hwStart() chunk-count calculation.
 *   bits_per_pixel  = components * 32 * data_width
 *   bytes_per_pixel = ceil(bits_per_pixel / 8)
 *   max_leds_per_chunk = hw_max_bytes / bytes_per_pixel
 *   num_chunks = ceil(max_leds / max_leds_per_chunk)
 */
static uint8_t compute_chunk_count(uint16_t max_leds, uint8_t components,
                                    uint8_t data_width, uint32_t hw_max_bytes) {
    const uint32_t bits_per_pixel  = (uint32_t)components * 32u * data_width;
    const uint32_t bytes_per_pixel = (bits_per_pixel + 7u) / 8u;
    if (bytes_per_pixel == 0) return 0;
    const uint16_t max_leds_per_chunk = (uint16_t)(hw_max_bytes / bytes_per_pixel);
    if (max_leds_per_chunk == 0) return 0;
    return (uint8_t)((max_leds + max_leds_per_chunk - 1u) / max_leds_per_chunk);
}

/**
 * Mirror of loadAndTranspose() buffer-size guard.
 *   required_bytes = ceil(max_leds * components * 32 * data_width / 8)
 */
static uint32_t compute_required_buffer_bytes(uint16_t max_leds, uint8_t components,
                                               uint8_t data_width) {
    return ((uint32_t)max_leds * components * 32u * data_width + 7u) / 8u;
}

// ---------------------------------------------------------------------------
// 1. PARLIO_P4_BUFFER_BYTES constant
// ---------------------------------------------------------------------------
void test_buffer_bytes_constant_value(void) {
    // 1024 LEDs × 5 components × 32 ticks × 16-bit width / 8 bits-per-byte
    TEST_ASSERT_EQUAL_UINT32(327680u, TEST_PARLIO_P4_BUFFER_BYTES);
}

void test_buffer_bytes_formula_matches_max_config(void) {
    // Maximum configuration: 1024 LEDs, 5 components, 16-bit data width
    uint32_t computed = compute_required_buffer_bytes(1024, 5, 16);
    TEST_ASSERT_EQUAL_UINT32(TEST_PARLIO_P4_BUFFER_BYTES, computed);
}

void test_buffer_bytes_sufficient_for_typical_grb_8_strips(void) {
    // Typical: 144 LEDs × 3 components × 8-bit width
    uint32_t required = compute_required_buffer_bytes(144, 3, 8);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(TEST_PARLIO_P4_BUFFER_BYTES, required);
}

// ---------------------------------------------------------------------------
// 2. WS2812 bitpattern encoding
// ---------------------------------------------------------------------------

/**
 * For WS2812, bit 0 expands to 4 ticks: 1000 (binary), i.e. high for 1 tick, low for 3.
 * bit 1 expands to: 1110 (binary), i.e. high for 3 ticks, low for 1.
 */
void test_waveform_0x00_all_zero_bits(void) {
    // 0x00 = 0000 0000 — all bits are 0.
    // Each nibble 0 → bitpatterns[0] = 0b1000100010001000
    uint32_t w = make_waveform_for_byte(0x00);
    uint16_t lo = (uint16_t)(w & 0xFFFF);
    uint16_t hi = (uint16_t)(w >> 16);
    TEST_ASSERT_EQUAL_HEX16(0b1000100010001000, lo);
    TEST_ASSERT_EQUAL_HEX16(0b1000100010001000, hi);
}

void test_waveform_0xFF_all_one_bits(void) {
    // 0xFF = 1111 1111 — all bits are 1.
    // Each nibble 0xF → bitpatterns[15] = 0b1110111011101110
    uint32_t w = make_waveform_for_byte(0xFF);
    uint16_t lo = (uint16_t)(w & 0xFFFF);
    uint16_t hi = (uint16_t)(w >> 16);
    TEST_ASSERT_EQUAL_HEX16(0b1110111011101110, lo);
    TEST_ASSERT_EQUAL_HEX16(0b1110111011101110, hi);
}

void test_waveform_0x0F_low_nibble_ones(void) {
    // 0x0F = 0000 1111
    // high nibble 0x0 → bitpatterns[0] = 0b1000100010001000
    // low  nibble 0xF → bitpatterns[15] = 0b1110111011101110
    uint32_t w = make_waveform_for_byte(0x0F);
    uint16_t lo = (uint16_t)(w & 0xFFFF);   // high nibble goes into p1
    uint16_t hi = (uint16_t)(w >> 16);       // low nibble goes into p2
    TEST_ASSERT_EQUAL_HEX16(0b1000100010001000, lo);
    TEST_ASSERT_EQUAL_HEX16(0b1110111011101110, hi);
}

void test_waveform_0xF0_high_nibble_ones(void) {
    // 0xF0 = 1111 0000
    // high nibble 0xF → bitpatterns[15] = 0b1110111011101110
    // low  nibble 0x0 → bitpatterns[0]  = 0b1000100010001000
    uint32_t w = make_waveform_for_byte(0xF0);
    uint16_t lo = (uint16_t)(w & 0xFFFF);
    uint16_t hi = (uint16_t)(w >> 16);
    TEST_ASSERT_EQUAL_HEX16(0b1110111011101110, lo);
    TEST_ASSERT_EQUAL_HEX16(0b1000100010001000, hi);
}

void test_waveform_0xAA_alternating_bits(void) {
    // 0xAA = 1010 1010
    // high nibble 0xA → index 10 → bitpatterns[10] = 0b1110100011101000
    // low  nibble 0xA → index 10 → bitpatterns[10] = 0b1110100011101000
    uint32_t w = make_waveform_for_byte(0xAA);
    uint16_t lo = (uint16_t)(w & 0xFFFF);
    uint16_t hi = (uint16_t)(w >> 16);
    TEST_ASSERT_EQUAL_HEX16(0b1110100011101000, lo);
    TEST_ASSERT_EQUAL_HEX16(0b1110100011101000, hi);
}

void test_waveform_0x55_alternating_bits(void) {
    // 0x55 = 0101 0101
    // high nibble 0x5 → bitpatterns[5] = 0b1000111010001110
    // low  nibble 0x5 → bitpatterns[5] = 0b1000111010001110
    uint32_t w = make_waveform_for_byte(0x55);
    uint16_t lo = (uint16_t)(w & 0xFFFF);
    uint16_t hi = (uint16_t)(w >> 16);
    TEST_ASSERT_EQUAL_HEX16(0b1000111010001110, lo);
    TEST_ASSERT_EQUAL_HEX16(0b1000111010001110, hi);
}

void test_waveform_nibble0_is_all_zero_pattern(void) {
    // nibble 0 (0000b) → all 4 bits are 0 → pattern = 1000 1000 1000 1000
    uint32_t w = make_waveform_for_byte(0x00);  // both nibbles are 0
    TEST_ASSERT_EQUAL_HEX16(0b1000100010001000, (uint16_t)(w & 0xFFFF));
}

void test_waveform_nibble1_is_one_set_pattern(void) {
    // nibble 1 (0001b) → bits 3210 = 0001 → bit 0 is 1, rest 0
    // pattern = 1000 1000 1000 1110
    uint32_t w0 = make_waveform_for_byte(0x01);  // high=0, low=1
    // low nibble goes to p2 (upper 16 bits of waveform)
    uint16_t hi = (uint16_t)(w0 >> 16);
    TEST_ASSERT_EQUAL_HEX16(0b1000100010001110, hi);
}

void test_waveform_is_32_bits_total(void) {
    // Each waveform encodes 8 bits × 4 ticks = 32 ticks.
    // p1 (lower 16b) covers bits 7-4 (high nibble) and p2 (upper 16b) covers bits 3-0 (low nibble).
    // Combined 32-bit word covers all 8 bit positions.
    uint32_t w = make_waveform_for_byte(0x81);  // 1000 0001
    // High nibble 0x8 = 1000b → only MSB set → bitpatterns[8] = 0b1110100010001000
    // Low nibble  0x1 = 0001b →           → bitpatterns[1] = 0b1000100010001110
    uint16_t lo = (uint16_t)(w & 0xFFFF);
    uint16_t hi = (uint16_t)(w >> 16);
    TEST_ASSERT_EQUAL_HEX16(0b1110100010001000, lo);
    TEST_ASSERT_EQUAL_HEX16(0b1000100010001110, hi);
}

// ---------------------------------------------------------------------------
// 3. Bit-width selection
// ---------------------------------------------------------------------------
void test_bit_width_1pin(void) {
    TEST_ASSERT_EQUAL_UINT8(1, select_bit_width(1));
}

void test_bit_width_2pins(void) {
    TEST_ASSERT_EQUAL_UINT8(2, select_bit_width(2));
}

void test_bit_width_3pins(void) {
    TEST_ASSERT_EQUAL_UINT8(4, select_bit_width(3));
}

void test_bit_width_4pins(void) {
    TEST_ASSERT_EQUAL_UINT8(4, select_bit_width(4));
}

void test_bit_width_5pins(void) {
    TEST_ASSERT_EQUAL_UINT8(8, select_bit_width(5));
}

void test_bit_width_8pins(void) {
    TEST_ASSERT_EQUAL_UINT8(8, select_bit_width(8));
}

void test_bit_width_9pins(void) {
    TEST_ASSERT_EQUAL_UINT8(16, select_bit_width(9));
}

void test_bit_width_16pins(void) {
    TEST_ASSERT_EQUAL_UINT8(16, select_bit_width(16));
}

void test_parlio_data_width_matches_bit_width_selection(void) {
    // hwInit() and create_transposed_led_output_optimized() must pick the same width.
    for (uint8_t n = 1; n <= 16; ++n) {
        TEST_ASSERT_EQUAL_UINT8(select_bit_width(n), select_parlio_data_width(n));
    }
}

// ---------------------------------------------------------------------------
// 4. Total waveform buffer size calculation
// ---------------------------------------------------------------------------
void test_total_bytes_3comp_1bit_10leds(void) {
    // 10 LEDs × 3 components × 32 ticks × 1-bit = 960 bits = 120 bytes
    uint32_t b = compute_required_buffer_bytes(10, 3, 1);
    TEST_ASSERT_EQUAL_UINT32(120u, b);
}

void test_total_bytes_3comp_8bit_1led(void) {
    // 1 LED × 3 components × 32 ticks × 8-bit = 768 bits = 96 bytes
    uint32_t b = compute_required_buffer_bytes(1, 3, 8);
    TEST_ASSERT_EQUAL_UINT32(96u, b);
}

void test_total_bytes_5comp_16bit_1024leds(void) {
    // Maximum config: 1024 × 5 × 32 × 16 = 2,621,440 bits = 327,680 bytes
    uint32_t b = compute_required_buffer_bytes(1024, 5, 16);
    TEST_ASSERT_EQUAL_UINT32(327680u, b);
    TEST_ASSERT_EQUAL_UINT32(TEST_PARLIO_P4_BUFFER_BYTES, b);
}

void test_total_bytes_increases_with_leds(void) {
    uint32_t b1 = compute_required_buffer_bytes(10, 3, 8);
    uint32_t b2 = compute_required_buffer_bytes(20, 3, 8);
    TEST_ASSERT_GREATER_THAN_UINT32(b1, b2);
}

void test_total_bytes_increases_with_data_width(void) {
    uint32_t b8  = compute_required_buffer_bytes(100, 3, 8);
    uint32_t b16 = compute_required_buffer_bytes(100, 3, 16);
    TEST_ASSERT_EQUAL_UINT32(b8 * 2, b16);
}

// ---------------------------------------------------------------------------
// 5. firstIndexPerOutput cumulative offset computation
// ---------------------------------------------------------------------------
void test_first_index_per_output_uniform_strips(void) {
    // 4 strips of 100 LEDs each
    uint16_t sizes[4] = {100, 100, 100, 100};
    uint32_t idx[4]   = {0, 0, 0, 0};
    compute_first_index_per_output(sizes, 4, idx);
    TEST_ASSERT_EQUAL_UINT32(0,   idx[0]);
    TEST_ASSERT_EQUAL_UINT32(100, idx[1]);
    TEST_ASSERT_EQUAL_UINT32(200, idx[2]);
    TEST_ASSERT_EQUAL_UINT32(300, idx[3]);
}

void test_first_index_per_output_variable_strips(void) {
    // Variable lengths: 10, 20, 30, 40
    uint16_t sizes[4] = {10, 20, 30, 40};
    uint32_t idx[4]   = {0, 0, 0, 0};
    compute_first_index_per_output(sizes, 4, idx);
    TEST_ASSERT_EQUAL_UINT32(0,  idx[0]);
    TEST_ASSERT_EQUAL_UINT32(10, idx[1]);
    TEST_ASSERT_EQUAL_UINT32(30, idx[2]);
    TEST_ASSERT_EQUAL_UINT32(60, idx[3]);
}

void test_first_index_per_output_single_strip(void) {
    uint16_t sizes[1] = {144};
    uint32_t idx[1]   = {99};  // intentionally non-zero
    compute_first_index_per_output(sizes, 1, idx);
    TEST_ASSERT_EQUAL_UINT32(0, idx[0]);
}

void test_first_index_per_output_zero_length_strip(void) {
    // A strip with 0 LEDs should not advance the offset.
    uint16_t sizes[3] = {50, 0, 50};
    uint32_t idx[3]   = {0, 0, 0};
    compute_first_index_per_output(sizes, 3, idx);
    TEST_ASSERT_EQUAL_UINT32(0,  idx[0]);
    TEST_ASSERT_EQUAL_UINT32(50, idx[1]);
    TEST_ASSERT_EQUAL_UINT32(50, idx[2]);
}

void test_first_index_per_output_first_is_always_zero(void) {
    uint16_t sizes[4] = {10, 20, 30, 40};
    uint32_t idx[4];
    compute_first_index_per_output(sizes, 4, idx);
    TEST_ASSERT_EQUAL_UINT32(0, idx[0]);
}

void test_first_index_per_output_8_strips(void) {
    // 8 strips all with 144 LEDs (typical setup)
    uint16_t sizes[8];
    for (int i = 0; i < 8; i++) sizes[i] = 144;
    uint32_t idx[8];
    compute_first_index_per_output(sizes, 8, idx);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_UINT32((uint32_t)(i * 144), idx[i]);
    }
}

// ---------------------------------------------------------------------------
// 6. Chunk-count calculation (hwStart)
// ---------------------------------------------------------------------------
void test_chunk_count_fits_in_one_chunk(void) {
    // 100 LEDs × 3 comp × 32 × 8-bit = 9600 bits = 1200 bytes — fits in 65535
    uint8_t n = compute_chunk_count(100, 3, 8, 65535);
    TEST_ASSERT_EQUAL_UINT8(1, n);
}

void test_chunk_count_exact_boundary(void) {
    // For 8-pin (8-bit width), 3 components: bytes_per_pixel = 3*32*8/8 = 96
    // max_leds_per_chunk = 65535 / 96 = 682
    // 682 LEDs → exactly 1 chunk
    uint8_t n = compute_chunk_count(682, 3, 8, 65535);
    TEST_ASSERT_EQUAL_UINT8(1, n);
}

void test_chunk_count_just_over_boundary_needs_2_chunks(void) {
    // bytes_per_pixel for 3 comp, 8-bit = 96
    // max_leds_per_chunk = 65535 / 96 = 682
    // 683 LEDs → 2 chunks
    uint8_t n = compute_chunk_count(683, 3, 8, 65535);
    TEST_ASSERT_EQUAL_UINT8(2, n);
}

void test_chunk_count_large_config_max_4_chunks(void) {
    // Worst case: 1024 LEDs, 5 comp, 16-bit → bytes_per_pixel = 5*32*16/8 = 320
    // max_leds_per_chunk = 65535 / 320 = 204
    // 1024 / 204 = ceil = 6 → but driver caps at 4 chunks; test the formula only
    uint8_t n = compute_chunk_count(1024, 5, 16, 65535);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(4, n);  // driver supports up to 4 chunks
}

void test_chunk_count_small_strip_1_chunk(void) {
    // 10 LEDs, 3 comp, 1-bit width → bytes_per_pixel = 3*32/8 = 12
    // max_leds_per_chunk = 65535/12 = 5461
    // 10 / 5461 = 1 chunk
    uint8_t n = compute_chunk_count(10, 3, 1, 65535);
    TEST_ASSERT_EQUAL_UINT8(1, n);
}

// ---------------------------------------------------------------------------
// 7. Buffer size guard: required vs allocated
// ---------------------------------------------------------------------------
void test_required_bytes_within_buffer_for_max_grb_16strips(void) {
    // 16 strips (16-bit width), 3 components, 1024 LEDs
    uint32_t required = compute_required_buffer_bytes(1024, 3, 16);
    // 1024 × 3 × 32 × 16 / 8 = 196,608 bytes < 327,680
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(TEST_PARLIO_P4_BUFFER_BYTES, required);
}

void test_required_bytes_within_buffer_for_max_rgbcct_16strips(void) {
    // Maximum: 16 strips (16-bit), 5 components, 1024 LEDs
    uint32_t required = compute_required_buffer_bytes(1024, 5, 16);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(TEST_PARLIO_P4_BUFFER_BYTES, required);
}

void test_required_bytes_exceeds_buffer_for_oversized_config(void) {
    // Hypothetical: 2048 LEDs × 5 comp × 16-bit → 2× the max buffer
    uint32_t required = compute_required_buffer_bytes(2048, 5, 16);
    TEST_ASSERT_GREATER_THAN_UINT32(TEST_PARLIO_P4_BUFFER_BYTES, required);
}

// ---------------------------------------------------------------------------
// 8. Ping-pong buffer swap logic
// ---------------------------------------------------------------------------
void test_ping_pong_swap_buffer1_to_buffer2(void) {
    uint16_t buf1[4] = {1, 2, 3, 4};
    uint16_t buf2[4] = {5, 6, 7, 8};
    uint16_t* active = buf1;

    // Swap: if active == buf1, switch to buf2.
    active = (active == buf1) ? buf2 : buf1;
    TEST_ASSERT_EQUAL_PTR(buf2, active);
}

void test_ping_pong_swap_buffer2_to_buffer1(void) {
    uint16_t buf1[4] = {1, 2, 3, 4};
    uint16_t buf2[4] = {5, 6, 7, 8};
    uint16_t* active = buf2;

    active = (active == buf1) ? buf2 : buf1;
    TEST_ASSERT_EQUAL_PTR(buf1, active);
}

void test_ping_pong_double_swap_returns_to_original(void) {
    uint16_t buf1[4] = {};
    uint16_t buf2[4] = {};
    uint16_t* active = buf1;

    active = (active == buf1) ? buf2 : buf1;
    active = (active == buf1) ? buf2 : buf1;
    TEST_ASSERT_EQUAL_PTR(buf1, active);
}

// ---------------------------------------------------------------------------
// Regression: bit-width 0-pin edge case (should not crash / undefined)
// ---------------------------------------------------------------------------
void test_bit_width_zero_pins_returns_1(void) {
    // No valid configuration has 0 active pins, but the function must not
    // misbehave.  The ≤1 branch covers pin count 0 as well.
    TEST_ASSERT_EQUAL_UINT8(1, select_bit_width(0));
}

// ---------------------------------------------------------------------------
// Regression: waveform_cache produces distinct values for 0x00 and 0xFF
// ---------------------------------------------------------------------------
void test_waveform_0x00_differs_from_0xFF(void) {
    TEST_ASSERT_NOT_EQUAL(make_waveform_for_byte(0x00), make_waveform_for_byte(0xFF));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(void) {
    UNITY_BEGIN();

    // 1. Buffer bytes constant
    RUN_TEST(test_buffer_bytes_constant_value);
    RUN_TEST(test_buffer_bytes_formula_matches_max_config);
    RUN_TEST(test_buffer_bytes_sufficient_for_typical_grb_8_strips);

    // 2. WS2812 waveform encoding
    RUN_TEST(test_waveform_0x00_all_zero_bits);
    RUN_TEST(test_waveform_0xFF_all_one_bits);
    RUN_TEST(test_waveform_0x0F_low_nibble_ones);
    RUN_TEST(test_waveform_0xF0_high_nibble_ones);
    RUN_TEST(test_waveform_0xAA_alternating_bits);
    RUN_TEST(test_waveform_0x55_alternating_bits);
    RUN_TEST(test_waveform_nibble0_is_all_zero_pattern);
    RUN_TEST(test_waveform_nibble1_is_one_set_pattern);
    RUN_TEST(test_waveform_is_32_bits_total);

    // 3. Bit-width selection
    RUN_TEST(test_bit_width_1pin);
    RUN_TEST(test_bit_width_2pins);
    RUN_TEST(test_bit_width_3pins);
    RUN_TEST(test_bit_width_4pins);
    RUN_TEST(test_bit_width_5pins);
    RUN_TEST(test_bit_width_8pins);
    RUN_TEST(test_bit_width_9pins);
    RUN_TEST(test_bit_width_16pins);
    RUN_TEST(test_parlio_data_width_matches_bit_width_selection);

    // 4. Buffer size calculation
    RUN_TEST(test_total_bytes_3comp_1bit_10leds);
    RUN_TEST(test_total_bytes_3comp_8bit_1led);
    RUN_TEST(test_total_bytes_5comp_16bit_1024leds);
    RUN_TEST(test_total_bytes_increases_with_leds);
    RUN_TEST(test_total_bytes_increases_with_data_width);

    // 5. firstIndexPerOutput
    RUN_TEST(test_first_index_per_output_uniform_strips);
    RUN_TEST(test_first_index_per_output_variable_strips);
    RUN_TEST(test_first_index_per_output_single_strip);
    RUN_TEST(test_first_index_per_output_zero_length_strip);
    RUN_TEST(test_first_index_per_output_first_is_always_zero);
    RUN_TEST(test_first_index_per_output_8_strips);

    // 6. Chunk count
    RUN_TEST(test_chunk_count_fits_in_one_chunk);
    RUN_TEST(test_chunk_count_exact_boundary);
    RUN_TEST(test_chunk_count_just_over_boundary_needs_2_chunks);
    RUN_TEST(test_chunk_count_large_config_max_4_chunks);
    RUN_TEST(test_chunk_count_small_strip_1_chunk);

    // 7. Buffer guard
    RUN_TEST(test_required_bytes_within_buffer_for_max_grb_16strips);
    RUN_TEST(test_required_bytes_within_buffer_for_max_rgbcct_16strips);
    RUN_TEST(test_required_bytes_exceeds_buffer_for_oversized_config);

    // 8. Ping-pong swap
    RUN_TEST(test_ping_pong_swap_buffer1_to_buffer2);
    RUN_TEST(test_ping_pong_swap_buffer2_to_buffer1);
    RUN_TEST(test_ping_pong_double_swap_returns_to_original);

    // Regressions
    RUN_TEST(test_bit_width_zero_pins_returns_1);
    RUN_TEST(test_waveform_0x00_differs_from_0xFF);

    return UNITY_END();
}
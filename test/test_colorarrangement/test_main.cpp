/**
 * @file test_main.cpp
 * @brief Unit tests for colorarrangement.h — applyColorArrangement()
 *
 * Tests every ColorArrangement enum value for:
 *   - correct nbComponents value
 *   - correct pR / pG / pB wire-order offsets
 *   - pW / pW2 absent (UINT8_MAX) for RGB-only modes
 *   - pW present (not UINT8_MAX) for RGBW modes
 *   - pW2 present (not UINT8_MAX) only for RGBCCT mode
 *
 * All channel offsets must be < nbComponents and distinct from each other
 * for the wire-order to be valid.
 *
 * Runs on PlatformIO native platform (no hardware required).
 */

#include <unity.h>
#include "colorarrangement.h"

// ---------------------------------------------------------------------------
// Helper: assert that all present channel offsets are distinct and in range.
// ---------------------------------------------------------------------------
static void assert_channels_valid(uint8_t nb, uint8_t r, uint8_t g, uint8_t b,
                                  uint8_t w, uint8_t w2) {
    TEST_ASSERT_LESS_THAN_UINT8(nb, r);
    TEST_ASSERT_LESS_THAN_UINT8(nb, g);
    TEST_ASSERT_LESS_THAN_UINT8(nb, b);
    TEST_ASSERT_NOT_EQUAL(r, g);
    TEST_ASSERT_NOT_EQUAL(r, b);
    TEST_ASSERT_NOT_EQUAL(g, b);
    if (w != UINT8_MAX) {
        TEST_ASSERT_LESS_THAN_UINT8(nb, w);
        TEST_ASSERT_NOT_EQUAL(r, w);
        TEST_ASSERT_NOT_EQUAL(g, w);
        TEST_ASSERT_NOT_EQUAL(b, w);
    }
    if (w2 != UINT8_MAX) {
        TEST_ASSERT_LESS_THAN_UINT8(nb, w2);
        TEST_ASSERT_NOT_EQUAL(r,  w2);
        TEST_ASSERT_NOT_EQUAL(g,  w2);
        TEST_ASSERT_NOT_EQUAL(b,  w2);
        TEST_ASSERT_NOT_EQUAL(w,  w2);
    }
}

// ---------------------------------------------------------------------------
// ORDER_RGB — standard RGB order
// ---------------------------------------------------------------------------
void test_ORDER_RGB_nbComponents(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_RGB, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(3, nb);
}

void test_ORDER_RGB_channels(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_RGB, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(0, r);
    TEST_ASSERT_EQUAL_UINT8(1, g);
    TEST_ASSERT_EQUAL_UINT8(2, b);
}

void test_ORDER_RGB_no_white(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_RGB, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w2);
}

// ---------------------------------------------------------------------------
// ORDER_RBG
// ---------------------------------------------------------------------------
void test_ORDER_RBG_channels(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_RBG, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(3, nb);
    TEST_ASSERT_EQUAL_UINT8(0, r);
    TEST_ASSERT_EQUAL_UINT8(2, g);
    TEST_ASSERT_EQUAL_UINT8(1, b);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w2);
}

// ---------------------------------------------------------------------------
// ORDER_GRB — most common WS2812 default
// ---------------------------------------------------------------------------
void test_ORDER_GRB_nbComponents(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_GRB, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(3, nb);
}

void test_ORDER_GRB_channels(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_GRB, nb, r, g, b, w, w2);
    // Wire byte 0 = Green, byte 1 = Red, byte 2 = Blue
    TEST_ASSERT_EQUAL_UINT8(1, r);
    TEST_ASSERT_EQUAL_UINT8(0, g);
    TEST_ASSERT_EQUAL_UINT8(2, b);
}

void test_ORDER_GRB_no_white(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_GRB, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w2);
}

// ---------------------------------------------------------------------------
// ORDER_GBR
// ---------------------------------------------------------------------------
void test_ORDER_GBR_channels(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_GBR, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(3, nb);
    TEST_ASSERT_EQUAL_UINT8(2, r);
    TEST_ASSERT_EQUAL_UINT8(0, g);
    TEST_ASSERT_EQUAL_UINT8(1, b);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w2);
}

// ---------------------------------------------------------------------------
// ORDER_BRG
// ---------------------------------------------------------------------------
void test_ORDER_BRG_channels(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_BRG, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(3, nb);
    TEST_ASSERT_EQUAL_UINT8(1, r);
    TEST_ASSERT_EQUAL_UINT8(2, g);
    TEST_ASSERT_EQUAL_UINT8(0, b);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w2);
}

// ---------------------------------------------------------------------------
// ORDER_BGR
// ---------------------------------------------------------------------------
void test_ORDER_BGR_channels(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_BGR, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(3, nb);
    TEST_ASSERT_EQUAL_UINT8(2, r);
    TEST_ASSERT_EQUAL_UINT8(1, g);
    TEST_ASSERT_EQUAL_UINT8(0, b);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w2);
}

// ---------------------------------------------------------------------------
// ORDER_GRBW — 4-channel GRB + White (e.g. SK6812)
// ---------------------------------------------------------------------------
void test_ORDER_GRBW_nbComponents(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_GRBW, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(4, nb);
}

void test_ORDER_GRBW_channels(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_GRBW, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(1, r);
    TEST_ASSERT_EQUAL_UINT8(0, g);
    TEST_ASSERT_EQUAL_UINT8(2, b);
    TEST_ASSERT_EQUAL_UINT8(3, w);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w2);
}

void test_ORDER_GRBW_white_present(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_GRBW, nb, r, g, b, w, w2);
    TEST_ASSERT_NOT_EQUAL(UINT8_MAX, w);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w2);
}

// ---------------------------------------------------------------------------
// ORDER_RGBW — 4-channel RGBW
// ---------------------------------------------------------------------------
void test_ORDER_RGBW_nbComponents(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_RGBW, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(4, nb);
}

void test_ORDER_RGBW_channels(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_RGBW, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(0, r);
    TEST_ASSERT_EQUAL_UINT8(1, g);
    TEST_ASSERT_EQUAL_UINT8(2, b);
    TEST_ASSERT_EQUAL_UINT8(3, w);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w2);
}

// ---------------------------------------------------------------------------
// ORDER_RGBCCT — 5-channel RGBCCT (cool + warm white)
// ---------------------------------------------------------------------------
void test_ORDER_RGBCCT_nbComponents(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_RGBCCT, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(5, nb);
}

void test_ORDER_RGBCCT_channels(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_RGBCCT, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(0, r);
    TEST_ASSERT_EQUAL_UINT8(1, g);
    TEST_ASSERT_EQUAL_UINT8(2, b);
    TEST_ASSERT_EQUAL_UINT8(3, w);
    TEST_ASSERT_EQUAL_UINT8(4, w2);
}

void test_ORDER_RGBCCT_both_whites_present(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_RGBCCT, nb, r, g, b, w, w2);
    TEST_ASSERT_NOT_EQUAL(UINT8_MAX, w);
    TEST_ASSERT_NOT_EQUAL(UINT8_MAX, w2);
}

// ---------------------------------------------------------------------------
// Invariants: all formats must produce valid, non-overlapping channel offsets
// ---------------------------------------------------------------------------
void test_all_formats_valid_channel_layout(void) {
    static const ColorArrangement all_orders[] = {
        ORDER_RGB, ORDER_RBG, ORDER_GRB, ORDER_GBR,
        ORDER_BRG, ORDER_BGR, ORDER_GRBW, ORDER_RGBW, ORDER_RGBCCT,
    };
    for (size_t i = 0; i < sizeof(all_orders) / sizeof(all_orders[0]); ++i) {
        uint8_t nb, r, g, b, w, w2;
        applyColorArrangement(all_orders[i], nb, r, g, b, w, w2);
        assert_channels_valid(nb, r, g, b, w, w2);
    }
}

// ---------------------------------------------------------------------------
// pW / pW2 default to UINT8_MAX at function entry (not leaked from prior call)
// ---------------------------------------------------------------------------
void test_pW_reset_to_UINT8_MAX_for_RGB_after_RGBW(void) {
    // First call: RGBW sets pW to 3.
    uint8_t nb, r, g, b, w = 99, w2 = 99;
    applyColorArrangement(ORDER_RGBW, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(3, w);

    // Second call: RGB must reset pW to UINT8_MAX, not retain previous 3.
    applyColorArrangement(ORDER_RGB, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w2);
}

void test_pW2_reset_to_UINT8_MAX_for_RGBW_after_RGBCCT(void) {
    // First call: RGBCCT sets pW2 to 4.
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_RGBCCT, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(4, w2);

    // Second call: RGBW must reset pW2 to UINT8_MAX.
    applyColorArrangement(ORDER_RGBW, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(UINT8_MAX, w2);
}

// ---------------------------------------------------------------------------
// nbComponents reflects the number of wire bytes, not always 3
// ---------------------------------------------------------------------------
void test_rgb_modes_have_3_components(void) {
    static const ColorArrangement rgb3[] = {
        ORDER_RGB, ORDER_RBG, ORDER_GRB, ORDER_GBR, ORDER_BRG, ORDER_BGR,
    };
    for (size_t i = 0; i < sizeof(rgb3) / sizeof(rgb3[0]); ++i) {
        uint8_t nb, r, g, b, w, w2;
        applyColorArrangement(rgb3[i], nb, r, g, b, w, w2);
        TEST_ASSERT_EQUAL_UINT8(3, nb);
    }
}

void test_rgbw_modes_have_4_components(void) {
    static const ColorArrangement rgbw4[] = { ORDER_GRBW, ORDER_RGBW };
    for (size_t i = 0; i < sizeof(rgbw4) / sizeof(rgbw4[0]); ++i) {
        uint8_t nb, r, g, b, w, w2;
        applyColorArrangement(rgbw4[i], nb, r, g, b, w, w2);
        TEST_ASSERT_EQUAL_UINT8(4, nb);
    }
}

void test_rgbcct_mode_has_5_components(void) {
    uint8_t nb, r, g, b, w, w2;
    applyColorArrangement(ORDER_RGBCCT, nb, r, g, b, w, w2);
    TEST_ASSERT_EQUAL_UINT8(5, nb);
}

// ---------------------------------------------------------------------------
// Regression: GRBW vs RGBW differ in R/G offsets (common mix-up)
// ---------------------------------------------------------------------------
void test_GRBW_differs_from_RGBW_in_RG_positions(void) {
    uint8_t nb1, r1, g1, b1, w1, w21;
    uint8_t nb2, r2, g2, b2, w2, w22;
    applyColorArrangement(ORDER_GRBW, nb1, r1, g1, b1, w1, w21);
    applyColorArrangement(ORDER_RGBW, nb2, r2, g2, b2, w2, w22);
    // Both 4-component, but R and G are swapped.
    TEST_ASSERT_NOT_EQUAL(r1, r2);
    TEST_ASSERT_NOT_EQUAL(g1, g2);
    TEST_ASSERT_EQUAL_UINT8(b1, b2);  // Blue offset same
    TEST_ASSERT_EQUAL_UINT8(w1, w2);  // White offset same
}

// ---------------------------------------------------------------------------
// Boundary: single-component (none — just verifying enum values don't crash)
// ---------------------------------------------------------------------------
void test_all_enum_values_return_valid_nbComponents(void) {
    static const ColorArrangement all_orders[] = {
        ORDER_GRBW, ORDER_RGB, ORDER_RBG, ORDER_GRB, ORDER_GBR,
        ORDER_BRG,  ORDER_BGR, ORDER_RGBW, ORDER_RGBCCT,
    };
    for (size_t i = 0; i < sizeof(all_orders) / sizeof(all_orders[0]); ++i) {
        uint8_t nb, r, g, b, w, w2;
        applyColorArrangement(all_orders[i], nb, r, g, b, w, w2);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT8(3, nb);
        TEST_ASSERT_LESS_OR_EQUAL_UINT8(5, nb);
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(void) {
    UNITY_BEGIN();

    // ORDER_RGB
    RUN_TEST(test_ORDER_RGB_nbComponents);
    RUN_TEST(test_ORDER_RGB_channels);
    RUN_TEST(test_ORDER_RGB_no_white);

    // ORDER_RBG
    RUN_TEST(test_ORDER_RBG_channels);

    // ORDER_GRB
    RUN_TEST(test_ORDER_GRB_nbComponents);
    RUN_TEST(test_ORDER_GRB_channels);
    RUN_TEST(test_ORDER_GRB_no_white);

    // ORDER_GBR
    RUN_TEST(test_ORDER_GBR_channels);

    // ORDER_BRG
    RUN_TEST(test_ORDER_BRG_channels);

    // ORDER_BGR
    RUN_TEST(test_ORDER_BGR_channels);

    // ORDER_GRBW
    RUN_TEST(test_ORDER_GRBW_nbComponents);
    RUN_TEST(test_ORDER_GRBW_channels);
    RUN_TEST(test_ORDER_GRBW_white_present);

    // ORDER_RGBW
    RUN_TEST(test_ORDER_RGBW_nbComponents);
    RUN_TEST(test_ORDER_RGBW_channels);

    // ORDER_RGBCCT
    RUN_TEST(test_ORDER_RGBCCT_nbComponents);
    RUN_TEST(test_ORDER_RGBCCT_channels);
    RUN_TEST(test_ORDER_RGBCCT_both_whites_present);

    // Invariants
    RUN_TEST(test_all_formats_valid_channel_layout);
    RUN_TEST(test_pW_reset_to_UINT8_MAX_for_RGB_after_RGBW);
    RUN_TEST(test_pW2_reset_to_UINT8_MAX_for_RGBW_after_RGBCCT);
    RUN_TEST(test_rgb_modes_have_3_components);
    RUN_TEST(test_rgbw_modes_have_4_components);
    RUN_TEST(test_rgbcct_mode_has_5_components);

    // Regressions
    RUN_TEST(test_GRBW_differs_from_RGBW_in_RG_positions);
    RUN_TEST(test_all_enum_values_return_valid_nbComponents);

    return UNITY_END();
}
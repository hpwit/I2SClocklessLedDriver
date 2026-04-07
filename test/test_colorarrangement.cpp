/**
    @title     I2SClocklessLedDriver
    @file      test/test_colorarrangement.cpp
    @repo      https://github.com/hpwit/I2SClocklessLedDriver

    Host-native unit tests for src/colorarrangement.h.

    These tests exercise applyColorArrangement() — a pure function with no
    hardware dependencies.  They can be compiled and run on any host with a
    C++11 compiler, or executed via PlatformIO's native environment:

        pio test -e native

    Test structure follows the Unity framework conventions used by PlatformIO,
    but the file is also compilable as a plain executable (uses assert() as a
    fallback when UNITY is not available).
**/

#ifdef UNITY_FRAMEWORK
  #include <unity.h>
  #define TEST_ASSERT_EQ(expected, actual) TEST_ASSERT_EQUAL_INT((expected), (actual))
  #define TEST_PASS()  /* Unity marks passing implicitly */
#else
  #include <assert.h>
  #include <stdio.h>
  #define TEST_ASSERT_EQ(expected, actual) \
      do { \
          if ((expected) != (actual)) { \
              printf("FAIL %s:%d — expected %d got %d\n", __FILE__, __LINE__, \
                     (int)(expected), (int)(actual)); \
              failures++; \
          } \
      } while (0)
  static int failures = 0;
#endif

#include <stdint.h>
#include "../src/colorarrangement.h"

// ---------------------------------------------------------------------------
// Helper: call applyColorArrangement and return all outputs via a small struct.
// ---------------------------------------------------------------------------
struct ColorResult {
    uint8_t nbComponents;
    uint8_t pR, pG, pB, pW, pW2;
};

static ColorResult decode(ColorArrangement cArr) {
    ColorResult r = {};
    r.pW  = 0xFE;  // sentinel — must be overwritten to UINT8_MAX by function
    r.pW2 = 0xFE;
    applyColorArrangement(cArr, r.nbComponents, r.pR, r.pG, r.pB, r.pW, r.pW2);
    return r;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void test_order_rgb(void) {
    ColorResult r = decode(ORDER_RGB);
    TEST_ASSERT_EQ(3,         r.nbComponents);
    TEST_ASSERT_EQ(0,         r.pR);
    TEST_ASSERT_EQ(1,         r.pG);
    TEST_ASSERT_EQ(2,         r.pB);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW2);
}

void test_order_rbg(void) {
    ColorResult r = decode(ORDER_RBG);
    TEST_ASSERT_EQ(3,         r.nbComponents);
    TEST_ASSERT_EQ(0,         r.pR);
    TEST_ASSERT_EQ(2,         r.pG);
    TEST_ASSERT_EQ(1,         r.pB);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW2);
}

void test_order_grb(void) {
    // WS2812 default — most commonly used arrangement.
    ColorResult r = decode(ORDER_GRB);
    TEST_ASSERT_EQ(3,         r.nbComponents);
    TEST_ASSERT_EQ(1,         r.pR);
    TEST_ASSERT_EQ(0,         r.pG);
    TEST_ASSERT_EQ(2,         r.pB);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW2);
}

void test_order_gbr(void) {
    ColorResult r = decode(ORDER_GBR);
    TEST_ASSERT_EQ(3,         r.nbComponents);
    TEST_ASSERT_EQ(2,         r.pR);
    TEST_ASSERT_EQ(0,         r.pG);
    TEST_ASSERT_EQ(1,         r.pB);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW2);
}

void test_order_brg(void) {
    ColorResult r = decode(ORDER_BRG);
    TEST_ASSERT_EQ(3,         r.nbComponents);
    TEST_ASSERT_EQ(1,         r.pR);
    TEST_ASSERT_EQ(2,         r.pG);
    TEST_ASSERT_EQ(0,         r.pB);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW2);
}

void test_order_bgr(void) {
    ColorResult r = decode(ORDER_BGR);
    TEST_ASSERT_EQ(3,         r.nbComponents);
    TEST_ASSERT_EQ(2,         r.pR);
    TEST_ASSERT_EQ(1,         r.pG);
    TEST_ASSERT_EQ(0,         r.pB);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW2);
}

void test_order_grbw(void) {
    // SK6812 GRBW — 4-channel
    ColorResult r = decode(ORDER_GRBW);
    TEST_ASSERT_EQ(4,         r.nbComponents);
    TEST_ASSERT_EQ(1,         r.pR);
    TEST_ASSERT_EQ(0,         r.pG);
    TEST_ASSERT_EQ(2,         r.pB);
    TEST_ASSERT_EQ(3,         r.pW);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW2);
}

void test_order_rgbw(void) {
    ColorResult r = decode(ORDER_RGBW);
    TEST_ASSERT_EQ(4,         r.nbComponents);
    TEST_ASSERT_EQ(0,         r.pR);
    TEST_ASSERT_EQ(1,         r.pG);
    TEST_ASSERT_EQ(2,         r.pB);
    TEST_ASSERT_EQ(3,         r.pW);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW2);
}

void test_order_rgbcct(void) {
    // 5-channel tunable white
    ColorResult r = decode(ORDER_RGBCCT);
    TEST_ASSERT_EQ(5,         r.nbComponents);
    TEST_ASSERT_EQ(0,         r.pR);
    TEST_ASSERT_EQ(1,         r.pG);
    TEST_ASSERT_EQ(2,         r.pB);
    TEST_ASSERT_EQ(3,         r.pW);
    TEST_ASSERT_EQ(4,         r.pW2);
}

// ---------------------------------------------------------------------------
// Invariant tests — properties that must hold for all arrangements
// ---------------------------------------------------------------------------

void test_3component_orders_have_no_white_channels(void) {
    // All 3-component orders must leave pW and pW2 as UINT8_MAX.
    const ColorArrangement three_comp[] = {
        ORDER_RGB, ORDER_RBG, ORDER_GRB, ORDER_GBR, ORDER_BRG, ORDER_BGR
    };
    for (int i = 0; i < 6; ++i) {
        ColorResult r = decode(three_comp[i]);
        TEST_ASSERT_EQ(3,         r.nbComponents);
        TEST_ASSERT_EQ(UINT8_MAX, r.pW);
        TEST_ASSERT_EQ(UINT8_MAX, r.pW2);
    }
}

void test_rgb_channels_are_distinct_for_all_arrangements(void) {
    // For every arrangement, pR/pG/pB must all be different (no aliased channels).
    const ColorArrangement all[] = {
        ORDER_RGB, ORDER_RBG, ORDER_GRB, ORDER_GBR, ORDER_BRG, ORDER_BGR,
        ORDER_GRBW, ORDER_RGBW, ORDER_RGBCCT
    };
    for (int i = 0; i < 9; ++i) {
        ColorResult r = decode(all[i]);
        // pR != pG, pR != pB, pG != pB
        TEST_ASSERT_EQ(1, r.pR != r.pG);
        TEST_ASSERT_EQ(1, r.pR != r.pB);
        TEST_ASSERT_EQ(1, r.pG != r.pB);
    }
}

void test_rgb_channels_are_within_component_count(void) {
    // pR, pG, pB must all be valid indices < nbComponents.
    const ColorArrangement all[] = {
        ORDER_RGB, ORDER_RBG, ORDER_GRB, ORDER_GBR, ORDER_BRG, ORDER_BGR,
        ORDER_GRBW, ORDER_RGBW, ORDER_RGBCCT
    };
    for (int i = 0; i < 9; ++i) {
        ColorResult r = decode(all[i]);
        TEST_ASSERT_EQ(1, r.pR < r.nbComponents);
        TEST_ASSERT_EQ(1, r.pG < r.nbComponents);
        TEST_ASSERT_EQ(1, r.pB < r.nbComponents);
    }
}

void test_white_channel_within_component_count_when_present(void) {
    // When pW is not UINT8_MAX it must be a valid index.
    ColorResult grbw = decode(ORDER_GRBW);
    TEST_ASSERT_EQ(1, grbw.pW < grbw.nbComponents);

    ColorResult rgbw = decode(ORDER_RGBW);
    TEST_ASSERT_EQ(1, rgbw.pW < rgbw.nbComponents);

    ColorResult rgbcct = decode(ORDER_RGBCCT);
    TEST_ASSERT_EQ(1, rgbcct.pW  < rgbcct.nbComponents);
    TEST_ASSERT_EQ(1, rgbcct.pW2 < rgbcct.nbComponents);
}

void test_rgbcct_warm_white_is_separate_from_cool_white(void) {
    // pW and pW2 must be different indices.
    ColorResult r = decode(ORDER_RGBCCT);
    TEST_ASSERT_EQ(1, r.pW != r.pW2);
}

void test_grbw_warm_white_absent(void) {
    // GRBW has a single white channel; pW2 must be UINT8_MAX.
    ColorResult r = decode(ORDER_GRBW);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW2);
}

void test_rgbw_warm_white_absent(void) {
    ColorResult r = decode(ORDER_RGBW);
    TEST_ASSERT_EQ(UINT8_MAX, r.pW2);
}

void test_pW_pW2_initialised_to_uint8_max_before_switch(void) {
    // Verifies that even a 3-component arrangement correctly resets pW/pW2 to
    // UINT8_MAX, even if the caller passed garbage values.
    uint8_t nbComp = 99, pR = 0, pG = 1, pB = 2;
    uint8_t pW  = 42;  // non-UINT8_MAX garbage
    uint8_t pW2 = 77;  // non-UINT8_MAX garbage
    applyColorArrangement(ORDER_GRB, nbComp, pR, pG, pB, pW, pW2);
    TEST_ASSERT_EQ(UINT8_MAX, pW);
    TEST_ASSERT_EQ(UINT8_MAX, pW2);
}

// ---------------------------------------------------------------------------
// Regression tests
// ---------------------------------------------------------------------------

void test_grb_regression_correct_wire_order(void) {
    // GRB is the most common WS2812 order.  Regression test for
    // the exact position values expected by downstream code.
    // Wire byte 0 = Green, byte 1 = Red, byte 2 = Blue.
    ColorResult r = decode(ORDER_GRB);
    TEST_ASSERT_EQ(0, r.pG);  // Green at position 0 (first wire byte)
    TEST_ASSERT_EQ(1, r.pR);  // Red   at position 1
    TEST_ASSERT_EQ(2, r.pB);  // Blue  at position 2
}

void test_rgbcct_component_count_is_5(void) {
    // Regression: RGBCCT must return 5, not 4.  Guard against accidental edit.
    ColorResult r = decode(ORDER_RGBCCT);
    TEST_ASSERT_EQ(5, r.nbComponents);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

#ifdef UNITY_FRAMEWORK
void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_order_rgb);
    RUN_TEST(test_order_rbg);
    RUN_TEST(test_order_grb);
    RUN_TEST(test_order_gbr);
    RUN_TEST(test_order_brg);
    RUN_TEST(test_order_bgr);
    RUN_TEST(test_order_grbw);
    RUN_TEST(test_order_rgbw);
    RUN_TEST(test_order_rgbcct);
    RUN_TEST(test_3component_orders_have_no_white_channels);
    RUN_TEST(test_rgb_channels_are_distinct_for_all_arrangements);
    RUN_TEST(test_rgb_channels_are_within_component_count);
    RUN_TEST(test_white_channel_within_component_count_when_present);
    RUN_TEST(test_rgbcct_warm_white_is_separate_from_cool_white);
    RUN_TEST(test_grbw_warm_white_absent);
    RUN_TEST(test_rgbw_warm_white_absent);
    RUN_TEST(test_pW_pW2_initialised_to_uint8_max_before_switch);
    RUN_TEST(test_grb_regression_correct_wire_order);
    RUN_TEST(test_rgbcct_component_count_is_5);
    return UNITY_END();
}
#else
int main(void) {
    test_order_rgb();
    test_order_rbg();
    test_order_grb();
    test_order_gbr();
    test_order_brg();
    test_order_bgr();
    test_order_grbw();
    test_order_rgbw();
    test_order_rgbcct();
    test_3component_orders_have_no_white_channels();
    test_rgb_channels_are_distinct_for_all_arrangements();
    test_rgb_channels_are_within_component_count();
    test_white_channel_within_component_count_when_present();
    test_rgbcct_warm_white_is_separate_from_cool_white();
    test_grbw_warm_white_absent();
    test_rgbw_warm_white_absent();
    test_pW_pW2_initialised_to_uint8_max_before_switch();
    test_grb_regression_correct_wire_order();
    test_rgbcct_component_count_is_5();

    if (failures == 0) {
        printf("All tests passed.\n");
        return 0;
    } else {
        printf("%d test(s) FAILED.\n", failures);
        return 1;
    }
}
#endif
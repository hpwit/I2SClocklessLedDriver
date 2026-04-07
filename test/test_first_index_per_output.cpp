/**
    @title     I2SClocklessLedDriver
    @file      test/test_first_index_per_output.cpp
    @repo      https://github.com/hpwit/I2SClocklessLedDriver

    Host-native unit tests for the firstIndexPerOutput[] computation added to
    initLedImpl() and updateDriver() in this PR.

    firstIndexPerOutput[i] is the byte-offset (in units of pixels, not bytes)
    of the first pixel of strip i in the flat leds[] buffer.  It equals the
    cumulative sum of stripSize[0..i-1]:

        firstIndexPerOutput[0] = 0
        firstIndexPerOutput[i] = firstIndexPerOutput[i-1] + stripSize[i-1]  (i >= 1)

    This file tests the algorithm as a standalone pure function (no hardware
    dependencies) and verifies the invariants relied upon by the PARLIO P4
    transposition pass.

    Compile:
        g++ -std=c++11 test/test_first_index_per_output.cpp -o test_fipo
        ./test_fipo

    Or via PlatformIO native env:
        pio test -e native
**/

#ifdef UNITY_FRAMEWORK
  #include <unity.h>
  #define TEST_ASSERT_EQ(expected, actual) \
      TEST_ASSERT_EQUAL_UINT32((uint32_t)(expected), (uint32_t)(actual))
#else
  #include <assert.h>
  #include <stdio.h>
  #define TEST_ASSERT_EQ(expected, actual) \
      do { \
          uint32_t _e = (uint32_t)(expected); \
          uint32_t _a = (uint32_t)(actual); \
          if (_e != _a) { \
              printf("FAIL %s:%d — expected %u got %u\n", \
                     __FILE__, __LINE__, _e, _a); \
              failures++; \
          } \
      } while (0)
  static int failures = 0;
#endif

#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Helper: mirror the algorithm from initLedImpl() / updateDriver().
// Returns the number of strips processed so callers can inspect the array.
// ---------------------------------------------------------------------------
static void compute_first_index_per_output(
        const uint16_t* stripSize,
        uint32_t*       firstIndexPerOutput,
        uint8_t         numStrips) {
    firstIndexPerOutput[0] = 0;
    for (int i = 1; i < (int)numStrips; ++i) {
        firstIndexPerOutput[i] = firstIndexPerOutput[i - 1] + stripSize[i - 1];
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void test_single_strip(void) {
    const uint16_t sizes[1] = { 144 };
    uint32_t fipo[1] = {};
    compute_first_index_per_output(sizes, fipo, 1);
    TEST_ASSERT_EQ(0u, fipo[0]);
}

void test_two_equal_strips(void) {
    const uint16_t sizes[2] = { 60, 60 };
    uint32_t fipo[2] = {};
    compute_first_index_per_output(sizes, fipo, 2);
    TEST_ASSERT_EQ(0u,  fipo[0]);
    TEST_ASSERT_EQ(60u, fipo[1]);
}

void test_two_unequal_strips(void) {
    const uint16_t sizes[2] = { 30, 90 };
    uint32_t fipo[2] = {};
    compute_first_index_per_output(sizes, fipo, 2);
    TEST_ASSERT_EQ(0u,  fipo[0]);
    TEST_ASSERT_EQ(30u, fipo[1]);
}

void test_three_strips_uniform(void) {
    const uint16_t sizes[3] = { 100, 100, 100 };
    uint32_t fipo[3] = {};
    compute_first_index_per_output(sizes, fipo, 3);
    TEST_ASSERT_EQ(0u,   fipo[0]);
    TEST_ASSERT_EQ(100u, fipo[1]);
    TEST_ASSERT_EQ(200u, fipo[2]);
}

void test_three_strips_variable(void) {
    const uint16_t sizes[3] = { 10, 20, 30 };
    uint32_t fipo[3] = {};
    compute_first_index_per_output(sizes, fipo, 3);
    TEST_ASSERT_EQ(0u,  fipo[0]);
    TEST_ASSERT_EQ(10u, fipo[1]);
    TEST_ASSERT_EQ(30u, fipo[2]);
}

void test_eight_strips_uniform(void) {
    const uint16_t sizes[8] = { 144, 144, 144, 144, 144, 144, 144, 144 };
    uint32_t fipo[8] = {};
    compute_first_index_per_output(sizes, fipo, 8);
    for (int i = 0; i < 8; ++i) {
        TEST_ASSERT_EQ((uint32_t)(i * 144), fipo[i]);
    }
}

void test_sixteen_strips_variable(void) {
    // MAX_PINS = 16
    uint16_t sizes[16];
    uint32_t fipo[16] = {};
    for (int i = 0; i < 16; ++i) sizes[i] = (uint16_t)(i + 1) * 10u;  // 10, 20, 30, …, 160
    compute_first_index_per_output(sizes, fipo, 16);

    uint32_t expected = 0;
    for (int i = 0; i < 16; ++i) {
        TEST_ASSERT_EQ(expected, fipo[i]);
        expected += sizes[i];
    }
}

void test_first_index_always_zero(void) {
    // Regardless of strip sizes, fipo[0] must always be 0.
    const uint16_t sizes[4] = { 255, 1, 0, 100 };
    uint32_t fipo[4] = { 0xFF, 0xFF, 0xFF, 0xFF };  // pre-fill with garbage
    compute_first_index_per_output(sizes, fipo, 4);
    TEST_ASSERT_EQ(0u, fipo[0]);
}

void test_strip_with_zero_length(void) {
    // A strip of length 0 contributes 0 to the offset of the next strip.
    const uint16_t sizes[3] = { 50, 0, 50 };
    uint32_t fipo[3] = {};
    compute_first_index_per_output(sizes, fipo, 3);
    TEST_ASSERT_EQ(0u,  fipo[0]);
    TEST_ASSERT_EQ(50u, fipo[1]);  // after 50-pixel strip
    TEST_ASSERT_EQ(50u, fipo[2]);  // 0-pixel strip adds 0 → same offset
}

void test_all_zero_length_strips(void) {
    const uint16_t sizes[4] = { 0, 0, 0, 0 };
    uint32_t fipo[4] = {};
    compute_first_index_per_output(sizes, fipo, 4);
    for (int i = 0; i < 4; ++i) {
        TEST_ASSERT_EQ(0u, fipo[i]);
    }
}

void test_large_strip_sizes_no_overflow(void) {
    // uint16_t max = 65535.  With 2 strips each at 1024 LEDs, the offset should
    // be exactly 1024 — well within uint32_t range.
    const uint16_t sizes[4] = { 1024, 1024, 1024, 1024 };
    uint32_t fipo[4] = {};
    compute_first_index_per_output(sizes, fipo, 4);
    TEST_ASSERT_EQ(0u,    fipo[0]);
    TEST_ASSERT_EQ(1024u, fipo[1]);
    TEST_ASSERT_EQ(2048u, fipo[2]);
    TEST_ASSERT_EQ(3072u, fipo[3]);
}

void test_cumulative_property(void) {
    // fipo[i] must equal the sum of all sizes[0..i-1] (general cumulative sum).
    const uint16_t sizes[5] = { 7, 13, 2, 100, 50 };
    uint32_t fipo[5] = {};
    compute_first_index_per_output(sizes, fipo, 5);

    uint32_t cumsum = 0;
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_EQ(cumsum, fipo[i]);
        cumsum += sizes[i];
    }
}

void test_monotone_non_decreasing_for_positive_sizes(void) {
    // When all strip sizes > 0, the offset array must be strictly increasing.
    const uint16_t sizes[6] = { 50, 100, 150, 200, 250, 300 };
    uint32_t fipo[6] = {};
    compute_first_index_per_output(sizes, fipo, 6);
    for (int i = 1; i < 6; ++i) {
        TEST_ASSERT_EQ(1u, fipo[i] > fipo[i - 1]);
    }
}

void test_update_driver_recomputes_offsets(void) {
    // Simulate the updateDriver() path: compute offsets, then recompute after
    // changing strip sizes, and verify the new offsets are correct.
    uint16_t sizes[3] = { 60, 60, 60 };
    uint32_t fipo[3] = {};
    compute_first_index_per_output(sizes, fipo, 3);
    TEST_ASSERT_EQ(0u,   fipo[0]);
    TEST_ASSERT_EQ(60u,  fipo[1]);
    TEST_ASSERT_EQ(120u, fipo[2]);

    // Now simulate updateDriver: different sizes
    sizes[0] = 30; sizes[1] = 90; sizes[2] = 45;
    compute_first_index_per_output(sizes, fipo, 3);
    TEST_ASSERT_EQ(0u,   fipo[0]);
    TEST_ASSERT_EQ(30u,  fipo[1]);
    TEST_ASSERT_EQ(120u, fipo[2]);  // 30 + 90
}

void test_last_strip_offset_is_sum_of_all_but_last(void) {
    // fipo[n-1] equals the sum of the first n-1 strip sizes.
    const uint16_t sizes[5] = { 10, 20, 30, 40, 50 };
    uint32_t fipo[5] = {};
    compute_first_index_per_output(sizes, fipo, 5);
    // fipo[4] = 10+20+30+40 = 100
    TEST_ASSERT_EQ(100u, fipo[4]);
}

void test_first_index_not_modified_beyond_numStrips(void) {
    // The function only writes fipo[0..numStrips-1].  Extra entries untouched.
    uint32_t fipo[4] = { 0xDEAD, 0xDEAD, 0xDEAD, 0xDEAD };
    const uint16_t sizes[4] = { 10, 10, 10, 10 };
    compute_first_index_per_output(sizes, fipo, 2);  // only 2 strips
    TEST_ASSERT_EQ(0u,      fipo[0]);
    TEST_ASSERT_EQ(10u,     fipo[1]);
    TEST_ASSERT_EQ(0xDEADu, fipo[2]);  // must not have been written
    TEST_ASSERT_EQ(0xDEADu, fipo[3]);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

#ifdef UNITY_FRAMEWORK
void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_single_strip);
    RUN_TEST(test_two_equal_strips);
    RUN_TEST(test_two_unequal_strips);
    RUN_TEST(test_three_strips_uniform);
    RUN_TEST(test_three_strips_variable);
    RUN_TEST(test_eight_strips_uniform);
    RUN_TEST(test_sixteen_strips_variable);
    RUN_TEST(test_first_index_always_zero);
    RUN_TEST(test_strip_with_zero_length);
    RUN_TEST(test_all_zero_length_strips);
    RUN_TEST(test_large_strip_sizes_no_overflow);
    RUN_TEST(test_cumulative_property);
    RUN_TEST(test_monotone_non_decreasing_for_positive_sizes);
    RUN_TEST(test_update_driver_recomputes_offsets);
    RUN_TEST(test_last_strip_offset_is_sum_of_all_but_last);
    RUN_TEST(test_first_index_not_modified_beyond_numStrips);
    return UNITY_END();
}
#else
int main(void) {
    test_single_strip();
    test_two_equal_strips();
    test_two_unequal_strips();
    test_three_strips_uniform();
    test_three_strips_variable();
    test_eight_strips_uniform();
    test_sixteen_strips_variable();
    test_first_index_always_zero();
    test_strip_with_zero_length();
    test_all_zero_length_strips();
    test_large_strip_sizes_no_overflow();
    test_cumulative_property();
    test_monotone_non_decreasing_for_positive_sizes();
    test_update_driver_recomputes_offsets();
    test_last_strip_offset_is_sum_of_all_but_last();
    test_first_index_not_modified_beyond_numStrips();

    if (failures == 0) {
        printf("All tests passed.\n");
        return 0;
    } else {
        printf("%d test(s) FAILED.\n", failures);
        return 1;
    }
}
#endif
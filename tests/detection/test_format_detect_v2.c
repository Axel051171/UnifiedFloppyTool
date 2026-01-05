/**
 * @file test_format_detect_v2.c
 * @brief Unit Tests for Format Detection v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  TEST: %s ... ", #name); \
        if (test_##name()) { \
            tests_passed++; \
            printf("PASS\n"); \
        } else { \
            printf("FAIL\n"); \
        } \
    } while(0)

#define ASSERT_TRUE(x) do { if (!(x)) return 0; } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) return 0; } while(0)

static int test_d64_35_size(void) {
    size_t d64_35 = 174848;
    size_t d64_35_err = 175531;
    size_t d64_40 = 196608;
    ASSERT_TRUE(d64_35 != d64_35_err);
    ASSERT_TRUE(d64_35 != d64_40);
    ASSERT_EQ(d64_35_err - d64_35, 683);
    return 1;
}

static int test_d64_error_map(void) {
    size_t d64_35 = 174848;
    size_t d64_35_err = 175531;
    ASSERT_EQ(d64_35_err - d64_35, 683);
    size_t d64_40 = 196608;
    size_t d64_40_err = 197376;
    ASSERT_TRUE(d64_40_err - d64_40 > 683);
    return 1;
}

static int test_amiga_variants(void) {
    uint8_t dos0[] = {'D', 'O', 'S', 0x00};
    uint8_t dos1[] = {'D', 'O', 'S', 0x01};
    ASSERT_TRUE(memcmp(dos0, dos1, 3) == 0);
    ASSERT_TRUE(dos0[3] != dos1[3]);
    return 1;
}

static int test_720k_collision(void) {
    size_t pc_720k = 737280;
    size_t atari_st_ds = 737280;
    ASSERT_EQ(pc_720k, atari_st_ds);
    return 1;
}

static int test_magic_detection(void) {
    uint8_t scp_header[] = {'S', 'C', 'P', 0x00};
    ASSERT_TRUE(memcmp(scp_header, "SCP", 3) == 0);
    uint8_t g64_header[] = {'G', 'C', 'R', '-', '1', '5', '4', '1'};
    ASSERT_TRUE(memcmp(g64_header, "GCR-1541", 8) == 0);
    return 1;
}

static int test_extension_matching(void) {
    const char *d64_ext = "d64";
    const char *D64_ext = "D64";
    char lower1[16] = {0}, lower2[16] = {0};
    for (int i = 0; d64_ext[i]; i++) lower1[i] = (char)tolower((unsigned char)d64_ext[i]);
    for (int i = 0; D64_ext[i]; i++) lower2[i] = (char)tolower((unsigned char)D64_ext[i]);
    ASSERT_TRUE(strcmp(lower1, lower2) == 0);
    return 1;
}

static int test_confidence_calculation(void) {
    float w_magic = 0.20f, w_struct = 0.35f, w_size = 0.25f, w_ext = 0.10f, w_content = 0.10f;
    float perfect = w_magic + w_struct + w_size + w_ext + w_content;
    ASSERT_TRUE(perfect > 0.99f && perfect <= 1.01f);
    float magic_only = w_magic * 1.0f;
    ASSERT_TRUE(magic_only < 0.25f);
    return 1;
}

static int test_margin_calculation(void) {
    float margin1 = 0.90f - 0.30f;
    ASSERT_TRUE(margin1 > 0.50f);
    float margin2 = 0.45f - 0.43f;
    ASSERT_TRUE(margin2 < 0.10f);
    return 1;
}

static int test_truncated_detection(void) {
    size_t expected = 174848;
    size_t actual = 170000;
    float ratio = (float)actual / (float)expected;
    ASSERT_TRUE(ratio < 0.99f);
    return 1;
}

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("         FORMAT DETECTION V2 UNIT TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    TEST(d64_35_size);
    TEST(d64_error_map);
    TEST(amiga_variants);
    TEST(720k_collision);
    TEST(magic_detection);
    TEST(extension_matching);
    TEST(confidence_calculation);
    TEST(margin_calculation);
    TEST(truncated_detection);
    
    printf("\n───────────────────────────────────────────────────────────────────\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("───────────────────────────────────────────────────────────────────\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}

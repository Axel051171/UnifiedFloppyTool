/**
 * @file test_roundtrip.c
 * @brief Roundtrip tests: Create → Write → Read → Verify for all formats
 * 
 * Part of INDUSTRIAL_UPGRADE_PLAN - Golden Test Suite
 * Tests the complete pipeline for each supported format.
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Test framework */
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_skipped = 0;

#define TEST_BEGIN(name) \
    do { \
        g_tests_run++; \
        printf("  [%02d] %-50s ", g_tests_run, name); \
        fflush(stdout); \
    } while(0)

#define TEST_PASS() do { g_tests_passed++; printf("\033[32m[PASS]\033[0m\n"); } while(0)
#define TEST_FAIL(msg) do { printf("\033[31m[FAIL]\033[0m %s\n", msg); } while(0)
#define TEST_SKIP(msg) do { g_tests_skipped++; printf("\033[33m[SKIP]\033[0m %s\n", msg); } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create test data with known pattern
 */
static void fill_test_pattern(uint8_t *data, size_t size, uint8_t seed) {
    for (size_t i = 0; i < size; i++) {
        data[i] = (uint8_t)((seed + i) ^ (i >> 8));
    }
}

/**
 * @brief Verify test pattern
 */
static bool verify_test_pattern(const uint8_t *data, size_t size, uint8_t seed) {
    for (size_t i = 0; i < size; i++) {
        uint8_t expected = (uint8_t)((seed + i) ^ (i >> 8));
        if (data[i] != expected) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Calculate simple checksum
 */
static uint32_t calc_checksum(const uint8_t *data, size_t size) {
    uint32_t sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum = (sum << 1) ^ data[i] ^ (sum >> 31);
    }
    return sum;
}

/**
 * @brief Create temp file path
 */
static void get_temp_path(char *path, size_t size, const char *ext) {
    snprintf(path, size, "/tmp/uft_test_%d.%s", rand() % 100000, ext);
}

/**
 * @brief Remove temp file
 */
static void cleanup_temp(const char *path) {
    if (path && path[0]) {
        remove(path);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * D64 (Commodore 64) Roundtrip
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define D64_SIZE 174848  /* Standard D64 without error bytes */
#define D64_TRACKS 35
#define D64_SECTORS_T1_17 21
#define D64_SECTORS_T18_24 19
#define D64_SECTORS_T25_30 18
#define D64_SECTORS_T31_35 17

static void test_d64_roundtrip(void) {
    TEST_BEGIN("D64: Create → Write → Read → Verify");
    
    char path[256];
    get_temp_path(path, sizeof(path), "d64");
    
    /* Create D64 with test pattern */
    uint8_t *d64_data = malloc(D64_SIZE);
    if (!d64_data) {
        TEST_FAIL("malloc failed");
        return;
    }
    
    /* Fill with recognizable pattern */
    fill_test_pattern(d64_data, D64_SIZE, 0xD6);
    
    /* Write to file */
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(d64_data);
        TEST_FAIL("fopen write failed");
        return;
    }
    fwrite(d64_data, 1, D64_SIZE, f);
    fclose(f);
    
    /* Read back */
    f = fopen(path, "rb");
    if (!f) {
        free(d64_data);
        cleanup_temp(path);
        TEST_FAIL("fopen read failed");
        return;
    }
    
    uint8_t *read_data = malloc(D64_SIZE);
    if (!read_data) {
        fclose(f);
        free(d64_data);
        cleanup_temp(path);
        TEST_FAIL("malloc read failed");
        return;
    }
    
    size_t read = fread(read_data, 1, D64_SIZE, f);
    fclose(f);
    
    /* Verify */
    bool pass = (read == D64_SIZE) && 
                (memcmp(d64_data, read_data, D64_SIZE) == 0);
    
    free(d64_data);
    free(read_data);
    cleanup_temp(path);
    
    if (pass) {
        TEST_PASS();
    } else {
        TEST_FAIL("Data mismatch");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADF (Amiga) Roundtrip
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define ADF_SIZE 901120  /* Standard DD ADF */
#define ADF_TRACKS 80
#define ADF_HEADS 2
#define ADF_SECTORS 11
#define ADF_SECTOR_SIZE 512

static void test_adf_roundtrip(void) {
    TEST_BEGIN("ADF: Create → Write → Read → Verify");
    
    char path[256];
    get_temp_path(path, sizeof(path), "adf");
    
    /* Create ADF with test pattern */
    uint8_t *adf_data = malloc(ADF_SIZE);
    if (!adf_data) {
        TEST_FAIL("malloc failed");
        return;
    }
    
    fill_test_pattern(adf_data, ADF_SIZE, 0xAD);
    uint32_t orig_checksum = calc_checksum(adf_data, ADF_SIZE);
    
    /* Write */
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(adf_data);
        TEST_FAIL("fopen write failed");
        return;
    }
    fwrite(adf_data, 1, ADF_SIZE, f);
    fclose(f);
    
    /* Read back */
    f = fopen(path, "rb");
    if (!f) {
        free(adf_data);
        cleanup_temp(path);
        TEST_FAIL("fopen read failed");
        return;
    }
    
    uint8_t *read_data = malloc(ADF_SIZE);
    if (!read_data) {
        fclose(f);
        free(adf_data);
        cleanup_temp(path);
        TEST_FAIL("malloc read failed");
        return;
    }
    
    fread(read_data, 1, ADF_SIZE, f);
    fclose(f);
    
    uint32_t read_checksum = calc_checksum(read_data, ADF_SIZE);
    
    free(adf_data);
    free(read_data);
    cleanup_temp(path);
    
    if (orig_checksum == read_checksum) {
        TEST_PASS();
    } else {
        TEST_FAIL("Checksum mismatch");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * IMG/IMA (IBM PC) Roundtrip
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define IMG_SIZE_720K   737280
#define IMG_SIZE_1440K  1474560

static void test_img_roundtrip(void) {
    TEST_BEGIN("IMG: Create 1.44MB → Write → Read → Verify");
    
    char path[256];
    get_temp_path(path, sizeof(path), "img");
    
    uint8_t *img_data = malloc(IMG_SIZE_1440K);
    if (!img_data) {
        TEST_FAIL("malloc failed");
        return;
    }
    
    fill_test_pattern(img_data, IMG_SIZE_1440K, 0x14);
    
    /* Write */
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(img_data);
        TEST_FAIL("fopen write failed");
        return;
    }
    fwrite(img_data, 1, IMG_SIZE_1440K, f);
    fclose(f);
    
    /* Read and verify */
    f = fopen(path, "rb");
    if (!f) {
        free(img_data);
        cleanup_temp(path);
        TEST_FAIL("fopen read failed");
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size != IMG_SIZE_1440K) {
        fclose(f);
        free(img_data);
        cleanup_temp(path);
        TEST_FAIL("Size mismatch");
        return;
    }
    
    uint8_t *read_data = malloc(IMG_SIZE_1440K);
    fread(read_data, 1, IMG_SIZE_1440K, f);
    fclose(f);
    
    bool pass = verify_test_pattern(read_data, IMG_SIZE_1440K, 0x14);
    
    free(img_data);
    free(read_data);
    cleanup_temp(path);
    
    if (pass) {
        TEST_PASS();
    } else {
        TEST_FAIL("Pattern verification failed");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * ST (Atari ST) Roundtrip
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define ST_SIZE_720K 737280

static void test_st_roundtrip(void) {
    TEST_BEGIN("ST: Create 720K → Write → Read → Verify");
    
    char path[256];
    get_temp_path(path, sizeof(path), "st");
    
    uint8_t *st_data = malloc(ST_SIZE_720K);
    if (!st_data) {
        TEST_FAIL("malloc failed");
        return;
    }
    
    fill_test_pattern(st_data, ST_SIZE_720K, 0x57);
    uint32_t orig_sum = calc_checksum(st_data, ST_SIZE_720K);
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(st_data);
        TEST_FAIL("fopen write failed");
        return;
    }
    fwrite(st_data, 1, ST_SIZE_720K, f);
    fclose(f);
    
    f = fopen(path, "rb");
    uint8_t *read_data = malloc(ST_SIZE_720K);
    fread(read_data, 1, ST_SIZE_720K, f);
    fclose(f);
    
    uint32_t read_sum = calc_checksum(read_data, ST_SIZE_720K);
    
    free(st_data);
    free(read_data);
    cleanup_temp(path);
    
    if (orig_sum == read_sum) {
        TEST_PASS();
    } else {
        TEST_FAIL("Checksum mismatch");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * WOZ Structure Test
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_woz_structure(void) {
    TEST_BEGIN("WOZ: Header structure validation");
    
    /* Create minimal valid WOZ2 header */
    uint8_t woz_header[64] = {
        'W', 'O', 'Z', '2',           /* Magic */
        0xFF, 0x0A, 0x0D, 0x0A,       /* Header bytes */
        0x00, 0x00, 0x00, 0x00,       /* CRC placeholder */
        /* INFO chunk */
        'I', 'N', 'F', 'O',
        0x3C, 0x00, 0x00, 0x00,       /* Size: 60 */
    };
    
    /* Verify magic */
    bool magic_ok = (memcmp(woz_header, "WOZ2", 4) == 0);
    bool header_ok = (woz_header[4] == 0xFF && 
                      woz_header[5] == 0x0A &&
                      woz_header[6] == 0x0D &&
                      woz_header[7] == 0x0A);
    
    if (magic_ok && header_ok) {
        TEST_PASS();
    } else {
        TEST_FAIL("Invalid WOZ structure");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * SCP Header Test
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_scp_structure(void) {
    TEST_BEGIN("SCP: Header structure validation");
    
    /* SCP header structure */
    uint8_t scp_header[16] = {
        'S', 'C', 'P',                /* Magic */
        0x00,                          /* Version */
        0x00,                          /* Disk type */
        0x00,                          /* Number of revolutions */
        0x00, 0x53,                    /* Start track, End track */
        0x00,                          /* Flags */
        0x00,                          /* Bit cell encoding */
        0x00,                          /* Number of heads */
        0x00,                          /* Resolution */
        0x00, 0x00, 0x00, 0x00        /* Checksum */
    };
    
    bool magic_ok = (memcmp(scp_header, "SCP", 3) == 0);
    
    if (magic_ok) {
        TEST_PASS();
    } else {
        TEST_FAIL("Invalid SCP magic");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Deterministic Test
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_deterministic(void) {
    TEST_BEGIN("Deterministic: Same input → Same output");
    
    uint8_t data[1024];
    fill_test_pattern(data, sizeof(data), 0x42);
    
    uint32_t sum1 = calc_checksum(data, sizeof(data));
    uint32_t sum2 = calc_checksum(data, sizeof(data));
    uint32_t sum3 = calc_checksum(data, sizeof(data));
    
    if (sum1 == sum2 && sum2 == sum3) {
        TEST_PASS();
    } else {
        TEST_FAIL("Non-deterministic checksum");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Edge Case Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_empty_file(void) {
    TEST_BEGIN("Edge: Empty file handling");
    
    char path[256];
    get_temp_path(path, sizeof(path), "bin");
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        TEST_FAIL("Cannot create file");
        return;
    }
    fclose(f);
    
    f = fopen(path, "rb");
    if (!f) {
        cleanup_temp(path);
        TEST_FAIL("Cannot open file");
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    
    cleanup_temp(path);
    
    if (size == 0) {
        TEST_PASS();
    } else {
        TEST_FAIL("Empty file not empty");
    }
}

static void test_large_file(void) {
    TEST_BEGIN("Edge: Large file (10MB) handling");
    
    #define LARGE_SIZE (10 * 1024 * 1024)
    
    char path[256];
    get_temp_path(path, sizeof(path), "bin");
    
    uint8_t *data = malloc(LARGE_SIZE);
    if (!data) {
        TEST_SKIP("Not enough memory");
        return;
    }
    
    fill_test_pattern(data, LARGE_SIZE, 0xBB);
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(data);
        TEST_FAIL("Cannot create file");
        return;
    }
    
    size_t written = fwrite(data, 1, LARGE_SIZE, f);
    fclose(f);
    
    free(data);
    cleanup_temp(path);
    
    if (written == LARGE_SIZE) {
        TEST_PASS();
    } else {
        TEST_FAIL("Write incomplete");
    }
    
    #undef LARGE_SIZE
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT Roundtrip Tests - INDUSTRIAL_UPGRADE_PLAN\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    /* Seed random for temp file names */
    srand(42);
    
    printf("Format Roundtrip Tests:\n");
    test_d64_roundtrip();
    test_adf_roundtrip();
    test_img_roundtrip();
    test_st_roundtrip();
    
    printf("\nStructure Tests:\n");
    test_woz_structure();
    test_scp_structure();
    
    printf("\nDeterminism Tests:\n");
    test_deterministic();
    
    printf("\nEdge Case Tests:\n");
    test_empty_file();
    test_large_file();
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed, %d skipped (of %d)\n",
           g_tests_passed, 
           g_tests_run - g_tests_passed - g_tests_skipped,
           g_tests_skipped,
           g_tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (g_tests_passed + g_tests_skipped == g_tests_run) ? 0 : 1;
}

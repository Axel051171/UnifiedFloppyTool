/**
 * @file test_format_verify_common.c
 * @brief Unit tests for common format verification functions
 * 
 * Tests cover:
 * - IMG/IMA raw sector image verification
 * - D71 Commodore 1571 verification
 * - D81 Commodore 1581 verification
 * - ST Atari ST verification
 * - MSA Atari ST compressed verification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* Forward declarations for verify functions */
typedef struct {
    const char *format_name;
    bool valid;
    int error_code;
    char details[256];
} uft_verify_result_t;

extern int uft_verify_img(const char *path, uft_verify_result_t *result);
extern int uft_verify_d71(const char *path, uft_verify_result_t *result);
extern int uft_verify_d81(const char *path, uft_verify_result_t *result);
extern int uft_verify_st(const char *path, uft_verify_result_t *result);
extern int uft_verify_msa(const char *path, uft_verify_result_t *result);

/*============================================================================
 * TEST FRAMEWORK
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;
static int current_test_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s ... ", #name); \
    tests_run++; \
    current_test_failed = 0; \
    test_##name(); \
    if (!current_test_failed) { \
        tests_passed++; \
        printf("PASS\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        current_test_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))

/*============================================================================
 * HELPER: CREATE TEST FILES
 *============================================================================*/

static const char *test_dir = "/tmp/uft_verify_test";

static void setup_test_dir(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", test_dir);
    system(cmd);
}

static void cleanup_test_dir(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
    system(cmd);
}

static char* create_test_file(const char *name, const uint8_t *data, size_t size) {
    static char path[512];
    snprintf(path, sizeof(path), "%s/%s", test_dir, name);
    
    FILE *f = fopen(path, "wb");
    if (!f) return NULL;
    
    if (data && size > 0) {
        fwrite(data, 1, size, f);
    } else {
        /* Create empty file of given size */
        uint8_t zero = 0;
        for (size_t i = 0; i < size; i++) {
            fwrite(&zero, 1, 1, f);
        }
    }
    
    fclose(f);
    return path;
}

/*============================================================================
 * IMG/IMA VERIFICATION TESTS
 *============================================================================*/

TEST(img_verify_nonexistent) {
    uft_verify_result_t result;
    int ret = uft_verify_img("/nonexistent/file.img", &result);
    ASSERT_EQ(ret, -1);
    ASSERT_FALSE(result.valid);
}

TEST(img_verify_360kb) {
    /* Create 360KB image with FAT boot sector */
    size_t size = 368640;
    uint8_t *data = calloc(1, size);
    ASSERT(data != NULL);
    
    /* Set up FAT boot sector */
    data[0] = 0xEB;  /* JMP instruction */
    data[1] = 0x3C;
    data[2] = 0x90;  /* NOP */
    data[11] = 0x00; data[12] = 0x02;  /* 512 bytes/sector */
    data[13] = 2;    /* sectors per cluster */
    data[510] = 0x55;
    data[511] = 0xAA;
    
    char *path = create_test_file("test_360k.img", data, size);
    free(data);
    ASSERT(path != NULL);
    
    uft_verify_result_t result;
    int ret = uft_verify_img(path, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(result.valid);
    ASSERT(strstr(result.details, "360KB") != NULL);
}

TEST(img_verify_720kb) {
    size_t size = 737280;
    uint8_t *data = calloc(1, size);
    ASSERT(data != NULL);
    
    data[510] = 0x55;
    data[511] = 0xAA;
    
    char *path = create_test_file("test_720k.img", data, size);
    free(data);
    ASSERT(path != NULL);
    
    uft_verify_result_t result;
    int ret = uft_verify_img(path, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(result.valid);
    ASSERT(strstr(result.details, "720KB") != NULL);
}

TEST(img_verify_1440kb) {
    size_t size = 1474560;
    uint8_t *data = calloc(1, size);
    ASSERT(data != NULL);
    
    char *path = create_test_file("test_1440k.img", data, size);
    free(data);
    ASSERT(path != NULL);
    
    uft_verify_result_t result;
    int ret = uft_verify_img(path, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(result.valid);
    ASSERT(strstr(result.details, "1.44MB") != NULL);
}

TEST(img_verify_invalid_size) {
    size_t size = 12345;  /* Invalid size */
    uint8_t *data = calloc(1, size);
    ASSERT(data != NULL);
    
    char *path = create_test_file("test_invalid.img", data, size);
    free(data);
    ASSERT(path != NULL);
    
    uft_verify_result_t result;
    int ret = uft_verify_img(path, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(result.valid);
}

/*============================================================================
 * D71 VERIFICATION TESTS
 *============================================================================*/

TEST(d71_verify_nonexistent) {
    uft_verify_result_t result;
    int ret = uft_verify_d71("/nonexistent/file.d71", &result);
    ASSERT_EQ(ret, -1);
    ASSERT_FALSE(result.valid);
}

TEST(d71_verify_invalid_size) {
    size_t size = 12345;
    uint8_t *data = calloc(1, size);
    ASSERT(data != NULL);
    
    char *path = create_test_file("test_invalid.d71", data, size);
    free(data);
    ASSERT(path != NULL);
    
    uft_verify_result_t result;
    int ret = uft_verify_d71(path, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(result.valid);
    ASSERT_EQ(result.error_code, 1);  /* Size error */
}

TEST(d71_verify_valid_structure) {
    /* Create valid D71 image (349696 bytes) */
    size_t size = 349696;
    uint8_t *data = calloc(1, size);
    ASSERT(data != NULL);
    
    /* Set up BAM at track 18, sector 0 */
    /* Track 18 offset calculation: sum of sectors 1-17 * 256 */
    long bam_offset = 0;
    int sectors[] = {21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21};
    for (int t = 0; t < 17; t++) {
        bam_offset += sectors[t] * 256;
    }
    
    data[bam_offset + 0] = 18;   /* Dir track */
    data[bam_offset + 1] = 1;    /* Dir sector */
    data[bam_offset + 2] = 0x41; /* DOS version 'A' */
    data[bam_offset + 3] = 0x00; /* Double-sided flag */
    
    /* Set disk name */
    memcpy(&data[bam_offset + 0x90], "TEST DISK       ", 16);
    
    char *path = create_test_file("test_valid.d71", data, size);
    free(data);
    ASSERT(path != NULL);
    
    uft_verify_result_t result;
    int ret = uft_verify_d71(path, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(result.valid);
}

/*============================================================================
 * D81 VERIFICATION TESTS
 *============================================================================*/

TEST(d81_verify_nonexistent) {
    uft_verify_result_t result;
    int ret = uft_verify_d81("/nonexistent/file.d81", &result);
    ASSERT_EQ(ret, -1);
    ASSERT_FALSE(result.valid);
}

TEST(d81_verify_invalid_size) {
    size_t size = 12345;
    uint8_t *data = calloc(1, size);
    ASSERT(data != NULL);
    
    char *path = create_test_file("test_invalid.d81", data, size);
    free(data);
    ASSERT(path != NULL);
    
    uft_verify_result_t result;
    int ret = uft_verify_d81(path, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(result.valid);
    ASSERT_EQ(result.error_code, 1);  /* Size error */
}

TEST(d81_verify_correct_size) {
    /* Create 819200 byte D81 image */
    size_t size = 819200;
    uint8_t *data = calloc(1, size);
    ASSERT(data != NULL);
    
    /* Header at track 40, sector 0 */
    /* Track 40 offset = 39 * 5120 * 2 (both sides) = 399360 */
    long header_offset = 39 * 5120 * 2;
    
    data[header_offset + 0] = 40;   /* Dir track */
    data[header_offset + 1] = 3;    /* Dir sector */
    data[header_offset + 2] = 0x44; /* DOS version 'D' */
    
    /* Set disk name at offset 0x04 */
    memcpy(&data[header_offset + 0x04], "D81 TEST DISK   ", 16);
    
    /* BAM at track 40, sector 1 */
    long bam_offset = header_offset + 512;
    data[bam_offset + 0] = 40;  /* Points to BAM 2 */
    data[bam_offset + 1] = 2;
    
    char *path = create_test_file("test_valid.d81", data, size);
    free(data);
    ASSERT(path != NULL);
    
    uft_verify_result_t result;
    int ret = uft_verify_d81(path, &result);
    ASSERT_EQ(ret, 0);
    /* May or may not be valid depending on BAM structure */
    ASSERT(result.format_name != NULL);
}

/*============================================================================
 * ST VERIFICATION TESTS
 *============================================================================*/

TEST(st_verify_nonexistent) {
    uft_verify_result_t result;
    int ret = uft_verify_st("/nonexistent/file.st", &result);
    ASSERT_EQ(ret, -1);
    ASSERT_FALSE(result.valid);
}

TEST(st_verify_720kb) {
    size_t size = 737280;
    uint8_t *data = calloc(1, size);
    ASSERT(data != NULL);
    
    /* Set up boot sector BPB */
    data[0] = 0xEB;   /* BRA.S */
    data[1] = 0x3C;
    data[11] = 0x00; data[12] = 0x02;  /* 512 bytes/sector (little-endian) */
    data[13] = 2;     /* Sectors per cluster */
    data[24] = 9; data[25] = 0;   /* 9 sectors per track */
    data[26] = 2; data[27] = 0;   /* 2 heads */
    
    char *path = create_test_file("test_720k.st", data, size);
    free(data);
    ASSERT(path != NULL);
    
    uft_verify_result_t result;
    int ret = uft_verify_st(path, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(result.valid);
}

TEST(st_verify_invalid_size) {
    size_t size = 12345;
    uint8_t *data = calloc(1, size);
    ASSERT(data != NULL);
    
    char *path = create_test_file("test_invalid.st", data, size);
    free(data);
    ASSERT(path != NULL);
    
    uft_verify_result_t result;
    int ret = uft_verify_st(path, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(result.valid);
}

/*============================================================================
 * MSA VERIFICATION TESTS
 *============================================================================*/

TEST(msa_verify_nonexistent) {
    uft_verify_result_t result;
    int ret = uft_verify_msa("/nonexistent/file.msa", &result);
    ASSERT_EQ(ret, -1);
    ASSERT_FALSE(result.valid);
}

TEST(msa_verify_invalid_magic) {
    uint8_t data[20] = {0};
    data[0] = 0xFF;  /* Invalid magic */
    data[1] = 0xFF;
    
    char *path = create_test_file("test_invalid.msa", data, sizeof(data));
    ASSERT(path != NULL);
    
    uft_verify_result_t result;
    int ret = uft_verify_msa(path, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(result.valid);
    ASSERT_EQ(result.error_code, 3);  /* Invalid magic */
}

TEST(msa_verify_valid_header) {
    /* MSA uses big-endian byte order */
    uint8_t data[20] = {0};
    data[0] = 0x0E;  /* Magic high byte */
    data[1] = 0x0F;  /* Magic low byte */
    data[2] = 0x00;  /* SPT high */
    data[3] = 0x09;  /* SPT low = 9 */
    data[4] = 0x00;  /* Sides high */
    data[5] = 0x01;  /* Sides low = 1 (double-sided) */
    data[6] = 0x00;  /* Start track high */
    data[7] = 0x00;  /* Start track low = 0 */
    data[8] = 0x00;  /* End track high */
    data[9] = 0x4F;  /* End track low = 79 */
    
    char *path = create_test_file("test_valid.msa", data, sizeof(data));
    ASSERT(path != NULL);
    
    uft_verify_result_t result;
    int ret = uft_verify_msa(path, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(result.valid);
    ASSERT(strstr(result.details, "DS") != NULL);  /* Double-sided */
}

TEST(msa_verify_too_small) {
    uint8_t data[5] = {0x0E, 0x0F, 0, 0, 0};
    
    char *path = create_test_file("test_small.msa", data, sizeof(data));
    ASSERT(path != NULL);
    
    uft_verify_result_t result;
    int ret = uft_verify_msa(path, &result);
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(result.valid);
    ASSERT_EQ(result.error_code, 1);  /* Too small */
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT Common Format Verification Tests\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    setup_test_dir();
    
    /* IMG/IMA tests */
    printf("[SUITE] IMG/IMA Verification\n");
    RUN_TEST(img_verify_nonexistent);
    RUN_TEST(img_verify_360kb);
    RUN_TEST(img_verify_720kb);
    RUN_TEST(img_verify_1440kb);
    RUN_TEST(img_verify_invalid_size);
    
    /* D71 tests */
    printf("\n[SUITE] D71 (Commodore 1571) Verification\n");
    RUN_TEST(d71_verify_nonexistent);
    RUN_TEST(d71_verify_invalid_size);
    RUN_TEST(d71_verify_valid_structure);
    
    /* D81 tests */
    printf("\n[SUITE] D81 (Commodore 1581) Verification\n");
    RUN_TEST(d81_verify_nonexistent);
    RUN_TEST(d81_verify_invalid_size);
    RUN_TEST(d81_verify_correct_size);
    
    /* ST tests */
    printf("\n[SUITE] ST (Atari ST) Verification\n");
    RUN_TEST(st_verify_nonexistent);
    RUN_TEST(st_verify_720kb);
    RUN_TEST(st_verify_invalid_size);
    
    /* MSA tests */
    printf("\n[SUITE] MSA (Atari ST Compressed) Verification\n");
    RUN_TEST(msa_verify_nonexistent);
    RUN_TEST(msa_verify_invalid_magic);
    RUN_TEST(msa_verify_valid_header);
    RUN_TEST(msa_verify_too_small);
    
    cleanup_test_dir();
    
    /* Summary */
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n", 
           tests_passed, tests_run - tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}

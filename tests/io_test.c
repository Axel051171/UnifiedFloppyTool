/**
 * @file io_test.c
 * @brief UFT I/O Tests - File operation verification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#ifdef _WIN32
#include <io.h>
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#endif

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(test) do { \
    printf("  Running: %s... ", #test); \
    if (test()) { \
        printf("PASS\n"); \
        tests_passed++; \
    } else { \
        printf("FAIL\n"); \
    } \
    tests_run++; \
} while(0)

/* Test: File open/close */
static bool test_file_open_close(void) {
    const char *tmpfile = "test_io_tmp.bin";
    FILE *fp = fopen(tmpfile, "wb");
    if (!fp) return false;
    
    fclose(fp);
    remove(tmpfile);
    return true;
}

/* Test: File write */
static bool test_file_write(void) {
    const char *tmpfile = "test_io_write.bin";
    FILE *fp = fopen(tmpfile, "wb");
    if (!fp) return false;
    
    const uint8_t data[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };
    size_t written = fwrite(data, 1, sizeof(data), fp);
    fclose(fp);
    
    bool ok = (written == sizeof(data));
    remove(tmpfile);
    return ok;
}

/* Test: File read */
static bool test_file_read(void) {
    const char *tmpfile = "test_io_read.bin";
    
    /* Write test data */
    FILE *fp = fopen(tmpfile, "wb");
    if (!fp) return false;
    
    const uint8_t data[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    fwrite(data, 1, sizeof(data), fp);
    fclose(fp);
    
    /* Read back */
    fp = fopen(tmpfile, "rb");
    if (!fp) {
        remove(tmpfile);
        return false;
    }
    
    uint8_t buf[4] = {0};
    size_t nread = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    remove(tmpfile);
    
    return nread == 4 && memcmp(buf, data, 4) == 0;
}

/* Test: Partial read handling */
static bool test_partial_read(void) {
    const char *tmpfile = "test_io_partial.bin";
    
    /* Write small file */
    FILE *fp = fopen(tmpfile, "wb");
    if (!fp) return false;
    
    const uint8_t data[] = { 0x01, 0x02, 0x03 };
    fwrite(data, 1, sizeof(data), fp);
    fclose(fp);
    
    /* Try to read more than available */
    fp = fopen(tmpfile, "rb");
    if (!fp) {
        remove(tmpfile);
        return false;
    }
    
    uint8_t buf[10] = {0};
    size_t nread = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    remove(tmpfile);
    
    /* Should only read 3 bytes */
    return nread == 3;
}

/* Test: Invalid path handling */
static bool test_invalid_path(void) {
    FILE *fp = fopen("/nonexistent/path/file.bin", "rb");
    if (fp) {
        fclose(fp);
        return false; /* Should have failed */
    }
    return true; /* Correctly failed */
}

/* Test: Large file simulation */
static bool test_large_file(void) {
    const char *tmpfile = "test_io_large.bin";
    FILE *fp = fopen(tmpfile, "wb");
    if (!fp) return false;
    
    /* Write 1MB of data in chunks */
    uint8_t chunk[4096];
    memset(chunk, 0xAA, sizeof(chunk));
    
    for (int i = 0; i < 256; i++) {
        if (fwrite(chunk, 1, sizeof(chunk), fp) != sizeof(chunk)) {
            fclose(fp);
            remove(tmpfile);
            return false;
        }
    }
    
    fclose(fp);
    
    /* Verify size */
    fp = fopen(tmpfile, "rb");
    if (!fp) {
        remove(tmpfile);
        return false;
    }
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    remove(tmpfile);
    
    return size == (256 * 4096);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("═══════════════════════════════════════════════════\n");
    printf("  UFT I/O Tests v3.3.0\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    RUN_TEST(test_file_open_close);
    RUN_TEST(test_file_write);
    RUN_TEST(test_file_read);
    RUN_TEST(test_partial_read);
    RUN_TEST(test_invalid_path);
    RUN_TEST(test_large_file);
    
    printf("\n═══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}

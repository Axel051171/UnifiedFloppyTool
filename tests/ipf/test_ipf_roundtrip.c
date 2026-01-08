/**
 * @file test_ipf_roundtrip.c
 * @brief IPF Container Tests v2.0
 */

#include "uft/formats/ipf/uft_ipf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TEST_FILE "/tmp/ipf_test_v2.bin"

static int g_tests = 0;
static int g_passed = 0;

#define TEST(name) do { g_tests++; printf("Test %d: %s... ", g_tests, name); } while(0)
#define PASS() do { g_passed++; printf("PASS\n"); return 0; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)

static int test_probe(void) {
    TEST("Probe function");
    
    /* Create valid IPF */
    uft_ipf_writer_t w;
    if (uft_ipf_writer_open(&w, TEST_FILE) != UFT_IPF_OK) FAIL("writer_open");
    if (uft_ipf_writer_write_header(&w) != UFT_IPF_OK) FAIL("write_header");
    uft_ipf_writer_close(&w);
    
    if (!uft_ipf_probe(TEST_FILE)) FAIL("probe should return true");
    
    /* Create invalid file */
    FILE *f = fopen(TEST_FILE, "wb");
    fprintf(f, "NOT AN IPF FILE");
    fclose(f);
    
    if (uft_ipf_probe(TEST_FILE)) FAIL("probe should return false");
    
    PASS();
}

static int test_basic_write_read(void) {
    TEST("Basic write/read");
    
    /* Write */
    uft_ipf_writer_t w;
    if (uft_ipf_writer_open(&w, TEST_FILE) != UFT_IPF_OK) FAIL("writer_open");
    
    /* Add INFO */
    uft_ipf_info_t info = {0};
    info.min_track = 0;
    info.max_track = 79;
    info.min_side = 0;
    info.max_side = 1;
    info.platforms = UFT_IPF_PLATFORM_AMIGA_OCS;
    info.media_type = UFT_IPF_MEDIA_FLOPPY_DD;
    
    if (uft_ipf_writer_add_info(&w, &info) != UFT_IPF_OK) FAIL("add_info");
    
    /* Add custom record */
    const char data[] = "Test data";
    if (uft_ipf_writer_add_record(&w, UFT_IPF_REC_DATA, data, sizeof(data)) != UFT_IPF_OK) 
        FAIL("add_record");
    
    uft_ipf_writer_close(&w);
    
    /* Read */
    uft_ipf_t ipf;
    if (uft_ipf_open(&ipf, TEST_FILE) != UFT_IPF_OK) FAIL("open");
    
    if (!ipf.is_valid_ipf) FAIL("not valid IPF");
    if (ipf.record_count != 3) FAIL("expected 3 records"); /* CAPS + INFO + DATA */
    
    const uft_ipf_info_t *ri = uft_ipf_get_info(&ipf);
    if (!ri) FAIL("no INFO record");
    if (ri->max_track != 79) FAIL("max_track mismatch");
    if (!(ri->platforms & UFT_IPF_PLATFORM_AMIGA_OCS)) FAIL("platform mismatch");
    
    uft_ipf_close(&ipf);
    PASS();
}

static int test_crc_validation(void) {
    TEST("CRC validation");
    
    /* Create file */
    uft_ipf_writer_t w;
    uft_ipf_writer_open(&w, TEST_FILE);
    uft_ipf_writer_write_header(&w);
    uft_ipf_writer_add_record(&w, UFT_IPF_REC_DATA, "hello", 5);
    uft_ipf_writer_close(&w);
    
    /* Validate */
    uft_ipf_t ipf;
    uft_ipf_open(&ipf, TEST_FILE);
    
    uft_ipf_err_t e = uft_ipf_validate(&ipf, true);
    if (e != UFT_IPF_OK) FAIL("validation failed");
    
    /* Check CRC on DATA record */
    if (!uft_ipf_verify_record_crc(&ipf, 1)) FAIL("CRC verify failed");
    
    uft_ipf_close(&ipf);
    PASS();
}

static int test_record_types(void) {
    TEST("Record type functions");
    
    if (!uft_ipf_record_type_known(UFT_IPF_REC_CAPS)) FAIL("CAPS unknown");
    if (!uft_ipf_record_type_known(UFT_IPF_REC_INFO)) FAIL("INFO unknown");
    if (!uft_ipf_record_type_known(UFT_IPF_REC_DATA)) FAIL("DATA unknown");
    if (uft_ipf_record_type_known(0x12345678)) FAIL("random should be unknown");
    
    if (strcmp(uft_ipf_record_type_name(UFT_IPF_REC_CAPS), "CAPS") != 0) FAIL("CAPS name");
    if (strcmp(uft_ipf_record_type_name(UFT_IPF_REC_INFO), "INFO") != 0) FAIL("INFO name");
    
    uint32_t t = uft_ipf_string_to_type("DATA");
    if (t != UFT_IPF_REC_DATA) FAIL("string_to_type");
    
    const char *s = uft_ipf_type_to_string(UFT_IPF_REC_TRCK);
    if (strcmp(s, "TRCK") != 0) FAIL("type_to_string");
    
    PASS();
}

static int test_find_record(void) {
    TEST("Find record");
    
    uft_ipf_writer_t w;
    uft_ipf_writer_open(&w, TEST_FILE);
    uft_ipf_writer_write_header(&w);
    uft_ipf_writer_add_record(&w, UFT_IPF_REC_DATA, "1", 1);
    uft_ipf_writer_add_record(&w, UFT_IPF_REC_DATA, "2", 1);
    uft_ipf_writer_add_record(&w, UFT_IPF_REC_DATA, "3", 1);
    uft_ipf_writer_close(&w);
    
    uft_ipf_t ipf;
    uft_ipf_open(&ipf, TEST_FILE);
    
    size_t idx = uft_ipf_find_record(&ipf, UFT_IPF_REC_DATA);
    if (idx != 1) FAIL("first DATA not at 1");
    
    idx = uft_ipf_find_next_record(&ipf, UFT_IPF_REC_DATA, 1);
    if (idx != 2) FAIL("second DATA not at 2");
    
    idx = uft_ipf_find_next_record(&ipf, UFT_IPF_REC_DATA, 3);
    if (idx != SIZE_MAX) FAIL("should not find after 3");
    
    idx = uft_ipf_find_record(&ipf, UFT_IPF_REC_TRCK);
    if (idx != SIZE_MAX) FAIL("should not find TRCK");
    
    uft_ipf_close(&ipf);
    PASS();
}

static int test_dump(void) {
    TEST("Dump function");
    
    uft_ipf_writer_t w;
    uft_ipf_writer_open(&w, TEST_FILE);
    
    uft_ipf_info_t info = {0};
    info.min_track = 0;
    info.max_track = 79;
    info.platforms = UFT_IPF_PLATFORM_AMIGA_OCS | UFT_IPF_PLATFORM_ATARI_ST;
    uft_ipf_writer_add_info(&w, &info);
    uft_ipf_writer_add_record(&w, UFT_IPF_REC_DATA, "test", 4);
    uft_ipf_writer_close(&w);
    
    uft_ipf_t ipf;
    uft_ipf_open(&ipf, TEST_FILE);
    
    printf("\n");
    uft_ipf_dump(&ipf, stdout, true);
    
    uft_ipf_close(&ipf);
    PASS();
}

static int test_error_handling(void) {
    TEST("Error handling");
    
    uft_ipf_t ipf;
    
    if (uft_ipf_open(&ipf, "/nonexistent") == UFT_IPF_OK) FAIL("should fail");
    if (uft_ipf_open(NULL, TEST_FILE) == UFT_IPF_OK) FAIL("NULL ctx");
    if (uft_ipf_open(&ipf, NULL) == UFT_IPF_OK) FAIL("NULL path");
    
    /* Create non-IPF file (must be at least 12 bytes for EMAGIC) */
    FILE *f = fopen(TEST_FILE, "wb");
    fprintf(f, "NOT AN IPF FILE!");  /* 16 bytes */
    fclose(f);
    
    uft_ipf_err_t e = uft_ipf_open(&ipf, TEST_FILE);
    if (e != UFT_IPF_EMAGIC) {
        printf("got error %d (%s) ", e, uft_ipf_strerror(e));
        FAIL("should be EMAGIC");
    }
    
    PASS();
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("IPF Container Tests v2.0\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    test_probe();
    test_basic_write_read();
    test_crc_validation();
    test_record_types();
    test_find_record();
    test_error_handling();
    test_dump();
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("Results: %d/%d passed\n", g_passed, g_tests);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    remove(TEST_FILE);
    
    return (g_passed == g_tests) ? 0 : 1;
}

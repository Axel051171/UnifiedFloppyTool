/**
 * @file test_ipf_roundtrip.c
 * @brief IPF Container Roundtrip Test
 * 
 * Tests: write -> read -> validate -> compare
 */

#include "uft/formats/ipf/uft_ipf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TEST_FILE "/tmp/ipf_test_roundtrip.bin"

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int test_basic_roundtrip(void) {
    printf("Test 1: Basic roundtrip (8-byte header)...\n");
    
    /* Write */
    uft_ipf_writer_t w;
    if (uft_ipf_writer_open(&w, TEST_FILE, UFT_IPF_MAGIC_CAPS, true, 8) != UFT_IPF_OK) {
        return fail("writer_open");
    }
    
    const char msg[] = "Hello IPF!";
    if (uft_ipf_writer_add_chunk(&w, UFT_IPF_CHUNK_INFO, msg, (uint32_t)strlen(msg), false) != UFT_IPF_OK) {
        return fail("writer_add INFO");
    }
    
    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    if (uft_ipf_writer_add_chunk(&w, UFT_IPF_CHUNK_DATA, data, sizeof(data), false) != UFT_IPF_OK) {
        return fail("writer_add DATA");
    }
    
    uft_ipf_writer_close(&w);
    
    /* Read */
    uft_ipf_t ipf;
    if (uft_ipf_open(&ipf, TEST_FILE) != UFT_IPF_OK) return fail("open");
    if (uft_ipf_parse(&ipf) != UFT_IPF_OK) return fail("parse");
    if (uft_ipf_validate(&ipf, false) != UFT_IPF_OK) return fail("validate");
    
    if (uft_ipf_chunk_count(&ipf) != 2) return fail("chunk_count != 2");
    
    /* Verify INFO chunk */
    const uft_ipf_chunk_t *c0 = uft_ipf_chunk_at(&ipf, 0);
    if (!c0 || !uft_fourcc_eq(c0->id, UFT_IPF_CHUNK_INFO)) return fail("chunk 0 not INFO");
    
    char buf[64] = {0};
    size_t got;
    if (uft_ipf_read_chunk_data(&ipf, 0, buf, sizeof(buf), &got) != UFT_IPF_OK) return fail("read INFO");
    if (got != strlen(msg) || memcmp(buf, msg, got) != 0) return fail("INFO content mismatch");
    
    /* Verify DATA chunk */
    const uft_ipf_chunk_t *c1 = uft_ipf_chunk_at(&ipf, 1);
    if (!c1 || !uft_fourcc_eq(c1->id, UFT_IPF_CHUNK_DATA)) return fail("chunk 1 not DATA");
    
    uint8_t buf2[16] = {0};
    if (uft_ipf_read_chunk_data(&ipf, 1, buf2, sizeof(buf2), &got) != UFT_IPF_OK) return fail("read DATA");
    if (got != sizeof(data) || memcmp(buf2, data, got) != 0) return fail("DATA content mismatch");
    
    uft_ipf_close(&ipf);
    printf("  PASS\n");
    return 0;
}

static int test_crc32_function(void) {
    printf("Test 2: CRC32 calculation...\n");
    
    /* Known CRC32 values */
    const char test1[] = "123456789";
    uint32_t crc1 = uft_ipf_crc32(test1, 9);
    /* CRC32 of "123456789" should be 0xCBF43926 */
    if (crc1 != 0xCBF43926) {
        printf("  CRC32('123456789') = %08X, expected CBF43926\n", crc1);
        return fail("CRC32 mismatch");
    }
    
    printf("  PASS\n");
    return 0;
}

static int test_find_chunk(void) {
    printf("Test 3: Find chunk by FourCC...\n");
    
    /* Write multiple chunks */
    uft_ipf_writer_t w;
    if (uft_ipf_writer_open(&w, TEST_FILE, UFT_IPF_MAGIC_CAPS, false, 8) != UFT_IPF_OK) {
        return fail("writer_open");
    }
    
    uft_ipf_writer_add_chunk(&w, UFT_IPF_CHUNK_INFO, "info", 4, false);
    uft_ipf_writer_add_chunk(&w, UFT_IPF_CHUNK_TRCK, "track", 5, false);
    uft_ipf_writer_add_chunk(&w, UFT_IPF_CHUNK_DATA, "data", 4, false);
    uft_ipf_writer_close(&w);
    
    /* Find chunks */
    uft_ipf_t ipf;
    uft_ipf_open(&ipf, TEST_FILE);
    uft_ipf_parse(&ipf);
    
    size_t idx = uft_ipf_find_chunk(&ipf, UFT_IPF_CHUNK_TRCK);
    if (idx != 1) return fail("TRCK not at index 1");
    
    idx = uft_ipf_find_chunk(&ipf, UFT_IPF_CHUNK_DATA);
    if (idx != 2) return fail("DATA not at index 2");
    
    idx = uft_ipf_find_chunk(&ipf, uft_fourcc_make('N','O','N','E'));
    if (idx != SIZE_MAX) return fail("Found non-existent chunk");
    
    uft_ipf_close(&ipf);
    printf("  PASS\n");
    return 0;
}

static int test_error_handling(void) {
    printf("Test 4: Error handling...\n");
    
    uft_ipf_t ipf;
    
    /* Non-existent file */
    if (uft_ipf_open(&ipf, "/nonexistent/path.ipf") == UFT_IPF_OK) {
        uft_ipf_close(&ipf);
        return fail("Should fail on non-existent file");
    }
    
    /* NULL arguments */
    if (uft_ipf_open(NULL, TEST_FILE) == UFT_IPF_OK) return fail("Should fail on NULL ctx");
    if (uft_ipf_open(&ipf, NULL) == UFT_IPF_OK) return fail("Should fail on NULL path");
    
    printf("  PASS\n");
    return 0;
}

static int test_dump_info(void) {
    printf("Test 5: Dump info...\n");
    
    /* Write test file */
    uft_ipf_writer_t w;
    uft_ipf_writer_open(&w, TEST_FILE, UFT_IPF_MAGIC_CAPS, true, 8);
    uft_ipf_writer_add_chunk(&w, UFT_IPF_CHUNK_INFO, "metadata", 8, false);
    uft_ipf_writer_add_chunk(&w, UFT_IPF_CHUNK_DATA, "content", 7, false);
    uft_ipf_writer_close(&w);
    
    /* Parse and dump */
    uft_ipf_t ipf;
    uft_ipf_open(&ipf, TEST_FILE);
    uft_ipf_parse(&ipf);
    
    printf("  --- dump ---\n");
    uft_ipf_dump_info(&ipf, stdout);
    printf("  --- end ---\n");
    
    uft_ipf_close(&ipf);
    printf("  PASS\n");
    return 0;
}

int main(void) {
    printf("=== IPF Container Tests ===\n\n");
    
    int failures = 0;
    failures += test_basic_roundtrip();
    failures += test_crc32_function();
    failures += test_find_chunk();
    failures += test_error_handling();
    failures += test_dump_info();
    
    printf("\n=== Results: %d failures ===\n", failures);
    
    /* Cleanup */
    remove(TEST_FILE);
    
    return failures > 0 ? 1 : 0;
}

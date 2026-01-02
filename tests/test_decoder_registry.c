/**
 * @file test_decoder_registry.c
 * @brief Unit Tests für Decoder Registry
 */

#include "uft/uft_decoder_registry.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define TEST(name) printf("TEST: %s... ", #name)
#define PASS() printf("PASS\n")
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)

// Mock decoder for testing
static int mock_probe(const uft_flux_track_data_t* flux, int* conf) {
    *conf = 75;
    return 1;
}

static uft_error_t mock_decode(const uft_flux_track_data_t* flux,
                                uft_track_t* sectors,
                                const uft_decode_options_t* opts) {
    return UFT_OK;
}

static const uft_decoder_ops_t mock_decoder = {
    .name = "MOCK",
    .description = "Mock decoder for testing",
    .version = 0x00010000,
    .encoding = UFT_ENCODING_MFM,
    .probe = mock_probe,
    .decode_track = mock_decode,
};

static int test_register_decoder(void) {
    TEST(register_decoder);
    
    uft_error_t err = uft_decoder_register(&mock_decoder);
    if (err != UFT_OK) FAIL("register failed");
    
    PASS();
    return 0;
}

static int test_find_decoder(void) {
    TEST(find_decoder);
    
    const uft_decoder_ops_t* dec = uft_decoder_find_by_name("MOCK");
    if (!dec) FAIL("find_by_name returned NULL");
    if (strcmp(dec->name, "MOCK") != 0) FAIL("wrong decoder returned");
    
    PASS();
    return 0;
}

static int test_find_by_encoding(void) {
    TEST(find_by_encoding);
    
    const uft_decoder_ops_t* dec = uft_decoder_find_by_encoding(UFT_ENCODING_MFM);
    if (!dec) FAIL("find_by_encoding returned NULL");
    
    PASS();
    return 0;
}

static int test_decoder_count(void) {
    TEST(decoder_count);
    
    size_t count = uft_decoder_count();
    if (count == 0) FAIL("count should be > 0");
    
    printf("(%zu decoders) ", count);
    PASS();
    return 0;
}

int main(void) {
    printf("=== Decoder Registry Tests ===\n\n");
    
    // Register built-in decoders first
    extern void uft_register_builtin_decoders(void);
    uft_register_builtin_decoders();
    
    int failures = 0;
    failures += test_register_decoder();
    failures += test_find_decoder();
    failures += test_find_by_encoding();
    failures += test_decoder_count();
    
    printf("\n%s: %d failures\n", failures ? "FAILED" : "PASSED", failures);
    return failures;
}

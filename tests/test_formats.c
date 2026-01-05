/**
 * @file test_formats.c
 * @brief Tests für Format-System
 */

#include "uft/uft_formats_extended.h"
#include <stdio.h>
#include <string.h>

#define TEST(name) printf("TEST: %s... ", #name)
#define PASS() printf("PASS\n")
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)

static int test_format_registry_init(void) {
    TEST(format_registry_init);
    
    uft_error_t err = uft_format_registry_init();
    if (err != UFT_OK) FAIL("init failed");
    
    PASS();
    return 0;
}

static int test_format_handler_lookup(void) {
    TEST(format_handler_lookup);
    
    const uft_format_handler_t* h = uft_format_get_handler(UFT_FORMAT_SCP);
    if (!h) FAIL("SCP handler not found");
    if (strcmp(h->name, "SCP") != 0) FAIL("wrong handler");
    if (!h->supports_flux) FAIL("SCP should support flux");
    
    PASS();
    return 0;
}

static int test_format_detect_scp(void) {
    TEST(format_detect_scp);
    
    uint8_t scp_data[] = { 'S', 'C', 'P', 0, 0, 0, 0, 0 };
    
    const uft_format_handler_t* h = uft_format_detect(scp_data, sizeof(scp_data));
    if (!h) FAIL("detection failed");
    if (h->format != UFT_FORMAT_SCP) FAIL("wrong format detected");
    
    PASS();
    return 0;
}

static int test_format_detect_ipf(void) {
    TEST(format_detect_ipf);
    
    uint8_t ipf_data[] = { 'C', 'A', 'P', 'S', 0, 0, 0, 0 };
    
    const uft_format_handler_t* h = uft_format_detect(ipf_data, sizeof(ipf_data));
    if (!h) FAIL("detection failed");
    if (h->format != UFT_FORMAT_IPF) FAIL("wrong format detected");
    
    PASS();
    return 0;
}

static int test_format_detect_by_extension(void) {
    TEST(format_detect_by_extension);
    
    const uft_format_handler_t* h;
    
    h = uft_format_detect_by_extension("disk.scp");
    if (!h || h->format != UFT_FORMAT_SCP) FAIL(".scp detection failed");
    
    h = uft_format_detect_by_extension("game.adf");
    if (!h || h->format != UFT_FORMAT_ADF) FAIL(".adf detection failed");
    
    h = uft_format_detect_by_extension("archive.d64");
    if (!h || h->format != UFT_FORMAT_D64) FAIL(".d64 detection failed");
    
    PASS();
    return 0;
}

static int test_format_list_flux(void) {
    TEST(format_list_flux);
    
    const uft_format_handler_t* handlers[32];
    int count = uft_format_list_by_capability(true, false, handlers, 32);
    
    if (count == 0) FAIL("no flux formats found");
    
    for (int i = 0; i < count; i++) {
        if (!handlers[i]->supports_flux) {
            FAIL("non-flux format in list");
        }
    }
    
    printf("(%d flux formats) ", count);
    PASS();
    return 0;
}

static int test_format_conversion_check(void) {
    TEST(format_conversion_check);
    
    const char* warning = NULL;
    
    // SCP → D64 should warn about loss
    bool can = uft_format_can_convert(UFT_FORMAT_SCP, UFT_FORMAT_D64, &warning);
    if (!can) FAIL("SCP→D64 should be possible");
    if (!warning) FAIL("should have warning about flux loss");
    
    PASS();
    return 0;
}

int main(void) {
    printf("=== Format System Tests ===\n\n");
    
    int failures = 0;
    failures += test_format_registry_init();
    failures += test_format_handler_lookup();
    failures += test_format_detect_scp();
    failures += test_format_detect_ipf();
    failures += test_format_detect_by_extension();
    failures += test_format_list_flux();
    failures += test_format_conversion_check();
    
    uft_format_registry_shutdown();
    
    printf("\n%s: %d failures\n", failures ? "FAILED" : "PASSED", failures);
    return failures;
}

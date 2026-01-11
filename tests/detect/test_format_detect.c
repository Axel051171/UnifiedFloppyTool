/**
 * @file test_format_detect.c
 * @brief Tests for Format Auto-Detection Engine
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "uft/detect/uft_format_detect.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        tests_failed++; \
    } else { \
        printf("  OK: %s\n", msg); \
        tests_passed++; \
    } \
} while(0)

static void create_hfe_header(uint8_t *data) {
    memset(data, 0, 512);
    memcpy(data, "HXCPICFE", 8);
}

static void create_woz_header(uint8_t *data) {
    memset(data, 0, 512);
    memcpy(data, "WOZ2", 4);
    memcpy(data + 8, "INFO", 4);
}

static void create_scp_header(uint8_t *data) {
    memset(data, 0, 512);
    memcpy(data, "SCP", 3);
}

static void test_hfe_detection(void) {
    printf("\n=== Test: HFE Detection ===\n");
    uint8_t data[512];
    create_hfe_header(data);
    
    uft_detect_result_t result;
    int rc = uft_detect_format(data, sizeof(data), &result);
    
    TEST(rc == 0, "Detection succeeded");
    TEST(result.format == UFT_FORMAT_HFE, "Format is HFE");
    TEST(result.confidence >= 90, "High confidence");
    printf("  Confidence: %d%%\n", result.confidence);
}

static void test_woz_detection(void) {
    printf("\n=== Test: WOZ Detection ===\n");
    uint8_t data[512];
    create_woz_header(data);
    
    uft_detect_result_t result;
    int rc = uft_detect_format(data, sizeof(data), &result);
    
    TEST(rc == 0, "Detection succeeded");
    TEST(result.format == UFT_FORMAT_WOZ, "Format is WOZ");
    TEST(result.confidence >= 95, "Very high confidence");
    printf("  Confidence: %d%%\n", result.confidence);
}

static void test_scp_detection(void) {
    printf("\n=== Test: SCP Detection ===\n");
    uint8_t data[512];
    create_scp_header(data);
    
    uft_detect_result_t result;
    int rc = uft_detect_format(data, sizeof(data), &result);
    
    TEST(rc == 0, "Detection succeeded");
    TEST(result.format == UFT_FORMAT_SCP, "Format is SCP");
    TEST(result.confidence >= 95, "Very high confidence");
    printf("  Confidence: %d%%\n", result.confidence);
}

static void test_registry_api(void) {
    printf("\n=== Test: Registry API ===\n");
    
    size_t count;
    const uft_format_detector_t *detectors = uft_get_detectors(&count);
    
    TEST(detectors != NULL, "Detectors returned");
    TEST(count > 10, "Multiple detectors registered");
    printf("  Registered: %zu detectors\n", count);
    
    const char *name = uft_format_name(UFT_FORMAT_D64);
    TEST(strcmp(name, "D64") == 0, "D64 name lookup");
    
    TEST(uft_format_is_flux(UFT_FORMAT_SCP) == true, "SCP is flux");
    TEST(uft_format_is_flux(UFT_FORMAT_D64) == false, "D64 not flux");
}

static void test_unknown_format(void) {
    printf("\n=== Test: Unknown Format ===\n");
    uint8_t data[512];
    memset(data, 0x42, sizeof(data));
    
    uft_detect_result_t result;
    uft_detect_format(data, sizeof(data), &result);
    
    TEST(result.format == UFT_FORMAT_UNKNOWN || result.confidence < 50,
         "Low confidence for garbage");
    printf("  Format: %s, Confidence: %d%%\n", 
           result.format_name, result.confidence);
}

int main(void) {
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║     UFT Format Auto-Detection Engine Tests            ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n");
    
    test_hfe_detection();
    test_woz_detection();
    test_scp_detection();
    test_registry_api();
    test_unknown_format();
    
    printf("\n════════════════════════════════════════════════════════\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}

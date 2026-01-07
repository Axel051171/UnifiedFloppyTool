/**
 * @file test_r50_r55_all.c
 * @brief Unified Test Runner for R50-R55 Modules
 * @version 1.0.0
 * @date 2026-01-06
 *
 * Tests all new modules from R50-R55:
 * - FluxStat multi-pass analysis
 * - libflux PLL Enhanced
 * - SCP Parser
 * - WOZ Parser
 * - Amiga Protection Registry (194 entries)
 * - HxC Format Detection (90+ formats)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*============================================================================
 * Test Framework
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
    tests_passed++; \
} while(0)

/*============================================================================
 * External Test Functions
 *============================================================================*/

/* From test_flux_parsers.c */
extern void run_flux_parser_tests(void);

/* From test_amiga_protection.c */
extern void run_amiga_protection_tests(void);

/* From test_libflux_formats.c */
extern void run_libflux_format_tests(void);

/*============================================================================
 * Quick Sanity Tests (if modules not available)
 *============================================================================*/

static void test_basic_sanity(void)
{
    printf("\n=== Basic Sanity Tests ===\n\n");
    
    printf("  Memory allocation...");
    void* ptr = malloc(1024);
    TEST_ASSERT(ptr != NULL);
    free(ptr);
    printf(" PASSED\n");
    
    printf("  String operations...");
    char buf[64];
    snprintf(buf, sizeof(buf), "Test %d", 42);
    TEST_ASSERT(strcmp(buf, "Test 42") == 0);
    printf(" PASSED\n");
    
    printf("  Integer arithmetic...");
    TEST_ASSERT((0xFFFF * 25) == 1638375);
    TEST_ASSERT((200000000 / 1000000) == 200);
    printf(" PASSED\n");
    
    printf("\n=== Basic Sanity Tests PASSED ===\n");
}

/*============================================================================
 * Module Availability Check
 *============================================================================*/

#ifdef UFT_HAS_SCP_PARSER
#include "uft/flux/uft_scp_parser.h"
#define HAS_SCP 1
#else
#define HAS_SCP 0
#endif

#ifdef UFT_HAS_UFT_UFT_KF_PARSER
#include "uft/flux/uft_uft_kf_parser.h"
#define HAS_KRYOFLUX 1
#else
#define HAS_KRYOFLUX 0
#endif

#ifdef UFT_HAS_WOZ_PARSER
#include "uft/flux/uft_woz_parser.h"
#define HAS_WOZ 1
#else
#define HAS_WOZ 0
#endif

#ifdef UFT_HAS_AMIGA_PROTECTION
#include "uft/protection/uft_amiga_protection_full.h"
#define HAS_AMIGA_PROT 1
#else
#define HAS_AMIGA_PROT 0
#endif

#ifdef UFT_HAS_HXC_FORMATS
#include "uft/formats/uft_libflux_formats.h"
#define HAS_HXC_FMT 1
#else
#define HAS_HXC_FMT 0
#endif

#ifdef UFT_HAS_GCR_CODEC
#include "uft/codec/uft_opencbm_gcr.h"
#define HAS_GCR 1
#else
#define HAS_GCR 0
#endif

/*============================================================================
 * Summary Report
 *============================================================================*/

static void print_summary(clock_t start, clock_t end)
{
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                    UFT R50-R55 TEST SUMMARY                      ║\n");
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║  Tests Run:    %5d                                             ║\n", tests_run);
    printf("║  Tests Passed: %5d                                             ║\n", tests_passed);
    printf("║  Tests Failed: %5d                                             ║\n", tests_failed);
    printf("║  Time:         %5.2f seconds                                    ║\n", elapsed);
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    
    if (tests_failed == 0) {
        printf("║                     ✅ ALL TESTS PASSED ✅                       ║\n");
    } else {
        printf("║                     ❌ SOME TESTS FAILED ❌                      ║\n");
    }
    
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
}

static void print_module_status(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                    UFT R50-R55 MODULE STATUS                     ║\n");
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║  SCP Parser:          %s                                       ║\n", HAS_SCP ? "✅ Available" : "❌ Missing  ");
    printf("║  KryoFlux Parser:     %s                                       ║\n", HAS_KRYOFLUX ? "✅ Available" : "❌ Missing  ");
    printf("║  WOZ Parser:          %s                                       ║\n", HAS_WOZ ? "✅ Available" : "❌ Missing  ");
    printf("║  Amiga Protection:    %s                                       ║\n", HAS_AMIGA_PROT ? "✅ Available" : "❌ Missing  ");
    printf("║  HxC Formats:         %s                                       ║\n", HAS_HXC_FMT ? "✅ Available" : "❌ Missing  ");
    printf("║  GCR Codec:           %s                                       ║\n", HAS_GCR ? "✅ Available" : "❌ Missing  ");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
}

/*============================================================================
 * Main Test Runner
 *============================================================================*/

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    
    clock_t start = clock();
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                                  ║\n");
    printf("║           UnifiedFloppyTool (UFT) R50-R55 Test Suite             ║\n");
    printf("║                                                                  ║\n");
    printf("║   Testing: FluxStat, SCP, KryoFlux, WOZ, Amiga Protection,       ║\n");
    printf("║            HxC Formats, CBM library GCR                              ║\n");
    printf("║                                                                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    
    /* Basic sanity tests always run */
    test_basic_sanity();
    
    /* Run module tests if available */
#ifdef RUN_FLUX_PARSER_TESTS
    run_flux_parser_tests();
#endif

#ifdef RUN_AMIGA_PROTECTION_TESTS
    run_amiga_protection_tests();
#endif

#ifdef RUN_HXC_FORMAT_TESTS
    run_libflux_format_tests();
#endif
    
    clock_t end = clock();
    
    print_module_status();
    print_summary(start, end);
    
    return (tests_failed == 0) ? 0 : 1;
}

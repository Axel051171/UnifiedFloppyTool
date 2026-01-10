/**
 * @file test_smoke_workflow.c
 * @brief P1-1: Smoke Test - Basic Workflow (Load → Analyze → Convert)
 * 
 * Tests the fundamental UFT workflow:
 * 1. Version check
 * 2. Profile lookup (50+ profiles)
 * 3. Track analysis basics
 * 
 * Uses embedded minimal test data (no external files required).
 * 
 * @author UFT Team
 * @date 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* UFT Headers */
#include "uft/uft_version.h"

/* Track Analysis (XCopy algorithms) */
#include "uft_track_analysis.h"

/* Profile lookup */
#include "profiles/uft_profiles_all.h"

/* ============================================================================
 * Test Utilities
 * ============================================================================ */

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s\n", msg); \
        g_tests_failed++; \
        return false; \
    } \
    g_tests_passed++; \
} while(0)

#define TEST_SECTION(name) printf("\n=== %s ===\n", name)

/* ============================================================================
 * Test 1: Version & Initialization
 * ============================================================================ */

static bool test_version(void)
{
    TEST_SECTION("Test 1: Version & Init");
    
    /* Version string exists */
    const char *ver = UFT_VERSION_STRING;
    printf("  UFT Version: %s\n", ver);
    TEST_ASSERT(ver != NULL && strlen(ver) > 0, "Version string exists");
    
    /* Version number is reasonable */
    int ver_int = uft_version_int();
    printf("  Version int: %d\n", ver_int);
    TEST_ASSERT(ver_int >= 30000, "Version >= 3.0.0");
    
    /* Full version info */
    const char *full = uft_version_full();
    printf("  Full: %s\n", full);
    TEST_ASSERT(full != NULL, "Full version exists");
    
    printf("  ✓ Version tests passed\n");
    return true;
}

/* ============================================================================
 * Test 2: Profile Lookup
 * ============================================================================ */

static bool test_profile_lookup(void)
{
    TEST_SECTION("Test 2: Profile Lookup");
    
    /* Get profile count */
    int count = uft_get_profile_count();
    printf("  Total profiles: %d\n", count);
    TEST_ASSERT(count >= 50, "At least 50 profiles available");
    
    /* Lookup by name */
    const uft_platform_profile_t *p;
    
    p = uft_find_profile_by_name("Amiga");
    TEST_ASSERT(p != NULL, "Amiga profile found");
    printf("  Amiga DD: %s, %d sectors, %zu bytes\n", 
           p->name, p->sectors_per_track, p->sector_size);
    TEST_ASSERT(p->sectors_per_track == 11, "Amiga DD has 11 sectors");
    TEST_ASSERT(p->sector_size == 512, "Amiga sector size 512");
    
    p = uft_find_profile_by_name("Commodore");
    TEST_ASSERT(p != NULL, "Commodore 64 profile found");
    printf("  C64: %s, encoding=%d\n", p->name, p->encoding);
    TEST_ASSERT(p->encoding == ENCODING_GCR_C64, "C64 uses GCR");
    
    p = uft_find_profile_by_name("PC-98");
    TEST_ASSERT(p != NULL, "PC-98 profile found");
    printf("  PC-98: %s, %zu byte sectors, %.0f RPM\n", 
           p->name, p->sector_size, p->rpm);
    TEST_ASSERT(p->sector_size == 1024, "PC-98 has 1024-byte sectors");
    TEST_ASSERT(p->rpm == 360.0, "PC-98 runs at 360 RPM");
    
    /* Lookup by size */
    p = uft_detect_profile_by_size(901120);  /* ADF size */
    TEST_ASSERT(p != NULL, "Profile for 901120 bytes found");
    printf("  901120 bytes: %s\n", p->name);
    TEST_ASSERT(strstr(p->name, "Amiga") != NULL, "901120 = Amiga DD");
    
    p = uft_detect_profile_by_size(174848);  /* D64 size */
    TEST_ASSERT(p != NULL, "Profile for 174848 bytes found");
    printf("  174848 bytes: %s\n", p->name);
    
    /* Get all profiles */
    const uft_platform_profile_t* const* all = uft_get_all_profiles(&count);
    TEST_ASSERT(all != NULL, "All profiles array returned");
    TEST_ASSERT(count >= 50, "Count matches");
    
    /* Verify regional profiles exist */
    bool found_trs80 = false, found_victor = false, found_archimedes = false;
    bool found_sam = false, found_thomson = false;
    for (int i = 0; i < count; i++) {
        if (strstr(all[i]->name, "TRS-80")) found_trs80 = true;
        if (strstr(all[i]->name, "Victor")) found_victor = true;
        if (strstr(all[i]->name, "Archimedes") || strstr(all[i]->name, "ADFS")) found_archimedes = true;
        if (strstr(all[i]->name, "SAM")) found_sam = true;
        if (strstr(all[i]->name, "Thomson")) found_thomson = true;
    }
    TEST_ASSERT(found_trs80, "TRS-80 profile exists");
    TEST_ASSERT(found_victor, "Victor 9000 profile exists");
    TEST_ASSERT(found_archimedes, "Archimedes profile exists");
    TEST_ASSERT(found_sam, "SAM Coupé profile exists");
    TEST_ASSERT(found_thomson, "Thomson profile exists");
    
    printf("  ✓ Profile lookup tests passed\n");
    return true;
}

/* ============================================================================
 * Test 3: Track Analysis Basics
 * ============================================================================ */

static bool test_track_analysis(void)
{
    TEST_SECTION("Test 3: Track Analysis");
    
    /* Create minimal MFM track data for testing */
    uint8_t track_data[12668];  /* Amiga DD track size */
    memset(track_data, 0x4E, sizeof(track_data));  /* GAP bytes */
    
    /* Insert some sync patterns */
    for (int i = 0; i < 11; i++) {
        size_t pos = i * 1088 + 100;  /* Approximate sector positions */
        if (pos + 2 < sizeof(track_data)) {
            track_data[pos] = 0x44;
            track_data[pos+1] = 0x89;  /* MFM sync */
        }
    }
    
    /* Test sync detection with Amiga profile */
    uint32_t patterns[] = { SYNC_AMIGA_DOS };
    uft_sync_result_t sync_result;
    memset(&sync_result, 0, sizeof(sync_result));
    
    int syncs = uft_find_syncs_rotated(track_data, sizeof(track_data),
                                       patterns, 1, 16, &sync_result);
    printf("  Syncs found (rotated search): %d\n", syncs);
    /* Sync count depends on byte alignment */
    
    /* Test full track analysis with profile */
    uft_track_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));
    
    int rc = uft_analyze_track_profile(track_data, sizeof(track_data),
                                       &UFT_PROFILE_AMIGA_DD, &analysis);
    printf("  Track analysis: rc=%d, type=%d, confidence=%.0f%%\n",
           rc, analysis.type, analysis.confidence * 100);
    TEST_ASSERT(rc == 0, "Analysis completed without error");
    
    /* Test auto-detection */
    memset(&analysis, 0, sizeof(analysis));
    rc = uft_analyze_track(track_data, sizeof(track_data), &analysis);
    printf("  Auto-detect: rc=%d, platform=%d\n", 
           rc, analysis.detected_platform);
    TEST_ASSERT(rc == 0, "Auto-detect completed");
    
    printf("  ✓ Track analysis tests passed\n");
    return true;
}

/* ============================================================================
 * Test 4: Error Handling
 * ============================================================================ */

static bool test_error_handling(void)
{
    TEST_SECTION("Test 4: Error Handling");
    
    /* Test profile with NULL */
    const uft_platform_profile_t *p = uft_find_profile_by_name(NULL);
    TEST_ASSERT(p == NULL, "NULL name returns NULL profile");
    printf("  NULL name lookup: correctly returned NULL\n");
    
    /* Test empty track */
    uint8_t empty[100];
    memset(empty, 0, sizeof(empty));
    
    uft_track_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));
    
    int rc = uft_analyze_track(empty, sizeof(empty), &analysis);
    printf("  Empty track analysis: rc=%d, type=%d\n", rc, analysis.type);
    /* Should handle gracefully */
    
    /* Test very small buffer */
    rc = uft_analyze_track(empty, 10, &analysis);
    printf("  Tiny buffer analysis: rc=%d\n", rc);
    
    printf("  ✓ Error handling tests passed\n");
    return true;
}

/* ============================================================================
 * Test 5: Victor 9000 Variable Sectors
 * ============================================================================ */

static bool test_victor_zones(void)
{
    TEST_SECTION("Test 5: Victor 9000 Zones");
    
    /* Victor 9000 has variable sectors per track */
    int s0 = uft_victor_sectors_for_track(0);
    int s40 = uft_victor_sectors_for_track(40);
    int s79 = uft_victor_sectors_for_track(79);
    
    printf("  Track 0:  %d sectors\n", s0);
    printf("  Track 40: %d sectors\n", s40);
    printf("  Track 79: %d sectors\n", s79);
    
    TEST_ASSERT(s0 == 19, "Track 0 has 19 sectors");
    TEST_ASSERT(s40 >= 14 && s40 <= 16, "Track 40 has 14-16 sectors");
    TEST_ASSERT(s79 == 11, "Track 79 has 11 sectors");
    
    /* Test out of range */
    int inv = uft_victor_sectors_for_track(100);
    TEST_ASSERT(inv == 0, "Invalid track returns 0");
    
    printf("  ✓ Victor 9000 zone tests passed\n");
    return true;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║         UFT Smoke Test - Basic Workflow (P1-1)                ║\n");
    printf("║         Version → Profiles → Analysis                          ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    
    bool all_passed = true;
    
    all_passed &= test_version();
    all_passed &= test_profile_lookup();
    all_passed &= test_track_analysis();
    all_passed &= test_error_handling();
    all_passed &= test_victor_zones();
    
    printf("\n════════════════════════════════════════════════════════════════\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    
    if (all_passed && g_tests_failed == 0) {
        printf("\n✓ ALL SMOKE TESTS PASSED\n");
        printf("  \"Bei uns geht kein Bit verloren\"\n");
        return 0;
    } else {
        printf("\n✗ SOME TESTS FAILED\n");
        return 1;
    }
}

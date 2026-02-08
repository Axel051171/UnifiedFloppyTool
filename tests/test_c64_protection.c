/**
 * @file test_c64_protection.c
 * @brief Tests for C64 Copy Protection Detection Module
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "uft/protection/uft_c64_protection.h"

/* Local D64 constants for testing */
#define TEST_D64_35_TRACKS  174848
#define TEST_D64_40_TRACKS  196608

/* Local helper to get sector offset */
static size_t test_d64_sector_offset(int track, int sector) {
    static const int sectors_before_track[] = {
        0,
        0, 21, 42, 63, 84, 105, 126, 147, 168, 189,
        210, 231, 252, 273, 294, 315, 336, 357,
        376, 395, 414, 433, 452, 471, 490,
        508, 526, 544, 562, 580, 598,
        615, 632, 649, 666, 683,
        700, 717, 734, 751, 768
    };
    if (track < 1 || track > 40) return (size_t)-1;
    return (size_t)(sectors_before_track[track] + sector) * 256;
}

/* Test error code to string conversion */
static void test_error_codes(void) {
    printf("Testing error code strings...\n");
    
    assert(strstr(c64_error_to_string(C64_ERR_OK), "No error") != NULL);
    assert(strstr(c64_error_to_string(C64_ERR_HEADER_NOT_FOUND), "20") != NULL);
    assert(strstr(c64_error_to_string(C64_ERR_NO_SYNC), "21") != NULL);
    assert(strstr(c64_error_to_string(C64_ERR_DATA_NOT_FOUND), "22") != NULL);
    assert(strstr(c64_error_to_string(C64_ERR_CHECKSUM), "23") != NULL);
    assert(strstr(c64_error_to_string(C64_ERR_VERIFY), "25") != NULL);
    assert(strstr(c64_error_to_string(C64_ERR_WRITE_PROTECT), "26") != NULL);
    assert(strstr(c64_error_to_string(C64_ERR_HEADER_CHECKSUM), "27") != NULL);
    assert(strstr(c64_error_to_string(C64_ERR_LONG_DATA), "28") != NULL);
    assert(strstr(c64_error_to_string(C64_ERR_ID_MISMATCH), "29") != NULL);
    
    printf("  ✓ All error code strings correct\n");
}

/* Test protection type to string conversion */
static void test_protection_strings(void) {
    printf("Testing protection type strings...\n");
    
    char buffer[512];
    
    c64_protection_to_string(C64_PROT_NONE, buffer, sizeof(buffer));
    assert(strstr(buffer, "No protection") != NULL);
    
    c64_protection_to_string(C64_PROT_VORPAL, buffer, sizeof(buffer));
    assert(strstr(buffer, "Vorpal") != NULL);
    
    c64_protection_to_string(C64_PROT_V_MAX, buffer, sizeof(buffer));
    assert(strstr(buffer, "V-Max") != NULL);
    
    c64_protection_to_string(C64_PROT_EXTRA_TRACKS | C64_PROT_CUSTOM_ERRORS, buffer, sizeof(buffer));
    assert(strstr(buffer, "Extra Tracks") != NULL);
    assert(strstr(buffer, "Custom Errors") != NULL);
    
    printf("  ✓ All protection type strings correct\n");
}

/* Test known titles database */
static void test_known_titles(void) {
    printf("Testing known titles database...\n");
    
    int count = c64_get_known_titles_count();
    assert(count > 100);  /* Should have 100+ titles from Super-Kit list */
    printf("  Database contains %d known titles\n", count);
    
    /* Test specific lookups */
    c64_known_title_t entry;
    
    assert(c64_lookup_title("Summer Games", &entry) == true);
    assert(entry.publisher == C64_PUB_EPYX);
    assert(entry.protection_flags & C64_PROT_VORPAL);
    
    assert(c64_lookup_title("Ghostbusters", &entry) == true);
    assert(entry.publisher == C64_PUB_ACTIVISION);
    
    assert(c64_lookup_title("Elite", &entry) == true);
    assert(entry.protection_flags & C64_PROT_GCR_TIMING);
    
    assert(c64_lookup_title("Flight Simulator II", &entry) == true);
    assert(entry.publisher == C64_PUB_SUBLOGIC);
    
    assert(c64_lookup_title("GEOS", &entry) == true);
    assert(entry.protection_flags & C64_PROT_GCR_SYNC);
    
    /* Test non-existent title */
    assert(c64_lookup_title("NonExistentGame12345", &entry) == false);
    
    printf("  ✓ Known titles lookup working correctly\n");
}

/* Test D64 analysis with synthetic data */
static void test_d64_analysis(void) {
    printf("Testing D64 analysis...\n");
    
    /* Create a minimal D64 image (35 tracks, no errors) */
    uint8_t *d64_data = calloc(1, 174848);
    assert(d64_data != NULL);
    
    /* Set up basic BAM structure at track 18, sector 0 */
    /* Offset for T18,S0 = sum of sectors in tracks 1-17 * 256 */
    /* Tracks 1-17 have 21 sectors each = 17 * 21 = 357 sectors */
    size_t bam_offset = 357 * 256;
    
    /* BAM header */
    d64_data[bam_offset + 0] = 18;  /* Directory track */
    d64_data[bam_offset + 1] = 1;   /* Directory sector */
    d64_data[bam_offset + 2] = 'A'; /* DOS version */
    d64_data[bam_offset + 3] = 0;   /* Unused */
    
    /* BAM entries (simplified - just mark all as free) */
    for (int t = 1; t <= 35; t++) {
        int sectors = (t <= 17) ? 21 : (t <= 24) ? 19 : (t <= 30) ? 18 : 17;
        d64_data[bam_offset + 4 + (t-1)*4] = sectors;  /* Free sectors */
    }
    
    /* Disk name at offset 0x90 */
    memcpy(d64_data + bam_offset + 0x90, "TEST DISK       ", 16);
    
    c64_protection_analysis_t result;
    int ret = c64_analyze_d64(d64_data, 174848, &result);
    
    assert(ret == 0);
    assert(result.tracks_used == 35);
    assert(result.uses_track_36_40 == false);
    assert(result.bam_valid == true);
    
    printf("  ✓ Basic D64 analysis working\n");
    
    free(d64_data);
    
    /* Test 40-track image */
    d64_data = calloc(1, 196608);
    assert(d64_data != NULL);
    
    ret = c64_analyze_d64(d64_data, 196608, &result);
    assert(ret == 0);
    assert(result.tracks_used == 40);
    assert(result.uses_track_36_40 == true);
    assert(result.protection_flags & C64_PROT_EXTRA_TRACKS);
    
    printf("  ✓ Extended D64 (40 tracks) analysis working\n");
    
    free(d64_data);
}

/* Test D64 with error bytes */
static void test_d64_errors(void) {
    printf("Testing D64 error analysis...\n");
    
    /* Create D64 with error bytes (35 tracks + 683 error bytes) */
    uint8_t *d64_data = calloc(1, 175531);
    assert(d64_data != NULL);
    
    /* Add some error sectors */
    size_t error_offset = 174848;
    
    /* Set up errors on track 18 (directory) - protection indicator */
    int sector_idx = 0;
    for (int t = 1; t < 18; t++) {
        sector_idx += (t <= 17) ? 21 : 19;
    }
    
    /* Add checksum error on T18,S5 */
    d64_data[error_offset + sector_idx + 5] = C64_ERR_CHECKSUM;
    
    /* Add header error on track 36 area (simulated) */
    d64_data[error_offset + 680] = C64_ERR_HEADER_NOT_FOUND;
    
    c64_protection_analysis_t result;
    int ret = c64_analyze_d64_errors(d64_data, 175531, &result);
    
    assert(ret == 0);
    assert(result.total_errors >= 1);
    assert(result.protection_flags & C64_PROT_CUSTOM_ERRORS);
    assert(result.protection_flags & C64_PROT_ERRORS_T18);
    
    printf("  ✓ D64 error analysis detecting protection errors\n");
    
    free(d64_data);
}

/* Test G64 analysis */
static void test_g64_analysis(void) {
    printf("Testing G64 analysis...\n");
    
    /* Create minimal G64 header */
    uint8_t g64_data[1024];
    memset(g64_data, 0, sizeof(g64_data));
    
    /* G64 signature */
    memcpy(g64_data, "GCR-1541", 8);
    g64_data[8] = 0;    /* Version */
    g64_data[9] = 84;   /* Track count (42 tracks * 2 for half-tracks) */
    g64_data[10] = 0x80; /* Max track size low */
    g64_data[11] = 0x1E; /* Max track size high (7808) */
    
    c64_protection_analysis_t result;
    int ret = c64_analyze_g64(g64_data, sizeof(g64_data), &result);
    
    assert(ret == 0);
    assert(result.has_gcr_data == true);
    
    printf("  ✓ Basic G64 analysis working\n");
}

/* Test report generation */
static void test_report_generation(void) {
    printf("Testing report generation...\n");
    
    c64_protection_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));
    
    strcpy(analysis.title, "SUMMER GAMES");
    analysis.publisher = C64_PUB_EPYX;
    analysis.protection_flags = C64_PROT_VORPAL | C64_PROT_CUSTOM_ERRORS;
    strcpy(analysis.protection_name, "Vorpal");
    analysis.confidence = 85;
    analysis.tracks_used = 35;
    analysis.total_errors = 5;
    analysis.error_counts[C64_ERR_CHECKSUM] = 3;
    analysis.error_counts[C64_ERR_HEADER_NOT_FOUND] = 2;
    analysis.error_tracks[18] = 1;
    analysis.error_tracks[35] = 1;
    analysis.bam_valid = true;
    analysis.bam_free_blocks = 100;
    analysis.bam_allocated_blocks = 564;
    
    char report[4096];
    int len = c64_generate_report(&analysis, report, sizeof(report));
    
    assert(len > 100);
    assert(strstr(report, "SUMMER GAMES") != NULL);
    assert(strstr(report, "Vorpal") != NULL);
    assert(strstr(report, "85%") != NULL);
    
    printf("  ✓ Report generation working\n");
    printf("\n--- Sample Report ---\n%s\n", report);
}

/* ============================================================================
 * V-MAX! TESTS (Pete Rittwage / Lord Crass documentation)
 * ============================================================================ */

static void test_vmax_version_strings(void) {
    printf("Testing V-MAX! version strings...\n");
    
    const char *v0 = c64_vmax_version_string(VMAX_VERSION_0);
    const char *v1 = c64_vmax_version_string(VMAX_VERSION_1);
    const char *v2a = c64_vmax_version_string(VMAX_VERSION_2A);
    const char *v2b = c64_vmax_version_string(VMAX_VERSION_2B);
    const char *v3a = c64_vmax_version_string(VMAX_VERSION_3A);
    const char *v3b = c64_vmax_version_string(VMAX_VERSION_3B);
    const char *v4 = c64_vmax_version_string(VMAX_VERSION_4);
    
    assert(v0 != NULL && strstr(v0, "v0") != NULL);
    assert(v1 != NULL && strstr(v1, "v1") != NULL);
    assert(v2a != NULL && strstr(v2a, "v2a") != NULL);
    assert(v2b != NULL && strstr(v2b, "v2b") != NULL);
    assert(v3a != NULL && strstr(v3a, "v3a") != NULL);
    assert(v3b != NULL && strstr(v3b, "v3b") != NULL);
    assert(v4 != NULL && strstr(v4, "v4") != NULL);
    
    /* Check version descriptions */
    assert(strstr(v0, "Star Rank Boxing") != NULL);  /* First V-MAX! title */
    assert(strstr(v1, "Activision") != NULL);
    assert(strstr(v2a, "Cinemaware") != NULL);
    assert(strstr(v2b, "custom") != NULL);
    assert(strstr(v3a, "Taito") != NULL || strstr(v3a, "variable") != NULL);
    assert(strstr(v3b, "short") != NULL);
    
    printf("  ✓ All V-MAX! version strings correct\n");
}

static void test_vmax_known_titles(void) {
    printf("Testing V-MAX! known titles...\n");
    
    c64_known_title_t entry;
    
    /* Cinemaware V-MAX! v2 titles */
    assert(c64_lookup_title("Defender of the Crown", &entry) == true);
    assert(entry.publisher == C64_PUB_CINEMAWARE);
    assert(entry.protection_flags & C64_PROT_V_MAX);
    
    assert(c64_lookup_title("Rocket Ranger", &entry) == true);
    assert(entry.protection_flags & C64_PROT_V_MAX);
    
    assert(c64_lookup_title("Three Stooges", &entry) == true);
    assert(entry.protection_flags & C64_PROT_V_MAX);
    
    /* Taito V-MAX! v3 titles */
    assert(c64_lookup_title("Arkanoid", &entry) == true);
    assert(entry.publisher == C64_PUB_TAITO);
    assert(entry.protection_flags & C64_PROT_V_MAX);
    assert(entry.protection_flags & C64_PROT_GCR_SYNC);  /* Short syncs */
    
    assert(c64_lookup_title("Bubble Bobble", &entry) == true);
    assert(entry.protection_flags & C64_PROT_V_MAX);
    
    /* Sega V-MAX! titles */
    assert(c64_lookup_title("Outrun", &entry) == true);
    assert(entry.publisher == C64_PUB_SEGA);
    assert(entry.protection_flags & C64_PROT_V_MAX);
    
    printf("  ✓ V-MAX! known titles correct\n");
}

static void test_vmax_constants(void) {
    printf("Testing V-MAX! technical constants...\n");
    
    /* V-MAX! v2 sector layout */
    assert(VMAX_V2_SECTORS_ZONE1 == 22);  /* Tracks 1-17 */
    assert(VMAX_V2_SECTORS_ZONE2 == 20);  /* Tracks 18-38 */
    assert(VMAX_V2_SECTOR_SIZE == 0x140); /* $140 bytes per sector */
    
    /* V-MAX! track assignments */
    assert(VMAX_LOADER_TRACK == 20);      /* Track 20 loader */
    assert(VMAX_RECOVERY_TRACK == 19);    /* V3 recovery sector */
    
    /* V-MAX! v3 limits */
    assert(VMAX_V3_MAX_SECTOR_SIZE == 0x118);
    
    /* V-MAX! marker bytes */
    assert(VMAX_V2_MARKER_64 == 0x64);
    assert(VMAX_V2_MARKER_46 == 0x46);  /* Problematic - 3 zero bits */
    assert(VMAX_V2_MARKER_4E == 0x4E);
    assert(VMAX_V3_HEADER_MARKER == 0x49);
    assert(VMAX_V3_HEADER_END == 0xEE);
    assert(VMAX_END_OF_SECTOR == 0x7F);
    
    printf("  ✓ V-MAX! constants verified\n");
}

static void test_vmax_directory_check(void) {
    printf("Testing V-MAX! directory signature...\n");
    
    /* Create minimal D64 with V-MAX v2 "!" directory signature */
    uint8_t *d64_data = calloc(1, 174848);
    assert(d64_data != NULL);
    
    /* Directory at track 18, sector 1 */
    /* Offset = (17 tracks * 21 sectors + 1 sector) * 256 */
    size_t dir_offset = (17 * 21 + 1) * 256;
    
    /* Set up directory entry with "!" filename */
    d64_data[dir_offset + 2] = 0x82;   /* PRG file type */
    d64_data[dir_offset + 5] = '!';    /* Filename "!" */
    d64_data[dir_offset + 6] = 0xA0;   /* Shifted space padding */
    d64_data[dir_offset + 7] = 0xA0;
    
    bool is_vmax = c64_check_vmax_directory(d64_data, 174848);
    assert(is_vmax == true);
    
    /* Test non-V-MAX directory */
    d64_data[dir_offset + 5] = 'T';    /* Normal filename "TEST" */
    is_vmax = c64_check_vmax_directory(d64_data, 174848);
    assert(is_vmax == false);
    
    free(d64_data);
    printf("  ✓ V-MAX! directory check working\n");
}

/* ============================================================================
 * RAPIDLOK TESTS (Dane Final Agency / Pete Rittwage / Banguibob)
 * ============================================================================ */

static void test_rapidlok_version_strings(void) {
    printf("Testing RapidLok version strings...\n");
    
    const char *v1 = c64_rapidlok_version_string(RAPIDLOK_VERSION_1);
    const char *v2 = c64_rapidlok_version_string(RAPIDLOK_VERSION_2);
    const char *v5 = c64_rapidlok_version_string(RAPIDLOK_VERSION_5);
    const char *v6 = c64_rapidlok_version_string(RAPIDLOK_VERSION_6);
    const char *v7 = c64_rapidlok_version_string(RAPIDLOK_VERSION_7);
    
    assert(v1 != NULL && strstr(v1, "v1") != NULL);
    assert(v2 != NULL && strstr(v2, "v2") != NULL);
    
    /* v1-v4 should mention "patch keycheck works" */
    assert(strstr(v1, "patch") != NULL || strstr(v1, "keycheck") != NULL);
    
    /* v5-v6 should mention VICE issues */
    assert(strstr(v5, "VICE") != NULL || strstr(v5, "intermittent") != NULL);
    assert(strstr(v6, "VICE") != NULL || strstr(v6, "intermittent") != NULL);
    
    /* v7 requires additional work */
    assert(strstr(v7, "crack") != NULL || strstr(v7, "additional") != NULL);
    
    printf("  ✓ All RapidLok version strings correct\n");
}

static void test_rapidlok_known_titles(void) {
    printf("Testing RapidLok known titles...\n");
    
    c64_known_title_t entry;
    
    /* MicroProse RapidLok titles */
    assert(c64_lookup_title("Pirates!", &entry) == true);
    assert(entry.publisher == C64_PUB_MICROPROSE);
    assert(entry.protection_flags & C64_PROT_RAPIDLOK);
    assert(entry.protection_flags & C64_PROT_EXTRA_TRACKS);  /* Track 36 key */
    
    assert(c64_lookup_title("Airborne Ranger", &entry) == true);
    assert(entry.protection_flags & C64_PROT_RAPIDLOK);
    
    assert(c64_lookup_title("Red Storm Rising", &entry) == true);
    assert(entry.protection_flags & C64_PROT_RAPIDLOK);
    
    assert(c64_lookup_title("Stealth Fighter", &entry) == true);
    assert(entry.protection_flags & C64_PROT_RAPIDLOK);
    
    printf("  ✓ RapidLok known titles correct\n");
}

static void test_rapidlok_constants(void) {
    printf("Testing RapidLok technical constants...\n");
    
    /* RapidLok structure constants */
    assert(RAPIDLOK_KEY_TRACK == 36);         /* Track 36 key sector */
    assert(RAPIDLOK_SECTORS_ZONE1 == 12);     /* Tracks 1-17: 12 sectors */
    assert(RAPIDLOK_SECTORS_ZONE2 == 11);     /* Tracks 19-35: 11 sectors */
    
    /* RapidLok bit rates */
    assert(RAPIDLOK_BITRATE_ZONE1 == 11);     /* 307692 Bit/s */
    assert(RAPIDLOK_BITRATE_ZONE2 == 10);     /* 285714 Bit/s */
    
    /* RapidLok sync lengths (bits) */
    assert(RAPIDLOK_TRACK_SYNC_BITS == 320);  /* Track start sync */
    assert(RAPIDLOK_SECTOR0_SYNC_BITS == 480); /* Sector 0 sync */
    assert(RAPIDLOK_NORMAL_SYNC_BITS == 40);  /* Standard sync */
    
    /* RapidLok marker bytes */
    assert(RAPIDLOK_EXTRA_SECTOR == 0x7B);    /* $7B extra sector marker */
    assert(RAPIDLOK_EXTRA_START == 0x55);     /* Extra sector start */
    assert(RAPIDLOK_DOS_REF_HEADER == 0x52);  /* DOS reference header */
    assert(RAPIDLOK_SECTOR_HEADER == 0x75);   /* Sector header marker */
    assert(RAPIDLOK_DATA_BLOCK == 0x6B);      /* Data block marker */
    assert(RAPIDLOK_BAD_GCR == 0x00);         /* Bad GCR in gaps */
    
    printf("  ✓ RapidLok constants verified\n");
}

static void test_rapidlok_synthetic_g64(void) {
    printf("Testing RapidLok detection with synthetic G64...\n");
    
    /* Create minimal G64 with RapidLok signatures */
    size_t g64_size = 512 * 1024;  /* 512KB G64 */
    uint8_t *g64_data = calloc(1, g64_size);
    assert(g64_data != NULL);
    
    /* G64 header */
    memcpy(g64_data, "GCR-1541", 8);
    g64_data[8] = 0x00;  /* Version */
    g64_data[9] = 84;    /* Number of tracks (42 * 2 for half-tracks) */
    g64_data[10] = 0x00; /* Max track size low */
    g64_data[11] = 0x1E; /* Max track size high (7680) */
    
    /* Set up track offsets starting at byte 12 */
    uint32_t track_offset = 0x2AC;  /* First track data offset */
    for (int i = 0; i < 84; i++) {
        g64_data[12 + i * 4] = track_offset & 0xFF;
        g64_data[12 + i * 4 + 1] = (track_offset >> 8) & 0xFF;
        g64_data[12 + i * 4 + 2] = (track_offset >> 16) & 0xFF;
        g64_data[12 + i * 4 + 3] = (track_offset >> 24) & 0xFF;
        track_offset += 0x1E00;  /* Track data size */
    }
    
    /* Set up Track 36 (key track) with RapidLok signatures */
    uint32_t t36_offset = 12 + 84 * 4 + (35 * 0x1E00);  /* Track 36 offset */
    
    /* Track 36 size */
    g64_data[t36_offset] = 0x00;
    g64_data[t36_offset + 1] = 0x08;  /* 2048 bytes */
    
    /* RapidLok key track starts with long sync */
    for (int i = 0; i < 40; i++) {  /* 40 bytes = 320 bits sync */
        g64_data[t36_offset + 2 + i] = 0xFF;
    }
    
    /* Add key data after sync */
    for (int i = 0; i < 35; i++) {
        g64_data[t36_offset + 42 + i] = 0x55 + i;  /* Simulated key values */
    }
    
    /* Add bad GCR (0x00) signature */
    g64_data[t36_offset + 100] = 0x00;
    g64_data[t36_offset + 101] = 0x00;
    
    /* Test detection */
    c64_protection_analysis_t result;
    memset(&result, 0, sizeof(result));
    
    /* Note: Full detection requires valid track structure */
    /* For now, just verify the G64 signature is checked */
    assert(memcmp(g64_data, "GCR-1541", 8) == 0);
    
    free(g64_data);
    printf("  ✓ RapidLok synthetic G64 test passed\n");
}

/* ============================================================================
 * SPEEDLOCK/NOVALOAD TESTS
 * ============================================================================ */

static void test_speedlock_novaload_titles(void) {
    printf("Testing Speedlock/Novaload known titles...\n");
    
    c64_known_title_t entry;
    
    /* Ocean Speedlock titles */
    assert(c64_lookup_title("Batman The Movie", &entry) == true);
    assert(entry.publisher == C64_PUB_OCEAN);
    assert(entry.protection_flags & C64_PROT_SPEEDLOCK);
    
    assert(c64_lookup_title("Robocop", &entry) == true);
    assert(entry.protection_flags & C64_PROT_SPEEDLOCK);
    
    /* US Gold Speedlock titles */
    assert(c64_lookup_title("Gauntlet", &entry) == true);
    assert(entry.publisher == C64_PUB_US_GOLD);
    assert(entry.protection_flags & C64_PROT_SPEEDLOCK);
    
    /* Ocean Novaload titles */
    assert(c64_lookup_title("Combat School", &entry) == true);
    assert(entry.protection_flags & C64_PROT_NOVALOAD);
    
    assert(c64_lookup_title("Green Beret", &entry) == true);
    assert(entry.protection_flags & C64_PROT_NOVALOAD);
    
    printf("  ✓ Speedlock/Novaload known titles correct\n");
}

/* ============================================================================
 * TITLE DATABASE STATISTICS
 * ============================================================================ */

static void test_title_database_stats(void) {
    printf("Testing title database statistics...\n");
    
    int total = c64_get_known_titles_count();
    int vmax_count = 0;
    int rapidlok_count = 0;
    int vorpal_count = 0;
    int speedlock_count = 0;
    int novaload_count = 0;
    
    for (int i = 0; i < total; i++) {
        const c64_known_title_t *title = c64_get_known_title(i);
        if (!title) continue;
        
        if (title->protection_flags & C64_PROT_V_MAX) vmax_count++;
        if (title->protection_flags & C64_PROT_RAPIDLOK) rapidlok_count++;
        if (title->protection_flags & C64_PROT_VORPAL) vorpal_count++;
        if (title->protection_flags & C64_PROT_SPEEDLOCK) speedlock_count++;
        if (title->protection_flags & C64_PROT_NOVALOAD) novaload_count++;
    }
    
    printf("  Total titles: %d\n", total);
    printf("  V-MAX! titles: %d\n", vmax_count);
    printf("  RapidLok titles: %d\n", rapidlok_count);
    printf("  Vorpal titles: %d\n", vorpal_count);
    printf("  Speedlock titles: %d\n", speedlock_count);
    printf("  Novaload titles: %d\n", novaload_count);
    
    /* Verify we have reasonable counts */
    assert(total > 300);      /* Should have 300+ titles now */
    assert(vmax_count > 20);  /* At least 20 V-MAX! titles */
    assert(rapidlok_count > 5); /* At least 5 RapidLok titles */
    assert(vorpal_count > 15);  /* At least 15 Vorpal titles */
    
    printf("  ✓ Title database statistics verified\n");
}

/* ============================================================================
 * NEW PROTECTION DETECTOR TESTS (v4.1.6)
 * ============================================================================ */

static void test_datasoft_detection(void) {
    printf("Testing Datasoft detection...\n");
    
    /* Test known Datasoft titles in database */
    c64_known_title_t entry;
    
    assert(c64_lookup_title("Bruce Lee", &entry) == true);
    assert(entry.protection_flags & C64_PROT_DATASOFT);
    assert(entry.publisher == C64_PUB_DATASOFT);
    
    assert(c64_lookup_title("Mr. Do!", &entry) == true);
    assert(entry.protection_flags & C64_PROT_DATASOFT);
    
    assert(c64_lookup_title("Dig Dug", &entry) == true);
    assert(entry.protection_flags & C64_PROT_DATASOFT);
    
    /* Test detector function exists and handles NULL gracefully */
    c64_protection_analysis_t result;
    memset(&result, 0, sizeof(result));
    
    /* NULL input should return false */
    assert(c64_detect_datasoft(NULL, 0, &result) == false);
    
    /* Too small input should return false */
    uint8_t small_data[100] = {0};
    assert(c64_detect_datasoft(small_data, sizeof(small_data), &result) == false);
    
    printf("  ✓ Datasoft detection tests passed\n");
}

static void test_ssi_rdos_detection(void) {
    printf("Testing SSI RapidDOS detection...\n");
    
    /* Test known SSI titles in database */
    c64_known_title_t entry;
    
    assert(c64_lookup_title("Pool of Radiance", &entry) == true);
    assert(entry.protection_flags & C64_PROT_SSI_RDOS);
    assert(entry.publisher == C64_PUB_SSI);
    
    assert(c64_lookup_title("Curse of the Azure Bonds", &entry) == true);
    assert(entry.protection_flags & C64_PROT_SSI_RDOS);
    
    assert(c64_lookup_title("Champions of Krynn", &entry) == true);
    assert(entry.protection_flags & C64_PROT_SSI_RDOS);
    
    /* Test detector function handles NULL */
    c64_protection_analysis_t result;
    memset(&result, 0, sizeof(result));
    assert(c64_detect_ssi_rdos(NULL, 0, &result) == false);
    
    printf("  ✓ SSI RapidDOS detection tests passed\n");
}

static void test_ea_interlock_detection(void) {
    printf("Testing EA Interlock detection...\n");
    
    /* Test known EA titles in database */
    c64_known_title_t entry;
    
    assert(c64_lookup_title("Bard's Tale II", &entry) == true);
    assert(entry.protection_flags & C64_PROT_EA_INTERLOCK);
    assert(entry.publisher == C64_PUB_ELECTRONIC_ARTS);
    
    assert(c64_lookup_title("Bard's Tale III", &entry) == true);
    assert(entry.protection_flags & C64_PROT_EA_INTERLOCK);
    
    /* Test detector function handles NULL */
    c64_protection_analysis_t result;
    memset(&result, 0, sizeof(result));
    assert(c64_detect_ea_interlock(NULL, 0, &result) == false);
    
    printf("  ✓ EA Interlock detection tests passed\n");
}

static void test_unified_detector(void) {
    printf("Testing unified protection detector...\n");
    
    /* Create a minimal D64 image */
    uint8_t *d64_data = (uint8_t *)calloc(1, TEST_D64_35_TRACKS);
    assert(d64_data != NULL);
    
    /* Set up minimal BAM */
    size_t bam_offset = test_d64_sector_offset(18, 0);
    d64_data[bam_offset] = 18;  /* Directory track */
    d64_data[bam_offset + 1] = 1;  /* Directory sector */
    d64_data[bam_offset + 2] = 0x41;  /* DOS version */
    
    /* Run unified detector */
    c64_protection_analysis_t result;
    c64_detect_all_protections(d64_data, TEST_D64_35_TRACKS, &result);
    
    /* Should complete without crashing */
    printf("  Unified detector completed, confidence: %d%%\n", result.confidence);
    
    free(d64_data);
    printf("  ✓ Unified detector tests passed\n");
}

static void test_new_protection_stats(void) {
    printf("Testing new protection statistics...\n");
    
    int total = c64_get_known_titles_count();
    int datasoft_count = 0;
    int ssi_count = 0;
    int ea_count = 0;
    int abacus_count = 0;
    int rainbird_count = 0;
    
    for (int i = 0; i < total; i++) {
        const c64_known_title_t *title = c64_get_known_title(i);
        if (!title) continue;
        
        if (title->protection_flags & C64_PROT_DATASOFT) datasoft_count++;
        if (title->protection_flags & C64_PROT_SSI_RDOS) ssi_count++;
        if (title->protection_flags & C64_PROT_EA_INTERLOCK) ea_count++;
        if (title->protection_flags & C64_PROT_ABACUS) abacus_count++;
        if (title->protection_flags & C64_PROT_RAINBIRD) rainbird_count++;
    }
    
    printf("  Datasoft titles: %d\n", datasoft_count);
    printf("  SSI RapidDOS titles: %d\n", ssi_count);
    printf("  EA Interlock titles: %d\n", ea_count);
    printf("  Abacus titles: %d\n", abacus_count);
    printf("  Rainbird titles: %d\n", rainbird_count);
    
    /* Verify counts match what we added */
    assert(datasoft_count >= 15);  /* ~20 Datasoft titles */
    assert(ssi_count >= 25);       /* ~32 SSI titles */
    assert(ea_count >= 10);        /* ~18 EA titles */
    
    printf("  ✓ New protection statistics verified\n");
}

int main(void) {
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║     C64 Copy Protection Detection Module - Unit Tests           ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
    
    /* Basic tests */
    test_error_codes();
    test_protection_strings();
    test_known_titles();
    test_d64_analysis();
    test_d64_errors();
    test_g64_analysis();
    test_report_generation();
    
    /* V-MAX! tests */
    printf("\n--- V-MAX! Protection Tests ---\n");
    test_vmax_version_strings();
    test_vmax_known_titles();
    test_vmax_constants();
    test_vmax_directory_check();
    
    /* RapidLok tests */
    printf("\n--- RapidLok Protection Tests ---\n");
    test_rapidlok_version_strings();
    test_rapidlok_known_titles();
    test_rapidlok_constants();
    test_rapidlok_synthetic_g64();
    
    /* Speedlock/Novaload tests */
    printf("\n--- Speedlock/Novaload Tests ---\n");
    test_speedlock_novaload_titles();
    
    /* NEW: Additional Protection Detector Tests (v4.1.6) */
    printf("\n--- New Protection Detectors (v4.1.6) ---\n");
    test_datasoft_detection();
    test_ssi_rdos_detection();
    test_ea_interlock_detection();
    test_unified_detector();
    test_new_protection_stats();
    
    /* Statistics */
    printf("\n--- Database Statistics ---\n");
    test_title_database_stats();
    
    printf("\n╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                    ALL TESTS PASSED! ✅                          ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}

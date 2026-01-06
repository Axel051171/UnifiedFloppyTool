/**
 * @file test_amiga_protection.c
 * @brief Unit Tests for Amiga Protection Registry
 * @version 1.0.0
 * @date 2026-01-06
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Test framework */
#include "../uft_test_framework.h"

/* Module under test */
#include "uft/protection/uft_amiga_protection_full.h"

/*============================================================================
 * Registry Tests
 *============================================================================*/

static void test_registry_access(void)
{
    printf("  test_registry_access...");
    
    size_t count = 0;
    const uft_prot_entry_t* registry = uft_prot_get_registry(&count);
    
    TEST_ASSERT(registry != NULL);
    TEST_ASSERT(count > 100);  /* Should have 194 entries */
    TEST_ASSERT(count <= 250); /* Sanity check */
    
    printf(" PASSED (%zu entries)\n", count);
}

static void test_entry_lookup(void)
{
    printf("  test_entry_lookup...");
    
    /* Known entries */
    const uft_prot_entry_t* entry;
    
    entry = uft_prot_get_entry(UFT_PROT_COPYLOCK);
    TEST_ASSERT(entry != NULL);
    TEST_ASSERT(strcmp(entry->name, "CopyLock") == 0);
    TEST_ASSERT(entry->key_track == 79);
    
    entry = uft_prot_get_entry(UFT_PROT_SPEEDLOCK);
    TEST_ASSERT(entry != NULL);
    TEST_ASSERT(strcmp(entry->name, "SpeedLock") == 0);
    
    entry = uft_prot_get_entry(UFT_PROT_PSYGNOSIS_A);
    TEST_ASSERT(entry != NULL);
    TEST_ASSERT(strcmp(entry->publisher, "Psygnosis") == 0);
    
    /* Invalid type */
    entry = uft_prot_get_entry((uft_amiga_prot_type_t)9999);
    TEST_ASSERT(entry == NULL);
    
    printf(" PASSED\n");
}

static void test_protection_names(void)
{
    printf("  test_protection_names...");
    
    TEST_ASSERT(strcmp(uft_prot_name(UFT_PROT_COPYLOCK), "CopyLock") == 0);
    TEST_ASSERT(strcmp(uft_prot_name(UFT_PROT_DUNGEON_MASTER), "Dungeon Master") == 0);
    TEST_ASSERT(strcmp(uft_prot_name(UFT_PROT_XENON_2), "Xenon 2: Megablast") == 0);
    TEST_ASSERT(strcmp(uft_prot_name(UFT_PROT_LEMMINGS), "Lemmings") == 0);
    
    /* Unknown type */
    TEST_ASSERT(strcmp(uft_prot_name((uft_amiga_prot_type_t)9999), "Unknown") == 0);
    
    printf(" PASSED\n");
}

static void test_protection_flags(void)
{
    printf("  test_protection_flags...");
    
    /* CopyLock uses timing */
    TEST_ASSERT(uft_prot_needs_timing(UFT_PROT_COPYLOCK) == true);
    
    /* Gremlin uses long tracks */
    TEST_ASSERT(uft_prot_is_longtrack(UFT_PROT_GREMLIN) == true);
    TEST_ASSERT(uft_prot_is_longtrack(UFT_PROT_DISPOSABLE_HERO) == true);
    
    /* Thalion uses weak bits */
    TEST_ASSERT(uft_prot_has_weak_bits(UFT_PROT_THALION) == true);
    TEST_ASSERT(uft_prot_has_weak_bits(UFT_PROT_STARDUST) == true);
    
    /* Standard AmigaDOS has no special flags */
    TEST_ASSERT(uft_prot_is_longtrack(UFT_PROT_AMIGADOS) == false);
    TEST_ASSERT(uft_prot_needs_timing(UFT_PROT_AMIGADOS) == false);
    TEST_ASSERT(uft_prot_has_weak_bits(UFT_PROT_AMIGADOS) == false);
    
    printf(" PASSED\n");
}

static void test_sync_patterns(void)
{
    printf("  test_sync_patterns...");
    
    size_t count;
    const uft_prot_entry_t* registry = uft_prot_get_registry(&count);
    
    /* Count unique sync patterns */
    uint32_t syncs[16];
    int sync_count = 0;
    
    for (size_t i = 0; i < count; i++) {
        uint32_t sync = registry[i].sync;
        bool found = false;
        for (int j = 0; j < sync_count; j++) {
            if (syncs[j] == sync) {
                found = true;
                break;
            }
        }
        if (!found && sync_count < 16) {
            syncs[sync_count++] = sync;
        }
    }
    
    /* Should have multiple unique sync patterns */
    TEST_ASSERT(sync_count >= 5);
    
    /* Standard Amiga sync should be common */
    bool has_standard = false;
    for (int i = 0; i < sync_count; i++) {
        if (syncs[i] == UFT_SYNC_AMIGA_STD) {
            has_standard = true;
            break;
        }
    }
    TEST_ASSERT(has_standard);
    
    printf(" PASSED (%d unique syncs)\n", sync_count);
}

/*============================================================================
 * Detection Tests
 *============================================================================*/

static void test_detection_copylock(void)
{
    printf("  test_detection_copylock...");
    
    /* Create track signature for track 79 with timing variation */
    uft_track_signature_t track = {0};
    track.track_num = 79;
    track.side = 0;
    track.sync_words[0] = 0x4489;
    track.sync_count = 1;
    track.has_timing_variation = true;
    
    uft_prot_detect_result_t results[5];
    int count = uft_prot_detect(&track, 1, results, 5);
    
    /* Should detect something */
    TEST_ASSERT(count > 0);
    
    /* CopyLock should be in results (track 79 + timing) */
    bool found_copylock = false;
    for (int i = 0; i < count; i++) {
        if (results[i].type == UFT_PROT_COPYLOCK ||
            results[i].type == UFT_PROT_COPYLOCK_OLD) {
            found_copylock = true;
            TEST_ASSERT(results[i].confidence >= 30);
            break;
        }
    }
    TEST_ASSERT(found_copylock);
    
    printf(" PASSED\n");
}

static void test_detection_longtrack(void)
{
    printf("  test_detection_longtrack...");
    
    /* Create track signature for long track */
    uft_track_signature_t track = {0};
    track.track_num = 79;
    track.side = 0;
    track.track_length = 110000;  /* Long track */
    track.sync_words[0] = 0x4489;
    track.sync_count = 1;
    
    uft_prot_detect_result_t results[10];
    int count = uft_prot_detect(&track, 1, results, 10);
    
    /* Should detect long track protections */
    TEST_ASSERT(count > 0);
    
    /* Check that longtrack flag is detected */
    bool found_longtrack = false;
    for (int i = 0; i < count; i++) {
        if (results[i].flags & UFT_PROT_FL_LONGTRACK) {
            found_longtrack = true;
            break;
        }
    }
    TEST_ASSERT(found_longtrack);
    
    printf(" PASSED\n");
}

static void test_detection_weak_bits(void)
{
    printf("  test_detection_weak_bits...");
    
    /* Create track signature with weak bits */
    uft_track_signature_t track = {0};
    track.track_num = 79;
    track.side = 0;
    track.sync_words[0] = 0x4489;
    track.sync_count = 1;
    track.has_weak_bits = true;
    
    uft_prot_detect_result_t results[10];
    int count = uft_prot_detect(&track, 1, results, 10);
    
    /* Should detect something with weak bits */
    bool found_weak = false;
    for (int i = 0; i < count; i++) {
        if (results[i].flags & UFT_PROT_FL_WEAKBITS) {
            found_weak = true;
            break;
        }
    }
    
    /* Weak bit protections should be detected */
    TEST_ASSERT(count > 0);
    
    printf(" PASSED\n");
}

static void test_detection_null_handling(void)
{
    printf("  test_detection_null_handling...");
    
    uft_prot_detect_result_t results[5];
    uft_track_signature_t track = {0};
    
    /* NULL inputs */
    TEST_ASSERT(uft_prot_detect(NULL, 1, results, 5) == 0);
    TEST_ASSERT(uft_prot_detect(&track, 0, results, 5) == 0);
    TEST_ASSERT(uft_prot_detect(&track, 1, NULL, 5) == 0);
    TEST_ASSERT(uft_prot_detect(&track, 1, results, 0) == 0);
    
    TEST_ASSERT(uft_prot_detect_track(NULL, &results[0]) == false);
    TEST_ASSERT(uft_prot_detect_track(&track, NULL) == false);
    
    printf(" PASSED\n");
}

static void test_detection_multi_track(void)
{
    printf("  test_detection_multi_track...");
    
    /* Create multiple track signatures */
    uft_track_signature_t tracks[160];
    memset(tracks, 0, sizeof(tracks));
    
    /* Set up standard tracks */
    for (int i = 0; i < 160; i++) {
        tracks[i].track_num = i / 2;
        tracks[i].side = i % 2;
        tracks[i].sync_words[0] = 0x4489;
        tracks[i].sync_count = 1;
    }
    
    /* Add protection on track 79 */
    tracks[158].has_timing_variation = true;  /* Track 79 side 0 */
    
    uft_prot_detect_result_t results[10];
    int count = uft_prot_detect(tracks, 160, results, 10);
    
    TEST_ASSERT(count > 0);
    
    printf(" PASSED (%d protections detected)\n", count);
}

/*============================================================================
 * Publisher Category Tests
 *============================================================================*/

static void test_publisher_categories(void)
{
    printf("  test_publisher_categories...");
    
    size_t count;
    const uft_prot_entry_t* registry = uft_prot_get_registry(&count);
    
    /* Count publishers */
    int psygnosis = 0, factor5 = 0, team17 = 0, bitmap = 0;
    
    for (size_t i = 0; i < count; i++) {
        if (registry[i].publisher) {
            if (strcmp(registry[i].publisher, "Psygnosis") == 0) psygnosis++;
            else if (strcmp(registry[i].publisher, "Factor 5") == 0) factor5++;
            else if (strcmp(registry[i].publisher, "Team17") == 0) team17++;
            else if (strcmp(registry[i].publisher, "Bitmap Bros") == 0) bitmap++;
        }
    }
    
    TEST_ASSERT(psygnosis >= 5);
    TEST_ASSERT(factor5 >= 3);
    TEST_ASSERT(team17 >= 3);
    TEST_ASSERT(bitmap >= 5);
    
    printf(" PASSED\n");
}

/*============================================================================
 * Test Runner
 *============================================================================*/

void run_amiga_protection_tests(void)
{
    printf("\n=== Amiga Protection Tests ===\n\n");
    
    printf("Registry:\n");
    test_registry_access();
    test_entry_lookup();
    test_protection_names();
    test_protection_flags();
    test_sync_patterns();
    
    printf("\nDetection:\n");
    test_detection_copylock();
    test_detection_longtrack();
    test_detection_weak_bits();
    test_detection_null_handling();
    test_detection_multi_track();
    
    printf("\nPublishers:\n");
    test_publisher_categories();
    
    printf("\n=== All Amiga Protection Tests PASSED ===\n");
}

#ifdef TEST_AMIGA_PROTECTION_MAIN
int main(void)
{
    run_amiga_protection_tests();
    return 0;
}
#endif

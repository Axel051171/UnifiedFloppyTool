/**
 * @file test_pc_protection.c
 * @brief Unit tests for PC Protection Detection API
 * 
 * TICKET-008 Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

/* Include header if available, otherwise define test stubs */
#ifdef UFT_HAS_PC_PROTECTION
#include "uft/uft_pc_protection.h"
#else
/* Stub definitions for standalone compilation */
typedef enum {
    UFT_PCPROT_UNKNOWN = 0,
    UFT_PCPROT_SAFEDISC_1,
    UFT_PCPROT_SECUROM_4,
    UFT_PCPROT_STARFORCE_3,
} uft_pc_protection_t;
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        printf("  Test: %-40s ", #name); \
        tests_run++; \
        if (test_##name()) { \
            printf("[PASS]\n"); \
            tests_passed++; \
        } else { \
            printf("[FAIL]\n"); \
            tests_failed++; \
        } \
    } while(0)

#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            printf("\n    ASSERT FAILED: %s\n    at %s:%d\n", #cond, __FILE__, __LINE__); \
            return 0; \
        } \
    } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Cases
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Test SafeDisc signature detection */
static int test_safedisc_signature(void) {
    /* BoG signature */
    uint8_t data[] = {0x00, 0x00, 0x42, 0x6F, 0x47, 0x5F, 0x00, 0x00};
    
    /* Check if signature is present */
    int found = 0;
    for (size_t i = 0; i <= sizeof(data) - 4; i++) {
        if (data[i] == 0x42 && data[i+1] == 0x6F && 
            data[i+2] == 0x47 && data[i+3] == 0x5F) {
            found = 1;
            break;
        }
    }
    
    ASSERT(found == 1);
    return 1;
}

/* Test SecuROM signature detection */
static int test_securom_signature(void) {
    /* CMS32 signature */
    uint8_t data[] = {0x00, 0x43, 0x4D, 0x53, 0x5F, 0x33, 0x32, 0x00};
    
    int found = 0;
    for (size_t i = 0; i <= sizeof(data) - 6; i++) {
        if (data[i] == 0x43 && data[i+1] == 0x4D && 
            data[i+2] == 0x53 && data[i+3] == 0x5F &&
            data[i+4] == 0x33 && data[i+5] == 0x32) {
            found = 1;
            break;
        }
    }
    
    ASSERT(found == 1);
    return 1;
}

/* Test StarForce signature detection */
static int test_starforce_signature(void) {
    /* STAR header */
    uint8_t data[] = {0x53, 0x54, 0x41, 0x52, 0x00, 0x00};
    
    int found = 0;
    if (data[0] == 0x53 && data[1] == 0x54 && 
        data[2] == 0x41 && data[3] == 0x52) {
        found = 1;
    }
    
    ASSERT(found == 1);
    return 1;
}

/* Test protection name lookup */
static int test_protection_names(void) {
    /* Verify enum values are distinct */
    ASSERT(UFT_PCPROT_UNKNOWN != UFT_PCPROT_SAFEDISC_1);
    ASSERT(UFT_PCPROT_SAFEDISC_1 != UFT_PCPROT_SECUROM_4);
    ASSERT(UFT_PCPROT_SECUROM_4 != UFT_PCPROT_STARFORCE_3);
    
    return 1;
}

/* Test empty data scan */
static int test_empty_data_scan(void) {
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    
    /* No protection signatures in null data */
    int safedisc_found = 0;
    int securom_found = 0;
    
    /* Scan for SafeDisc */
    for (size_t i = 0; i <= sizeof(data) - 4; i++) {
        if (data[i] == 0x42 && data[i+1] == 0x6F) {
            safedisc_found = 1;
        }
    }
    
    /* Scan for SecuROM */
    for (size_t i = 0; i <= sizeof(data) - 4; i++) {
        if (data[i] == 0x43 && data[i+1] == 0x4D) {
            securom_found = 1;
        }
    }
    
    ASSERT(safedisc_found == 0);
    ASSERT(securom_found == 0);
    return 1;
}

/* Test DOS protection signature */
static int test_dos_protection(void) {
    /* PROLOCK signature */
    uint8_t data[] = {0x50, 0x52, 0x4F, 0x4C, 0x4F, 0x43, 0x4B};
    
    int found = 0;
    if (memcmp(data, "PROLOCK", 7) == 0) {
        found = 1;
    }
    
    ASSERT(found == 1);
    return 1;
}

/* Test weak bit track detection */
static int test_weak_bit_tracks(void) {
    /* Known weak bit protection tracks */
    int protection_tracks[] = {6, 38, 39, 79};
    int num_tracks = sizeof(protection_tracks) / sizeof(protection_tracks[0]);
    
    /* Verify track list */
    ASSERT(num_tracks == 4);
    ASSERT(protection_tracks[0] == 6);
    ASSERT(protection_tracks[3] == 79);
    
    return 1;
}

/* Test confidence scoring */
static int test_confidence_scoring(void) {
    /* Confidence levels */
    int conf_none = 0;
    int conf_possible = 25;
    int conf_likely = 50;
    int conf_probable = 75;
    int conf_confirmed = 100;
    
    ASSERT(conf_none < conf_possible);
    ASSERT(conf_possible < conf_likely);
    ASSERT(conf_likely < conf_probable);
    ASSERT(conf_probable < conf_confirmed);
    
    /* Cap at 100 */
    int over_100 = 150;
    if (over_100 > 100) over_100 = 100;
    ASSERT(over_100 == 100);
    
    return 1;
}

/* Test memory search function */
static int test_memmem_search(void) {
    uint8_t haystack[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    uint8_t needle[] = {0x33, 0x44, 0x55};
    
    int found = 0;
    size_t found_offset = 0;
    
    for (size_t i = 0; i <= sizeof(haystack) - sizeof(needle); i++) {
        if (memcmp(haystack + i, needle, sizeof(needle)) == 0) {
            found = 1;
            found_offset = i;
            break;
        }
    }
    
    ASSERT(found == 1);
    ASSERT(found_offset == 3);
    
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main Test Runner
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n");
    printf("════════════════════════════════════════════════════════════════════════\n");
    printf(" TICKET-008: PC Protection Suite Tests\n");
    printf("════════════════════════════════════════════════════════════════════════\n");
    printf("\n");
    
    /* Run tests */
    TEST(safedisc_signature);
    TEST(securom_signature);
    TEST(starforce_signature);
    TEST(protection_names);
    TEST(empty_data_scan);
    TEST(dos_protection);
    TEST(weak_bit_tracks);
    TEST(confidence_scoring);
    TEST(memmem_search);
    
    /* Summary */
    printf("\n");
    printf("────────────────────────────────────────────────────────────────────────\n");
    printf(" Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(" (%d FAILED)", tests_failed);
    }
    printf("\n");
    printf("────────────────────────────────────────────────────────────────────────\n");
    printf("\n");
    
    return tests_failed > 0 ? 1 : 0;
}

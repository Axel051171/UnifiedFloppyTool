/**
 * @file test_drive_scan.c
 * @brief Unit tests for CBM drive/tool scanner
 */

#include "uft/c64/uft_cbm_drive_scan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST(name) static int test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %-40s ", #name); \
    if (test_##name() == 0) { printf("[PASS]\n"); passed++; } \
    else { printf("[FAIL]\n"); failed++; } \
    total++; \
} while(0)

/* ============================================================================
 * Test Data
 * ============================================================================ */

/* Simulated nibbler-like content */
static const uint8_t NIBBLER_PAYLOAD[] = 
    "TURBO NIBBLER V2.0\x00"
    "M-W HALFTRACK COPY\x00"
    "TRACK 1-40 GCR BURST\x00"
    "1541 DISK STATUS\x00";

/* Simple copier content */
static const uint8_t COPIER_PAYLOAD[] =
    "DISK COPY V1.0\x00"
    "INSERT SOURCE DISK\x00"
    "INSERT DESTINATION\x00"
    "COPY TRACK SECTOR\x00";

/* Fastloader content */
static const uint8_t LOADER_PAYLOAD[] =
    "FAST LOADER 1541\x00"
    "M-R M-W TURBO\x00";

/* Innocent text */
static const uint8_t INNOCENT_PAYLOAD[] =
    "HELLO WORLD\x00"
    "THIS IS A GAME\x00"
    "PRESS FIRE TO START\x00";

/* ============================================================================
 * Test Cases
 * ============================================================================ */

TEST(scan_nibbler)
{
    uft_scan_result_t result;
    
    int ret = uft_cbm_drive_scan_payload(NIBBLER_PAYLOAD, 
                                         sizeof(NIBBLER_PAYLOAD) - 1, 
                                         &result);
    
    if (ret != 0) return 1;
    if (result.score < UFT_SCAN_SCORE_NIBBLER) return 2;
    if (!result.has_gcr_keywords) return 3;
    if (!result.has_halftrack) return 4;
    if (!result.is_nibbler) return 5;
    
    return 0;
}

TEST(scan_copier)
{
    uft_scan_result_t result;
    
    int ret = uft_cbm_drive_scan_payload(COPIER_PAYLOAD,
                                         sizeof(COPIER_PAYLOAD) - 1,
                                         &result);
    
    if (ret != 0) return 1;
    if (!result.has_track_refs) return 2;
    /* Should have some hits but lower score than nibbler */
    if (result.hit_count == 0) return 3;
    
    return 0;
}

TEST(scan_loader)
{
    uft_scan_result_t result;
    
    int ret = uft_cbm_drive_scan_payload(LOADER_PAYLOAD,
                                         sizeof(LOADER_PAYLOAD) - 1,
                                         &result);
    
    if (ret != 0) return 1;
    if (!result.has_dos_commands) return 2;
    if (!result.has_drive_refs) return 3;
    
    return 0;
}

TEST(scan_innocent)
{
    uft_scan_result_t result;
    
    int ret = uft_cbm_drive_scan_payload(INNOCENT_PAYLOAD,
                                         sizeof(INNOCENT_PAYLOAD) - 1,
                                         &result);
    
    if (ret != 0) return 1;
    /* Should have very low or zero score */
    if (result.score > 5) return 2;
    if (result.is_copier) return 3;
    if (result.is_nibbler) return 4;
    
    return 0;
}

TEST(classify_nibbler)
{
    uft_scan_result_t result;
    
    uft_cbm_drive_scan_payload(NIBBLER_PAYLOAD, 
                               sizeof(NIBBLER_PAYLOAD) - 1, 
                               &result);
    
    uft_cbm_tool_type_t type = uft_cbm_classify_tool(&result);
    
    if (type != UFT_CBM_TOOL_NIBBLER) return 1;
    
    return 0;
}

TEST(has_dos_command)
{
    if (!uft_cbm_has_dos_command(NIBBLER_PAYLOAD, 
                                  sizeof(NIBBLER_PAYLOAD) - 1, 
                                  "M-W")) return 1;
    
    if (!uft_cbm_has_dos_command(LOADER_PAYLOAD,
                                  sizeof(LOADER_PAYLOAD) - 1,
                                  "M-R")) return 2;
    
    if (uft_cbm_has_dos_command(INNOCENT_PAYLOAD,
                                 sizeof(INNOCENT_PAYLOAD) - 1,
                                 "M-W")) return 3;
    
    return 0;
}

TEST(identify_tool)
{
    char name[64] = {0};
    
    int found = uft_cbm_identify_tool(NIBBLER_PAYLOAD,
                                       sizeof(NIBBLER_PAYLOAD) - 1,
                                       name, sizeof(name));
    
    if (!found) return 1;
    if (name[0] == '\0') return 2;
    /* Should identify as nibbler type */
    if (strstr(name, "NIBBLER") == NULL && strstr(name, "Nibbler") == NULL) return 3;
    
    return 0;
}

TEST(extract_strings)
{
    char *strings[16] = {0};
    
    int count = uft_cbm_extract_strings(NIBBLER_PAYLOAD,
                                         sizeof(NIBBLER_PAYLOAD) - 1,
                                         strings,
                                         16);
    
    if (count == 0) return 1;
    
    /* Check that we found some strings */
    int found_turbo = 0;
    for (int i = 0; i < count; i++) {
        if (strings[i] && strstr(strings[i], "TURBO")) found_turbo = 1;
        free(strings[i]);  /* Clean up allocated strings */
    }
    
    if (!found_turbo) return 2;
    
    return 0;
}

TEST(tool_type_names)
{
    if (strstr(uft_cbm_tool_type_name(UFT_CBM_TOOL_COPIER), "Copier") == NULL) return 1;
    if (strstr(uft_cbm_tool_type_name(UFT_CBM_TOOL_NIBBLER), "Nibbler") == NULL) return 2;
    if (strstr(uft_cbm_tool_type_name(UFT_CBM_TOOL_FASTLOADER), "Loader") == NULL) return 3;
    
    return 0;
}

TEST(scan_prg_wrapper)
{
    /* Create fake PRG with 2-byte header */
    uint8_t prg[128];
    prg[0] = 0x01;  /* Load address lo */
    prg[1] = 0x08;  /* Load address hi = $0801 */
    memcpy(prg + 2, NIBBLER_PAYLOAD, sizeof(NIBBLER_PAYLOAD) - 1);
    
    uft_scan_result_t result;
    int ret = uft_cbm_drive_scan_prg(prg, 2 + sizeof(NIBBLER_PAYLOAD) - 1, &result);
    
    if (ret != 0) return 1;
    if (!result.is_nibbler) return 2;
    
    return 0;
}

TEST(null_handling)
{
    uft_scan_result_t result;
    
    /* NULL output */
    if (uft_cbm_drive_scan_payload(NIBBLER_PAYLOAD, 10, NULL) != -1) return 1;
    
    /* NULL input */
    if (uft_cbm_drive_scan_payload(NULL, 10, &result) != -2) return 2;
    
    /* Zero length */
    if (uft_cbm_drive_scan_payload(NIBBLER_PAYLOAD, 0, &result) != -2) return 3;
    
    /* NULL for has_dos_command */
    if (uft_cbm_has_dos_command(NULL, 10, "M-W") != 0) return 4;
    if (uft_cbm_has_dos_command(NIBBLER_PAYLOAD, 10, NULL) != 0) return 5;
    
    return 0;
}

TEST(hit_recording)
{
    uft_scan_result_t result;
    
    uft_cbm_drive_scan_payload(NIBBLER_PAYLOAD,
                               sizeof(NIBBLER_PAYLOAD) - 1,
                               &result);
    
    if (result.hit_count == 0) return 1;
    
    /* Check first hit has valid data */
    if (result.hits[0].offset > sizeof(NIBBLER_PAYLOAD)) return 2;
    if (result.hits[0].text[0] == '\0') return 3;
    if (result.hits[0].score == 0) return 4;
    if (result.hits[0].category == NULL) return 5;
    
    return 0;
}

TEST(case_insensitive)
{
    /* Test lowercase detection */
    const uint8_t lowercase[] = "turbo nibbler gcr halftrack m-w";
    uft_scan_result_t result;
    
    int ret = uft_cbm_drive_scan_payload(lowercase, sizeof(lowercase) - 1, &result);
    
    if (ret != 0) return 1;
    if (result.score == 0) return 2;  /* Should still detect */
    if (!result.has_gcr_keywords) return 3;
    if (!result.has_halftrack) return 4;
    
    return 0;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    int passed = 0, failed = 0, total = 0;
    
    printf("\n=== CBM Drive Scanner Tests ===\n\n");
    
    RUN_TEST(scan_nibbler);
    RUN_TEST(scan_copier);
    RUN_TEST(scan_loader);
    RUN_TEST(scan_innocent);
    RUN_TEST(classify_nibbler);
    RUN_TEST(has_dos_command);
    RUN_TEST(identify_tool);
    RUN_TEST(extract_strings);
    RUN_TEST(tool_type_names);
    RUN_TEST(scan_prg_wrapper);
    RUN_TEST(null_handling);
    RUN_TEST(hit_recording);
    RUN_TEST(case_insensitive);
    
    printf("\n-----------------------------------------\n");
    printf("Results: %d/%d passed", passed, total);
    if (failed > 0) printf(", %d FAILED", failed);
    printf("\n\n");
    
    return failed > 0 ? 1 : 0;
}

// SPDX-License-Identifier: MIT
/*
 * bootblock_db.c - Bootblock Database Implementation
 * 
 * Detection engine with pattern matching and CRC32 verification.
 * 
 * Performance: ~1ms for pattern match, ~5ms with CRC32
 * 
 * @version 1.0.0
 * @date 2024-12-25
 */

#include "uft/uft_error.h"
#include "bootblock_db.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================*
 * DATABASE STORAGE
 *============================================================================*/

static bb_signature_t *signatures = NULL;
static int signature_count = 0;
static int signatures_capacity = 0;

static int virus_count_cache = 0;
static int xcopy_count_cache = 0;

/*============================================================================*
 * CATEGORY NAMES
 *============================================================================*/

const char* bb_category_name(bootblock_category_t category)
{
    switch (category) {
        case BB_CAT_UTILITY:      return "Utility";
        case BB_CAT_VIRUS:        return "VIRUS";
        case BB_CAT_LOADER:       return "Loader";
        case BB_CAT_SCENE:        return "Scene";
        case BB_CAT_INTRO:        return "Intro";
        case BB_CAT_BOOTLOADER:   return "Bootloader";
        case BB_CAT_XCOPY:        return "X-Copy";
        case BB_CAT_CUSTOM:       return "Custom";
        case BB_CAT_DEMOSCENE:    return "Demoscene";
        case BB_CAT_VIRUS_FAKE:   return "Virus (Fake)";
        case BB_CAT_GAME:         return "Game";
        case BB_CAT_PASSWORD:     return "Password";
        default:                  return "Unknown";
    }
}

/**
 * @brief Parse category string to enum
 */
static bootblock_category_t parse_category(const char *cat)
{
    if (!cat) return BB_CAT_UNKNOWN;
    
    if (strcmp(cat, "u") == 0) return BB_CAT_UTILITY;
    if (strcmp(cat, "v") == 0) return BB_CAT_VIRUS;
    if (strcmp(cat, "l") == 0) return BB_CAT_LOADER;
    if (strcmp(cat, "sc") == 0) return BB_CAT_SCENE;
    if (strcmp(cat, "i") == 0) return BB_CAT_INTRO;
    if (strcmp(cat, "bl") == 0) return BB_CAT_BOOTLOADER;
    if (strcmp(cat, "xc") == 0) return BB_CAT_XCOPY;
    if (strcmp(cat, "cust") == 0) return BB_CAT_CUSTOM;
    if (strcmp(cat, "ds") == 0) return BB_CAT_DEMOSCENE;
    if (strcmp(cat, "vfm") == 0) return BB_CAT_VIRUS_FAKE;
    if (strcmp(cat, "g") == 0) return BB_CAT_GAME;
    if (strcmp(cat, "p") == 0) return BB_CAT_PASSWORD;
    
    return BB_CAT_UNKNOWN;
}

/*============================================================================*
 * PATTERN MATCHING
 *============================================================================*/

/**
 * @brief Check if bootblock matches pattern
 * 
 * Fast pattern matching using offset,value pairs.
 * 
 * @param bootblock Bootblock data
 * @param pattern Pattern to match
 * @return bool true if all pattern elements match
 */
static bool pattern_matches(const uint8_t bootblock[BOOTBLOCK_SIZE], 
                           const bb_pattern_t *pattern)
{
    if (!pattern || pattern->count == 0) {
        return false;
    }
    
    for (uint8_t i = 0; i < pattern->count; i++) {
        uint16_t offset = pattern->elements[i].offset;
        uint8_t expected = pattern->elements[i].value;
        
        // Check bounds
        if (offset >= BOOTBLOCK_SIZE) {
            return false;
        }
        
        // Check value
        if (bootblock[offset] != expected) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Parse pattern string from XML
 * 
 * Format: "offset,value,offset,value,..."
 * Example: "471,104,481,114,371,242"
 * 
 * @param pattern_str Pattern string
 * @param pattern Output pattern
 * @return int 0 on success, negative on error
 */
static int parse_pattern(const char *pattern_str, bb_pattern_t *pattern)
{
    if (!pattern_str || !pattern) {
        return -1;
    }
    
    memset(pattern, 0, sizeof(*pattern));
    
    // Parse comma-separated offset,value pairs
    char *str_copy = strdup(pattern_str);
    if (!str_copy) return -1;
    
    char *token = strtok(str_copy, ",");
    int count = 0;
    
    while (token && count < BOOTBLOCK_MAX_PATTERNS) {
        // Parse offset
        int offset = atoi(token);
        token = strtok(NULL, ",");
        if (!token) break;
        
        // Parse value
        int value = atoi(token);
        token = strtok(NULL, ",");
        
        // Add to pattern
        if (offset >= 0 && offset < BOOTBLOCK_SIZE && value >= 0 && value <= 255) {
            pattern->elements[count].offset = (uint16_t)offset;
            pattern->elements[count].value = (uint8_t)value;
            count++;
        }
    }
    
    free(str_copy);
    pattern->count = (uint8_t)count;
    
    return 0;
}

/*============================================================================*
 * DATABASE MANAGEMENT
 *============================================================================*/

/**
 * @brief Add signature to database
 */
static int add_signature(const bb_signature_t *sig)
{
    if (!sig) return -1;
    
    // Expand capacity if needed
    if (signature_count >= signatures_capacity) {
        int new_capacity = (signatures_capacity == 0) ? 512 : signatures_capacity * 2;
        bb_signature_t *new_sigs = (bb_signature_t*)realloc(signatures, 
                                                             new_capacity * sizeof(bb_signature_t));
        if (!new_sigs) return -1;
        
        signatures = new_sigs;
        signatures_capacity = new_capacity;
    }
    
    // Add signature
    memcpy(&signatures[signature_count], sig, sizeof(bb_signature_t));
    signature_count++;
    
    // Update cache
    if (bb_is_virus(sig->category)) {
        virus_count_cache++;
    }
    if (sig->category == BB_CAT_XCOPY) {
        xcopy_count_cache++;
    }
    
    return 0;
}

void bb_db_free(void)
{
    if (signatures) {
        free(signatures);
        signatures = NULL;
    }
    signature_count = 0;
    signatures_capacity = 0;
    virus_count_cache = 0;
    xcopy_count_cache = 0;
}

int bb_db_get_stats(int *total_count, int *virus_count, int *xcopy_count)
{
    if (total_count) *total_count = signature_count;
    if (virus_count) *virus_count = virus_count_cache;
    if (xcopy_count) *xcopy_count = xcopy_count_cache;
    return 0;
}

/*============================================================================*
 * SIMPLE XML PARSER (brainfile.xml)
 *============================================================================*/

/**
 * @brief Simple XML tag extractor
 * 
 * Finds <tag>content</tag> and returns content.
 * 
 * @param xml XML string
 * @param tag Tag name (without <>)
 * @param output Output buffer
 * @param output_size Output buffer size
 * @return bool true if found
 */
static bool extract_xml_tag(const char *xml, const char *tag, char *output, size_t output_size)
{
    if (!xml || !tag || !output) return false;
    
    // Build opening tag
    char open_tag[128];
    snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
    
    // Build closing tag
    char close_tag[128];
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);
    
    // Find tags
    const char *start = strstr(xml, open_tag);
    if (!start) return false;
    start += strlen(open_tag);
    
    const char *end = strstr(start, close_tag);
    if (!end) return false;
    
    // Extract content
    size_t len = end - start;
    if (len >= output_size) len = output_size - 1;
    
    memcpy(output, start, len);
    output[len] = '\0';
    
    return true;
}

/**
 * @brief Parse CRC32 from hex string
 */
static uint32_t parse_crc32(const char *hex)
{
    if (!hex) return 0;
    return (uint32_t)strtoul(hex, NULL, 16);
}

/**
 * @brief Parse single bootblock entry from XML
 */
static int parse_bootblock_entry(const char *xml_block, bb_signature_t *sig)
{
    if (!xml_block || !sig) return -1;
    
    memset(sig, 0, sizeof(*sig));
    
    char buffer[512];
    
    // Name
    if (extract_xml_tag(xml_block, "n", buffer, sizeof(buffer))) {
        snprintf(sig->name, sizeof(sig->name), "%s", buffer);
    }
    
    // Category
    if (extract_xml_tag(xml_block, "Class", buffer, sizeof(buffer))) {
        sig->category = parse_category(buffer);
    }
    
    // CRC32
    if (extract_xml_tag(xml_block, "CRC", buffer, sizeof(buffer))) {
        sig->crc32 = parse_crc32(buffer);
    }
    
    // Pattern (Recog)
    if (extract_xml_tag(xml_block, "Recog", buffer, sizeof(buffer))) {
        parse_pattern(buffer, &sig->pattern);
    }
    
    // Bootable
    if (extract_xml_tag(xml_block, "Bootable", buffer, sizeof(buffer))) {
        sig->bootable = (strcmp(buffer, "True") == 0);
    }
    
    // Data
    if (extract_xml_tag(xml_block, "Data", buffer, sizeof(buffer))) {
        sig->has_data = (strcmp(buffer, "True") == 0);
    }
    
    // Kickstart
    if (extract_xml_tag(xml_block, "KS", buffer, sizeof(buffer))) {
        snprintf(sig->kickstart, sizeof(sig->kickstart), "%s", buffer);
    }
    
    // Notes
    if (extract_xml_tag(xml_block, "Notes", buffer, sizeof(buffer))) {
        snprintf(sig->notes, sizeof(sig->notes), "%s", buffer);
    }
    
    // URL
    if (extract_xml_tag(xml_block, "URL", buffer, sizeof(buffer))) {
        snprintf(sig->url, sizeof(sig->url), "%s", buffer);
    }
    
    return 0;
}

/*============================================================================*
 * DATABASE INITIALIZATION
 *============================================================================*/

int bb_db_init(const char *xml_path)
{
    // Free existing database
    bb_db_free();
    
    // Determine XML path
    const char *path = xml_path ? xml_path : "brainfile.xml";
    
    // Open XML file
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "bb_db_init: Cannot open %s\n", path);
        return -1;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 100*1024*1024) { // Max 100MB
        fclose(f);
        return -1;
    }
    
    // Read entire file
    char *xml = (char*)malloc(size + 1);
    if (!xml) {
        fclose(f);
        return -1;
    }
    
    if (fread(xml, 1, size, f) != (size_t)size) {
        free(xml);
        fclose(f);
        return -1;
    }
    xml[size] = '\0';
    fclose(f);
    
    // Parse XML (simple approach: find all <Bootblock>...</Bootblock>)
    const char *block_start = xml;
    while ((block_start = strstr(block_start, "<Bootblock>")) != NULL) {
        const char *block_end = strstr(block_start, "</Bootblock>");
        if (!block_end) break;
        
        // Extract block
        size_t block_len = block_end - block_start + strlen("</Bootblock>");
        char *block = (char*)malloc(block_len + 1);
        if (!block) break;
        
        memcpy(block, block_start, block_len);
        block[block_len] = '\0';
        
        // Parse signature
        bb_signature_t sig;
        if (parse_bootblock_entry(block, &sig) == 0) {
            // Only add if we have either pattern or CRC
            if (sig.pattern.count > 0 || sig.crc32 != 0) {
                add_signature(&sig);
            }
        }
        
        free(block);
        block_start = block_end + strlen("</Bootblock>");
    }
    
    free(xml);
    
    printf("bb_db_init: Loaded %d signatures (%d viruses, %d X-Copy)\n",
           signature_count, virus_count_cache, xcopy_count_cache);
    
    return 0;
}

/*============================================================================*
 * DETECTION
 *============================================================================*/

int bb_detect(const uint8_t bootblock[BOOTBLOCK_SIZE], bb_detection_result_t *result)
{
    if (!bootblock || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    // Compute CRC32
    result->computed_crc = bb_crc32(bootblock);
    
    // Extract DOS type
    result->dos_type = bootblock[3];
    
    // Verify checksum
    bool checksum_valid;
    bb_verify_checksum((uint8_t*)bootblock, &checksum_valid, false);
    result->checksum_valid = checksum_valid;
    result->checksum = ((uint32_t)bootblock[4] << 24) |
                      ((uint32_t)bootblock[5] << 16) |
                      ((uint32_t)bootblock[6] << 8) |
                      ((uint32_t)bootblock[7]);
    
    // Search signatures
    for (int i = 0; i < signature_count; i++) {
        bb_signature_t *sig = &signatures[i];
        
        bool matched = false;
        
        // Try pattern match first (fast!)
        if (sig->pattern.count > 0 && pattern_matches(bootblock, &sig->pattern)) {
            matched = true;
            result->matched_by_pattern = true;
        }
        
        // Try CRC match (exact)
        if (!matched && sig->crc32 != 0 && sig->crc32 == result->computed_crc) {
            matched = true;
            result->matched_by_crc = true;
        }
        
        if (matched) {
            result->detected = true;
            memcpy(&result->signature, sig, sizeof(bb_signature_t));
            return 0;
        }
    }
    
    return 0; // Not found
}

/*============================================================================*
 * STATISTICS
 *============================================================================*/

void bb_stats_init(bb_scan_stats_t *stats)
{
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));
}

void bb_stats_add(bb_scan_stats_t *stats, const bb_detection_result_t *result)
{
    if (!stats || !result) return;
    
    stats->total_disks++;
    
    if (result->detected) {
        stats->detected_count++;
        
        if (bb_is_virus(result->signature.category)) {
            stats->virus_count++;
        }
        
        if (result->signature.category == BB_CAT_XCOPY) {
            stats->xcopy_count++;
        }
        
        if (result->signature.category == BB_CAT_DEMOSCENE ||
            result->signature.category == BB_CAT_INTRO ||
            result->signature.category == BB_CAT_SCENE) {
            stats->demoscene_count++;
        }
    } else {
        stats->unknown_count++;
    }
}

void bb_stats_print(const bb_scan_stats_t *stats)
{
    if (!stats) return;
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  BOOTBLOCK SCAN STATISTICS\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Total disks scanned:  %u\n", stats->total_disks);
    printf("  Detected bootblocks:  %u\n", stats->detected_count);
    printf("  Viruses found:        %u ⚠️\n", stats->virus_count);
    printf("  X-Copy bootblocks:    %u\n", stats->xcopy_count);
    printf("  Demoscene intros:     %u\n", stats->demoscene_count);
    printf("  Unknown bootblocks:   %u\n", stats->unknown_count);
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
}

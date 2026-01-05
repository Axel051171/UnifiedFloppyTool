/**
 * @file fuzz_forensic.c
 * @brief Fuzz Targets for Forensic APIs
 * 
 * Build: clang -fsanitize=fuzzer,address -g fuzz_forensic.c -o fuzz_forensic
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "uft/forensic/uft_protection.h"
#include "uft/forensic/uft_recovery.h"
#include "uft/forensic/uft_xcopy.h"

// ============================================================================
// FUZZ: Protection Detection
// ============================================================================

#ifdef FUZZ_PROTECTION_DETECT

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0) return 0;
    
    uft_protection_context_t ctx;
    uft_protection_context_init(&ctx);
    ctx.data = data;
    ctx.data_size = size;
    ctx.hint_platform = data[0] % 12;  // Random platform
    
    uft_protection_result_t result;
    uft_protection_detect(&ctx, &result);
    uft_protection_result_free(&result);
    
    return 0;
}

#endif

// ============================================================================
// FUZZ: Weak Bit Analysis
// ============================================================================

#ifdef FUZZ_WEAK_BITS

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 6) return 0;  // Need at least 2 bytes per rev, 3 revs
    
    size_t rev_size = size / 3;
    const uint8_t* revs[3] = {data, data + rev_size, data + 2 * rev_size};
    size_t sizes[3] = {rev_size, rev_size, rev_size};
    
    uint8_t* weak_map = malloc(rev_size);
    if (!weak_map) return 0;
    
    size_t weak_count;
    uft_protection_analyze_weak_bits(revs, sizes, 3, weak_map, &weak_count);
    
    free(weak_map);
    return 0;
}

#endif

// ============================================================================
// FUZZ: BAM Analysis
// ============================================================================

#ifdef FUZZ_BAM_ANALYZE

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 256) return 0;  // Minimum sector size
    
    uft_bam_analysis_t analysis;
    
    // Try as D64
    if (size >= 174848) {
        uft_recovery_bam_analyze(data, size, 0x0100, &analysis);
        uft_recovery_bam_analysis_free(&analysis);
    }
    
    return 0;
}

#endif

// ============================================================================
// FUZZ: Directory Analysis
// ============================================================================

#ifdef FUZZ_DIR_ANALYZE

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 256) return 0;
    
    uft_directory_analysis_t analysis;
    
    // Try as D64
    if (size >= 174848) {
        if (uft_recovery_dir_analyze(data, size, 0x0100, &analysis) == 0) {
            uft_recovery_dir_analysis_free(&analysis);
        }
    }
    
    return 0;
}

#endif

// ============================================================================
// FUZZ: XCopy Profile Parse
// ============================================================================

#ifdef FUZZ_XCOPY_PARSE

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0 || size > 1024) return 0;
    
    // Null-terminate the input
    char* str = malloc(size + 1);
    if (!str) return 0;
    
    memcpy(str, data, size);
    str[size] = 0;
    
    uft_copy_profile_t profile;
    uft_xcopy_profile_parse(str, &profile);
    uft_xcopy_profile_free(&profile);
    
    free(str);
    return 0;
}

#endif

// ============================================================================
// FUZZ: Fingerprint Match
// ============================================================================

#ifdef FUZZ_FINGERPRINT

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 10) return 0;
    
    uft_prot_scheme_t schemes[8];
    double confidences[8];
    
    int track = data[0] % 85;  // Valid track range
    
    uft_protection_match_fingerprint(data + 1, size - 1, track,
                                     schemes, confidences, 8);
    
    return 0;
}

#endif

// ============================================================================
// FUZZ: Sync Analysis
// ============================================================================

#ifdef FUZZ_SYNC_ANALYZE

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 100) return 0;
    
    uft_protection_result_t result;
    memset(&result, 0, sizeof(result));
    
    uft_platform_t platform = data[0] % 3;  // CBM, Amiga, Apple
    
    uft_protection_analyze_sync(data + 1, size - 1, platform, &result);
    
    return 0;
}

#endif

// ============================================================================
// FUZZ: Complete Detection Pipeline
// ============================================================================

#ifdef FUZZ_FULL_PIPELINE

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 100) return 0;
    
    // Create multiple "revolutions" from input
    size_t rev_size = size / 5;
    if (rev_size < 10) return 0;
    
    const uint8_t* revs[5];
    size_t sizes[5];
    for (int i = 0; i < 5; i++) {
        revs[i] = data + i * rev_size;
        sizes[i] = rev_size;
    }
    
    uft_protection_context_t ctx;
    uft_protection_context_init(&ctx);
    ctx.data = data;
    ctx.data_size = size;
    ctx.revolutions = revs;
    ctx.rev_sizes = sizes;
    ctx.rev_count = 5;
    ctx.hint_platform = data[0] % 12;
    ctx.deep_scan = (data[1] & 1);
    ctx.detect_weak_bits = (data[1] & 2);
    ctx.detect_sync = (data[1] & 4);
    ctx.detect_halftrack = (data[1] & 8);
    
    uft_protection_result_t result;
    if (uft_protection_detect(&ctx, &result) == 0) {
        // Exercise result accessors
        uft_protection_has_technique(&result, UFT_PROT_TECH_WEAK_BITS);
        uft_protection_has_technique(&result, UFT_PROT_TECH_SYNC_ANOMALY);
        uft_protection_result_free(&result);
    }
    
    return 0;
}

#endif

// ============================================================================
// STANDALONE TEST (not fuzz)
// ============================================================================

#ifndef FUZZ_PROTECTION_DETECT
#ifndef FUZZ_WEAK_BITS
#ifndef FUZZ_BAM_ANALYZE
#ifndef FUZZ_DIR_ANALYZE
#ifndef FUZZ_XCOPY_PARSE
#ifndef FUZZ_FINGERPRINT
#ifndef FUZZ_SYNC_ANALYZE
#ifndef FUZZ_FULL_PIPELINE

#include <stdio.h>

int main(void) {
    printf("Fuzz targets available:\n");
    printf("  -DFUZZ_PROTECTION_DETECT\n");
    printf("  -DFUZZ_WEAK_BITS\n");
    printf("  -DFUZZ_BAM_ANALYZE\n");
    printf("  -DFUZZ_DIR_ANALYZE\n");
    printf("  -DFUZZ_XCOPY_PARSE\n");
    printf("  -DFUZZ_FINGERPRINT\n");
    printf("  -DFUZZ_SYNC_ANALYZE\n");
    printf("  -DFUZZ_FULL_PIPELINE\n");
    printf("\nBuild with: clang -fsanitize=fuzzer,address -DFUZZ_<target> ...\n");
    return 0;
}

#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif

/**
 * @file examples.c
 * @brief UFT Example Programs Collection
 * 
 * P3-002: Comprehensive examples for all major UFT features
 * 
 * Compile with:
 *   gcc -o examples examples.c -luft -I/usr/include/uft
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* UFT headers */
#include "uft/uft_public_api.h"
#include "uft/core/uft_unified_types.h"
#include "uft/core/uft_fusion.h"
#include "uft/formats/uft_adf_pipeline.h"

/* ============================================================================
 * Example 1: Basic Disk Reading
 * ============================================================================ */

/**
 * @brief Read and display basic disk information
 */
void example_read_disk(const char *path) {
    printf("\n=== Example 1: Read Disk ===\n");
    
    /* Initialize UFT */
    uft_init();
    
    /* Detect format */
    uft_format_info_t info;
    uft_error_t err = uft_detect_format_file(path, &info);
    
    if (err != UFT_OK) {
        printf("Error detecting format: %s\n", uft_error_str(err));
        uft_cleanup();
        return;
    }
    
    printf("File: %s\n", path);
    printf("Format: %s\n", info.name);
    printf("Confidence: %d%%\n", info.confidence);
    
    /* Read disk */
    uft_disk_image_t *disk = NULL;
    err = uft_read_disk(path, &disk, NULL);
    
    if (err != UFT_OK) {
        printf("Error reading disk: %s\n", uft_error_str(err));
        uft_cleanup();
        return;
    }
    
    /* Display info */
    printf("Tracks: %d\n", disk->tracks);
    printf("Heads: %d\n", disk->heads);
    printf("Sectors/Track: %d\n", disk->sectors_per_track);
    printf("Bytes/Sector: %d\n", disk->bytes_per_sector);
    
    /* Cleanup */
    uft_disk_free(disk);
    uft_cleanup();
}

/* ============================================================================
 * Example 2: Format Conversion
 * ============================================================================ */

/**
 * @brief Convert disk image to another format
 */
void example_convert(const char *input, const char *output) {
    printf("\n=== Example 2: Format Conversion ===\n");
    
    uft_init();
    
    /* Setup options */
    uft_convert_options_t opts;
    uft_convert_options_init(&opts);
    opts.preserve_errors = true;
    opts.preserve_timing = true;
    
    /* Progress callback */
    opts.progress_callback = NULL;  /* Could set a callback here */
    
    /* Convert */
    printf("Converting %s -> %s\n", input, output);
    
    uft_error_t err = uft_convert(input, output, &opts);
    
    if (err == UFT_OK) {
        printf("Conversion successful!\n");
    } else {
        printf("Conversion failed: %s\n", uft_error_str(err));
    }
    
    uft_cleanup();
}

/* ============================================================================
 * Example 3: Sector-Level Access
 * ============================================================================ */

/**
 * @brief Read and display individual sectors
 */
void example_sector_access(const char *path) {
    printf("\n=== Example 3: Sector Access ===\n");
    
    uft_init();
    
    uft_disk_image_t *disk = NULL;
    uft_error_t err = uft_read_disk(path, &disk, NULL);
    
    if (err != UFT_OK || !disk) {
        printf("Error: %s\n", uft_error_str(err));
        uft_cleanup();
        return;
    }
    
    /* Read track 1, sector 0 */
    uint8_t buffer[512];
    size_t bytes_read;
    
    err = uft_read_sector(disk, 1, 0, 0, buffer, sizeof(buffer), &bytes_read);
    
    if (err == UFT_OK) {
        printf("Read %zu bytes from track 1, sector 0:\n", bytes_read);
        
        /* Hex dump first 64 bytes */
        for (int i = 0; i < 64 && i < (int)bytes_read; i++) {
            printf("%02X ", buffer[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        printf("\n");
    } else {
        printf("Error reading sector: %s\n", uft_error_str(err));
    }
    
    uft_disk_free(disk);
    uft_cleanup();
}

/* ============================================================================
 * Example 4: Disk Analysis
 * ============================================================================ */

/**
 * @brief Analyze disk quality and detect issues
 */
void example_analyze(const char *path) {
    printf("\n=== Example 4: Disk Analysis ===\n");
    
    uft_init();
    
    uft_disk_image_t *disk = NULL;
    uft_error_t err = uft_read_disk(path, &disk, NULL);
    
    if (err != UFT_OK || !disk) {
        printf("Error: %s\n", uft_error_str(err));
        uft_cleanup();
        return;
    }
    
    /* Analyze */
    uft_analysis_result_t result;
    err = uft_analyze_disk(disk, &result);
    
    if (err == UFT_OK) {
        printf("Analysis Results:\n");
        printf("  Quality: %.1f%%\n", result.quality_percent);
        printf("  Total sectors: %d\n", result.total_sectors);
        printf("  Good sectors: %d\n", result.good_sectors);
        printf("  Bad sectors: %d\n", result.bad_sectors);
        printf("  CRC errors: %d\n", result.crc_errors);
        printf("  Missing sectors: %d\n", result.missing_sectors);
        printf("  Protected: %s\n", result.has_protection ? "Yes" : "No");
        
        if (result.has_protection) {
            printf("  Protection type: %s\n", result.protection_name);
        }
    }
    
    uft_disk_free(disk);
    uft_cleanup();
}

/* ============================================================================
 * Example 5: Multi-Revision Fusion
 * ============================================================================ */

/**
 * @brief Combine multiple reads for data recovery
 */
void example_fusion(void) {
    printf("\n=== Example 5: Multi-Revision Fusion ===\n");
    
    /* Simulate 3 reads with some differences */
    uint8_t rev1[4] = {0xAA, 0x55, 0xAA, 0x55};
    uint8_t rev2[4] = {0xAA, 0x55, 0xAA, 0x55};
    uint8_t rev3[4] = {0xAA, 0xFF, 0xAA, 0x55};  /* Byte 1 differs */
    
    uft_revision_input_t revisions[3] = {
        { .data = rev1, .bit_count = 32, .quality = 100, .crc_valid = true },
        { .data = rev2, .bit_count = 32, .quality = 100, .crc_valid = true },
        { .data = rev3, .bit_count = 32, .quality = 80, .crc_valid = false }
    };
    
    uint8_t output[4];
    size_t out_bits;
    uft_fusion_result_t result;
    
    uft_fusion_options_t opts;
    uft_fusion_options_init(&opts);
    
    uft_error_t err = uft_fusion_merge(revisions, 3, output, &out_bits,
                                       NULL, NULL, &opts, &result);
    
    if (err == UFT_OK) {
        printf("Fusion result:\n");
        printf("  Output: %02X %02X %02X %02X\n", 
               output[0], output[1], output[2], output[3]);
        printf("  Bits: %zu\n", out_bits);
        printf("  Success: %s\n", result.success ? "Yes" : "No");
        printf("  Confidence: %d%%\n", result.confidence);
        printf("  Weak bits: %d\n", result.weak_bit_count);
    }
}

/* ============================================================================
 * Example 6: ADF Pipeline Processing
 * ============================================================================ */

/**
 * @brief Process Amiga ADF with full pipeline
 */
void example_adf_pipeline(const char *path) {
    printf("\n=== Example 6: ADF Pipeline ===\n");
    
    adf_pipeline_ctx_t ctx;
    adf_pipeline_init(&ctx);
    
    /* Configure options */
    adf_pipeline_options_t opts;
    adf_pipeline_options_init(&opts);
    opts.analyze_checksums = true;
    opts.detect_weak_bits = true;
    
    /* Process stages */
    printf("Stage 1: Reading...\n");
    uft_error_t err = adf_pipeline_read_file(&ctx, path);
    if (err != UFT_OK) {
        printf("Read failed: %s\n", uft_error_str(err));
        adf_pipeline_free(&ctx);
        return;
    }
    
    printf("Stage 2: Analyzing...\n");
    err = adf_pipeline_analyze(&ctx, &opts);
    if (err != UFT_OK) {
        printf("Analysis failed\n");
    }
    
    printf("Stage 3: Results...\n");
    printf("  Filesystem: %s\n", adf_filesystem_name(ctx.filesystem_type));
    printf("  Boot block valid: %s\n", ctx.boot_valid ? "Yes" : "No");
    printf("  Bad sectors: %d\n", ctx.bad_sector_count);
    
    adf_pipeline_free(&ctx);
}

/* ============================================================================
 * Example 7: CRC Calculation
 * ============================================================================ */

/**
 * @brief Calculate various CRCs
 */
void example_crc(void) {
    printf("\n=== Example 7: CRC Calculation ===\n");
    
    uint8_t data[] = "Hello, UFT!";
    size_t len = strlen((char*)data);
    
    printf("Data: \"%s\"\n", data);
    printf("CRC-16 CCITT: 0x%04X\n", uft_crc16_ccitt(data, len));
    printf("CRC-16 IBM:   0x%04X\n", uft_crc16_ibm(data, len));
    printf("CRC-32:       0x%08X\n", uft_crc32(data, len));
}

/* ============================================================================
 * Example 8: GCR Encoding
 * ============================================================================ */

/**
 * @brief Encode and decode GCR data
 */
void example_gcr(void) {
    printf("\n=== Example 8: GCR Encoding ===\n");
    
    /* Original data */
    uint8_t original[4] = {0x12, 0x34, 0x56, 0x78};
    uint8_t gcr[5];
    uint8_t decoded[4];
    
    /* Encode */
    gcr_encode_c64_4to5(original, gcr);
    
    printf("Original: %02X %02X %02X %02X\n",
           original[0], original[1], original[2], original[3]);
    printf("GCR:      %02X %02X %02X %02X %02X\n",
           gcr[0], gcr[1], gcr[2], gcr[3], gcr[4]);
    
    /* Decode */
    if (gcr_decode_c64_5to4(gcr, decoded) == 0) {
        printf("Decoded:  %02X %02X %02X %02X\n",
               decoded[0], decoded[1], decoded[2], decoded[3]);
        printf("Match:    %s\n", 
               memcmp(original, decoded, 4) == 0 ? "Yes" : "No");
    } else {
        printf("Decode error!\n");
    }
}

/* ============================================================================
 * Example 9: Protection Detection
 * ============================================================================ */

/**
 * @brief Detect copy protection on a disk
 */
void example_protection(const char *path) {
    printf("\n=== Example 9: Protection Detection ===\n");
    
    uft_init();
    
    uft_disk_image_t *disk = NULL;
    uft_error_t err = uft_read_disk(path, &disk, NULL);
    
    if (err != UFT_OK || !disk) {
        printf("Error: %s\n", uft_error_str(err));
        uft_cleanup();
        return;
    }
    
    /* Detect all protections */
    protection_detection_result_t results[16];
    int count = detect_all_protections(disk, results, 16);
    
    printf("Detected %d protection scheme(s):\n", count);
    
    for (int i = 0; i < count; i++) {
        printf("  [%d] %s (confidence: %d%%)\n",
               i + 1, results[i].name, results[i].confidence);
        printf("      Tracks: %d-%d\n", 
               results[i].track_start, results[i].track_end);
        
        protection_copy_strategy_t s = get_copy_strategy(results[i].type);
        printf("      Copy requires: %s%s%s\n",
               s.use_flux_copy ? "Flux " : "",
               s.preserve_timing ? "Timing " : "",
               s.preserve_weak_bits ? "WeakBits" : "");
    }
    
    uft_disk_free(disk);
    uft_cleanup();
}

/* ============================================================================
 * Example 10: Parameter Validation
 * ============================================================================ */

/**
 * @brief Validate parameters before operation
 */
void example_param_validation(void) {
    printf("\n=== Example 10: Parameter Validation ===\n");
    
    param_set_t params;
    param_set_init(&params);
    
    /* Set some parameters */
    param_set_value_int(&params, "track_start", 0);
    param_set_value_int(&params, "track_end", 79);
    param_set_value_int(&params, "bitrate", 250000);
    param_set_value_int(&params, "density", 0);  /* DD */
    
    /* Create validator */
    param_validator_t v;
    param_validator_init(&v);
    param_validator_load_format_rules(&v, "MFM");
    
    /* Validate */
    param_validation_result_t result = param_validator_validate(&v, &params);
    
    printf("Validation result: %s\n", result.valid ? "VALID" : "INVALID");
    printf("  Errors: %d\n", result.error_count);
    printf("  Warnings: %d\n", result.warning_count);
    
    for (int i = 0; i < result.conflict_count; i++) {
        char buf[256];
        param_conflict_to_string(&result.conflicts[i], buf, sizeof(buf));
        printf("  %s\n", buf);
    }
    
    param_validator_free(&v);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char *argv[]) {
    printf("UnifiedFloppyTool Examples\n");
    printf("==========================\n");
    
    /* Examples that don't need files */
    example_fusion();
    example_crc();
    example_gcr();
    example_param_validation();
    
    /* Examples that need a file */
    if (argc > 1) {
        const char *path = argv[1];
        
        example_read_disk(path);
        example_sector_access(path);
        example_analyze(path);
        
        /* Format-specific examples */
        if (strstr(path, ".adf") || strstr(path, ".ADF")) {
            example_adf_pipeline(path);
        }
        
        /* Protection detection for C64/Amiga formats */
        if (strstr(path, ".d64") || strstr(path, ".g64") ||
            strstr(path, ".adf")) {
            example_protection(path);
        }
        
        /* Conversion example */
        if (argc > 2) {
            example_convert(argv[1], argv[2]);
        }
    } else {
        printf("\nTo run file-based examples:\n");
        printf("  %s <disk_image> [output_file]\n", argv[0]);
    }
    
    printf("\n=== All examples complete ===\n");
    return 0;
}

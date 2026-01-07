/**
 * @file fuzz_phase2.c
 * @brief Fuzz targets for Phase 2 algorithms
 * 
 * Build each target separately with:
 * clang -g -fsanitize=fuzzer,address -DFUZZ_TARGET_XXX fuzz_phase2.c \
 *       <algorithm_source>.c -lm -o fuzz_xxx
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Fuzz Target: CRC Alignment
 * Build with -DFUZZ_TARGET_CRC
 * ============================================================================ */
#ifdef FUZZ_TARGET_CRC

#include "../algorithms/crc/uft_crc_aligned.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) return 0;
    
    /* Test basic CRC calculation */
    uint16_t crc = uft_crc16_calc(data, size);
    (void)crc;
    
    /* Test bit-level CRC */
    uft_crc16_ctx_t ctx;
    uft_crc16_init_ccitt(&ctx);
    
    for (size_t i = 0; i < size; i++) {
        uft_crc16_byte(&ctx, data[i]);
    }
    uint16_t final = uft_crc16_final(&ctx);
    
    /* Both methods should match */
    if (crc != final) {
        abort();  /* Bug: inconsistent CRC */
    }
    
    /* Test alignment search */
    if (size >= 8) {
        uint16_t expected = ((uint16_t)data[0] << 8) | data[1];
        uft_crc_alignment_t align = uft_crc16_find_alignment(
            data + 2, size - 2, expected, 7);
        
        if (align.found && align.confidence > 100) {
            abort();  /* Bug: confidence > 100 */
        }
    }
    
    return 0;
}

#endif /* FUZZ_TARGET_CRC */

/* ============================================================================
 * Fuzz Target: Track Boundary
 * Build with -DFUZZ_TARGET_BOUNDARY
 * ============================================================================ */
#ifdef FUZZ_TARGET_BOUNDARY

#include "../algorithms/track/uft_track_boundary.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 32) return 0;
    
    uft_boundary_config_t cfg;
    uft_boundary_config_init(&cfg);
    
    /* Vary config from fuzz data */
    cfg.tolerance = 0.1 + (data[0] / 255.0) * 0.3;
    cfg.min_match_score = 0.7 + (data[1] / 255.0) * 0.25;
    
    /* Detect boundary */
    uft_track_boundary_t boundary = uft_boundary_detect(
        data + 2, (size - 2) * 8, NULL, 0, &cfg);
    
    /* Invariants */
    if (boundary.end_bit < boundary.start_bit) {
        abort();  /* Bug: invalid range */
    }
    if (boundary.boundary_confidence > 100) {
        abort();  /* Bug: confidence > 100 */
    }
    if (boundary.match_score < 0 || boundary.match_score > 1.0) {
        abort();  /* Bug: invalid match score */
    }
    
    return 0;
}

#endif /* FUZZ_TARGET_BOUNDARY */

/* ============================================================================
 * Fuzz Target: Encoding Detection
 * Build with -DFUZZ_TARGET_ENCODING
 * ============================================================================ */
#ifdef FUZZ_TARGET_ENCODING

#include "../algorithms/encoding/uft_encoding_detect.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 16) return 0;
    
    uft_encoding_candidates_t candidates;
    uft_encoding_detect_all(data, size, 4e6, &candidates);
    
    /* Invariants */
    if (candidates.best && candidates.count == 0) {
        abort();  /* Bug: best without candidates */
    }
    
    for (size_t i = 0; i < candidates.count; i++) {
        if (candidates.results[i].score < 0) {
            abort();  /* Bug: negative score */
        }
    }
    
    /* Test histogram */
    uft_pulse_histogram_t hist;
    uft_encoding_build_histogram(data, size * 8, &hist);
    uft_encoding_find_peaks(&hist);
    
    if (hist.peak_count > 8) {
        abort();  /* Bug: too many peaks */
    }
    
    return 0;
}

#endif /* FUZZ_TARGET_ENCODING */

/* ============================================================================
 * Fuzz Target: Partial Recovery
 * Build with -DFUZZ_TARGET_RECOVERY
 * ============================================================================ */
#ifdef FUZZ_TARGET_RECOVERY

#include "../algorithms/recovery/uft_partial_recovery.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 32) return 0;
    
    size_t sector_size = 128 << (data[0] & 3);  /* 128-1024 */
    if (sector_size > size - 4) sector_size = size - 4;
    
    uft_partial_sector_t sector;
    if (uft_partial_init(&sector, sector_size) != 0) {
        return 0;  /* OOM acceptable */
    }
    
    /* Add revisions */
    int num_revs = (data[1] & 7) + 1;  /* 1-8 revisions */
    
    for (int r = 0; r < num_revs && (size_t)(4 + r * sector_size) < size; r++) {
        uft_partial_add_revision(&sector,
                                 data + 4 + r * sector_size,
                                 NULL,
                                 sector_size,
                                 0, 0);
    }
    
    /* Fuse */
    uft_partial_fuse(&sector);
    
    /* Invariants */
    if (sector.valid_bytes + sector.error_bytes > sector.data_len) {
        abort();  /* Bug: byte counts overflow */
    }
    if (sector.revision_count > UFT_MAX_REVISIONS) {
        abort();  /* Bug: too many revisions */
    }
    
    double rate = uft_partial_get_recovery_rate(&sector);
    if (rate < 0 || rate > 1.0) {
        abort();  /* Bug: invalid rate */
    }
    
    uft_partial_free(&sector);
    return 0;
}

#endif /* FUZZ_TARGET_RECOVERY */

/* ============================================================================
 * Fuzz Target: Format Detection
 * Build with -DFUZZ_TARGET_FORMAT
 * ============================================================================ */
#ifdef FUZZ_TARGET_FORMAT

#include "../algorithms/format/uft_format_detect.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return 0;
    
    uft_format_registry_t reg;
    uft_format_registry_init(&reg);
    
    uft_format_candidates_t candidates;
    uft_format_detect_all(&reg, data, size, size, "test.img", &candidates);
    
    /* Invariants */
    if (candidates.count > 16) {
        abort();  /* Bug: too many candidates */
    }
    
    for (size_t i = 0; i < candidates.count; i++) {
        if (candidates.results[i].confidence > 100) {
            abort();  /* Bug: confidence > 100 */
        }
        if (!candidates.results[i].name) {
            abort();  /* Bug: NULL name */
        }
    }
    
    /* Test with various "filenames" */
    const char *exts[] = {".d64", ".adf", ".hfe", ".img", ".scp", NULL};
    for (int i = 0; exts[i]; i++) {
        char filename[32];
        snprintf(filename, sizeof(filename), "test%s", exts[i]);
        
        uft_format_info_t info = uft_format_detect(&reg, data, size, size, filename);
        (void)info;
    }
    
    return 0;
}

#endif /* FUZZ_TARGET_FORMAT */

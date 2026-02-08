/**
 * @file uft_confidence.c
 * @brief Confidence Detection Implementation
 */

#include "uft/detection/uft_confidence.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// CONFIDENCE LEVEL NAMES
// ============================================================================

static const char* level_names[] = {
    "UNKNOWN",
    "GUESS",
    "POSSIBLE",
    "LIKELY",
    "CERTAIN",
    "VERIFIED"
};

const char* uft_confidence_name(uft_confidence_level_t level) {
    if (level < 0 || level > UFT_CONF_VERIFIED) return "INVALID";
    return level_names[level];
}

// ============================================================================
// SCORE TO LEVEL CONVERSION
// ============================================================================

uft_confidence_level_t uft_score_to_level(double score) {
    if (score <= 0.0) return UFT_CONF_UNKNOWN;
    if (score < 0.4) return UFT_CONF_GUESS;
    if (score < 0.6) return UFT_CONF_POSSIBLE;
    if (score < 0.85) return UFT_CONF_LIKELY;
    if (score < 0.99) return UFT_CONF_CERTAIN;
    return UFT_CONF_VERIFIED;
}

// ============================================================================
// EVIDENCE NAMES
// ============================================================================

const char* uft_evidence_name(uint32_t evidence_flag) {
    switch (evidence_flag) {
        case UFT_EVIDENCE_MAGIC:         return "Magic bytes";
        case UFT_EVIDENCE_SIZE:          return "File size";
        case UFT_EVIDENCE_EXTENSION:     return "Extension";
        case UFT_EVIDENCE_HEADER:        return "Header structure";
        case UFT_EVIDENCE_CHECKSUM:      return "Checksum";
        case UFT_EVIDENCE_FILESYSTEM:    return "Filesystem";
        case UFT_EVIDENCE_BOOTBLOCK:     return "Bootblock";
        case UFT_EVIDENCE_SECTOR_LAYOUT: return "Sector layout";
        case UFT_EVIDENCE_ENCODING:      return "Encoding";
        case UFT_EVIDENCE_SIGNATURE:     return "Signature";
        case UFT_EVIDENCE_CONTENT_SCAN:  return "Content scan";
        case UFT_EVIDENCE_PRIOR:         return "Prior probability";
        default: return "Unknown evidence";
    }
}

// ============================================================================
// BUILD REASON STRING
// ============================================================================

void uft_build_reason(uint32_t evidence_flags, char* buf, size_t size) {
    if (!buf || size == 0) return;
    
    buf[0] = '\0';
    size_t pos = 0;
    bool first = true;
    
    static const uint32_t all_flags[] = {
        UFT_EVIDENCE_MAGIC, UFT_EVIDENCE_SIZE, UFT_EVIDENCE_EXTENSION,
        UFT_EVIDENCE_HEADER, UFT_EVIDENCE_CHECKSUM, UFT_EVIDENCE_FILESYSTEM,
        UFT_EVIDENCE_BOOTBLOCK, UFT_EVIDENCE_SECTOR_LAYOUT, UFT_EVIDENCE_ENCODING,
        UFT_EVIDENCE_SIGNATURE, UFT_EVIDENCE_CONTENT_SCAN, UFT_EVIDENCE_PRIOR
    };
    
    for (size_t i = 0; i < sizeof(all_flags)/sizeof(all_flags[0]); i++) {
        if (evidence_flags & all_flags[i]) {
            const char* name = uft_evidence_name(all_flags[i]);
            size_t len = strlen(name);
            
            if (pos + len + 3 < size) {
                if (!first) {
                    buf[pos++] = ',';
                    buf[pos++] = ' ';
                }
                memcpy(buf + pos, name, len + 1);  /* Safe: size checked above */
                pos += len;
                first = false;
            }
        }
    }
}

// ============================================================================
// DEFAULT OPTIONS
// ============================================================================

void uft_detect_options_default(uft_detect_options_t* opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    opts->deep_scan = false;
    opts->check_filesystem = true;
    opts->trust_extension = true;
    opts->check_encoding = true;
    opts->min_confidence = 0.25;
    opts->max_candidates = 8;
    
    // Default priors
    opts->prior_c64 = 0.2;
    opts->prior_amiga = 0.2;
    opts->prior_pc = 0.4;
    opts->prior_apple = 0.1;
}

// ============================================================================
// DETECT FUNCTION (STUB)
// ============================================================================

int uft_detect(const uint8_t* data, size_t size,
               const char* filename,
               const uft_detect_options_t* opts,
               uft_detect_result_t* result) {
    if (!result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->best_index = -1;
    
    if (!data || size == 0) {
        return 0;  // No candidates for empty data
    }
    
    // Copy first bytes for inspection
    size_t copy_size = (size < 64) ? size : 64;
    memcpy(result->first_bytes, data, copy_size);
    result->file_size = size;
    
    if (filename) {
        strncpy(result->filename, filename, sizeof(result->filename) - 1);
    }
    
    /* Format detection via magic bytes, file size, and extension analysis */
    int candidates = 0;
    
    /* Check magic bytes */
    if (size >= 4) {
        /* SCP: "SCP\0" */
        if (data[0] == 'S' && data[1] == 'C' && data[2] == 'P') {
            result->candidates[candidates].format_id = 1;
            result->candidates[candidates].confidence = 95;
            strncpy(result->candidates[candidates].name, "SCP", 31);
            candidates++;
        }
        /* HFE: "HXCPICFE" */
        if (size >= 8 && memcmp(data, "HXCPICFE", 8) == 0) {
            result->candidates[candidates].format_id = 2;
            result->candidates[candidates].confidence = 98;
            strncpy(result->candidates[candidates].name, "HFE", 31);
            candidates++;
        }
        /* IPF: "CAPS" */
        if (memcmp(data, "CAPS", 4) == 0) {
            result->candidates[candidates].format_id = 3;
            result->candidates[candidates].confidence = 97;
            strncpy(result->candidates[candidates].name, "IPF", 31);
            candidates++;
        }
        /* WOZ: "WOZ1"/"WOZ2" */
        if (data[0] == 'W' && data[1] == 'O' && data[2] == 'Z') {
            result->candidates[candidates].format_id = 4;
            result->candidates[candidates].confidence = 96;
            strncpy(result->candidates[candidates].name, "WOZ", 31);
            candidates++;
        }
    }
    
    /* Size-based detection */
    if (candidates == 0) {
        if (size == 174848) {  /* D64 */
            result->candidates[candidates].format_id = 10;
            result->candidates[candidates].confidence = 80;
            strncpy(result->candidates[candidates].name, "D64", 31);
            candidates++;
        } else if (size == 901120) {  /* ADF */
            result->candidates[candidates].format_id = 11;
            result->candidates[candidates].confidence = 75;
            strncpy(result->candidates[candidates].name, "ADF", 31);
            candidates++;
        } else if (size == 737280) {  /* Atari ST */
            result->candidates[candidates].format_id = 12;
            result->candidates[candidates].confidence = 70;
            strncpy(result->candidates[candidates].name, "Atari ST", 31);
            candidates++;
        }
    }
    
    /* Extension-based boost */
    if (filename && candidates > 0) {
        const char *ext = strrchr(filename, '.');
        if (ext) {
            for (int i = 0; i < candidates; i++) {
                if ((strcasecmp(ext, ".scp") == 0 && result->candidates[i].format_id == 1) ||
                    (strcasecmp(ext, ".hfe") == 0 && result->candidates[i].format_id == 2) ||
                    (strcasecmp(ext, ".ipf") == 0 && result->candidates[i].format_id == 3) ||
                    (strcasecmp(ext, ".woz") == 0 && result->candidates[i].format_id == 4)) {
                    result->candidates[i].confidence += 3;
                    if (result->candidates[i].confidence > 100) result->candidates[i].confidence = 100;
                }
            }
        }
    }
    
    /* Find best candidate */
    result->candidate_count = candidates;
    int best_conf = 0;
    for (int i = 0; i < candidates; i++) {
        if (result->candidates[i].confidence > best_conf) {
            best_conf = result->candidates[i].confidence;
            result->best_index = i;
        }
    }
    
    (void)opts;
    
    return 0;
}

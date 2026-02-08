/**
 * @file uft_format_convert.c
 * @brief Format Conversion Matrix Implementation
 */

#include "uft/uft_format_convert.h"
#include "uft/uft_format_probe.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Format Classification Table
// ============================================================================

typedef struct format_info {
    uft_format_t        format;
    const char*         name;
    const char*         extension;
    uft_format_class_t  fclass;
} format_info_t;

static const format_info_t g_format_info[] = {
    // FLUX
    { UFT_FORMAT_SCP,       "SCP",      ".scp",     UFT_FCLASS_FLUX },
    { UFT_FORMAT_KRYOFLUX,  "Kryoflux", ".raw",     UFT_FCLASS_FLUX },
    { UFT_FORMAT_A2R,       "A2R",      ".a2r",     UFT_FCLASS_FLUX },
    
    // BITSTREAM
    { UFT_FORMAT_HFE,       "HFE",      ".hfe",     UFT_FCLASS_BITSTREAM },
    { UFT_FORMAT_G64,       "G64",      ".g64",     UFT_FCLASS_BITSTREAM },
    { UFT_FORMAT_WOZ,       "WOZ",      ".woz",     UFT_FCLASS_BITSTREAM },
    { UFT_FORMAT_NIB,       "NIB",      ".nib",     UFT_FCLASS_BITSTREAM },
    
    // CONTAINER
    { UFT_FORMAT_IPF,       "IPF",      ".ipf",     UFT_FCLASS_CONTAINER },
    { UFT_FORMAT_STX,       "STX",      ".stx",     UFT_FCLASS_CONTAINER },
    
    // SECTOR
    { UFT_FORMAT_D64,       "D64",      ".d64",     UFT_FCLASS_SECTOR },
    { UFT_FORMAT_ADF,       "ADF",      ".adf",     UFT_FCLASS_SECTOR },
    { UFT_FORMAT_IMG,       "IMG",      ".img",     UFT_FCLASS_SECTOR },
    { UFT_FORMAT_DSK,       "DSK",      ".dsk",     UFT_FCLASS_SECTOR },
    { UFT_FORMAT_IMD,       "IMD",      ".imd",     UFT_FCLASS_SECTOR },
    { UFT_FORMAT_FDI,       "FDI",      ".fdi",     UFT_FCLASS_SECTOR },
    
    // ARCHIVE
    { UFT_FORMAT_TD0,       "TD0",      ".td0",     UFT_FCLASS_ARCHIVE },
    { UFT_FORMAT_NBZ,       "NBZ",      ".nbz",     UFT_FCLASS_ARCHIVE },
    
    { UFT_FORMAT_UNKNOWN,   NULL,       NULL,       UFT_FCLASS_SECTOR }
};

// ============================================================================
// Conversion Matrix
// ============================================================================

static const uft_conversion_path_t g_conversion_paths[] = {
    // === FLUX → BITSTREAM (Lossless) ===
    {
        .source = UFT_FORMAT_SCP, .target = UFT_FORMAT_HFE,
        .quality = UFT_CONV_LOSSLESS,
        .preserves_timing = true, .preserves_weak = true,
        .description = "SCP flux to HFE bitstream"
    },
    {
        .source = UFT_FORMAT_SCP, .target = UFT_FORMAT_G64,
        .quality = UFT_CONV_LOSSLESS,
        .preserves_timing = true, .preserves_weak = true,
        .requires_decode = true,
        .description = "SCP flux to G64 (CBM GCR)"
    },
    {
        .source = UFT_FORMAT_KRYOFLUX, .target = UFT_FORMAT_SCP,
        .quality = UFT_CONV_LOSSLESS,
        .preserves_timing = true, .preserves_weak = true,
        .description = "Kryoflux stream to SCP"
    },
    {
        .source = UFT_FORMAT_KRYOFLUX, .target = UFT_FORMAT_HFE,
        .quality = UFT_CONV_LOSSLESS,
        .preserves_timing = true,
        .description = "Kryoflux stream to HFE"
    },
    
    // === FLUX → SECTOR (Lossy - timing lost) ===
    {
        .source = UFT_FORMAT_SCP, .target = UFT_FORMAT_D64,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "Timing and weak bit information will be lost",
        .description = "Decode SCP flux to D64 sectors"
    },
    {
        .source = UFT_FORMAT_SCP, .target = UFT_FORMAT_ADF,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "Timing information will be lost",
        .description = "Decode SCP flux to ADF sectors"
    },
    {
        .source = UFT_FORMAT_SCP, .target = UFT_FORMAT_IMG,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "Timing information will be lost",
        .description = "Decode SCP flux to raw IMG"
    },
    {
        .source = UFT_FORMAT_KRYOFLUX, .target = UFT_FORMAT_D64,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "Timing and weak bit information will be lost",
        .description = "Decode Kryoflux to D64"
    },
    {
        .source = UFT_FORMAT_KRYOFLUX, .target = UFT_FORMAT_ADF,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "Timing information will be lost",
        .description = "Decode Kryoflux to ADF"
    },
    
    // === BITSTREAM → FLUX (Lossless) ===
    {
        .source = UFT_FORMAT_HFE, .target = UFT_FORMAT_SCP,
        .quality = UFT_CONV_LOSSLESS,
        .preserves_timing = true,
        .description = "HFE bitstream to SCP flux"
    },
    {
        .source = UFT_FORMAT_G64, .target = UFT_FORMAT_SCP,
        .quality = UFT_CONV_SYNTHETIC,
        .warning = "Flux timing will be synthesized from GCR data",
        .description = "G64 to SCP (synthetic flux)"
    },
    
    // === BITSTREAM → SECTOR (Lossy) ===
    {
        .source = UFT_FORMAT_G64, .target = UFT_FORMAT_D64,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "GCR encoding and error info will be lost",
        .description = "Decode G64 GCR to D64 sectors"
    },
    {
        .source = UFT_FORMAT_HFE, .target = UFT_FORMAT_IMG,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "Bitstream encoding will be lost",
        .description = "Decode HFE to raw IMG"
    },
    {
        .source = UFT_FORMAT_HFE, .target = UFT_FORMAT_ADF,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .description = "Decode HFE to ADF"
    },
    {
        .source = UFT_FORMAT_WOZ, .target = UFT_FORMAT_DSK,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "Copy protection features will be lost",
        .description = "Decode WOZ to DSK"
    },
    {
        .source = UFT_FORMAT_NIB, .target = UFT_FORMAT_DSK,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .description = "Decode NIB nibbles to DSK sectors"
    },
    
    // === BITSTREAM → BITSTREAM ===
    {
        .source = UFT_FORMAT_G64, .target = UFT_FORMAT_HFE,
        .quality = UFT_CONV_LOSSLESS,
        .preserves_timing = true,
        .description = "G64 to HFE"
    },
    {
        .source = UFT_FORMAT_HFE, .target = UFT_FORMAT_G64,
        .quality = UFT_CONV_LOSSY,
        .warning = "Only CBM-compatible tracks will convert correctly",
        .description = "HFE to G64 (CBM only)"
    },
    {
        .source = UFT_FORMAT_WOZ, .target = UFT_FORMAT_NIB,
        .quality = UFT_CONV_LOSSY,
        .warning = "Timing metadata will be lost",
        .description = "WOZ to NIB"
    },
    
    // === SECTOR → BITSTREAM (Synthetic) ===
    {
        .source = UFT_FORMAT_D64, .target = UFT_FORMAT_G64,
        .quality = UFT_CONV_SYNTHETIC,
        .warning = "GCR encoding will be synthesized (no original timing)",
        .description = "Encode D64 to G64 (synthetic GCR)"
    },
    {
        .source = UFT_FORMAT_ADF, .target = UFT_FORMAT_HFE,
        .quality = UFT_CONV_SYNTHETIC,
        .warning = "MFM encoding will be synthesized",
        .description = "Encode ADF to HFE (synthetic MFM)"
    },
    {
        .source = UFT_FORMAT_IMG, .target = UFT_FORMAT_HFE,
        .quality = UFT_CONV_SYNTHETIC,
        .warning = "MFM encoding will be synthesized",
        .description = "Encode IMG to HFE (synthetic MFM)"
    },
    {
        .source = UFT_FORMAT_DSK, .target = UFT_FORMAT_WOZ,
        .quality = UFT_CONV_SYNTHETIC,
        .warning = "GCR encoding will be synthesized",
        .description = "Encode DSK to WOZ (synthetic)"
    },
    {
        .source = UFT_FORMAT_DSK, .target = UFT_FORMAT_NIB,
        .quality = UFT_CONV_SYNTHETIC,
        .description = "Encode DSK to NIB"
    },
    
    // === SECTOR → FLUX (Synthetic) ===
    {
        .source = UFT_FORMAT_D64, .target = UFT_FORMAT_SCP,
        .quality = UFT_CONV_SYNTHETIC,
        .warning = "Flux will be fully synthesized (not original)",
        .description = "Synthesize D64 to SCP flux"
    },
    {
        .source = UFT_FORMAT_ADF, .target = UFT_FORMAT_SCP,
        .quality = UFT_CONV_SYNTHETIC,
        .warning = "Flux will be fully synthesized",
        .description = "Synthesize ADF to SCP flux"
    },
    {
        .source = UFT_FORMAT_IMG, .target = UFT_FORMAT_SCP,
        .quality = UFT_CONV_SYNTHETIC,
        .warning = "Flux will be fully synthesized",
        .description = "Synthesize IMG to SCP flux"
    },
    
    // === SECTOR → SECTOR ===
    {
        .source = UFT_FORMAT_D64, .target = UFT_FORMAT_D64,
        .quality = UFT_CONV_LOSSLESS,
        .description = "D64 copy/repair"
    },
    {
        .source = UFT_FORMAT_ADF, .target = UFT_FORMAT_ADF,
        .quality = UFT_CONV_LOSSLESS,
        .description = "ADF copy"
    },
    {
        .source = UFT_FORMAT_IMG, .target = UFT_FORMAT_ADF,
        .quality = UFT_CONV_LOSSY,
        .warning = "Layout must match Amiga geometry",
        .description = "Raw IMG to ADF"
    },
    {
        .source = UFT_FORMAT_ADF, .target = UFT_FORMAT_IMG,
        .quality = UFT_CONV_LOSSLESS,
        .description = "ADF to raw IMG"
    },
    {
        .source = UFT_FORMAT_IMD, .target = UFT_FORMAT_IMG,
        .quality = UFT_CONV_LOSSY,
        .warning = "Sector metadata will be lost",
        .description = "IMD to raw IMG"
    },
    {
        .source = UFT_FORMAT_IMG, .target = UFT_FORMAT_IMD,
        .quality = UFT_CONV_LOSSLESS,
        .description = "IMG to IMD with metadata"
    },
    
    // === CONTAINER → SECTOR (Lossy) ===
    {
        .source = UFT_FORMAT_IPF, .target = UFT_FORMAT_ADF,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "Copy protection and timing info will be lost",
        .description = "IPF to ADF (decode protected)"
    },
    {
        .source = UFT_FORMAT_IPF, .target = UFT_FORMAT_IMG,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "Copy protection info will be lost",
        .description = "IPF to raw IMG"
    },
    {
        .source = UFT_FORMAT_STX, .target = UFT_FORMAT_DSK,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "Atari ST protection features will be lost",
        .description = "STX to DSK"
    },
    {
        .source = UFT_FORMAT_STX, .target = UFT_FORMAT_IMG,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .description = "STX to raw IMG"
    },
    
    // === CONTAINER → FLUX (Special) ===
    {
        .source = UFT_FORMAT_IPF, .target = UFT_FORMAT_SCP,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "Some IPF timing hints may not convert perfectly",
        .description = "IPF to SCP flux"
    },
    {
        .source = UFT_FORMAT_STX, .target = UFT_FORMAT_SCP,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .description = "STX to SCP flux"
    },
    
    // === ARCHIVE → SECTOR ===
    {
        .source = UFT_FORMAT_TD0, .target = UFT_FORMAT_IMG,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "Teledisk compression/metadata will be lost",
        .description = "Decompress TD0 to IMG"
    },
    {
        .source = UFT_FORMAT_TD0, .target = UFT_FORMAT_IMD,
        .quality = UFT_CONV_LOSSLESS,
        .requires_decode = true,
        .description = "TD0 to IMD (preserves metadata)"
    },
    {
        .source = UFT_FORMAT_NBZ, .target = UFT_FORMAT_D64,
        .quality = UFT_CONV_LOSSLESS,
        .requires_decode = true,
        .description = "Decompress NBZ to D64"
    },
    {
        .source = UFT_FORMAT_NBZ, .target = UFT_FORMAT_G64,
        .quality = UFT_CONV_LOSSLESS,
        .requires_decode = true,
        .description = "Decompress NBZ to G64"
    },
    
    // Terminator
    { .source = UFT_FORMAT_UNKNOWN, .target = UFT_FORMAT_UNKNOWN }
};

#define NUM_CONVERSION_PATHS \
    (sizeof(g_conversion_paths) / sizeof(g_conversion_paths[0]) - 1)

// ============================================================================
// API Implementation
// ============================================================================

uft_format_class_t uft_format_get_class(uft_format_t format) {
    for (int i = 0; g_format_info[i].name; i++) {
        if (g_format_info[i].format == format) {
            return g_format_info[i].fclass;
        }
    }
    return UFT_FCLASS_SECTOR;
}

const char* uft_format_get_name(uft_format_t format) {
    for (int i = 0; g_format_info[i].name; i++) {
        if (g_format_info[i].format == format) {
            return g_format_info[i].name;
        }
    }
    return "Unknown";
}

const uft_conversion_path_t* uft_convert_get_path(uft_format_t src, 
                                                    uft_format_t dst) {
    for (size_t i = 0; i < NUM_CONVERSION_PATHS; i++) {
        if (g_conversion_paths[i].source == src &&
            g_conversion_paths[i].target == dst) {
            return &g_conversion_paths[i];
        }
    }
    return NULL;
}

bool uft_convert_can(uft_format_t src, uft_format_t dst,
                      uft_conv_quality_t* quality,
                      const char** warning) {
    const uft_conversion_path_t* path = uft_convert_get_path(src, dst);
    
    if (!path) {
        if (quality) *quality = UFT_CONV_IMPOSSIBLE;
        if (warning) *warning = "No conversion path available";
        return false;
    }
    
    if (quality) *quality = path->quality;
    if (warning) *warning = path->warning;
    return true;
}

int uft_convert_list_targets(uft_format_t src,
                              const uft_conversion_path_t** paths,
                              int max) {
    int count = 0;
    
    for (size_t i = 0; i < NUM_CONVERSION_PATHS && count < max; i++) {
        if (g_conversion_paths[i].source == src) {
            if (paths) paths[count] = &g_conversion_paths[i];
            count++;
        }
    }
    
    return count;
}

uft_convert_options_t uft_convert_default_options(void) {
    return (uft_convert_options_t) {
        .verify_after = true,
        .preserve_errors = true,
        .preserve_weak_bits = true,
        .synthetic_cell_time_us = 2.0,
        .synthetic_jitter_percent = 5.0,
        .synthetic_revolutions = 3,
        .decode_retries = 5,
        .use_multiple_revs = true,
        .interpolate_errors = true,
    };
}

// ============================================================================
// Actual Conversion (Stubs - full implementation in separate files)
// ============================================================================

uft_error_t uft_convert_file(const char* src_path,
                              const char* dst_path,
                              uft_format_t dst_format,
                              const uft_convert_options_t* options,
                              uft_convert_result_t* result) {
    if (!src_path || !dst_path || !result) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    memset(result, 0, sizeof(*result));
    
    // Read source file
    FILE* f = fopen(src_path, "rb");
    if (!f) {
        result->error = UFT_ERROR_NOT_FOUND;
        return UFT_ERROR_NOT_FOUND;
    }
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t src_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    uint8_t* src_data = malloc(src_size);
    if (!src_data) {
        fclose(f);
        result->error = UFT_ERROR_NO_MEMORY;
        return UFT_ERROR_NO_MEMORY;
    }
    
    if (fread(src_data, 1, src_size, f) != src_size) { /* I/O error */ }
    fclose(f);
    
    // Detect source format
    uft_probe_result_t probe;
    uft_format_t src_format = uft_probe_format(src_data, src_size, src_path, &probe);
    
    if (src_format == UFT_FORMAT_UNKNOWN) {
        free(src_data);
        result->error = UFT_ERROR_INVALID_FORMAT;
        snprintf(result->warnings[0], sizeof(result->warnings[0]),
                 "Could not detect source format");
        result->warning_count = 1;
        return UFT_ERROR_INVALID_FORMAT;
    }
    
    // Check conversion path
    const uft_conversion_path_t* path = uft_convert_get_path(src_format, dst_format);
    if (!path) {
        free(src_data);
        result->error = UFT_ERROR_FORMAT_NOT_SUPPORTED;
        snprintf(result->warnings[0], sizeof(result->warnings[0]),
                 "No conversion path from %s to %s",
                 uft_format_get_name(src_format),
                 uft_format_get_name(dst_format));
        result->warning_count = 1;
        return UFT_ERROR_FORMAT_NOT_SUPPORTED;
    }
    
    // Add quality warning
    if (path->warning) {
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]), "%s", path->warning);
    }
    
    // Conversion dispatch based on format class hierarchy
    uft_format_class_t src_class = uft_format_get_class(src_format);
    uft_format_class_t dst_class = uft_format_get_class(dst_format);
    
    if (src_format == dst_format) {
        // Same format: direct copy
        FILE* out = fopen(dst_path, "wb");
        if (!out) { free(src_data); return UFT_ERR_FILE_OPEN; }
        if (fwrite(src_data, 1, src_size, out) != src_size) {
            fclose(out); free(src_data);
            result->error = UFT_ERROR_IO;
            return UFT_ERROR_IO;
        }
        fclose(out);
        result->success = true;
        result->bytes_written = src_size;
    } else if (src_class == UFT_FCLASS_SECTOR && dst_class == UFT_FCLASS_SECTOR) {
        // Sector → Sector: geometry remapping (IMG↔ADF, IMD↔IMG, etc.)
        FILE* out = fopen(dst_path, "wb");
        if (!out) { free(src_data); return UFT_ERR_FILE_OPEN; }
        // For sector formats, data is raw sectors - write directly
        // Format-specific headers would need per-format handling
        if (fwrite(src_data, 1, src_size, out) != src_size) {
            fclose(out); free(src_data);
            return UFT_ERROR_IO;
        }
        fclose(out);
        result->success = true;
        result->bytes_written = src_size;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Raw sector copy - format headers may need adjustment");
    } else if (src_class == UFT_FCLASS_ARCHIVE && dst_class == UFT_FCLASS_SECTOR) {
        // Archive → Sector: decompression (TD0→IMG, NBZ→D64)
        // Requires format-specific decompressor
        result->error = UFT_ERROR_FORMAT_NOT_SUPPORTED;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Archive decompression for %s→%s requires format-specific decoder",
                 uft_format_get_name(src_format), uft_format_get_name(dst_format));
    } else if (path->requires_decode) {
        // Flux/Bitstream → Sector: requires decoding pipeline
        // This needs the flux decoder + sector extractor chain
        result->error = UFT_ERROR_FORMAT_NOT_SUPPORTED;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Decode conversion %s→%s requires flux/bitstream decode pipeline",
                 uft_format_get_name(src_format), uft_format_get_name(dst_format));
    } else {
        // Other conversions (bitstream↔flux, sector→bitstream synthetic)
        result->error = UFT_ERROR_FORMAT_NOT_SUPPORTED;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Conversion %s→%s: path exists but encoder not yet wired",
                 uft_format_get_name(src_format), uft_format_get_name(dst_format));
    }
    
    free(src_data);
    return result->success ? UFT_OK : result->error;
}

// ============================================================================
// Print Conversion Matrix (Debug/Documentation)
// ============================================================================

void uft_convert_print_matrix(void) {
    printf("=== FORMAT CONVERSION MATRIX ===\n\n");
    
    printf("Format Classes:\n");
    printf("  FLUX:      SCP, Kryoflux, A2R\n");
    printf("  BITSTREAM: HFE, G64, WOZ, NIB\n");
    printf("  CONTAINER: IPF, STX\n");
    printf("  SECTOR:    D64, ADF, IMG, DSK, IMD\n");
    printf("  ARCHIVE:   TD0, NBZ\n\n");
    
    printf("Conversion Paths:\n");
    printf("%-10s %-10s %-12s %s\n", "Source", "Target", "Quality", "Notes");
    printf("%-10s %-10s %-12s %s\n", "------", "------", "-------", "-----");
    
    for (size_t i = 0; i < NUM_CONVERSION_PATHS; i++) {
        const uft_conversion_path_t* p = &g_conversion_paths[i];
        
        const char* quality_str;
        switch (p->quality) {
            case UFT_CONV_LOSSLESS:  quality_str = "LOSSLESS"; break;
            case UFT_CONV_LOSSY:     quality_str = "LOSSY"; break;
            case UFT_CONV_SYNTHETIC: quality_str = "SYNTHETIC"; break;
            default:                 quality_str = "???"; break;
        }
        
        printf("%-10s %-10s %-12s %s\n",
               uft_format_get_name(p->source),
               uft_format_get_name(p->target),
               quality_str,
               p->warning ? p->warning : "");
    }
}

/**
 * @file uft_format_convert_tables.c
 * @brief Format conversion tables and shared helpers.
 *
 * Contains the format info table, conversion path matrix, and
 * shared utility functions used by all converter files.
 */

#include "uft_format_convert_internal.h"

// ============================================================================
// Format Classification Table
// ============================================================================

const format_info_t g_format_info[] = {
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

const uft_conversion_path_t g_conversion_paths[] = {
    // === FLUX -> BITSTREAM (Lossless) ===
    {
        .source = UFT_FORMAT_SCP, .target = UFT_FORMAT_HFE,
        .quality = UFT_CONV_LOSSY,
        .preserves_timing = true, .preserves_weak = false,
        .warning = "HFE writer does not currently preserve weak bit masks. Copy protection data may be lost.",
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
        .quality = UFT_CONV_LOSSY,
        .preserves_timing = true, .preserves_weak = false,
        .warning = "HFE writer does not currently preserve weak bit masks. Copy protection data may be lost.",
        .description = "Kryoflux stream to HFE"
    },

    // === FLUX -> SECTOR (Lossy - timing lost) ===
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
        .warning = "Timing and weak bit/copy protection information will be lost",
        .description = "Decode SCP flux to ADF sectors"
    },
    {
        .source = UFT_FORMAT_SCP, .target = UFT_FORMAT_IMG,
        .quality = UFT_CONV_LOSSY,
        .requires_decode = true,
        .warning = "Timing and weak bit/copy protection information will be lost",
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
        .warning = "Timing and weak bit/copy protection information will be lost",
        .description = "Decode Kryoflux to ADF"
    },

    // === BITSTREAM -> FLUX (Lossless) ===
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

    // === BITSTREAM -> SECTOR (Lossy) ===
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
        .warning = "Bitstream encoding and weak bit/copy protection information will be lost",
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
        .warning = "Nibble encoding and copy protection features will be lost",
        .description = "Decode NIB nibbles to DSK sectors"
    },

    // === BITSTREAM -> BITSTREAM ===
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

    // === SECTOR -> BITSTREAM (Synthetic) ===
    {
        .source = UFT_FORMAT_D64, .target = UFT_FORMAT_G64,
        .quality = UFT_CONV_SYNTHETIC,
        .warning = "GCR encoding will be synthesized (no original timing)",
        .description = "Encode D64 to G64 (synthetic GCR)"
    },
    {
        .source = UFT_FORMAT_D64, .target = UFT_FORMAT_HFE,
        .quality = UFT_CONV_SYNTHETIC,
        .warning = "GCR encoding synthesized via G64 intermediate",
        .description = "D64 to HFE via G64 (for HxC/Gotek)"
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

    // === SECTOR -> FLUX (Synthetic) ===
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

    // === SECTOR -> SECTOR ===
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

    // === CONTAINER -> SECTOR (Lossy) ===
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
        .warning = "Atari ST protection features and timing will be lost",
        .description = "STX to raw IMG"
    },

    // === CONTAINER -> FLUX (Special) ===
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
        .warning = "Some STX protection timing may not convert perfectly",
        .description = "STX to SCP flux"
    },

    // === ARCHIVE -> SECTOR ===
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

const size_t g_num_conversion_paths =
    sizeof(g_conversion_paths) / sizeof(g_conversion_paths[0]) - 1;

// ============================================================================
// Helper: Write output file
// ============================================================================

uft_error_t uftc_write_output_file(const char* path, const uint8_t* data,
                                    size_t size) {
    FILE* out = fopen(path, "wb");
    if (!out) return UFT_ERR_IO;
    if (fwrite(data, 1, size, out) != size) {
        fclose(out);
        return UFT_ERR_IO;
    }
    fclose(out);
    return UFT_OK;
}

// ============================================================================
// Helper: Progress callback
// ============================================================================

void uftc_report_progress(const uft_convert_options_ext_t* opts,
                           int percent, const char* stage) {
    if (opts && opts->progress_cb) {
        opts->progress_cb(percent, stage, opts->progress_user);
    }
}

bool uftc_is_cancelled(const uft_convert_options_ext_t* opts) {
    return opts && opts->cancel && *opts->cancel;
}

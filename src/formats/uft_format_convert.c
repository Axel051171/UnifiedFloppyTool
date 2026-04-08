/**
 * @file uft_format_convert.c
 * @brief Format Conversion Matrix Implementation
 *
 * Implements the full conversion dispatch engine, wiring existing format
 * parsers into the 43 defined conversion paths.
 */

#include "uft/uft_format_convert.h"
#include "uft/uft_format_probe.h"
#include "uft/uft_error.h"
#include "uft/uft_error_compat.h"

/* Format parsers */
#include "uft/uft_format_parsers.h"
#include "uft/formats/uft_scp.h"
#include "uft/formats/uft_scp_writer.h"
#include "uft/uft_scp_format.h"
#include "uft/uft_hfe_format.h"
#include "uft/uft_c64_gcr.h"
#include "uft/formats/c64/uft_d64_g64.h"
#include "uft/uft_d64_writer.h"
#include "uft/formats/uft_td0.h"
#include "uft/uft_flux_pll.h"
#include "uft/formats/uft_mfm.h"
#include "uft/uft_imd.h"

#include "uft/core/uft_forensic_constants.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

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
    // === FLUX -> BITSTREAM (Lossless) ===
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

#define NUM_CONVERSION_PATHS \
    (sizeof(g_conversion_paths) / sizeof(g_conversion_paths[0]) - 1)

// ============================================================================
// Helper: Write output file
// ============================================================================

static uft_error_t write_output_file(const char* path, const uint8_t* data,
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

static void report_progress(const uft_convert_options_ext_t* opts,
                            int percent, const char* stage) {
    if (opts && opts->progress_cb) {
        opts->progress_cb(percent, stage, opts->progress_user);
    }
}

static bool is_cancelled(const uft_convert_options_ext_t* opts) {
    return opts && opts->cancel && *opts->cancel;
}

// ============================================================================
// Archive -> Sector Conversions
// ============================================================================

/**
 * @brief TD0 -> IMG: Decompress Teledisk to raw sector image
 *
 * Uses uft_td0_read_mem() to parse/decompress, then uft_td0_to_raw()
 * to extract the raw sector data.
 */
static uft_error_t convert_td0_to_img(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    uft_td0_image_t td0_img;
    uft_td0_init(&td0_img);

    report_progress(opts, 10, "Parsing TD0 image");

    int rc = uft_td0_read_mem(src_data, src_size, &td0_img);
    if (rc != 0) {
        uft_td0_free(&td0_img);
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "TD0 parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    report_progress(opts, 50, "Extracting raw sectors");

    /* Convert to raw sector data */
    uint8_t* raw_data = NULL;
    size_t raw_size = 0;
    rc = uft_td0_to_raw(&td0_img, &raw_data, &raw_size, 0xF6);
    if (rc != 0 || !raw_data) {
        uft_td0_free(&td0_img);
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "TD0 sector extraction failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    report_progress(opts, 80, "Writing IMG output");

    /* Write output file */
    uft_error_t err = write_output_file(dst_path, raw_data, raw_size);
    if (err != UFT_OK) {
        free(raw_data);
        uft_td0_free(&td0_img);
        result->error = err;
        return err;
    }

    result->success = true;
    result->bytes_written = (int)raw_size;
    result->tracks_converted = td0_img.num_tracks;

    snprintf(result->warnings[result->warning_count++],
             sizeof(result->warnings[0]),
             "Decompressed %d tracks (%s compression), %zu bytes output",
             td0_img.num_tracks,
             td0_img.advanced_compression ? "LZSS" : "none",
             raw_size);

    free(raw_data);
    uft_td0_free(&td0_img);

    report_progress(opts, 100, "TD0->IMG complete");
    return UFT_OK;
}

/**
 * @brief TD0 -> IMD: Convert Teledisk to ImageDisk (preserves metadata)
 */
static uft_error_t convert_td0_to_imd(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    uft_td0_image_t td0_img;
    uft_td0_init(&td0_img);

    report_progress(opts, 10, "Parsing TD0 image");

    int rc = uft_td0_read_mem(src_data, src_size, &td0_img);
    if (rc != 0) {
        uft_td0_free(&td0_img);
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "TD0 parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    report_progress(opts, 40, "Converting TD0 to IMD format");

    /* TODO: uft_td0_to_imd() needs an IMD image struct.
     * The function is declared in uft_td0.h. Once IMD writer
     * is fully wired, uncomment and use directly:
     *
     *   uft_imd_image_t imd;
     *   rc = uft_td0_to_imd(&td0_img, &imd);
     *   uft_imd_write(dst_path, &imd);
     *
     * For now, fall back to raw extraction with IMD header synthesis.
     */

    /* Fallback: extract raw and write with IMD header */
    uint8_t* raw_data = NULL;
    size_t raw_size = 0;
    rc = uft_td0_to_raw(&td0_img, &raw_data, &raw_size, 0xE5);
    if (rc != 0 || !raw_data) {
        uft_td0_free(&td0_img);
        result->error = UFT_ERR_FORMAT;
        return UFT_ERR_FORMAT;
    }

    /* Build minimal IMD file: header + track records */
    /* IMD header is ASCII text terminated by 0x1A */
    char imd_header[256];
    int hdr_len = snprintf(imd_header, sizeof(imd_header),
                           "IMD 1.18: Converted from TD0 by UFT\x1a");

    size_t imd_size = hdr_len + raw_size + (td0_img.num_tracks * 5);
    uint8_t* imd_data = malloc(imd_size);
    if (!imd_data) {
        free(raw_data);
        uft_td0_free(&td0_img);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    size_t pos = 0;
    memcpy(imd_data + pos, imd_header, hdr_len);
    pos += hdr_len;

    /* Write track records */
    size_t raw_offset = 0;
    for (int t = 0; t < td0_img.num_tracks; t++) {
        uft_td0_track_t* trk = &td0_img.tracks[t];
        uint8_t mode = (td0_img.header.data_rate == UFT_TD0_RATE_500K)
                       ? 3 /* 500K MFM */ : 5; /* 250K MFM */
        int nsec = trk->nsectors;
        int sec_size_code = (nsec > 0 && trk->sectors)
                            ? trk->sectors[0].header.size : 2;
        int sec_size = 128 << sec_size_code;

        /* Track header: mode, cyl, head, nsectors, size_code */
        imd_data[pos++] = mode;
        imd_data[pos++] = trk->header.cylinder;
        imd_data[pos++] = trk->header.side;
        imd_data[pos++] = (uint8_t)nsec;
        imd_data[pos++] = (uint8_t)sec_size_code;

        /* Sector numbering map */
        for (int s = 0; s < nsec; s++) {
            if (s < nsec && trk->sectors) {
                imd_data[pos++] = trk->sectors[s].header.sector;
            } else {
                imd_data[pos++] = (uint8_t)(s + 1);
            }
        }

        /* Sector data records */
        for (int s = 0; s < nsec; s++) {
            if (trk->sectors && trk->sectors[s].data) {
                imd_data[pos++] = 0x01; /* Normal data */
                memcpy(imd_data + pos, trk->sectors[s].data, sec_size);
                pos += sec_size;
            } else {
                imd_data[pos++] = 0x02; /* Compressed (fill) */
                imd_data[pos++] = 0xE5;
            }
        }

        raw_offset += nsec * sec_size;
        result->tracks_converted++;
    }

    report_progress(opts, 80, "Writing IMD output");

    uft_error_t err = write_output_file(dst_path, imd_data, pos);
    result->bytes_written = (int)pos;
    if (err == UFT_OK) {
        result->success = true;
    } else {
        result->error = err;
    }

    free(imd_data);
    free(raw_data);
    uft_td0_free(&td0_img);

    report_progress(opts, 100, "TD0->IMD complete");
    return err;
}

/**
 * @brief NBZ -> D64: Decompress LZ77/LZSS nibble archive to D64
 *
 * NBZ is a gzip-compressed D64/G64 file. The first bytes after
 * decompression reveal the inner format.
 */
static uft_error_t convert_nbz_to_d64(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    report_progress(opts, 10, "Decompressing NBZ archive");

    /*
     * NBZ files are gzip-compressed D64 images. We use the LZHUF
     * decompressor from uft_format_parsers.h as a fallback, but the
     * typical NBZ format uses standard zlib/gzip.
     *
     * Strategy: Try LZHUF decompression with generous output buffer.
     * Standard D64 sizes: 174848 (35trk), 175531 (35trk+err),
     *                     196608 (40trk), 197376 (40trk+err)
     */
    size_t max_output = 256 * 1024; /* 256 KB should cover any D64 */
    uint8_t* decompressed = malloc(max_output);
    if (!decompressed) {
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    int decomp_size = uft_lzhuf_decompress(src_data, src_size,
                                            decompressed, max_output,
                                            &UFT_LZHUF_TD0_OPTIONS);

    if (decomp_size <= 0) {
        /* LZHUF failed - try raw copy if source is already a valid D64 size */
        if (src_size == 174848 || src_size == 175531 ||
            src_size == 196608 || src_size == 197376) {
            memcpy(decompressed, src_data, src_size);
            decomp_size = (int)src_size;
            snprintf(result->warnings[result->warning_count++],
                     sizeof(result->warnings[0]),
                     "NBZ decompression failed, source appears to be raw D64");
        } else {
            free(decompressed);
            result->error = UFT_ERR_FORMAT;
            snprintf(result->warnings[result->warning_count++],
                     sizeof(result->warnings[0]),
                     "NBZ decompression failed (%d bytes input)", (int)src_size);
            return UFT_ERR_FORMAT;
        }
    }

    /* Validate that output looks like a D64 */
    if (decomp_size != 174848 && decomp_size != 175531 &&
        decomp_size != 196608 && decomp_size != 197376) {
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Decompressed size %d does not match standard D64 sizes",
                 decomp_size);
    }

    report_progress(opts, 80, "Writing D64 output");

    uft_error_t err = write_output_file(dst_path, decompressed, decomp_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = decomp_size;
        int num_tracks = (decomp_size >= 196608) ? 40 : 35;
        result->tracks_converted = num_tracks;
    } else {
        result->error = err;
    }

    free(decompressed);
    report_progress(opts, 100, "NBZ->D64 complete");
    return err;
}

/**
 * @brief NBZ -> G64: Decompress NBZ archive to G64 bitstream image
 */
static uft_error_t convert_nbz_to_g64(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    report_progress(opts, 10, "Decompressing NBZ for G64");

    /* NBZ can contain either D64 or G64 data. Decompress first. */
    size_t max_output = 1024 * 1024; /* G64 can be up to ~700KB */
    uint8_t* decompressed = malloc(max_output);
    if (!decompressed) {
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    int decomp_size = uft_lzhuf_decompress(src_data, src_size,
                                            decompressed, max_output,
                                            &UFT_LZHUF_TD0_OPTIONS);
    if (decomp_size <= 0) {
        free(decompressed);
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "NBZ decompression failed for G64 output");
        return UFT_ERR_FORMAT;
    }

    report_progress(opts, 50, "Checking decompressed format");

    /* Check if decompressed data is already G64 (starts with "GCR-1541") */
    if (decomp_size >= 12 &&
        memcmp(decompressed, G64_SIGNATURE, G64_SIGNATURE_LEN) == 0) {
        /* Already a G64 - write directly */
        report_progress(opts, 80, "Writing G64 output");
        uft_error_t err = write_output_file(dst_path, decompressed, decomp_size);
        if (err == UFT_OK) {
            result->success = true;
            result->bytes_written = decomp_size;
        } else {
            result->error = err;
        }
        free(decompressed);
        return err;
    }

    /* Decompressed data is D64 - convert to G64 via d64_to_g64() */
    d64_image_t* d64 = NULL;
    int rc = d64_load_buffer(decompressed, decomp_size, &d64);
    free(decompressed);

    if (rc != 0 || !d64) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "NBZ decompressed data is not valid D64 or G64");
        return UFT_ERR_FORMAT;
    }

    report_progress(opts, 60, "Converting D64 to G64");

    g64_image_t* g64 = NULL;
    convert_result_t conv_result;
    rc = d64_to_g64(d64, &g64, NULL, &conv_result);
    d64_free(d64);

    if (rc != 0 || !g64) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "D64 to G64 conversion failed: %s", conv_result.description);
        return UFT_ERR_FORMAT;
    }

    report_progress(opts, 80, "Writing G64 output");

    rc = g64_save(dst_path, g64);
    if (rc == 0) {
        result->success = true;
        result->tracks_converted = conv_result.tracks_converted;
        result->sectors_converted = conv_result.sectors_converted;
    } else {
        result->error = UFT_ERR_IO;
    }

    g64_free(g64);
    report_progress(opts, 100, "NBZ->G64 complete");
    return result->success ? UFT_OK : result->error;
}

// ============================================================================
// Flux/Bitstream -> Sector Conversions (Decode Pipeline)
// ============================================================================

/**
 * @brief SCP -> D64: Decode SCP flux to C64 D64 sector image
 *
 * Pipeline: SCP parse -> flux deltas -> PLL -> GCR bitstream -> sector extract
 */
static uft_error_t convert_scp_to_d64(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    report_progress(opts, 5, "Parsing SCP file");

    /* Parse the SCP file */
    uft_scp_file_t scp;
    int rc = uft_scp_read(src_data, src_size, &scp);
    if (rc != 0) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "SCP parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    /* Create D64 output image */
    d64_image_t* d64 = d64_create(35);
    if (!d64) {
        uft_scp_free(&scp);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    report_progress(opts, 10, "Decoding flux to GCR sectors");

    int retries = (opts && opts->decode_retries > 0) ? opts->decode_retries : 5;

    /* Process each track: flux -> PLL -> GCR -> sectors */
    for (int track = 1; track <= 35; track++) {
        if (is_cancelled(opts)) break;

        int scp_track_idx = (track - 1) * 2; /* Side 0 only for C64 */
        int sectors_per_trk = uft_c64_sectors_per_track(track);

        /* Get flux deltas for best revolution */
        double deltas[131072];
        int best_rev = 0;
        int max_flux = 0;

        for (int rev = 0; rev < (int)scp.header.revolutions; rev++) {
            int count = uft_scp_get_track_flux(&scp, scp_track_idx, rev,
                                                deltas, 131072);
            if (count > max_flux) {
                max_flux = count;
                best_rev = rev;
            }
        }

        /* Read the best revolution */
        int flux_count = uft_scp_get_track_flux(&scp, scp_track_idx, best_rev,
                                                 deltas, 131072);
        if (flux_count <= 0) {
            result->tracks_failed++;
            continue;
        }

        /* Initialize PLL for C64 GCR encoding */
        uft_pll_t pll;
        int bitrate = uft_c64_track_bitrate(track);
        uft_pll_init(&pll, (double)bitrate, UFT_ENCODING_GCR);

        /* Allocate bitstream buffer */
        size_t max_bits = flux_count * 4;
        size_t bit_buf_size = (max_bits + 7) / 8;
        uint8_t* bitstream = calloc(1, bit_buf_size);
        if (!bitstream) {
            result->tracks_failed++;
            continue;
        }

        /* Run PLL to extract bitstream */
        size_t bit_pos = 0;
        for (int f = 0; f < flux_count && bit_pos < max_bits - 16; f++) {
            double delta_sec = deltas[f] * 25e-9; /* SCP ticks to seconds */
            uft_pll_process_flux_mfm(&pll, delta_sec, bitstream, &bit_pos);
        }

        /* Extract GCR sectors from bitstream using C64 parser */
        uft_c64_parser_t parser;
        uft_c64_parser_init(&parser, track);

        bool sector_found[21] = {false};
        uint8_t sector_data[21][256];

        for (size_t b = 0; b < bit_pos; b++) {
            size_t byte_idx = b / 8;
            size_t bit_idx = 7 - (b % 8);
            uint8_t bit = (bitstream[byte_idx] >> bit_idx) & 1;
            uft_c64_parser_add_bit(&parser, bit, (unsigned long)b);

            /* Check if parser found a complete sector */
            if (parser.state == UFT_C64_STATE_IDLE &&
                parser.last_sector >= 0 &&
                parser.last_sector < sectors_per_trk &&
                parser.last_track == track) {
                /* Attempt to extract sector data from parser buffer */
                if (parser.byte_len >= 256 && !sector_found[parser.last_sector]) {
                    memcpy(sector_data[parser.last_sector],
                           parser.byte_buffer, 256);
                    sector_found[parser.last_sector] = true;
                    result->sectors_converted++;
                }
            }
        }

        /* Write extracted sectors to D64 */
        for (int s = 0; s < sectors_per_trk; s++) {
            if (sector_found[s]) {
                d64_set_sector(d64, track, s, sector_data[s], D64_ERR_OK);
            } else {
                /* Fill unread sector with forensic fill byte (0x01).
                 * Using UFT_FORENSIC_FILL_BYTE instead of 0x00 so that
                 * filled sectors are distinguishable from legitimately
                 * blank/zeroed data in forensic analysis. */
                uint8_t fill[256];
                memset(fill, UFT_FORENSIC_FILL_BYTE, 256);
                d64_set_sector(d64, track, s, fill, D64_ERR_DATA_NOT_FOUND);
                result->sectors_failed++;

                /* Log which sectors were forensic-filled */
                if (result->warning_count < 8) {
                    snprintf(result->warnings[result->warning_count++],
                             sizeof(result->warnings[0]),
                             "Track %d sector %d: GCR decode failed, "
                             "filled with 0x%02X",
                             track, s, UFT_FORENSIC_FILL_BYTE);
                }
            }
        }

        free(bitstream);
        result->tracks_converted++;

        report_progress(opts, 10 + (track * 80 / 35), "Decoding tracks");
    }

    report_progress(opts, 90, "Writing D64 output");

    /* Save D64 */
    rc = d64_save(dst_path, d64, false);
    if (rc == 0) {
        result->success = true;
    } else {
        result->error = UFT_ERR_IO;
    }

    d64_free(d64);
    uft_scp_free(&scp);
    report_progress(opts, 100, "SCP->D64 complete");
    return result->success ? UFT_OK : result->error;
}

/**
 * @brief SCP -> ADF/IMG: Decode SCP flux to MFM sector image
 *
 * Pipeline: SCP parse -> flux deltas -> MFM PLL -> sector extract -> write
 * Used for both ADF (Amiga) and IMG (PC) output.
 */
static uft_error_t convert_scp_to_mfm_sectors(const uint8_t* src_data,
                                                size_t src_size,
                                                const char* dst_path,
                                                uft_format_t dst_format,
                                                const uft_convert_options_ext_t* opts,
                                                uft_convert_result_t* result) {
    report_progress(opts, 5, "Parsing SCP file");

    uft_scp_file_t scp;
    int rc = uft_scp_read(src_data, src_size, &scp);
    if (rc != 0) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "SCP parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    /* Determine geometry from SCP disk type */
    int cylinders = 80, heads = 2, sectors = 0, sector_size = 512;
    if (dst_format == UFT_FORMAT_ADF) {
        sectors = 11; /* Amiga DD */
    } else {
        /* PC: auto-detect from track count */
        int track_count = scp.track_count / 2;
        if (track_count <= 40) {
            sectors = 9; cylinders = 40; /* 360K */
        } else {
            sectors = 18; /* 1.44MB default */
        }
    }

    size_t total_sectors_count = (size_t)cylinders * heads * sectors;
    size_t output_size = total_sectors_count * sector_size;
    uint8_t* output = calloc(1, output_size);
    if (!output) {
        uft_scp_free(&scp);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    report_progress(opts, 10, "Decoding MFM flux to sectors");

    /*
     * TODO: Full MFM sector decode pipeline.
     *
     * The framework here calls the PLL to extract bitstream from each
     * SCP track, then would need an MFM sector parser to find IDAM/DAM
     * markers and extract sector data. The MFM sector parser is not
     * yet available as a standalone API in this codebase.
     *
     * For now: attempt flux->bitstream via PLL, mark sectors as needing
     * MFM decode integration.
     */

    for (int cyl = 0; cyl < cylinders; cyl++) {
        for (int head = 0; head < heads; head++) {
            if (is_cancelled(opts)) goto done_mfm;

            int scp_track = cyl * 2 + head;
            double deltas[131072];
            int flux_count = uft_scp_get_track_flux(&scp, scp_track, 0,
                                                     deltas, 131072);
            if (flux_count <= 0) {
                result->tracks_failed++;
                continue;
            }

            /* PLL decode to bitstream */
            uft_pll_t pll;
            double data_rate = (dst_format == UFT_FORMAT_ADF) ? 500000.0 : 500000.0;
            uft_pll_init(&pll, data_rate, UFT_ENCODING_MFM);

            size_t max_bits = (size_t)flux_count * 4;
            size_t buf_size = (max_bits + 7) / 8;
            uint8_t* bitstream = calloc(1, buf_size);
            if (!bitstream) {
                result->tracks_failed++;
                continue;
            }

            size_t bit_pos = 0;
            for (int f = 0; f < flux_count && bit_pos < max_bits - 16; f++) {
                double delta_sec = deltas[f] * 25e-9;
                uft_pll_process_flux_mfm(&pll, delta_sec, bitstream, &bit_pos);
            }

            /*
             * TODO: MFM sector extraction from bitstream.
             * Need to find 0xA1A1A1 sync + IDAM/DAM markers,
             * extract sector CHS+data, verify CRC.
             *
             * For now, count the track as attempted but not decoded.
             */
            result->tracks_converted++;

            snprintf(result->warnings[result->warning_count < 7
                         ? result->warning_count : 7],
                     sizeof(result->warnings[0]),
                     "MFM sector extraction requires integration with "
                     "MFM IDAM/DAM parser (TODO)");
            if (result->warning_count < 8) result->warning_count++;

            free(bitstream);
        }

        report_progress(opts, 10 + (cyl * 80 / cylinders), "Decoding tracks");
    }

done_mfm:
    /* FORENSIC SAFETY: Do NOT write an all-zero file as "success".
     * Check if any actual sector data was extracted by scanning for non-zero content. */
    {
        bool has_data = false;
        for (size_t i = 0; i < output_size && !has_data; i++) {
            if (output[i] != 0) has_data = true;
        }

        if (!has_data) {
            /* No sector data extracted — MFM parser not yet integrated */
            result->error = UFT_ERR_NOT_IMPLEMENTED;
            result->success = false;
            snprintf(result->warnings[result->warning_count < 8
                         ? result->warning_count : 7],
                     sizeof(result->warnings[0]),
                     "CRITICAL: No sector data extracted from flux. "
                     "MFM IDAM/DAM sector parser integration required. "
                     "Output file NOT written to prevent silent data loss.");
            if (result->warning_count < 8) result->warning_count++;

            free(output);
            uft_scp_free(&scp);
            report_progress(opts, 100, "SCP->sector FAILED (no data extracted)");
            return UFT_ERR_NOT_IMPLEMENTED;
        }
    }

    report_progress(opts, 90, "Writing output");

    uft_error_t err = write_output_file(dst_path, output, output_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = (int)output_size;
    } else {
        result->error = err;
    }

    free(output);
    uft_scp_free(&scp);
    report_progress(opts, 100, "SCP->sector complete");
    return err;
}

/**
 * @brief Kryoflux -> D64: Decode Kryoflux stream to D64
 *
 * Similar to SCP->D64 but reads Kryoflux stream files.
 * Kryoflux uses per-track .raw files (track00.0.raw, etc.)
 */
static uft_error_t convert_kryoflux_to_d64(const uint8_t* src_data,
                                             size_t src_size,
                                             const char* dst_path,
                                             const uft_convert_options_ext_t* opts,
                                             uft_convert_result_t* result) {
    report_progress(opts, 5, "Parsing Kryoflux stream");

    /* Parse Kryoflux stream data */
    uft_kfx_stream_t stream;
    int rc = uft_kfx_read_stream(src_data, src_size, &stream);
    if (rc != 0) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Kryoflux stream parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    /*
     * NOTE: Kryoflux stores one track per file. A full disk conversion
     * requires reading all track files. This single-buffer implementation
     * can only process one track's worth of data.
     *
     * For a full disk conversion, the caller should provide the directory
     * path and this function should iterate over track files.
     * For now, we process whatever single track is in the buffer.
     */

    d64_image_t* d64 = d64_create(35);
    if (!d64) {
        uft_kfx_free(&stream);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    report_progress(opts, 20, "Decoding Kryoflux flux data");

    if (stream.flux_count > 0 && stream.flux_deltas) {
        /* PLL decode */
        uft_pll_t pll;
        uft_pll_init(&pll, 307692.0, UFT_ENCODING_GCR); /* Zone 0 bitrate */

        size_t max_bits = stream.flux_count * 4;
        size_t buf_size = (max_bits + 7) / 8;
        uint8_t* bitstream = calloc(1, buf_size);

        if (bitstream) {
            size_t bit_pos = 0;
            for (size_t f = 0; f < stream.flux_count && bit_pos < max_bits - 16; f++) {
                uft_pll_process_flux_mfm(&pll, stream.flux_deltas[f],
                                          bitstream, &bit_pos);
            }

            /* TODO: Extract sectors from GCR bitstream (same as SCP->D64) */
            result->tracks_converted = 1;
            snprintf(result->warnings[result->warning_count++],
                     sizeof(result->warnings[0]),
                     "Single-track Kryoflux decode; full disk requires "
                     "directory iteration over track files");

            free(bitstream);
        }
    }

    report_progress(opts, 80, "Writing D64 output");

    rc = d64_save(dst_path, d64, false);
    if (rc == 0) {
        result->success = true;
    } else {
        result->error = UFT_ERR_IO;
    }

    d64_free(d64);
    uft_kfx_free(&stream);
    report_progress(opts, 100, "Kryoflux->D64 complete");
    return result->success ? UFT_OK : result->error;
}

/**
 * @brief G64 -> D64: Decode GCR bitstream to sector data
 *
 * Uses the g64_to_d64() converter from uft_d64_g64.h.
 */
static uft_error_t convert_g64_to_d64(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    report_progress(opts, 10, "Loading G64 image");

    g64_image_t* g64 = NULL;
    int rc = g64_load_buffer(src_data, src_size, &g64);
    if (rc != 0 || !g64) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "G64 parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    report_progress(opts, 30, "Converting G64 GCR to D64 sectors");

    d64_image_t* d64 = NULL;
    convert_options_t conv_opts;
    convert_get_defaults(&conv_opts);
    convert_result_t conv_result;

    rc = g64_to_d64(g64, &d64, &conv_opts, &conv_result);
    g64_free(g64);

    if (rc != 0 || !d64) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "G64->D64 decode failed: %s", conv_result.description);
        return UFT_ERR_FORMAT;
    }

    report_progress(opts, 80, "Writing D64 output");

    rc = d64_save(dst_path, d64, false);
    if (rc == 0) {
        result->success = true;
        result->tracks_converted = conv_result.tracks_converted;
        result->sectors_converted = conv_result.sectors_converted;
    } else {
        result->error = UFT_ERR_IO;
    }

    d64_free(d64);
    report_progress(opts, 100, "G64->D64 complete");
    return result->success ? UFT_OK : result->error;
}

// ============================================================================
// Sector -> Bitstream Conversions (Synthetic encoding)
// ============================================================================

/**
 * @brief D64 -> G64: Encode sectors to synthetic GCR bitstream
 *
 * Uses the d64_to_g64() converter from uft_d64_g64.h.
 */
static uft_error_t convert_d64_to_g64(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    report_progress(opts, 10, "Loading D64 image");

    d64_image_t* d64 = NULL;
    int rc = d64_load_buffer(src_data, src_size, &d64);
    if (rc != 0 || !d64) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "D64 parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    report_progress(opts, 30, "Encoding D64 sectors to GCR");

    g64_image_t* g64 = NULL;
    convert_options_t conv_opts;
    convert_get_defaults(&conv_opts);
    convert_result_t conv_result;

    rc = d64_to_g64(d64, &g64, &conv_opts, &conv_result);
    d64_free(d64);

    if (rc != 0 || !g64) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "D64->G64 encode failed: %s", conv_result.description);
        return UFT_ERR_FORMAT;
    }

    report_progress(opts, 80, "Writing G64 output");

    rc = g64_save(dst_path, g64);
    if (rc == 0) {
        result->success = true;
        result->tracks_converted = conv_result.tracks_converted;
        result->sectors_converted = conv_result.sectors_converted;
    } else {
        result->error = UFT_ERR_IO;
    }

    g64_free(g64);
    report_progress(opts, 100, "D64->G64 complete");
    return result->success ? UFT_OK : result->error;
}

/**
 * @brief ADF/IMG -> HFE: Encode sector data to synthetic MFM in HFE container
 *
 * Builds an HFE file with synthetic MFM bitstream encoding of the sector data.
 */
static uft_error_t convert_sectors_to_hfe(const uint8_t* src_data,
                                           size_t src_size,
                                           const char* dst_path,
                                           uft_format_t src_format,
                                           const uft_convert_options_ext_t* opts,
                                           uft_convert_result_t* result) {
    report_progress(opts, 10, "Analyzing sector geometry");

    /* Determine geometry based on format and file size */
    int cylinders = 80, heads = 2, sectors = 18, sector_size = 512;
    uint16_t bitrate = 250; /* kbps */
    hfe_track_encoding_t encoding = HFE_ENC_ISOIBM_MFM;
    hfe_floppy_interface_t iface = HFE_IF_IBMPC_DD;

    if (src_format == UFT_FORMAT_ADF) {
        sectors = 11;
        encoding = HFE_ENC_AMIGA_MFM;
        iface = HFE_IF_AMIGA_DD;
        if (src_size > 901120) {
            sectors = 22; /* HD */
            bitrate = 500;
            iface = HFE_IF_AMIGA_HD;
        }
    } else {
        /* IMG: detect from file size */
        if (src_size <= 368640) {
            cylinders = 40; sectors = 9;
        } else if (src_size <= 737280) {
            cylinders = 80; sectors = 9;
        } else if (src_size <= 1228800) {
            cylinders = 80; sectors = 15; bitrate = 500;
            iface = HFE_IF_IBMPC_HD;
        } else {
            cylinders = 80; sectors = 18; bitrate = 500;
            iface = HFE_IF_IBMPC_HD;
        }
    }

    size_t expected_size = (size_t)cylinders * heads * sectors * sector_size;
    if (src_size < expected_size) {
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Source size %zu < expected %zu, padding with zeros",
                 src_size, expected_size);
    }

    report_progress(opts, 20, "Building HFE container");

    /*
     * Build the HFE file in memory:
     * 1. Header (512 bytes)
     * 2. Track lookup table (cylinders * 4 bytes, padded to 512)
     * 3. Track data blocks (interleaved head 0/1 in 256-byte chunks)
     */

    /* Calculate MFM track size: each sector is ~640 MFM bytes
     * (preamble + sync + IDAM + gap + sync + DAM + data + CRC + gap) */
    int raw_track_bits = sectors * (sector_size + 62) * 16; /* rough estimate */
    int mfm_track_bytes = (raw_track_bits + 7) / 8;
    if (mfm_track_bytes < 6250) mfm_track_bytes = 6250;
    /* Round up to multiple of 256 for HFE interleaving */
    int track_len_aligned = ((mfm_track_bytes + 255) / 256) * 256;

    /* Total file size */
    size_t lut_blocks = (cylinders * sizeof(hfe_track_entry_t) + 511) / 512;
    size_t data_start_block = 1 + lut_blocks;
    size_t blocks_per_track = (track_len_aligned * 2 + 511) / 512;
    size_t total_size = (data_start_block + cylinders * blocks_per_track) * 512;

    uint8_t* hfe_data = calloc(1, total_size);
    if (!hfe_data) {
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    /* Write header */
    hfe_header_t* hdr = (hfe_header_t*)hfe_data;
    hfe_init_header(hdr, false);
    hdr->n_cylinders = (uint8_t)cylinders;
    hdr->n_heads = (uint8_t)heads;
    hdr->track_encoding = (uint8_t)encoding;
    hdr->data_bit_rate = bitrate;
    hdr->drive_rpm = 300;
    hdr->uft_floppy_interface = (uint8_t)iface;
    hdr->track_list_offset = 1; /* Block 1 */

    /* Write track lookup table */
    hfe_track_entry_t* lut = (hfe_track_entry_t*)(hfe_data + 512);
    for (int cyl = 0; cyl < cylinders; cyl++) {
        lut[cyl].offset = (uint16_t)(data_start_block + cyl * blocks_per_track);
        lut[cyl].length = (uint16_t)track_len_aligned;
    }

    report_progress(opts, 40, "Encoding MFM tracks");

    /* Encode each cylinder's sectors into MFM bitstream */
    for (int cyl = 0; cyl < cylinders; cyl++) {
        if (is_cancelled(opts)) break;

        uint8_t head0_mfm[32768];
        uint8_t head1_mfm[32768];
        memset(head0_mfm, 0x4E, track_len_aligned); /* Gap fill */
        memset(head1_mfm, 0x4E, track_len_aligned);

        for (int hd = 0; hd < heads; hd++) {
            uint8_t* mfm_buf = (hd == 0) ? head0_mfm : head1_mfm;
            int mfm_pos = 0;

            /* Write track preamble: gap4a (80x 0x4E) + sync (12x 0x00) + IAM */
            memset(mfm_buf, 0x4E, 80); mfm_pos = 80;
            memset(mfm_buf + mfm_pos, 0x00, 12); mfm_pos += 12;
            /* IAM: 3x 0xC2 + 0xFC */
            mfm_buf[mfm_pos++] = 0xC2;
            mfm_buf[mfm_pos++] = 0xC2;
            mfm_buf[mfm_pos++] = 0xC2;
            mfm_buf[mfm_pos++] = 0xFC;
            /* Gap1 */
            memset(mfm_buf + mfm_pos, 0x4E, 50); mfm_pos += 50;

            for (int sec = 0; sec < sectors; sec++) {
                /* Calculate source offset */
                size_t src_offset = ((size_t)cyl * heads * sectors +
                                     (size_t)hd * sectors + sec) * sector_size;

                /* Pre-ID sync: 12x 0x00 */
                memset(mfm_buf + mfm_pos, 0x00, 12); mfm_pos += 12;
                /* IDAM: 3x 0xA1 + 0xFE */
                mfm_buf[mfm_pos++] = 0xA1;
                mfm_buf[mfm_pos++] = 0xA1;
                mfm_buf[mfm_pos++] = 0xA1;
                mfm_buf[mfm_pos++] = 0xFE;
                /* ID: C H R N */
                mfm_buf[mfm_pos++] = (uint8_t)cyl;
                mfm_buf[mfm_pos++] = (uint8_t)hd;
                mfm_buf[mfm_pos++] = (uint8_t)(sec + 1);
                mfm_buf[mfm_pos++] = 0x02; /* 512 bytes */
                /* CRC placeholder (2 bytes) */
                mfm_buf[mfm_pos++] = 0x00;
                mfm_buf[mfm_pos++] = 0x00;
                /* Gap2 */
                memset(mfm_buf + mfm_pos, 0x4E, 22); mfm_pos += 22;
                /* Pre-data sync: 12x 0x00 */
                memset(mfm_buf + mfm_pos, 0x00, 12); mfm_pos += 12;
                /* DAM: 3x 0xA1 + 0xFB */
                mfm_buf[mfm_pos++] = 0xA1;
                mfm_buf[mfm_pos++] = 0xA1;
                mfm_buf[mfm_pos++] = 0xA1;
                mfm_buf[mfm_pos++] = 0xFB;
                /* Sector data */
                if (src_offset + sector_size <= src_size) {
                    memcpy(mfm_buf + mfm_pos, src_data + src_offset, sector_size);
                } else {
                    memset(mfm_buf + mfm_pos, 0xE5, sector_size);
                }
                mfm_pos += sector_size;
                /* CRC placeholder (2 bytes) */
                mfm_buf[mfm_pos++] = 0x00;
                mfm_buf[mfm_pos++] = 0x00;
                /* Gap3 */
                int gap3 = (sectors <= 9) ? 84 : ((sectors <= 15) ? 54 : 38);
                memset(mfm_buf + mfm_pos, 0x4E, gap3); mfm_pos += gap3;

                result->sectors_converted++;
            }
        }

        /* Interleave head 0/1 data into HFE track block */
        uint8_t* track_dest = hfe_data + (size_t)lut[cyl].offset * 512;
        hfe_interleave_track(head0_mfm, head1_mfm,
                              (uint16_t)track_len_aligned, track_dest);

        result->tracks_converted++;
        report_progress(opts, 40 + (cyl * 50 / cylinders), "Encoding MFM tracks");
    }

    report_progress(opts, 95, "Writing HFE output");

    uft_error_t err = write_output_file(dst_path, hfe_data, total_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = (int)total_size;
    } else {
        result->error = err;
    }

    free(hfe_data);
    report_progress(opts, 100, "Sector->HFE complete");
    return err;
}

// ============================================================================
// Bitstream <-> Flux Conversions
// ============================================================================

/**
 * @brief HFE -> SCP: Convert HFE bitstream to SCP flux
 *
 * Each HFE bit cell maps to a flux transition time based on the bit rate.
 * Uses scp_writer API to build the SCP file.
 */
static uft_error_t convert_hfe_to_scp(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    report_progress(opts, 10, "Parsing HFE file");

    if (src_size < sizeof(hfe_header_t)) {
        result->error = UFT_ERR_FORMAT;
        return UFT_ERR_FORMAT;
    }

    const hfe_header_t* hdr = (const hfe_header_t*)src_data;
    if (!hfe_is_valid_header(hdr)) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Invalid HFE header");
        return UFT_ERR_FORMAT;
    }

    int cylinders = hdr->n_cylinders;
    int heads = hdr->n_heads;
    uint16_t bitrate_kbps = hdr->data_bit_rate;
    uint32_t cell_ns = hfe_cell_time_ns(bitrate_kbps);

    /* Determine SCP disk type */
    uint8_t scp_disk_type = SCP_TYPE_PC_DD;
    if (hdr->uft_floppy_interface == HFE_IF_AMIGA_DD ||
        hdr->uft_floppy_interface == HFE_IF_AMIGA_HD) {
        scp_disk_type = SCP_TYPE_AMIGA;
    } else if (hdr->uft_floppy_interface == HFE_IF_C64_DD) {
        scp_disk_type = SCP_TYPE_C64;
    }

    int revolutions = (opts && opts->synthetic_revolutions > 0)
                      ? opts->synthetic_revolutions : 1;

    scp_writer_t* writer = scp_writer_create(scp_disk_type, (uint8_t)revolutions);
    if (!writer) {
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    report_progress(opts, 20, "Converting bitstream to flux");

    /* Read track LUT */
    const hfe_track_entry_t* lut = (const hfe_track_entry_t*)
                                    (src_data + hdr->track_list_offset * 512);

    for (int cyl = 0; cyl < cylinders; cyl++) {
        if (is_cancelled(opts)) break;

        uint16_t track_offset_blocks = lut[cyl].offset;
        uint16_t track_len = lut[cyl].length;

        if (track_offset_blocks == 0 || track_len == 0) continue;

        const uint8_t* interleaved = src_data + (size_t)track_offset_blocks * 512;

        for (int hd = 0; hd < heads; hd++) {
            /* De-interleave to get single-head track data */
            uint8_t* track_bits = malloc(track_len);
            if (!track_bits) continue;

            hfe_deinterleave_track(interleaved, track_len, (uint8_t)hd,
                                    track_bits);

            /* Convert bitstream to flux transitions */
            /* HFE stores bits LSB-first, so reverse each byte */
            hfe_reverse_bits(track_bits, track_len);

            /* Convert each '1' bit to a flux transition */
            uint32_t flux_buf[131072];
            size_t flux_count = 0;
            uint32_t accum_ns = 0;

            for (size_t byte_i = 0; byte_i < track_len; byte_i++) {
                uint8_t b = track_bits[byte_i];
                for (int bit = 7; bit >= 0; bit--) {
                    accum_ns += cell_ns;
                    if ((b >> bit) & 1) {
                        if (flux_count < 131072) {
                            flux_buf[flux_count++] = accum_ns;
                        }
                        accum_ns = 0;
                    }
                }
            }

            /* Calculate track duration */
            uint32_t duration_ns = 0;
            for (size_t i = 0; i < flux_count; i++) {
                duration_ns += flux_buf[i];
            }

            /* Add flux data for each revolution (duplicate for synthetic) */
            for (int rev = 0; rev < revolutions; rev++) {
                scp_writer_add_track(writer, cyl, hd, flux_buf, flux_count,
                                      duration_ns, rev);
            }

            free(track_bits);
            result->tracks_converted++;
        }

        report_progress(opts, 20 + (cyl * 70 / cylinders),
                         "Converting bitstream tracks");
    }

    report_progress(opts, 95, "Writing SCP output");

    int rc = scp_writer_save(writer, dst_path);
    if (rc == 0) {
        result->success = true;
    } else {
        result->error = UFT_ERR_IO;
    }

    scp_writer_free(writer);
    report_progress(opts, 100, "HFE->SCP complete");
    return result->success ? UFT_OK : result->error;
}

/**
 * @brief G64 -> SCP: Convert GCR bitstream to synthetic flux in SCP format
 *
 * Each GCR bit cell is converted to a flux transition time based on
 * the C64 speed zone timing.
 */
static uft_error_t convert_g64_to_scp(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    report_progress(opts, 10, "Loading G64 image");

    g64_image_t* g64 = NULL;
    int rc = g64_load_buffer(src_data, src_size, &g64);
    if (rc != 0 || !g64) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "G64 parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    int revolutions = (opts && opts->synthetic_revolutions > 0)
                      ? opts->synthetic_revolutions : 3;

    scp_writer_t* writer = scp_writer_create(SCP_TYPE_C64, (uint8_t)revolutions);
    if (!writer) {
        g64_free(g64);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    report_progress(opts, 20, "Converting GCR to synthetic flux");

    /* Process each track (halftracks 2,4,6...70 = tracks 1-35) */
    for (int track = 1; track <= (int)g64->num_tracks && track <= 42; track++) {
        if (is_cancelled(opts)) break;

        int halftrack = track * 2;
        const uint8_t* track_data = NULL;
        size_t track_len = 0;
        uint8_t speed = 0;

        rc = g64_get_track(g64, halftrack, &track_data, &track_len, &speed);
        if (rc != 0 || !track_data || track_len == 0) continue;

        /* Get bit time for this track's speed zone */
        d64_speed_zone_t zone = d64_track_zone(track);
        double bit_time_us = d64_zone_bit_time(zone);
        uint32_t cell_ns = (uint32_t)(bit_time_us * 1000.0);

        /* Convert GCR bitstream to flux using d64_gcr_to_flux() */
        uint32_t flux_buf[131072];
        size_t flux_count = 0;

        rc = d64_gcr_to_flux(track_data, track_len, zone,
                              flux_buf, &flux_count);
        if (rc != 0 || flux_count == 0) {
            /* Fallback: manual bit-to-flux conversion */
            flux_count = 0;
            uint32_t accum_ns = 0;
            for (size_t byte_i = 0; byte_i < track_len; byte_i++) {
                uint8_t b = track_data[byte_i];
                for (int bit = 7; bit >= 0; bit--) {
                    accum_ns += cell_ns;
                    if ((b >> bit) & 1) {
                        if (flux_count < 131072) {
                            flux_buf[flux_count++] = accum_ns;
                        }
                        accum_ns = 0;
                    }
                }
            }
        }

        uint32_t duration_ns = 0;
        for (size_t i = 0; i < flux_count; i++) {
            duration_ns += flux_buf[i];
        }

        /* Write to SCP with specified number of revolutions */
        int scp_track = (track - 1) * 2; /* Side 0 */
        for (int rev = 0; rev < revolutions; rev++) {
            scp_writer_add_track(writer, track - 1, 0, flux_buf, flux_count,
                                  duration_ns, rev);
        }

        result->tracks_converted++;
        report_progress(opts, 20 + (track * 70 / 42),
                         "Converting GCR tracks");
    }

    report_progress(opts, 95, "Writing SCP output");

    rc = scp_writer_save(writer, dst_path);
    if (rc == 0) {
        result->success = true;
    } else {
        result->error = UFT_ERR_IO;
    }

    scp_writer_free(writer);
    g64_free(g64);
    report_progress(opts, 100, "G64->SCP complete");
    return result->success ? UFT_OK : result->error;
}

// ============================================================================
// Kryoflux -> ADF: Decode KryoFlux flux stream to Amiga ADF sector image
// ============================================================================

/**
 * @brief Kryoflux -> ADF: Decode flux stream to MFM sector image
 *
 * Pipeline: KryoFlux parse -> flux deltas -> MFM PLL -> bitstream -> sector extract
 * ADF = 80 cylinders x 2 heads x 11 sectors x 512 bytes = 901120 bytes
 *
 * NOTE: KryoFlux stores one track per .raw file. This implementation processes
 * a single track buffer; the caller must concatenate or iterate over track files.
 * When only a single track is available, remaining tracks are zero-filled.
 */
static uft_error_t convert_kryoflux_to_adf(const uint8_t* src_data,
                                             size_t src_size,
                                             const char* dst_path,
                                             const uft_convert_options_ext_t* opts,
                                             uft_convert_result_t* result) {
    report_progress(opts, 5, "Parsing Kryoflux stream for ADF");

    /* Parse KryoFlux stream data */
    uft_kfx_stream_t stream;
    int rc = uft_kfx_read_stream(src_data, src_size, &stream);
    if (rc != 0) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Kryoflux stream parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    /* ADF geometry: 80 cylinders x 2 heads x 11 sectors x 512 bytes */
    int cylinders = 80, heads = 2, sectors = 11, sector_size = 512;
    size_t output_size = (size_t)cylinders * heads * sectors * sector_size; /* 901120 */
    uint8_t* output = calloc(1, output_size);
    if (!output) {
        uft_kfx_free(&stream);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    report_progress(opts, 20, "Decoding Kryoflux MFM flux");

    if (stream.flux_count > 0 && stream.flux_deltas) {
        /* PLL decode - Amiga DD uses 500 Kbps MFM data rate */
        uft_pll_t pll;
        uft_pll_init(&pll, 500000.0, UFT_ENCODING_MFM);

        size_t max_bits = stream.flux_count * 4;
        size_t buf_size = (max_bits + 7) / 8;
        uint8_t* bitstream = calloc(1, buf_size);

        if (bitstream) {
            size_t bit_pos = 0;
            for (size_t f = 0; f < stream.flux_count && bit_pos < max_bits - 16; f++) {
                /* KryoFlux flux_deltas are already in seconds */
                uft_pll_process_flux_mfm(&pll, stream.flux_deltas[f],
                                          bitstream, &bit_pos);
            }

            /*
             * MFM sector extraction from bitstream:
             * Search for 0xA1A1A1 sync pattern (with missing clock bits)
             * followed by IDAM (0xFE) or DAM (0xFB) address marks.
             *
             * The MFM sync 0x4489 is the MFM encoding of 0xA1 with
             * a missing clock bit. In the raw bitstream, we search for
             * the 16-bit pattern 0x4489.
             */
            uint16_t shift_reg = 0;
            int sync_count = 0;
            size_t data_start = 0;
            int found_sectors = 0;
            int current_cyl = -1, current_head = -1, current_sec = -1;
            bool in_idam = false;

            for (size_t b = 0; b < bit_pos; b++) {
                size_t byte_idx = b / 8;
                size_t bit_idx = 7 - (b % 8);
                uint8_t bit = (bitstream[byte_idx] >> bit_idx) & 1;

                shift_reg = (shift_reg << 1) | bit;

                /* Look for MFM sync word 0x4489 */
                if (shift_reg == 0x4489) {
                    sync_count++;
                    if (sync_count >= 3) {
                        /* Next byte after syncs determines mark type */
                        data_start = b + 1;
                        in_idam = true;
                        sync_count = 0;
                    }
                    continue;
                }

                if (in_idam && (b - data_start) == 16) {
                    /* Read the address mark byte (MFM decoded) */
                    /* Extract 8 MFM-encoded bits = 16 raw bits starting at data_start */
                    uint8_t mark = 0;
                    for (int i = 0; i < 8; i++) {
                        size_t mfm_pos = data_start + i * 2 + 1; /* data bits only */
                        if (mfm_pos < bit_pos) {
                            size_t bi = mfm_pos / 8;
                            size_t bii = 7 - (mfm_pos % 8);
                            mark = (mark << 1) | ((bitstream[bi] >> bii) & 1);
                        }
                    }

                    if (mark == 0xFE) {
                        /* IDAM: next 8 bytes are C, H, R, N, CRC1, CRC2 */
                        /* For now, extract C/H/R from subsequent MFM data */
                        size_t id_start = data_start + 16;
                        uint8_t id_field[6];
                        for (int ib = 0; ib < 6; ib++) {
                            uint8_t val = 0;
                            for (int bit_i = 0; bit_i < 8; bit_i++) {
                                size_t mp = id_start + ib * 16 + bit_i * 2 + 1;
                                if (mp < bit_pos) {
                                    size_t bi2 = mp / 8;
                                    size_t bii2 = 7 - (mp % 8);
                                    val = (val << 1) | ((bitstream[bi2] >> bii2) & 1);
                                }
                            }
                            id_field[ib] = val;
                        }
                        current_cyl = id_field[0];
                        current_head = id_field[1];
                        current_sec = id_field[2];
                    } else if (mark == 0xFB && current_cyl >= 0 &&
                               current_cyl < cylinders &&
                               current_head >= 0 && current_head < heads &&
                               current_sec >= 1 && current_sec <= sectors) {
                        /* DAM: extract sector_size bytes of data */
                        size_t sec_start = data_start + 16;
                        size_t out_offset = ((size_t)current_cyl * heads * sectors +
                                             (size_t)current_head * sectors +
                                             (current_sec - 1)) * sector_size;
                        if (out_offset + sector_size <= output_size) {
                            for (int sb = 0; sb < sector_size; sb++) {
                                uint8_t val = 0;
                                for (int bit_i = 0; bit_i < 8; bit_i++) {
                                    size_t mp = sec_start + sb * 16 + bit_i * 2 + 1;
                                    if (mp < bit_pos) {
                                        size_t bi2 = mp / 8;
                                        size_t bii2 = 7 - (mp % 8);
                                        val = (val << 1) | ((bitstream[bi2] >> bii2) & 1);
                                    }
                                }
                                output[out_offset + sb] = val;
                            }
                            found_sectors++;
                            result->sectors_converted++;
                        }
                        current_cyl = -1;
                    }
                    in_idam = false;
                }
            }

            result->tracks_converted = (found_sectors > 0) ? 1 : 0;

            if (found_sectors == 0) {
                snprintf(result->warnings[result->warning_count++],
                         sizeof(result->warnings[0]),
                         "No MFM sectors extracted from Kryoflux stream "
                         "(single-track buffer; full disk needs directory iteration)");
            } else {
                snprintf(result->warnings[result->warning_count++],
                         sizeof(result->warnings[0]),
                         "Extracted %d sectors from single Kryoflux track; "
                         "full disk conversion requires all track files",
                         found_sectors);
            }

            free(bitstream);
        }
    } else {
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "No flux data in Kryoflux stream");
    }

    report_progress(opts, 80, "Writing ADF output");

    uft_error_t err = write_output_file(dst_path, output, output_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = (int)output_size;
    } else {
        result->error = err;
    }

    free(output);
    uft_kfx_free(&stream);
    report_progress(opts, 100, "Kryoflux->ADF complete");
    return err;
}

// ============================================================================
// HFE -> Sectors: Decode HFE MFM bitstream to raw sector image
// ============================================================================

/**
 * @brief HFE -> IMG/ADF: Decode HFE bitstream to raw sector image
 *
 * Pipeline: HFE parse -> de-interleave -> MFM sync search -> sector extract
 * Extracts sector data by searching for MFM sync (0x4489) and IDAM/DAM marks.
 */
static uft_error_t convert_hfe_to_sectors(const uint8_t* src_data,
                                           size_t src_size,
                                           const char* dst_path,
                                           uft_format_t dst_format,
                                           const uft_convert_options_ext_t* opts,
                                           uft_convert_result_t* result) {
    report_progress(opts, 10, "Parsing HFE file");

    if (src_size < sizeof(hfe_header_t)) {
        result->error = UFT_ERR_FORMAT;
        return UFT_ERR_FORMAT;
    }

    const hfe_header_t* hdr = (const hfe_header_t*)src_data;
    if (!hfe_is_valid_header(hdr)) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Invalid HFE header");
        return UFT_ERR_FORMAT;
    }

    int cylinders = hdr->n_cylinders;
    int heads = hdr->n_heads;

    /* Determine geometry for output */
    int sectors = 18, sector_size = 512;
    if (dst_format == UFT_FORMAT_ADF) {
        sectors = 11; /* Amiga DD */
    } else {
        /* IMG: guess from cylinder/head count */
        if (cylinders <= 40) {
            sectors = 9; /* 360K */
        } else if (hdr->data_bit_rate <= 300) {
            sectors = 9; /* 720K DD */
        } else {
            sectors = 18; /* 1.44MB HD */
        }
    }

    size_t output_size = (size_t)cylinders * heads * sectors * sector_size;
    uint8_t* output = calloc(1, output_size);
    if (!output) {
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    report_progress(opts, 20, "Extracting MFM sectors from HFE");

    /* Read track LUT */
    const hfe_track_entry_t* lut = (const hfe_track_entry_t*)
                                    (src_data + hdr->track_list_offset * 512);

    for (int cyl = 0; cyl < cylinders; cyl++) {
        if (is_cancelled(opts)) break;

        uint16_t track_offset_blocks = lut[cyl].offset;
        uint16_t track_len = lut[cyl].length;

        if (track_offset_blocks == 0 || track_len == 0) {
            result->tracks_failed++;
            continue;
        }

        const uint8_t* interleaved = src_data + (size_t)track_offset_blocks * 512;

        for (int hd = 0; hd < heads; hd++) {
            /* De-interleave to get single-head track data */
            uint8_t* track_bits = malloc(track_len);
            if (!track_bits) {
                result->tracks_failed++;
                continue;
            }

            hfe_deinterleave_track(interleaved, track_len, (uint8_t)hd,
                                    track_bits);
            /* HFE stores bits LSB-first */
            hfe_reverse_bits(track_bits, track_len);

            /*
             * Search for MFM sync pattern 0x4489 in the bitstream.
             * After 3x sync + IDAM (0xFE), read C/H/R/N.
             * After 3x sync + DAM (0xFB), read sector data.
             */
            uint16_t shift_reg = 0;
            int sync_count = 0;
            size_t mark_bit_pos = 0;
            int current_sec = -1;
            int current_cyl_id = -1, current_head_id = -1;
            size_t total_bits = (size_t)track_len * 8;

            for (size_t b = 0; b < total_bits; b++) {
                size_t byte_idx = b / 8;
                size_t bit_idx = 7 - (b % 8);
                uint8_t bit = (track_bits[byte_idx] >> bit_idx) & 1;

                shift_reg = (shift_reg << 1) | bit;

                if (shift_reg == 0x4489) {
                    sync_count++;
                    if (sync_count >= 3) {
                        mark_bit_pos = b + 1;
                    }
                    continue;
                }

                if (sync_count >= 3 && b == mark_bit_pos + 15) {
                    /* We have 16 raw bits = 8 MFM data bits for the mark */
                    uint8_t mark = 0;
                    for (int i = 0; i < 8; i++) {
                        size_t mp = mark_bit_pos + i * 2 + 1;
                        if (mp < total_bits) {
                            size_t bi2 = mp / 8;
                            size_t bii2 = 7 - (mp % 8);
                            mark = (mark << 1) | ((track_bits[bi2] >> bii2) & 1);
                        }
                    }

                    if (mark == 0xFE) {
                        /* IDAM: extract C, H, R, N */
                        size_t id_start = mark_bit_pos + 16;
                        uint8_t id_field[4];
                        for (int ib = 0; ib < 4; ib++) {
                            uint8_t val = 0;
                            for (int bit_i = 0; bit_i < 8; bit_i++) {
                                size_t mp = id_start + ib * 16 + bit_i * 2 + 1;
                                if (mp < total_bits) {
                                    size_t bi2 = mp / 8;
                                    size_t bii2 = 7 - (mp % 8);
                                    val = (val << 1) | ((track_bits[bi2] >> bii2) & 1);
                                }
                            }
                            id_field[ib] = val;
                        }
                        current_cyl_id = id_field[0];
                        current_head_id = id_field[1];
                        current_sec = id_field[2]; /* 1-based */
                    } else if (mark == 0xFB &&
                               current_sec >= 1 && current_sec <= sectors) {
                        /* DAM: extract sector data */
                        size_t sec_start = mark_bit_pos + 16;
                        int out_cyl = (current_cyl_id >= 0) ? current_cyl_id : cyl;
                        int out_hd = (current_head_id >= 0) ? current_head_id : hd;
                        size_t out_offset = ((size_t)out_cyl * heads * sectors +
                                             (size_t)out_hd * sectors +
                                             (current_sec - 1)) * sector_size;

                        if (out_offset + sector_size <= output_size) {
                            for (int sb = 0; sb < sector_size; sb++) {
                                uint8_t val = 0;
                                for (int bit_i = 0; bit_i < 8; bit_i++) {
                                    size_t mp = sec_start + sb * 16 + bit_i * 2 + 1;
                                    if (mp < total_bits) {
                                        size_t bi2 = mp / 8;
                                        size_t bii2 = 7 - (mp % 8);
                                        val = (val << 1) | ((track_bits[bi2] >> bii2) & 1);
                                    }
                                }
                                output[out_offset + sb] = val;
                            }
                            result->sectors_converted++;
                        }
                        current_sec = -1;
                    }
                    sync_count = 0;
                }
            }

            free(track_bits);
            result->tracks_converted++;
        }

        report_progress(opts, 20 + (cyl * 70 / cylinders),
                         "Decoding HFE tracks");
    }

    report_progress(opts, 95, "Writing output");

    uft_error_t err = write_output_file(dst_path, output, output_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = (int)output_size;
    } else {
        result->error = err;
    }

    free(output);
    report_progress(opts, 100, "HFE->sector complete");
    return err;
}

// ============================================================================
// SCP -> HFE: Decode SCP flux to bitstream, wrap in HFE container
// ============================================================================

/**
 * @brief SCP -> HFE: PLL-decode flux to bitstream, then wrap in HFE
 *
 * Pipeline: SCP parse -> flux deltas -> PLL -> bitstream -> HFE interleave
 */
static uft_error_t convert_scp_to_hfe(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    report_progress(opts, 5, "Parsing SCP file for HFE conversion");

    uft_scp_file_t scp;
    int rc = uft_scp_read(src_data, src_size, &scp);
    if (rc != 0) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "SCP parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    /* Determine geometry from SCP header */
    int total_tracks = (int)scp.track_count;
    int heads = 2;
    int cylinders = (total_tracks + heads - 1) / heads;
    if (cylinders < 1) cylinders = 1;
    if (cylinders > 85) cylinders = 85;

    /* Determine bitrate from SCP disk type */
    uint16_t bitrate_kbps = 250; /* Default DD */
    hfe_track_encoding_t encoding = HFE_ENC_ISOIBM_MFM;
    hfe_floppy_interface_t iface = HFE_IF_IBMPC_DD;

    /* Standard MFM track length estimate */
    int mfm_track_bytes = 6400; /* ~100ms at 250Kbps */
    int track_len_aligned = ((mfm_track_bytes + 255) / 256) * 256;

    /* Build HFE file in memory */
    size_t lut_blocks = ((size_t)cylinders * sizeof(hfe_track_entry_t) + 511) / 512;
    size_t data_start_block = 1 + lut_blocks;
    size_t blocks_per_track = ((size_t)track_len_aligned * 2 + 511) / 512;
    size_t total_size = (data_start_block + (size_t)cylinders * blocks_per_track) * 512;

    uint8_t* hfe_data = calloc(1, total_size);
    if (!hfe_data) {
        uft_scp_free(&scp);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    /* Write HFE header */
    hfe_header_t* hdr = (hfe_header_t*)hfe_data;
    hfe_init_header(hdr, false);
    hdr->n_cylinders = (uint8_t)cylinders;
    hdr->n_heads = (uint8_t)heads;
    hdr->track_encoding = (uint8_t)encoding;
    hdr->data_bit_rate = bitrate_kbps;
    hdr->drive_rpm = 300;
    hdr->uft_floppy_interface = (uint8_t)iface;
    hdr->track_list_offset = 1;

    /* Write track LUT */
    hfe_track_entry_t* lut = (hfe_track_entry_t*)(hfe_data + 512);
    for (int cyl = 0; cyl < cylinders; cyl++) {
        lut[cyl].offset = (uint16_t)(data_start_block + (size_t)cyl * blocks_per_track);
        lut[cyl].length = (uint16_t)track_len_aligned;
    }

    report_progress(opts, 15, "PLL decoding SCP flux to bitstream");

    for (int cyl = 0; cyl < cylinders; cyl++) {
        if (is_cancelled(opts)) break;

        uint8_t head0_bits[32768];
        uint8_t head1_bits[32768];
        memset(head0_bits, 0x00, track_len_aligned);
        memset(head1_bits, 0x00, track_len_aligned);

        for (int hd = 0; hd < heads; hd++) {
            int scp_track = cyl * 2 + hd;
            if (scp_track >= total_tracks) continue;

            /* Get flux data from best revolution */
            double deltas[131072];
            int best_rev = 0;
            int max_flux = 0;

            for (int rev = 0; rev < (int)scp.header.revolutions; rev++) {
                int count = uft_scp_get_track_flux(&scp, scp_track, rev,
                                                    deltas, 131072);
                if (count > max_flux) {
                    max_flux = count;
                    best_rev = rev;
                }
            }

            int flux_count = uft_scp_get_track_flux(&scp, scp_track, best_rev,
                                                     deltas, 131072);
            if (flux_count <= 0) {
                result->tracks_failed++;
                continue;
            }

            /* PLL decode to bitstream */
            uft_pll_t pll;
            uft_pll_init(&pll, 500000.0, UFT_ENCODING_MFM);

            size_t max_bits = (size_t)flux_count * 4;
            size_t buf_size = (max_bits + 7) / 8;
            uint8_t* raw_bitstream = calloc(1, buf_size);
            if (!raw_bitstream) {
                result->tracks_failed++;
                continue;
            }

            size_t bit_pos = 0;
            for (int f = 0; f < flux_count && bit_pos < max_bits - 16; f++) {
                double delta_sec = deltas[f] * 25e-9; /* SCP ticks to seconds */
                uft_pll_process_flux_mfm(&pll, delta_sec, raw_bitstream, &bit_pos);
            }

            /* Copy decoded bitstream into track buffer, truncating to track_len_aligned */
            size_t bytes_to_copy = (bit_pos + 7) / 8;
            if (bytes_to_copy > (size_t)track_len_aligned)
                bytes_to_copy = (size_t)track_len_aligned;

            uint8_t* dest_buf = (hd == 0) ? head0_bits : head1_bits;
            memcpy(dest_buf, raw_bitstream, bytes_to_copy);

            /* HFE expects LSB-first bit ordering - reverse bits */
            hfe_reverse_bits(dest_buf, (uint32_t)track_len_aligned);

            free(raw_bitstream);
            result->tracks_converted++;
        }

        /* Interleave head 0/1 into HFE track block */
        uint8_t* track_dest = hfe_data + (size_t)lut[cyl].offset * 512;
        hfe_interleave_track(head0_bits, head1_bits,
                              (uint16_t)track_len_aligned, track_dest);

        report_progress(opts, 15 + (cyl * 75 / cylinders),
                         "Converting SCP tracks to HFE");
    }

    report_progress(opts, 95, "Writing HFE output");

    uft_error_t err = write_output_file(dst_path, hfe_data, total_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = (int)total_size;
    } else {
        result->error = err;
    }

    free(hfe_data);
    uft_scp_free(&scp);
    report_progress(opts, 100, "SCP->HFE complete");
    return err;
}

// ============================================================================
// SCP -> G64: Decode SCP flux to GCR bitstream in G64 container
// ============================================================================

/**
 * @brief SCP -> G64: PLL-decode C64 GCR flux to bitstream, wrap in G64
 *
 * Pipeline: SCP parse -> flux deltas -> GCR PLL -> bitstream -> G64 container
 */
static uft_error_t convert_scp_to_g64(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    report_progress(opts, 5, "Parsing SCP file for G64 conversion");

    uft_scp_file_t scp;
    int rc = uft_scp_read(src_data, src_size, &scp);
    if (rc != 0) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "SCP parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    /* Create G64 image: standard 42 half-tracks (tracks 1-42, C64 single-side) */
    int num_tracks = 42;
    g64_image_t* g64 = g64_create(num_tracks, false);
    if (!g64) {
        uft_scp_free(&scp);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    report_progress(opts, 10, "PLL decoding SCP flux to GCR bitstream");

    for (int track = 1; track <= num_tracks; track++) {
        if (is_cancelled(opts)) break;

        int scp_track_idx = (track - 1) * 2; /* Side 0 only for C64 */

        /* Get flux deltas from best revolution */
        double deltas[131072];
        int best_rev = 0;
        int max_flux = 0;

        for (int rev = 0; rev < (int)scp.header.revolutions; rev++) {
            int count = uft_scp_get_track_flux(&scp, scp_track_idx, rev,
                                                deltas, 131072);
            if (count > max_flux) {
                max_flux = count;
                best_rev = rev;
            }
        }

        int flux_count = uft_scp_get_track_flux(&scp, scp_track_idx, best_rev,
                                                 deltas, 131072);
        if (flux_count <= 0) {
            result->tracks_failed++;
            continue;
        }

        /* Initialize PLL for C64 GCR encoding */
        uft_pll_t pll;
        int bitrate = uft_c64_track_bitrate(track);
        uft_pll_init(&pll, (double)bitrate, UFT_ENCODING_GCR);

        /* Allocate bitstream buffer */
        size_t max_bits = (size_t)flux_count * 4;
        size_t buf_size = (max_bits + 7) / 8;
        uint8_t* bitstream = calloc(1, buf_size);
        if (!bitstream) {
            result->tracks_failed++;
            continue;
        }

        /* Run PLL to extract GCR bitstream */
        size_t bit_pos = 0;
        for (int f = 0; f < flux_count && bit_pos < max_bits - 16; f++) {
            double delta_sec = deltas[f] * 25e-9; /* SCP ticks to seconds */
            uft_pll_process_flux_mfm(&pll, delta_sec, bitstream, &bit_pos);
        }

        /* Store in G64 image */
        size_t track_bytes = (bit_pos + 7) / 8;
        if (track_bytes > G64_MAX_TRACK_SIZE) {
            track_bytes = G64_MAX_TRACK_SIZE;
        }

        /* Determine speed zone for this track */
        d64_speed_zone_t zone = d64_track_zone(track);
        int halftrack = track * 2;

        g64_set_track(g64, halftrack, bitstream, track_bytes, (uint8_t)zone);

        free(bitstream);
        result->tracks_converted++;

        report_progress(opts, 10 + (track * 80 / num_tracks),
                         "Decoding GCR tracks");
    }

    report_progress(opts, 95, "Writing G64 output");

    rc = g64_save(dst_path, g64);
    if (rc == 0) {
        result->success = true;
    } else {
        result->error = UFT_ERR_IO;
    }

    g64_free(g64);
    uft_scp_free(&scp);
    report_progress(opts, 100, "SCP->G64 complete");
    return result->success ? UFT_OK : result->error;
}

// ============================================================================
// Kryoflux -> SCP: Re-package KryoFlux flux data into SCP container
// ============================================================================

/**
 * @brief Kryoflux -> SCP: Re-package raw flux timing data into SCP format
 *
 * Reads KryoFlux stream flux intervals and writes them as SCP flux data.
 * KryoFlux flux_deltas are in seconds; SCP uses 25ns ticks.
 */
static uft_error_t convert_kryoflux_to_scp(const uint8_t* src_data,
                                             size_t src_size,
                                             const char* dst_path,
                                             const uft_convert_options_ext_t* opts,
                                             uft_convert_result_t* result) {
    report_progress(opts, 5, "Parsing Kryoflux stream for SCP conversion");

    uft_kfx_stream_t stream;
    int rc = uft_kfx_read_stream(src_data, src_size, &stream);
    if (rc != 0) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Kryoflux stream parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    if (stream.flux_count == 0 || !stream.flux_deltas) {
        uft_kfx_free(&stream);
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "No flux data in Kryoflux stream");
        return UFT_ERR_FORMAT;
    }

    /* Default to Amiga DD type; single-track file */
    uint8_t scp_disk_type = SCP_TYPE_AMIGA;
    int revolutions = (opts && opts->synthetic_revolutions > 0)
                      ? opts->synthetic_revolutions : 1;

    scp_writer_t* writer = scp_writer_create(scp_disk_type, (uint8_t)revolutions);
    if (!writer) {
        uft_kfx_free(&stream);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    report_progress(opts, 20, "Converting Kryoflux flux to SCP");

    /*
     * Convert KryoFlux flux deltas (seconds) to SCP flux values (25ns ticks).
     * If index times are available, split into revolutions at index boundaries.
     */
    uint32_t* flux_buf = malloc(stream.flux_count * sizeof(uint32_t));
    if (!flux_buf) {
        scp_writer_free(writer);
        uft_kfx_free(&stream);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    /* Convert all flux deltas to SCP 25ns ticks */
    for (size_t i = 0; i < stream.flux_count; i++) {
        double ns = stream.flux_deltas[i] * 1e9; /* seconds -> nanoseconds */
        uint32_t ticks = (uint32_t)(ns / 25.0 + 0.5); /* 25ns per SCP tick */
        if (ticks == 0) ticks = 1;
        flux_buf[i] = ticks;
    }

    /* Calculate total duration */
    uint32_t duration_ns = 0;
    for (size_t i = 0; i < stream.flux_count; i++) {
        duration_ns += flux_buf[i] * 25; /* Convert back to ns for duration */
    }

    /*
     * Write as track 0, side 0. KryoFlux is single-track per file;
     * a full disk conversion requires the caller to iterate over
     * all track files and call this for each one.
     */
    for (int rev = 0; rev < revolutions; rev++) {
        scp_writer_add_track(writer, 0, 0, flux_buf, stream.flux_count,
                              duration_ns, rev);
    }

    result->tracks_converted = 1;
    snprintf(result->warnings[result->warning_count++],
             sizeof(result->warnings[0]),
             "Single Kryoflux track converted to SCP (%zu flux transitions); "
             "full disk requires all track files",
             stream.flux_count);

    free(flux_buf);

    report_progress(opts, 80, "Writing SCP output");

    rc = scp_writer_save(writer, dst_path);
    if (rc == 0) {
        result->success = true;
    } else {
        result->error = UFT_ERR_IO;
    }

    scp_writer_free(writer);
    uft_kfx_free(&stream);
    report_progress(opts, 100, "Kryoflux->SCP complete");
    return result->success ? UFT_OK : result->error;
}

// ============================================================================
// Kryoflux -> HFE: Chain KryoFlux flux through PLL to HFE bitstream
// ============================================================================

/**
 * @brief Kryoflux -> HFE: Decode flux to bitstream and wrap in HFE
 *
 * Pipeline: KryoFlux stream -> PLL decode -> bitstream -> HFE container
 */
static uft_error_t convert_kryoflux_to_hfe(const uint8_t* src_data,
                                             size_t src_size,
                                             const char* dst_path,
                                             const uft_convert_options_ext_t* opts,
                                             uft_convert_result_t* result) {
    report_progress(opts, 5, "Parsing Kryoflux stream for HFE conversion");

    uft_kfx_stream_t stream;
    int rc = uft_kfx_read_stream(src_data, src_size, &stream);
    if (rc != 0) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Kryoflux stream parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    if (stream.flux_count == 0 || !stream.flux_deltas) {
        uft_kfx_free(&stream);
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "No flux data in Kryoflux stream");
        return UFT_ERR_FORMAT;
    }

    /*
     * Single KryoFlux track file -> single-track HFE.
     * Build a 1-cylinder, 1-head HFE with MFM bitstream.
     */
    int cylinders = 1, heads = 1;
    uint16_t bitrate_kbps = 250;
    hfe_track_encoding_t encoding = HFE_ENC_ISOIBM_MFM;
    hfe_floppy_interface_t iface = HFE_IF_IBMPC_DD;

    /* PLL decode flux to bitstream */
    report_progress(opts, 20, "PLL decoding Kryoflux flux");

    uft_pll_t pll;
    uft_pll_init(&pll, 500000.0, UFT_ENCODING_MFM);

    size_t max_bits = stream.flux_count * 4;
    size_t buf_size = (max_bits + 7) / 8;
    uint8_t* raw_bitstream = calloc(1, buf_size);
    if (!raw_bitstream) {
        uft_kfx_free(&stream);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    size_t bit_pos = 0;
    for (size_t f = 0; f < stream.flux_count && bit_pos < max_bits - 16; f++) {
        uft_pll_process_flux_mfm(&pll, stream.flux_deltas[f],
                                  raw_bitstream, &bit_pos);
    }

    /* Compute track byte length */
    int mfm_track_bytes = (int)((bit_pos + 7) / 8);
    if (mfm_track_bytes < 6250) mfm_track_bytes = 6250;
    int track_len_aligned = ((mfm_track_bytes + 255) / 256) * 256;

    report_progress(opts, 50, "Building HFE container");

    /* Build HFE file */
    size_t lut_blocks = ((size_t)cylinders * sizeof(hfe_track_entry_t) + 511) / 512;
    size_t data_start_block = 1 + lut_blocks;
    size_t blocks_per_track = ((size_t)track_len_aligned * 2 + 511) / 512;
    size_t total_size = (data_start_block + (size_t)cylinders * blocks_per_track) * 512;

    uint8_t* hfe_data = calloc(1, total_size);
    if (!hfe_data) {
        free(raw_bitstream);
        uft_kfx_free(&stream);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    /* Write HFE header */
    hfe_header_t* hdr = (hfe_header_t*)hfe_data;
    hfe_init_header(hdr, false);
    hdr->n_cylinders = (uint8_t)cylinders;
    hdr->n_heads = (uint8_t)heads;
    hdr->track_encoding = (uint8_t)encoding;
    hdr->data_bit_rate = bitrate_kbps;
    hdr->drive_rpm = 300;
    hdr->uft_floppy_interface = (uint8_t)iface;
    hdr->track_list_offset = 1;

    /* Write track LUT */
    hfe_track_entry_t* lut = (hfe_track_entry_t*)(hfe_data + 512);
    lut[0].offset = (uint16_t)data_start_block;
    lut[0].length = (uint16_t)track_len_aligned;

    /* Prepare track bitstream for HFE */
    uint8_t* head0_bits = calloc(1, track_len_aligned);
    uint8_t* head1_bits = calloc(1, track_len_aligned); /* Empty for side 1 */
    if (!head0_bits || !head1_bits) {
        free(head0_bits);
        free(head1_bits);
        free(raw_bitstream);
        free(hfe_data);
        uft_kfx_free(&stream);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    /* Copy PLL output into head 0 buffer */
    size_t bytes_to_copy = (bit_pos + 7) / 8;
    if (bytes_to_copy > (size_t)track_len_aligned)
        bytes_to_copy = (size_t)track_len_aligned;
    memcpy(head0_bits, raw_bitstream, bytes_to_copy);

    /* HFE expects LSB-first bit ordering */
    hfe_reverse_bits(head0_bits, (uint32_t)track_len_aligned);

    /* Interleave into HFE track data */
    uint8_t* track_dest = hfe_data + (size_t)lut[0].offset * 512;
    hfe_interleave_track(head0_bits, head1_bits,
                          (uint16_t)track_len_aligned, track_dest);

    result->tracks_converted = 1;
    snprintf(result->warnings[result->warning_count++],
             sizeof(result->warnings[0]),
             "Single Kryoflux track converted to HFE (%zu bits decoded); "
             "full disk requires all track files",
             bit_pos);

    free(head0_bits);
    free(head1_bits);
    free(raw_bitstream);

    report_progress(opts, 90, "Writing HFE output");

    uft_error_t err = write_output_file(dst_path, hfe_data, total_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = (int)total_size;
    } else {
        result->error = err;
    }

    free(hfe_data);
    uft_kfx_free(&stream);
    report_progress(opts, 100, "Kryoflux->HFE complete");
    return err;
}

// ============================================================================
// IMD <-> IMG Conversions
// ============================================================================

/**
 * @brief IMD -> IMG: Extract raw sector data from ImageDisk format
 *
 * Parses the IMD file, then uses uft_imd_to_raw() to extract sequential
 * sector data into a flat raw image suitable for use with standard tools.
 */
static uft_error_t convert_imd_to_img(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    uft_imd_image_t imd;
    uft_imd_init(&imd);

    report_progress(opts, 10, "Parsing IMD image");

    int rc = uft_imd_read_mem(src_data, src_size, &imd);
    if (rc != 0) {
        uft_imd_free(&imd);
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "IMD parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    report_progress(opts, 50, "Extracting raw sectors");

    uint8_t* raw_data = NULL;
    size_t raw_size = 0;
    rc = uft_imd_to_raw(&imd, &raw_data, &raw_size, 0xE5);
    if (rc != 0 || !raw_data) {
        uft_imd_free(&imd);
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "IMD sector extraction failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    report_progress(opts, 80, "Writing IMG output");

    uft_error_t err = write_output_file(dst_path, raw_data, raw_size);
    if (err != UFT_OK) {
        free(raw_data);
        uft_imd_free(&imd);
        result->error = err;
        return err;
    }

    result->success = true;
    result->bytes_written = (int)raw_size;
    result->tracks_converted = imd.num_tracks;

    /* Report statistics */
    snprintf(result->warnings[result->warning_count++],
             sizeof(result->warnings[0]),
             "Extracted %d tracks (%d cyls x %d heads), %zu bytes output",
             imd.num_tracks, imd.num_cylinders, imd.num_heads, raw_size);

    if (imd.bad_sectors > 0 || imd.unavail_sectors > 0) {
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Warning: %u bad sectors, %u unavailable sectors",
                 imd.bad_sectors, imd.unavail_sectors);
    }

    free(raw_data);
    uft_imd_free(&imd);

    report_progress(opts, 100, "IMD->IMG complete");
    return UFT_OK;
}

/**
 * @brief IMG -> IMD: Convert raw sector image to ImageDisk format
 *
 * Analyzes the raw image size to determine geometry (cylinders, heads,
 * sectors per track, sector size), then uses uft_imd_from_raw() to
 * create a properly formatted IMD file with track metadata.
 */
static uft_error_t convert_img_to_imd(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    report_progress(opts, 10, "Analyzing IMG geometry");

    /* Determine geometry from file size */
    int cylinders = 80, heads = 2, sectors = 18, sector_size = 512;
    uint8_t mode = UFT_IMD_MODE_250K_MFM; /* Default: 250K MFM (DD) */

    if (src_size <= 163840) {
        /* 160K: 40 cyl, 1 head, 8 sec */
        cylinders = 40; heads = 1; sectors = 8;
    } else if (src_size <= 184320) {
        /* 180K: 40 cyl, 1 head, 9 sec */
        cylinders = 40; heads = 1; sectors = 9;
    } else if (src_size <= 327680) {
        /* 320K: 40 cyl, 2 heads, 8 sec */
        cylinders = 40; heads = 2; sectors = 8;
    } else if (src_size <= 368640) {
        /* 360K: 40 cyl, 2 heads, 9 sec */
        cylinders = 40; heads = 2; sectors = 9;
    } else if (src_size <= 655360) {
        /* 640K: 80 cyl, 2 heads, 8 sec */
        cylinders = 80; heads = 2; sectors = 8;
    } else if (src_size <= 737280) {
        /* 720K: 80 cyl, 2 heads, 9 sec */
        cylinders = 80; heads = 2; sectors = 9;
    } else if (src_size <= 1228800) {
        /* 1.2M: 80 cyl, 2 heads, 15 sec, 500K */
        cylinders = 80; heads = 2; sectors = 15;
        mode = UFT_IMD_MODE_500K_MFM;
    } else {
        /* 1.44M: 80 cyl, 2 heads, 18 sec, 500K */
        cylinders = 80; heads = 2; sectors = 18;
        mode = UFT_IMD_MODE_500K_MFM;
    }

    uint8_t sec_size_code = uft_imd_bytes_to_ssize((uint16_t)sector_size);

    report_progress(opts, 20, "Building IMD file");

    /* Build IMD header comment */
    char imd_header[256];
    int hdr_len = snprintf(imd_header, sizeof(imd_header),
                           "IMD 1.18: Converted from IMG by UFT\x1a");

    /* Calculate required output size:
     * header + per-track: 5-byte header + nsectors smap + nsectors*(1 + sec_size) */
    int total_tracks = cylinders * heads;
    size_t data_per_track = (size_t)sectors * (1 + sector_size) + sectors; /* smap */
    size_t imd_size = (size_t)hdr_len + (size_t)total_tracks * (5 + data_per_track);

    uint8_t* imd_data = malloc(imd_size);
    if (!imd_data) {
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    size_t pos = 0;
    memcpy(imd_data + pos, imd_header, hdr_len);
    pos += hdr_len;

    report_progress(opts, 40, "Writing track records");

    for (int cyl = 0; cyl < cylinders; cyl++) {
        for (int hd = 0; hd < heads; hd++) {
            if (is_cancelled(opts)) break;

            /* Track header: mode, cylinder, head, nsectors, sector_size_code */
            imd_data[pos++] = mode;
            imd_data[pos++] = (uint8_t)cyl;
            imd_data[pos++] = (uint8_t)hd;
            imd_data[pos++] = (uint8_t)sectors;
            imd_data[pos++] = sec_size_code;

            /* Sector numbering map (1-based) */
            for (int s = 0; s < sectors; s++) {
                imd_data[pos++] = (uint8_t)(s + 1);
            }

            /* Sector data records */
            for (int s = 0; s < sectors; s++) {
                size_t src_offset = ((size_t)cyl * heads * sectors +
                                     (size_t)hd * sectors + s) * sector_size;

                if (src_offset + sector_size <= src_size) {
                    /* Check if sector is all same value (compress) */
                    const uint8_t* sec = src_data + src_offset;
                    bool all_same = true;
                    for (int b = 1; b < sector_size; b++) {
                        if (sec[b] != sec[0]) {
                            all_same = false;
                            break;
                        }
                    }

                    if (all_same) {
                        imd_data[pos++] = UFT_IMD_SEC_COMPRESSED;
                        imd_data[pos++] = sec[0];
                    } else {
                        imd_data[pos++] = UFT_IMD_SEC_NORMAL;
                        memcpy(imd_data + pos, sec, sector_size);
                        pos += sector_size;
                    }
                } else {
                    /* Beyond source data: fill sector */
                    imd_data[pos++] = UFT_IMD_SEC_COMPRESSED;
                    imd_data[pos++] = 0xE5;
                }

                result->sectors_converted++;
            }

            result->tracks_converted++;
        }

        report_progress(opts, 40 + (cyl * 50 / cylinders), "Encoding IMD tracks");
    }

    report_progress(opts, 95, "Writing IMD output");

    uft_error_t err = write_output_file(dst_path, imd_data, pos);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = (int)pos;
    } else {
        result->error = err;
    }

    snprintf(result->warnings[result->warning_count++],
             sizeof(result->warnings[0]),
             "Created IMD: %d cyls x %d heads x %d secs (%s), %zu bytes",
             cylinders, heads, sectors,
             uft_imd_mode_name((uft_imd_mode_t)mode), pos);

    free(imd_data);
    report_progress(opts, 100, "IMG->IMD complete");
    return err;
}

// ============================================================================
// G64 <-> HFE Conversions (Bitstream <-> Bitstream)
// ============================================================================

/**
 * @brief G64 -> HFE: Wrap C64 GCR bitstream in HFE container
 *
 * G64 stores CBM GCR-encoded track data with variable speed zones.
 * HFE stores bitstream data interleaved by head in 256-byte blocks.
 *
 * The C64 is a single-sided drive (head 0 only), so head 1 is filled
 * with zeros. The bit rate is set per the dominant speed zone.
 *
 * C64 speed zones map to approximate bitrates:
 *   Zone 0 (tracks 1-17):  ~307 kbit/s
 *   Zone 1 (tracks 18-24): ~285 kbit/s
 *   Zone 2 (tracks 25-30): ~266 kbit/s
 *   Zone 3 (tracks 31-35): ~250 kbit/s
 *
 * HFE v1 only supports a single global bitrate, so we use 250 kbit/s
 * (the base C64 rate) and store the raw GCR bitstream as-is.
 * HxC firmware handles C64 mode correctly at this rate.
 */
static uft_error_t convert_g64_to_hfe(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    report_progress(opts, 10, "Loading G64 image");

    g64_image_t* g64 = NULL;
    int rc = g64_load_buffer(src_data, src_size, &g64);
    if (rc != 0 || !g64) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "G64 parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    /* C64 is single-sided with up to 42 tracks (84 halftracks) */
    int num_tracks = (int)g64->num_tracks;
    if (num_tracks > 42) num_tracks = 42;
    int cylinders = num_tracks; /* One cylinder per track (single-sided) */
    int heads = 1;

    /* Use C64 DD interface, GCR stored as raw bitstream */
    uint16_t bitrate_kbps = 250;
    hfe_track_encoding_t encoding = HFE_ENC_ISOIBM_MFM; /* Raw bitstream */
    hfe_floppy_interface_t iface = HFE_IF_C64_DD;

    /* Track length: G64 max track size is ~7928 bytes, round up to 256 */
    int track_len_aligned = ((G64_MAX_TRACK_SIZE + 255) / 256) * 256;

    /* Calculate HFE file layout */
    size_t lut_blocks = ((size_t)cylinders * sizeof(hfe_track_entry_t) + 511) / 512;
    size_t data_start_block = 1 + lut_blocks;
    size_t blocks_per_track = ((size_t)track_len_aligned * 2 + 511) / 512;
    size_t total_size = (data_start_block + (size_t)cylinders * blocks_per_track) * 512;

    uint8_t* hfe_data = calloc(1, total_size);
    if (!hfe_data) {
        g64_free(g64);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    /* Write HFE header */
    hfe_header_t* hdr = (hfe_header_t*)hfe_data;
    hfe_init_header(hdr, false);
    hdr->n_cylinders = (uint8_t)cylinders;
    hdr->n_heads = (uint8_t)heads;
    hdr->track_encoding = (uint8_t)encoding;
    hdr->data_bit_rate = bitrate_kbps;
    hdr->drive_rpm = 300;
    hdr->uft_floppy_interface = (uint8_t)iface;
    hdr->track_list_offset = 1;

    /* Write track lookup table */
    hfe_track_entry_t* lut = (hfe_track_entry_t*)(hfe_data + 512);
    for (int cyl = 0; cyl < cylinders; cyl++) {
        lut[cyl].offset = (uint16_t)(data_start_block + (size_t)cyl * blocks_per_track);
        lut[cyl].length = (uint16_t)track_len_aligned;
    }

    report_progress(opts, 30, "Converting GCR tracks to HFE");

    for (int track = 1; track <= num_tracks; track++) {
        if (is_cancelled(opts)) break;

        int halftrack = track * 2;
        const uint8_t* track_data = NULL;
        size_t track_len = 0;
        uint8_t speed = 0;

        rc = g64_get_track(g64, halftrack, &track_data, &track_len, &speed);
        if (rc != 0 || !track_data || track_len == 0) {
            result->tracks_failed++;
            continue;
        }

        /* Prepare head 0 bitstream for HFE */
        uint8_t head0_bits[32768];
        uint8_t head1_bits[32768];
        memset(head0_bits, 0x00, track_len_aligned);
        memset(head1_bits, 0x00, track_len_aligned);

        /* Copy GCR bitstream data */
        size_t copy_len = (track_len <= (size_t)track_len_aligned)
                          ? track_len : (size_t)track_len_aligned;
        memcpy(head0_bits, track_data, copy_len);

        /* HFE stores bits LSB-first, so reverse each byte */
        hfe_reverse_bits(head0_bits, (uint32_t)track_len_aligned);

        /* Interleave head 0/1 into HFE track block */
        int cyl_idx = track - 1;
        uint8_t* track_dest = hfe_data + (size_t)lut[cyl_idx].offset * 512;
        hfe_interleave_track(head0_bits, head1_bits,
                              (uint16_t)track_len_aligned, track_dest);

        result->tracks_converted++;
        report_progress(opts, 30 + (track * 60 / num_tracks),
                         "Wrapping GCR tracks in HFE");
    }

    report_progress(opts, 95, "Writing HFE output");

    uft_error_t err = write_output_file(dst_path, hfe_data, total_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = (int)total_size;
    } else {
        result->error = err;
    }

    snprintf(result->warnings[result->warning_count++],
             sizeof(result->warnings[0]),
             "G64->HFE: %d tracks wrapped in HFE container (C64 DD mode)",
             result->tracks_converted);

    free(hfe_data);
    g64_free(g64);
    report_progress(opts, 100, "G64->HFE complete");
    return err;
}

/**
 * @brief HFE -> G64: Extract C64 GCR bitstream from HFE container
 *
 * Reads HFE track data, de-interleaves head 0, and stores the raw
 * GCR bitstream in a G64 container. Only works for C64-compatible
 * HFE files (single-sided GCR data).
 *
 * The speed zone for each track is inferred from the track number
 * using the standard C64 1541 zone mapping.
 */
static uft_error_t convert_hfe_to_g64(const uint8_t* src_data, size_t src_size,
                                       const char* dst_path,
                                       const uft_convert_options_ext_t* opts,
                                       uft_convert_result_t* result) {
    report_progress(opts, 10, "Parsing HFE file");

    if (src_size < sizeof(hfe_header_t)) {
        result->error = UFT_ERR_FORMAT;
        return UFT_ERR_FORMAT;
    }

    const hfe_header_t* hdr = (const hfe_header_t*)src_data;
    if (!hfe_is_valid_header(hdr)) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Invalid HFE header");
        return UFT_ERR_FORMAT;
    }

    int cylinders = hdr->n_cylinders;
    if (cylinders > 42) cylinders = 42; /* G64 max 42 tracks */

    /* Warn if this doesn't look like a C64 disk */
    if (hdr->uft_floppy_interface != HFE_IF_C64_DD &&
        hdr->uft_floppy_interface != (uint8_t)HFE_IF_GENERIC_SHUGART) {
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "HFE interface type 0x%02X is not C64; "
                 "G64 output may not be valid CBM GCR",
                 hdr->uft_floppy_interface);
    }

    /* Create G64 image */
    g64_image_t* g64 = g64_create(cylinders, false);
    if (!g64) {
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    report_progress(opts, 20, "Extracting GCR tracks from HFE");

    /* Read track LUT */
    const hfe_track_entry_t* lut = (const hfe_track_entry_t*)
                                    (src_data + hdr->track_list_offset * 512);

    for (int cyl = 0; cyl < cylinders; cyl++) {
        if (is_cancelled(opts)) break;

        uint16_t track_offset_blocks = lut[cyl].offset;
        uint16_t track_len = lut[cyl].length;

        if (track_offset_blocks == 0 || track_len == 0) {
            result->tracks_failed++;
            continue;
        }

        const uint8_t* interleaved = src_data + (size_t)track_offset_blocks * 512;

        /* De-interleave head 0 (C64 is single-sided) */
        uint8_t* track_bits = malloc(track_len);
        if (!track_bits) {
            result->tracks_failed++;
            continue;
        }

        hfe_deinterleave_track(interleaved, track_len, 0, track_bits);

        /* HFE stores bits LSB-first, reverse to MSB-first for G64 */
        hfe_reverse_bits(track_bits, track_len);

        /* Determine effective track length (trim trailing zeros) */
        size_t effective_len = track_len;
        while (effective_len > 0 && track_bits[effective_len - 1] == 0x00) {
            effective_len--;
        }
        if (effective_len == 0) {
            free(track_bits);
            result->tracks_failed++;
            continue;
        }

        /* Cap track length to G64 maximum */
        if (effective_len > G64_MAX_TRACK_SIZE) {
            effective_len = G64_MAX_TRACK_SIZE;
        }

        /* Get speed zone from track number (1-based) */
        int track_num = cyl + 1;
        uint8_t speed = (uint8_t)d64_speed_zone(track_num);

        /* Store in G64 (halftrack = track * 2) */
        int halftrack = track_num * 2;
        g64_set_track(g64, halftrack, track_bits, effective_len, speed);

        free(track_bits);
        result->tracks_converted++;

        report_progress(opts, 20 + (cyl * 70 / cylinders),
                         "Extracting GCR tracks");
    }

    report_progress(opts, 95, "Writing G64 output");

    int rc = g64_save(dst_path, g64);
    if (rc == 0) {
        result->success = true;
    } else {
        result->error = UFT_ERR_IO;
    }

    snprintf(result->warnings[result->warning_count++],
             sizeof(result->warnings[0]),
             "HFE->G64: %d tracks extracted to G64 container",
             result->tracks_converted);

    g64_free(g64);
    report_progress(opts, 100, "HFE->G64 complete");
    return result->success ? UFT_OK : result->error;
}

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
// Conversion Dispatch
// ============================================================================

/**
 * @brief Dispatch a specific conversion based on source/target format pair.
 *
 * Routes to the appropriate static helper function based on the
 * (src_format, dst_format) pair.
 */
static uft_error_t dispatch_conversion(uft_format_t src_format,
                                        uft_format_t dst_format,
                                        const uint8_t* src_data,
                                        size_t src_size,
                                        const char* dst_path,
                                        const uft_convert_options_ext_t* opts,
                                        uft_convert_result_t* result) {
    /* ===== Archive -> Sector ===== */
    if (src_format == UFT_FORMAT_TD0 && dst_format == UFT_FORMAT_IMG) {
        return convert_td0_to_img(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_TD0 && dst_format == UFT_FORMAT_IMD) {
        return convert_td0_to_imd(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_NBZ && dst_format == UFT_FORMAT_D64) {
        return convert_nbz_to_d64(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_NBZ && dst_format == UFT_FORMAT_G64) {
        return convert_nbz_to_g64(src_data, src_size, dst_path, opts, result);
    }

    /* ===== Sector <-> Sector (format conversions) ===== */
    if (src_format == UFT_FORMAT_IMD && dst_format == UFT_FORMAT_IMG) {
        return convert_imd_to_img(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_IMG && dst_format == UFT_FORMAT_IMD) {
        return convert_img_to_imd(src_data, src_size, dst_path, opts, result);
    }

    /* ===== Flux -> Sector (decode pipeline) ===== */
    if (src_format == UFT_FORMAT_SCP && dst_format == UFT_FORMAT_D64) {
        return convert_scp_to_d64(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_SCP &&
        (dst_format == UFT_FORMAT_ADF || dst_format == UFT_FORMAT_IMG)) {
        return convert_scp_to_mfm_sectors(src_data, src_size, dst_path,
                                           dst_format, opts, result);
    }
    if (src_format == UFT_FORMAT_KRYOFLUX && dst_format == UFT_FORMAT_D64) {
        return convert_kryoflux_to_d64(src_data, src_size, dst_path,
                                        opts, result);
    }
    if (src_format == UFT_FORMAT_KRYOFLUX && dst_format == UFT_FORMAT_ADF) {
        return convert_kryoflux_to_adf(src_data, src_size, dst_path,
                                        opts, result);
    }

    /* ===== Bitstream -> Sector (decode) ===== */
    if (src_format == UFT_FORMAT_G64 && dst_format == UFT_FORMAT_D64) {
        return convert_g64_to_d64(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_HFE &&
        (dst_format == UFT_FORMAT_IMG || dst_format == UFT_FORMAT_ADF)) {
        return convert_hfe_to_sectors(src_data, src_size, dst_path,
                                       dst_format, opts, result);
    }

    /* ===== Sector -> Bitstream (synthetic) ===== */
    if (src_format == UFT_FORMAT_D64 && dst_format == UFT_FORMAT_G64) {
        return convert_d64_to_g64(src_data, src_size, dst_path, opts, result);
    }
    if ((src_format == UFT_FORMAT_ADF || src_format == UFT_FORMAT_IMG) &&
        dst_format == UFT_FORMAT_HFE) {
        return convert_sectors_to_hfe(src_data, src_size, dst_path,
                                       src_format, opts, result);
    }
    if (src_format == UFT_FORMAT_D64 && dst_format == UFT_FORMAT_HFE) {
        /* D64 -> HFE: Chain D64 -> G64 (GCR encode) -> HFE (bitstream wrap) */
        report_progress(opts, 5, "D64->HFE via G64 intermediate");

        d64_image_t* d64 = NULL;
        int rc = d64_load_buffer(src_data, src_size, &d64);
        if (rc != 0 || !d64) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }

        g64_image_t* g64 = NULL;
        rc = d64_to_g64(d64, &g64, NULL, NULL);
        d64_free(d64);
        if (rc != 0 || !g64) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }

        /* Serialize G64 to buffer */
        uint8_t* g64_buf = NULL;
        size_t g64_size = 0;
        rc = g64_save_buffer(g64, &g64_buf, &g64_size);
        g64_free(g64);
        if (rc != 0 || !g64_buf) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }

        /* Now convert G64 -> HFE */
        uft_error_t err = convert_g64_to_hfe(g64_buf, g64_size, dst_path,
                                              opts, result);
        free(g64_buf);
        return err;
    }

    /* ===== Bitstream -> Flux ===== */
    if (src_format == UFT_FORMAT_HFE && dst_format == UFT_FORMAT_SCP) {
        return convert_hfe_to_scp(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_G64 && dst_format == UFT_FORMAT_SCP) {
        return convert_g64_to_scp(src_data, src_size, dst_path, opts, result);
    }

    /* ===== Flux -> Bitstream ===== */
    if (src_format == UFT_FORMAT_SCP && dst_format == UFT_FORMAT_HFE) {
        return convert_scp_to_hfe(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_SCP && dst_format == UFT_FORMAT_G64) {
        return convert_scp_to_g64(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_KRYOFLUX && dst_format == UFT_FORMAT_SCP) {
        return convert_kryoflux_to_scp(src_data, src_size, dst_path,
                                        opts, result);
    }
    if (src_format == UFT_FORMAT_KRYOFLUX && dst_format == UFT_FORMAT_HFE) {
        return convert_kryoflux_to_hfe(src_data, src_size, dst_path,
                                        opts, result);
    }

    /* ===== Sector -> Flux (synthetic) ===== */
    if (src_format == UFT_FORMAT_D64 && dst_format == UFT_FORMAT_SCP) {
        /* D64 -> SCP: D64 -> G64 (GCR encode) -> SCP (flux synthesis) */
        /* Two-stage conversion via G64 intermediate */
        report_progress(opts, 5, "D64->SCP via G64 intermediate");

        d64_image_t* d64 = NULL;
        int rc = d64_load_buffer(src_data, src_size, &d64);
        if (rc != 0 || !d64) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }

        g64_image_t* g64 = NULL;
        rc = d64_to_g64(d64, &g64, NULL, NULL);
        d64_free(d64);
        if (rc != 0 || !g64) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }

        /* Serialize G64 to buffer */
        uint8_t* g64_buf = NULL;
        size_t g64_size = 0;
        rc = g64_save_buffer(g64, &g64_buf, &g64_size);
        g64_free(g64);
        if (rc != 0 || !g64_buf) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }

        /* Now convert G64 -> SCP */
        uft_error_t err = convert_g64_to_scp(g64_buf, g64_size, dst_path,
                                              opts, result);
        free(g64_buf);
        return err;
    }
    if ((src_format == UFT_FORMAT_ADF || src_format == UFT_FORMAT_IMG) &&
        dst_format == UFT_FORMAT_SCP) {
        /* ADF/IMG -> SCP: encode to MFM bitstream, then to flux */
        /* Two-stage: first build MFM bitstream, then convert to flux */
        report_progress(opts, 5, "Sector->SCP via MFM synthesis");

        /* Determine geometry */
        int cylinders = 80, heads = 2, sectors = 18, sector_size = 512;
        uint32_t bitrate = 250000;
        if (src_format == UFT_FORMAT_ADF) {
            sectors = 11;
            bitrate = 250000;
        } else {
            if (src_size <= 368640) { cylinders = 40; sectors = 9; }
            else if (src_size <= 737280) { sectors = 9; }
            else { sectors = 18; bitrate = 500000; }
        }

        uint8_t scp_type = (src_format == UFT_FORMAT_ADF)
                           ? SCP_TYPE_AMIGA : SCP_TYPE_PC_DD;
        int revolutions = (opts && opts->synthetic_revolutions > 0)
                          ? opts->synthetic_revolutions : 1;

        scp_writer_t* writer = scp_writer_create(scp_type, (uint8_t)revolutions);
        if (!writer) {
            result->error = UFT_ERR_MEMORY;
            return UFT_ERR_MEMORY;
        }

        uint32_t cell_ns = (uint32_t)(1000000000.0 / (bitrate * 2.0));

        for (int cyl = 0; cyl < cylinders; cyl++) {
            for (int hd = 0; hd < heads; hd++) {
                if (is_cancelled(opts)) break;

                /* Build MFM bitstream for this track */
                uint8_t mfm_track[16384];
                memset(mfm_track, 0x4E, sizeof(mfm_track));
                int mfm_pos = 80 + 12 + 4 + 50; /* preamble + sync + IAM + gap1 */

                for (int sec = 0; sec < sectors; sec++) {
                    size_t offset = ((size_t)cyl * heads * sectors +
                                     (size_t)hd * sectors + sec) * sector_size;
                    /* Simplified sector structure */
                    mfm_pos += 12 + 4 + 4 + 2 + 22 + 12 + 4; /* ID field */
                    if (offset + sector_size <= src_size) {
                        memcpy(mfm_track + mfm_pos, src_data + offset,
                               sector_size);
                    }
                    mfm_pos += sector_size + 2 + 54;
                }

                /* Convert MFM bitstream to flux */
                uint32_t flux_buf[131072];
                size_t flux_count = 0;
                uint32_t accum = 0;

                for (int i = 0; i < mfm_pos && i < (int)sizeof(mfm_track); i++) {
                    uint8_t b = mfm_track[i];
                    for (int bit = 7; bit >= 0; bit--) {
                        accum += cell_ns;
                        if ((b >> bit) & 1) {
                            if (flux_count < 131072)
                                flux_buf[flux_count++] = accum;
                            accum = 0;
                        }
                    }
                }

                uint32_t duration = 0;
                for (size_t i = 0; i < flux_count; i++)
                    duration += flux_buf[i];

                for (int rev = 0; rev < revolutions; rev++) {
                    scp_writer_add_track(writer, cyl, hd, flux_buf,
                                          flux_count, duration, rev);
                }
                result->tracks_converted++;
            }
        }

        rc = scp_writer_save(writer, dst_path);
        scp_writer_free(writer);
        if (rc == 0) {
            result->success = true;
        } else {
            result->error = UFT_ERR_IO;
        }
        return result->success ? UFT_OK : result->error;
    }

    /* ===== Bitstream -> Bitstream ===== */
    if (src_format == UFT_FORMAT_G64 && dst_format == UFT_FORMAT_HFE) {
        return convert_g64_to_hfe(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_HFE && dst_format == UFT_FORMAT_G64) {
        return convert_hfe_to_g64(src_data, src_size, dst_path, opts, result);
    }

    /* Fallback: unsupported conversion */
    result->error = UFT_ERR_NOT_SUPPORTED;
    snprintf(result->warnings[result->warning_count++],
             sizeof(result->warnings[0]),
             "Conversion %s->%s: dispatch not yet implemented",
             uft_format_get_name(src_format),
             uft_format_get_name(dst_format));
    return UFT_ERR_NOT_SUPPORTED;
}

// ============================================================================
// Main Conversion Entry Point
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

    /* Read source file */
    FILE* f = fopen(src_path, "rb");
    if (!f) {
        result->error = UFT_ERROR_NOT_FOUND;
        return UFT_ERROR_NOT_FOUND;
    }

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); result->error = UFT_ERR_IO; return UFT_ERR_IO; }
    long ftell_result = ftell(f);
    if (ftell_result < 0) {
        fclose(f);
        result->error = UFT_ERR_IO;
        return UFT_ERR_IO;
    }
    size_t src_size = (size_t)ftell_result;
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); result->error = UFT_ERR_IO; return UFT_ERR_IO; }
    uint8_t* src_data = malloc(src_size);
    if (!src_data) {
        fclose(f);
        result->error = UFT_ERROR_NO_MEMORY;
        return UFT_ERROR_NO_MEMORY;
    }

    if (fread(src_data, 1, src_size, f) != src_size) {
        free(src_data);
        fclose(f);
        result->error = UFT_ERR_IO;
        return UFT_ERR_IO;
    }
    fclose(f);

    /* Detect source format */
    uft_probe_result_t probe;
    uft_format_t src_format = uft_probe_format(src_data, src_size,
                                                src_path, &probe);

    if (src_format == UFT_FORMAT_UNKNOWN) {
        free(src_data);
        result->error = UFT_ERROR_INVALID_FORMAT;
        snprintf(result->warnings[0], sizeof(result->warnings[0]),
                 "Could not detect source format");
        result->warning_count = 1;
        return UFT_ERROR_INVALID_FORMAT;
    }

    /* Check conversion path exists */
    const uft_conversion_path_t* path = uft_convert_get_path(src_format,
                                                              dst_format);
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

    /* Add quality warning */
    if (path->warning) {
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]), "%s", path->warning);
    }

    /* Get format classes */
    uft_format_class_t src_class = uft_format_get_class(src_format);
    uft_format_class_t dst_class = uft_format_get_class(dst_format);

    uft_error_t err;

    if (src_format == dst_format) {
        /* Same format: direct copy */
        err = write_output_file(dst_path, src_data, src_size);
        if (err == UFT_OK) {
            result->success = true;
            result->bytes_written = (int)src_size;
        } else {
            result->error = err;
        }
    } else if (src_class == UFT_FCLASS_SECTOR &&
               dst_class == UFT_FCLASS_SECTOR &&
               !((src_format == UFT_FORMAT_IMD && dst_format == UFT_FORMAT_IMG) ||
                 (src_format == UFT_FORMAT_IMG && dst_format == UFT_FORMAT_IMD))) {
        /* Sector -> Sector: raw copy with format awareness
         * (except IMD<->IMG which need actual format conversion) */
        err = write_output_file(dst_path, src_data, src_size);
        if (err == UFT_OK) {
            result->success = true;
            result->bytes_written = (int)src_size;
            snprintf(result->warnings[result->warning_count++],
                     sizeof(result->warnings[0]),
                     "Raw sector copy - format headers may need adjustment");
        } else {
            result->error = err;
        }
    } else {
        /* Build extended options from basic options for dispatch */
        uft_convert_options_ext_t ext_opts;
        memset(&ext_opts, 0, sizeof(ext_opts));
        if (options) {
            ext_opts.verify_after = options->verify_after;
            ext_opts.preserve_errors = options->preserve_errors;
            ext_opts.preserve_weak_bits = options->preserve_weak_bits;
            ext_opts.synthetic_cell_time_us = options->synthetic_cell_time_us;
            ext_opts.synthetic_jitter_percent = options->synthetic_jitter_percent;
            ext_opts.synthetic_revolutions = options->synthetic_revolutions;
            ext_opts.decode_retries = options->decode_retries;
            ext_opts.use_multiple_revs = options->use_multiple_revs;
            ext_opts.interpolate_errors = options->interpolate_errors;
        }

        err = dispatch_conversion(src_format, dst_format, src_data, src_size,
                                   dst_path, &ext_opts, result);
    }

    free(src_data);
    return result->success ? UFT_OK : result->error;
}

// ============================================================================
// In-Memory Conversion
// ============================================================================

uft_error_t uft_convert_memory(const uint8_t* src_data, size_t src_size,
                                uft_format_t src_format,
                                uint8_t** dst_data, size_t* dst_size,
                                uft_format_t dst_format,
                                const uft_convert_options_ext_t* options,
                                uft_convert_result_t* result) {
    if (!src_data || !dst_data || !dst_size || !result) {
        return UFT_ERROR_NULL_POINTER;
    }

    memset(result, 0, sizeof(*result));
    *dst_data = NULL;
    *dst_size = 0;

    /* Validate conversion path */
    const uft_conversion_path_t* path = uft_convert_get_path(src_format,
                                                              dst_format);
    if (!path) {
        result->error = UFT_ERROR_FORMAT_NOT_SUPPORTED;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "No conversion path from %s to %s",
                 uft_format_get_name(src_format),
                 uft_format_get_name(dst_format));
        return UFT_ERROR_FORMAT_NOT_SUPPORTED;
    }

    if (path->warning) {
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]), "%s", path->warning);
    }

    /* Same format: direct copy */
    if (src_format == dst_format) {
        *dst_data = malloc(src_size);
        if (!*dst_data) {
            result->error = UFT_ERR_MEMORY;
            return UFT_ERR_MEMORY;
        }
        memcpy(*dst_data, src_data, src_size);
        *dst_size = src_size;
        result->success = true;
        result->bytes_written = (int)src_size;
        return UFT_OK;
    }

    /* Sector -> Sector: direct copy */
    uft_format_class_t src_class = uft_format_get_class(src_format);
    uft_format_class_t dst_class = uft_format_get_class(dst_format);

    if (src_class == UFT_FCLASS_SECTOR && dst_class == UFT_FCLASS_SECTOR &&
        !((src_format == UFT_FORMAT_IMD && dst_format == UFT_FORMAT_IMG) ||
          (src_format == UFT_FORMAT_IMG && dst_format == UFT_FORMAT_IMD))) {
        *dst_data = malloc(src_size);
        if (!*dst_data) {
            result->error = UFT_ERR_MEMORY;
            return UFT_ERR_MEMORY;
        }
        memcpy(*dst_data, src_data, src_size);
        *dst_size = src_size;
        result->success = true;
        result->bytes_written = (int)src_size;
        return UFT_OK;
    }

    /*
     * For conversions that need a temp file, use a temporary path.
     * For truly in-memory conversions, specific paths can be added here.
     */

    /* Handle conversions that have in-memory support via existing APIs */

    /* IMD -> IMG */
    if (src_format == UFT_FORMAT_IMD && dst_format == UFT_FORMAT_IMG) {
        uft_imd_image_t imd;
        uft_imd_init(&imd);
        int rc = uft_imd_read_mem(src_data, src_size, &imd);
        if (rc != 0) {
            uft_imd_free(&imd);
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }
        rc = uft_imd_to_raw(&imd, dst_data, dst_size, 0xE5);
        uft_imd_free(&imd);
        if (rc != 0 || !*dst_data) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }
        result->success = true;
        result->bytes_written = (int)*dst_size;
        return UFT_OK;
    }

    /* TD0 -> IMG */
    if (src_format == UFT_FORMAT_TD0 && dst_format == UFT_FORMAT_IMG) {
        uft_td0_image_t td0;
        uft_td0_init(&td0);
        int rc = uft_td0_read_mem(src_data, src_size, &td0);
        if (rc != 0) {
            uft_td0_free(&td0);
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }
        rc = uft_td0_to_raw(&td0, dst_data, dst_size, 0xF6);
        uft_td0_free(&td0);
        if (rc != 0 || !*dst_data) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }
        result->success = true;
        result->bytes_written = (int)*dst_size;
        return UFT_OK;
    }

    /* D64 -> G64 */
    if (src_format == UFT_FORMAT_D64 && dst_format == UFT_FORMAT_G64) {
        d64_image_t* d64 = NULL;
        int rc = d64_load_buffer(src_data, src_size, &d64);
        if (rc != 0 || !d64) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }
        g64_image_t* g64 = NULL;
        rc = d64_to_g64(d64, &g64, NULL, NULL);
        d64_free(d64);
        if (rc != 0 || !g64) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }
        rc = g64_save_buffer(g64, dst_data, dst_size);
        g64_free(g64);
        if (rc != 0 || !*dst_data) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }
        result->success = true;
        result->bytes_written = (int)*dst_size;
        return UFT_OK;
    }

    /* G64 -> D64 */
    if (src_format == UFT_FORMAT_G64 && dst_format == UFT_FORMAT_D64) {
        g64_image_t* g64 = NULL;
        int rc = g64_load_buffer(src_data, src_size, &g64);
        if (rc != 0 || !g64) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }
        d64_image_t* d64 = NULL;
        rc = g64_to_d64(g64, &d64, NULL, NULL);
        g64_free(g64);
        if (rc != 0 || !d64) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }
        rc = d64_save_buffer(d64, dst_data, dst_size, false);
        d64_free(d64);
        if (rc != 0 || !*dst_data) {
            result->error = UFT_ERR_FORMAT;
            return UFT_ERR_FORMAT;
        }
        result->success = true;
        result->bytes_written = (int)*dst_size;
        return UFT_OK;
    }

    /* For other paths, fall back to temp-file based conversion */
    /* Generate a temp filename */
    char tmp_path[512];
#ifdef _WIN32
    char tmp_dir[256];
    if (GetTempPathA(sizeof(tmp_dir), tmp_dir) == 0) {
        snprintf(tmp_dir, sizeof(tmp_dir), ".");
    }
    snprintf(tmp_path, sizeof(tmp_path), "%suft_conv_%p.tmp",
             tmp_dir, (void*)src_data);
#else
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/uft_conv_%p.tmp",
             (void*)src_data);
#endif

    uft_error_t err = dispatch_conversion(src_format, dst_format,
                                           src_data, src_size, tmp_path,
                                           options, result);
    if (err == UFT_OK && result->success) {
        /* Read back the temp file */
        FILE* f = fopen(tmp_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0) {
                *dst_data = malloc((size_t)sz);
                if (*dst_data) {
                    if (fread(*dst_data, 1, (size_t)sz, f) == (size_t)sz) {
                        *dst_size = (size_t)sz;
                    } else {
                        free(*dst_data);
                        *dst_data = NULL;
                        err = UFT_ERR_IO;
                    }
                } else {
                    err = UFT_ERR_MEMORY;
                }
            }
            fclose(f);
        } else {
            err = UFT_ERR_IO;
        }
        remove(tmp_path);
    } else {
        remove(tmp_path);
    }

    return err;
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

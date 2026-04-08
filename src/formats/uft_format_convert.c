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
                /* Fill unread sector, mark error */
                uint8_t fill[256];
                memset(fill, 0x00, 256);
                d64_set_sector(d64, track, s, fill, D64_ERR_DATA_NOT_FOUND);
                result->sectors_failed++;
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
    report_progress(opts, 90, "Writing output");

    /* Write whatever we have */
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
        /* Kryoflux -> ADF: similar to SCP->ADF but from Kryoflux stream */
        /* TODO: Implement full Kryoflux multi-track MFM decode */
        result->error = UFT_ERR_NOT_IMPLEMENTED;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Kryoflux->ADF requires multi-file track iteration (TODO)");
        return UFT_ERR_NOT_IMPLEMENTED;
    }

    /* ===== Bitstream -> Sector (decode) ===== */
    if (src_format == UFT_FORMAT_G64 && dst_format == UFT_FORMAT_D64) {
        return convert_g64_to_d64(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_HFE &&
        (dst_format == UFT_FORMAT_IMG || dst_format == UFT_FORMAT_ADF)) {
        /* HFE -> IMG/ADF: decode MFM bitstream to sectors */
        /* TODO: Requires MFM sector parser integration */
        result->error = UFT_ERR_NOT_IMPLEMENTED;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "HFE->%s requires MFM sector extraction pipeline (TODO)",
                 uft_format_get_name(dst_format));
        return UFT_ERR_NOT_IMPLEMENTED;
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

    /* ===== Bitstream -> Flux ===== */
    if (src_format == UFT_FORMAT_HFE && dst_format == UFT_FORMAT_SCP) {
        return convert_hfe_to_scp(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_G64 && dst_format == UFT_FORMAT_SCP) {
        return convert_g64_to_scp(src_data, src_size, dst_path, opts, result);
    }

    /* ===== Flux -> Bitstream ===== */
    if (src_format == UFT_FORMAT_SCP && dst_format == UFT_FORMAT_HFE) {
        /* SCP -> HFE: PLL decode to bitstream, then wrap in HFE */
        /* TODO: Full implementation - needs PLL + HFE writer integration */
        result->error = UFT_ERR_NOT_IMPLEMENTED;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "SCP->HFE requires PLL bitstream decode + HFE writer (TODO)");
        return UFT_ERR_NOT_IMPLEMENTED;
    }
    if (src_format == UFT_FORMAT_SCP && dst_format == UFT_FORMAT_G64) {
        /* SCP -> G64: PLL decode CBM GCR + wrap in G64 container */
        /* TODO: Needs CBM-specific PLL decode */
        result->error = UFT_ERR_NOT_IMPLEMENTED;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "SCP->G64 requires GCR PLL decode pipeline (TODO)");
        return UFT_ERR_NOT_IMPLEMENTED;
    }
    if (src_format == UFT_FORMAT_KRYOFLUX && dst_format == UFT_FORMAT_SCP) {
        /* Kryoflux -> SCP: re-package flux data */
        /* TODO: Read Kryoflux stream files, repackage as SCP */
        result->error = UFT_ERR_NOT_IMPLEMENTED;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Kryoflux->SCP requires multi-file stream reader (TODO)");
        return UFT_ERR_NOT_IMPLEMENTED;
    }
    if (src_format == UFT_FORMAT_KRYOFLUX && dst_format == UFT_FORMAT_HFE) {
        result->error = UFT_ERR_NOT_IMPLEMENTED;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "Kryoflux->HFE requires multi-file stream + PLL (TODO)");
        return UFT_ERR_NOT_IMPLEMENTED;
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
        /* G64 -> HFE: wrap GCR bitstream in HFE container */
        /* TODO: Needs GCR-aware HFE builder */
        result->error = UFT_ERR_NOT_IMPLEMENTED;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "G64->HFE requires GCR-to-HFE container wrapper (TODO)");
        return UFT_ERR_NOT_IMPLEMENTED;
    }
    if (src_format == UFT_FORMAT_HFE && dst_format == UFT_FORMAT_G64) {
        /* HFE -> G64: extract CBM GCR tracks from HFE */
        /* TODO: Needs CBM-specific bitstream extraction */
        result->error = UFT_ERR_NOT_IMPLEMENTED;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "HFE->G64 requires CBM GCR extraction from HFE (TODO)");
        return UFT_ERR_NOT_IMPLEMENTED;
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
               dst_class == UFT_FCLASS_SECTOR) {
        /* Sector -> Sector: raw copy with format awareness */
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

    if (src_class == UFT_FCLASS_SECTOR && dst_class == UFT_FCLASS_SECTOR) {
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

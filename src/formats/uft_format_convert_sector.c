/**
 * @file uft_format_convert_sector.c
 * @brief Sector-level format converters.
 *
 * Contains converters between sector formats and bitstream/sector:
 *   - G64 -> D64 (bitstream to sector)
 *   - D64 -> G64 (sector to bitstream)
 *   - IMD -> IMG (sector to sector)
 *   - IMG -> IMD (sector to sector)
 */

#include "uft_format_convert_internal.h"

// ============================================================================
// Bitstream -> Sector (decode)
// ============================================================================

/**
 * @brief G64 -> D64: Decode GCR bitstream to sector data
 *
 * Uses the g64_to_d64() converter from uft_d64_g64.h.
 */
uft_error_t uftc_convert_g64_to_d64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uftc_report_progress(opts, 10, "Loading G64 image");

    g64_image_t* g64 = NULL;
    int rc = g64_load_buffer(src_data, src_size, &g64);
    if (rc != 0 || !g64) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "G64 parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    uftc_report_progress(opts, 30, "Converting G64 GCR to D64 sectors");

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

    uftc_report_progress(opts, 80, "Writing D64 output");

    rc = d64_save(dst_path, d64, false);
    if (rc == 0) {
        result->success = true;
        result->tracks_converted = conv_result.tracks_converted;
        result->sectors_converted = conv_result.sectors_converted;
    } else {
        result->error = UFT_ERR_IO;
    }

    d64_free(d64);
    uftc_report_progress(opts, 100, "G64->D64 complete");
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
uft_error_t uftc_convert_d64_to_g64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uftc_report_progress(opts, 10, "Loading D64 image");

    d64_image_t* d64 = NULL;
    int rc = d64_load_buffer(src_data, src_size, &d64);
    if (rc != 0 || !d64) {
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "D64 parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    uftc_report_progress(opts, 30, "Encoding D64 sectors to GCR");

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

    uftc_report_progress(opts, 80, "Writing G64 output");

    rc = g64_save(dst_path, g64);
    if (rc == 0) {
        result->success = true;
        result->tracks_converted = conv_result.tracks_converted;
        result->sectors_converted = conv_result.sectors_converted;
    } else {
        result->error = UFT_ERR_IO;
    }

    g64_free(g64);
    uftc_report_progress(opts, 100, "D64->G64 complete");
    return result->success ? UFT_OK : result->error;
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
uft_error_t uftc_convert_imd_to_img(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uft_imd_image_t imd;
    uft_imd_init(&imd);

    uftc_report_progress(opts, 10, "Parsing IMD image");

    int rc = uft_imd_read_mem(src_data, src_size, &imd);
    if (rc != 0) {
        uft_imd_free(&imd);
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "IMD parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    uftc_report_progress(opts, 50, "Extracting raw sectors");

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

    uftc_report_progress(opts, 80, "Writing IMG output");

    uft_error_t err = uftc_write_output_file(dst_path, raw_data, raw_size);
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

    uftc_report_progress(opts, 100, "IMD->IMG complete");
    return UFT_OK;
}

/**
 * @brief IMG -> IMD: Convert raw sector image to ImageDisk format
 *
 * Analyzes the raw image size to determine geometry (cylinders, heads,
 * sectors per track, sector size), then uses uft_imd_from_raw() to
 * create a properly formatted IMD file with track metadata.
 */
uft_error_t uftc_convert_img_to_imd(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uftc_report_progress(opts, 10, "Analyzing IMG geometry");

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

    uftc_report_progress(opts, 20, "Building IMD file");

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

    uftc_report_progress(opts, 40, "Writing track records");

    for (int cyl = 0; cyl < cylinders; cyl++) {
        for (int hd = 0; hd < heads; hd++) {
            if (uftc_is_cancelled(opts)) break;

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

        uftc_report_progress(opts, 40 + (cyl * 50 / cylinders), "Encoding IMD tracks");
    }

    uftc_report_progress(opts, 95, "Writing IMD output");

    uft_error_t err = uftc_write_output_file(dst_path, imd_data, pos);
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
    uftc_report_progress(opts, 100, "IMG->IMD complete");
    return err;
}

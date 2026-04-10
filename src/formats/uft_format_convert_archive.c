/**
 * @file uft_format_convert_archive.c
 * @brief Archive format converters: TD0, NBZ decompression.
 *
 * Contains converters for archive/compressed formats to sector images:
 *   - TD0 -> IMG
 *   - TD0 -> IMD
 *   - NBZ -> D64
 *   - NBZ -> G64
 */

#include "uft_format_convert_internal.h"

// ============================================================================
// Archive -> Sector Conversions
// ============================================================================

/**
 * @brief TD0 -> IMG: Decompress Teledisk to raw sector image
 *
 * Uses uft_td0_read_mem() to parse/decompress, then uft_td0_to_raw()
 * to extract the raw sector data.
 */
uft_error_t uftc_convert_td0_to_img(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uft_td0_image_t td0_img;
    uft_td0_init(&td0_img);

    uftc_report_progress(opts, 10, "Parsing TD0 image");

    int rc = uft_td0_read_mem(src_data, src_size, &td0_img);
    if (rc != 0) {
        uft_td0_free(&td0_img);
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "TD0 parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    uftc_report_progress(opts, 50, "Extracting raw sectors");

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

    uftc_report_progress(opts, 80, "Writing IMG output");

    /* Write output file */
    uft_error_t err = uftc_write_output_file(dst_path, raw_data, raw_size);
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

    uftc_report_progress(opts, 100, "TD0->IMG complete");
    return UFT_OK;
}

/**
 * @brief TD0 -> IMD: Convert Teledisk to ImageDisk (preserves metadata)
 */
uft_error_t uftc_convert_td0_to_imd(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uft_td0_image_t td0_img;
    uft_td0_init(&td0_img);

    uftc_report_progress(opts, 10, "Parsing TD0 image");

    int rc = uft_td0_read_mem(src_data, src_size, &td0_img);
    if (rc != 0) {
        uft_td0_free(&td0_img);
        result->error = UFT_ERR_FORMAT;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "TD0 parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    uftc_report_progress(opts, 40, "Converting TD0 to IMD format");

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

    uftc_report_progress(opts, 80, "Writing IMD output");

    uft_error_t err = uftc_write_output_file(dst_path, imd_data, pos);
    result->bytes_written = (int)pos;
    if (err == UFT_OK) {
        result->success = true;
    } else {
        result->error = err;
    }

    free(imd_data);
    free(raw_data);
    uft_td0_free(&td0_img);

    uftc_report_progress(opts, 100, "TD0->IMD complete");
    return err;
}

/**
 * @brief NBZ -> D64: Decompress LZ77/LZSS nibble archive to D64
 *
 * NBZ is a gzip-compressed D64/G64 file. The first bytes after
 * decompression reveal the inner format.
 */
uft_error_t uftc_convert_nbz_to_d64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uftc_report_progress(opts, 10, "Decompressing NBZ archive");

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

    uftc_report_progress(opts, 80, "Writing D64 output");

    uft_error_t err = uftc_write_output_file(dst_path, decompressed, decomp_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = decomp_size;
        int num_tracks = (decomp_size >= 196608) ? 40 : 35;
        result->tracks_converted = num_tracks;
    } else {
        result->error = err;
    }

    free(decompressed);
    uftc_report_progress(opts, 100, "NBZ->D64 complete");
    return err;
}

/**
 * @brief NBZ -> G64: Decompress NBZ archive to G64 bitstream image
 */
uft_error_t uftc_convert_nbz_to_g64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uftc_report_progress(opts, 10, "Decompressing NBZ for G64");

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

    uftc_report_progress(opts, 50, "Checking decompressed format");

    /* Check if decompressed data is already G64 (starts with "GCR-1541") */
    if (decomp_size >= 12 &&
        memcmp(decompressed, G64_SIGNATURE, G64_SIGNATURE_LEN) == 0) {
        /* Already a G64 - write directly */
        uftc_report_progress(opts, 80, "Writing G64 output");
        uft_error_t err = uftc_write_output_file(dst_path, decompressed, decomp_size);
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

    uftc_report_progress(opts, 60, "Converting D64 to G64");

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
    uftc_report_progress(opts, 100, "NBZ->G64 complete");
    return result->success ? UFT_OK : result->error;
}

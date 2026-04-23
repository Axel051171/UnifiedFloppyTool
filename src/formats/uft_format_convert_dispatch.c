/**
 * @file uft_format_convert_dispatch.c
 * @brief Format conversion dispatch engine and public API.
 *
 * Contains the conversion dispatch function, public API entry points
 * (uft_convert_file, uft_convert_memory), and utility API functions
 * (uft_format_get_class, uft_format_get_name, etc.).
 */

#include "uft_format_convert_internal.h"

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
    for (size_t i = 0; i < g_num_conversion_paths; i++) {
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

    for (size_t i = 0; i < g_num_conversion_paths && count < max; i++) {
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
 * Routes to the appropriate converter function based on the
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
        return uftc_convert_td0_to_img(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_TD0 && dst_format == UFT_FORMAT_IMD) {
        return uftc_convert_td0_to_imd(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_NBZ && dst_format == UFT_FORMAT_D64) {
        return uftc_convert_nbz_to_d64(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_NBZ && dst_format == UFT_FORMAT_G64) {
        return uftc_convert_nbz_to_g64(src_data, src_size, dst_path, opts, result);
    }

    /* ===== Sector <-> Sector (format conversions) ===== */
    if (src_format == UFT_FORMAT_IMD && dst_format == UFT_FORMAT_IMG) {
        return uftc_convert_imd_to_img(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_IMG && dst_format == UFT_FORMAT_IMD) {
        return uftc_convert_img_to_imd(src_data, src_size, dst_path, opts, result);
    }

    /* ===== Flux -> Sector (decode pipeline) ===== */
    if (src_format == UFT_FORMAT_SCP && dst_format == UFT_FORMAT_D64) {
        return uftc_convert_scp_to_d64(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_SCP &&
        (dst_format == UFT_FORMAT_ADF || dst_format == UFT_FORMAT_IMG)) {
        return uftc_convert_scp_to_mfm_sectors(src_data, src_size, dst_path,
                                                dst_format, opts, result);
    }
    if (src_format == UFT_FORMAT_KRYOFLUX && dst_format == UFT_FORMAT_D64) {
        return uftc_convert_kryoflux_to_d64(src_data, src_size, dst_path,
                                             opts, result);
    }
    if (src_format == UFT_FORMAT_KRYOFLUX && dst_format == UFT_FORMAT_ADF) {
        return uftc_convert_kryoflux_to_adf(src_data, src_size, dst_path,
                                             opts, result);
    }

    /* ===== Bitstream -> Sector (decode) ===== */
    if (src_format == UFT_FORMAT_G64 && dst_format == UFT_FORMAT_D64) {
        return uftc_convert_g64_to_d64(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_HFE &&
        (dst_format == UFT_FORMAT_IMG || dst_format == UFT_FORMAT_ADF)) {
        return uftc_convert_hfe_to_sectors(src_data, src_size, dst_path,
                                            dst_format, opts, result);
    }

    /* ===== Sector -> Bitstream (synthetic) ===== */
    if (src_format == UFT_FORMAT_D64 && dst_format == UFT_FORMAT_G64) {
        return uftc_convert_d64_to_g64(src_data, src_size, dst_path, opts, result);
    }
    if ((src_format == UFT_FORMAT_ADF || src_format == UFT_FORMAT_IMG) &&
        dst_format == UFT_FORMAT_HFE) {
        return uftc_convert_sectors_to_hfe(src_data, src_size, dst_path,
                                            src_format, opts, result);
    }
    if (src_format == UFT_FORMAT_D64 && dst_format == UFT_FORMAT_HFE) {
        /* D64 -> HFE: Chain D64 -> G64 (GCR encode) -> HFE (bitstream wrap) */
        uftc_report_progress(opts, 5, "D64->HFE via G64 intermediate");

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
        uft_error_t err = uftc_convert_g64_to_hfe(g64_buf, g64_size, dst_path,
                                                    opts, result);
        free(g64_buf);
        return err;
    }

    /* ===== Bitstream -> Flux ===== */
    if (src_format == UFT_FORMAT_HFE && dst_format == UFT_FORMAT_SCP) {
        return uftc_convert_hfe_to_scp(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_G64 && dst_format == UFT_FORMAT_SCP) {
        return uftc_convert_g64_to_scp(src_data, src_size, dst_path, opts, result);
    }

    /* ===== Flux -> Bitstream ===== */
    if (src_format == UFT_FORMAT_SCP && dst_format == UFT_FORMAT_HFE) {
        return uftc_convert_scp_to_hfe(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_SCP && dst_format == UFT_FORMAT_G64) {
        return uftc_convert_scp_to_g64(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_KRYOFLUX && dst_format == UFT_FORMAT_SCP) {
        return uftc_convert_kryoflux_to_scp(src_data, src_size, dst_path,
                                             opts, result);
    }
    if (src_format == UFT_FORMAT_KRYOFLUX && dst_format == UFT_FORMAT_HFE) {
        return uftc_convert_kryoflux_to_hfe(src_data, src_size, dst_path,
                                             opts, result);
    }

    /* ===== Sector -> Flux (synthetic) ===== */
    if (src_format == UFT_FORMAT_D64 && dst_format == UFT_FORMAT_SCP) {
        /* D64 -> SCP: D64 -> G64 (GCR encode) -> SCP (flux synthesis) */
        /* Two-stage conversion via G64 intermediate */
        uftc_report_progress(opts, 5, "D64->SCP via G64 intermediate");

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
        uft_error_t err = uftc_convert_g64_to_scp(g64_buf, g64_size, dst_path,
                                                    opts, result);
        free(g64_buf);
        return err;
    }
    if ((src_format == UFT_FORMAT_ADF || src_format == UFT_FORMAT_IMG) &&
        dst_format == UFT_FORMAT_SCP) {
        /* ADF/IMG -> SCP: encode to MFM bitstream, then to flux */
        /* Two-stage: first build MFM bitstream, then convert to flux */
        uftc_report_progress(opts, 5, "Sector->SCP via MFM synthesis");

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
                if (uftc_is_cancelled(opts)) break;

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

        int rc = scp_writer_save(writer, dst_path);
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
        return uftc_convert_g64_to_hfe(src_data, src_size, dst_path, opts, result);
    }
    if (src_format == UFT_FORMAT_HFE && dst_format == UFT_FORMAT_G64) {
        return uftc_convert_hfe_to_g64(src_data, src_size, dst_path, opts, result);
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
        return UFT_ERR_NULL_POINTER;
    }

    memset(result, 0, sizeof(*result));

    /* Read source file */
    FILE* f = fopen(src_path, "rb");
    if (!f) {
        result->error = UFT_ERR_FILE_NOT_FOUND;
        return UFT_ERR_FILE_NOT_FOUND;
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
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
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
        result->error = UFT_ERR_FORMAT_INVALID;
        snprintf(result->warnings[0], sizeof(result->warnings[0]),
                 "Could not detect source format");
        result->warning_count = 1;
        return UFT_ERR_FORMAT_INVALID;
    }

    /* Check conversion path exists */
    const uft_conversion_path_t* path = uft_convert_get_path(src_format,
                                                              dst_format);
    if (!path) {
        free(src_data);
        result->error = UFT_ERR_NOT_SUPPORTED;
        snprintf(result->warnings[0], sizeof(result->warnings[0]),
                 "No conversion path from %s to %s",
                 uft_format_get_name(src_format),
                 uft_format_get_name(dst_format));
        result->warning_count = 1;
        return UFT_ERR_NOT_SUPPORTED;
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
        err = uftc_write_output_file(dst_path, src_data, src_size);
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
        err = uftc_write_output_file(dst_path, src_data, src_size);
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
        return UFT_ERR_NULL_POINTER;
    }

    memset(result, 0, sizeof(*result));
    *dst_data = NULL;
    *dst_size = 0;

    /* Validate conversion path */
    const uft_conversion_path_t* path = uft_convert_get_path(src_format,
                                                              dst_format);
    if (!path) {
        result->error = UFT_ERR_NOT_SUPPORTED;
        snprintf(result->warnings[result->warning_count++],
                 sizeof(result->warnings[0]),
                 "No conversion path from %s to %s",
                 uft_format_get_name(src_format),
                 uft_format_get_name(dst_format));
        return UFT_ERR_NOT_SUPPORTED;
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

    for (size_t i = 0; i < g_num_conversion_paths; i++) {
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

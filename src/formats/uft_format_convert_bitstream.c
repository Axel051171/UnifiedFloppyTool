/**
 * @file uft_format_convert_bitstream.c
 * @brief Bitstream format converters.
 *
 * Contains converters between bitstream formats and to/from flux:
 *   - HFE -> SCP (bitstream to flux)
 *   - G64 -> SCP (bitstream to flux)
 *   - G64 -> HFE (bitstream to bitstream)
 *   - HFE -> G64 (bitstream to bitstream)
 *   - sectors -> HFE (sector to bitstream)
 */

#include "uft_format_convert_internal.h"

// ============================================================================
// Bitstream <-> Flux Conversions
// ============================================================================

/**
 * @brief HFE -> SCP: Convert HFE bitstream to SCP flux
 *
 * Each HFE bit cell maps to a flux transition time based on the bit rate.
 * Uses scp_writer API to build the SCP file.
 */
uft_error_t uftc_convert_hfe_to_scp(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uftc_report_progress(opts, 10, "Parsing HFE file");

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

    uftc_report_progress(opts, 20, "Converting bitstream to flux");

    /* Read track LUT */
    const hfe_track_entry_t* lut = (const hfe_track_entry_t*)
                                    (src_data + hdr->track_list_offset * 512);

    for (int cyl = 0; cyl < cylinders; cyl++) {
        if (uftc_is_cancelled(opts)) break;

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

        uftc_report_progress(opts, 20 + (cyl * 70 / cylinders),
                             "Converting bitstream tracks");
    }

    uftc_report_progress(opts, 95, "Writing SCP output");

    int rc = scp_writer_save(writer, dst_path);
    if (rc == 0) {
        result->success = true;
    } else {
        result->error = UFT_ERR_IO;
    }

    scp_writer_free(writer);
    uftc_report_progress(opts, 100, "HFE->SCP complete");
    return result->success ? UFT_OK : result->error;
}

/**
 * @brief G64 -> SCP: Convert GCR bitstream to synthetic flux in SCP format
 *
 * Each GCR bit cell is converted to a flux transition time based on
 * the C64 speed zone timing.
 */
uft_error_t uftc_convert_g64_to_scp(const uint8_t* src_data, size_t src_size,
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

    int revolutions = (opts && opts->synthetic_revolutions > 0)
                      ? opts->synthetic_revolutions : 3;

    scp_writer_t* writer = scp_writer_create(SCP_TYPE_C64, (uint8_t)revolutions);
    if (!writer) {
        g64_free(g64);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    uftc_report_progress(opts, 20, "Converting GCR to synthetic flux");

    /* Process each track (halftracks 2,4,6...70 = tracks 1-35) */
    for (int track = 1; track <= (int)g64->num_tracks && track <= 42; track++) {
        if (uftc_is_cancelled(opts)) break;

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
        uftc_report_progress(opts, 20 + (track * 70 / 42),
                             "Converting GCR tracks");
    }

    uftc_report_progress(opts, 95, "Writing SCP output");

    rc = scp_writer_save(writer, dst_path);
    if (rc == 0) {
        result->success = true;
    } else {
        result->error = UFT_ERR_IO;
    }

    scp_writer_free(writer);
    g64_free(g64);
    uftc_report_progress(opts, 100, "G64->SCP complete");
    return result->success ? UFT_OK : result->error;
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
uft_error_t uftc_convert_g64_to_hfe(const uint8_t* src_data, size_t src_size,
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

    uftc_report_progress(opts, 30, "Converting GCR tracks to HFE");

    for (int track = 1; track <= num_tracks; track++) {
        if (uftc_is_cancelled(opts)) break;

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
        uftc_report_progress(opts, 30 + (track * 60 / num_tracks),
                             "Wrapping GCR tracks in HFE");
    }

    uftc_report_progress(opts, 95, "Writing HFE output");

    uft_error_t err = uftc_write_output_file(dst_path, hfe_data, total_size);
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
    uftc_report_progress(opts, 100, "G64->HFE complete");
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
uft_error_t uftc_convert_hfe_to_g64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uftc_report_progress(opts, 10, "Parsing HFE file");

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

    uftc_report_progress(opts, 20, "Extracting GCR tracks from HFE");

    /* Read track LUT */
    const hfe_track_entry_t* lut = (const hfe_track_entry_t*)
                                    (src_data + hdr->track_list_offset * 512);

    for (int cyl = 0; cyl < cylinders; cyl++) {
        if (uftc_is_cancelled(opts)) break;

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

        uftc_report_progress(opts, 20 + (cyl * 70 / cylinders),
                             "Extracting GCR tracks");
    }

    uftc_report_progress(opts, 95, "Writing G64 output");

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
    uftc_report_progress(opts, 100, "HFE->G64 complete");
    return result->success ? UFT_OK : result->error;
}

// ============================================================================
// Sector -> Bitstream: Encode sector data to synthetic MFM in HFE container
// ============================================================================

/**
 * @brief ADF/IMG -> HFE: Encode sector data to synthetic MFM in HFE container
 *
 * Builds an HFE file with synthetic MFM bitstream encoding of the sector data.
 */
uft_error_t uftc_convert_sectors_to_hfe(const uint8_t* src_data,
                                          size_t src_size,
                                          const char* dst_path,
                                          uft_format_t src_format,
                                          const uft_convert_options_ext_t* opts,
                                          uft_convert_result_t* result) {
    uftc_report_progress(opts, 10, "Analyzing sector geometry");

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

    uftc_report_progress(opts, 20, "Building HFE container");

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

    uftc_report_progress(opts, 40, "Encoding MFM tracks");

    /* Encode each cylinder's sectors into MFM bitstream */
    for (int cyl = 0; cyl < cylinders; cyl++) {
        if (uftc_is_cancelled(opts)) break;

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
        uftc_report_progress(opts, 40 + (cyl * 50 / cylinders), "Encoding MFM tracks");
    }

    uftc_report_progress(opts, 95, "Writing HFE output");

    uft_error_t err = uftc_write_output_file(dst_path, hfe_data, total_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = (int)total_size;
    } else {
        result->error = err;
    }

    free(hfe_data);
    uftc_report_progress(opts, 100, "Sector->HFE complete");
    return err;
}

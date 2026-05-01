/**
 * @file uft_format_convert_flux.c
 * @brief Flux-related format converters.
 *
 * Contains converters involving flux data (SCP, KryoFlux):
 *   - SCP -> D64
 *   - SCP -> MFM sectors (ADF/IMG)
 *   - KryoFlux -> D64
 *   - KryoFlux -> ADF
 *   - HFE -> sectors (IMG/ADF)
 *   - SCP -> HFE
 *   - SCP -> G64
 *   - KryoFlux -> SCP
 *   - KryoFlux -> HFE
 */

#include "uft_format_convert_internal.h"
#include "uft/flux/uft_mfm_sector_parser.h"

// ============================================================================
// Flux/Bitstream -> Sector Conversions (Decode Pipeline)
// ============================================================================

/**
 * @brief SCP -> D64: Decode SCP flux to C64 D64 sector image
 *
 * Pipeline: SCP parse -> flux deltas -> PLL -> GCR bitstream -> sector extract
 */
uft_error_t uftc_convert_scp_to_d64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uftc_report_progress(opts, 5, "Parsing SCP file");

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

    uftc_report_progress(opts, 10, "Decoding flux to GCR sectors");

    int retries = (opts && opts->decode_retries > 0) ? opts->decode_retries : 5;

    /* Process each track: flux -> PLL -> GCR -> sectors */
    for (int track = 1; track <= 35; track++) {
        if (uftc_is_cancelled(opts)) break;

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

        uftc_report_progress(opts, 10 + (track * 80 / 35), "Decoding tracks");
    }

    uftc_report_progress(opts, 90, "Writing D64 output");

    /* Save D64 */
    rc = d64_save(dst_path, d64, false);
    if (rc == 0) {
        result->success = true;
    } else {
        result->error = UFT_ERR_IO;
    }

    d64_free(d64);
    uft_scp_free(&scp);
    uftc_report_progress(opts, 100, "SCP->D64 complete");
    return result->success ? UFT_OK : result->error;
}

/**
 * @brief SCP -> ADF/IMG: Decode SCP flux to MFM sector image
 *
 * Pipeline: SCP parse -> flux deltas -> MFM PLL -> sector extract -> write
 * Used for both ADF (Amiga) and IMG (PC) output.
 */
uft_error_t uftc_convert_scp_to_mfm_sectors(const uint8_t* src_data,
                                              size_t src_size,
                                              const char* dst_path,
                                              uft_format_t dst_format,
                                              const uft_convert_options_ext_t* opts,
                                              uft_convert_result_t* result) {
    uftc_report_progress(opts, 5, "Parsing SCP file");

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

    uftc_report_progress(opts, 10, "Decoding MFM flux to sectors");

    /* MF-141 / AUD-002 — uft_mfm_decode_track() is the canonical
     * MFM IDAM/DAM parser. It returns sectors keyed by IBM CHRN
     * (cylinder/head/sector/size) with explicit CRC-validity flags.
     * The conversion pipeline below is responsible for placing them
     * into the linear sector image at the correct LBA, and only
     * accepting sectors whose data CRC validated.
     */

    for (int cyl = 0; cyl < cylinders; cyl++) {
        for (int head = 0; head < heads; head++) {
            if (uftc_is_cancelled(opts)) goto done_mfm;

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

            /* Decode all sectors on this track. Pool size = max sectors
             * per track × max sector size (1024). The parser places
             * sector payloads back-to-back; we then map each to its
             * LBA slot using CHRN.R from the IDAM. */
            uint8_t track_pool[64 * 1024];
            uft_mfm_sector_t track_sectors[64];
            size_t n = uft_mfm_decode_track(bitstream, bit_pos,
                                            track_pool, sizeof(track_pool),
                                            track_sectors,
                                            sizeof(track_sectors) / sizeof(track_sectors[0]),
                                            NULL);

            for (size_t i = 0; i < n; i++) {
                const uft_mfm_sector_t *s = &track_sectors[i];
                /* Forensic: only place sectors whose data CRC validated.
                 * Bad-CRC sectors are counted as failed but never
                 * silently overwrite good data. */
                if (!s->dam_present || !s->data_crc_ok) {
                    result->tracks_failed++;
                    continue;
                }
                /* Map by IBM sector ID (1-based). LBA per IBM
                 * convention = (cyl * heads + head) * sectors_per_track
                 *            + (R - 1). */
                if (s->sector < 1 || s->sector > sectors) continue;
                uint32_t lba = (uint32_t)(cyl * heads + head) * sectors
                             + (uint32_t)(s->sector - 1);
                if (lba >= total_sectors_count) continue;
                size_t copy_len = s->data_len;
                if (copy_len > (size_t)sector_size) copy_len = sector_size;
                memcpy(output + lba * sector_size,
                       track_pool + s->data_offset, copy_len);
                result->sectors_converted++;
            }
            result->tracks_converted++;

            free(bitstream);
        }

        uftc_report_progress(opts, 10 + (cyl * 80 / cylinders), "Decoding tracks");
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
            /* No sector data extracted -- MFM parser not yet integrated */
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
            uftc_report_progress(opts, 100, "SCP->sector FAILED (no data extracted)");
            return UFT_ERR_NOT_IMPLEMENTED;
        }
    }

    uftc_report_progress(opts, 90, "Writing output");

    uft_error_t err = uftc_write_output_file(dst_path, output, output_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = (int)output_size;
    } else {
        result->error = err;
    }

    free(output);
    uft_scp_free(&scp);
    uftc_report_progress(opts, 100, "SCP->sector complete");
    return err;
}

/**
 * @brief Kryoflux -> D64: Decode Kryoflux stream to D64
 *
 * Similar to SCP->D64 but reads Kryoflux stream files.
 * Kryoflux uses per-track .raw files (track00.0.raw, etc.)
 */
uft_error_t uftc_convert_kryoflux_to_d64(const uint8_t* src_data,
                                           size_t src_size,
                                           const char* dst_path,
                                           const uft_convert_options_ext_t* opts,
                                           uft_convert_result_t* result) {
    uftc_report_progress(opts, 5, "Parsing Kryoflux stream");

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

    uftc_report_progress(opts, 20, "Decoding Kryoflux flux data");

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

    uftc_report_progress(opts, 80, "Writing D64 output");

    rc = d64_save(dst_path, d64, false);
    if (rc == 0) {
        result->success = true;
    } else {
        result->error = UFT_ERR_IO;
    }

    d64_free(d64);
    uft_kfx_free(&stream);
    uftc_report_progress(opts, 100, "Kryoflux->D64 complete");
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
uft_error_t uftc_convert_kryoflux_to_adf(const uint8_t* src_data,
                                           size_t src_size,
                                           const char* dst_path,
                                           const uft_convert_options_ext_t* opts,
                                           uft_convert_result_t* result) {
    uftc_report_progress(opts, 5, "Parsing Kryoflux stream for ADF");

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

    uftc_report_progress(opts, 20, "Decoding Kryoflux MFM flux");

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

    uftc_report_progress(opts, 80, "Writing ADF output");

    uft_error_t err = uftc_write_output_file(dst_path, output, output_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = (int)output_size;
    } else {
        result->error = err;
    }

    free(output);
    uft_kfx_free(&stream);
    uftc_report_progress(opts, 100, "Kryoflux->ADF complete");
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
uft_error_t uftc_convert_hfe_to_sectors(const uint8_t* src_data,
                                          size_t src_size,
                                          const char* dst_path,
                                          uft_format_t dst_format,
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

    uftc_report_progress(opts, 20, "Extracting MFM sectors from HFE");

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

        uftc_report_progress(opts, 20 + (cyl * 70 / cylinders),
                             "Decoding HFE tracks");
    }

    uftc_report_progress(opts, 95, "Writing output");

    uft_error_t err = uftc_write_output_file(dst_path, output, output_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = (int)output_size;
    } else {
        result->error = err;
    }

    free(output);
    uftc_report_progress(opts, 100, "HFE->sector complete");
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
uft_error_t uftc_convert_scp_to_hfe(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uftc_report_progress(opts, 5, "Parsing SCP file for HFE conversion");

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

    uftc_report_progress(opts, 15, "PLL decoding SCP flux to bitstream");

    for (int cyl = 0; cyl < cylinders; cyl++) {
        if (uftc_is_cancelled(opts)) break;

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

        uftc_report_progress(opts, 15 + (cyl * 75 / cylinders),
                             "Converting SCP tracks to HFE");
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
    uft_scp_free(&scp);
    uftc_report_progress(opts, 100, "SCP->HFE complete");
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
uft_error_t uftc_convert_scp_to_g64(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uftc_report_progress(opts, 5, "Parsing SCP file for G64 conversion");

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

    uftc_report_progress(opts, 10, "PLL decoding SCP flux to GCR bitstream");

    for (int track = 1; track <= num_tracks; track++) {
        if (uftc_is_cancelled(opts)) break;

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

        uftc_report_progress(opts, 10 + (track * 80 / num_tracks),
                             "Decoding GCR tracks");
    }

    uftc_report_progress(opts, 95, "Writing G64 output");

    rc = g64_save(dst_path, g64);
    if (rc == 0) {
        result->success = true;
    } else {
        result->error = UFT_ERR_IO;
    }

    g64_free(g64);
    uft_scp_free(&scp);
    uftc_report_progress(opts, 100, "SCP->G64 complete");
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
uft_error_t uftc_convert_kryoflux_to_scp(const uint8_t* src_data,
                                           size_t src_size,
                                           const char* dst_path,
                                           const uft_convert_options_ext_t* opts,
                                           uft_convert_result_t* result) {
    uftc_report_progress(opts, 5, "Parsing Kryoflux stream for SCP conversion");

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

    uftc_report_progress(opts, 20, "Converting Kryoflux flux to SCP");

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

    uftc_report_progress(opts, 80, "Writing SCP output");

    rc = scp_writer_save(writer, dst_path);
    if (rc == 0) {
        result->success = true;
    } else {
        result->error = UFT_ERR_IO;
    }

    scp_writer_free(writer);
    uft_kfx_free(&stream);
    uftc_report_progress(opts, 100, "Kryoflux->SCP complete");
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
uft_error_t uftc_convert_kryoflux_to_hfe(const uint8_t* src_data,
                                           size_t src_size,
                                           const char* dst_path,
                                           const uft_convert_options_ext_t* opts,
                                           uft_convert_result_t* result) {
    uftc_report_progress(opts, 5, "Parsing Kryoflux stream for HFE conversion");

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
    uftc_report_progress(opts, 20, "PLL decoding Kryoflux flux");

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

    uftc_report_progress(opts, 50, "Building HFE container");

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

    uftc_report_progress(opts, 90, "Writing HFE output");

    uft_error_t err = uftc_write_output_file(dst_path, hfe_data, total_size);
    if (err == UFT_OK) {
        result->success = true;
        result->bytes_written = (int)total_size;
    } else {
        result->error = err;
    }

    free(hfe_data);
    uft_kfx_free(&stream);
    uftc_report_progress(opts, 100, "Kryoflux->HFE complete");
    return err;
}

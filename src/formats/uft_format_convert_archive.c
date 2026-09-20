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
 * MF-1287: nutzt den Strom-Kern (`uft_td0_strom_aus_bytes`) und
 * `uft_td0_to_imd()`. Vorher stand hier `uft_td0_read_mem()` — der
 * zweite TD0-Leser des Baums, der am Dateiende bis zu 255 Spuren
 * erfand.
 */
/* MF-701: Speicher-Kern, letzte der sechs Doppelungen aus MF-695.
 *
 * Wie bei IMD->IMG verlor die Abkuerzung in `uft_convert_memory()`
 * nicht Bytes, sondern den BERICHT: Spurzahl und die Angabe, ob das
 * Abbild LZSS-komprimiert war, standen nur in der Datei-Fassung. */
uft_error_t uftc_td0_to_img_mem(const uint8_t* src_data, size_t src_size,
                                 const uft_convert_options_ext_t* opts,
                                 uft_convert_result_t* result,
                                 uint8_t** out_data, size_t* out_size) {
    if (!out_data || !out_size) return UFT_ERR_NULL_POINTER;
    *out_data = NULL;
    *out_size = 0;
    /* MF-1287: hier stand `uft_td0_read_mem()` — der ZWEITE TD0-Leser.
     * Er ist weg; gelesen wird mit dem Strom-Kern, den auch das Plugin
     * benutzt. Dass dieser Wandler keinen PFAD bekommt, sondern nur
     * Bytes, ist genau der Grund, warum der Kern an Bytes haengt und
     * nicht an einem `uft_disk_t` (ARCH-6). */
    uft_td0_strom_t strom;
    memset(&strom, 0, sizeof(strom));

    uftc_report_progress(opts, 10, "Parsing TD0 image");

    int rc = uft_td0_strom_aus_bytes(src_data, src_size, &strom);
    if (rc != UFT_OK) {
        uft_td0_strom_frei(&strom);
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result,
                 "TD0 parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    uftc_report_progress(opts, 50, "Extracting raw sectors");

    /* Convert to raw sector data.
     *
     * MF-1287: der Umweg ueber IMD stand vorher in `uft_td0_to_raw()`
     * und tat dasselbe. Er steht jetzt HIER, weil dann die Spurzahl eine
     * GEZAEHLTE ist statt einer angesagten — `num_tracks` zaehlt nur
     * Spuren, die wirklich Sektoren geliefert haben. `uft_td0_to_raw()`
     * haette danach keinen Aufrufer mehr und ist mit dem alten Leser
     * gefallen. */
    uft_imd_image_t imd_zwischen;
    uft_imd_init(&imd_zwischen);
    uint8_t* raw_data = NULL;
    size_t raw_size = 0;
    rc = uft_td0_to_imd(&strom, &imd_zwischen);
    if (rc == UFT_OK)
        rc = uft_imd_to_raw(&imd_zwischen, &raw_data, &raw_size, 0xF6);
    const size_t spuren = imd_zwischen.num_tracks;
    uft_imd_free(&imd_zwischen);
    if (rc != 0 || !raw_data) {
        uft_td0_strom_frei(&strom);
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result,
                 "TD0 sector extraction failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    result->tracks_converted = (int)spuren;

    /* Der Bericht gehoert in den KERN (MF-701) — sonst bekommt ihn
     * nur, wer ueber die Datei-API kommt. */
    uftc_add_warning(result,
             "Decompressed %zu tracks (%s compression), %zu bytes output",
             spuren,
             strom.gepackt ? "LZSS" : "none",
             raw_size);

    uft_td0_strom_frei(&strom);
    *out_data = raw_data;
    *out_size = raw_size;
    return UFT_OK;
}

/* Die Datei-Huelle: Kern + schreiben, keine Wandlungslogik. */
uft_error_t uftc_convert_td0_to_img(const uint8_t* src_data, size_t src_size,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uint8_t* buf = NULL;
    size_t n = 0;
    uft_error_t e = uftc_td0_to_img_mem(src_data, src_size, opts, result,
                                         &buf, &n);
    if (e != UFT_OK) return e;
    uftc_report_progress(opts, 80, "Writing IMG output");
    e = uftc_write_output_file(dst_path, buf, n);
    if (e != UFT_OK) {
        free(buf);
        result->error = e;
        return e;
    }
    result->success = true;
    result->bytes_written = (int)n;
    free(buf);
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
    /* MF-1287: derselbe Strom-Kern wie im Plugin. Siehe to_img_mem. */
    uft_td0_strom_t strom;
    memset(&strom, 0, sizeof(strom));

    uftc_report_progress(opts, 10, "Parsing TD0 image");

    int rc = uft_td0_strom_aus_bytes(src_data, src_size, &strom);
    if (rc != UFT_OK) {
        uft_td0_strom_frei(&strom);
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result,
                 "TD0 parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    uftc_report_progress(opts, 40, "Converting TD0 to IMD format");

    /* Use the canonical converters from src/formats/uft_format_converters.c.
     * uft_td0_to_imd() builds a uft_imd_image_t from the parsed TD0 (sector
     * mode, geometry, per-sector data preserved), and uft_imd_write()
     * serialises to the IMD on-disk layout. This replaces the previous
     * inline fallback that synthesised an IMD by hand from raw bytes — the
     * fallback lost compressed-fill semantics and used a hardcoded mode. */
    uft_imd_image_t imd_img;
    uft_imd_init(&imd_img);

    rc = uft_td0_to_imd(&strom, &imd_img);
    if (rc != 0) {
        uft_imd_free(&imd_img);
        uft_td0_strom_frei(&strom);
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result,
                 "TD0->IMD conversion failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    uftc_report_progress(opts, 80, "Writing IMD output");

    rc = uft_imd_write(dst_path, &imd_img);
    if (rc != 0) {
        uft_imd_free(&imd_img);
        uft_td0_strom_frei(&strom);
        result->error = UFT_ERR_IO;
        uftc_add_warning(result,
                 "IMD write failed (error %d)", rc);
        return UFT_ERR_IO;
    }

    /* MF-1287: die Zahl kommt aus der gerade gebauten IMD — gezaehlte
     * Spuren mit Sektoren, nicht die angesagte Geometrie. */
    result->tracks_converted = (int)imd_img.num_tracks;

    /* uft_imd_write() does not report bytes_written; query the output file
     * to populate result->bytes_written so callers see the actual size. */
    {
        FILE *bf = fopen(dst_path, "rb");
        if (bf) {
            fseek(bf, 0, SEEK_END);
            result->bytes_written = (int)ftell(bf);
            fclose(bf);
        }
    }

    result->success = true;

    uft_imd_free(&imd_img);
    uft_td0_strom_frei(&strom);

    uftc_report_progress(opts, 100, "TD0->IMD complete");
    return UFT_OK;
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
            uftc_add_warning(result,
                     "NBZ decompression failed, source appears to be raw D64");
        } else {
            free(decompressed);
            result->error = UFT_ERR_FORMAT;
            uftc_add_warning(result,
                     "NBZ decompression failed (%d bytes input)", (int)src_size);
            return UFT_ERR_FORMAT;
        }
    }

    /* Validate that output looks like a D64 */
    if (decomp_size != 174848 && decomp_size != 175531 &&
        decomp_size != 196608 && decomp_size != 197376) {
        uftc_add_warning(result,
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
        uftc_add_warning(result,
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
        uftc_add_warning(result,
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
        uftc_add_warning(result,
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

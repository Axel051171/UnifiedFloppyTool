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
#include "uft/uft_format_plugin.h"   /* MF-433: read via the plugin */

extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_g64;

// ============================================================================
// Bitstream -> Sector (decode)
// ============================================================================

/**
 * @brief G64 -> D64: Decode GCR bitstream to sector data
 *
 * Uses the g64_to_d64() converter from uft_d64_g64.h.
 */
/* ── Die Optionen des C64-Kodierers, EINMAL uebersetzt (MF-695) ───────
 *
 * `convert_options_t` (der GCR-Kodierer) und `uft_convert_options_ext_t`
 * (die oeffentliche Wandlungs-API) waren bis MF-695 nicht verbunden: der
 * Kodierer bekam immer `convert_get_defaults()`, und keine einzige
 * Einstellung des Aufrufers erreichte ihn. Gemessen in
 * `tests/test_convert_options_reach_encoder.c`: der Kodierer reagiert auf
 * seine eigenen Parameter (gap_fill 0x55 vs 0xAA aendert die Bytes),
 * aber **0 von 9** Feldern der oeffentlichen API taten es.
 *
 * Uebersetzt wird nur, was eine BELEGTE Bedeutung hat — gemessen an
 * `src/formats/c64/uft_d64_g64.c`:
 *
 *   extended_tracks     `:975`/`:1093`  num_tracks = 42 statt num_tracks
 *   include_halftracks  `:976`/`:1099`  g64_create(..., halftracks)
 *   gap_fill            `:1006`/`:1135` Fuellbyte je Spur
 *   generate_errors     `:1328`/`:1423` legt die D64-Fehlerkarte an
 *   align_tracks        KEIN LESER
 *   sync_length         KEIN LESER
 *
 * Die letzten beiden stehen in der Struktur und werden nirgends gelesen —
 * dieselbe Klasse wie OPT-1, eine Schicht tiefer. Sie bekommen darum
 * KEINE erfundene Zuordnung; das waere eine Einstellung, die anzukommen
 * scheint und nichts tut.
 *
 * `generate_errors` kann hier nur STAERKER werden, nie schwaecher: die
 * Vorgabe ist `true`, die Fehlerkarte wird heute also immer angelegt.
 * `preserve_errors` aus der oeffentlichen API ist per Vorgabe `false` —
 * eine 1:1-Zuweisung wuerde die Karte fuer jeden Aufrufer abschalten, der
 * das Feld nicht setzt. Das waere stiller Datenverlust an einer Stelle,
 * die vorher sicher war. Forensik schlaegt Symmetrie.
 */
static convert_options_t uftc_c64_encoder_options(
        const uft_convert_options_ext_t* opts) {
    convert_options_t conv;
    convert_get_defaults(&conv);
    if (!opts) return conv;

    /* Mehr als 35 Spuren angefordert => die 40/42-Spur-Fassung. 35 ist
     * die 1541-Norm; alles darueber ist genau das, was
     * `extended_tracks` erzeugt. */
    if (opts->target_geometry.cylinders > 35) {
        conv.extended_tracks = true;
    }
    /* Nur verstaerken, nie abschalten — siehe Kopfkommentar. */
    if (opts->preserve_errors) {
        conv.generate_errors = true;
    }
    return conv;
}

uft_error_t uftc_convert_g64_to_d64(const uint8_t* src_data, size_t src_size,
                                      const char* src_path,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    convert_options_t conv_opts = uftc_c64_encoder_options(opts);
    convert_result_t conv_result;
    memset(&conv_result, 0, sizeof(conv_result));
    d64_image_t* d64 = NULL;
    int rc;

    if (src_path) {
        /* MF-436: read the raw GCR through uft_format_plugin_g64. Same
         * decoder as the blob path — gcr_track_to_sectors() — so the output
         * is byte-identical; test_convert_via_plugin pins that against the
         * c1541 reference image, and pins the full D64->G64->D64 roundtrip
         * on top. */
        uftc_report_progress(opts, 10, "Opening G64 via format plugin");

        uft_disk_t disk;
        memset(&disk, 0, sizeof(disk));
        disk.read_only = true;
        if (uft_format_plugin_g64.open(&disk, src_path, true) != UFT_OK) {
            result->error = UFT_ERR_FORMAT;
            uftc_add_warning(result, "G64 open failed via plugin");
            return UFT_ERR_FORMAT;
        }

        uftc_report_progress(opts, 30, "Converting G64 GCR to D64 sectors");
        rc = uft_cbm_d64_decode_via_plugin(&uft_format_plugin_g64, &disk,
                                            &conv_opts, &d64, &conv_result);
        uft_format_plugin_g64.close(&disk);
    } else {
        uftc_report_progress(opts, 10, "Loading G64 image");

        g64_image_t* g64 = NULL;
        rc = g64_load_buffer(src_data, src_size, &g64);
        if (rc != 0 || !g64) {
            result->error = UFT_ERR_FORMAT;
            uftc_add_warning(result,
                     "G64 parse failed (error %d)", rc);
            return UFT_ERR_FORMAT;
        }

        uftc_report_progress(opts, 30, "Converting G64 GCR to D64 sectors");
        rc = g64_to_d64(g64, &d64, &conv_opts, &conv_result);
        g64_free(g64);
    }

    if (rc != 0 || !d64) {
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result,
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
 * Reads the source through uft_format_plugin_d64 when a path is available
 * (MF-433), falling back to the private d64_load_buffer() blob path when only
 * bytes are on hand — uft_convert_memory() is the caller without a path.
 *
 * The two produce byte-identical G64 output; test_convert_via_plugin pins that
 * against the c1541 reference image, which is why the switch is safe to make.
 *
 * One deliberate behaviour change: the plugin recognises 41- and 42-track
 * images, the blob loader caps at 40 and silently drops the rest. Such an
 * image now converts in full. No 41/42-track reference image exists in the
 * corpus, so that path is reasoned, not tested.
 */
/* MF-695: Speicher-Kern. Bis dahin stand die D64->G64-Kette ZWEIMAL im
 * Baum — hier als Dateiwandler und ein zweites Mal, von Hand nachgebaut,
 * in `uft_convert_memory()`. Der Kommentar an
 * `uft_format_convert_dispatch.c:973` nannte die Doppelung seit MF-655
 * und ihre Ursache: MF-567.
 *
 * Gemessen erzeugten beide Fassungen dieselben Bytes — die Doppelung war
 * ein RISIKO, keine Divergenz. Sie wurde eine, sobald die
 * Optionen-Uebersetzung dazukam: sie haette nur an einer der beiden
 * Stellen gewirkt. Darum erst vereinigen, dann uebersetzen.
 *
 * Die Form ist die des Hauses (`uftc_atr_to_xfd_mem` seit MF-655): ein
 * Kern, der einen Puffer liefert, und eine Huelle, die ihn schreibt. */
uft_error_t uftc_d64_to_g64_mem(const uint8_t* src_data, size_t src_size,
                                 const char* src_path,
                                 const uft_convert_options_ext_t* opts,
                                 uft_convert_result_t* result,
                                 uint8_t** out_data, size_t* out_size) {
    if (!out_data || !out_size) return UFT_ERR_NULL_POINTER;
    *out_data = NULL;
    *out_size = 0;
    convert_options_t conv_opts = uftc_c64_encoder_options(opts);
    convert_result_t conv_result;
    memset(&conv_result, 0, sizeof(conv_result));
    g64_image_t* g64 = NULL;
    int rc;

    if (src_path) {
        uftc_report_progress(opts, 10, "Opening D64 via format plugin");

        uft_disk_t disk;
        memset(&disk, 0, sizeof(disk));
        disk.read_only = true;
        if (uft_format_plugin_d64.open(&disk, src_path, true) != UFT_OK) {
            result->error = UFT_ERR_FORMAT;
            uftc_add_warning(result, "D64 open failed via plugin");
            return UFT_ERR_FORMAT;
        }

        uftc_report_progress(opts, 30, "Encoding D64 sectors to GCR");
        rc = uft_cbm_g64_encode_via_plugin(&uft_format_plugin_d64, &disk,
                                            &conv_opts, &g64, &conv_result);
        uft_format_plugin_d64.close(&disk);
    } else {
        uftc_report_progress(opts, 10, "Loading D64 image");

        d64_image_t* d64 = NULL;
        rc = d64_load_buffer(src_data, src_size, &d64);
        if (rc != 0 || !d64) {
            result->error = UFT_ERR_FORMAT;
            uftc_add_warning(result,
                     "D64 parse failed (error %d)", rc);
            return UFT_ERR_FORMAT;
        }

        uftc_report_progress(opts, 30, "Encoding D64 sectors to GCR");
        rc = d64_to_g64(d64, &g64, &conv_opts, &conv_result);
        d64_free(d64);
    }

    if (rc != 0 || !g64) {
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result,
                 "D64->G64 encode failed: %s", conv_result.description);
        return UFT_ERR_FORMAT;
    }

    uftc_report_progress(opts, 80, "G64-Spurdaten serialisieren");

    rc = g64_save_buffer(g64, out_data, out_size);
    g64_free(g64);
    if (rc != 0 || !*out_data) {
        result->error = UFT_ERR_IO;
        uftc_add_warning(result, "G64-Serialisierung fehlgeschlagen (%d)", rc);
        return UFT_ERR_IO;
    }
    result->tracks_converted = conv_result.tracks_converted;
    result->sectors_converted = conv_result.sectors_converted;
    return UFT_OK;
}

/* Die Datei-Huelle: Kern + schreiben. Sie enthaelt KEINE Wandlungslogik,
 * damit es keine zweite geben kann (MF-695). */
uft_error_t uftc_convert_d64_to_g64(const uint8_t* src_data, size_t src_size,
                                      const char* src_path,
                                      const char* dst_path,
                                      const uft_convert_options_ext_t* opts,
                                      uft_convert_result_t* result) {
    uint8_t* buf = NULL;
    size_t n = 0;
    uft_error_t e = uftc_d64_to_g64_mem(src_data, src_size, src_path,
                                         opts, result, &buf, &n);
    if (e != UFT_OK) return e;
    uftc_report_progress(opts, 90, "G64 schreiben");
    e = uftc_finish_or_refuse(result, dst_path, buf, n, "G64-Spurdaten");
    free(buf);
    uftc_report_progress(opts, 100, "D64->G64 complete");
    return e;
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
        uftc_add_warning(result,
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
        uftc_add_warning(result,
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
    uftc_add_warning(result,
             "Extracted %d tracks (%d cyls x %d heads), %zu bytes output",
             imd.num_tracks, imd.num_cylinders, imd.num_heads, raw_size);

    if (imd.bad_sectors > 0 || imd.unavail_sectors > 0) {
        uftc_add_warning(result,
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

    /* MF-545: schreiben ODER ablehnen. Vorher folgte `success`
     * allein daraus, dass sich die Datei anlegen liess. */
    uft_error_t err = uftc_finish_or_refuse(result, dst_path,
                                            imd_data, pos, "IMG->IMD");

    uftc_add_warning(result,
             "Created IMD: %d cyls x %d heads x %d secs (%s), %zu bytes",
             cylinders, heads, sectors,
             uft_imd_mode_name((uft_imd_mode_t)mode), pos);

    free(imd_data);
    uftc_report_progress(opts, 100, "IMG->IMD complete");
    return err;
}

/* ==========================================================================
 * ATR <-> XFD  (MF-655)
 *
 * XFD ist das ATR ohne seinen 16-Byte-Kopf. Selbst gemessen am Korpus-Paar
 * `atrcopy_dos2sd.atr` (92 176 B) / `.xfd` (92 160 B):  atr[16:] == xfd,
 * byteweise, ohne Ausnahme.
 *
 * Der Kopf traegt (Feldlage aus `src/formats/atr/uft_atr.c:53-56`, also aus
 * dem eigenen Leser, nicht geraten):
 *
 *     0-1   Magic 0x0296
 *     2-3   Paragraphen, niederwertig     -- aus der Dateigroesse ableitbar
 *     4-5   SEKTORGROESSE                 -- NICHT ableitbar
 *     6     Paragraphen, hoeherwertig     -- ableitbar
 *     7-15  unbenutzt
 *
 * Daraus folgt die Grenze: die Sektorgroesse (128/256/512) kann XFD nicht
 * speichern, und aus der Dateigroesse folgt sie nicht — 184 320 Byte sind
 * 1440 Sektoren zu 128 ODER 720 zu 256. Verlustfrei ist der Weg genau dann,
 * wenn der Kopf nichts traegt, was die Groesse nicht schon sagt.
 *
 * WARUM EIN GEMEINSAMER KERN: der Verteiler hat ZWEI Ketten — eine in
 * dispatch_conversion() fuer den Dateiweg, eine in uft_convert_memory().
 * Stuenden die Verlustregeln zweimal da, koennte eine Fassung nachziehen
 * und die andere nicht. Genau diese Doppelung war die Ursache von MF-567,
 * wo der Speicherweg am Preflight-Tor vorbeilief. Die Regeln stehen
 * deshalb hier, einmal; die beiden Wandler unten sind nur noch Verpackung.
 * ========================================================================== */

#define UFTC_ATR_HEADER  16
#define UFTC_ATR_MAGIC   0x0296

/**
 * @brief ATR -> XFD im Speicher: Kopf pruefen, Verlustregel anwenden, Rumpf
 *        herausgeben.
 *
 * @param out      erhaelt einen mit malloc() geholten Puffer (Aufrufer gibt frei)
 * @param out_size erhaelt dessen Groesse
 * @return UFT_OK, oder UFT_ERR_NOT_SUPPORTED wenn Daten verloren gingen und
 *         `accept_data_loss` fehlt.
 */
static uft_error_t uftc_atr_to_xfd_mem(const uint8_t* src_data, size_t src_size,
                                        const uft_convert_options_ext_t* opts,
                                        uft_convert_result_t* result,
                                        uint8_t** out, size_t* out_size) {
    *out = NULL; *out_size = 0;

    if (src_size < UFTC_ATR_HEADER) {
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result, "ATR zu kurz: %zu Byte, Kopf braucht %d",
                         src_size, UFTC_ATR_HEADER);
        return UFT_ERR_FORMAT;
    }

    uint16_t magic = (uint16_t)(src_data[0] | (src_data[1] << 8));
    if (magic != UFTC_ATR_MAGIC) {
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result, "kein ATR: Magic 0x%04X statt 0x%04X",
                         magic, UFTC_ATR_MAGIC);
        return UFT_ERR_FORMAT;
    }

    uint16_t sector_size = (uint16_t)(src_data[4] | (src_data[5] << 8));
    uint32_t paragraphs  = (uint32_t)(src_data[2] | (src_data[3] << 8))
                         | ((uint32_t)src_data[6] << 16);
    const size_t payload = src_size - UFTC_ATR_HEADER;

    bool reserved_used = false;
    for (int i = 7; i < UFTC_ATR_HEADER; i++)
        if (src_data[i] != 0) reserved_used = true;

    bool size_field_matches = ((size_t)paragraphs * 16u == payload);
    bool lossless = (sector_size == 128) && !reserved_used && size_field_matches;

    if (!lossless && !(opts && opts->accept_data_loss)) {
        result->error = UFT_ERR_NOT_SUPPORTED;
        if (sector_size != 128)
            uftc_add_warning(result,
                "ATR->XFD verliert hier Daten: Sektorgroesse %u steht im Kopf, "
                "XFD kann sie nicht speichern und aus der Dateigroesse folgt "
                "sie nicht. Mit accept_data_loss moeglich.",
                (unsigned)sector_size);
        if (reserved_used)
            uftc_add_warning(result,
                "ATR->XFD verliert hier Daten: Bytes 7-15 des Kopfes sind "
                "belegt. Mit accept_data_loss moeglich.");
        if (!size_field_matches)
            uftc_add_warning(result,
                "ATR->XFD: Kopf nennt %u Paragraphen (%zu Byte), die Datei hat "
                "%zu Byte Nutzdaten — der Widerspruch ginge verloren. Mit "
                "accept_data_loss moeglich.",
                (unsigned)paragraphs, (size_t)paragraphs * 16u, payload);
        return UFT_ERR_NOT_SUPPORTED;
    }
    if (!lossless)
        uftc_add_warning(result,
            "mit Zustimmung gewandelt: Sektorgroesse %u und Kopf-Reserve gehen "
            "verloren", (unsigned)sector_size);

    uint8_t* buf = (uint8_t*)malloc(payload ? payload : 1u);
    if (!buf) { result->error = UFT_ERR_MEMORY; return UFT_ERR_MEMORY; }
    memcpy(buf, src_data + UFTC_ATR_HEADER, payload);
    *out = buf; *out_size = payload;

    uftc_add_warning(result,
        "ATR->XFD: %zu Byte Nutzdaten unveraendert, 16 Byte Kopf entfernt "
        "(Sektorgroesse %u)", payload, (unsigned)sector_size);
    return UFT_OK;
}

/**
 * @brief XFD -> ATR im Speicher: den 16-Byte-Kopf erzeugen.
 *
 * XFD traegt keine Sektorgroesse. Hier wird 128 gesetzt — der Atari-Standard
 * fuer Single Density — und das AUSGESPROCHEN, statt es stillschweigend
 * anzunehmen.
 */
static uft_error_t uftc_xfd_to_atr_mem(const uint8_t* src_data, size_t src_size,
                                        const uft_convert_options_ext_t* opts,
                                        uft_convert_result_t* result,
                                        uint8_t** out, size_t* out_size) {
    (void)opts;
    *out = NULL; *out_size = 0;

    if (src_size == 0) {
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result, "XFD ist leer");
        return UFT_ERR_FORMAT;
    }
    if (src_size % 16u != 0u) {
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result,
            "XFD-Groesse %zu ist kein Vielfaches von 16 — der ATR-Kopf zaehlt "
            "in Paragraphen zu 16 Byte und koennte sie nicht darstellen",
            src_size);
        return UFT_ERR_FORMAT;
    }

    uint32_t paragraphs = (uint32_t)(src_size / 16u);
    if (paragraphs > 0xFFFFFFu) {
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result,
            "XFD zu gross: %u Paragraphen passen nicht in die 24 Bit des "
            "ATR-Kopfes", (unsigned)paragraphs);
        return UFT_ERR_FORMAT;
    }

    uint8_t* buf = (uint8_t*)malloc(UFTC_ATR_HEADER + src_size);
    if (!buf) { result->error = UFT_ERR_MEMORY; return UFT_ERR_MEMORY; }
    memset(buf, 0, UFTC_ATR_HEADER);
    buf[0] = (uint8_t)(UFTC_ATR_MAGIC & 0xFF);
    buf[1] = (uint8_t)(UFTC_ATR_MAGIC >> 8);
    buf[2] = (uint8_t)(paragraphs & 0xFF);
    buf[3] = (uint8_t)((paragraphs >> 8) & 0xFF);
    buf[4] = 128; buf[5] = 0;                 /* Sektorgroesse 128, siehe oben */
    buf[6] = (uint8_t)((paragraphs >> 16) & 0xFF);
    memcpy(buf + UFTC_ATR_HEADER, src_data, src_size);
    *out = buf; *out_size = UFTC_ATR_HEADER + src_size;

    uftc_add_warning(result,
        "XFD->ATR: %zu Byte unveraendert, 16-Byte-Kopf erzeugt. SEKTORGROESSE "
        "AUF 128 GESETZT — XFD speichert sie nicht, und aus der Dateigroesse "
        "folgt sie nicht. Bei einem Double-Density-Abbild ist diese Angabe "
        "falsch und gehoert von Hand berichtigt.", src_size);
    return UFT_OK;
}

/* --- Verpackung fuer den Dateiweg -------------------------------------- */

uft_error_t uftc_convert_atr_to_xfd(const uint8_t* src_data, size_t src_size,
                                     const char* dst_path,
                                     const uft_convert_options_ext_t* opts,
                                     uft_convert_result_t* result) {
    uftc_report_progress(opts, 20, "ATR-Kopf lesen");
    uint8_t* buf = NULL; size_t n = 0;
    uft_error_t e = uftc_atr_to_xfd_mem(src_data, src_size, opts, result,
                                        &buf, &n);
    if (e != UFT_OK) return e;
    uftc_report_progress(opts, 70, "XFD schreiben");
    /* VOR finish_or_refuse: die Funktion lehnt bei
     * tracks_converted == 0 ab (MF-545) — sie soll ja gerade
     * verhindern, dass eine leere Wandlung als Erfolg gilt.
     * Ein Abbild ist hier genau eine Einheit. */
    result->tracks_converted = 1;
    e = uftc_finish_or_refuse(result, dst_path, buf, n, "XFD-Sektordaten");
    free(buf);
    if (e != UFT_OK) { result->tracks_converted = 0; return e; }
    return UFT_OK;
}

uft_error_t uftc_convert_xfd_to_atr(const uint8_t* src_data, size_t src_size,
                                     const char* dst_path,
                                     const uft_convert_options_ext_t* opts,
                                     uft_convert_result_t* result) {
    uftc_report_progress(opts, 20, "ATR-Kopf erzeugen");
    uint8_t* buf = NULL; size_t n = 0;
    uft_error_t e = uftc_xfd_to_atr_mem(src_data, src_size, opts, result,
                                        &buf, &n);
    if (e != UFT_OK) return e;
    uftc_report_progress(opts, 70, "ATR schreiben");
    /* VOR finish_or_refuse: die Funktion lehnt bei
     * tracks_converted == 0 ab (MF-545) — sie soll ja gerade
     * verhindern, dass eine leere Wandlung als Erfolg gilt.
     * Ein Abbild ist hier genau eine Einheit. */
    result->tracks_converted = 1;
    e = uftc_finish_or_refuse(result, dst_path, buf, n,
                              "ATR mit erzeugtem Kopf");
    free(buf);
    if (e != UFT_OK) { result->tracks_converted = 0; return e; }
    return UFT_OK;
}

/* --- Verpackung fuer den Speicherweg ------------------------------------ */

uft_error_t uftc_atr_xfd_memory(uft_format_t src_format,
                                 uft_format_t dst_format,
                                 const uint8_t* src_data, size_t src_size,
                                 const uft_convert_options_ext_t* opts,
                                 uft_convert_result_t* result,
                                 uint8_t** out, size_t* out_size) {
    if (src_format == UFT_FORMAT_ATR && dst_format == UFT_FORMAT_XFD)
        return uftc_atr_to_xfd_mem(src_data, src_size, opts, result,
                                   out, out_size);
    if (src_format == UFT_FORMAT_XFD && dst_format == UFT_FORMAT_ATR)
        return uftc_xfd_to_atr_mem(src_data, src_size, opts, result,
                                   out, out_size);
    return UFT_ERR_NOT_SUPPORTED;
}

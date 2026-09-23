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
#include "uft/uft_mfm_encoder.h"
#include "uft/uft_amiga_mfm_encoder.h"
/* MF-1168: die Profiltafel des FDC. Sie fuehrt rpm und raw_bits fuer 17
 * Geometrien und hatte bis zu diesem Commit keinen einzigen Aufrufer. */
#include "uft/formats/uft_fdc_gaps.h"
/* P3-423: die IMG-Geometrie kommt aus dem BPB der Diskette, nicht aus
 * Groessenbereichen. Der Leser und die acht Geometriezeilen liegen seit
 * Jahren daneben und hatten keinen Produktivaufrufer. */
#include "uft/formats/fat/uft_fat_bootsector.h"

/* SCP speichert Flusslaengen als 16-Bit-Vielfache von 25 ns. */
#ifndef UFT_SCP_TICK_NS
#define UFT_SCP_TICK_NS 25u
#endif

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
        uftc_add_warning(result,
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

    /* Read track LUT.
     *
     * MF-526: hier stand keine einzige Schranke. Alle drei Groessen kommen
     * aus der Datei und duerfen beliebig sein:
     *
     *   track_list_offset  -> die LUT konnte hinter dem Puffer beginnen
     *   n_cylinders        -> die LUT-Schleife konnte hinter ihr Ende laufen
     *   lut[].offset/length-> ein Track konnte hinter dem Puffer liegen
     *
     * Bei einer auf 300 Byte gekuerzten HFE liegt die LUT laut Kopf bei
     * Byte 512 — der erste Zugriff war also schon daneben. Gefunden von
     * tests/test_convert_fuzz.c. */
    const size_t lut_start = (size_t)hdr->track_list_offset * 512;
    if (lut_start >= src_size ||
        (src_size - lut_start) / sizeof(hfe_track_entry_t) < (size_t)cylinders) {
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result, "HFE track list does not fit in the file");
        scp_writer_free(writer);
        return UFT_ERR_FORMAT;
    }
    const hfe_track_entry_t* lut = (const hfe_track_entry_t*)(src_data + lut_start);

    for (int cyl = 0; cyl < cylinders; cyl++) {
        if (uftc_is_cancelled(opts)) break;

        uint16_t track_offset_blocks = lut[cyl].offset;
        uint16_t track_len = lut[cyl].length;

        if (track_offset_blocks == 0 || track_len == 0) continue;

        /* Liegt der Track ueberhaupt in der Datei? Ein Kopf, der auf
         * Daten zeigt, die es nicht gibt, ist missgebildet — nicht eine
         * Einladung, dort zu lesen. */
        const size_t track_start = (size_t)track_offset_blocks * 512;
        if (track_start >= src_size || src_size - track_start < track_len) {
            result->tracks_failed++;
            continue;
        }

        const uint8_t* interleaved = src_data + track_start;

        for (int hd = 0; hd < heads; hd++) {
            /* De-interleave to get single-head track data */
            /* MF-526: `lut[].length` ist die Laenge des interleavten
             * Blocks, also BEIDER Seiten zusammen. `hfe_deinterleave_track()`
             * und `hfe_track_blocks()` erwarten die Laenge JE SEITE — beide
             * rechnen in 256-Byte-Haelften und schreiten in 512er-Bloecken.
             *
             * Hier wurde die Gesamtlaenge uebergeben. Damit verdoppelte sich
             * die Blockzahl, und die Schleife las weit hinter den Track
             * hinaus: bei gw_amigados.hfe (track_len 25336, Zylinder 79 ab
             * Byte 2023424) bis Byte 2074104 — 25080 Byte hinter dem
             * Dateiende von 2049024. Absturz auf einer GUELTIGEN Datei,
             * gefunden von tests/test_convert_fuzz.c.
             *
             * Belegt durch die Datei selbst: eine Amiga-DD-Spur hat rund
             * 12500 Byte je Seite, 25336 ist das Doppelte.
             *
             * BERICHTIGT MF-1125. Hier stand zusaetzlich: „Belegt durch
             * den Leser, der es richtig macht:
             * src/formats/hfe/uft_hfe.c::deinterleave_track schreitet
             * `pos += 512` und gibt jeder Seite 256 Byte, also
             * Gesamtlaenge / 2." Der Satz stimmte fuer die VOLLEN
             * Bloecke und nicht fuer den Rest: bei `track_len = 25336`
             * bleiben 248 Byte, und `deinterleave_track()` gab sie
             * ungeteilt der Seite 1 — Seite 0 verlor ihre letzten 124
             * echten Bytes, Seite 1 bekam 124 Byte ECHTEN FLUSS DER
             * ANDEREN SEITE plus 124 Byte deren Polster. Gemessen:
             * 12544 statt 12668 und 12792 statt 12668.
             *
             * Der genannte Leser war also der einzige im Baum, der es
             * FALSCH machte; richtig war und ist `hfe_deinterleave_track()`
             * hier. Ein Kommentar, der eine ANDERE Datei fuer geprueft
             * erklaert, ist eine Aussage ueber diese Datei — und diese
             * war ungeprueft. Behoben und festgehalten in
             * `tests/test_hfe_spurende.c`. */
            const uint16_t head_len = (uint16_t)(track_len / 2);
            if (head_len == 0) continue;
            uint8_t* track_bits = malloc(head_len);
            if (!track_bits) continue;

            hfe_deinterleave_track(interleaved, head_len, (uint8_t)hd,
                                    track_bits);

            /* Convert bitstream to flux transitions */
            /* HFE stores bits LSB-first, so reverse each byte */
            hfe_reverse_bits(track_bits, head_len);

            /* Convert each '1' bit to a flux transition */
            uint32_t flux_buf[131072];
            size_t flux_count = 0;
            size_t flux_dropped = 0;
            uint32_t accum_ns = 0;

            for (size_t byte_i = 0; byte_i < head_len; byte_i++) {
                uint8_t b = track_bits[byte_i];
                for (int bit = 7; bit >= 0; bit--) {
                    accum_ns += cell_ns;
                    if ((b >> bit) & 1) {
                        if (flux_count < 131072) {
                            flux_buf[flux_count++] = accum_ns;
                        } else {
                            flux_dropped++;
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
            /* MF-555: die Antwort des Schreibers wird gelesen.
             *
             * Sie wurde verworfen. Schlaegt das Schreiben fehl, fehlt die
             * Spur im Abbild — und der Wandler zaehlte sie trotzdem als
             * gewandelt. Dieselbe Sache wie MF-545, eine Ebene tiefer:
             * dort folgte der Erfolg daraus, dass sich die DATEI anlegen
             * liess, hier daraus, dass der Aufruf zurueckkam.
             *
             * Gefunden von scripts/audit_discarded_result.py, gebaut nach
             * dem dritten Fall von "eine Stelle repariert, das Geschwister
             * nicht" (MF-526, MF-550, MF-554). Von fuenf Aufrufen des
             * SCP-Schreibers pruefte genau einer. */
            int add_rc = 0;
            for (int rev = 0; rev < revolutions; rev++) {
                if (scp_writer_add_track(writer, cyl, hd, flux_buf,
                                          flux_count, duration_ns, rev) != 0)
                    add_rc = -1;
            }
            if (add_rc != 0) {
                uftc_add_warning(result,
                         "HFE->SCP Spur %d Kopf %d: der SCP-Schreiber hat "
                         "die Spur abgewiesen — sie fehlt im Abbild "
                         "(MF-555)", cyl, hd);
                result->tracks_failed++;
                continue;
            }

            free(track_bits);

            /* MF-550: die Kappung wird GEZAEHLT, nicht verschwiegen.
             *
             * Vorher stand oben nur `if (flux_count < 131072) ...` und
             * sonst nichts: was nicht mehr hineinpasste, verschwand ohne
             * Warnung, ohne Zaehler, ohne dass die Spur als unvollstaendig
             * galt.
             *
             * Erreichbar ist das. `track_len` ist ein uint16 aus der Datei,
             * also bis 32767 Byte je Seite; 32767 x 8 = 262136 moegliche
             * Uebergaenge gegen einen Deckel von 131072. Eine normale Spur
             * (12500 Byte, 100000 Zellen) bleibt darunter — eine
             * praeparierte oder sehr dicht beschriebene nicht.
             *
             * Schlimmer als das blosse Fehlen: `accum_ns = 0` lief weiter
             * mit. Der Zeitbezug hinter der Kappung stimmt also auch dann
             * nicht, wenn spaeter wieder Platz gewesen waere. Was
             * herauskommt, ist nicht "etwas weniger Daten", sondern keine
             * verwertbare Aussage mehr — deshalb gilt die Spur als
             * gescheitert und nicht als gewandelt.
             *
             * Vorbild: MF-528 in uft_format_convert_flux.c (SCP->HFE), wo
             * genau das seit einer Weile richtig steht. Drei Stellen
             * hatten es nicht. */
            if (flux_dropped) {
                uftc_add_warning(result,
                         "HFE->SCP Spur %d Kopf %d: %zu von %zu "
                         "Flusswechseln verworfen (Puffer fasst 131072) — "
                         "die Spur ist unvollstaendig und zaehlt als "
                         "gescheitert (MF-550)",
                         cyl, hd, flux_dropped, flux_count + flux_dropped);
                result->tracks_failed++;
            } else {
                result->tracks_converted++;
            }
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
        uftc_add_warning(result,
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

        /* MF-1332: hier stand `track * 2`. MF-928 hat die Konvention auf
         * „Dateieintrag i IST Platz i" umgestellt und dafuer
         * `G64_TRACK_TO_HALFTRACK()` eingefuehrt — in der eigenen Datei
         * an drei Stellen nachgezogen, die drei Stellen hier uebersehen
         * (MF-519/MF-529). Gemessen las diese Schleife damit die echten
         * Spuren 2..35 unter den Namen 1..34; Spur 1 liegt auf Platz 0
         * und wurde nie gefragt. */
        int halftrack = G64_TRACK_TO_HALFTRACK(track);
        const uint8_t* track_data = NULL;
        size_t track_len = 0;
        uint8_t speed = 0;

        rc = g64_get_track(g64, halftrack, &track_data, &track_len, &speed);
        if (rc != 0 || !track_data || track_len == 0) {
            /* MF-1332: hier stand ein nacktes `continue;`. Was der
             * Wandler nicht wandelt, muss er als gescheitert melden —
             * sonst steht die Spur weder in `tracks_converted` noch in
             * `tracks_failed` und ist still verschwunden.
             *
             * Gemessen am Vorzustand, Quelle
             * `tests/corpus_free/vice_c1541_35trk.g64`, BEIDE Wandler
             * ueber dieselbe Datei und dieselbe Schleifengrenze
             * (`track <= 42`):
             *
             *   G64->HFE   34 gewandelt,  8 gescheitert  = 42
             *   G64->SCP   34 gewandelt,  0 gescheitert  = 34
             *
             * Acht Spuren fielen hier stumm heraus, und der Bericht
             * meldete Erfolg.
             *
             * Das Gegenbeispiel stand die ganze Zeit in DIESER Datei,
             * gut zweihundert Zeilen weiter unten (`:548`): derselbe
             * Test, mit `result->tracks_failed++`. Bauform MF-1177
             * (eine Groesse, zwei Rechnungen), verschraenkt mit
             * MF-519/MF-529 (eine Stelle geholt, den Nachbarn
             * uebersehen). */
            result->tracks_failed++;
            continue;
        }

        /* Get bit time for this track's speed zone */
        d64_speed_zone_t zone = d64_track_zone(track);
        double bit_time_us = d64_zone_bit_time(zone);
        uint32_t cell_ns = (uint32_t)(bit_time_us * 1000.0);

        /* Convert GCR bitstream to flux using d64_gcr_to_flux() */
        uint32_t flux_buf[131072];
        size_t flux_count = 0;
        size_t flux_dropped = 0;   /* MF-550 */

        rc = d64_gcr_to_flux(track_data, track_len, zone,
                              flux_buf, &flux_count);

        /* MF-537: EINHEITEN. `d64_gcr_to_flux()` liefert SCP-TICKS — sein
         * eigener Kommentar sagt es: "Convert to SCP ticks (25ns
         * resolution)", und es rechnet `bit_time * 1000.0 / 25.0`.
         * `scp_writer_add_track()` erwartet aber NANOSEKUNDEN; so fuettert
         * es auch uftc_convert_hfe_to_scp(), und so tut es der
         * Rueckfallpfad direkt darunter (`accum_ns += cell_ns`).
         *
         * Zwei Einheiten im selben Puffer, je nachdem welcher Pfad lief.
         * Der Hauptpfad gab Ticks als Nanosekunden weiter, der Writer
         * teilte sie noch einmal durch 25 — und die Rechnung geht exakt
         * auf:
         *
         *     4000 ns Zellzeit  ->  160 Ticks
         *     als "ns" uebergeben -> 160 / 25 = 6 Ticks
         *     zurueckgelesen      -> 6 * 25 = 150 ns
         *
         * Genau diese 150 ns kamen im Rundlauf G64 -> SCP -> G64 beim
         * Leser an, wo 4000 erwartet wurden. Der PLL fand mit einer
         * Zellzeit von 3250 ns in 150-ns-Abstaenden keine einzige
         * Zellgrenze und lieferte 1 Bit je Spur; die G64 war 789 Byte gross
         * und meldete trotzdem "35 Spuren gewandelt" (MF-534, OPEN_ITEMS
         * P0-15).
         *
         * Belegt durch Messung an der Zwischendatei
         * (tests/test_convert_roundtrip_measured.c). */
        if (rc == 0 && flux_count > 0) {
            for (size_t i = 0; i < flux_count; i++)
                flux_buf[i] *= UFT_SCP_TICK_NS;
        }

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
                        } else {
                            flux_dropped++;
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
        /* MF-555: siehe die Begruendung bei HFE->SCP weiter oben. */
        int add_rc = 0;
        for (int rev = 0; rev < revolutions; rev++) {
            if (scp_writer_add_track(writer, track - 1, 0, flux_buf,
                                      flux_count, duration_ns, rev) != 0)
                add_rc = -1;
        }
        if (add_rc != 0) {
            uftc_add_warning(result,
                     "G64->SCP Spur %d: der SCP-Schreiber hat die Spur "
                     "abgewiesen — sie fehlt im Abbild (MF-555)", track);
            result->tracks_failed++;
            continue;
        }

        /* MF-550: siehe die ausfuehrliche Begruendung bei HFE->SCP weiter
         * oben. Dieselbe stille Kappung, dieselbe Folge. */
        if (flux_dropped) {
            uftc_add_warning(result,
                     "G64->SCP Spur %d: %zu von %zu Flusswechseln verworfen "
                     "(Puffer fasst 131072) — die Spur ist unvollstaendig "
                     "und zaehlt als gescheitert (MF-550)",
                     track, flux_dropped, flux_count + flux_dropped);
            result->tracks_failed++;
        } else {
            result->tracks_converted++;
        }
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
        uftc_add_warning(result,
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
        /* MF-533: die LUT-Laenge ist die GESAMTLAENGE beider Seiten
         * (MF-526). `track_len_aligned` ist die je Seite — dieselbe
         * Datei reserviert damit `blocks_per_track =
         * (track_len_aligned * 2 + 511) / 512` und interleavt mit der
         * Seitenlaenge. Hier stand die halbe Zahl; ein Leser, der sie
         * als Gesamtlaenge nimmt, sieht die Haelfte.
         *
         * Gemessen am Rundlauf G64 -> HFE -> G64: 278234 Byte gingen
         * hinein, 139634 kamen heraus — genau die Haelfte
         * (tests/test_convert_roundtrip_measured.c). Dieselbe
         * Korrektur wie MF-528, nur an den drei uebrigen Schreibern. */
        lut[cyl].length = (uint16_t)(track_len_aligned * 2);
    }

    uftc_report_progress(opts, 30, "Converting GCR tracks to HFE");

    for (int track = 1; track <= num_tracks; track++) {
        if (uftc_is_cancelled(opts)) break;

        /* MF-1332: siehe die Leseschleife von `g64_to_scp` — dieselbe
         * uebersehene Umstellung aus MF-928. */
        int halftrack = G64_TRACK_TO_HALFTRACK(track);
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

    /* MF-545: schreiben ODER ablehnen. Vorher folgte `success`
     * allein daraus, dass sich die Datei anlegen liess. */
    uft_error_t err = uftc_finish_or_refuse(result, dst_path,
                                            hfe_data, total_size, "G64->HFE");

    uftc_add_warning(result,
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
        uftc_add_warning(result,
                 "Invalid HFE header");
        return UFT_ERR_FORMAT;
    }

    int cylinders = hdr->n_cylinders;
    if (cylinders > 42) cylinders = 42; /* G64 max 42 tracks */

    /* Warn if this doesn't look like a C64 disk */
    if (hdr->uft_floppy_interface != HFE_IF_C64_DD &&
        hdr->uft_floppy_interface != (uint8_t)HFE_IF_GENERIC_SHUGART) {
        uftc_add_warning(result,
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

    /* Read track LUT — dritte Kopie derselben Rechnung (MF-526). Alle drei
     * hatten dieselbe fehlende Schranke; dass es drei sind, ist das
     * eigentliche Problem. */
    const size_t lut_start = (size_t)hdr->track_list_offset * 512;
    if (lut_start >= src_size ||
        (src_size - lut_start) / sizeof(hfe_track_entry_t) < (size_t)cylinders) {
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result, "HFE track list does not fit in the file");
        return UFT_ERR_FORMAT;
    }
    const hfe_track_entry_t* lut = (const hfe_track_entry_t*)(src_data + lut_start);

    for (int cyl = 0; cyl < cylinders; cyl++) {
        if (uftc_is_cancelled(opts)) break;

        uint16_t track_offset_blocks = lut[cyl].offset;
        uint16_t track_len = lut[cyl].length;

        if (track_offset_blocks == 0 || track_len == 0) {
            result->tracks_failed++;
            continue;
        }

        const size_t track_start = (size_t)track_offset_blocks * 512;
        if (track_start >= src_size || src_size - track_start < track_len) {
            result->tracks_failed++;
            continue;
        }

        const uint8_t* interleaved = src_data + track_start;

        /* De-interleave head 0 (C64 is single-sided) */
        /* MF-526: siehe oben — lut[].length ist die Gesamtlaenge
         * beider Seiten, hfe_deinterleave_track() will die je Seite. */
        const uint16_t head_len = (uint16_t)(track_len / 2);
        if (head_len == 0) continue;
        uint8_t* track_bits = malloc(head_len);
        if (!track_bits) {
            result->tracks_failed++;
            continue;
        }

        hfe_deinterleave_track(interleaved, head_len, 0, track_bits);

        /* HFE stores bits LSB-first, reverse to MSB-first for G64 */
        hfe_reverse_bits(track_bits, head_len);

        /* Determine effective track length (trim trailing zeros) */
        size_t effective_len = head_len;
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

        /* Store in G64. MF-1332: hier stand `track_num * 2` samt einem
         * Kommentar, der die alte Rechnung als Regel wiederholte. Die
         * Schreibseite traegt denselben Versatz wie die Leseseite —
         * Spur 1 waere auf Platz 2 gelandet und Platz 0 leer geblieben,
         * also genau das Muster, das MF-928 abgestellt hat. */
        int halftrack = G64_TRACK_TO_HALFTRACK(track_num);
        /* MF-555: die Antwort von `g64_set_track()` wurde verworfen —
         * eine abgewiesene Spur fehlte im Abbild und galt trotzdem als
         * geschrieben. Dass sie jetzt gelesen wird, ist richtig.
         *
         * BERICHTIGT MF-1333. Hier stand: "`g64_set_track()` weist
         * Halbspuren und zu lange Spuren ab (MF-534)". Gemessen an ihrem
         * Rumpf prueft sie ausschliesslich
         * `halftrack < 0 || halftrack >= G64_MAX_TRACKS` — eine
         * Bereichspruefung des INDEX. Eine LAENGENpruefung gibt es dort
         * nicht; `effective_len` geht unbesehen nach `malloc`/`memcpy`.
         *
         * Diese Stelle ist damit gegen eine zu lange Spur NICHT
         * gesichert, und das ist hier benannt statt verschwiegen (P3-535). Wer
         * aus Sektoren baut, bekommt die Absage seit MF-1333 von
         * `build_gcr_track()`; dieser Pfad nimmt einen fertigen
         * Bitstrom entgegen und hat sie nicht. */
        if (g64_set_track(g64, halftrack, track_bits, effective_len,
                          speed) != 0) {
            uftc_add_warning(result,
                     "Halbspur %d: der G64-Schreiber hat sie abgewiesen — "
                     "sie fehlt im Abbild (MF-555)", halftrack);
            result->tracks_failed++;
            continue;
        }

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

    uftc_add_warning(result,
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
    /* MF-1166: die Drehzahl gehoert zum MEDIUM und stand fest auf 300.
     *
     * MF-539 hat die richtige Regel festgestellt — „die Spurlaenge ergibt
     * sich aus Bitrate und Drehzahl" — und die Drehzahl dann als Literal
     * in die Rechnung geschrieben. Die Geometriewahl unten waehlt aber
     * einen Fall, der 360 braucht: 1 228 800 Byte = 80 x 2 x 15 x 512 ist
     * die 1,2-MB-Diskette im 5,25-Zoll-HD-Laufwerk, und die dreht mit
     * **360** U/min. Gemessen am Vorzustand bekam sie eine Spur von
     * 25 000 statt 20 834 Byte — 20 % zu lang — und der Kopf behauptete
     * 300 U/min. `IMG->HFE` ist einer der sieben als VERLUSTFREI
     * angebotenen Wandlungspfade (MF-541/567).
     *
     * Die Bitrate allein entscheidet es NICHT: 5,25" HD und 3,5" HD sind
     * beide 500 kbit/s und drehen verschieden. Deshalb steht die Drehzahl
     * neben der Geometrie, wo sie hergeleitet wird, und nicht in der
     * Rechnung.
     *
     * Belegt durch `cw2dmk/jv3.h` (GPL-2, nur GELESEN — Kanal *Spec*), das
     * die Herleitung im Kommentar mitfuehrt, als DATENbytes:
     *     TRKSIZE_DD    6250    250kHz / 5 Hz [300rpm] / 8
     *     TRKSIZE_5HD  10416    500kHz / 6 Hz [360rpm] / 8
     *     TRKSIZE_3HD  12500    500kHz / 5 Hz [300rpm] / 8
     * UFT zaehlt Zellbytes, also das Doppelte — 2 x 12 500 = 25 000 und
     * 2 x 6 250 = 12 500 treffen genau, 2 x 10 416 = 20 832 gegen unsere
     * 20 834 (Ganzzahlteilung 30 000 000 / 360). Zwei unabhaengige
     * Rechenwege, dieselben Zahlen. */
    unsigned rpm = 300;
    hfe_track_encoding_t encoding = HFE_ENC_ISOIBM_MFM;
    hfe_floppy_interface_t iface = HFE_IF_IBMPC_DD;

    if (src_format == UFT_FORMAT_ADF) {
        /* MF-539: hier stand eine Amiga-Geometrie, und darunter kodierte die
         * IBM-System-34-Schleife. Der Kopf sagte AMIGA_MFM, der Inhalt war
         * IBM — und beides war ohnehin unkodiert (siehe unten).
         *
         * Gemessen gegen die echte Aufnahme tests/corpus_free/gw_amigados.hfe:
         *
         *                          echte HFE        unsere Ausgabe
         *      Sync 0x4489              22                     0
         *      haeufigstes Byte    0x55 (12498)      0x4E (6712)
         *      rohe Nullbytes        0 von 12792   5969 von 12800
         *
         * Fuer AmigaDOS gibt es in diesem Baum keinen Encoder — `grep -rln
         * 0x4489 src/ --include=*.c` findet Dekoder und Schutz-Erkennung,
         * aber nichts, was eine Amiga-Spur schreibt. Solange das so ist,
         * wird die Wandlung ABGELEHNT statt eine Datei zu erzeugen, die
         * niemand lesen kann. Das ist die Regel aus UFT-A02: lieber ein
         * ehrlicher Fehler als eine plausible Datei.
         *
         * Die Gegenrichtung HFE -> ADF ist davon nicht betroffen; sie geht
         * ueber den AmigaDOS-Dekoder und liefert die Quell-ADF byteweise
         * zurueck (tests/test_convert_hfe_adf.c). */
        /* MF-1081: der Encoder ist da, und die Absage faellt.
         *
         * `src/core/uft_amiga_mfm_encoder.c` ist die exakte Umkehrung
         * des baumeigenen, bereits abgenommenen Dekoders
         * `decode_amiga_sector()`. Abgenommen wurde er nicht gegen sich
         * selbst, sondern gegen die ECHTE Aufnahme, an der MF-539 die
         * Zahlen oben gemessen hat: aus `gw_amigados.hfe` dekodiert,
         * neu kodiert, und **11 von 11 Sektoren stehen byteidentisch**
         * in der Originalspur — je 1084 Byte samt Sync, Info,
         * Label, beiden Pruefsummen und jedem Taktbit.
         *
         * Das hat nebenbei einen Widerspruch zwischen zwei Referenzen
         * entschieden: Keir Frasers `libdisk` (Public Domain) nennt die
         * Feldreihenfolge `info_even, info_odd`, UFTs Dekoder nennt die
         * erste Haelfte `odd`. Es sind dieselben Bits — entschieden
         * am Objekt, nicht durch Auswahl zwischen zwei
         * Beschreibungen. */
        sector_size = 512;
        heads       = 2;
        encoding    = HFE_ENC_AMIGA_MFM;
        if (src_size >= 1802240u) {   /* HD: 80 x 2 x 22 x 512 */
            sectors = 22; bitrate = 500; iface = HFE_IF_AMIGA_HD;
        } else {                      /* DD: 80 x 2 x 11 x 512 */
            sectors = 11; bitrate = 250; iface = HFE_IF_AMIGA_DD;
        }
        {
            const size_t spur = (size_t)heads * sectors * sector_size;
            /* Der REST, nicht nur der Quotient. Der erste Entwurf hier
             * prueft nur `cylinders <= 0 || > 84` — und eine Datei
             * von 901 119 Byte ergibt ganzzahlig 79 Zylinder, wurde
             * also angenommen und eine Spur still verworfen. Gefunden
             * hat das die Gegenprobe in
             * `tests/test_convert_adf_hfe_roundtrip.c`, bevor der
             * Commit lag. */
            cylinders = (spur && src_size % spur == 0)
                        ? (int)(src_size / spur) : 0;
        }
        if (cylinders <= 0 || cylinders > 84) {
            /* Keine Amiga-Geometrie, die ein Laufwerk erreicht. Absagen
             * statt eine Zahl zu erfinden (MF-1073). */
            result->error = UFT_ERR_INVALID_FORMAT;
            uftc_add_warning(result,
                     "ADF->HFE: file size is not a whole number of Amiga "
                     "tracks (11 or 22 sectors x 512 bytes x 2 heads), "
                     "so the geometry cannot be derived; refused rather "
                     "than guessed (MF-1081).");
            return UFT_ERR_INVALID_FORMAT;
        }
    } else {
        /* IMG: die Geometrie steht IN der Diskette (P3-423).
         *
         * Hier standen vier `else if`-Bereiche ueber der Dateigroesse, und
         * MF-1174 hat gemessen, was sie treffen: von zwoelf echten
         * Diskettengroessen **genau vier** (368 640 / 737 280 / 1 228 800 /
         * 1 474 560). Die anderen acht bekamen eine falsche Geometrie —
         * 160K (40x1x8x512) wurde 40x2x9x512, also Kopfzahl UND Sektorzahl
         * falsch; BBC DFS traf denselben Zweig; PC-98 2HD (77x2x8x1024)
         * wurde 80x2x18x512. Seit MF-1174 wird abgesagt statt falsch
         * gelesen — ehrlich, aber unfaehig.
         *
         * Eine FAT-Diskette BESCHREIBT SICH SELBST. Der BPB fuehrt
         * `bytes_per_sector` (0x0B), `total_sectors_16` (0x13),
         * `media_type` (0x15), `sectors_per_track` (0x18) und
         * `head_count` (0x1A); `fat_analyze_boot_sector()` liest das
         * alles, und `fat_find_geometry()` fuehrt acht Geometriezeilen.
         * Beide lagen im Baum und hatten **keinen** Produktivaufrufer —
         * dritte Auflage des Musters aus MF-1015 und MF-1168: die richtige
         * Tafel liegt da, daneben raet Code.
         *
         * Drei Regeln, und jede sagt bei Nichterfuellung AB:
         *
         *  (1) BPB lesbar UND mit der Dateigroesse vereinbar -> er gilt.
         *  (2) BPB lesbar, aber im WIDERSPRUCH zur Dateigroesse -> Absage.
         *      Zwischen zwei Aussagen derselben Diskette wird nicht
         *      geraten (MF-1039), und eine Geometrie, die nicht in die
         *      Datei passt, liest hinter das Dateiende (MF-1027).
         *  (3) kein lesbarer BPB -> EXAKTER Treffer in der benannten
         *      Achtzeilen-Tafel, sonst Absage. Das ist nicht dasselbe wie
         *      die alten `<=`-Bereiche: die acht Zeilen haben paarweise
         *      verschiedene Sektorsummen, ein Treffer ist also eindeutig,
         *      und 204 800 (BBC DFS) oder 409 600 (Apple 800K) treffen
         *      keine — sie werden abgesagt statt beansprucht.
         *
         * Von fremder Hand bestaetigt, ausgefuehrt und nicht gelesen:
         * hxcfe meldet fuer eine 163 840-Byte-Datei „40 tracks, 1 side(s),
         * 8 sectors/track, rpm:300" und schreibt eine IMD, in der 319 von
         * 319 selbstbenennenden Sektoren an ihrer eigenen Ortsmarke
         * stehen. floptool taugt hier NICHT als zweite Hand: es erkennt
         * die BPB-freie Fassung identisch, entscheidet also ueber die
         * Groesse — dieselbe Falle, die hier behoben wird. */
        fat_analysis_result_t fa;
        int bpb_lesbar = 0;

        if (fat_analyze_boot_sector(src_data, src_size, &fa) == 0
            && fa.has_boot_signature
            && (fa.bytes_per_sector == 128u || fa.bytes_per_sector == 256u
                || fa.bytes_per_sector == 512u
                || fa.bytes_per_sector == 1024u)
            && fa.sectors_per_track >= 1u && fa.sectors_per_track <= 63u
            && fa.head_count >= 1u && fa.head_count <= 2u
            && fa.total_sectors > 0u) {
            bpb_lesbar = 1;
        }

        if (bpb_lesbar) {
            size_t sagt = (size_t)fa.total_sectors * fa.bytes_per_sector;
            unsigned pro_zyl = (unsigned)fa.sectors_per_track
                             * (unsigned)fa.head_count;

            if (sagt != src_size || pro_zyl == 0u
                || fa.total_sectors % pro_zyl != 0u) {
                /* Regel (2): der Kopf widerspricht der Datei. */
                result->error = UFT_ERR_INVALID_FORMAT;
                uftc_add_warning(result,
                         "sectors->HFE: the BPB describes %u sectors x %u "
                         "bytes (%zu bytes) but the file has %zu, or %u "
                         "sectors do not divide into %u per cylinder. The "
                         "disk contradicts itself; refused rather than "
                         "choosing one of its two statements (P3-423, "
                         "MF-1039/MF-1027).",
                         (unsigned)fa.total_sectors,
                         (unsigned)fa.bytes_per_sector, sagt, src_size,
                         (unsigned)fa.total_sectors, pro_zyl);
                return UFT_ERR_INVALID_FORMAT;
            }
            cylinders   = (int)(fa.total_sectors / pro_zyl);
            heads       = (int)fa.head_count;
            sectors     = (int)fa.sectors_per_track;
            sector_size = (int)fa.bytes_per_sector;
        } else {
            /* Regel (3): kein lesbarer BPB — die benannte Tafel, exakt. */
            const fat_disk_geometry_t *g =
                fat_find_geometry((uint32_t)(src_size / 512u), 0u);

            if (!g || (size_t)g->total_sectors * g->bytes_per_sector
                      != src_size) {
                result->error = UFT_ERR_INVALID_FORMAT;
                uftc_add_warning(result,
                         "sectors->HFE: the image carries no readable FAT "
                         "BPB (no 0x55AA signature or implausible fields) "
                         "and its size %zu matches none of the eight "
                         "geometries in fat_find_geometry() exactly. "
                         "Refused rather than derived from a size range — "
                         "the old ranges hit 4 of 12 real sizes and gave "
                         "the other eight a wrong geometry (P3-423, "
                         "MF-1174).",
                         src_size);
                return UFT_ERR_INVALID_FORMAT;
            }
            cylinders   = (int)g->tracks;
            heads       = (int)g->heads;
            sectors     = (int)g->sectors_per_track;
            sector_size = (int)g->bytes_per_sector;
        }

        /* Bitrate und Drehzahl folgen der SPURBELEGUNG, nicht der
         * Dateigroesse — und nur, wo es eine Quelle gibt. Belegt durch
         * cw2dmk `jv3.h` (GPL-2, nur GELESEN — Kanal *Spec*), das die
         * Herleitung als DATENbytes je Spur mitfuehrt (MF-1166):
         *     250 kbit/s @ 300 U/min ->  6250
         *     500 kbit/s @ 360 U/min -> 10416
         *     500 kbit/s @ 300 U/min -> 12500
         *
         * Die Kapazitaet allein entscheidet die beiden HD-Faelle NICHT:
         * 18 x 512 = 9216 passt auch in 10 416. Unterschieden werden sie
         * deshalb an der Sektorzahl, und zwar nur fuer die ZWEI Faelle,
         * fuer die dieser Baum eine Quelle hat — 15 Sektoren sind die
         * 1,2-M-Diskette im 5,25-Zoll-HD-Laufwerk mit 360 U/min, 18 die
         * 1,44-M-Diskette mit 300. Alles andere ueber DD wird ABGESAGT,
         * samt seinen Zahlen: eine Drehzahl zu erfinden waere genau der
         * Verstoss aus MF-1077. Das trifft die 2,88-M-ED-Diskette
         * (36 x 512 = 18 432, 1000 kbit/s ohne Quelle im Baum) und
         * PC-98 2HD (8 x 1024 = 8192, dessen Drehzahl als P3-431
         * ausdruecklich widersprüchlich belegt ist). */
        {
            unsigned je_spur = (unsigned)sectors * (unsigned)sector_size;

            if (je_spur <= 6250u) {
                bitrate = 250; rpm = 300; iface = HFE_IF_IBMPC_DD;
            } else if (sectors == 15) {
                bitrate = 500; rpm = 360; iface = HFE_IF_IBMPC_HD;
            } else if (sectors == 18) {
                bitrate = 500; rpm = 300; iface = HFE_IF_IBMPC_HD;
            } else {
                result->error = UFT_ERR_INVALID_FORMAT;
                uftc_add_warning(result,
                         "sectors->HFE: geometry %d x %d x %d x %d needs "
                         "%u data bytes per track, which exceeds the "
                         "6250 of 250 kbit/s at 300 rpm, and %d sectors "
                         "per track is neither the sourced 15 (1.2M, 500 "
                         "kbit/s at 360 rpm) nor 18 (1.44M, 500 at 300). "
                         "No source in this tree names a rate for it, so "
                         "it is refused instead of invented (P3-423, "
                         "MF-1166/MF-1077).",
                         cylinders, heads, sectors, sector_size, je_spur,
                         sectors);
                return UFT_ERR_INVALID_FORMAT;
            }
        }
    }

    size_t expected_size = (size_t)cylinders * heads * sectors * sector_size;
    if (src_size < expected_size) {
        /* MF-1174: hier stand eine WARNUNG, und sie hat den falschen Fehler
         * beschrieben.
         *
         * MF-1173 hat sie schon einmal berichtigt — sie sagte „padding with
         * zeros", gefuellt wird mit 0xE5 — und P3-422 hat verlangt, erst zu
         * MESSEN, ob die Fuellfaehigkeit ueberhaupt gebraucht wird. Die
         * Messung hat etwas anderes gefunden als die Frage.
         *
         * Gemessen fuer zwoelf echte Diskettengroessen, was die vier
         * `else if`-Bereiche der IMG-Erkennung oben daraus machen:
         *
         *   160K  40x1x 8x512    163840 -> gelesen als 40x2x 9x512  +204800
         *   180K  40x1x 9x512    184320 -> gelesen als 40x2x 9x512  +184320
         *   BBC   80x1x10x256    204800 -> gelesen als 40x2x 9x512  +163840
         *   320K  40x2x 8x512    327680 -> gelesen als 40x2x 9x512   +40960
         *   360K  40x2x 9x512    368640 -> GENAU
         *   400K  80x1x10x512    409600 -> gelesen als 80x2x 9x512  +327680
         *   640K  80x2x16x256    655360 -> gelesen als 80x2x 9x512   +81920
         *   720K  80x2x 9x512    737280 -> GENAU
         *   800K  80x2x10x512    819200 -> gelesen als 80x2x15x512  +409600
         *   1,2M  80x2x15x512   1228800 -> GENAU
         *   PC-98 77x2x 8x1024  1261568 -> gelesen als 80x2x18x512  +212992
         *   1,44M 80x2x18x512   1474560 -> GENAU
         *
         * VIER VON ZWOELF. Und das „+" ist nicht die Nachricht: es war keine
         * Fuellung, sondern eine FALSCHE GEOMETRIE, die die Fuellwarnung
         * verdeckt hat. Ein einseitiges 8-Sektor-Abbild als 40x2x9 gelesen
         * legt jeden Sektor hinter Spur 0 Kopf 0 an die falsche Stelle —
         * `heads` 2 statt 1, `sectors` 9 statt 8. Bei 160K waeren 204 800
         * von 368 640 Byte erfunden gewesen, mehr als die Diskette hat.
         * Klasse MF-1016 / MF-1026 / MF-1037.
         *
         * Die Absage ist damit SYMMETRISCH zu der nach oben (MF-1170). Fuer
         * ADF kann keine der beiden greifen, und das ist gemessen: dort ist
         * `cylinders` aus `src_size / spur` MIT Restpruefung abgeleitet
         * (MF-1081), also gilt `expected_size == src_size` genau.
         *
         * DIE BEHEBUNG LIEGT SCHON IM BAUM, und die Meldung nennt sie:
         * `src/formats/fat/uft_fat_bootsector.c` fuehrt acht Geometrien
         * (`fat_geometry_160k` bis `_2880k`) plus
         * `fat_find_geometry(total_sectors, media_byte)` — genau das, was
         * die Bereiche oben raten, und ohne Produktionsaufrufer. Der Umbau
         * (Geometrie aus dem BPB der Diskette lesen) steht als P3-423 und
         * gehoert je Groesse abgenommen, nicht im Vorbeigehen eingebaut. */
        result->error = UFT_ERR_INVALID_FORMAT;
        uftc_add_warning(result,
                 "sectors->HFE: source size %zu does not fill the derived "
                 "geometry %d x %d x %d x %d (%zu bytes); %zu bytes would be "
                 "invented with the fill byte 0xE5 and reported as sector "
                 "data. Refused rather than padded (MF-1174). The size "
                 "buckets only handle 368640, 737280, 1228800 and 1474560 "
                 "exactly; `fat_find_geometry()` in "
                 "src/formats/fat/uft_fat_bootsector.c knows eight "
                 "geometries and has no caller yet (P3-423).",
                 src_size, cylinders, heads, sectors, sector_size,
                 expected_size, expected_size - src_size);
        return UFT_ERR_INVALID_FORMAT;
    }

    /* MF-1170: ein ZUVIEL war bisher still — und bei einer 2,88-M-Diskette
     * war es die HAELFTE.
     *
     * Die Bedingung darueber prueft nur `src_size < expected_size`. Der
     * letzte Groessenzweig der IMG-Erkennung ist ein `else` ohne obere
     * Schranke, also wurde JEDE Datei ueber 1 228 800 Byte zu
     * 80 x 2 x 18 x 512, und die Sektorschleife rechnet
     *
     *     src_offset = ((cyl * heads * sectors) + hd * sectors + sec)
     *                  * sector_size,        mit cyl < cylinders
     *
     * erreicht also hoechstens `expected_size`. Gemessen an einer
     * 2,88-M-ED-Datei (80 x 2 x 36 x 512 = 2 949 120 Byte): 1 474 560 Byte
     * wurden nie angefasst — genau die Haelfte, weil 36 = 2 x 18 — und
     * `UFT_OK` kam zurueck. Klasse MF-1001/1022/1038/1040, und ein Verstoss
     * gegen „Kein Bit verloren" auf dem Hauptpfad.
     *
     * DIE ADF-SEITE DERSELBEN FUNKTION PRUEFT DAS SEIT MF-1081, die
     * IMG-Seite hat es nie bekommen: dort steht ausdruecklich „Der REST,
     * nicht nur der Quotient", weil 901 119 Byte ganzzahlig 79 Zylinder
     * ergaben und still eine Spur verwarfen. Zwei Zweige einer Funktion,
     * einer gehaertet — Klasse MF-519/529/1026, und MF-1164 hat dieselbe
     * Gestalt schon einmal INNERHALB einer Datei gefunden.
     *
     * Fuer ADF kann die Absage nicht greifen, und das ist kein Zufall:
     * dort ist `cylinders` aus `src_size / spur` MIT Restpruefung
     * abgeleitet, also gilt `expected_size == src_size` genau. */
    if (src_size > expected_size) {
        result->error = UFT_ERR_INVALID_FORMAT;
        if (src_size == 2949120u) {
            /* Die naheliegendste Falle, darum mit Namen statt allgemein
             * (MF-1079/P3-361: die Zahlen nennen, nicht pauschal absagen).
             * Sie scheitert NICHT an der Diskette — 36 x (512 + 62) + 146
             * = 20 810 Byte passen in die 25 000-Byte-Spur, `gap3` 116 —,
             * sondern am Behaelter: `hfe_track_entry_t.length` ist ein
             * `uint16_t`, und ein ED-Spurpaar braucht 100 352 Byte. */
            uftc_add_warning(result,
                     "sectors->HFE: %zu bytes is a 2.88M ED disk "
                     "(80 x 2 x 36 x 512). HFE v1 cannot carry an ED track: "
                     "its track table holds a pair length in 16 bits and ED "
                     "needs 100352 bytes. Refused rather than silently "
                     "converting the first half (MF-1170).", src_size);
        } else {
            uftc_add_warning(result,
                     "sectors->HFE: source size %zu exceeds the %zu bytes of "
                     "the derived geometry %d x %d x %d x %d, so %zu bytes "
                     "would be dropped without notice. Refused rather than "
                     "truncated (MF-1170).",
                     src_size, expected_size, cylinders, heads, sectors,
                     sector_size, src_size - expected_size);
        }
        return UFT_ERR_INVALID_FORMAT;
    }

    uftc_report_progress(opts, 20, "Building HFE container");

    /*
     * Build the HFE file in memory:
     * 1. Header (512 bytes)
     * 2. Track lookup table (cylinders * 4 bytes, padded to 512)
     * 3. Track data blocks (interleaved head 0/1 in 256-byte chunks)
     */

    /* MF-539: die Spurlaenge ergibt sich aus Bitrate und Drehzahl, nicht aus
     * einer Schaetzung ueber die Sektorzahl.
     *
     * Eine Umdrehung dauert 60/rpm Sekunden. `bitrate` ist die DATENrate,
     * nicht die Zellrate — MFM legt zwischen je zwei Datenbits ein Taktbit,
     * der Zellenstrom ist also doppelt so lang. Bei 250 kbit/s und 300 U/min:
     *
     *      50000 Datenbits -> 100000 Zellen -> 12500 Byte Zellenstrom
     *
     * Gegenprobe an der echten Aufnahme tests/corpus_free/gw_amigados.hfe
     * (Amiga DD, Kopf meldet 253 kbit/s): 12792 Byte je Seite. Passt.
     *
     * Ohne die Verdopplung war die Spur exakt halb so lang, und in eine
     * 18-Sektor-Spur passten gemessen 10 Sektoren — die uebrigen 8 gab
     * `uft_mfm_encode_track()` nicht mehr aus. Der Rundlauf
     * tests/test_convert_img_hfe_roundtrip.c meldete 1600 statt 2880
     * zurueckgelesene Sektoren und 48,25 % abweichende Bytes.
     *
     * Vorher stand hier `sectors * (sector_size + 62) * 16 / 8`, eine
     * Schaetzung ueber die Sektorzahl. Sie fiel nicht auf, weil der Inhalt
     * ohnehin nicht kodiert war und die Laenge damit bedeutungslos. */
    /* MF-1168: die Zeiten kommen aus der Profiltafel, wo es ein Profil gibt
     * — nicht aus einer zweiten Herleitung.
     *
     * MF-1166 hat `rpm` hier eingefuehrt und damit unbemerkt eine ZWEITE
     * Stelle geschaffen, die dieselbe Zahl herleitet; die erste ist
     * `include/uft/formats/uft_fdc_gaps.h` mit 17 Profilen, die `rpm` und
     * `raw_bits` fuehren. Das ist die Klasse MF-1015 (drei Pruefsummen im
     * Baum, keine zwei gleich), und sie war hier besonders leicht zu
     * uebersehen, weil die Tafel keinen Aufrufer hatte (P3-204).
     *
     * Gemessen, dass beide Wege dasselbe sagen — fuer die vier Groessen,
     * die dieser Wandler entscheidet, liefert die Tafel genau die Werte,
     * die die Herleitung darunter rechnet:
     *
     *     40/2/ 9x512  PC 360K   rpm 300  raw 100000   (250 * 60/300 * 2)
     *     80/2/ 9x512  PC 720K   rpm 300  raw 100000
     *     80/2/15x512  PC 1.2M   rpm 360  raw 166666   (500 * 60/360 * 2)
     *     80/2/18x512  PC 1.44M  rpm 300  raw 200000   (500 * 60/300 * 2)
     *
     * Die 166666 stimmen einschliesslich der Abschneidung: 500000 * 60/360
     * sind 83333,33 Datenbits, abgeschnitten 83333, verdoppelt 166666.
     *
     * DER RUECKFALL BLEIBT, und er ist nicht Bequemlichkeit: fuer AmigaDOS
     * gibt es kein Profil. Gemessen antwortet die Tafel auf 80/2/11x512 und
     * 80/2/22x512 mit NULL — und genau diese zwei Geometrien setzt der
     * ADF-Zweig oben.
     *
     * NICHT von der Tafel genommen wird `bitrate`: ihr Feld `data_rate`
     * traegt bei den MFM-Eintraegen die Datenrate und bei `BBC DFS` die
     * Zellrate (siehe die Notiz an `UFT_FDC_FM_SD`). Solange das eine
     * Konventionsfrage ist, bleibt die Bitrate aus der eigenen Herleitung.
     *
     * Ein mehrfacher Treffer ist hier unschaedlich und gemessen: 80/2/18x512
     * trifft „PC 1.44M" UND „Atari ST HD", beide mit rpm 300 und raw 200000.
     * Gebraucht werden nur diese zwei Zahlen; Name und Luecken bleiben
     * unbeansprucht. */
    const uft_fdc_format_t *profil =
        uft_fdc_detect_format((uint8_t)cylinders, (uint8_t)heads,
                              (uint8_t)sectors, (uint16_t)sector_size);
    if (profil) {
        rpm = profil->rpm;
    }
    /* MF-1166: `rpm` statt des Literals 300 — siehe die Herleitung oben. */
    const int track_cells = profil
        ? (int)profil->raw_bits
        : (int)((uint32_t)bitrate * 1000u * 60u / (uint32_t)rpm * 2u);
    int mfm_track_bytes = (track_cells + 7) / 8;
    /* Round up to multiple of 256 for HFE interleaving */
    int track_len_aligned = ((mfm_track_bytes + 255) / 256) * 256;

    /* MF-1170: die Spurtabelle von HFE v1 traegt die Laenge eines
     * SPURPAARS in einem `uint16_t` — `hfe_track_entry_t.length`,
     * `include/uft/uft_hfe_format.h:115-118`. Mehr als 65 535 Byte kann sie
     * nicht ausdruecken, und die Zuweisung unten wuerde still kuerzen.
     *
     * Gemessen, was dieser Wandler heute setzen kann und was nicht:
     *
     *      250 kbps / 300 U/min  ->  12 544 je Seite  ->  Paar  25 088
     *      500 kbps / 360 U/min  ->  20 992           ->  Paar  41 984
     *      500 kbps / 300 U/min  ->  25 088           ->  Paar  50 176
     *     1000 kbps / 300 U/min  ->  50 176           ->  Paar 100 352  (ED)
     *
     * Die Absage ist damit HEUTE UNERREICHBAR — der Wandler setzt nie mehr
     * als 500 kbps. Sie steht hier als die Zusage, die einen ED-Zweig
     * verhindert, solange der Behaelter HFE v1 ist: `(uint16_t)100352` waere
     * 34 816, eine Spurtabelle, die auf ein Drittel der Daten zeigt — also
     * ein ZWEITER stiller Verlust ueber dem ersten. Genau dieser Zweig war
     * der erste Entwurf von MF-1170, und die Messung hat ihn verworfen.
     *
     * Die Grenze liegt bei 32 767 Byte je Seite, also etwa 655 kbps bei
     * 300 U/min. Der Weg zu 2,88 M ist HFE v3, nicht ein Zweig hier.
     * `tests/test_hfe_kappt_nicht_still.c` haelt beide Haelften fest — dass
     * die vier heutigen Faelle passen UND dass ED nicht passt. */
    if ((long)track_len_aligned * 2 > 0xFFFF) {
        result->error = UFT_ERR_INVALID_FORMAT;
        uftc_add_warning(result,
                 "sectors->HFE: a track pair of %ld bytes (%d per side at "
                 "%u kbps / %u rpm) does not fit the 16-bit length field of "
                 "the HFE v1 track table (max 65535). Refused rather than "
                 "truncated (MF-1170).",
                 (long)track_len_aligned * 2, track_len_aligned,
                 (unsigned)bitrate, rpm);
        return UFT_ERR_INVALID_FORMAT;
    }

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
    /* MF-1166: dieselbe Zahl, die auch die Spurlaenge bestimmt hat. Vorher
     * stand hier das Literal 300 — der Kopf behauptete es fuer JEDES
     * Medium, auch fuer die 1,2-MB-Diskette mit 360 U/min. */
    hdr->drive_rpm = (uint16_t)rpm;
    hdr->uft_floppy_interface = (uint8_t)iface;
    hdr->track_list_offset = 1; /* Block 1 */

    /* Write track lookup table */
    hfe_track_entry_t* lut = (hfe_track_entry_t*)(hfe_data + 512);
    for (int cyl = 0; cyl < cylinders; cyl++) {
        lut[cyl].offset = (uint16_t)(data_start_block + cyl * blocks_per_track);
        /* MF-533: die LUT-Laenge ist die GESAMTLAENGE beider Seiten
         * (MF-526). `track_len_aligned` ist die je Seite — dieselbe
         * Datei reserviert damit `blocks_per_track =
         * (track_len_aligned * 2 + 511) / 512` und interleavt mit der
         * Seitenlaenge. Hier stand die halbe Zahl; ein Leser, der sie
         * als Gesamtlaenge nimmt, sieht die Haelfte.
         *
         * Gemessen am Rundlauf G64 -> HFE -> G64: 278234 Byte gingen
         * hinein, 139634 kamen heraus — genau die Haelfte
         * (tests/test_convert_roundtrip_measured.c). Dieselbe
         * Korrektur wie MF-528, nur an den drei uebrigen Schreibern. */
        lut[cyl].length = (uint16_t)(track_len_aligned * 2);
    }

    uftc_report_progress(opts, 40, "Encoding MFM tracks");

    /* MF-539: kodiert wird mit `uft_mfm_encode_track()`, nicht mehr von Hand.
     *
     * Hier stand eine zweite, handgeschriebene Fassung des IBM-System-34-
     * Aufbaus. Sie hatte drei voneinander unabhaengige Fehler, von denen
     * jeder einzelne die Ausgabe unlesbar macht:
     *
     *   1. KEIN KODIERSCHRITT. `0x4E`, `0x00`, `0xC2`, `0xA1`, `0xFE`,
     *      `0xFB` und die Nutzdaten gingen als rohe Bytes in den Puffer.
     *      Eine HFE-Spur enthaelt aber den MFM-ZELLENSTROM: acht Datenbits
     *      werden zu sechzehn Zellen. Deshalb steht in einer echten
     *      Aufnahme ueberall 0x55 (= kodiertes 0x00) und nirgends ein
     *      Nullbyte — MFM kann nie mehr als drei Nullbits am Stueck
     *      erzeugen. Unsere Ausgabe hatte 5969 Nullbytes je Spur.
     *
     *   2. CRC NIE BERECHNET. Beide CRC-Felder jedes Sektors blieben
     *      `0x00 0x00` ("CRC placeholder"). Selbst bei richtiger Kodierung
     *      haette jeder Leser auf JEDEM Sektor einen CRC-Fehler gemeldet.
     *
     *   3. KEINE BIT-SPIEGELUNG. HFE speichert LSB-first. Alle sechs
     *      anderen HFE-Schreiber dieses Baums rufen `hfe_reverse_bits()`
     *      (uft_format_convert_bitstream.c:145/458/586,
     *      uft_format_convert_flux.c:1563/1860/2269) — dieser eine nicht.
     *
     * Trotzdem liefen `sectors_converted++` und `tracks_converted++`
     * bedingungslos durch, und `success = true` folgte allein daraus, dass
     * sich die Datei schreiben liess.
     *
     * Der richtige Encoder lag die ganze Zeit im Baum und wurde von
     * niemandem gerufen. Er ist seit MF-539 belegt — nicht durch Lesen,
     * sondern durch Rueckwandlung mit dem vorhandenen Dekoder
     * (tests/test_mfm_encoder_decodes_back.c):
     *
     *      kodiert: 32768 Byte Zellenstrom
     *      Sync 0x4489: 108 (erwartet 108 = 6 je Sektor x 18)
     *      Nullbytes: 0 von 32768
     *      dekodiert: 18 Sektoren, alle mit gueltiger ID- und Daten-CRC
     *      und byteweise gleichem Inhalt
     *
     * `uft_mfm_encode_track()` fuellt bis zur uebergebenen Kapazitaet mit
     * Gap auf, gibt also genau die Spurlaenge zurueck; 0 bedeutet, dass die
     * Sektoren nicht hineinpassen. Der Rueckgabewert wird geprueft. */
    {
        const size_t track_cap = (size_t)track_len_aligned;
        uint8_t *head_buf[2];
        head_buf[0] = malloc(track_cap);
        head_buf[1] = malloc(track_cap);
        uft_sector_t *secs = calloc((size_t)sectors, sizeof(uft_sector_t));
        if (!head_buf[0] || !head_buf[1] || !secs) {
            free(head_buf[0]); free(head_buf[1]); free(secs);
            free(hfe_data);
            result->error = UFT_ERR_MEMORY;
            return UFT_ERR_MEMORY;
        }

        uft_mfm_encode_params_t enc_params = UFT_MFM_PARAMS_DEFAULT_DD;
        if (bitrate >= 400) {
            uft_mfm_encode_params_t hd = UFT_MFM_PARAMS_DEFAULT_HD;
            enc_params = hd;
        }

        /* Ein Ersatzsektor fuer Quelldaten, die die Datei nicht mehr
         * hergibt. 0xE5 ist das Formatier-Fuellbyte der IBM-Welt — es
         * bedeutet "nie beschrieben" und ist von echten Nullen
         * unterscheidbar. Die Warnung oben nennt die Groessendifferenz. */
        uint8_t *pad = malloc((size_t)sector_size);
        if (!pad) {
            free(head_buf[0]); free(head_buf[1]); free(secs);
            free(hfe_data);
            result->error = UFT_ERR_MEMORY;
            return UFT_ERR_MEMORY;
        }
        memset(pad, 0xE5, (size_t)sector_size);

        for (int cyl = 0; cyl < cylinders; cyl++) {
            if (uftc_is_cancelled(opts)) break;

            int heads_done = 0;
            for (int hd = 0; hd < heads; hd++) {
                for (int sec = 0; sec < sectors; sec++) {
                    size_t src_offset = ((size_t)cyl * heads * sectors +
                                         (size_t)hd * sectors + sec)
                                        * (size_t)sector_size;
                    memset(&secs[sec], 0, sizeof(secs[sec]));
                    secs[sec].id.cylinder  = (uint8_t)cyl;
                    secs[sec].id.head      = (uint8_t)hd;
                    secs[sec].id.sector    = (uint8_t)(sec + 1);
                    secs[sec].id.size_code = 2;   /* 2 = 512 Byte */
                    secs[sec].data_len     = (size_t)sector_size;
                    secs[sec].data_size    = (uint16_t)sector_size;
                    secs[sec].data = (src_offset + (size_t)sector_size <= src_size)
                                     ? (uint8_t *)(src_data + src_offset)
                                     : pad;
                }

                size_t written;
                if (src_format == UFT_FORMAT_ADF) {
                    /* MF-1081: AmigaDOS statt IBM System 34.
                     *
                     * Die Spurnummer ist `cyl * 2 + head` — so steht
                     * sie im Info-Long, und so liest der Dekoder sie
                     * zurueck. Der Rest der Umdrehung wird mit **0xAA**
                     * gefuellt: das sind die MFM-Zellen fuer 0x00, also
                     * der Zwischenraum einer echten Amiga-Spur. (Die
                     * IBM-Seite fuellt mit 0x55; das ist ihr Gap-Byte,
                     * nicht unseres.) */
                    const size_t brauche =
                        (size_t)sectors * UFT_AMIGA_SECTOR_MFM_BYTES;
                    memset(head_buf[hd], 0xAA, track_cap);
                    written = (brauche <= track_cap)
                        ? uft_amiga_mfm_encode_track(
                              src_data + ((size_t)cyl * heads + hd)
                                         * sectors * sector_size,
                              (unsigned)sectors,
                              (unsigned)(cyl * 2 + hd), NULL,
                              head_buf[hd], track_cap)
                        : 0u;
                    if (written) written = track_cap;   /* Rest ist Gap */
                } else {
                    written = uft_mfm_encode_track(secs, (size_t)sectors,
                                                   (uint8_t)cyl, (uint8_t)hd,
                                                   &enc_params,
                                                   head_buf[hd], track_cap);
                }
                if (written == 0) {
                    /* Passt nicht in eine Umdrehung. Nicht als Erfolg
                     * zaehlen und keine halbe Spur ablegen — die Seite
                     * bleibt Gap, und die Spur gilt als gescheitert. */
                    memset(head_buf[hd], 0x55, track_cap);
                    continue;
                }

                /* HFE speichert LSB-first. */
                hfe_reverse_bits(head_buf[hd], (uint32_t)track_cap);

                result->sectors_converted += sectors;
                heads_done++;
            }

            uint8_t *track_dest = hfe_data + (size_t)lut[cyl].offset * 512;
            hfe_interleave_track(head_buf[0], head_buf[1],
                                  (uint16_t)track_len_aligned, track_dest);

            if (heads_done == heads) result->tracks_converted++;
            else                     result->tracks_failed++;

            uftc_report_progress(opts, 40 + (cyl * 50 / cylinders),
                                 "Encoding MFM tracks");
        }

        free(pad);
        free(secs);
        free(head_buf[0]);
        free(head_buf[1]);
    }

    /* MF-539: eine Datei, in der keine einzige Spur steht, wird nicht
     * geschrieben. Vorher folgte `success = true` allein daraus, dass sich
     * die Datei anlegen liess — dieselbe Bauart, die in MF-538 eine ADF aus
     * lauter Nullen als Erfolg gemeldet hat. */
    if (result->tracks_converted == 0) {
        free(hfe_data);
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result,
                 "no track could be encoded (%d of %d failed); no file was "
                 "written rather than leaving an empty HFE behind (MF-539)",
                 result->tracks_failed, cylinders);
        return UFT_ERR_FORMAT;
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

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
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"
#include "uft/flux/uft_flux_decoder.h"
#include "uft/flux/uft_decode_timeline.h"

extern const uft_format_plugin_t uft_format_plugin_hfe;
extern const uft_format_plugin_t uft_format_plugin_scp;
#include "uft/flux/uft_mfm_sector_parser.h"
#include "uft/flux/uft_scp_parser.h"
#include "uft/recovery/uft_multiread_pipeline.h"
#include "uft/detect/uft_mfm_detect_bridge.h"

// ============================================================================
// Flux/Bitstream -> Sector Conversions (Decode Pipeline)
// ============================================================================

/**
 * Richtet einen PLL-Bitstrom an der CBM-Sync-Marke aus (MF-565).
 *
 * ── Warum das noetig ist ────────────────────────────────────────────────
 *
 * Der geprufte GCR-Dekoder (@ref uft_cbm_gcr_track_to_sectors, seit
 * MF-536 gegen VICE gemessen) sucht 0xFF-BYTES. Aus einem G64 stimmt die
 * Byte-Grenze von selbst; aus Fluss nicht — der PLL faengt an, wo die
 * Aufnahme anfing, und das ist eine beliebige Bitphase. Ohne Ausrichtung
 * sieht der Dekoder nie ein 0xFF, obwohl der Strom voll davon ist.
 *
 * ── Wie ausgerichtet wird ───────────────────────────────────────────────
 *
 * Die CBM-Sync-Marke ist ein Lauf von Einsen (Standard: 5 Byte = 40).
 * Gesucht wird das ENDE des ersten Laufs von mindestens
 * @ref UFT_CBM_SYNC_MIN_ONES Einsen; dahinter faengt der Kopf an, und
 * genau dort wird die Byte-Grenze hingelegt. Damit endet der Lauf auf
 * einer Byte-Grenze und seine letzten acht Bits stehen als 0xFF da —
 * das, wonach der Dekoder sucht.
 *
 * @param bits      Bitstrom, MSB zuerst je Byte (so schreibt der PLL)
 * @param bit_count Anzahl gueltiger Bits
 * @param out       Zielpuffer fuer die ausgerichteten Bytes
 * @param out_cap   dessen Groesse
 * @return Anzahl geschriebener Bytes; 0 wenn keine Sync-Marke im Strom
 *         liegt. **Null heisst „keine gefunden", nicht „leer"** — der
 *         Aufrufer darf das nicht als leere Spur melden.
 */
#define UFT_CBM_SYNC_MIN_ONES  10

static size_t uftc_cbm_align_bits(const uint8_t *bits, size_t bit_count,
                                  uint8_t *out, size_t out_cap)
{
    if (!bits || !out || bit_count < UFT_CBM_SYNC_MIN_ONES) return 0;

    size_t run = 0, sync_end = SIZE_MAX;
    for (size_t b = 0; b < bit_count; b++) {
        int v = (bits[b / 8] >> (7 - (b % 8))) & 1;
        if (v) {
            run++;
        } else {
            if (run >= UFT_CBM_SYNC_MIN_ONES) { sync_end = b; break; }
            run = 0;
        }
    }
    /* Ein Lauf, der bis ans Ende reicht, ist auch einer. */
    if (sync_end == SIZE_MAX && run >= UFT_CBM_SYNC_MIN_ONES)
        sync_end = bit_count;
    if (sync_end == SIZE_MAX) return 0;

    /* Die Byte-Grenze liegt auf dem ersten Bit NACH dem Lauf. Davor
     * bleiben so viele Bits, wie in ganze Bytes passen — die tragen die
     * 0xFF-Marke. */
    size_t phase = sync_end % 8;
    size_t n = 0;
    for (size_t b = phase; b + 8 <= bit_count && n < out_cap; b += 8) {
        uint8_t byte = 0;
        for (int k = 0; k < 8; k++)
            byte = (uint8_t)((byte << 1) |
                             ((bits[(b + k) / 8] >> (7 - ((b + k) % 8))) & 1));
        out[n++] = byte;
    }
    return n;
}

/* Eine Sektorposition, gesehen ueber alle Umdrehungen einer Spur (MF-566).
 *
 * Spiegelbild von @ref uftc_adf_votes_t weiter unten — dieselbe Aufgabe auf
 * der Commodore-Seite. 21 x 5 x 256 Byte sind 26,9 kB je Spur, einmal auf
 * dem Stapel und pro Spur genullt. */
typedef struct {
    uint8_t data[UFT_SCP_MAX_REVOLUTIONS][256];
    bool    crc_ok[UFT_SCP_MAX_REVOLUTIONS];
    int     count;
} uftc_d64_votes_t;

/**
 * Ein Sektor aus allen Umdrehungen, die ihn gelesen haben (MF-566).
 *
 * Genommen wird der Abstimmer aus `uft_multiread_pipeline.c` — derselbe,
 * den `uftc_adf_place_voted()` seit MF-473 fuer die Amiga-Seite fahrt. Er
 * stimmt BYTEWEISE ab, wiegt eine Lesung mit heilem Pruefbyte 100 und eine
 * mit falschem 50, und erkennt schwache Bits.
 *
 * @param out      256 Byte Ziel
 * @param cls_out  Einordnung der Abstimmung; darf NULL sein
 * @return true, wenn etwas Verwertbares herauskam
 */
static bool uftc_d64_place_voted(const uftc_d64_votes_t* v, uint8_t* out,
                                 multiread_class_t* cls_out)
{
    if (!v || v->count <= 0) return false;

    multiread_config_t cfg = multiread_config_default();
    /* Wie viele Umdrehungen es gibt, bestimmt die Datei, nicht eine
     * Wiederhol-Strategie: min_passes = 1. Eine einzige Umdrehung heisst
     * „keine Abstimmung moeglich", nicht „Fehler". */
    cfg.min_passes = 1;
    cfg.detect_weak_bits = true;

    multiread_ctx_t* mr = multiread_create(&cfg);
    if (!mr) return false;

    for (int r = 0; r < v->count; r++)
        multiread_add_pass(mr, v->data[r], 256,
                           v->crc_ok[r] ? 100 : 50, v->crc_ok[r]);

    multiread_sector_t res;
    memset(&res, 0, sizeof(res));
    bool placed = false;

    if (multiread_execute(mr, out, 256, &res) == MULTIREAD_OK) {
        if (cls_out) *cls_out = res.class_;
        placed = res.recovered;
    }

    free(res.weak_mask);      /* execute() belegt sie, der Aufrufer gibt frei */
    multiread_destroy(mr);
    return placed;
}

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
        uftc_add_warning(result,
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

    /* ── `decode_retries` bekommt seine dokumentierte Bedeutung (MF-565) ──
     *
     * Das Feld ist als „Sector extraction (Flux -> Sector)" dokumentiert und
     * steht per Vorgabe auf 5. Gemessen vor dieser Aenderung: es wurde genau
     * hier in eine lokale Variable gelesen und **nie benutzt** — ein
     * Vorkommen im ganzen Baum, ein Knopf ohne Draht.
     *
     * Jetzt begrenzt es, wie viele Umdrehungen versucht werden. */
    int retries = (opts && opts->decode_retries > 0) ? opts->decode_retries : 5;

    /* Nebenbild fuer die Einzelauswertung je Umdrehung (MF-566). Einmal
     * angelegt und wiederverwendet — 35 Spuren mal bis zu 5 Umdrehungen
     * waeren sonst 175 Bilder zu je 175 kB. */
    d64_image_t* scratch = d64_create(35);
    if (!scratch) {
        d64_free(d64);
        uft_scp_free(&scp);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    /* Process each track: flux -> PLL -> GCR -> sectors */
    for (int track = 1; track <= 35; track++) {
        if (uftc_is_cancelled(opts)) break;

        int scp_track_idx = (track - 1) * 2; /* Side 0 only for C64 */
        int sectors_per_trk = uft_c64_sectors_per_track(track);
        int bitrate = uft_c64_track_bitrate(track);

        /* ── Alle Umdrehungen, nicht die mit dem meisten Fluss ────────────
         *
         * Vorher suchte diese Schleife `max_flux` und dekodierte NUR die.
         * Zwei Fehler in einem: die uebrigen Umdrehungen lagen ungenutzt in
         * der Datei, und das Kriterium war verkehrt herum — eine Umdrehung
         * mit ZUSAETZLICHEN Uebergaengen (Rauschen, schwaches Bit,
         * Schreibnaht) hat MEHR Fluss als eine saubere. `max_flux` waehlte
         * also unter Umstaenden gerade die schlechteste aus.
         *
         * Der Amiga-Zwilling in dieser Datei
         * (`uftc_convert_scp_to_adf_via_plugin`) laeuft seit MF-473 ueber
         * alle Umdrehungen. Derselbe Job, dieselbe Datei, zwei Fassungen. */
        /* ── Und: Uebereinstimmung schlaegt Pruefbyte (MF-566) ────────────
         *
         * Die CBM-Datenpruefsumme ist EIN XOR-Byte. Ein zerstoerter Sektor
         * besteht sie mit Wahrscheinlichkeit 1/256 — auf einer Diskette mit
         * 683 Sektoren sind das 2,7 zu erwartende Zufallstreffer, und
         * gemessen wurden 3 (tests/test_convert_scp_d64_multirev.c).
         *
         * Wer „Pruefbyte stimmt" als „geprueft" nimmt, laesst so einen
         * Treffer stehen und die saubere Lesung der naechsten Umdrehung
         * nicht mehr durch. Gemessen: 683 gemeldet, 680 byteweise richtig.
         *
         * Liegen mehrere Umdrehungen vor, gibt es eine bessere Auskunft:
         * Lesungen, die denselben Sektor BYTEWEISE GLEICH liefern. Das ist
         * ein 2048-Bit-Beleg statt eines 8-Bit-Belegs.
         *
         * Abgestimmt wird mit DEMSELBEN Abstimmer, den der Amiga-Pfad in
         * dieser Datei seit MF-473 benutzt (`multiread_*`) — nicht mit
         * einem zweiten. Ein zweiter Abstimmer waere genau der
         * Geschwister-Fall, gegen den diese ganze Reihe laeuft. Er wiegt
         * eine gepruefte Lesung 100, eine ungepruefte 50, und erkennt
         * nebenbei schwache Bits. */
        uftc_d64_votes_t votes[21];
        memset(votes, 0, sizeof(votes));

        int revs = (int)scp.header.revolutions;
        if (revs > retries) revs = retries;
        if (revs < 1) revs = 1;
        if (revs > UFT_SCP_MAX_REVOLUTIONS) revs = UFT_SCP_MAX_REVOLUTIONS;

        double deltas[131072];
        for (int rev = 0; rev < revs; rev++) {
            int flux_count = uft_scp_get_track_flux(&scp, scp_track_idx, rev,
                                                    deltas, 131072);
            if (flux_count <= 0) continue;

            /* Initialize PLL for C64 GCR encoding */
            uft_pll_t pll;
            uft_pll_init(&pll, (double)bitrate, UFT_ENCODING_GCR);

            size_t max_bits = (size_t)flux_count * 4;
            size_t bit_buf_size = (max_bits + 7) / 8;
            uint8_t* bitstream = calloc(1, bit_buf_size);
            if (!bitstream) break;

            /* Run PLL to extract bitstream */
            size_t bit_pos = 0;
            for (int f = 0; f < flux_count && bit_pos < max_bits - 16; f++) {
                double delta_sec = deltas[f] * 1e-9;  /* MF-418: uft_scp_get_track_flux()
                                                 * yields NANOSECONDS, with the
                                                 * SCP resolution multiplier already
                                                 * applied by the shipped parser. The
                                                 * previous 25e-9 assumed raw ticks and
                                                 * was off by 25*(1+resolution). Never
                                                 * observed because the accessor was a
                                                 * stub returning -1. */
                uft_pll_process_flux_mfm(&pll, delta_sec, bitstream, &bit_pos);
            }

            /* ── Ausrichten, dann der GEPRUEFTE Dekoder ───────────────────
             *
             * Hier stand bis MF-565 `uft_c64_parser_add_bit()` — ein Stub,
             * der Bits in ein Schieberegister schob und laut eigenem
             * Kommentar „Full sync-pattern detection + sector extraction
             * deferred" war. Er konnte keinen Sektor finden, und er fand
             * keinen: 0 von 683 auf einer fehlerfreien Aufnahme.
             *
             * Genommen wird jetzt derselbe Dekoder wie im G64-Pfad, der
             * seit MF-536 gegen VICE gemessen ist. Er will Bytes, also
             * wird der Bitstrom vorher an der Sync-Marke ausgerichtet. */
            size_t gcr_cap = bit_pos / 8 + 1;
            uint8_t* gcr = malloc(gcr_cap);
            if (gcr) {
                size_t gcr_len = uftc_cbm_align_bits(bitstream, bit_pos,
                                                     gcr, gcr_cap);
                if (gcr_len > 0) {
                    /* Diese Umdrehung wird fuer sich dekodiert — in ein
                     * Nebenbild, mit frischer Karte. Nur so hat sie eine
                     * eigene Stimme; schriebe sie direkt ins Ergebnis,
                     * gaebe es nichts zu vergleichen. */
                    uint8_t rev_state[21] = {0};
                    uft_cbm_gcr_track_to_sectors(scratch, track, gcr,
                                                 gcr_len, NULL, rev_state);

                    for (int s = 0; s < sectors_per_trk; s++) {
                        if (rev_state[s] == 0) continue;
                        uftc_d64_votes_t* v = &votes[s];
                        if (v->count >= UFT_SCP_MAX_REVOLUTIONS) continue;
                        if (d64_get_sector(scratch, track, s,
                                           v->data[v->count], NULL) != 0)
                            continue;
                        v->crc_ok[v->count] = (rev_state[s] == 2);
                        v->count++;
                    }
                }
                free(gcr);
            }
            free(bitstream);
        }

        /* ── Das Urteil je Sektor ──────────────────────────────────────────
         *
         * Der Abstimmer sagt, WAS herauskam, und wie er dazu kam. Nur
         * `MULTIREAD_STABLE_GOOD` heisst „geprueft": ueber alle vorliegenden
         * Umdrehungen dasselbe, mit heilem Pruefbyte. Alles andere wird
         * BEHALTEN und als fehlerhaft markiert — verworfen wird nichts. */
        for (int s = 0; s < sectors_per_trk; s++) {
            uint8_t voted[256];
            multiread_class_t cls = MULTIREAD_CLASS_UNKNOWN;

            if (votes[s].count > 0 &&
                uftc_d64_place_voted(&votes[s], voted, &cls)) {

                if (cls == MULTIREAD_CLASS_STABLE_GOOD) {
                    d64_set_sector(d64, track, s, voted, D64_ERR_OK);
                    result->sectors_converted++;
                } else {
                    /* Geborgen, aber nicht bestaetigt: entweder stimmte
                     * das Pruefbyte nicht, oder zwei Umdrehungen lesen
                     * verschiedene Daten. Welches von beidem — instabiler
                     * Sektor, schwache Bits, oder ein 1/256-Zufallstreffer
                     * des Pruefbytes — sagt @ref multiread_class_t.
                     *
                     * Die Daten BLEIBEN stehen; sie mit Fuellbyte zu
                     * ueberschreiben waere „Kein Bit verloren" verletzt.
                     * `D64_ERR_CHECKSUM` ist die naechstliegende Marke, die
                     * das Format hergibt — sie untertreibt das Vertrauen,
                     * sie uebertreibt es nie (MF-566). */
                    d64_set_sector(d64, track, s, voted, D64_ERR_CHECKSUM);
                    result->sectors_failed++;
                    if (result->warning_count < 8) {
                        uftc_add_warning(result,
                                 "Track %d sector %d: %d Lesung(en), nicht "
                                 "bestaetigt (%s) — Daten behalten, als "
                                 "fehlerhaft markiert",
                                 track, s, votes[s].count,
                                 multiread_class_name(cls));
                    }
                }
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
                    uftc_add_warning(result,
                             "Track %d sector %d: GCR decode failed, "
                             "filled with 0x%02X",
                             track, s, UFT_FORENSIC_FILL_BYTE);
                }
            }
        }

        result->tracks_converted++;

        uftc_report_progress(opts, 10 + (track * 80 / 35), "Decoding tracks");
    }

    d64_free(scratch);

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
/* SCP -> ADF through uft_format_plugin_scp and the AmigaDOS decoder (MF-438).
 *
 * The IBM path below runs uft_mfm_decode_track(), its own comment calling it
 * "the canonical MFM IDAM/DAM parser". AmigaDOS has no IDAM and no DAM, so on
 * an Amiga disk it returns nothing and the calloc'd output stays zero — the
 * same defect MF-437 fixed one layer up, in HFE->ADF.
 *
 * Reading through the plugin also avoids a sixth private SCP reader: the
 * plugin parses the header, selects a revolution and converts ticks to
 * nanoseconds. */
enum { ADF_CYLS = 80, ADF_HEADS = 2, ADF_SPT = 11, ADF_SECSZ = 512 };

/* Eine Sektorposition, gesehen ueber alle Umdrehungen einer Spur (MF-473).
 *
 * Die Inhalte werden KOPIERT, nicht referenziert: flux_decoded_track_free()
 * gibt die Sektorpuffer einer Umdrehung frei, bevor die naechste dekodiert
 * wird. 11 x 5 x 512 Byte sind 28 KB pro Spur — einmal belegt, pro Spur
 * genullt. */
typedef struct {
    uint8_t data[UFT_SCP_MAX_REVOLUTIONS][ADF_SECSZ];
    bool    crc_ok[UFT_SCP_MAX_REVOLUTIONS];
    int     count;
} uftc_adf_votes_t;

/* Die Sektoren EINER dekodierten Umdrehung in die Stimmentabelle einsortieren.
 * Eine Umdrehung kann dieselbe Sektornummer zweimal enthalten (der Flux laeuft
 * ueber den Index hinaus); die Obergrenze faengt das ab. */
static void uftc_adf_collect(const flux_decoded_track_t* dt,
                             uftc_adf_votes_t* votes)
{
    for (size_t s = 0; s < dt->sector_count; s++) {
        const flux_decoded_sector_t* sec = &dt->sectors[s];
        if (!sec->data || sec->data_size != ADF_SECSZ) continue;
        if (sec->sector >= ADF_SPT) continue;
        uftc_adf_votes_t* v = &votes[sec->sector];
        if (v->count >= UFT_SCP_MAX_REVOLUTIONS) continue;
        memcpy(v->data[v->count], sec->data, ADF_SECSZ);
        v->crc_ok[v->count] = sec->data_crc_ok;
        v->count++;
    }
}

/* Ueber die Lesungen EINER Sektorposition abstimmen und das Ergebnis ablegen
 * (MF-473).
 *
 * Warum ueberhaupt abgestimmt wird: eine SCP-Aufnahme enthaelt bis zu fuenf
 * Umdrehungen derselben Spur, und bis hierher benutzte der Wandler die erste
 * und warf die uebrigen weg — das SCP-Plugin sagt das in seinem eigenen
 * Kommentar ("Fuer bessere Ergebnisse koennte man alle Revolutions
 * kombinieren"). Damit war jede weitere Aufnahme derselben Umdrehung
 * gemessene, bezahlte, gespeicherte Information ohne Wirkung.
 *
 * Geschrieben wird weiterhin nur, was `recovered` ist — eine Lesung mit
 * falscher Pruefsumme ueberschreibt nie gute Daten. Neu ist die Aussage
 * darueber, WARUM ein Sektor nicht gutging: @ref multiread_class_t
 * unterscheidet den stabil falschen (Kopierschutz, erneutes Lesen aendert
 * nichts) vom schwankenden (weak). Bisher zaehlte der Pfad nur
 * `sectors_failed` hoch und sagte nichts.
 *
 * @return true wenn der Sektor in @p output geschrieben wurde. */
static bool uftc_adf_place_voted(const uftc_adf_votes_t* v,
                                 uint8_t* output, size_t off,
                                 multiread_class_t* cls_out)
{
    multiread_config_t cfg = multiread_config_default();
    /* Wie viele Umdrehungen es gibt, bestimmt die Datei, nicht eine
     * Wiederhol-Strategie: min_passes = 1. Eine einzige Umdrehung heisst
     * "keine Abstimmung moeglich", nicht "Fehler" — und STABLE_GOOD bzw.
     * STABLE_BAD_CRC sind auch ueber eine Lesung eine gueltige Aussage. */
    cfg.min_passes = 1;
    cfg.detect_weak_bits = true;

    multiread_ctx_t* mr = multiread_create(&cfg);
    if (!mr) return false;

    for (int r = 0; r < v->count; r++) {
        /* Dieselbe Qualitaets-Konvention wie im Pipeline-Inneren: eine
         * gepruefte Lesung wiegt 100, eine ungepruefte 50. */
        multiread_add_pass(mr, v->data[r], ADF_SECSZ,
                           v->crc_ok[r] ? 100 : 50, v->crc_ok[r]);
    }

    multiread_sector_t res;
    memset(&res, 0, sizeof(res));
    uint8_t voted[ADF_SECSZ];
    bool placed = false;

    if (multiread_execute(mr, voted, ADF_SECSZ, &res) == MULTIREAD_OK) {
        if (cls_out) *cls_out = res.class_;
        if (res.recovered) {
            memcpy(output + off, voted, ADF_SECSZ);
            placed = true;
        }
    }

    free(res.weak_mask);      /* execute() belegt sie, der Aufrufer gibt frei */
    multiread_destroy(mr);
    return placed;
}

/* Das eigene Ergebnis nachsehen (MF-489).
 *
 * Zweimal hat dieser Baum ein Abbild aus lauter Nullen als erfolgreiche
 * Wandlung gemeldet: MF-437 (HFE->ADF) und MF-438 (SCP->ADF), beide Male
 * weil ein IBM-Parser auf einer AmigaDOS-Spur nichts fand und der Zaehler
 * trotzdem hochlief. Beide Male fiel es erst auf, als jemand die Datei
 * aufmachte.
 *
 * Ein Abbild, das kein erkennbares Dateisystem traegt, ist kein Beweis fuer
 * einen Fehler — eine leere oder fremdformatierte Diskette gibt es. Aber es
 * ist der Punkt, an dem der Wandler den Mund aufmachen muss, statt
 * "erfolgreich" zu melden und zu schweigen.
 *
 * Der Erkenner dafuer liegt seit jeher im Baum (src/detect/mfm/, 3609
 * Zeilen, 43 exportierte Symbole) — und hatte **keinen einzigen Aufrufer**.
 * Neunter Eintrag derselben Liste in dieser Woche.
 *
 * Mehrere Kandidaten werden ALLE gemeldet, nicht nur der beste: eine
 * Diskette kann zwei Dateisysteme tragen (Amiga + Atari auf derselben
 * Scheibe ist ein bekannter Fall), und nur den Sieger zu nennen hiesse,
 * das zweite stillschweigend zu unterschlagen. */
/* ── Wo auf der Umdrehung der Schaden sitzt (MF-501) ─────────────────────
 *
 * Der Decoder weiss seit jeher, WO ein Sektor lag (`id_position`), und
 * seit MF-496 auch, mit welcher Zellendauer er gelesen wurde. Zusammen mit
 * der gemessenen Umdrehungsdauer wird daraus eine Winkellage — die
 * Datenquelle, die der Polarkarte (Mammut 3.2) fehlte.
 *
 * Berichtet werden ROHE Zahlen: acht gleich grosse Sektoren der Umdrehung
 * und wieviel Schaden in jedem liegt. Keine Deutung („der Schaden haeuft
 * sich bei 3 Uhr") — die waere eine Behauptung ueber die Ursache, und die
 * ist aus einer Wandlung nicht ableitbar.
 */
#define UFTC_ANGLE_BINS 8

typedef struct {
    /** Anteil beschaedigter Bits je Achtel der Umdrehung, aufsummiert. */
    double bins[UFTC_ANGLE_BINS];
    /** Spuren, fuer die eine Winkellage bestimmt werden konnte. */
    int    tracks_with_angle;
    /** Spuren mit ueberhaupt beschaedigten Abschnitten. */
    int    tracks_damaged;
    /** Groesste Winkel-Fehlerschranke ueber alle Spuren, in Grad.
     *  Eine Verteilung ohne diese Zahl behauptet eine Genauigkeit,
     *  die niemand gemessen hat (MF-502). */
    double worst_angle_err;
    /** Spuren, deren Schaden zu ungenau lag, um ihn einzuordnen. */
    int    tracks_angle_too_coarse;
    double sum_decoded;
    double sum_damaged;
    double sum_untouched;
} uftc_angle_stats_t;

/**
 * Eine dekodierte Spur in die Winkelverteilung eintragen.
 *
 * @param revolution_ns  gemessene Umdrehungsdauer; 0 heisst „nicht
 *                       gemessen", und dann wird KEINE Winkellage
 *                       eingetragen statt einer geschaetzten.
 */
static void uftc_angle_note(uftc_angle_stats_t *st,
                            const flux_decoded_track_t *dt,
                            double revolution_ns)
{
    if (!st || !dt || dt->track_length_bits == 0) return;

    uft_decode_timeline_t tl;
    if (!uft_timeline_build(dt, (size_t)dt->track_length_bits,
                            dt->used_cell_ns, revolution_ns, &tl))
        return;

    st->sum_decoded   += uft_timeline_fraction(&tl, UFT_SLICE_DECODED);
    st->sum_damaged   += uft_timeline_fraction(&tl, UFT_SLICE_DAMAGED);
    st->sum_untouched += uft_timeline_fraction(&tl, UFT_SLICE_UNTOUCHED);

    /* Wie genau ist der Winkel auf DIESER Spur? Die Karte teilt die
     * Umdrehung in Achtel zu 45 Grad; ist die Schranke groesser als ein
     * halbes Achtel, landet die Marke womoeglich im falschen Fach, und
     * eine Verteilung aus falschen Faechern ist schlechter als keine. */
    const double err_deg = uft_timeline_angle_error(&tl);
    const bool angle_usable = (err_deg >= 0.0) && (err_deg <= 22.5);
    if (err_deg > st->worst_angle_err) st->worst_angle_err = err_deg;

    bool any_damage = false, any_angle = false;
    for (size_t i = 0; i < tl.count; i++) {
        if (tl.slices[i].status != UFT_SLICE_DAMAGED) continue;
        any_damage = true;

        if (!angle_usable) continue;
        double a = uft_timeline_angle(&tl, i);
        if (a < 0.0) continue;              /* keine Zeitbasis, kein Winkel */
        any_angle = true;

        int bin = (int)(a * UFTC_ANGLE_BINS);
        if (bin < 0) bin = 0;
        if (bin >= UFTC_ANGLE_BINS) bin = UFTC_ANGLE_BINS - 1;
        st->bins[bin] += (double)(tl.slices[i].end_bit
                                  - tl.slices[i].first_bit)
                         / (double)tl.bit_count;
    }
    if (any_damage) st->tracks_damaged++;
    if (any_angle)  st->tracks_with_angle++;
    if (any_damage && !angle_usable) st->tracks_angle_too_coarse++;

    uft_timeline_free(&tl);
}

static void uftc_angle_report(const uftc_angle_stats_t *st, int tracks,
                              uft_convert_result_t *result)
{
    if (!st || !result || tracks <= 0 || st->tracks_damaged == 0) return;

    if (st->tracks_with_angle == 0) {
        /* Schaden ja, Winkel nein. Zwei verschiedene Gruende, und sie
         * fuehren zu verschiedenen Abhilfen — deshalb werden sie
         * unterschieden statt zusammengefasst. */
        if (st->tracks_angle_too_coarse > 0)
            uftc_add_warning(result,
                "Beschaedigte Abschnitte auf %d Spuren; ihre Lage auf der "
                "Umdrehung wird nicht angegeben, weil die Spur ungleich "
                "schnell lief (Fehlerschranke bis %.0f Grad, ein Achtel "
                "misst 45)", st->tracks_damaged, st->worst_angle_err);
        else
            uftc_add_warning(result,
                "Beschaedigte Abschnitte auf %d Spuren; ihre Lage auf der "
                "Umdrehung ist unbekannt, weil keine Umdrehungsdauer "
                "gemessen wurde", st->tracks_damaged);
        return;
    }

    char bins[128];
    int n = 0;
    for (int i = 0; i < UFTC_ANGLE_BINS && n < (int)sizeof(bins) - 8; i++)
        n += snprintf(bins + n, sizeof(bins) - (size_t)n, "%s%.0f",
                      i ? " " : "", 1000.0 * st->bins[i] / (double)tracks);

    uftc_add_warning(result,
        "Schadenslage ueber die Umdrehung (8 Achtel, Promille der "
        "Spurlaenge): %s - beschaedigt auf %d von %d Spuren, Winkel auf "
        "+/-%.0f Grad genau",
        bins, st->tracks_damaged, tracks, st->worst_angle_err);
}

/* ── Was der Decoder ueber seine Zeitbasis gemeldet hat (MF-496) ─────────
 *
 * Der Decoder probiert seit MF-492/MF-495 mehrere Zeitbasen durch und
 * behaelt die beste. Bis hierher endete diese Messung im Decoder: sie
 * entschied ueber das Ergebnis und wurde verworfen. Der Mensch am
 * Feineinsteller (MF-480) sah davon nichts und musste raten, obwohl das
 * Werkzeug die Antwort schon gemessen hatte.
 *
 * Gesammelt wird ueber alle Spuren und EINMAL berichtet. Es gibt acht
 * Warnungsplaetze; eine Zeile je Spur waere nach der vierten Spur nur noch
 * eine Ueberlaufmarkierung.
 */
typedef struct {
    int    tracks;          /**< Spuren, die eine Zellendauer gemeldet haben */
    int    won_measured;    /**< davon: die Messung lieferte das Ergebnis */
    int    won_dewarped;    /**< davon: der entzerrte Strom lieferte es */
    int    warped;          /**< Spuren mit nennenswertem Gleichlauffehler */
    double worst_span;      /**< groesste gemessene Spanne */
    double ratio_sum;       /**< Summe gemessen/erste Wahl */
    int    ratio_n;
} uftc_timing_stats_t;

static void uftc_timing_note(uftc_timing_stats_t *st,
                             const flux_decoded_track_t *dt)
{
    if (!st || !dt) return;
    if (dt->initial_cell_ns <= 0.0) return;

    st->tracks++;
    if (dt->timing_source == FLUX_TIMING_MEASURED) st->won_measured++;
    if (dt->timing_source == FLUX_TIMING_DEWARPED) st->won_dewarped++;

    if (dt->measured_cell_ns > 0.0) {
        st->ratio_sum += dt->measured_cell_ns / dt->initial_cell_ns;
        st->ratio_n++;
    }
    /* 1,02 ist die Schwelle, ab der die Entzerrung ueberhaupt antritt
     * (uft_dewarp.c) — darunter ist die Spanne Messrauschen, kein Befund. */
    if (dt->warp_span >= 1.02) {
        st->warped++;
        if (dt->warp_span > st->worst_span) st->worst_span = dt->warp_span;
    }
}

static void uftc_timing_report(const uftc_timing_stats_t *st,
                               uft_convert_result_t *result)
{
    if (!st || !result || st->tracks == 0) return;

    /* Der Feineinsteller-Vorschlag. Gemeldet wird er nur, wenn die Messung
     * ueber die Spuren hinweg systematisch woanders liegt als die
     * Ableitung — ein einzelner Ausreisser ist keine Empfehlung. */
    if (st->ratio_n * 2 >= st->tracks && st->ratio_n > 0) {
        double pct = 100.0 * st->ratio_sum / (double)st->ratio_n;
        if (pct < 98.0 || pct > 102.0) {
            uftc_add_warning(result,
                "Die Sync-Marken messen eine Zellendauer von %.0f %% der "
                "abgeleiteten (%d Spuren gemittelt). Wer das bestaetigen "
                "will, setzt den Feineinsteller auf diesen Wert - der "
                "Decoder hat ihn bereits benutzt, wo er mehr Sektoren "
                "brachte (%d Spuren)",
                pct, st->ratio_n, st->won_measured);
        }
    }

    if (st->warped > 0) {
        uftc_add_warning(result,
            "Gleichlauffehler gemessen: bis %.0f %% Schwankung der "
            "Zellendauer innerhalb einer Spur, auf %d von %d Spuren. Die "
            "Entzerrung hat auf %d Spuren mehr Sektoren geliefert",
            100.0 * (st->worst_span - 1.0), st->warped, st->tracks,
            st->won_dewarped);
    }
}

static void uftc_verify_output_filesystem(const uint8_t *image, size_t size,
                                          uft_convert_result_t *result)
{
    if (!image || size == 0 || !result) return;

    uft_mfm_detect_info_t info;
    memset(&info, 0, sizeof(info));
    if (uft_mfmd_detect_image(image, size, &info) != UFT_MFMD_OK) return;

    /* Kein Kandidat ODER nur ein schwacher.
     *
     * Die zweite Haelfte ist gemessen, nicht vorsorglich: ein 720-KB-Abbild
     * aus lauter Nullen meldet der Erkenner als „CP/M (vorlaeufig, Stufe 3
     * noetig)" mit Konfidenz 20 — also NICHT als unbekannt. Eine Pruefung
     * allein auf `num_candidates == 0` haette dort geschwiegen. Echte
     * Treffer liegen bei 75-90, die Platzhalter-Vermutung bei 20; die
     * Schwelle 50 trennt beides mit Abstand. */
    if (info.num_candidates == 0 || info.confidence < 50) {
        uftc_add_warning(result,
            "Das erzeugte Abbild traegt kein erkennbares Dateisystem - "
            "moeglich, aber pruefenswert: genau so sahen die Fehler MF-437 "
            "und MF-438 aus");
    } else {
        uftc_add_warning(result, "Dateisystem im Ergebnis: %s (%s, %u %%)",
                         info.fs_name ? info.fs_name : "?",
                         info.system_name ? info.system_name : "?",
                         (unsigned)info.confidence);

        /* Weitere Kandidaten nennen — eine Hybrid-Diskette traegt zwei
         * Dateisysteme, und nur eines zu melden verschweigt das andere.
         *
         * GEMESSEN UND EHRLICH: der Erkenner liefert heute NIE mehr als
         * einen Kandidaten. Er kurzschliesst beim ersten starken Treffer —
         * selbst ein Abbild mit AmigaDOS-Kennung UND gueltigem FAT-BPB
         * ergibt nur „Amiga OFS". Diese Schleife ist also richtig, aber
         * unerreichbar; sie zu pruefen ginge nur ueber eine Aenderung am
         * Erkenner. Siehe KNOWN_ISSUES FLUX-13. */
        for (uint8_t i = 1; i < info.num_candidates; i++) {
            const char *fs = NULL, *sys = NULL;
            uint8_t conf = 0;
            if (uft_mfmd_get_candidate(&info, i, &fs, &sys, &conf))
                uftc_add_warning(result,
                    "Weiteres Dateisystem erkannt: %s (%s, %u %%) - "
                    "moeglicherweise eine Hybrid-Diskette",
                    fs ? fs : "?", sys ? sys : "?", (unsigned)conf);
        }
    }

    uft_mfmd_free(&info);
}

static uft_error_t uftc_convert_scp_to_adf_via_plugin(
    const char* src_path, const char* dst_path,
    const uft_convert_options_ext_t* opts, uft_convert_result_t* result)
{
    const size_t adf_size = (size_t)ADF_CYLS * ADF_HEADS * ADF_SPT * ADF_SECSZ;

    uftc_report_progress(opts, 5, "Opening SCP via format plugin");

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    if (uft_format_plugin_scp.open(&disk, src_path, true) != UFT_OK) {
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result, "SCP open failed via plugin");
        return UFT_ERR_FORMAT;
    }

    /* Der Lesebereich entsteht in drei Stufen (MF-482): was die Datei sagt,
     * was der Aufrufer vorgibt, was das Zielformat fasst.
     *
     * Bis hierher gab es nur Stufe eins und drei — und Stufe drei war STILL.
     * Eine Amiga-Diskette mit 82 oder 83 Zylindern ist nichts Exotisches;
     * genau dort liegen Zusatzkapazitaet und Kopierschutz, und der
     * AmigaDOS-Dekoder laesst Spuren bis 167 ausdruecklich zu (MF-452). Das
     * ADF-Format fasst 80, also MUSS gekuerzt werden — aber „Kein Bit
     * verloren" heisst hier „kein Bit STILL verloren". Die Kuerzung wird
     * gemeldet.
     *
     * Stufe zwei ist a8rawconvs `-g tracks,sides` (a8rawconv.cpp:1354-1364,
     * beim Lesen eines Abbilds als `forced_tracks`/`forced_sides` in
     * rawdiskscp.cpp:126-127). `uft_convert_options_t::target_geometry` gibt
     * es dafuer seit jeher — mit dem Kommentar „(0 = auto)" — und niemand las
     * es je. Ein gesetzter Wert gewinnt gegen die Datei: das ist der Sinn
     * einer Vorgabe, und blindes Lesen ueber das Aufgezeichnete hinaus zaehlt
     * die fehlenden Spuren als nicht gelesen statt sie zu verschweigen. */
    int file_cyls  = disk.geometry.cylinders > 0 ? disk.geometry.cylinders
                                                 : ADF_CYLS;
    int file_heads = disk.geometry.heads     > 0 ? disk.geometry.heads
                                                 : ADF_HEADS;

    int cyls  = file_cyls;
    int heads = file_heads;
    bool forced = false;
    if (opts && opts->target_geometry.cylinders > 0) {
        cyls = opts->target_geometry.cylinders;
        forced = true;
    }
    if (opts && opts->target_geometry.heads > 0) {
        heads = opts->target_geometry.heads;
        forced = true;
    }
    if (forced) {
        uftc_add_warning(result,
            "Geometrie vorgegeben: %d Zylinder x %d Seiten "
            "(aus der Datei abgeleitet waeren %d x %d)",
            cyls, heads, file_cyls, file_heads);
    }

    if (cyls > ADF_CYLS) {
        uftc_add_warning(result,
            "%d Zylinder angefordert, ADF fasst nur %d - %d Zylinder je Seite "
            "nicht uebernommen",
            cyls, ADF_CYLS, cyls - ADF_CYLS);
        cyls = ADF_CYLS;
    }
    if (heads > ADF_HEADS) {
        uftc_add_warning(result,
            "%d Seiten angefordert, ADF fasst nur %d",
            heads, ADF_HEADS);
        heads = ADF_HEADS;
    }

    uint8_t* output = calloc(1, adf_size);
    if (!output) {
        uft_format_plugin_scp.close(&disk);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    uftc_timing_stats_t timing;
    memset(&timing, 0, sizeof(timing));
    uftc_angle_stats_t angles;
    memset(&angles, 0, sizeof(angles));

    flux_decoder_options_t dopts;
    flux_decoder_options_init(&dopts);
    /* Das Medium ist hier bekannt: das Ziel ist ADF, und ADF gibt es nur fuer
     * AmigaDOS-DD (MF-475). Zusammen mit der gemessenen Umdrehungsdauer aus dem
     * SCP-Datensatz (unten) waehlt der Decoder die Zellendauer aus der echten
     * Drehzahl statt aus dem Nennwert. Ohne beides zusammen bleibt es beim
     * Nennwert — ein Medienprofil ohne Messung ist keine Verbesserung. */
    dopts.media = UFT_MEDIA_AMIGA_DD;

    /* Feineinsteller in Prozent (MF-480).
     *
     * Die abgeleitete Zellendauer kann systematisch danebenliegen, auch wenn
     * die Umdrehungsmessung stimmt — dann naemlich, wenn die Diskette mit
     * einer anderen Datenrate beschrieben wurde als das Medienprofil
     * annimmt. Dagegen hilft kein besserer Decoder, sondern nur ein Wert,
     * den der Aufrufer verstellen kann. Der Arbeitsablauf ist alt: in
     * kleinen Schritten nachstellen und sectors_converted gegen
     * sectors_failed beobachten. Beide Zahlen stehen bereits im Ergebnis.
     *
     * Ausserhalb von 50…200 lehnt das Medienprofil den Wert ab und der
     * Decoder faellt still auf 100 zurueck. Still ist hier falsch: wer einen
     * Wert vorgibt, soll erfahren, dass er nicht benutzt wurde. */
    if (opts && opts->decode_cell_adjust_pct > 0.0) {
        if (opts->decode_cell_adjust_pct < 50.0 ||
            opts->decode_cell_adjust_pct > 200.0) {
            uftc_add_warning(result,
                "Zellendauer-Feineinsteller %.1f%% liegt ausserhalb 50-200 "
                "und wurde nicht angewandt",
                opts->decode_cell_adjust_pct);
        } else {
            dopts.media_adjust_pct = opts->decode_cell_adjust_pct;
        }
    }

    /* Alle Umdrehungen, nicht nur die erste (MF-473).
     *
     * Das SCP-Plugin liefert je Spur genau eine Umdrehung — das steht so in
     * seinem Kommentar und ist Absicht. Der kanonische Parser gibt alle
     * heraus, also wird er hier zusaetzlich geoeffnet; das Plugin bleibt
     * zustaendig fuer Kopfpruefung und Geometrie, damit diese Logik nicht ein
     * zweites Mal existiert. Laesst sich die Datei mit dem Parser nicht
     * oeffnen, faellt der Pfad auf die eine Umdrehung des Plugins zurueck —
     * das ist genau das bisherige Verhalten, nicht weniger.
     *
     * `use_multiple_revs` ist seit jeher in den Optionen und war bis hier
     * unbenutzt; der Standard (uft_convert_default_options) steht auf true. */
    const bool want_multirev = (!opts || opts->use_multiple_revs);
    uft_scp_ctx_t sctx;
    memset(&sctx, 0, sizeof(sctx));
    bool multirev = want_multirev &&
                    (uft_scp_open(&sctx, src_path) == UFT_SCP_OK);

    uftc_adf_votes_t* votes = calloc(ADF_SPT, sizeof(*votes));
    if (!votes) {
        if (multirev) uft_scp_close(&sctx);
        uft_format_plugin_scp.close(&disk);
        free(output);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    int class_counts[5] = { 0, 0, 0, 0, 0 };
    int revs_seen = 0, tracks_with_flux = 0;

    uftc_report_progress(opts, 10, "Decoding AmigaDOS sectors from flux");

    for (int cyl = 0; cyl < cyls; cyl++) {
        if (uftc_is_cancelled(opts)) break;
        for (int hd = 0; hd < heads; hd++) {
            memset(votes, 0, ADF_SPT * sizeof(*votes));
            int revs_used = 0;

            if (multirev) {
                /* Dieselbe Abbildung wie im Plugin: 0=C0H0, 1=C0H1, 2=C1H0 … */
                uft_scp_track_data_t td;
                memset(&td, 0, sizeof(td));
                if (uft_scp_read_track(&sctx, cyl * 2 + hd, &td) == UFT_SCP_OK) {
                    for (int r = 0; r < td.revolution_count; r++) {
                        const uft_scp_rev_data_t* rev = &td.revolutions[r];
                        if (!rev->flux_data || rev->flux_count == 0) continue;

                        /* ns intervals -> cumulative transition times (MF-438),
                         * samt der gemessenen Umdrehungsdauer (MF-475). */
                        flux_raw_data_t raw;
                        if (flux_raw_from_ns_intervals_indexed(
                                rev->flux_data, rev->flux_count,
                                rev->index_time_ns, &raw) != FLUX_OK)
                            continue;

                        /* Flippy-Rueckseite: Zeitachse spiegeln, bevor
                         * dekodiert wird (MF-484). */
                        if (opts && opts->reverse_decode)
                            flux_raw_reverse(&raw);

                        flux_decoded_track_t dt;
                        memset(&dt, 0, sizeof(dt));
                        flux_decode_amiga(&raw, &dt, &dopts);
                        uftc_timing_note(&timing, &dt);
                        uftc_angle_note(&angles, &dt,
                                        (double)rev->index_time_ns);
                        uftc_adf_collect(&dt, votes);
                        flux_decoded_track_free(&dt);
                        flux_raw_free(&raw);
                        revs_used++;
                    }
                }
                uft_scp_free_track(&td);
            }

            if (revs_used == 0) {
                /* Rueckfallpfad: eine Umdrehung ueber das Plugin. */
                uft_track_t t;
                memset(&t, 0, sizeof(t));
                if (uft_format_plugin_scp.read_track(&disk, cyl, hd, &t) == UFT_OK
                    && t.flux && t.flux_count > 0) {
                    flux_raw_data_t raw;
                    if (flux_raw_from_ns_intervals_indexed(
                            t.flux, t.flux_count,
                            t.metrics.index_time_ns, &raw) == FLUX_OK) {
                        if (opts && opts->reverse_decode)
                            flux_raw_reverse(&raw);

                        flux_decoded_track_t dt;
                        memset(&dt, 0, sizeof(dt));
                        flux_decode_amiga(&raw, &dt, &dopts);
                        uftc_timing_note(&timing, &dt);
                        uftc_angle_note(&angles, &dt,
                                        (double)t.metrics.index_time_ns);
                        uftc_adf_collect(&dt, votes);
                        flux_decoded_track_free(&dt);
                        flux_raw_free(&raw);
                        revs_used = 1;
                    }
                }
                uft_track_release(&t);
            }

            if (revs_used == 0) { result->tracks_failed++; continue; }
            revs_seen += revs_used;
            tracks_with_flux++;

            int placed = 0;
            for (int s = 0; s < ADF_SPT; s++) {
                if (votes[s].count == 0) { result->sectors_failed++; continue; }
                size_t off = (((size_t)cyl * ADF_HEADS + (size_t)hd) * ADF_SPT
                              + (size_t)s) * ADF_SECSZ;
                if (off + ADF_SECSZ > adf_size) continue;

                /* Forensik: nur was `recovered` ist, wird geschrieben — eine
                 * Lesung ohne gueltige Pruefsumme ueberschreibt nie gute
                 * Daten. Die Klasse sagt, warum. */
                multiread_class_t cls = MULTIREAD_CLASS_UNKNOWN;
                if (uftc_adf_place_voted(&votes[s], output, off, &cls)) placed++;
                else result->sectors_failed++;
                if ((int)cls >= 0 && (int)cls < 5) class_counts[cls]++;
            }
            result->sectors_converted += placed;
            if (placed > 0) result->tracks_converted++;
            else            result->tracks_failed++;
        }
        uftc_report_progress(opts, 10 + (cyl * 80 / (cyls ? cyls : 1)),
                             "Decoding AmigaDOS sectors from flux");
    }

    free(votes);
    if (multirev) uft_scp_close(&sctx);
    uft_format_plugin_scp.close(&disk);

    /* Was die Abstimmung ueber die Diskette sagt. Ohne diese Meldungen bliebe
     * es bei einer Zahl in sectors_failed, die drei verschiedene Ursachen in
     * einen Topf wirft. */
    if (class_counts[MULTIREAD_CLASS_STABLE_BAD_CRC] > 0) {
        uftc_add_warning(result,
            /* Nur ASCII in Meldungstexten - wie jede andere Warnung in diesem
             * Modul; die 256-Byte-Puffer und die GUI-Anzeige sind darauf
             * eingerichtet. */
            "%d Sektoren %s: jede Umdrehung liest dasselbe, die Pruefsumme "
            "bleibt falsch - Kopierschutz, erneutes Lesen aendert das nicht",
            class_counts[MULTIREAD_CLASS_STABLE_BAD_CRC],
            multiread_class_name(MULTIREAD_CLASS_STABLE_BAD_CRC));
    }
    if (class_counts[MULTIREAD_CLASS_WEAK] > 0) {
        uftc_add_warning(result,
            "%d Sektoren %s: die Umdrehungen lasen Unterschiedliches",
            class_counts[MULTIREAD_CLASS_WEAK],
            multiread_class_name(MULTIREAD_CLASS_WEAK));
    }
    if (class_counts[MULTIREAD_CLASS_AMBIGUOUS_GOOD] > 0) {
        uftc_add_warning(result,
            "%d Sektoren %s: gueltige Pruefsumme bei abweichendem Inhalt",
            class_counts[MULTIREAD_CLASS_AMBIGUOUS_GOOD],
            multiread_class_name(MULTIREAD_CLASS_AMBIGUOUS_GOOD));
    }
    if (tracks_with_flux > 0 && revs_seen > tracks_with_flux) {
        uftc_add_warning(result,
            "%d Umdrehungen ueber %d Spuren ausgewertet (%.1f je Spur)",
            revs_seen, tracks_with_flux,
            (double)revs_seen / (double)tracks_with_flux);
    }

    /* Reicht EINE Umdrehung fuer dieses Medium ueberhaupt? (MF-483)
     *
     * AmigaDOS bindet die Sektorlage nicht an die Indexmarke — die Spur wird
     * am Stueck geschrieben, wo der Kopf gerade steht, und das Info-Long
     * fuehrt deshalb ein Feld "sectors to gap". Ein Ein-Umdrehungs-Abbild
     * schneidet einen Sektor, der ueber den Index laeuft, mitten durch, und
     * zwar UNAUFFAELLIG: es fehlt einfach einer, so wie bei einem Lesefehler.
     * Wer das nicht gesagt bekommt, haelt eine unvollstaendige Sicherung fuer
     * eine beschaedigte Diskette.
     *
     * Die Eigenschaft steht im Medienprofil, nicht hier: a8rawconv hat die
     * Warnung fest verdrahtet (rawdiskscp.cpp:120-124), weil es nur Atari
     * kennt. UFT kennt viele Formate — waere sie hier fest, wuerde entweder
     * bei jedem gewarnt oder bei keinem. */
    int min_revs = uft_media_min_revolutions(dopts.media);
    if (tracks_with_flux > 0 && min_revs > 1 &&
        revs_seen < min_revs * tracks_with_flux) {
        const uft_media_profile_t *mp = uft_media_profile(dopts.media);
        uftc_add_warning(result,
            "%s ist nicht indexsynchron und braucht mindestens %d Umdrehungen "
            "je Spur - das Abbild bringt im Schnitt %.1f. Sektoren, die ueber "
            "die Indexmarke laufen, koennen fehlen",
            mp ? mp->name : "Dieses Medium", min_revs,
            (double)revs_seen / (double)tracks_with_flux);
    }

    uftc_timing_report(&timing, result);
    uftc_angle_report(&angles, tracks_with_flux, result);

    uftc_report_progress(opts, 95, "Writing output");
    uftc_verify_output_filesystem(output, adf_size, result);
    /* MF-545: schreiben ODER ablehnen. Vorher folgte `success`
     * allein daraus, dass sich die Datei anlegen liess. */
    uft_error_t err = uftc_finish_or_refuse(result, dst_path,
                                            output, adf_size, "SCP->ADF");
    free(output);
    uftc_report_progress(opts, 100, "SCP->ADF complete");
    return err;
}

uft_error_t uftc_convert_scp_to_mfm_sectors(const uint8_t* src_data,
                                              size_t src_size,
                                              const char* src_path,
                                              const char* dst_path,
                                              uft_format_t dst_format,
                                              const uft_convert_options_ext_t* opts,
                                              uft_convert_result_t* result) {
    /* MF-438: an Amiga target goes to the AmigaDOS decoder. The IBM path
     * below cannot read it — see the note on the helper above. */
    if (dst_format == UFT_FORMAT_ADF && src_path) {
        return uftc_convert_scp_to_adf_via_plugin(src_path, dst_path, opts,
                                                   result);
    }

    uftc_report_progress(opts, 5, "Parsing SCP file");

    uft_scp_file_t scp;
    int rc = uft_scp_read(src_data, src_size, &scp);
    if (rc != 0) {
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result,
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
                /* MF-418: nanoseconds, see note at the first conversion above. */
                double delta_sec = deltas[f] * 1e-9;
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

    /* MF-545: schreiben ODER ablehnen. Vorher folgte `success`
     * allein daraus, dass sich die Datei anlegen liess. */
    uft_error_t err = uftc_finish_or_refuse(result, dst_path,
                                            output, output_size, "SCP->Sektoren");

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
        uftc_add_warning(result,
                 "Kryoflux stream parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    /*
     * UFT-A02: Honest NOT_IMPLEMENTED instead of fabricated empty D64.
     *
     * Background: prior code path here decoded flux into a bitstream and
     * then issued the comment "TODO: Extract sectors from GCR bitstream"
     * before saving an EMPTY d64_create(35) with result->success=true.
     * That violated DESIGN_PRINCIPLE 4 ("Keine erfundenen Daten"): an
     * empty disk image is not a successful conversion of a real KryoFlux
     * capture.
     *
     * Until real GCR sector extraction (C64 1541 GCR encoding, ID/data
     * marks, sync detection, byte-extraction with checksum verify) is
     * implemented here, this entry point returns UFT_ERR_NOT_IMPLEMENTED
     * with a 3-part diagnostic (what / why / fix). The roundtrip-matrix
     * entry for KFX->D64 stays UNTESTED so uft_convert_file/-memory
     * abort earlier via the shared preflight gate; this function is the
     * defense-in-depth in case a caller bypasses the table.
     *
     * Stream parse above already validated the input — a malformed
     * KryoFlux file returns FORMAT, not NOT_IMPLEMENTED.
     */
    (void)dst_path;
    uft_kfx_free(&stream);
    result->error = UFT_ERR_NOT_IMPLEMENTED;
    if (result->warning_count + 3 <=
        sizeof(result->warnings) / sizeof(result->warnings[0])) {
        uftc_add_warning(result,
                 "WHAT: KryoFlux->D64 not implemented "
                 "(GCR sector extraction missing).");
        uftc_add_warning(result,
                 "WHY: prior code wrote empty D64 with success=true "
                 "(DESIGN_PRINCIPLE 4 violation, UFT-A02).");
        uftc_add_warning(result,
                 "FIX: capture with Greaseweazle for now, or wait for "
                 "v4.1.6 GCR sector extractor.");
    }
    uftc_report_progress(opts, 100, "KryoFlux->D64 not implemented");
    return UFT_ERR_NOT_IMPLEMENTED;
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
        uftc_add_warning(result,
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
                uftc_add_warning(result,
                         "No MFM sectors extracted from Kryoflux stream "
                         "(single-track buffer; full disk needs directory iteration)");
            } else {
                uftc_add_warning(result,
                         "Extracted %d sectors from single Kryoflux track; "
                         "full disk conversion requires all track files",
                         found_sectors);
            }

            free(bitstream);
        }
    } else {
        uftc_add_warning(result,
                 "No flux data in Kryoflux stream");
    }

    uftc_report_progress(opts, 80, "Writing ADF output");

    /* MF-545: schreiben ODER ablehnen. Vorher folgte `success`
     * allein daraus, dass sich die Datei anlegen liess. */
    uft_error_t err = uftc_finish_or_refuse(result, dst_path,
                                            output, output_size, "HFE->Sektoren");

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
/* HFE -> ADF through uft_format_plugin_hfe and the AmigaDOS decoder (MF-437).
 *
 * The plugin already de-interleaves the two sides and bit-reverses the LSB-first
 * HFE bytes — exactly what the IBM path below open-codes — and hands back a
 * clean MSB-first bitstream. An HFE stores recovered cells, not flux, so
 * flux_decode_amiga_bits() consumes it directly with no PLL in between. */
static uft_error_t uftc_convert_hfe_to_adf_via_plugin(
    const char* src_path, const char* dst_path,
    const uft_convert_options_ext_t* opts, uft_convert_result_t* result)
{
    /* Geometrie-Konstanten stehen seit MF-473 im Dateikopf (die SCP-Variante
     * braucht sie ausserhalb ihrer Funktion) — hier keine zweite Kopie. */
    const size_t adf_size = (size_t)ADF_CYLS * ADF_HEADS * ADF_SPT * ADF_SECSZ;

    uftc_report_progress(opts, 10, "Opening HFE via format plugin");

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    if (uft_format_plugin_hfe.open(&disk, src_path, true) != UFT_OK) {
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result, "HFE open failed via plugin");
        return UFT_ERR_FORMAT;
    }

    int cyls  = disk.geometry.cylinders > 0 ? disk.geometry.cylinders : ADF_CYLS;
    int heads = disk.geometry.heads     > 0 ? disk.geometry.heads     : ADF_HEADS;
    if (cyls > ADF_CYLS)  cyls = ADF_CYLS;
    if (heads > ADF_HEADS) heads = ADF_HEADS;

    uint8_t* output = calloc(1, adf_size);
    if (!output) {
        uft_format_plugin_hfe.close(&disk);
        result->error = UFT_ERR_MEMORY;
        return UFT_ERR_MEMORY;
    }

    flux_decoder_options_t dopts;
    flux_decoder_options_init(&dopts);

    uftc_report_progress(opts, 20, "Decoding AmigaDOS sectors from HFE");

    for (int cyl = 0; cyl < cyls; cyl++) {
        if (uftc_is_cancelled(opts)) break;
        for (int hd = 0; hd < heads; hd++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (uft_format_plugin_hfe.read_track(&disk, cyl, hd, &t) != UFT_OK ||
                !t.raw_data || t.raw_size == 0) {
                uft_track_release(&t);
                result->tracks_failed++;
                continue;
            }

            flux_decoded_track_t dt;
            memset(&dt, 0, sizeof(dt));
            flux_decode_amiga_bits(t.raw_data, t.raw_size * 8, &dt, &dopts);

            int placed = 0;
            for (int s = 0; s < dt.sector_count; s++) {
                flux_decoded_sector_t* sec = &dt.sectors[s];
                if (!sec->data || sec->data_size != ADF_SECSZ) continue;
                if (sec->sector >= ADF_SPT) continue;
                if (!sec->data_crc_ok) { result->sectors_failed++; continue; }
                size_t off = (((size_t)cyl * ADF_HEADS + (size_t)hd) * ADF_SPT
                              + sec->sector) * ADF_SECSZ;
                if (off + ADF_SECSZ > adf_size) continue;
                memcpy(output + off, sec->data, ADF_SECSZ);
                placed++;
            }
            result->sectors_converted += placed;
            /* Only a track that yielded sectors counts as converted — the old
             * path incremented this unconditionally. */
            if (placed > 0) result->tracks_converted++;
            else            result->tracks_failed++;

            flux_decoded_track_free(&dt);
            uft_track_release(&t);
        }
        uftc_report_progress(opts, 20 + (cyl * 70 / (cyls ? cyls : 1)),
                             "Decoding AmigaDOS sectors from HFE");
    }
    uft_format_plugin_hfe.close(&disk);

    uftc_report_progress(opts, 95, "Writing output");
    uftc_verify_output_filesystem(output, adf_size, result);
    /* MF-545: schreiben ODER ablehnen. Vorher folgte `success`
     * allein daraus, dass sich die Datei anlegen liess. */
    uft_error_t err = uftc_finish_or_refuse(result, dst_path,
                                            output, adf_size, "HFE->ADF");
    free(output);
    uftc_report_progress(opts, 100, "HFE->ADF complete");
    return err;
}

uft_error_t uftc_convert_hfe_to_sectors(const uint8_t* src_data,
                                          size_t src_size,
                                          const char* src_path,
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
        uftc_add_warning(result,
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

    /* MF-437: an Amiga target is decoded by the AmigaDOS decoder, not by the
     * IBM System-34 loop below.
     *
     * That loop needs three consecutive 0x4489 sync words, an IDAM 0xFE and a
     * DAM 0xFB. AmigaDOS has two syncs per sector and neither mark, so on an
     * Amiga disk it fired exactly zero times — while tracks_converted++ ran
     * unconditionally per head and success was set as soon as the file was
     * written. The result was an 880 KB file of zeros reported as 160
     * converted tracks. test_convert_hfe_adf pins that measurement and pins
     * the correct decode against the source ADF, byte for byte. */
    if (dst_format == UFT_FORMAT_ADF && src_path) {
        free(output);
        return uftc_convert_hfe_to_adf_via_plugin(src_path, dst_path, opts,
                                                   result);
    }

    uftc_report_progress(opts, 20, "Extracting MFM sectors from HFE");

    /* Read track LUT.
     *
     * MF-526: dieselbe fehlende Schranke wie in uftc_convert_hfe_to_scp().
     * `track_list_offset`, `n_cylinders` und `lut[].offset/length` kommen
     * alle aus der Datei und duerfen beliebig sein. Bei einer auf die
     * Haelfte gekuerzten HFE zeigt die LUT auf Tracks, die es nicht mehr
     * gibt — der Zugriff lag dann hinter dem Puffer. Gefunden von
     * tests/test_convert_fuzz.c, nachdem der erste Durchgang nur den
     * SCP-Zweig geschlossen hatte: zwei Kopien derselben Rechnung, und ich
     * hatte nur eine gesehen. */
    const size_t lut_start = (size_t)hdr->track_list_offset * 512;
    if (lut_start >= src_size ||
        (src_size - lut_start) / sizeof(hfe_track_entry_t) < (size_t)cylinders) {
        free(output);
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

        for (int hd = 0; hd < heads; hd++) {
            /* De-interleave to get single-head track data */
            uint8_t* track_bits = malloc(track_len);
            if (!track_bits) {
                result->tracks_failed++;
                continue;
            }

            /* MF-526: lut[].length ist die Gesamtlaenge BEIDER Seiten;
             * hfe_deinterleave_track() will die je Seite. Die
             * Gesamtlaenge zu uebergeben verdoppelt die Blockzahl und
             * liest hinter den Track hinaus. */
            const uint16_t head_len = (uint16_t)(track_len / 2);
            hfe_deinterleave_track(interleaved, head_len, (uint8_t)hd,
                                    track_bits);
            /* HFE stores bits LSB-first */
            hfe_reverse_bits(track_bits, head_len);

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
        uftc_add_warning(result,
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

    /* Spurlaenge JE SEITE, aus der Zellrate gerechnet, mit der weiter unten
     * tatsaechlich dekodiert wird.
     *
     * MF-528: hier stand `int mfm_track_bytes = 6400; /* ~100ms at 250Kbps *\/`
     * — eine feste Zahl, die den Fluss gar nicht ansah. Zwei Fehler darin,
     * die sich multiplizierten:
     *
     *   1. 6400 Byte sind 250 kbps x 200 ms, also die DATENRATE. Der HFE-
     *      Bitstrom speichert aber ZELLEN, und dekodiert wird unten mit
     *      `uft_pll_init(&pll, 500000.0, UFT_ENCODING_MFM)` — 500 kbps.
     *      Faktor 2.
     *   2. `lut[].length` bekam denselben Wert. Das Feld ist aber die
     *      GESAMTLAENGE beider Seiten (MF-526), nicht die je Seite.
     *      Nochmal Faktor 2.
     *
     * Zusammen Faktor 4, und genau das war messbar: der Rundlauf
     * HFE -> SCP -> HFE machte aus einer Amiga-Spur von 25336 Byte eine
     * von 6400 (tests/test_convert_roundtrip_lossless.c, MF-527). Der
     * Rest wurde vom `bytes_to_copy`-Deckel weiter unten still
     * abgeschnitten, waehrend `tracks_converted++` trotzdem lief.
     *
     * Die Zahlen hier sind gemessen, nicht gewaehlt: die Quelldatei
     * gw_amigados.hfe fuehrt 25336 Byte je Spur fuer beide Seiten, also
     * 12668 je Seite. 500000 Zellen/s x 0,2 s / 8 = 12500 Byte — dieselbe
     * Groessenordnung, auf 256 aufgerundet 12544. */
    const double cell_rate_hz = 500000.0;   /* wie uft_pll_init() unten */
    const double rev_seconds  = 60.0 / 300.0;  /* 300 U/min */
    int mfm_track_bytes = (int)((cell_rate_hz * rev_seconds) / 8.0 + 0.5);
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
        /* MF-528: das Feld ist die GESAMTLAENGE beider Seiten
         * (MF-526), track_len_aligned ist die je Seite. Hier stand
         * die halbe Zahl — ein Leser, der sie als Gesamtlaenge nimmt,
         * sah damit nur die Haelfte der geschriebenen Bytes. */
        lut[cyl].length = (uint16_t)(track_len_aligned * 2);
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
                double delta_sec = deltas[f] * 1e-9;  /* MF-418: uft_scp_get_track_flux()
                                                 * yields NANOSECONDS, with the
                                                 * SCP resolution multiplier already
                                                 * applied by the shipped parser. The
                                                 * previous 25e-9 assumed raw ticks and
                                                 * was off by 25*(1+resolution). Never
                                                 * observed because the accessor was a
                                                 * stub returning -1. */
                uft_pll_process_flux_mfm(&pll, delta_sec, raw_bitstream, &bit_pos);
            }

            /* Copy decoded bitstream into track buffer, truncating to track_len_aligned */
            size_t bytes_to_copy = (bit_pos + 7) / 8;
            if (bytes_to_copy > (size_t)track_len_aligned) {
                /* MF-528: hier wurde still gekappt und danach trotzdem
                 * `tracks_converted++` gezaehlt. Eine Spur, von der drei
                 * Viertel fehlen, als "konvertiert" zu melden, ist genau
                 * die stille Veraenderung, die DESIGN_PRINCIPLES verbietet.
                 * Gekappt wird weiterhin — der Puffer ist fest —, aber es
                 * steht jetzt im Ergebnis. */
                uftc_add_warning(result,
                    "Spur %d/%d: Bitstrom %zu Byte, Platz %d Byte — "
                    "%zu Byte abgeschnitten",
                    cyl, hd, bytes_to_copy, track_len_aligned,
                    bytes_to_copy - (size_t)track_len_aligned);
                result->tracks_failed++;
                bytes_to_copy = (size_t)track_len_aligned;
            }

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

    /* MF-545: schreiben ODER ablehnen. Vorher folgte `success`
     * allein daraus, dass sich die Datei anlegen liess. */
    uft_error_t err = uftc_finish_or_refuse(result, dst_path,
                                            hfe_data, total_size, "SCP->HFE");

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
        uftc_add_warning(result,
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
            double delta_sec = deltas[f] * 1e-9;  /* MF-418: uft_scp_get_track_flux()
                                                 * yields NANOSECONDS, with the
                                                 * SCP resolution multiplier already
                                                 * applied by the shipped parser. The
                                                 * previous 25e-9 assumed raw ticks and
                                                 * was off by 25*(1+resolution). Never
                                                 * observed because the accessor was a
                                                 * stub returning -1. */
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

        /* MF-534: der Rueckgabewert wurde verworfen und `tracks_converted++`
         * lief unabhaengig davon. `g64_set_track()` weist Halbspuren
         * ausserhalb [2, G64_MAX_TRACKS) ab und meldet das mit -1.
         *
         * Gemessen am Rundlauf G64 -> SCP -> G64: die Wandlung meldete
         * "35 Spuren gewandelt" und schrieb eine Datei von **789 Byte** —
         * Kopf und Tabelle, kein einziges Spurdatum
         * (tests/test_convert_roundtrip_measured.c). Eine Erfolgsmeldung
         * ohne Tat, dieselbe Bauart wie MF-522 und MF-528.
         *
         * Der Zaehler sagt jetzt die Wahrheit. Warum die Spuren leer
         * bleiben, ist damit NICHT behoben — es wird nur sichtbar; siehe
         * OPEN_ITEMS P0-15. */
        int set_rc = g64_set_track(g64, halftrack, bitstream, track_bytes,
                                   (uint8_t)zone);
        free(bitstream);
        if (set_rc != 0 || track_bytes == 0) {
            result->tracks_failed++;
            continue;
        }
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
        uftc_add_warning(result,
                 "Kryoflux stream parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    if (stream.flux_count == 0 || !stream.flux_deltas) {
        uft_kfx_free(&stream);
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result,
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
    /* MF-555: die Antwort des Schreibers wird gelesen.
     *
     * Sie wurde verworfen, und `result->tracks_converted = 1;` stand
     * unmittelbar dahinter — die eine Spur galt als gewandelt, gleich ob
     * der Schreiber sie angenommen hatte.
     *
     * Bei einer Ein-Spur-Wandlung wie dieser ist das besonders deutlich:
     * scheitert der Aufruf, enthaelt die SCP nichts, und das Ergebnis
     * meldet trotzdem eine gewandelte Spur. */
    int add_rc = 0;
    for (int rev = 0; rev < revolutions; rev++) {
        if (scp_writer_add_track(writer, 0, 0, flux_buf, stream.flux_count,
                                  duration_ns, rev) != 0)
            add_rc = -1;
    }
    if (add_rc != 0) {
        uftc_add_warning(result,
                 "KryoFlux->SCP: der SCP-Schreiber hat die Spur abgewiesen "
                 "— das Abbild bleibt leer (MF-555)");
        result->tracks_failed = 1;
        result->tracks_converted = 0;
    } else {
        result->tracks_converted = 1;
    }
    uftc_add_warning(result,
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
        uftc_add_warning(result,
                 "Kryoflux stream parse failed (error %d)", rc);
        return UFT_ERR_FORMAT;
    }

    if (stream.flux_count == 0 || !stream.flux_deltas) {
        uft_kfx_free(&stream);
        result->error = UFT_ERR_FORMAT;
        uftc_add_warning(result,
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
    lut[0].length = (uint16_t)(track_len_aligned * 2);

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
    uftc_add_warning(result,
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

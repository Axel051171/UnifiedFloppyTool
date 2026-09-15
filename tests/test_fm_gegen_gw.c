/**
 * @file test_fm_gegen_gw.c
 * @brief der erste inhaltliche Fremdzeuge auf der ZELLEBENE (MF-1143)
 *
 * ── Die Luecke, die das schliesst ─────────────────────────────────────
 *
 * P3-389 hat die duennste Achse dieses Baums beziffert: auf der
 * BEHAELTERebene haben **72 von 88** Formaten einen Beleg von fremder
 * Hand, auf der Zell-/Bitebene **null**. Zwischen Zellen, Bits, Sync und
 * ID/DAM verglich kein einziger Test Inhalt gegen fremde Ausgabe.
 *
 * Der Grund war benannt und gemessen: **dieser Baum hat keinen
 * FM-Encoder** (P3-218, MF-864). Es gab also keine hauseigene
 * Moeglichkeit, eine FM-Pruefspur zu erzeugen, und `fluxtoimd` — die
 * Fremdabnahme aus MF-864 — DEKODIERT nur. Ein Fixture aus derselben
 * Hand wie der Dekoder waere wertlos gewesen.
 *
 * ── Der Erzeuger, und warum er einer ist ──────────────────────────────
 *
 * `greaseweazle` 1.23 (keirf, **Unlicense**), Kanal *Oracle* nach
 * MF-695 — ausgefuehrt, nicht eingebunden. Registriert als `gw` in
 * `tests/differential/oracles.py`.
 *
 *     gw convert --format=acorn.dfs.ss80 --tracks="c=0-2:h=0" \
 *                <selbstbenennende SSD> gw_fm_acorn_3trk.scp
 *
 * **Dass gw wirklich FM KODIERT und nicht durchreicht, ist dreifach
 * gemessen** (MF-1142) — ohne diese Gegenprobe waere das Abbild kein
 * Beleg, sondern eine Verpackung (Klasse MF-1021, wo hxcfe Erfolg
 * meldete und eine Datei zu 100 % aus Fuellbyte schrieb):
 *
 *   1. die Marke `UFT-GW` kommt im Erzeugnis **0 Mal im Klartext** vor;
 *   2. gws eigener Dekoder meldet beim Ruecklesen je Spur
 *      `IBM FM (10/10 sectors) from Bitcells (100736 bits,
 *      504.0 kbit/s, 300.2 rpm)`;
 *   3. der Rundlauf SSD -> HFE -> SSD ist byteidentisch, 800 von 800
 *      Sektoren an ihrer eigenen Marke.
 *
 * Encoder-Quelle benannt: `src/greaseweazle/codec/ibm/ibm.py:50`
 * `fm_encode()`, `:402-420` `master_track()`.
 *
 * ── Was hier gemessen wird ────────────────────────────────────────────
 *
 * Der Weg ist der PRODUKTIONSPFAD, nicht ein Nachbau: das SCP-Plugin
 * liefert den Fluss, `flux_raw_from_ns_intervals()` wandelt Intervalle
 * in kumulative Zeiten (MF-438 — zwei Darstellungen, ein Wort), und
 * `flux_decode_fm()` dekodiert. Verglichen wird gegen die Marken, die
 * die Eingabe selbst traegt; ein Ergebnis sagt damit nicht nur, DASS
 * etwas kam, sondern ob die RICHTIGE Stelle getroffen war.
 *
 * Gemessen: **30 von 30** Sektoren a 256 Byte ueber drei Spuren.
 *
 * **Die Sektoren beginnen je Spur an anderer Stelle** — S00, S07, S04 —,
 * weil gw einen Rotationsversatz anwendet. Der Dekoder findet sie
 * trotzdem alle, und das ist die schaerfere Aussage: er haengt nicht an
 * einer festen Startstelle.
 *
 * ── Der Behaelter ist SCP und NICHT HFE, und das ist gemessen ─────────
 *
 * gw schreibt auf demselben Weg auch eine HFE — die traegt aber bei
 * Byte 11 `track_encoding = 0xFF` (**UNKNOWN**), nicht `0x02`
 * (ISOIBM_FM). UFT bildet das ehrlich ab (`hfe_to_uft_encoding()` ->
 * `UFT_ENC_UNKNOWN`, und `uft_encoding_caps.c` fuehrt dafuer
 * `can_detect = false, can_decode = false`), raet also nicht MFM —
 * damit ist die HFE als FM-Orakel unbrauchbar. Ein FLUSSbehaelter
 * braucht keine Kodierungsangabe, und UFTs Flusspfad MISST die
 * Zellzeit aus dem Intervall-Histogramm. Siehe P3-398.
 *
 * ── Was dieser Test NICHT belegt ──────────────────────────────────────
 *
 * Keine echte AUFNAHME. Der Fluss ist von gw **synthetisiert**, also
 * ohne Jitter, Drift und Schwachstellen einer wirklichen Diskette; das
 * belegt MF-869 an einer Applesauce-Aufnahme einer 8-Zoll-Diskette von
 * 1979 (32 Sektoren, 0 CRC-Fehler, 26 von 26 byteidentisch gegen die
 * IMD). Die beiden ergaenzen sich: dort ein echtes Objekt ohne
 * bekannte Wahrheit im Inhalt, hier bekannte Wahrheit ohne echte
 * Aufnahme.
 *
 * Und ein Orakel ist eine Referenz, kein Beweis (MF-1015): bei
 * `fdc_bitstream` sah es nach einem HFE-Orakel aus, und MF-1125 hat
 * gemessen, dass sein Leser 132 Byte Polster als Fluss nimmt und
 * 124 Byte hinter seinem eigenen Puffer liest.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "uft/uft_format_plugin.h"
#include "uft/flux/uft_flux_decoder.h"

extern const uft_format_plugin_t uft_format_plugin_scp;

#define SPUREN  3
#define SPT    10
#define SS    256

static int gruen = 0, rot = 0;
static void zusage(const char *was, int ok)
{
    if (ok) { printf("  [OK]   %s\n", was); gruen++; }
    else    { printf("  [ROT]  %s\n", was); rot++; }
}

static const char *abbild(void)
{
#ifdef UFT_CORPUS_FREE_DIR
    static char p[512];
    snprintf(p, sizeof p, "%s/gw_fm_acorn_3trk.scp", UFT_CORPUS_FREE_DIR);
    return p;
#else
    return "tests/corpus_free/gw_fm_acorn_3trk.scp";
#endif
}

int main(void)
{
    printf("\nFM gegen gw 1.23 - erster Fremdzeuge auf der Zellebene "
           "(MF-1143)\n\n");

    const char *pfad = abbild();
    FILE *f = fopen(pfad, "rb");
    if (!f) {
        /* Ohne Abbild wird benannt uebersprungen statt gruen gemeldet
         * (MF-1103: ein Test als „Skipped" in einem 100-%-Lauf). */
        printf("  [SKIP] Abbild fehlt: %s\n", pfad);
        return 77;
    }
    fclose(f);

    uft_disk_t d;
    memset(&d, 0, sizeof d);
    d.read_only = true;
    zusage("das SCP-Abbild laesst sich oeffnen",
           uft_format_plugin_scp.open(&d, pfad, true) == UFT_OK);
    if (rot) { printf("\n%d gruen, %d rot\n", gruen, rot); return 1; }

    long gefunden = 0, erwartet = 0;
    int spuren_mit_fluss = 0;

    for (int c = 0; c < SPUREN; c++) {
        uft_track_t t;
        memset(&t, 0, sizeof t);
        if (uft_format_plugin_scp.read_track(&d, c, 0, &t) != UFT_OK) {
            erwartet += SPT;
            continue;
        }
        if (!t.flux || t.flux_count == 0) {
            erwartet += SPT;
            uft_track_release(&t);
            continue;
        }
        spuren_mit_fluss++;

        /* MF-438: das Plugin liefert INTERVALLE, `flux_raw_data_t` will
         * KUMULATIVE Zeiten. */
        flux_raw_data_t raw;
        if (flux_raw_from_ns_intervals(t.flux, t.flux_count, &raw)
            != FLUX_OK) {
            erwartet += SPT;
            uft_track_release(&t);
            continue;
        }

        flux_decoder_options_t opts;
        flux_decoder_options_init(&opts);

        flux_decoded_track_t fm;
        memset(&fm, 0, sizeof fm);
        int frc = flux_decode_fm(&raw, &fm, &opts);

        char txt[160];
        snprintf(txt, sizeof txt,
                 "Spur %d: flux_decode_fm liefert %u Sektoren (rc=%d)",
                 c, (unsigned)fm.sector_count, frc);
        zusage(txt, frc == FLUX_OK && fm.sector_count == SPT);

        for (int s = 0; s < SPT; s++) {
            char marke[40];
            snprintf(marke, sizeof marke, "UFT-GW C%02d H0 S%02d ", c, s);
            erwartet++;
            for (size_t k = 0; k < fm.sector_count; k++) {
                const flux_decoded_sector_t *sec = &fm.sectors[k];
                if (sec->data && sec->data_size == SS
                    && memcmp(sec->data, marke, strlen(marke)) == 0) {
                    gefunden++;
                    break;
                }
            }
        }

        flux_decoded_track_free(&fm);
        flux_raw_free(&raw);
        uft_track_release(&t);
    }
    uft_format_plugin_scp.close(&d);

    zusage("alle drei Spuren tragen Fluss", spuren_mit_fluss == SPUREN);

    char txt[200];
    snprintf(txt, sizeof txt,
             "%ld von %ld selbstbenennenden Sektoren an ihrer eigenen "
             "Marke, je %d Byte", gefunden, erwartet, SS);
    zusage(txt, gefunden == erwartet && erwartet == SPUREN * SPT);

    /* Gegenprobe: derselbe Fluss durch den MFM-Dekoder darf NICHTS
     * liefern. Ohne sie koennte „irgendein Dekoder findet irgendwas"
     * als Erfolg durchgehen — dieselbe Form wie in
     * test_convert_scp_adf.c, wo der IBM-Dekoder auf einer Amiga-Spur
     * 0 Sektoren liefern MUSS. */
    memset(&d, 0, sizeof d);
    d.read_only = true;
    if (uft_format_plugin_scp.open(&d, pfad, true) == UFT_OK) {
        uft_track_t t;
        memset(&t, 0, sizeof t);
        if (uft_format_plugin_scp.read_track(&d, 0, 0, &t) == UFT_OK
            && t.flux && t.flux_count) {
            flux_raw_data_t raw;
            if (flux_raw_from_ns_intervals(t.flux, t.flux_count, &raw)
                == FLUX_OK) {
                flux_decoder_options_t opts;
                flux_decoder_options_init(&opts);
                flux_decoded_track_t mfm;
                memset(&mfm, 0, sizeof mfm);
                flux_decode_mfm(&raw, &mfm, &opts);
                char g[160];
                snprintf(g, sizeof g,
                         "Gegenprobe: der MFM-Dekoder findet auf derselben "
                         "FM-Spur %u Sektoren", (unsigned)mfm.sector_count);
                zusage(g, mfm.sector_count == 0);
                flux_decoded_track_free(&mfm);
                flux_raw_free(&raw);
            }
            uft_track_release(&t);
        }
        uft_format_plugin_scp.close(&d);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

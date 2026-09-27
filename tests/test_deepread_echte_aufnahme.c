/* SPDX-License-Identifier: MIT */
/**
 * @file test_deepread_echte_aufnahme.c
 * @brief DeepRead an einer ECHTEN Aufnahme mit belegtem Zustand (P3-630, MF-1471)
 *
 * ── WORUM ES GEHT ────────────────────────────────────────────────────
 *
 * P3-630 hielt fest: keine Schwelle der DeepRead-Klassen ist geeicht, und
 * der freie Korpus hatte keine Flussaufnahme mit echtem Jitter. Dieser
 * Test fuehrt die erste, und er misst an ihr, was die Klassen sagen.
 *
 * ── DIE AUFNAHME UND IHR ZUSTAND ─────────────────────────────────────
 *
 * `fluxfox_sector_test_t34_t77.scp` ist ein Ausschnitt aus fluxfox'
 * `tests/images/sector_test/sector_test_360k.scp` (Daniel Balsom, MIT,
 * Quellstand 1d72ff1b): die Spurbloecke 34 und 77 unveraendert, alle drei
 * Umdrehungen, Spurtafel und Pruefsumme neu gerechnet. Dass sie ECHT ist
 * und nicht aus einem Sektorabbild kodiert, ist gemessen: Umdrehung 0 und
 * 1 von Spur 0 unterscheiden sich in 18 851 von 42 564 Intervallen, die
 * Intervalle streuen von 3 350 bis 10 975 ns.
 *
 * Der Zustand ist doppelt belegt, und zwar NICHT durch UFT allein:
 *   - Inhalt: fluxfox legt dasselbe Abbild als `sector_test_360k.img` bei;
 *     jeder Sektor k (linear, Spur*9 + Sektor-1) ist ganz mit `k mod 256`
 *     gefuellt — gemessen 720 von 720 Sektoren.
 *   - Zwei Leser: dieselbe Diskette liegt dort ein zweites Mal als
 *     KryoFlux-Strom (`sector_test_kryoflux_360k.zip`). Beide Aufnahmen
 *     ergeben ueber `flux_decode_mfm()` je **719 von 720** CRC-guten,
 *     inhaltsgleichen Sektoren und denselben einen Fehler: **C38 H1 S9**
 *     (Spur 77), Daten-CRC falsch in allen drei Umdrehungen beider Leser,
 *     mit wechselndem Fehlerbild (121 / 218 abweichende Byte). Der Fehler
 *     liegt nicht ueber dem Index — die Umdrehungen 0+1 aneinandergehaengt
 *     lesen ihn ebenso falsch. Er ist eine Eigenschaft des Traegers.
 *
 * Spur 34 ist gewaehlt, weil sie auf der ganzen Diskette den GROESSTEN
 * Alterungsrest hat (16,04 dB) — und dabei 9 von 9 Sektoren gut liest.
 *
 * ── DIE BEFUNDE ──────────────────────────────────────────────────────
 *
 *   E1  Alterung: der Rest > 10 dB, den `uft_deepread_aging_*` als
 *       Schadensbereich zaehlt, trifft auf der ganzen Diskette 35 von 80
 *       Spuren (KryoFlux-Lesung: 44) — und die Klasse lautet darum
 *       „Damaged" auf einer Diskette mit 719 von 720 guten Sektoren. Die
 *       Spur mit dem EINEN echten Fehler (77, Rest 4,70 dB) ist nicht
 *       darunter. Die Klasse ist damit an dieser Aufnahme widerlegt.
 *   E2  Schreibnaht: die groesste Stufe des Qualitaetsprofils liegt auf
 *       79 von 80 Spuren in den ersten 64 Zellen — dort, wo die
 *       Profilrechnung nach dem Index einschwingt (Qualitaet -3 bis -7 dB
 *       in den Zellen 0-16, -1,65 dB bei Zelle 24). Nimmt man die ersten
 *       64 Zellen aus, bleibt eine Stufe um 3 dB, die ueber die drei
 *       Umdrehungen nur auf 10 von 80 Spuren innerhalb 50 Zellen
 *       wiederkehrt und nur auf 13 von 80 beim zweiten Leser an derselben
 *       Stelle liegt: Rauschen. Eine Naht ist an dieser Aufnahme nicht
 *       messbar; ein Ausschlussfenster wuerde nur anderes Rauschen zeigen.
 *   E3  Fingerabdruck: zwei Umdrehungen derselben Lesung liegen im
 *       Kosinus bei > 0,9999, die Kennung (CRC32) unterscheidet sich
 *       trotzdem — der Befund F1 aus `test_deepread_messung.c`, jetzt an
 *       echtem Jitter. (Ganze Diskette, nicht hier gepinnt: derselbe
 *       Traeger ueber den zweiten Leser 0,9638-0,9643, eine andere
 *       Diskette 0,635-0,697.)
 *
 * Referenz: die Aufnahme selbst und das beiliegende Sektorabbild; die
 * Kenngroessen haben keine fremde Quelle.
 */
#include "uft/analysis/floppy_otdr.h"
#include "uft/analysis/uft_deepread_aging.h"
#include "uft/analysis/uft_deepread_fingerprint.h"
#include "uft/analysis/uft_deepread_splice.h"
#include "uft/flux/uft_scp_parser.h"
#include "uft/flux/uft_flux_decoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_FREE_DIR
#define UFT_CORPUS_FREE_DIR "tests/corpus_free"
#endif
#define ECHT_SCP UFT_CORPUS_FREE_DIR "/fluxfox_sector_test_t34_t77.scp"

#define SPUR_GUT34 34
#define SPUR_FEHLER77 77
#define SPT 9
#define SS 512

static int fehler = 0;
static int geprueft = 0;

#define PRUEFE(bed, ...) do {                                   \
        geprueft++;                                             \
        if (!(bed)) {                                           \
            fehler++;                                           \
            printf("  FEHLER %s:%d: ", __FILE__, __LINE__);     \
            printf(__VA_ARGS__);                                \
            printf("\n");                                       \
        }                                                       \
    } while (0)

static uft_scp_track_data_t g_spur[2];   /* [0] = 34, [1] = 77 */

static int laden(void)
{
    uft_scp_ctx_t ctx;
    memset(&ctx, 0, sizeof ctx);
    if (uft_scp_open(&ctx, ECHT_SCP) != 0) return 0;
    int ok = uft_scp_read_track(&ctx, SPUR_GUT34, &g_spur[0]) == 0 &&
             uft_scp_read_track(&ctx, SPUR_FEHLER77, &g_spur[1]) == 0 &&
             g_spur[0].revolution_count == 3 && g_spur[1].revolution_count == 3;
    uft_scp_close(&ctx);
    return ok;
}

/* ── Zustand: welche Sektoren sind gut? ───────────────────────────── */

/* Zaehlt in EINER Umdrehung die guten Sektoren mit richtigem Inhalt;
 * meldet ueber *s9_kaputt, ob Sektor 9 mit gutem ID-, aber falschem
 * Daten-CRC gelesen wurde. */
static int gute_sektoren(const uft_scp_rev_data_t *rev, int spur, int *s9_kaputt)
{
    flux_raw_data_t raw;
    *s9_kaputt = 0;
    if (flux_raw_from_ns_intervals(rev->flux_data, rev->flux_count, &raw) != FLUX_OK)
        return -1;
    flux_decoder_options_t o;
    flux_decoder_options_init(&o);
    flux_decoded_track_t m;
    memset(&m, 0, sizeof m);
    flux_decode_mfm(&raw, &m, &o);
    int gut = 0;
    for (int s = 1; s <= SPT; s++) {
        uint8_t fuell = (uint8_t)((spur * SPT + s - 1) % 256);
        int dieser = 0;
        for (size_t k = 0; k < m.sector_count; k++) {
            const flux_decoded_sector_t *x = &m.sectors[k];
            if (x->cylinder != spur / 2 || x->head != spur % 2 || x->sector != s
                || !x->data || x->data_size != SS)
                continue;
            if (x->id_crc_ok && x->data_crc_ok) {
                int alle = 1;
                for (int i = 0; i < SS; i++) alle &= x->data[i] == fuell;
                dieser |= alle;
            } else if (s == 9 && x->id_crc_ok && !x->data_crc_ok) {
                *s9_kaputt = 1;
            }
        }
        gut += dieser;
    }
    flux_decoded_track_free(&m);
    flux_raw_free(&raw);
    return gut;
}

static void t_zustand_ist_belegt(void)
{
    for (int r = 0; r < 3; r++) {
        int s9;
        int g = gute_sektoren(&g_spur[0].revolutions[r], SPUR_GUT34, &s9);
        PRUEFE(g == 9, "Spur 34 Umdrehung %d: 9 von 9 gut, waren %d", r, g);
        g = gute_sektoren(&g_spur[1].revolutions[r], SPUR_FEHLER77, &s9);
        PRUEFE(g == 8, "Spur 77 Umdrehung %d: 8 von 9 gut, waren %d", r, g);
        PRUEFE(s9, "Spur 77 Umdrehung %d: S9 mit gutem ID- und falschem Daten-CRC", r);
    }
}

/* ── Die Spuren wie das OTDR-Panel ────────────────────────────────── */

static otdr_track_t *spur_wie_panel(const uft_scp_track_data_t *td, int nur_rev)
{
    otdr_track_t *t = (otdr_track_t *)calloc(1, sizeof *t);
    int slot = 0;
    for (int r = 0; r < (int)td->revolution_count && r < OTDR_MAX_REVOLUTIONS; r++) {
        if (nur_rev >= 0 && r != nur_rev) continue;
        otdr_track_load_flux(t, td->revolutions[r].flux_data,
                             td->revolutions[r].flux_count, (uint8_t)slot++);
    }
    otdr_config_t cfg;
    otdr_config_defaults(&cfg);
    otdr_track_analyze(t, &cfg);
    return t;
}

/* ── E1 ───────────────────────────────────────────────────────────── */
static void t_alterungsrest_trifft_die_gesunde_spur(void)
{
    otdr_track_t *g = spur_wie_panel(&g_spur[0], -1);
    otdr_track_t *d = spur_wie_panel(&g_spur[1], -1);
    float s, r2, rest_g = 0, rest_d = 0;
    PRUEFE(uft_deepread_aging_track(g, &s, &r2, &rest_g) == 0, "aging Spur 34");
    PRUEFE(uft_deepread_aging_track(d, &s, &r2, &rest_d) == 0, "aging Spur 77");
    PRUEFE(rest_g > 10.0f,
           "Spur 34 (9 von 9 gut): Rest > 10 dB, also als Schaden gezaehlt; war %.2f", rest_g);
    PRUEFE(rest_d < 10.0f && rest_d < rest_g,
           "Spur 77 (der echte Fehler): Rest < 10 dB und kleiner als auf Spur 34; "
           "war %.2f gegen %.2f", rest_d, rest_g);
    otdr_track_free(g);
    otdr_track_free(d);
}

/* ── E2 ───────────────────────────────────────────────────────────── */
static void t_naht_liegt_im_einschwingen(void)
{
    for (int i = 0; i < 2; i++) {
        otdr_track_t *t = spur_wie_panel(&g_spur[i], -1);
        uft_splice_result_t sp;
        memset(&sp, 0, sizeof sp);
        PRUEFE(uft_deepread_detect_splice(t, &sp) == 0, "splice rc");
        PRUEFE(sp.splice_bitcell < 64,
               "Spur %d: groesste Stufe in den ersten 64 Zellen (Einschwingen), war %u",
               i ? SPUR_FEHLER77 : SPUR_GUT34, sp.splice_bitcell);
        otdr_track_free(t);
    }
}

/* ── E3 ───────────────────────────────────────────────────────────── */
static otdr_disk_t *platte_aus_umdrehung(int rev)
{
    otdr_disk_t *d = otdr_disk_create(1, 2);
    otdr_config_t cfg;
    otdr_config_defaults(&cfg);
    for (int i = 0; i < 2; i++) {
        const uft_scp_rev_data_t *v = &g_spur[i].revolutions[rev];
        otdr_track_load_flux(&d->tracks[i], v->flux_data, v->flux_count, 0);
        otdr_track_analyze(&d->tracks[i], &cfg);
    }
    otdr_disk_compute_stats(d);
    return d;
}

static void t_fingerabdruck_zweier_umdrehungen(void)
{
    otdr_disk_t *a = platte_aus_umdrehung(0), *b = platte_aus_umdrehung(1);
    uft_media_fingerprint_t fa, fb;
    PRUEFE(uft_deepread_fingerprint(a, &fa) == 0 && uft_deepread_fingerprint(b, &fb) == 0,
           "fingerprint rc");
    float cos = uft_deepread_fingerprint_compare(&fa, &fb);
    PRUEFE(cos > 0.9999f, "zwei Umdrehungen derselben Lesung: Kosinus > 0,9999, war %.6f", cos);
    PRUEFE(strcmp(fa.hash_hex, fb.hash_hex) != 0,
           "und trotzdem zwei Kennungen (F1 an echtem Jitter): %s / %s", fa.hash_hex, fb.hash_hex);
    otdr_disk_free(a);
    otdr_disk_free(b);
}

int main(void)
{
    printf("DeepRead an echter Aufnahme (P3-630, MF-1471)\n");
    if (!laden()) {
        printf("  FEHLER: %s nicht lesbar (corpus_free ist versioniert)\n", ECHT_SCP);
        return 1;
    }
    t_zustand_ist_belegt();
    t_alterungsrest_trifft_die_gesunde_spur();
    t_naht_liegt_im_einschwingen();
    t_fingerabdruck_zweier_umdrehungen();
    uft_scp_free_track(&g_spur[0]);
    uft_scp_free_track(&g_spur[1]);
    printf("%d Pruefungen, %d Fehler\n", geprueft, fehler);
    return fehler ? 1 : 0;
}

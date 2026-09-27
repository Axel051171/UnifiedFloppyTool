/* SPDX-License-Identifier: MIT */
/**
 * @file test_deepread_naht.c
 * @brief Was der DeepRead-Schreibnaht-Erkenner misst (MF-1435)
 *
 * ── DAS MODELL, UND WOHER ES KOMMT ───────────────────────────────────
 *
 * Die Spur entsteht aus `uft_mfm_encode_track()` (IBM System 34, DD,
 * 9 x 512 Byte) — einer ZWEITEN Hand gegenueber dem Erkenner, wie in
 * `test_schreibnaht_mfm.c` (MF-1190). Aus den MFM-Zellen werden
 * Flussintervalle zu 2000 ns je Zelle.
 *
 * Eine Schreibnaht ist hier ein PHASENSPRUNG: der Schreibtakt des neuen
 * Schreibvorgangs ist nicht auf den alten phasenverriegelt, also ist das
 * Intervall ueber der Naht kein ganzes Vielfaches der Zelle. Das ist eine
 * Modellannahme, keine Aufnahme — der freie Korpus hat keine echte
 * Flussaufnahme mit Naht und Jitter (P3-630). Gemessen wird also, was der
 * Erkenner an einer BEKANNTEN Naht tut, nicht, wie echte Nahte aussehen.
 *
 * Das Rauschen ist ein fester xorshift-Generator (Gauss nach Box-Muller),
 * damit jeder Lauf dieselben Zahlen sieht.
 *
 * ── DIE BEFUNDE (gemessen) ───────────────────────────────────────────
 *
 *   W1  Die Lage stimmt in einem Fenster: bei Jitter bis 60 ns (3 % der
 *       Zelle) und Phasensprung 0,25–0,4 Zellen liegt die Spitze
 *       gemessen +-4 Zellen neben der Naht. Ab 100 ns liegt sie im
 *       Rauschen.
 *
 *   W2  `splice_stability` war unbrauchbar: dieselbe Naht auf zwei
 *       Umdrehungen ergab bei 20 ns Jitter **5942,5** — gerechnet als
 *       Streuung von FLUSS-Indizes des groessten Rohintervall-Sprungs,
 *       also des ersten 2T->4T-Wechsels, nicht der Naht. Seit MF-1435
 *       laeuft jede Umdrehung durch DIESELBE Messung wie die Spur, und
 *       die Streuung ist in Zellen. Das ist die eigentliche
 *       Unterscheidung: eine Naht sitzt auf dem Traeger und kehrt jede
 *       Umdrehung wieder, Rauschen nicht.
 *
 *   W3  `detected` (Sprung > 3 dB) ist KEIN Befund: ohne jede Naht meldet
 *       es gemessen ab 60 ns Jitter 1 — die Schwelle liegt unter dem
 *       Rauschmaximum einer Spur. Festgehalten, nicht geaendert; das
 *       OTDR-Panel zeigt das Feld nicht.
 *
 *   W4  `index_offset_ns` ist `splice_ns - revolution_ns`, also der
 *       NEGATIVE Abstand bis zum NAECHSTEN Indexpuls. Festgehalten; das
 *       Panel zeigt `splice_ns` (Abstand NACH dem Index).
 */

#include "uft/uft_types.h"
#include "uft/uft_mfm_encoder.h"
#include "uft/analysis/floppy_otdr.h"
#include "uft/analysis/uft_deepread_splice.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPT    9
#define SECSZ  512
#define ZELLE  2000.0
#define NAHT   40000u          /* Zelle, an der der neue Schreibvorgang beginnt */
#define MAXFL  120000u

static int fehler = 0, geprueft = 0;
#define PRUEFE(bed, ...) do { geprueft++; if (!(bed)) { fehler++;       \
        printf("  FEHLER %s:%d: ", __FILE__, __LINE__);                  \
        printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint8_t  g_zellen[16384];
static uint32_t g_nbits;
static uint64_t g_rng;

static double gleich(void)
{
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17;
    return (double)(g_rng >> 11) / 9007199254740992.0;
}
static double gauss(void)
{
    double u = gleich() + 1e-12, v = gleich();
    return sqrt(-2.0 * log(u)) * cos(6.283185307179586 * v);
}

static void spur_bauen(void)
{
    static uint8_t pl[SPT][SECSZ];
    static uft_sector_t s[SPT];
    for (int k = 0; k < SPT; k++)
        for (int i = 0; i < SECSZ; i++)
            pl[k][i] = (uint8_t)(k * 11 + i * 7 + (i >> 4));
    memset(s, 0, sizeof s);
    for (int k = 0; k < SPT; k++) {
        s[k].id.cylinder = 3; s[k].id.sector = (uint8_t)(k + 1);
        s[k].id.size_code = 2; s[k].data = pl[k];
        s[k].data_len = SECSZ; s[k].data_size = SECSZ;
    }
    uft_mfm_encode_params_t p = UFT_MFM_PARAMS_DEFAULT_DD;
    g_nbits = (uint32_t)uft_mfm_encode_track(s, SPT, 3, 0, &p,
                                             g_zellen, sizeof g_zellen) * 8u;
}

/* Zellen -> Intervalle; `phi` Zellen Phasensprung auf dem Intervall, das
 * die Zelle `naht` ueberquert (naht == 0: keine Naht). */
static uint32_t zu_fluss(uint32_t *out, uint32_t naht, double phi, double sigma)
{
    uint32_t n = 0, zuletzt = 0;
    for (uint32_t b = 1; b < g_nbits && n < MAXFL; b++) {
        if (!((g_zellen[b >> 3] >> (7 - (b & 7))) & 1)) continue;
        double len = (double)(b - zuletzt) * ZELLE;
        if (naht && zuletzt < naht && b >= naht) len += phi * ZELLE;
        len += sigma * gauss();
        out[n++] = (uint32_t)(len + 0.5);
        zuletzt = b;
    }
    return n;
}

/* Eine Spur mit `revs` Umdrehungen, jede mit eigenem Rauschen. */
static otdr_track_t *spur(uint32_t naht, double phi, double sigma, int revs,
                          uint64_t saat)
{
    static uint32_t f[MAXFL];
    otdr_track_t *t = (otdr_track_t *)calloc(1, sizeof *t);
    g_rng = saat;
    for (int r = 0; r < revs; r++) {
        uint32_t n = zu_fluss(f, naht, phi, sigma);
        otdr_track_load_flux(t, f, n, (uint8_t)r);
    }
    otdr_config_t cfg;
    otdr_config_defaults(&cfg);
    otdr_track_analyze(t, &cfg);
    return t;
}

static void t_lage_im_fenster(void)
{
    const double sig[] = { 0.0, 20.0, 40.0 };
    for (int i = 0; i < 3; i++) {
        otdr_track_t *t = spur(NAHT, 0.4, sig[i], 1, 88172645463325252ull + (uint64_t)i);
        uft_splice_result_t r;
        PRUEFE(uft_deepread_detect_splice(t, &r) == 0, "rc");
        int d = (int)r.splice_bitcell - (int)NAHT;
        PRUEFE(abs(d) <= 8, "W1 sigma %.0f: Spitze %u, Naht %u", sig[i], r.splice_bitcell, NAHT);
        /* W4: splice_ns ist die Lage nach dem Index, geschlossen 40000 x 2 us */
        PRUEFE(fabs((double)r.splice_ns - NAHT * ZELLE) < 50.0 * ZELLE,
               "splice_ns %u, erwartet ~%.0f", r.splice_ns, NAHT * ZELLE);
        PRUEFE(r.index_offset_ns == (int32_t)r.splice_ns - (int32_t)t->revolution_ns,
               "W4 festgehalten: index_offset_ns = splice_ns - revolution_ns");
        otdr_track_free(t);
    }
}

static void t_stabilitaet_trennt_naht_von_rauschen(void)
{
    /* Dieselbe Naht auf zwei Umdrehungen, eigenes Rauschen je Umdrehung */
    otdr_track_t *t = spur(NAHT, 0.4, 20.0, 2, 0x9E3779B97F4A7C15ull);
    uft_splice_result_t r;
    PRUEFE(uft_deepread_detect_splice(t, &r) == 0, "rc");
    PRUEFE(r.splice_stability <= 8.0f,
           "W2: dieselbe Naht auf 2 Umdrehungen streut <= 8 Zellen, war %.1f",
           r.splice_stability);
    otdr_track_free(t);

    /* Ohne Naht, 60 ns: die Spitze ist Rauschen und springt je Umdrehung */
    t = spur(0, 0.0, 60.0, 2, 0xD1B54A32D192ED03ull);
    PRUEFE(uft_deepread_detect_splice(t, &r) == 0, "rc");
    PRUEFE(r.splice_stability > 1000.0f,
           "W2: ohne Naht springt die Spitze (> 1000 Zellen), war %.1f",
           r.splice_stability);
    otdr_track_free(t);
}

static void t_detected_ist_kein_befund(void)
{
    otdr_track_t *t = spur(0, 0.0, 100.0, 1, 0x2545F4914F6CDD1Dull);
    uft_splice_result_t r;
    PRUEFE(uft_deepread_detect_splice(t, &r) == 0, "rc");
    PRUEFE(r.detected,
           "W3 festgehalten: ohne Naht meldet `detected` bei 100 ns Jitter 1 "
           "(Sprung %.2f dB > 3). Wer die Schwelle belegt, zieht P3-630 nach.",
           r.splice_magnitude);
    otdr_track_free(t);

    t = spur(0, 0.0, 0.0, 1, 1);
    PRUEFE(uft_deepread_detect_splice(t, &r) == 0 && !r.detected &&
           r.splice_magnitude < 1e-3f,
           "ohne Naht und Jitter: kein Sprung, war %.3f", r.splice_magnitude);
    otdr_track_free(t);
}

int main(void)
{
    printf("DeepRead-Schreibnaht (MF-1435)\n");
    spur_bauen();
    PRUEFE(g_nbits > NAHT + 1000u, "Spur zu kurz: %u Zellen", g_nbits);
    t_lage_im_fenster();
    t_stabilitaet_trennt_naht_von_rauschen();
    t_detected_ist_kein_befund();
    printf("%d Pruefungen, %d Fehler\n", geprueft, fehler);
    return fehler ? 1 : 0;
}

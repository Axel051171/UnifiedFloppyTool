/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_flux_splice_pos.c
 * @brief Woher der Schreibstartpunkt kommt — und was „kein Sync nach dem
 *        Gap" wirklich heißt (MF-769).
 *
 * ── Der Befund ───────────────────────────────────────────────────────────
 *
 * X-Copys Code 3 heißt „no sync after gap found". Gemessen durch den
 * echten Dekoder fielen **vier** Lagen auf dasselbe Verdikt:
 *
 *     (a) 2 Umdrehungen, Sync in beiden        Schutz / $4489
 *     (b) 2 Umdrehungen, Sync nur in Umdr. 1   Schutz / $4489
 *     (c) nur 1 Umdrehung aufgenommen          Schutz / $4489
 *     (d) Sync direkt hinter dem Index         Schutz / $4489
 *
 * Besonders (b) gegen (c): das eine ist ein Befund über die **Diskette**,
 * das andere über die **Aufnahme**. Wer sie zusammenwirft, schickt einen
 * Archivar los, den Fehler am falschen Ort zu suchen.
 *
 * ── Was P6 verlangt ──────────────────────────────────────────────────────
 *
 * „Der Startpunkt muss aus der Analyse der aufgenommenen Spur stammen,
 * nicht aus einer festen Annahme über den Indexbezug" — und es muss einen
 * Ausweichfall geben, wenn der erste Sync unmittelbar hinter dem Index
 * liegt. Dort fällt der Schreibstart sonst in den Aufsetzbereich des
 * Kopfs; das erwartete Fehlerbild ist ein beschädigter erster Sektor.
 *
 * Fall (d) prüft genau das: kleiner Abstand zum Index, und ein **zweiter
 * Kandidat** ist da. Eine Schwelle steht bewusst nirgends — sie hängt am
 * Laufwerk, und dieses Projekt hat keine Hardware zum Messen (MF-310).
 *
 * ── Die Folge-Aussage ────────────────────────────────────────────────────
 *
 * `UFT_SPLICE_FEHLT` setzt die Folge auf „nicht reproduzierbar". Das ist
 * **nicht** aus dem Handbuch abgeleitet — X-Copy führt Code 3 als
 * gewöhnlichen Lesefehler. Es folgt aus P6: ergibt die Analyse keinen
 * Startpunkt, hat ein bitgenaues Rückschreiben keinen sicheren Anfang.
 *
 * `UNBEKANNT` bleibt bewusst folgenlos. Fall (c) friert das ein: eine zu
 * kurze Aufnahme darf die Diskette nicht schlechter aussehen lassen.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/flux/uft_flux_decoder.h"

static int fehler = 0;

enum { ZELLE = 2000, CAP = 60000 };
static uint32_t st = 31337u;
static uint32_t rnd(void) { st ^= st << 13; st ^= st >> 17; st ^= st << 5; return st; }

static const char *lage_name(uft_splice_lage_t l)
{
    switch (l) {
    case UFT_SPLICE_GEFUNDEN:  return "GEFUNDEN";
    case UFT_SPLICE_FEHLT:     return "FEHLT (Code 3)";
    default:                   return "UNBEKANNT";
    }
}

/* Ein 16-Bit-Wort als Zellabstände. Bitlage ABSOLUT — siehe die
 * Fixture-Falle in test_flux_schutzmarke.c. */
static size_t wort(uint32_t *iv, size_t k, uint16_t w, long *bitpos, long *last)
{
    for (int b = 15; b >= 0; b--) {
        long pos = *bitpos + (15 - b);
        if (w & (1u << b)) {
            if (*last >= 0) iv[k++] = (uint32_t)(pos - *last) * ZELLE;
            *last = pos;
        }
    }
    *bitpos += 16;
    return k;
}

static size_t umdrehung(uint32_t *iv, size_t k, int mit_marke,
                        size_t vor, size_t nach, long *bitpos, long *last)
{
    for (size_t i = 0; i < vor; i++) {
        iv[k++] = (2 + (rnd() % 3)) * ZELLE; *bitpos += 4; *last = -1;
    }
    if (mit_marke) {
        *last = -1;
        for (int r = 0; r < 8; r++) k = wort(iv, k, MFM_SYNC_PATTERN, bitpos, last);
    }
    for (size_t i = 0; i < nach; i++) {
        iv[k++] = (2 + (rnd() % 3)) * ZELLE; *bitpos += 4; *last = -1;
    }
    return k;
}

/* Rohspur mit AUSDRUECKLICH gesetzten Indeximpulsen bauen. Die
 * Hilfsfunktion `flux_raw_from_ns_intervals_indexed()` setzt sie nur,
 * wenn die Umdrehungsdauer eine ±50-%-Prüfung besteht (MF-475) — für
 * einen Test über Umdrehungsgrenzen ist das eine Abhängigkeit zu viel. */
static int roh_bauen(const uint32_t *iv, size_t n, size_t bis_index2,
                     flux_raw_data_t *out)
{
    memset(out, 0, sizeof(*out));
    uint32_t *tr = (uint32_t *)malloc((n + 1) * sizeof(uint32_t));
    if (!tr) return 0;

    uint64_t cum = 0;
    uint64_t idx2 = 0;
    tr[0] = 0;
    for (size_t i = 0; i < n; i++) {
        cum += iv[i];
        tr[i + 1] = (uint32_t)cum;
        if (i + 1 == bis_index2) idx2 = cum;
    }
    out->transitions      = tr;
    out->transition_count = n + 1;
    out->sample_rate      = 1000000000u;    /* 1 GHz: ein Tick ist eine ns */

    if (bis_index2 > 0) {
        uint32_t *ix = (uint32_t *)malloc(2 * sizeof(uint32_t));
        if (!ix) { free(tr); return 0; }
        ix[0] = 0;
        ix[1] = (uint32_t)idx2;
        out->index_times = ix;
        out->index_count = 2;
    }
    return 1;
}

static void pruefe(const char *titel, const uint32_t *iv, size_t n,
                   size_t bis_index2, uft_splice_lage_t soll_lage,
                   uft_track_folge_t soll_folge, int alt_erwartet)
{
    flux_raw_data_t raw;
    if (!roh_bauen(iv, n, bis_index2, &raw)) { fehler++; return; }

    flux_decoded_track_t trk;
    flux_decoder_options_t opts;
    memset(&trk, 0, sizeof(trk));
    memset(&opts, 0, sizeof(opts));
    flux_decode_mfm(&raw, &trk, &opts);

    const uft_track_verdikt_t *v = &trk.verdikt;
    int ok = (v->splice_lage == soll_lage) && (v->folge == soll_folge);
    if (alt_erwartet >= 0)
        ok = ok && ((v->splice_alt_ns != 0) == (alt_erwartet != 0));
    if (!ok) fehler++;

    printf("  %s %-38s %-14s Abstand %8u ns  Ausweich %s\n",
           ok ? "ok  " : "FAIL", titel, lage_name(v->splice_lage),
           v->splice_abstand_ns, v->splice_alt_ns ? "ja" : "nein");
    if (!ok)
        printf("       erwartet: %s / %s / Ausweich %s\n",
               lage_name(soll_lage), uft_track_folge_name(soll_folge),
               alt_erwartet > 0 ? "ja" : "nein");

    free(raw.transitions);
    free(raw.index_times);
}

int main(void)
{
    uint32_t *iv = (uint32_t *)malloc(CAP * sizeof(uint32_t));
    if (!iv) return 2;
    long bp, last;
    size_t k, grenze;

    printf("test_flux_splice_pos (MF-769)\n");

    /* (a) Zwei Umdrehungen, Sync in beiden — der Normalfall. */
    bp = 0; last = -1; k = 0;
    k = umdrehung(iv, k, 1, 2000, 6000, &bp, &last);
    grenze = k;
    k = umdrehung(iv, k, 1, 2000, 6000, &bp, &last);
    pruefe("(a) Sync in beiden Umdrehungen", iv, k, grenze,
           UFT_SPLICE_GEFUNDEN, UFT_FOLGE_REPRODUZIERBAR, 1);

    /* (b) Sync NUR in Umdrehung 1 — das ist Code 3. */
    bp = 0; last = -1; k = 0;
    k = umdrehung(iv, k, 1, 2000, 6000, &bp, &last);
    grenze = k;
    k = umdrehung(iv, k, 0, 2000, 6000, &bp, &last);
    pruefe("(b) kein Sync nach dem Gap = Code 3", iv, k, grenze,
           UFT_SPLICE_FEHLT, UFT_FOLGE_NICHT_REPRODUZIERBAR, 0);

    /* (c) Nur EINE Umdrehung — Aussage ueber die Aufnahme, nicht ueber
     *     die Diskette. Die Folge MUSS unberuehrt bleiben. */
    bp = 0; last = -1; k = 0;
    k = umdrehung(iv, k, 1, 2000, 6000, &bp, &last);
    pruefe("(c) nur 1 Umdrehung -> unbekannt", iv, k, 0,
           UFT_SPLICE_UNBEKANNT, UFT_FOLGE_REPRODUZIERBAR, 0);

    /* (d) P6: Sync unmittelbar hinter dem Index. Der Startpunkt ist da,
     *     aber knapp — und ein Ausweichkandidat MUSS vorliegen. */
    bp = 0; last = -1; k = 0;
    k = umdrehung(iv, k, 1, 2000, 6000, &bp, &last);
    grenze = k;
    k = umdrehung(iv, k, 1, 2, 6000, &bp, &last);
    pruefe("(d) Sync direkt hinter dem Index", iv, k, grenze,
           UFT_SPLICE_GEFUNDEN, UFT_FOLGE_REPRODUZIERBAR, 1);

    free(iv);
    printf("%s (%d Fehler)\n", fehler ? "FAIL" : "PASS", fehler);
    return fehler ? 1 : 0;
}

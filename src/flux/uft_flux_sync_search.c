/**
 * @file uft_flux_sync_search.c
 * @brief Sync-Marken im Flussstrom finden, ohne die PLL (MF-492).
 *
 * Verfahren, Herleitung und Grenzen stehen im Header.
 */

#include "uft/flux/uft_flux_sync_search.h"

#include <string.h>

/* Eine Quelle, zwei Darstellungen — wie im Histogramm-Modul (MF-488).
 * `flux_raw_data_t` haelt KUMULIERTE Zeiten, die Erzeuger liefern
 * INTERVALLE. Statt die Suchschleife zweimal zu schreiben, holt sie ihre
 * Werte hierueber. */
typedef struct {
    const uint32_t *v;
    size_t          n;
    bool            cumulative;
} sync_src_t;

static uint32_t src_get(const sync_src_t *s, size_t i)
{
    if (!s->cumulative) return s->v[i];
    if (i == 0) return s->v[0];
    return (s->v[i] > s->v[i - 1]) ? (s->v[i] - s->v[i - 1]) : 0u;
}

bool uft_sync_pattern_from_words(const uint16_t *words, size_t n_words,
                                 uft_sync_pattern_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!words || n_words == 0) return false;

    /* Zellen des Sync auslegen, MSB zuerst, und die Abstaende der gesetzten
     * Zellen sammeln. Der erste Flusswechsel hat keinen Vorgaenger — von ihm
     * gibt es also eine Position, aber keinen Abstand. */
    long long prev = -1;
    for (size_t w = 0; w < n_words; w++) {
        for (int b = 15; b >= 0; b--) {
            if (!((words[w] >> b) & 1)) continue;
            long long pos = (long long)(w * 16u + (size_t)(15 - b));
            if (prev >= 0) {
                long long d = pos - prev;
                if (d <= 0 || d > 255) return false;      /* nicht darstellbar */
                if (out->k_count >= UFT_SYNC_MAX_CELLS) return false;
                out->k[out->k_count++] = (uint8_t)d;
            }
            prev = pos;
        }
    }

    /* Unter drei Abstaenden traegt das Muster zu wenig Form: es passt dann
     * auf so viele Stellen, dass die Suche zum Zufallsgenerator wird. */
    return out->k_count >= 3;
}

/**
 * Taktabgleich an einer Fundstelle.
 *
 * Entscheidend ist nicht, dass die Abstaende irgendwie passen, sondern dass
 * sie zu EINEM gemeinsamen Takt passen: d_i ~ k_i * T. T wird als Verhaeltnis
 * der Summen geschaetzt — die Kleinste-Quadrate-Loesung fuer diesen Ansatz,
 * ohne Iteration.
 *
 * Eine 0 im Strom ist eine SCP-Ueberlaufmarke und kein Flusswechsel
 * (MF-438). Sie braucht hier keinen eigenen Zweig: der Abgleich verwirft
 * sie ohnehin, denn |0 - k*T| / T = k >= 1 liegt ueber jeder brauchbaren
 * Toleranz. Ein zusaetzlicher Test waere ein Zweig, den kein Fall erreicht.
 */
static bool clock_fit(const sync_src_t *src, size_t at,
                      const uft_sync_pattern_t *pat, double tolerance,
                      double *out_clock, double *out_err)
{
    double sum_d = 0.0;
    unsigned sum_k = 0;
    for (size_t i = 0; i < pat->k_count; i++) {
        sum_d += (double)src_get(src, at + i);
        sum_k += pat->k[i];
    }
    if (sum_k == 0 || sum_d <= 0.0) return false;

    const double T = sum_d / (double)sum_k;
    if (!(T > 0.0)) return false;

    double worst = 0.0;
    for (size_t i = 0; i < pat->k_count; i++) {
        double diff = (double)src_get(src, at + i) - (double)pat->k[i] * T;
        if (diff < 0) diff = -diff;
        double rel = diff / T;                 /* in Zellen gemessen */
        if (rel > tolerance) return false;
        if (rel > worst) worst = rel;
    }

    if (out_clock) *out_clock = T;
    if (out_err)   *out_err = worst;
    return true;
}

static size_t sync_search(const sync_src_t *src, const uft_sync_pattern_t *pat,
                          double tolerance,
                          uft_sync_hit_t *hits, size_t max_hits)
{
    if (!src->v || !pat || !hits || max_hits == 0) return 0;
    if (pat->k_count < 3 || pat->k_count > UFT_SYNC_MAX_CELLS) return 0;
    if (src->n < pat->k_count) return 0;
    if (tolerance <= 0.0) tolerance = UFT_SYNC_DEFAULT_TOL;

    size_t found = 0;
    const size_t last = src->n - pat->k_count;

    for (size_t at = 0; at <= last && found < max_hits; at++) {
        double clk = 0.0, err = 0.0;
        if (!clock_fit(src, at, pat, tolerance, &clk, &err)) continue;

        hits[found].index       = at;
        hits[found].local_clock = clk;
        hits[found].fit_error   = err;
        found++;

        /* Ueber die gefundene Marke hinweg weitersuchen. Sonst meldet der
         * naechste Durchlauf dieselbe Stelle um eins verschoben noch einmal —
         * ein Zaehler, der Kandidaten zaehlt statt Marken (dieselbe Falle wie
         * MF-454 im Amiga-Decoder). */
        at += pat->k_count - 1;
    }
    return found;
}

double uft_sync_median_clock(const uft_sync_hit_t *hits, size_t n)
{
    if (!hits || n == 0) return 0.0;

    /* Einfuegesortieren auf einer festen Kopie: n ist durch die Groesse des
     * Fundfeldes begrenzt, und die Eingabe bleibt unveraendert — der
     * Aufrufer haelt seine Funde in Fundreihenfolge, nicht in Taktordnung. */
    double v[UFT_SYNC_MAX_CELLS];
    if (n > UFT_SYNC_MAX_CELLS) n = UFT_SYNC_MAX_CELLS;
    for (size_t i = 0; i < n; i++) v[i] = hits[i].local_clock;
    for (size_t i = 1; i < n; i++) {
        double x = v[i];
        size_t j = i;
        while (j > 0 && v[j - 1] > x) { v[j] = v[j - 1]; j--; }
        v[j] = x;
    }
    return (n & 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

size_t uft_sync_search_intervals(const uint32_t *deltas, size_t n,
                                 const uft_sync_pattern_t *pat,
                                 double tolerance,
                                 uft_sync_hit_t *hits, size_t max_hits)
{
    sync_src_t s = { deltas, n, false };
    return sync_search(&s, pat, tolerance, hits, max_hits);
}

size_t uft_sync_search_transitions(const uint32_t *transitions, size_t n,
                                   const uft_sync_pattern_t *pat,
                                   double tolerance,
                                   uft_sync_hit_t *hits, size_t max_hits)
{
    sync_src_t s = { transitions, n, true };
    return sync_search(&s, pat, tolerance, hits, max_hits);
}

/**
 * @file uft_dewarp.c
 * @brief Gleichlauffehler aus dem Flussstrom herausrechnen (MF-495).
 *
 * Verfahren, gemessene Grenzen und die Warnung vor dem Speichern des
 * Ergebnisses stehen im Header.
 */

#include "uft/flux/uft_dewarp.h"

#include <stdlib.h>
#include <string.h>

/** MFM traegt Abstaende von 2, 3 oder 4 Zellen. Ausserhalb liegt kein
 *  gueltiger Abstand, sondern ein Fehler — und ein Fehler darf den
 *  Taktschaetzer nicht mitziehen. */
enum { DEWARP_K_MIN = 2, DEWARP_K_MAX = 4 };

/**
 * Sofort-Takt aus einem Intervall.
 *
 * @return 0, wenn das Intervall keine brauchbare Aussage traegt: eine 0
 *         ist eine SCP-Ueberlaufmarke und kein Flusswechsel (MF-438), und
 *         ein Abstand weit ausserhalb von 2…4 Zellen ist ein Aussetzer.
 *         Beides wird uebersprungen statt eingerechnet.
 *
 * ── Warum diese Abweisung bleibt, obwohl sie einen Sektor kostet ─────────
 *
 * Gemessen, beide Fassungen gegeneinander:
 *
 *                          mit Abweisung      ohne
 *   sauber, Spanne              1,000        1,038
 *   zwei Tempi, heile Sektoren      9           10
 *
 * Ohne die Abweisung wackelt der Schaetzer auch auf einer voellig
 * gleichmaessigen Spur — ein 2T-Abstand direkt nach einem 4T schiebt die
 * Glaettung hin und her — und meldet 3,8 % Gleichlauffehler, wo keiner ist.
 * Dieser Wert ist zweierlei: eine Anzeige fuer den Menschen („das Medium
 * eiert um X %") und die Schwelle, ab der die Stufe ueberhaupt laeuft. Ein
 * erfundener Diagnosewert waere teurer als ein Sektor, und die Stufe liefe
 * dann auf JEDER gesunden Spur mit.
 */
static double inst_clock(uint32_t d, double t)
{
    if (d == 0 || t <= 0.0) return 0.0;
    double k = (double)d / t;
    if (k < (double)DEWARP_K_MIN - 0.75) return 0.0;
    if (k > (double)DEWARP_K_MAX + 0.75) return 0.0;
    long ki = (long)(k + 0.5);
    if (ki < DEWARP_K_MIN) ki = DEWARP_K_MIN;
    if (ki > DEWARP_K_MAX) ki = DEWARP_K_MAX;
    return (double)d / (double)ki;
}

bool uft_dewarp_intervals(const uint32_t *in, size_t n, double t0,
                          double alpha, uint32_t *out,
                          uft_dewarp_result_t *res)
{
    if (res) memset(res, 0, sizeof(*res));
    if (!in || !out || n < 2 || !(t0 > 0.0)) return false;
    if (alpha <= 0.0) alpha = UFT_DEWARP_DEFAULT_ALPHA;
    if (alpha > 1.0) alpha = 1.0;

    double *curve = (double *)malloc(n * sizeof(double));
    if (!curve) return false;

    /* Vorwaerts. */
    double t = t0;
    size_t used = 0;
    for (size_t i = 0; i < n; i++) {
        double c = inst_clock(in[i], t);
        if (c > 0.0) { t = (1.0 - alpha) * t + alpha * c; used++; }
        curve[i] = t;
    }
    if (used == 0) { free(curve); return false; }

    /* Rueckwaerts, und gleich mit dem Vorwaertswert gemittelt. Zwei
     * Richtungen, weil eine einseitige Glaettung genau an den Sprungstellen
     * am staerksten hinterherlaeuft. */
    t = curve[n - 1];
    double lo = 0.0, hi = 0.0, sum = 0.0;
    for (size_t i = n; i-- > 0; ) {
        double c = inst_clock(in[i], t);
        if (c > 0.0) t = (1.0 - alpha) * t + alpha * c;
        double avg = 0.5 * (curve[i] + t);
        curve[i] = avg;
        sum += avg;
        if (i == n - 1) { lo = hi = avg; }
        else {
            if (avg < lo) lo = avg;
            if (avg > hi) hi = avg;
        }
    }

    const double ref = sum / (double)n;
    if (!(ref > 0.0)) { free(curve); return false; }

    /* Eine 0 im Strom ist eine SCP-Ueberlaufmarke und kein Flusswechsel
     * (MF-438). Sie braucht hier keinen eigenen Zweig: 0 mal irgendetwas
     * bleibt 0, die Marke ueberlebt die Skalierung von selbst. Ein
     * zusaetzlicher Test waere ein Zweig, den kein Fall entscheidet — der
     * Rotbeweis dazu kippt nichts. */
    for (size_t i = 0; i < n; i++) {
        double scale = (curve[i] > 0.0) ? (ref / curve[i]) : 1.0;
        double v = (double)in[i] * scale + 0.5;
        if (v < 0.0) v = 0.0;
        if (v > (double)UINT32_MAX) v = (double)UINT32_MAX;
        out[i] = (uint32_t)v;
    }

    if (res) {
        res->ref_clock = ref;
        res->warp_span = (lo > 0.0) ? (hi / lo) : 1.0;
    }
    free(curve);
    return true;
}

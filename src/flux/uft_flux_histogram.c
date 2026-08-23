/**
 * @file uft_flux_histogram.c
 * @brief Zellendauer aus dem Intervall-Histogramm (MF-488).
 *
 * Verfahren, Herleitung und die Grenzen stehen im Header.
 */

#include "uft/flux/uft_flux_histogram.h"

#include <string.h>

/* Fachbreite. 50 ns ist fein genug, um 4000/6000/8000 ns sauber zu trennen
 * (40 Faecher Abstand), und grob genug, dass ein Berg nicht in Rauschen
 * zerfaellt. */
#define DEFAULT_BIN_NS   50u

/* Groesster Abstand, der noch ins Histogramm faellt. Darueber liegen
 * Ueberlaeufe und Luecken, keine Zellenvielfachen. */
#define MAX_NS           40000u

/* Feinste zugelassene Fachbreite: 25 ns, die native Aufloesung von SCP.
 * Feiner waere Scheingenauigkeit, und die Grenze deckelt zugleich das Feld
 * unten auf 1600 Faecher = 6,4 KB. Das ist als lokale Variable vertretbar
 * und vermeidet ein `static` — der Schaetzer wird aus Decodern gerufen, und
 * ein gemeinsamer Puffer waere nicht wiedereintrittsfaehig. */
#define MIN_BIN_NS       25u
#define MAX_BINS         (MAX_NS / MIN_BIN_NS)

/* Ein Berg zaehlt erst ab diesem Anteil des hoechsten. FloppyControl nimmt
 * dieselbe Groessenordnung; entscheidend ist, dass ein Nebenmaximum im
 * Rauschen nicht als dritter Berg durchgeht. */
#define PEAK_MIN_FRACTION  20u               /* 1/20 = 5 % */

/* Mindestabstand zweier Berge, in Nanosekunden. Bei 2 us Zellendauer liegen
 * die Berge 2000 ns auseinander; 500 ns trennt sie sicher und laesst einen
 * verbreiterten Berg nicht in zwei zerfallen. */
#define PEAK_MIN_DIST_NS   500u

/* MFM-Erkennung: Berg2/Berg1 muss nahe 1,5 liegen (2T -> 3T). Das Fenster
 * ist grosszuegig, weil ein verzitterter Strom die Schwerpunkte verschiebt —
 * aber eng genug, dass ein GCR- oder Rauschhistogramm durchfaellt. */
#define MFM_RATIO_LO   1.30
#define MFM_RATIO_HI   1.70

/* Das eigentliche Guetemass (MF-488).
 *
 * Das Berg-Verhaeltnis allein reicht NICHT: gleichverteiltes Rauschen
 * erzeugt zufaellige Nebenmaxima, die 1,5 ebenfalls treffen. Der erste
 * Anlauf dieses Moduls hat genau daran einen Rauschstrom als MFM erkannt.
 *
 * Physikalisch unterscheidbar sind die beiden ueber die VERTEILUNG: ein
 * echter MFM-Strom hat fast seine ganze Masse in den drei schmalen Baendern
 * um 2T, 3T und 4T; Rauschen verteilt sie ueber den ganzen Bereich. Ein
 * Fenster von +-8 % je Band deckt bei echtem MFM ueber 95 % ab, bei
 * gleichverteiltem Rauschen ueber 1-10 us nur rund ein Drittel. */
#define BAND_TOLERANCE  0.08
#define MIN_COVERAGE    0.70

/* Eine Quelle, zwei Darstellungen.
 *
 * `flux_raw_data_t` haelt KUMULIERTE Zeiten, die Erzeuger liefern
 * INTERVALLE — dieselbe Verwechslung hat MF-438 einmal gekostet. Statt die
 * Histogrammschleife zweimal zu schreiben (und damit die naechste
 * Auseinanderlauf-Stelle zu bauen), holt sie ihre Werte hierueber. */
typedef struct {
    const uint32_t *v;
    size_t          n;
    bool            cumulative;
} hist_src_t;

static uint32_t src_get(const hist_src_t *s, size_t i)
{
    if (!s->cumulative) return s->v[i];
    if (i == 0) return s->v[0];
    return (s->v[i] > s->v[i - 1]) ? (s->v[i] - s->v[i - 1]) : 0u;
}

static bool hist_analyze(const hist_src_t *src, uint32_t bin_ns,
                         uft_flux_hist_result_t *out);

bool uft_flux_histogram_analyze(const uint32_t *intervals, size_t count,
                                uint32_t bin_ns,
                                uft_flux_hist_result_t *out)
{
    hist_src_t s = { intervals, count, false };
    return hist_analyze(&s, bin_ns, out);
}

bool uft_flux_histogram_analyze_transitions(const uint32_t *transitions,
                                            size_t count, uint32_t bin_ns,
                                            uft_flux_hist_result_t *out)
{
    hist_src_t s = { transitions, count, true };
    return hist_analyze(&s, bin_ns, out);
}

static bool hist_analyze(const hist_src_t *src, uint32_t bin_ns,
                         uft_flux_hist_result_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!src->v || src->n == 0) return false;
    const size_t count = src->n;

    if (bin_ns == 0)         bin_ns = DEFAULT_BIN_NS;
    if (bin_ns < MIN_BIN_NS) bin_ns = MIN_BIN_NS;

    const size_t nbins = MAX_NS / bin_ns + 1u;
    if (nbins > MAX_BINS + 1u) return false;

    uint32_t bins[MAX_BINS + 2];
    memset(bins, 0, (nbins + 1) * sizeof(bins[0]));

    uint32_t max_count = 0;
    size_t   used = 0;
    for (size_t i = 0; i < count; i++) {
        uint32_t v = src_get(src, i);
        if (v == 0 || v > MAX_NS) continue;   /* 0 = Ueberlaufmarke (MF-438) */
        size_t b = v / bin_ns;
        bins[b]++;
        if (bins[b] > max_count) max_count = bins[b];
        used++;
    }
    if (used == 0 || max_count == 0) return false;

    const uint32_t threshold = max_count / PEAK_MIN_FRACTION;
    const size_t   min_dist  = PEAK_MIN_DIST_NS / bin_ns;

    /* Berge suchen: lokales Maximum ueber zwei Nachbarn je Seite, ueber der
     * Schwelle, und weit genug vom vorigen entfernt. Der Schwerpunkt wird
     * ueber fuenf Faecher gewichtet — ein Berg ist breiter als ein Fach, und
     * seine Lage steckt in der Verteilung, nicht im hoechsten Fach. */
    for (size_t i = 2; i + 2 < nbins && out->peak_count < UFT_FLUX_HIST_MAX_PEAKS;
         i++) {
        uint32_t v = bins[i];
        if (v < threshold) continue;
        if (!(v >= bins[i - 1] && v >= bins[i + 1] &&
              v >= bins[i - 2] && v >= bins[i + 2]))
            continue;

        if (out->peak_count > 0) {
            double prev = out->peaks[out->peak_count - 1].center_ns;
            if ((double)i * bin_ns - prev < (double)min_dist * bin_ns)
                continue;
        }

        double wsum = 0.0;
        double wtot = 0.0;
        for (size_t j = i - 2; j <= i + 2; j++) {
            wsum += (double)j * (double)bins[j];
            wtot += (double)bins[j];
        }
        double center_bin = (wtot > 0.0) ? (wsum / wtot) : (double)i;

        out->peaks[out->peak_count].center_ns = center_bin * (double)bin_ns;
        out->peaks[out->peak_count].count     = v;
        out->peak_count++;
    }

    /* Zellendauer schaetzen — aber nur, wenn das Histogramm wie MFM
     * aussieht. Der erste Berg liegt bei 2T. */
    if (out->peak_count >= 2 && out->peaks[0].center_ns > 0.0) {
        out->ratio_2_1 = out->peaks[1].center_ns / out->peaks[0].center_ns;
        if (out->ratio_2_1 >= MFM_RATIO_LO && out->ratio_2_1 <= MFM_RATIO_HI) {
            double cell = out->peaks[0].center_ns / 2.0;

            /* Gegenprobe an der Verteilung: liegt die Masse wirklich in den
             * drei Baendern? Ohne diese Frage haelt der Schaetzer Rauschen
             * fuer MFM. */
            size_t in_band = 0;
            for (size_t i = 0; i < count; i++) {
                uint32_t v = src_get(src, i);
                if (v == 0 || v > MAX_NS) continue;
                for (int k = 2; k <= 4; k++) {
                    double centre = cell * (double)k;
                    double d = (double)v - centre;
                    if (d < 0) d = -d;
                    if (d <= centre * BAND_TOLERANCE) { in_band++; break; }
                }
            }
            out->coverage = (used > 0) ? ((double)in_band / (double)used) : 0.0;

            if (cell > 0.0 && out->coverage >= MIN_COVERAGE) {
                out->cell_ns   = cell;
                out->confident = true;
            }
        }
    }
    return true;
}

static bool cell_ns_from(const hist_src_t *src, double *out_cell_ns)
{
    if (!out_cell_ns) return false;

    uft_flux_hist_result_t r;
    if (!hist_analyze(src, 0, &r)) return false;
    if (!r.confident) return false;

    *out_cell_ns = r.cell_ns;
    return true;
}

bool uft_flux_histogram_cell_ns(const uint32_t *intervals, size_t count,
                                double *out_cell_ns)
{
    hist_src_t s = { intervals, count, false };
    return cell_ns_from(&s, out_cell_ns);
}

bool uft_flux_histogram_cell_ns_from_transitions(const uint32_t *transitions,
                                                 size_t count,
                                                 double *out_cell_ns)
{
    hist_src_t s = { transitions, count, true };
    return cell_ns_from(&s, out_cell_ns);
}

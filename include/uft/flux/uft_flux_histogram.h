/**
 * @file uft_flux_histogram.h
 * @brief Zellendauer aus dem Intervall-Histogramm des Flusses (MF-488).
 *
 * Baustein A des FloppyControl-Umsetzungsplans, in der Fassung, die ohne
 * Bedienoberflaeche auskommt.
 *
 * ── Das Verfahren ────────────────────────────────────────────────────────
 *
 * Ein MFM-Datenstrom mit Zellendauer T traegt Abstaende von 2T, 3T und 4T —
 * mehr gibt die Kodierung nicht her. Im Histogramm der Abstaende sind das
 * drei Berge im Verhaeltnis 1 : 1,5 : 2. Der erste liegt bei 2T, also ist
 * T die halbe Lage des ERSTEN Bergs.
 *
 * Das ist keine neue Erkenntnis, es stand schon zweimal im Baum — und lief
 * beide Male nicht:
 *
 *   - `UftFluxHistogramWidget::detectPeaks()` + `detectEncoding()` rechnen
 *     es und behalten es im Widget; `m_peaks` und `m_cellTime` sind privat
 *     und haben ausserhalb der Zeichenroutine keinen Abnehmer.
 *   - `src/flux/fdc_bitstream/fdc_bitstream.cpp:511-513` tut dasselbe, und
 *     der Aufruf ist **auskommentiert**:
 *         //  std::vector<size_t> peaks = fdc_misc::find_peaks(dist_freq);
 *         //  m_codec.set_vfo_cell_size(peaks[0]/2.f);
 *
 * Achte Auspraegung derselben Diagnose in dieser Woche: ein Fakt mehrfach
 * implementiert, und es laeuft keine der Fassungen.
 *
 * ── Warum es eine Schaetzung mit Guetemass ist ───────────────────────────
 *
 * Der Schaetzer sagt nur dann eine Zellendauer, wenn das Histogramm
 * tatsaechlich wie MFM aussieht: mindestens zwei deutliche Berge, deren
 * Verhaeltnis nahe 1,5 liegt. Sieht es anders aus — Rauschen, GCR,
 * beschaedigter Strom —, meldet er `false` und der Aufrufer bleibt bei dem,
 * was er vorher hatte. Eine Zahl zu liefern, die nicht gemessen ist, waere
 * genau der Fehler, den dieses Projekt sonst dokumentiert.
 */
#ifndef UFT_FLUX_HISTOGRAM_H
#define UFT_FLUX_HISTOGRAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Hoechstzahl der Berge, die zurueckgegeben werden. */
#define UFT_FLUX_HIST_MAX_PEAKS  8

/** Ein Berg im Abstandshistogramm. */
typedef struct {
    double   center_ns;   /**< Schwerpunkt in Nanosekunden           */
    uint32_t count;       /**< Treffer im Gipfelfach                 */
} uft_flux_peak_t;

/** Was der Schaetzer aus einem Flussstrom gelesen hat. */
typedef struct {
    uft_flux_peak_t peaks[UFT_FLUX_HIST_MAX_PEAKS];
    size_t          peak_count;

    /** Geschaetzte Zellendauer, nur gueltig wenn @ref confident. */
    double          cell_ns;

    /** Verhaeltnis Berg2/Berg1. Bei MFM nahe 1,5. 0 bei < 2 Bergen. */
    double          ratio_2_1;

    /** Anteil der Abstaende, die nahe 2T, 3T oder 4T liegen (0..1).
     *
     * Das eigentliche Guetemass. Ein Berg-Verhaeltnis von 1,5 allein genuegt
     * NICHT: gleichverteiltes Rauschen erzeugt zufaellige Nebenmaxima, die es
     * ebenfalls treffen — das hat der erste Anlauf dieses Moduls schmerzhaft
     * gezeigt. Ein echter MFM-Strom hat dagegen fast seine ganze Masse in
     * den drei Baendern; Rauschen verteilt sie ueber den ganzen Bereich. */
    double          coverage;

    /** true, wenn das Histogramm wie ein MFM-Strom aussieht und
     *  @ref cell_ns benutzt werden darf. */
    bool            confident;
} uft_flux_hist_result_t;

/**
 * @brief Abstands-Histogramm bilden, Berge suchen, Zellendauer schaetzen.
 *
 * @param intervals   Abstaende zwischen Flusswechseln, in Nanosekunden.
 *                    Das sind INTERVALLE, keine kumulierten Zeiten — die
 *                    Verwechslung hat MF-438 einmal gekostet.
 * @param count       Anzahl der Abstaende
 * @param bin_ns      Fachbreite in Nanosekunden; 0 waehlt 50 ns
 * @param out         Ergebnis; bei @c false unveraendert bis auf Nullung
 * @return true, wenn ein Histogramm gebildet werden konnte (nicht: dass die
 *         Schaetzung brauchbar ist — dafuer @ref uft_flux_hist_result_t
 *         ::confident pruefen)
 */
bool uft_flux_histogram_analyze(const uint32_t *intervals, size_t count,
                                uint32_t bin_ns,
                                uft_flux_hist_result_t *out);

/**
 * @brief Bequemlichkeit: nur die Zellendauer, nur wenn sie belegt ist.
 *
 * @return true und setzt @p out_cell_ns, wenn der Strom wie MFM aussieht;
 *         sonst false und @p out_cell_ns bleibt unberuehrt.
 */
bool uft_flux_histogram_cell_ns(const uint32_t *intervals, size_t count,
                                double *out_cell_ns);

/** Wie oben, aber fuer KUMULIERTE Uebergangszeiten — die Darstellung in
 *  `flux_raw_data_t`. Die Abstaende werden im Durchlauf gebildet, das
 *  Histogramm ist dasselbe. */
bool uft_flux_histogram_analyze_transitions(const uint32_t *transitions,
                                            size_t count, uint32_t bin_ns,
                                            uft_flux_hist_result_t *out);

bool uft_flux_histogram_cell_ns_from_transitions(const uint32_t *transitions,
                                                 size_t count,
                                                 double *out_cell_ns);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_HISTOGRAM_H */

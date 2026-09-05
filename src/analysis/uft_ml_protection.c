/**
 * @file uft_ml_protection.c
 * @brief ML-based Copy Protection Classifier — Implementation
 *
 * Feature-based classifier that identifies floppy disk copy protection
 * schemes using cosine similarity against known reference signatures.
 *
 * Feature vector (8 dimensions per track):
 *   [0] histogram_entropy    — Shannon entropy of flux timing histogram
 *   [1] track_length_ratio   — actual/nominal (>1.02 = long track)
 *   [2] sync_pattern_score   — custom sync frequency, normalised 0-1
 *   [3] bad_gcr_ratio        — bad GCR bytes / total, normalised 0-1
 *   [4] duplicate_id_count   — normalised 0-1
 *   [5] jitter_rms           — normalised 0-1
 *   [6] half_track_flag      — 0.0 or 1.0
 *   [7] custom_sync_flag     — 0.0 or 1.0
 *
 * -- MF-882: der Signaturvergleich ist zurueckgenommen ----------------
 *
 * Bis MF-882 verglich dieses Modul den Merkmalsvektor per Cosinus gegen
 * 15 handgesetzte Signaturen und meldete ab 0.70 einen PRODUKTNAMEN mit
 * Prozentzahl. Gemessen am uebersetzten Modul, drei unabhaengige
 * Abtastungen:
 *
 *   Eingaberaum, den die GUI erzeugen kann :    441 Punkte, 0 freigesprochen
 *   Raster ueber den ganzen 8D-Raum (3^8)  :   6561 Punkte, 0 freigesprochen
 *   Zufallsabtastung ueber den 8D-Raum     : 500000 Punkte, 0 freigesprochen
 *
 * Der Zweig „No copy protection detected" war fuer JEDE Eingabe
 * unerreichbar. Kleinste je erreichte beste Aehnlichkeit: 0.5134 — die
 * Schwelle war 0.70, und was darunter fiel, fing der `weirdness`-Zweig.
 * Eine mustergueltig SAUBERE Diskette (Nennlaenge, kein Sync, kein
 * schlechtes GCR, kaum Jitter) wurde mit 99.1 % als „Long Track Generic"
 * benannt; ein LEERER Vektor — also „nichts gemessen" — als „Unknown
 * protection scheme suspected (weirdness: 50.0%)", weil eine ungemessene
 * Null als maximale Abweichung vom Nennwert gelesen wurde.
 *
 * Ursache: alle 15 Signaturen trugen an Stelle [1] (Laengenverhaeltnis)
 * einen Wert um 1.0 — die groesste Komponente jedes Vektors. Der Cosinus
 * ist skaleninvariant und mass danach im Wesentlichen die gemeinsame
 * Grosskomponente gegen sich selbst.
 *
 * Erschwerend war der einzige Aufrufer: `src/gui/uft_otdr_panel.cpp`
 * uebergab SECHS der acht Merkmale als Festwerte (1.0f, 0, 0, 0, false,
 * false). Nur Entropie und Jitter stammten aus Messdaten — der Vergleich
 * lief gegen die eigenen Platzhalter, und das Ergebnis wurde fett rot
 * gefaerbt.
 *
 * Die Tabelle wurde NICHT nachgebessert, sondern entfernt. Sie hatte
 * keine Quelle: kein Spezifikationsverweis, kein Korpus, kein
 * Trainingslauf. Schwellen oder Geometrie zu justieren hiesse, einen
 * zweiten Satz unbelegter Zahlen ueber den ersten zu legen — die Klasse,
 * an der dieser Baum fuenfmal verbrannt ist (FMT-2/3/10/11/12).
 *
 * Vorbild im selben Baum, dreimal: `uft_protection_extended.c:522`
 * („NOT IMPLEMENTED — returns an error, not 'not detected'"),
 * `uft_speedlock.c` und `uft_c64_protection_enhanced.c`
 * („A missing implementation must not look like a measurement.").
 *
 * Was bleibt: `uft_ml_extract_features()`. Sie normiert nur und
 * behauptet nichts — Entropie, Verhaeltnisse, Flaggen. Wer die
 * Klassifikation zurueckholen will, braucht zuerst eine BELEGTE
 * Signaturtabelle und eine Geometrie, die beide Antworten geben kann;
 * `tests/test_ml_schutz_kann_nicht_freisprechen.c` haelt beides fest.
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#include "uft/analysis/uft_ml_protection.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>


/* ===================================================================
 * Internal helpers
 * =================================================================== */

/**
 * Compute Shannon entropy of a histogram.
 */
static float shannon_entropy(const float *hist, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        if (hist[i] > 0.0f)
            sum += (double)hist[i];
    }
    if (sum <= 0.0)
        return 0.0f;

    double entropy = 0.0;
    for (int i = 0; i < n; i++) {
        double p = (double)hist[i] / sum;
        if (p > 1e-12)
            entropy -= p * log(p);
    }

    /* Normalise to [0, 1] using max possible entropy for 256 bins */
    double max_entropy = log(256.0);
    if (max_entropy > 0.0)
        entropy /= max_entropy;

    return (float)entropy;
}


/**
 * Clamp a float value to [min, max].
 */
static float clampf(float value, float lo, float hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}


/* ===================================================================
 * Public API
 * =================================================================== */

int uft_ml_extract_features(const float *histogram,
                             float track_length_ratio,
                             int sync_count,
                             int bad_gcr_count,
                             int duplicate_ids,
                             float jitter_rms,
                             bool has_half_track,
                             bool has_custom_sync,
                             float features_out[UFT_ML_PROT_FEATURES])
{
    if (!features_out)
        return -1;

    memset(features_out, 0, sizeof(float) * UFT_ML_PROT_FEATURES);

    /* [0] Histogram entropy (normalised 0-1) */
    if (histogram)
        features_out[0] = shannon_entropy(histogram, 256);
    else
        features_out[0] = 0.0f;

    /* [1] Track length ratio (keep as-is, typical range 0.8 - 1.3) */
    features_out[1] = track_length_ratio;

    /* [2] Sync pattern score: normalise sync_count to 0-1 range.
     *     Typical custom-sync tracks have 10-50 patterns; cap at 100. */
    features_out[2] = clampf((float)sync_count / 100.0f, 0.0f, 1.0f);

    /* [3] Bad GCR ratio: normalise against a typical track of ~7000 bytes.
     *     More than 500 bad GCR bytes = 1.0. */
    features_out[3] = clampf((float)bad_gcr_count / 500.0f, 0.0f, 1.0f);

    /* [4] Duplicate sector IDs: normalise against max ~20 sectors/track */
    features_out[4] = clampf((float)duplicate_ids / 20.0f, 0.0f, 1.0f);

    /* [5] Jitter RMS: normalise against 10 us (very high jitter) */
    features_out[5] = clampf(jitter_rms / 10.0f, 0.0f, 1.0f);

    /* [6] Half-track flag */
    features_out[6] = has_half_track ? 1.0f : 0.0f;

    /* [7] Custom sync flag */
    features_out[7] = has_custom_sync ? 1.0f : 0.0f;

    return 0;
}

int uft_ml_detect_protection(const float (*track_features)[UFT_ML_PROT_FEATURES],
                              int n_tracks,
                              uft_ml_prot_result_t *result)
{
    if (!track_features || n_tracks < 1 || !result)
        return -1;

    memset(result, 0, sizeof(*result));

    /* MF-882: Dieses Modul spricht kein Urteil mehr aus.
     *
     * Der frueher hier stehende Signaturvergleich konnte gemessen KEINE
     * Diskette freisprechen — 0 von 441 (GUI-erreichbar), 0 von 6561
     * (Raster ueber den ganzen Merkmalsraum) und 0 von 500000
     * (Zufallsabtastung). Ein Erkenner, der nur „ja" sagen kann, ist
     * keiner; die Begruendung steht ausfuehrlich im Dateikopf.
     *
     * Der Rueckgabewert ist bewusst NICHT 0. Ein `0` mit
     * `is_protected == false` waere ein stiller FREISPRUCH — derselbe
     * Fehler in die andere Richtung, und genau die Form, die
     * `docs/DESIGN_PRINCIPLES.md` „nicht gefunden" gegen „nicht geprueft"
     * abgrenzt.
     *
     * Die Merkmale werden bewusst NICHT ausgewertet: jede Aussage ueber
     * sie waere wieder eine aus unbelegten Schwellen. */
    result->is_protected        = false;
    result->count               = 0;
    result->unknown_probability = 0.0f;
    snprintf(result->summary, sizeof(result->summary),
             "Nicht geprueft: der Signaturvergleich ist zurueckgenommen "
             "(MF-882). Er konnte keine Diskette freisprechen - gemessen "
             "0 von 500000 Merkmalsvektoren -, und seine 15 Signaturen "
             "haben keine benannte Quelle. Dies ist KEIN Freispruch.");

    return UFT_ML_PROT_NICHT_GEPRUEFT;
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_multi_rev_fusion.c
 * @brief Weak-Bit-Erkennung durch Umdrehungsvergleich
 *
 * Vertrag, Prinzip und Herkunft stehen im Header
 * `include/uft/algorithms/uft_multi_rev_fusion.h`.
 *
 * ── Was MF-949 geaendert hat ─────────────────────────────────────────
 *
 * Das Modul gab es schon; gemessen hatte es **null** Aufrufer und
 * keinen Prototyp, obwohl es in `UnifiedFloppyTool.pro:791` steht und
 * uebersetzt wird. Der Rotbeweis `tests/test_multi_rev_fusion.c` fand
 * beim ersten Lauf drei Dinge:
 *
 *   1. **Eine einzelne Umdrehung wurde beantwortet** — mit Konfidenz
 *      1,0 fuer jedes Bit und „null schwache Bits". Das ist genau die
 *      Aussage, die aus einer Lesung nicht ableitbar ist, und damit ein
 *      Verstoss gegen Prinzip 1. Sie wird jetzt abgelehnt.
 *
 *   2. **Alle Umdrehungen mussten gleich lang sein.** Die Schnittstelle
 *      nahm EINE Bitzahl fuer alle und las bei einer kuerzeren ueber den
 *      Rand. Drehzahlschwankung ist der Normalfall; dafuer gibt es jetzt
 *      `uft_fuse_revolutions_laengen()`.
 *
 *   3. **Ein Widerspruch von 1 zu 4 fiel durch die Schwelle.** Bei fuenf
 *      Umdrehungen ergibt 4:1 die Konfidenz 0,8, und der Vergleich
 *      lautet `< 0.8` — also kein Befund. Der seltene Widerspruch ist
 *      aber das Signal. Die Zahlen lagen bereits in
 *      `vote_ones`/`vote_zeros` und wurden nur nicht ausgewertet; jetzt
 *      steht die Tatsache als `revolutions_disagreed` neben dem Urteil
 *      `weak_bit`.
 *
 * Die Bezeichner tragen seit MF-949 das `uft_`-Praefix.
 */
#include "uft/algorithms/uft_multi_rev_fusion.h"

#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 * Vorgabe
 * ========================================================================== */

static const uft_fusion_config_t UFT_FUSION_CONFIG_DEFAULT = {
    .weak_threshold      = 0.8f,
    .strong_threshold    = 0.95f,
    .weight_by_timing    = false,
    .revolution_weights  = NULL
};

/** Bit @p pos aus @p buf, hoechstwertiges Bit zuerst je Byte. */
static inline uint8_t bit_lesen(const uint8_t *buf, size_t pos)
{
    return (uint8_t)((buf[pos / 8] >> (7 - (pos % 8))) & 1u);
}

/* ==========================================================================
 * Kern
 * ========================================================================== */

/**
 * Gemeinsamer Rumpf. @p bits_per_rev ist bereits auf die kuerzeste
 * Umdrehung begrenzt.
 */
static bool fusion_rechnen(const uint8_t **revolutions,
                           size_t num_revolutions, size_t bits_per_rev,
                           const uft_fusion_config_t *config,
                           uft_fused_bitstream_t *result)
{
    result->bits = (uft_fused_bit_t *)calloc(bits_per_rev,
                                             sizeof(uft_fused_bit_t));
    if (!result->bits) return false;
    result->bit_count = bits_per_rev;

    float  gesamt_konfidenz = 0.0f;
    size_t schwach = 0;
    size_t uneinig = 0;

    for (size_t i = 0; i < bits_per_rev; i++) {
        uft_fused_bit_t *fb = &result->bits[i];
        float einsen = 0.0f, nullen = 0.0f, gewicht_summe = 0.0f;

        for (size_t r = 0; r < num_revolutions; r++) {
            float gewicht = 1.0f;
            if (config->weight_by_timing && config->revolution_weights)
                gewicht = config->revolution_weights[r];

            if (bit_lesen(revolutions[r], i)) {
                einsen += gewicht;
                fb->vote_ones++;
            } else {
                nullen += gewicht;
                fb->vote_zeros++;
            }
            gewicht_summe += gewicht;
        }

        fb->value = (einsen > nullen) ? 1u : 0u;

        /* Ein Gewichtssatz aus lauter Nullen ist kein Ergebnis, sondern
         * eine Division durch null. Dann zaehlt nur die Tatsache. */
        if (gewicht_summe > 0.0f) {
            const float mehrheit = (einsen > nullen) ? einsen : nullen;
            fb->confidence = mehrheit / gewicht_summe;
        } else {
            fb->confidence = 0.0f;
        }

        /* TATSACHE: haben die Umdrehungen dasselbe gelesen? Ungewichtet —
         * ein Gewicht aendert nichts daran, WAS gelesen wurde. */
        fb->revolutions_disagreed =
            (fb->vote_ones > 0u && fb->vote_zeros > 0u);
        if (fb->revolutions_disagreed) uneinig++;

        /* URTEIL: wie sicher ist der Mehrheitswert? */
        fb->weak_bit = (fb->confidence < config->weak_threshold);
        if (fb->weak_bit) schwach++;

        gesamt_konfidenz += fb->confidence;
    }

    result->overall_confidence = gesamt_konfidenz / (float)bits_per_rev;
    result->weak_bit_count     = schwach;
    result->disagreement_count = uneinig;
    result->total_votes        = num_revolutions * bits_per_rev;
    result->avg_agreement      = result->overall_confidence;
    return true;
}

bool uft_fuse_revolutions(const uint8_t **revolutions,
                          size_t num_revolutions, size_t bits_per_rev,
                          const uft_fusion_config_t *config,
                          uft_fused_bitstream_t *result)
{
    if (!revolutions || !result || bits_per_rev == 0) return false;

    /* MF-949: EINE Umdrehung wird abgelehnt, nicht beantwortet.
     *
     * Die Frage „ist dieses Bit schwach" ist aus einer einzelnen Lesung
     * nicht beantwortbar — das ist der ganze Inhalt des Prinzips. Bis
     * MF-949 kam hier `true` heraus, Konfidenz 1,0 fuer jedes Bit und
     * `weak_bit_count = 0`: eine Antwort, die niemand geben konnte.
     * Dieselbe Absage trifft `uft_bitstream_recovery.c:136`. */
    if (num_revolutions < 2) return false;

    for (size_t r = 0; r < num_revolutions; r++)
        if (!revolutions[r]) return false;

    if (!config) config = &UFT_FUSION_CONFIG_DEFAULT;
    return fusion_rechnen(revolutions, num_revolutions, bits_per_rev,
                          config, result);
}

bool uft_fuse_revolutions_laengen(const uint8_t **revolutions,
                                  const size_t *bit_counts,
                                  size_t num_revolutions,
                                  const uft_fusion_config_t *config,
                                  uft_fused_bitstream_t *result)
{
    if (!revolutions || !bit_counts || !result) return false;
    if (num_revolutions < 2) return false;

    /* Verglichen wird nur, was ALLE Umdrehungen tragen. Alles darueber
     * hinaus waere ein Vergleich gegen nicht vorhandene Daten. */
    size_t gemeinsam = bit_counts[0];
    for (size_t r = 0; r < num_revolutions; r++) {
        if (!revolutions[r]) return false;
        if (bit_counts[r] < gemeinsam) gemeinsam = bit_counts[r];
    }
    if (gemeinsam == 0) return false;

    if (!config) config = &UFT_FUSION_CONFIG_DEFAULT;
    return fusion_rechnen(revolutions, num_revolutions, gemeinsam,
                          config, result);
}

/* ==========================================================================
 * Ausrichtung (MF-950)
 *
 * Die Begruendung und die Messzahlen stehen im Header. Kurz: ohne
 * Ausrichtung meldete der Vergleich auf einer echten gw-Aufnahme 99,71 %
 * der Spur als schwach, obwohl beide Umdrehungen dasselbe Signal tragen
 * und nur um EIN Bit verschoben sind.
 * ========================================================================== */

/** Zaehlt abweichende Bits zwischen a[i] und b[i+versatz] im Fenster. */
static size_t abweichung_zaehlen(const uint8_t *a, size_t a_bits,
                                 const uint8_t *b, size_t b_bits,
                                 long versatz, size_t von, size_t bis,
                                 size_t *out_verglichen)
{
    size_t ab = 0, n = 0;
    for (size_t i = von; i < bis && i < a_bits; i++) {
        const long j = (long)i + versatz;
        if (j < 0 || (size_t)j >= b_bits) continue;
        if (bit_lesen(a, i) != bit_lesen(b, (size_t)j)) ab++;
        n++;
    }
    if (out_verglichen) *out_verglichen = n;
    return ab;
}

bool uft_revolutionen_ausrichten(const uint8_t *a, size_t a_bits,
                                 const uint8_t *b, size_t b_bits,
                                 long max_versatz,
                                 uft_rev_ausrichtung_t *out)
{
    if (!a || !b || !out || a_bits == 0 || b_bits == 0) return false;
    if (max_versatz < 0) max_versatz = -max_versatz;

    memset(out, 0, sizeof(*out));
    out->abweichung = 1.0;

    /* Verglichen wird die GANZE Ueberlappung, nur um `max_versatz` an
     * beiden Enden eingerueckt — damit jeder Versatz dieselbe Menge
     * Bits sieht und kein Rand ihn bevorzugt.
     *
     * MF-953: hier stand ein Fenster aus der MITTE (12,5 % bis 62,5 %),
     * begruendet mit Aufwand und mit der Vermutung, die Raender fuehrten
     * in die Irre. Beide Haelften der Begruendung sind gemessen worden,
     * und beide tragen nicht:
     *
     * AUFWAND: 4,8 ms mit Fenster, 9,6 ms ohne, bei 101 343 Bits und
     * 129 Versaetzen. Bei 80 Spuren ist das unter einer Sekunde.
     *
     * RICHTIGKEIT: das Fenster war nicht bloss unbelegt, es war
     * SCHAEDLICH. Liegt die unterscheidende Stelle einer Spur dahinter,
     * sieht die Ausrichtung nur gleichfoermiges Muster — und dort passen
     * viele Versaetze gleich gut. Gemessen an einer Spur, die vorne eine
     * periodische Luecke traegt und ihren Inhalt hinten:
     *
     *     Inhalt in der Mitte        Versatz +2  richtig
     *     Inhalt im letzten Viertel  Versatz +0  FALSCH
     *     Inhalt im letzten Achtel   Versatz +0  FALSCH
     *
     * Und zwar mit Abweichung 0,0000, also als VERLAESSLICH gemeldet.
     * Ein zuversichtlich falscher Versatz geht in die Fusion und erzeugt
     * genau die erfundenen Befunde, gegen die MF-950 gebaut wurde.
     *
     * Das ist realistisch und nicht konstruiert: Luecken sind periodisch
     * und machen den groessten Teil einer Spur aus.
     *
     * Warum die Mutationsprobe gruen blieb: kein Pruefmuster hatte die
     * unterscheidende Stelle ausserhalb des Fensters — sie waren
     * gleichmaessig zufaellig, also ueberall unterscheidend. */
    const size_t kurz = (a_bits < b_bits) ? a_bits : b_bits;
    if (kurz <= (size_t)(2 * max_versatz) + 16u) return false;

    const size_t von = (size_t)max_versatz;
    const size_t bis = kurz - (size_t)max_versatz;
    if (von >= bis) return false;

    long   bester = 0;
    double beste  = 1.0;
    size_t beste_n = 0;

    for (long d = -max_versatz; d <= max_versatz; d++) {
        size_t n = 0;
        const size_t ab = abweichung_zaehlen(a, a_bits, b, b_bits, d,
                                             von, bis, &n);
        if (n == 0) continue;
        const double q = (double)ab / (double)n;
        /* Bei Gleichstand gewinnt der kleinere Betrag: ein Versatz von 0
         * ist die sparsamere Erklaerung als einer von 12. */
        if (q < beste || (q == beste && labs(d) < labs(bester))) {
            beste = q; bester = d; beste_n = n;
        }
    }

    if (beste_n == 0) return false;

    out->versatz      = bester;
    out->abweichung   = beste;
    out->verglichen   = beste_n;
    out->verlaesslich = (beste <= UFT_REV_AUSRICHTUNG_SCHWELLE);
    return true;
}

bool uft_fuse_revolutions_ausgerichtet(const uint8_t **revolutions,
                                       const size_t *bit_counts,
                                       size_t num_revolutions,
                                       long max_versatz,
                                       const uft_fusion_config_t *config,
                                       uft_fused_bitstream_t *result)
{
    if (!revolutions || !bit_counts || !result) return false;
    if (num_revolutions < 2) return false;
    for (size_t r = 0; r < num_revolutions; r++)
        if (!revolutions[r]) return false;

    long *versatz = (long *)calloc(num_revolutions, sizeof(long));
    if (!versatz) return false;

    /* Alles wird an Umdrehung 0 ausgerichtet. */
    for (size_t r = 1; r < num_revolutions; r++) {
        uft_rev_ausrichtung_t aus;
        if (!uft_revolutionen_ausrichten(revolutions[0], bit_counts[0],
                                         revolutions[r], bit_counts[r],
                                         max_versatz, &aus) ||
            !aus.verlaesslich) {
            /* Kein belegbarer Versatz. Weitermachen hiesse, auf einer
             * geratenen Ausrichtung zu vergleichen — und das erzeugt
             * Befunde, die es nicht gibt. */
            free(versatz);
            return false;
        }
        versatz[r] = aus.versatz;
    }

    /* Gemeinsamer Bereich nach der Ausrichtung. */
    long lo = 0;
    long hi = (long)bit_counts[0];
    for (size_t r = 0; r < num_revolutions; r++) {
        const long u = -versatz[r];
        const long o = (long)bit_counts[r] - versatz[r];
        if (u > lo) lo = u;
        if (o < hi) hi = o;
    }
    if (hi <= lo) { free(versatz); return false; }

    const size_t gemeinsam = (size_t)(hi - lo);

    /* Ausgerichtete Kopien anlegen. Das kostet Speicher, haelt aber den
     * Kern (`fusion_rechnen`) frei von Versatz-Arithmetik — und der Kern
     * ist der gepruefte Teil. */
    const size_t bytes = (gemeinsam + 7u) / 8u;
    uint8_t **kopie = (uint8_t **)calloc(num_revolutions, sizeof(uint8_t *));
    const uint8_t **zeiger =
        (const uint8_t **)calloc(num_revolutions, sizeof(uint8_t *));
    if (!kopie || !zeiger) {
        free(kopie); free((void *)zeiger); free(versatz);
        return false;
    }

    bool ok = true;
    for (size_t r = 0; r < num_revolutions && ok; r++) {
        kopie[r] = (uint8_t *)calloc(bytes, 1);
        if (!kopie[r]) { ok = false; break; }
        for (size_t i = 0; i < gemeinsam; i++) {
            const size_t q = (size_t)((long)i + lo + versatz[r]);
            if (bit_lesen(revolutions[r], q))
                kopie[r][i / 8] |= (uint8_t)(1u << (7 - (i % 8)));
        }
        zeiger[r] = kopie[r];
    }

    if (ok) {
        if (!config) config = &UFT_FUSION_CONFIG_DEFAULT;
        ok = fusion_rechnen(zeiger, num_revolutions, gemeinsam,
                            config, result);
    }

    for (size_t r = 0; r < num_revolutions; r++) free(kopie[r]);
    free(kopie); free((void *)zeiger); free(versatz);
    return ok;
}

/* ==========================================================================
 * Abfragen
 * ========================================================================== */

size_t uft_fused_to_bytes(const uft_fused_bitstream_t *fused,
                          uint8_t *output, size_t max_bytes)
{
    if (!fused || !fused->bits || !output || max_bytes == 0) return 0;

    size_t bytes = (fused->bit_count + 7u) / 8u;
    if (bytes > max_bytes) bytes = max_bytes;
    memset(output, 0, bytes);

    for (size_t i = 0; i < fused->bit_count && i < bytes * 8u; i++)
        if (fused->bits[i].value)
            output[i / 8] |= (uint8_t)(1u << (7 - (i % 8)));

    return bytes;
}

size_t uft_get_weak_bit_positions(const uft_fused_bitstream_t *fused,
                                  size_t *positions, size_t max_positions)
{
    if (!fused || !fused->bits || !positions) return 0;
    size_t n = 0;
    for (size_t i = 0; i < fused->bit_count && n < max_positions; i++)
        if (fused->bits[i].weak_bit) positions[n++] = i;
    return n;
}

size_t uft_get_disagreement_positions(const uft_fused_bitstream_t *fused,
                                      size_t *positions, size_t max_positions)
{
    if (!fused || !fused->bits || !positions) return 0;
    size_t n = 0;
    for (size_t i = 0; i < fused->bit_count && n < max_positions; i++)
        if (fused->bits[i].revolutions_disagreed) positions[n++] = i;
    return n;
}

void uft_fused_bitstream_free(uft_fused_bitstream_t *fused)
{
    if (!fused) return;
    free(fused->bits);
    fused->bits = NULL;
    fused->bit_count = 0;
}

/* ==========================================================================
 * Gewichtung aus Qualitaetsmerkmalen
 * ========================================================================== */

void uft_calculate_revolution_weights(const uft_revolution_quality_t *qualities,
                                      size_t num_revs, float *weights)
{
    if (!qualities || !weights || num_revs == 0) return;

    float summe = 0.0f;
    for (size_t r = 0; r < num_revs; r++) {
        weights[r] = qualities[r].pll_confidence * 0.3f +
                     qualities[r].sync_quality   * 0.3f +
                     qualities[r].crc_rate       * 0.4f;
        summe += weights[r];
    }

    if (summe > 0.0f) {
        for (size_t r = 0; r < num_revs; r++) {
            weights[r] /= summe;
            weights[r] *= (float)num_revs;   /* Summe = num_revs */
        }
    } else {
        /* Keine Qualitaetsaussage: gleich gewichten statt alles auf null
         * zu setzen — sonst waere die Gewichtssumme 0 und die Konfidenz
         * jedes Bits undefiniert. */
        for (size_t r = 0; r < num_revs; r++) weights[r] = 1.0f;
    }
}

/* ==========================================================================
 * Zusammenhaengende Bereiche
 * ========================================================================== */

size_t uft_find_weak_regions(const uft_fused_bitstream_t *fused,
                             uft_weak_region_t *regions, size_t max_regions)
{
    if (!fused || !fused->bits || !regions || max_regions == 0) return 0;

    size_t n = 0;
    size_t start = SIZE_MAX;

    for (size_t i = 0; i <= fused->bit_count && n < max_regions; i++) {
        const bool schwach = (i < fused->bit_count) && fused->bits[i].weak_bit;

        if (schwach && start == SIZE_MAX) {
            start = i;
        } else if (!schwach && start != SIZE_MAX) {
            uft_weak_region_t *r = &regions[n++];
            r->start_bit = start;
            r->end_bit   = i;
            r->length    = i - start;

            float summe = 0.0f;
            for (size_t j = start; j < i; j++)
                summe += fused->bits[j].confidence;
            r->avg_confidence = summe / (float)r->length;

            start = SIZE_MAX;
        }
    }
    return n;
}

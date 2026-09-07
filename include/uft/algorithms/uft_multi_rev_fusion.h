/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_multi_rev_fusion.h
 * @brief Weak-Bit-Erkennung durch Umdrehungsvergleich (MF-949)
 *
 * ── Das Prinzip ──────────────────────────────────────────────────────
 *
 * **Eine einzelne Lesung kann ein schwaches Bit nicht von einer
 * ungewoehnlichen, aber stabilen Kodierung unterscheiden.** Beide sehen
 * in einer Messung gleich aus. Erst der Vergleich mehrerer unabhaengiger
 * Lesungen derselben Stelle trennt sie: widersprechen sich die
 * Umdrehungen, ist die Flanke zu schwach, um zuverlaessig erkannt zu
 * werden; sind sie einig, ist die Kodierung nur ungewoehnlich.
 *
 * Historische Entsprechung: die „Deep Nibble"/APWM-Betriebsarten des
 * Cyclone-Dongles zu X-Copy Professional (Amiga) betrieben dafuer zwei
 * Laufwerke synchron. Der Kern war nie besseres Timing, sondern der
 * echte Mehrfachvergleich.
 *
 * ── Warum dieser Header erst mit MF-949 entstand ─────────────────────
 *
 * `src/algorithms/advanced/uft_multi_rev_fusion.c` gibt es seit
 * laengerem und steht in `UnifiedFloppyTool.pro:791`, wird also
 * uebersetzt. Gemessen hatten alle **sechs** exportierten Funktionen
 * ausserhalb ihrer eigenen Datei **null** Nennungen — keinen Aufrufer
 * und keinen Prototyp. Ungerufener Code ist ungepruefter Code; die
 * Messung dazu steht in `tests/test_multi_rev_fusion.c`.
 *
 * Die Namen tragen seit MF-949 das `uft_`-Praefix. Vorher hiessen sie
 * `fuse_revolutions`, `find_weak_regions` usw. — Bezeichner ohne
 * Praefix in einer Bibliothek mit 700 Quelldateien sind eine
 * Kollisionsfalle, und der Baum fuehrt 26 Header-Namenskollisionen als
 * offenen Punkt.
 *
 * ── Zwei verschiedene Fragen, zwei verschiedene Felder ───────────────
 *
 * `revolutions_disagreed` ist eine **Tatsache**: an dieser Bitposition
 * haben nicht alle Umdrehungen dasselbe gelesen.
 *
 * `weak_bit` ist ein **Urteil**: die Konfidenz des Mehrheitswerts liegt
 * unter der Schwelle (Vorgabe 0,8).
 *
 * Die beiden fallen NICHT zusammen, und das war vor MF-949 ein stiller
 * Verlust: bei fuenf Umdrehungen ergibt 4:1 die Konfidenz 0,8, und der
 * Vergleich lautet `<` — der Widerspruch wurde nicht gemeldet. Gerade
 * der seltene Widerspruch ist aber das Signal, nicht das Rauschen.
 */
#ifndef UFT_ALGORITHMS_MULTI_REV_FUSION_H
#define UFT_ALGORITHMS_MULTI_REV_FUSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Ergebnis fuer EINE Bitposition ueber alle Umdrehungen. */
typedef struct {
    uint8_t  value;                  /**< Mehrheitswert, 0 oder 1        */
    float    confidence;             /**< Anteil der Mehrheit, 0.0 – 1.0 */
    bool     weak_bit;               /**< URTEIL: Konfidenz < Schwelle   */
    bool     revolutions_disagreed;  /**< TATSACHE: nicht alle gleich    */
    uint32_t vote_ones;              /**< Umdrehungen mit 1              */
    uint32_t vote_zeros;             /**< Umdrehungen mit 0              */
} uft_fused_bit_t;

typedef struct {
    uft_fused_bit_t *bits;
    size_t           bit_count;      /**< so weit reichten ALLE Umdrehungen */

    float  overall_confidence;
    size_t weak_bit_count;           /**< nach dem Urteil                */
    size_t disagreement_count;       /**< nach der Tatsache              */
    size_t total_votes;
    float  avg_agreement;
} uft_fused_bitstream_t;

typedef struct {
    float  weak_threshold;           /**< darunter gilt „schwach", Vorgabe 0.8  */
    float  strong_threshold;         /**< darueber „hohe Konfidenz", Vorgabe 0.95 */
    bool   weight_by_timing;
    float *revolution_weights;       /**< optional, je Umdrehung         */
} uft_fusion_config_t;

typedef struct {
    float pll_confidence;
    float sync_quality;
    float crc_rate;
} uft_revolution_quality_t;

typedef struct {
    size_t start_bit;
    size_t end_bit;
    size_t length;
    float  avg_confidence;
} uft_weak_region_t;

/**
 * @brief Vergleicht dieselbe Bitposition ueber mehrere Umdrehungen.
 *
 * @param revolutions      je Umdrehung ein Bitpuffer, MSB zuerst je Byte
 * @param num_revolutions  **mindestens 2** — siehe unten
 * @param bits_per_rev     Bits, die JEDE Umdrehung mindestens traegt
 * @param config           NULL = Vorgabe
 * @param result           Ausgabe; der Aufrufer gibt sie mit
 *                         uft_fused_bitstream_free() frei
 *
 * @return true bei Erfolg, false bei Ablehnung.
 *
 * **Bei EINER Umdrehung wird abgelehnt, nicht geantwortet.** Die Frage
 * „ist dieses Bit schwach" ist aus einer Lesung nicht beantwortbar; ein
 * Ergebnis waere eine erfundene Aussage (Prinzip 1). Bis MF-949 kam
 * hier `true` heraus, mit Konfidenz 1,0 fuer jedes Bit und null
 * gemeldeten schwachen Bits. Dieselbe Absage trifft
 * `src/recovery/uft_bitstream_recovery.c:136` in derselben Lage.
 */
bool uft_fuse_revolutions(const uint8_t **revolutions,
                          size_t num_revolutions, size_t bits_per_rev,
                          const uft_fusion_config_t *config,
                          uft_fused_bitstream_t *result);

/**
 * @brief Wie uft_fuse_revolutions(), aber mit EIGENER Laenge je Umdrehung.
 *
 * Umdrehungen sind in der Wirklichkeit verschieden lang —
 * Drehzahlschwankung ist der Normalfall, nicht die Ausnahme. Verglichen
 * wird nur so weit, wie ALLE Umdrehungen reichen; wie weit das war,
 * steht danach in `result->bit_count`.
 *
 * @param bit_counts  je Umdrehung ihre Bitzahl, @p num_revolutions Eintraege
 */
bool uft_fuse_revolutions_laengen(const uint8_t **revolutions,
                                  const size_t *bit_counts,
                                  size_t num_revolutions,
                                  const uft_fusion_config_t *config,
                                  uft_fused_bitstream_t *result);

/* ── Ausrichtung: die Voraussetzung, die MF-949 noch nicht hatte ──────
 *
 * `uft_fuse_revolutions()` vergleicht STELLENWEISE. Das setzt voraus,
 * dass die Umdrehungen an derselben Bitposition beginnen — und genau das
 * tun sie in der Wirklichkeit nicht.
 *
 * GEMESSEN an `tests/corpus/gw_amigados.scp`, einer echten Aufnahme von
 * `gw` (Spur 0, zwei Umdrehungen):
 *
 *     Fluss-Rohdaten          50 525 von 50 526 Intervallen GLEICH
 *     stellenweise verglichen 101 343 Bits
 *     davon uneinig           101 051   = 99,71 %
 *     mittlere Konfidenz      0,5014
 *     bester Bitversatz  +1   0,00 % Abweichung ueber 20 000 Bits
 *
 * Die beiden Umdrehungen tragen DASSELBE Signal. Sie sind um ein
 * einziges Bit verschoben, weil der Index-Splice das erste Intervall
 * anders teilt (1975 statt 3950 ns). Der stellenweise Vergleich meldete
 * daraufhin 99,71 % der Spur als schwach.
 *
 * Das ist der gefaehrlichste Fehler dieser Klasse: still und gross. Er
 * sagt nichts Falsches ueber ein Bit, sondern etwas Falsches ueber die
 * ganze Diskette — und zwar zuversichtlich.
 *
 * Deshalb: erst ausrichten, dann vergleichen. Und wo sich kein Versatz
 * finden laesst, der die Stroeme zur Deckung bringt, wird ABGELEHNT
 * statt den am wenigsten schlechten zu nehmen.
 */

/** Ab dieser Abweichung gilt eine Ausrichtung als nicht belegt. */
#define UFT_REV_AUSRICHTUNG_SCHWELLE  0.10

typedef struct {
    long   versatz;       /**< Bitversatz von b gegen a (b[i+versatz] ~ a[i]) */
    double abweichung;    /**< Anteil abweichender Bits beim besten Versatz */
    size_t verglichen;    /**< wie viele Bits dabei verglichen wurden */
    bool   verlaesslich;  /**< abweichung <= UFT_REV_AUSRICHTUNG_SCHWELLE */
} uft_rev_ausrichtung_t;

/**
 * @brief Misst den Bitversatz zwischen zwei Umdrehungen.
 *
 * Verglichen wird ein Fenster aus der MITTE beider Stroeme — an den
 * Raendern steht bei einem Versatz Fuellung, kein Inhalt.
 *
 * @param max_versatz  wie weit in beide Richtungen gesucht wird
 * @return true, wenn eine Messung zustande kam. Das ist NICHT dasselbe
 *         wie ein Treffer: ob der gefundene Versatz die Stroeme zur
 *         Deckung bringt, sagt `out->verlaesslich`. Die gemessene
 *         Abweichung wird in jedem Fall berichtet — sie ist der Grund
 *         fuer das Urteil und gehoert dem Aufrufer.
 */
bool uft_revolutionen_ausrichten(const uint8_t *a, size_t a_bits,
                                 const uint8_t *b, size_t b_bits,
                                 long max_versatz,
                                 uft_rev_ausrichtung_t *out);

/**
 * @brief Richtet alle Umdrehungen an der ersten aus und fusioniert dann.
 *
 * Laesst sich fuer eine Umdrehung kein verlaesslicher Versatz finden,
 * wird ABGELEHNT — ein Vergleich auf geratener Ausrichtung erzeugt
 * Befunde, die es nicht gibt (siehe die Messung oben).
 *
 * Verglichen wird der Bereich, den nach der Ausrichtung ALLE
 * Umdrehungen tragen; wie gross er war, steht in `result->bit_count`.
 */
bool uft_fuse_revolutions_ausgerichtet(const uint8_t **revolutions,
                                       const size_t *bit_counts,
                                       size_t num_revolutions,
                                       long max_versatz,
                                       const uft_fusion_config_t *config,
                                       uft_fused_bitstream_t *result);

/** Mehrheitswerte als Bytes. @return geschriebene Bytes. */
size_t uft_fused_to_bytes(const uft_fused_bitstream_t *fused,
                          uint8_t *output, size_t max_bytes);

/** Positionen mit `weak_bit`. @return gefundene Anzahl. */
size_t uft_get_weak_bit_positions(const uft_fused_bitstream_t *fused,
                                  size_t *positions, size_t max_positions);

/** Positionen mit `revolutions_disagreed` — die Tatsache, nicht das Urteil. */
size_t uft_get_disagreement_positions(const uft_fused_bitstream_t *fused,
                                      size_t *positions,
                                      size_t max_positions);

void uft_fused_bitstream_free(uft_fused_bitstream_t *fused);

/** Gewichte je Umdrehung aus Qualitaetsmerkmalen (Summe = num_revs). */
void uft_calculate_revolution_weights(const uft_revolution_quality_t *qualities,
                                      size_t num_revs, float *weights);

/** Zusammenhaengende Bereiche schwacher Bits. @return gefundene Anzahl. */
size_t uft_find_weak_regions(const uft_fused_bitstream_t *fused,
                             uft_weak_region_t *regions, size_t max_regions);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ALGORITHMS_MULTI_REV_FUSION_H */

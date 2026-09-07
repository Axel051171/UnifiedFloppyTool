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

#ifndef UFT_MULTIREAD_PIPELINE_H
#define UFT_MULTIREAD_PIPELINE_H
/**
 * @file uft_multiread_pipeline.h
 * @brief Unified Multi-Read Recovery Pipeline — public API.
 *
 * High-level pipeline combining multi-pass reading, majority voting
 * across reads, adaptive strategy, confidence scoring and weak-bit
 * detection — the "recover damaged media without losing the fact that
 * it was damaged" layer.
 *
 * MF-215: this header was extracted from src/recovery/uft_multiread_
 * pipeline.c, which previously defined its entire API inline in the .c
 * (no public surface — nothing outside the TU could call or test it).
 * Pure extraction: the type/constant/prototype declarations moved here
 * verbatim, the .c now #includes this header. No behaviour changed.
 *
 * Forensic note (DESIGN_PRINCIPLES "Kein Bit verloren"): the voting
 * here does NOT silently collapse divergent reads to a majority value
 * and discard the disagreement. When passes disagree at a byte the
 * majority value is emitted, but the divergence is preserved as
 * forensic metadata — weak_mask, has_weak_bits and a sub-100
 * confidence. A failed-CRC read can never outvote a CRC-verified one.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Maximum read passes */
#define MULTIREAD_MAX_PASSES        16

/** Default number of passes */
#define MULTIREAD_DEFAULT_PASSES    5

/** Minimum confidence for success */
#define MULTIREAD_MIN_CONFIDENCE    75

/** Majority threshold (percentage) */

/** Maximum track size */
#define MULTIREAD_MAX_TRACK_SIZE    32768

/** Maximum sectors per track */
#define MULTIREAD_MAX_SECTORS       32

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    MULTIREAD_OK = 0,
    MULTIREAD_ERR_NULL_PARAM,
    MULTIREAD_ERR_ALLOC,
    MULTIREAD_ERR_NO_DATA,
    MULTIREAD_ERR_READ_FAILED,
    MULTIREAD_ERR_INSUFFICIENT_PASSES,
    MULTIREAD_ERR_LOW_CONFIDENCE,
    MULTIREAD_ERR_CRC_FAILED,
    MULTIREAD_ERR_COUNT
} multiread_error_t;

/*============================================================================
 * Data Structures
 *============================================================================*/

/** @brief Single read pass data */
typedef struct {
    uint8_t    *data;               /**< Sector/track data */
    size_t      data_len;           /**< Data length */
    uint8_t     quality;            /**< Read quality (0-100) */
    /** CRC verified for THIS pass. false means "did not verify" — whether
     *  because the check failed or because none was available. A sector with
     *  no verified pass is never reported as recovered (MF-466). */
    bool        crc_ok;
    double      timing_variance;    /**< Timing variance */
} multiread_pass_t;

/**
 * @brief Was mehrere Lesungen derselben Sektorposition ergeben (MF-473).
 *
 * Die Unterscheidung, die a8rawconv beim Sichten trifft
 * (`src/a8rawconv/disk.cpp:236-365`, Referenz-Orakel, wird nicht gebaut) und
 * die UFT bisher nicht traf. Der Reihe nach:
 *
 *   1. Gibt es mindestens eine Lesung mit gueltiger CRC, werden alle
 *      ungueltigen verworfen — getrennt fuer Adress- und Datenfeld
 *      (disk.cpp:240-254). Diese Regel hat `vote_buffer()` schon.
 *   2. Bleibt danach GENAU EIN Inhalt uebrig, ist der Sektor stabil.
 *   3. Bleiben mehrere, gewinnt der haeufigste; bei schlechter CRC ist der
 *      Sektor ausserdem weak ab dem laengsten gemeinsamen Praefix
 *      (disk.cpp:331-345).
 *
 * Warum das zaehlt: `STABLE_BAD_CRC` ist kein Schaden und kein Wackelkontakt,
 * sondern ein Kopierschutz, der eine falsche CRC ABSICHTLICH aufzeichnet. Ihn
 * als weak zu melden waere falsch, ihn als beschaedigt zu melden auch — und
 * nochmal lesen hilft nicht, weil er stabil ist. a8rawconv sagt an dieser
 * Stelle woertlich "Stable CRC error detected" (disk.cpp:364).
 */
typedef enum {
    /** Noch nichts ausgewertet (kein Pass, oder execute() nicht gelaufen). */
    MULTIREAD_CLASS_UNKNOWN = 0,
    /** Ein Inhalt, CRC geprueft. Der Normalfall einer gesunden Diskette. */
    MULTIREAD_CLASS_STABLE_GOOD,
    /** Ein Inhalt, CRC falsch — und zwar jedes Mal derselbe. Kopierschutz;
     *  weitere Leseversuche bringen nichts Neues. */
    MULTIREAD_CLASS_STABLE_BAD_CRC,
    /** Mehrere Inhalte bei falscher CRC: weak. @ref weak_offset sagt, ab
     *  welchem Byte sie auseinanderlaufen. */
    MULTIREAD_CLASS_WEAK,
    /** Mehrere Inhalte trotz gueltiger CRC. Sehr ungewoehnlich — a8rawconv
     *  meldet es als Warnung und behaelt einen davon (disk.cpp:320-328). */
    MULTIREAD_CLASS_AMBIGUOUS_GOOD
} multiread_class_t;

/** Klartextname einer Klasse, nie NULL. */
const char *multiread_class_name(multiread_class_t cls);

/** @brief Sector result from multi-read */
typedef struct {
    uint8_t     track;              /**< Track number */
    uint8_t     head;               /**< Head (side) */
    uint8_t     sector;             /**< Sector number */
    uint8_t    *data;               /**< Recovered data */
    size_t      data_len;           /**< Data length */
    /** Agreement between the passes, 0-100. NOT a statement about the CRC:
     *  reads that all say the same wrong thing reach 100. */
    uint8_t     confidence;
    uint8_t     good_reads;         /**< Passes whose CRC verified */
    uint8_t     total_reads;        /**< Total read attempts */
    /** Data that can be stood behind: enough agreement AND at least one
     *  verified read. A stable sector with a bad CRC — what a copy protection
     *  writes on purpose — is returned in @ref data but is NOT recovered
     *  (MF-466). Check `good_reads == 0 && confidence` high to tell that case
     *  from a genuinely uncertain one. */
    bool        recovered;
    bool        has_weak_bits;      /**< Passes disagreed somewhere */
    uint8_t    *weak_mask;          /**< Weak bit mask (optional) */

    /* MF-473 — angehaengt, nicht eingefuegt (ABI). */

    /** Wie sich die Lesungen zueinander verhalten. Siehe
     *  @ref multiread_class_t; das ist die Aussage, die `has_weak_bits`
     *  allein nicht treffen kann. */
    multiread_class_t class_;

    /** Erstes Byte, ab dem die Lesungen auseinanderlaufen, oder -1 wenn sie
     *  es nicht tun. Das laengste gemeinsame Praefix aller Varianten —
     *  dieselbe Bedeutung wie der Weak-Chunk eines ATX-Abbilds
     *  (src/formats/atx/uft_atx.c, MF-467), damit der eine Wert direkt in
     *  das andere Format geschrieben werden kann. */
    int32_t     weak_offset;

    /** Wie viele UNTERSCHIEDLICHE Inhalte die Lesungen ergaben, nachdem die
     *  CRC-Vorauswahl gelaufen ist. 1 heisst stabil. */
    uint8_t     distinct_contents;
} multiread_sector_t;

/** @brief Track result from multi-read */
typedef struct {
    uint8_t     track;              /**< Track number */
    uint8_t     head;               /**< Head (side) */
    multiread_sector_t sectors[MULTIREAD_MAX_SECTORS];
    uint8_t     sector_count;       /**< Number of sectors */
    uint8_t     good_sectors;       /**< Sectors with 100% confidence */
    uint8_t     recovered_sectors;  /**< Sectors recovered with voting */
    uint8_t     failed_sectors;     /**< Unrecoverable sectors */
    uint8_t     overall_confidence; /**< Track-level confidence */
} multiread_track_t;

/** @brief Pipeline configuration */
typedef struct {
    uint8_t     min_passes;         /**< Minimum read passes */
    uint8_t     max_passes;         /**< Maximum read passes */
    uint8_t     min_confidence;     /**< Minimum required confidence */

    /* Hier standen bis MF-673 `majority_pct` und `generate_report`.
     *
     * `majority_pct` hiess "Majority vote percentage" und entschied
     * nichts: im ganzen Baum kam es an genau zwei Stellen vor — der
     * Vorbelegung und einem BERICHTSTEXT, der es "Majority threshold"
     * nannte. `vote_byte()` nimmt die relative Mehrheit und fragt keine
     * Schwelle. Wer eine echte Mehrheitsschwelle will, baut zuerst die
     * Lesestelle gegen eine benannte Referenz, dann das Feld.
     *
     * `generate_report` war ein Schalter ohne Schaltung: der Bericht
     * entsteht durch den ausdruecklichen Aufruf von
     * multiread_generate_report(), nicht durch dieses Feld.
     *
     * Die wirksame Strenge ist @ref min_confidence — seit MF-673 von
     * aussen einstellbar ueber uft_convert_options_t. */
    bool        adaptive_passes;    /**< Increase passes on failure */
    bool        detect_weak_bits;   /**< Enable weak bit detection */

    /* Callbacks */
    void       *user_data;
    int       (*read_callback)(void *user_data, uint8_t track, uint8_t head,
                               uint8_t *data, size_t *len);
    void      (*progress_callback)(void *user_data, uint8_t track, uint8_t head,
                                   uint8_t pass, uint8_t total_passes);
} multiread_config_t;

/** @brief Pipeline context */
typedef struct {
    multiread_config_t config;

    /* Pass data storage */
    multiread_pass_t passes[MULTIREAD_MAX_PASSES];
    uint8_t pass_count;

    /* Statistics */
    uint32_t total_reads;
    uint32_t successful_reads;
    uint32_t failed_reads;
    uint32_t bytes_recovered;
    double   avg_confidence;
} multiread_ctx_t;

/*============================================================================
 * Public API
 *============================================================================*/

/** @brief Human-readable error string for a multiread_error_t. */
const char *multiread_error_string(multiread_error_t err);

/** @brief Default pipeline configuration. */
multiread_config_t multiread_config_default(void);

/** @brief Create a pipeline context (NULL config → defaults). */
multiread_ctx_t *multiread_create(const multiread_config_t *config);

/** @brief Destroy a pipeline context and free its pass buffers. */
void multiread_destroy(multiread_ctx_t *ctx);

/** @brief Add one read pass (its data is copied into the context). */
multiread_error_t multiread_add_pass(multiread_ctx_t *ctx,
                                     const uint8_t *data, size_t len,
                                     uint8_t quality, bool crc_ok);

/**
 * @brief Vote across the added passes into @p output.
 *
 * Fills @p result including weak_mask / has_weak_bits — divergent reads
 * are preserved as forensic metadata, never silently flattened.
 */
multiread_error_t multiread_execute(multiread_ctx_t *ctx,
                                    uint8_t *output, size_t output_len,
                                    multiread_sector_t *result);

/** @brief High-level: read a track via the config's read_callback + vote. */
multiread_error_t multiread_track(multiread_ctx_t *ctx,
                                  uint8_t track, uint8_t head,
                                  multiread_track_t *result);

/** @brief Convenience: vote across a set of caller-owned buffers. */
multiread_error_t multiread_vote_buffers(const uint8_t **buffers,
                                         const size_t *lengths,
                                         uint8_t buffer_count,
                                         uint8_t *output, size_t output_len,
                                         uint8_t *confidence);

/** @brief Read out the running recovery statistics. */
void multiread_get_stats(const multiread_ctx_t *ctx,
                         uint32_t *total_reads, uint32_t *successful_reads,
                         uint32_t *bytes_recovered, double *avg_confidence);

/** @brief Generate a human-readable recovery report (caller frees). */
char *multiread_generate_report(const multiread_ctx_t *ctx,
                                const multiread_track_t *tracks,
                                uint8_t track_count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MULTIREAD_PIPELINE_H */

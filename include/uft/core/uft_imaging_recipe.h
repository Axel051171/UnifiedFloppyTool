#ifndef UFT_IMAGING_RECIPE_H
#define UFT_IMAGING_RECIPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_RECIPE_API_VERSION 1u
#define UFT_RECIPE_MAX_SYNC_BYTES 8u
#define UFT_RECIPE_MAX_DECODERS 64u
#define UFT_RECIPE_MAX_FINDINGS 64u

typedef enum {
    UFT_R_OK = 0,
    UFT_R_EINVAL = -1,
    UFT_R_ENOMEM = -2,
    UFT_R_EIO = -3,
    UFT_R_ENOSYNC = -4,
    UFT_R_ECHECKSUM = -5,
    UFT_R_ENOSECTOR = -6,
    UFT_R_EDECODE = -7,
    UFT_R_EUNSUPPORTED = -8,
    UFT_R_EBOUNDS = -9,
    UFT_R_ECONFLICT = -10,
    UFT_R_ECANCELLED = -11,
    UFT_R_EVERIFY = -12
} uft_recipe_status_t;

typedef enum {
    UFT_RECIPE_INPUT_SECTORS = 0,
    UFT_RECIPE_INPUT_TRACK_BYTES = 1,
    UFT_RECIPE_INPUT_BITSTREAM = 2,
    UFT_RECIPE_INPUT_FLUX = 3
} uft_recipe_input_level_t;

typedef enum {
    UFT_RECIPE_SIDE_INTERLEAVED = 0, /* C0H0,C0H1,C1H0,C1H1 */
    UFT_RECIPE_SIDE_SEQUENTIAL = 1,  /* all H0, then all H1 */
    UFT_RECIPE_SIDE_SWAPPED = 2,     /* C0H1,C0H0,... */
    UFT_RECIPE_SIDE_SINGLE_0 = 3,
    UFT_RECIPE_SIDE_SINGLE_1 = 4
} uft_recipe_side_order_t;

typedef enum {
    UFT_RECIPE_SYNC_NONE = 0,
    UFT_RECIPE_SYNC_WORD = 1,
    UFT_RECIPE_SYNC_INDEX = 2,
    UFT_RECIPE_SYNC_SEQUENCE = 3
} uft_recipe_sync_kind_t;

typedef enum {
    UFT_RECIPE_OUTPUT_AT_OFFSET = 0,
    UFT_RECIPE_OUTPUT_APPEND = 1,
    UFT_RECIPE_OUTPUT_ANALYSIS_ONLY = 2
} uft_recipe_output_mode_t;

typedef enum {
    UFT_RECIPE_ERROR_ABORT = 0,
    UFT_RECIPE_ERROR_RETRY = 1,
    UFT_RECIPE_ERROR_KEEP_BAD = 2,
    UFT_RECIPE_ERROR_SKIP = 3
} uft_recipe_error_policy_t;

/** Was an einer Spur geprueft wurde. Der Bericht fuehrt diesen Wert, damit
 *  "geprueft" und "nicht geprueft" unterscheidbar bleiben.
 *
 *  UFT_RECIPE_VERIFY_DECODER ist ein Protokollwert: kein mitgelieferter
 *  Dekoder meldet ihn heute. Wer einen Dekoder mit eigener Pruefsumme
 *  schreibt (AmigaDOS, GCR), setzt ihn selbst — die Maschine vergibt ihn
 *  nicht, damit sie nichts behauptet, was sie nicht getan hat. */
typedef enum {
    UFT_RECIPE_VERIFY_NONE = 0,
    UFT_RECIPE_VERIFY_CRC16_CCITT = 1,
    UFT_RECIPE_VERIFY_EXACT_SIZE = 2,
    UFT_RECIPE_VERIFY_DECODER = 3
} uft_recipe_verify_kind_t;

/** Der Ausgang einer Pruefung — drei Zustaende, nicht zwei.
 *
 *  Ein `false` in `checksum_ok` waere mehrdeutig: es kann "geprueft und
 *  falsch" heissen oder "gar nicht geprueft". Das auseinanderzuhalten ist
 *  der ganze Punkt, denn ungeprueft darf weder still ja noch still nein
 *  bedeuten. Gleiche Bauform wie `caps` / `caps_bekannt` im Kopierplan. */
typedef enum {
    UFT_RECIPE_CHECK_NONE = 0,   /**< nicht geprueft         */
    UFT_RECIPE_CHECK_OK = 1,     /**< geprueft, stimmt       */
    UFT_RECIPE_CHECK_FAILED = 2  /**< geprueft, stimmt nicht */
} uft_recipe_check_state_t;

/** Der einzige heute umgesetzte Pruefsummen-Bezeichner fuer
 *  `uft_recipe_track_rule_t::checksum_id`. Ein unbekannter Bezeichner
 *  wird von `uft_recipe_validate()` abgewiesen, nicht ignoriert. */
#define UFT_RECIPE_CHECKSUM_CRC16_CCITT "crc16-ccitt"

typedef enum {
    UFT_RECIPE_CAP_TRACK_BYTES = 1u << 0,
    UFT_RECIPE_CAP_BITSTREAM = 1u << 1,
    UFT_RECIPE_CAP_FLUX = 1u << 2,
    UFT_RECIPE_CAP_INDEX = 1u << 3,
    UFT_RECIPE_CAP_MULTI_REV = 1u << 4,
    UFT_RECIPE_CAP_CUSTOM_DECODER = 1u << 5,
    UFT_RECIPE_CAP_TIMING = 1u << 6,
    UFT_RECIPE_CAP_WEAK_BITS = 1u << 7
} uft_recipe_capability_t;

typedef struct {
    const uint8_t *data;
    size_t size_bytes;
    size_t size_bits;
    uint64_t index_bit;
    uint64_t rotation_ns;
    unsigned revolution;
    bool index_valid;
    bool timing_valid;
    bool owns_data;
} uft_recipe_capture_t;

typedef struct {
    int first_track;
    int last_track;
    int track_step;
    size_t decoded_bytes;
    size_t raw_min_bytes;
    size_t raw_max_bytes;
    uft_recipe_input_level_t input_level;
    uft_recipe_sync_kind_t sync_kind;
    uint8_t sync[UFT_RECIPE_MAX_SYNC_BYTES];
    uint8_t sync_bits;
    unsigned sync_occurrence;
    const char *decoder_id;
    const char *checksum_id;
    uft_recipe_output_mode_t output_mode;
    size_t output_offset;
    uft_recipe_error_policy_t error_policy;
    unsigned retries;
    unsigned revolutions;
    bool preserve_bad_capture;
    bool require_exact_raw_length;

    /* Angehaengt, nicht eingefuegt: die Felder davor bleiben unberuehrt.
     *
     * `checksum_id` war bis hierher ein Feld ohne Leser — die Maschine
     * rechnete eine CRC und setzte `checksum_ok` unbedingt auf true.
     * Eine Pruefung braucht einen Sollwert; ohne ihn ist `checksum_id`
     * eine Absichtserklaerung, keine Pruefung, und `validate()` weist
     * die Regel deshalb ab. */
    uint16_t checksum_expect;       /**< Sollwert, nur mit checksum_id  */
    bool checksum_expect_valid;     /**< false = kein Sollwert gesetzt  */
} uft_recipe_track_rule_t;

typedef enum {
    UFT_FP_CRC16 = 1u << 0,
    UFT_FP_RAW_LENGTH = 1u << 1,
    UFT_FP_SYNC_PRESENT = 1u << 2,
    UFT_FP_INDEX_POSITION = 1u << 3
} uft_recipe_fingerprint_field_t;

typedef struct {
    int track;
    uint32_t fields;
    uint16_t crc16;
    size_t raw_length;
    uint8_t sync[UFT_RECIPE_MAX_SYNC_BYTES];
    uint8_t sync_bits;
    uint64_t index_bit;
    uint64_t index_tolerance_bits;
    unsigned weight;
} uft_recipe_fingerprint_t;

typedef struct {
    int track;
    uft_recipe_capture_t capture;
} uft_recipe_observation_t;

typedef struct {
    const char *id;
    const char *description;
    const uft_recipe_track_rule_t *rules;
    size_t rule_count;
    const uft_recipe_fingerprint_t *fingerprints;
    size_t fingerprint_count;
    const char *postprocessor_id;
} uft_recipe_variant_t;

typedef struct {
    uint32_t api_version;
    const char *id;
    const char *title;
    const char *platform;
    const char *provenance;
    unsigned cylinders;
    unsigned heads;
    uft_recipe_side_order_t side_order;
    const uft_recipe_variant_t *variants;
    size_t variant_count;
    uint32_t required_capabilities;
    bool preservation_recipe;
    bool verified_with_real_media;
} uft_imaging_recipe_t;

typedef struct {
    int logical_track;
    int cylinder;
    int head;
    unsigned attempts;
    unsigned successful_reads;
    uft_recipe_status_t status;
    size_t raw_bytes;
    size_t decoded_bytes;
    size_t sync_bit_offset;
    uint16_t crc16;
    bool sync_found;
    /** true NUR wenn eine Pruefsumme wirklich verglichen wurde UND sie
     *  gestimmt hat. Ein `false` heisst "nicht verglichen" ODER
     *  "verglichen und falsch" — wer beides trennen muss, liest
     *  `checksum_state`. Der Name ist der alte; die Bedeutung ist seit
     *  der Berichtigung enger. */
    bool checksum_ok;
    bool retained_bad_capture;

    /* Angehaengt, nicht eingefuegt. */

    /** Ausgang der Pruefsummen-Pruefung. NONE heisst: es wurde keine
     *  verglichen — das ist kein Urteil ueber die Daten. */
    uft_recipe_check_state_t checksum_state;

    /** Was die REGEL verlangt hat. EXACT_SIZE zusammen mit
     *  `checksum_state == NONE` heisst: die Laenge wurde geprueft, eine
     *  Pruefsumme nicht. Ob die Laengenprobe bestanden wurde, sagt
     *  `status`. */
    uft_recipe_verify_kind_t verify_kind;

    /** true, wenn erst ein Wiederholungsversuch gelang (attempt > 0).
     *  Zaehlt in `uft_recipe_report_t::warnings`: die Spur ist lesbar,
     *  aber nicht beim ersten Anlauf — forensisch bemerkenswert. */
    bool retried;
} uft_recipe_track_report_t;

typedef struct {
    const uft_imaging_recipe_t *recipe;
    const uft_recipe_variant_t *variant;
    uft_recipe_track_report_t *tracks;
    size_t track_count;
    size_t track_capacity;
    unsigned errors;
    unsigned warnings;
    size_t output_bytes;
    bool complete;
} uft_recipe_report_t;

struct uft_recipe_engine;
struct uft_recipe_decode_context;

typedef uft_recipe_status_t (*uft_recipe_decoder_fn)(
    const struct uft_recipe_decode_context *ctx,
    const uft_recipe_track_rule_t *rule,
    const uft_recipe_capture_t *capture,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_size);

typedef struct {
    const char *id;
    const char *description;
    uft_recipe_decoder_fn decode;
    uint32_t required_capabilities;
} uft_recipe_decoder_t;

typedef uft_recipe_status_t (*uft_recipe_read_fn)(
    void *user, int cylinder, int head, uft_recipe_input_level_t level,
    unsigned attempt, unsigned revolution, uft_recipe_capture_t *out);
typedef void (*uft_recipe_release_fn)(void *user, uft_recipe_capture_t *capture);
typedef uft_recipe_status_t (*uft_recipe_write_fn)(
    void *user, size_t offset, const uint8_t *data, size_t size);
typedef uft_recipe_status_t (*uft_recipe_write_capture_fn)(
    void *user, int logical_track, int cylinder, int head,
    unsigned attempt, unsigned revolution,
    const uft_recipe_capture_t *capture, uft_recipe_status_t decode_status);
typedef bool (*uft_recipe_cancel_fn)(void *user);

typedef struct {
    void *user;
    uft_recipe_read_fn read;
    uft_recipe_release_fn release;
    uft_recipe_cancel_fn cancelled;
    uint32_t capabilities;
} uft_recipe_source_t;

typedef struct {
    void *user;
    uft_recipe_write_fn write;
    uft_recipe_write_capture_fn write_capture;
} uft_recipe_sink_t;

typedef struct uft_recipe_decode_context {
    const struct uft_recipe_engine *engine;
    int logical_track;
    int cylinder;
    int head;
    size_t sync_bit_offset;
} uft_recipe_decode_context_t;

typedef struct uft_recipe_engine {
    const uft_recipe_decoder_t *decoders[UFT_RECIPE_MAX_DECODERS];
    size_t decoder_count;
} uft_recipe_engine_t;

void uft_recipe_engine_init(uft_recipe_engine_t *engine);
uft_recipe_status_t uft_recipe_register_decoder(
    uft_recipe_engine_t *engine, const uft_recipe_decoder_t *decoder);
const uft_recipe_decoder_t *uft_recipe_find_decoder(
    const uft_recipe_engine_t *engine, const char *id);

uft_recipe_status_t uft_recipe_validate(
    const uft_recipe_engine_t *engine, const uft_imaging_recipe_t *recipe,
    char *message, size_t message_size);

uft_recipe_status_t uft_recipe_map_track(
    const uft_imaging_recipe_t *recipe, int logical_track,
    int *cylinder, int *head);

uft_recipe_status_t uft_recipe_select_variant(
    const uft_imaging_recipe_t *recipe,
    const uft_recipe_observation_t *observations,
    size_t observation_count,
    const uft_recipe_variant_t **selected,
    unsigned *score,
    bool *ambiguous);

uft_recipe_status_t uft_recipe_run(
    const uft_recipe_engine_t *engine,
    const uft_imaging_recipe_t *recipe,
    const uft_recipe_variant_t *variant,
    const uft_recipe_source_t *source,
    const uft_recipe_sink_t *sink,
    uft_recipe_report_t *report);

void uft_recipe_report_free(uft_recipe_report_t *report);
size_t uft_recipe_report_json(const uft_recipe_report_t *report,
                              char *buffer, size_t capacity);

size_t uft_recipe_find_bits(const uint8_t *data, size_t data_bits,
                            const uint8_t *pattern, size_t pattern_bits,
                            size_t start_bit, bool wrap);
uint16_t uft_recipe_crc16_ccitt(const uint8_t *data, size_t size,
                                uint16_t initial);

const char *uft_recipe_status_string(uft_recipe_status_t status);

extern const uft_recipe_decoder_t UFT_DECODER_RAW_COPY;
extern const uft_recipe_decoder_t UFT_DECODER_MFM_ODD_EVEN;
extern const uft_recipe_decoder_t UFT_DECODER_MFM_INTERLEAVED;

#ifdef __cplusplus
}
#endif

#endif

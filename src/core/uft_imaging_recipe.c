#include "uft/core/uft_imaging_recipe.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_BIT ((size_t)-1)

static int get_bit(const uint8_t *p, size_t bit)
{
    return (p[bit >> 3] >> (7u - (bit & 7u))) & 1u;
}

size_t uft_recipe_find_bits(const uint8_t *data, size_t data_bits,
                            const uint8_t *pattern, size_t pattern_bits,
                            size_t start_bit, bool wrap)
{
    if (!data || !pattern || !data_bits || !pattern_bits ||
        pattern_bits > data_bits || start_bit >= data_bits)
        return NO_BIT;

    size_t candidates = wrap ? data_bits : data_bits - pattern_bits + 1u;
    for (size_t n = 0; n < candidates; ++n) {
        size_t pos = (start_bit + n) % data_bits;
        if (!wrap && pos + pattern_bits > data_bits) break;
        bool equal = true;
        for (size_t j = 0; j < pattern_bits; ++j) {
            if (get_bit(data, (pos + j) % data_bits) != get_bit(pattern, j)) {
                equal = false;
                break;
            }
        }
        if (equal) return pos;
    }
    return NO_BIT;
}

uint16_t uft_recipe_crc16_ccitt(const uint8_t *data, size_t size,
                                uint16_t initial)
{
    uint16_t crc = initial;
    if (!data && size) return crc;
    for (size_t i = 0; i < size; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned b = 0; b < 8; ++b)
            crc = (uint16_t)((crc & 0x8000u)
                ? ((uint32_t)crc << 1) ^ 0x1021u
                : ((uint32_t)crc << 1));
    }
    return crc;
}

const char *uft_recipe_status_string(uft_recipe_status_t s)
{
    switch (s) {
    case UFT_R_OK: return "ok";
    case UFT_R_EINVAL: return "invalid";
    case UFT_R_ENOMEM: return "out_of_memory";
    case UFT_R_EIO: return "io_error";
    case UFT_R_ENOSYNC: return "sync_not_found";
    case UFT_R_ECHECKSUM: return "checksum_failed";
    case UFT_R_ENOSECTOR: return "sector_missing";
    case UFT_R_EDECODE: return "decode_failed";
    case UFT_R_EUNSUPPORTED: return "unsupported";
    case UFT_R_EBOUNDS: return "bounds_error";
    case UFT_R_ECONFLICT: return "conflict";
    case UFT_R_ECANCELLED: return "cancelled";
    case UFT_R_EVERIFY: return "verify_failed";
    default: return "unknown";
    }
}

static uft_recipe_status_t raw_copy(
    const uft_recipe_decode_context_t *ctx,
    const uft_recipe_track_rule_t *rule,
    const uft_recipe_capture_t *capture,
    uint8_t *output, size_t output_capacity, size_t *output_size)
{
    (void)ctx;
    if (!rule || !capture || !output_size) return UFT_R_EINVAL;
    size_t wanted = rule->decoded_bytes ? rule->decoded_bytes : capture->size_bytes;
    if (wanted > capture->size_bytes) return UFT_R_EBOUNDS;
    if (wanted > output_capacity || (wanted && !output)) return UFT_R_EBOUNDS;
    if (wanted) memcpy(output, capture->data, wanted);
    *output_size = wanted;
    return UFT_R_OK;
}

/* Amiga-style odd/even MFM: each encoded 32-bit half contributes alternating
 * data bits. This decoder deliberately handles only the primitive transform;
 * format-specific header/sector/checksum logic remains a named decoder. */
static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

/* Ein 32-Bit-Wort ab einer BELIEBIGEN Bitstelle, hoechstwertiges Bit
 * zuerst.
 *
 * Vorher rechneten beide MFM-Dekoder `start = (sync_bit + sync_bits + 7) / 8`
 * und begannen damit an der naechsten BYTEGRENZE. Gemessen an einem Sync
 * bei Bit 3: der Sync endet an Bit 11, der Dekoder begann bei Bit 16 —
 * fuenf Bit still uebersprungen, Ergebnis als UFT_R_OK gemeldet. In einem
 * echten MFM-Bitstrom ist eine Fundstelle abseits der Bytegrenze der
 * Normalfall und nicht die Ausnahme, also war das der Hauptpfad.
 *
 * Der Aufrufer stellt sicher, dass `bit + 32 <= verfuegbare Bits` gilt. */
static uint32_t be32_at_bit(const uint8_t *p, size_t bit)
{
    uint32_t v = 0;
    for (unsigned i = 0; i < 32u; ++i)
        v = (v << 1) | (uint32_t)get_bit(p, bit + i);
    return v;
}

/* Die Bitstelle, an der ein Dekoder hinter dem Sync weiterliest.
 * Ohne Sync ist das Bit 0 — nicht Byte 0 mit Rundung. */
static size_t decode_start_bit(const uft_recipe_decode_context_t *ctx,
                               const uft_recipe_track_rule_t *rule)
{
    if (ctx->sync_bit_offset == NO_BIT) return 0u;
    return ctx->sync_bit_offset + rule->sync_bits;
}

/* Wie viele Bits die Aufnahme wirklich traegt. `size_bits` ist die
 * Angabe der Quelle; `run_track()` hat sie gegen `size_bytes * 8`
 * geprueft, bevor ein Dekoder sie sieht. */
static size_t capture_bits(const uft_recipe_capture_t *c)
{
    return c->size_bits ? c->size_bits : c->size_bytes * 8u;
}

static uft_recipe_status_t mfm_odd_even(
    const uft_recipe_decode_context_t *ctx,
    const uft_recipe_track_rule_t *rule,
    const uft_recipe_capture_t *capture,
    uint8_t *output, size_t output_capacity, size_t *output_size)
{
    if (!ctx || !rule || !capture || !output || !output_size)
        return UFT_R_EINVAL;
    size_t bytes = rule->decoded_bytes;
    if (!bytes || (bytes & 3u) || bytes > output_capacity) return UFT_R_EINVAL;
    if (bytes > SIZE_MAX / 16u) return UFT_R_EINVAL;   /* bytes*2*8 */
    size_t encoded_bits = bytes * 16u;                 /* odd + even */
    size_t start = decode_start_bit(ctx, rule);
    size_t have = capture_bits(capture);
    if (start > have || encoded_bits > have - start) return UFT_R_EBOUNDS;
    /* Die geraden Bits liegen als geschlossener Block hinter den
     * ungeraden: erst `bytes` rohe MFM-Byte odd, dann `bytes` even. */
    size_t odd_bit = start;
    size_t even_bit = start + bytes * 8u;
    for (size_t i = 0; i < bytes; i += 4) {
        uint32_t o = be32_at_bit(capture->data, odd_bit + i * 8u) & 0x55555555u;
        uint32_t e = be32_at_bit(capture->data, even_bit + i * 8u) & 0x55555555u;
        put_be32(output + i, (o << 1) | e);
    }
    *output_size = bytes;
    return UFT_R_OK;
}

static uft_recipe_status_t mfm_interleaved(
    const uft_recipe_decode_context_t *ctx,
    const uft_recipe_track_rule_t *rule,
    const uft_recipe_capture_t *capture,
    uint8_t *output, size_t output_capacity, size_t *output_size)
{
    if (!ctx || !rule || !capture || !output || !output_size)
        return UFT_R_EINVAL;
    size_t bytes = rule->decoded_bytes;
    if (!bytes || (bytes & 3u) || bytes > output_capacity) return UFT_R_EINVAL;
    if (bytes > SIZE_MAX / 16u) return UFT_R_EINVAL;
    size_t encoded_bits = bytes * 16u;
    size_t start = decode_start_bit(ctx, rule);
    size_t have = capture_bits(capture);
    if (start > have || encoded_bits > have - start) return UFT_R_EBOUNDS;
    /* Hier wechseln sich odd und even je Langwort ab: 4 Byte odd,
     * 4 Byte even, und so fort. */
    for (size_t i = 0; i < bytes; i += 4) {
        size_t b = start + i * 16u;
        uint32_t o = be32_at_bit(capture->data, b) & 0x55555555u;
        uint32_t e = be32_at_bit(capture->data, b + 32u) & 0x55555555u;
        put_be32(output + i, (o << 1) | e);
    }
    *output_size = bytes;
    return UFT_R_OK;
}

const uft_recipe_decoder_t UFT_DECODER_RAW_COPY = {
    "raw-copy", "Bounded byte-for-byte track copy", raw_copy,
    0
};
const uft_recipe_decoder_t UFT_DECODER_MFM_ODD_EVEN = {
    "mfm-odd-even", "Generic separated odd/even MFM primitive", mfm_odd_even,
    UFT_RECIPE_CAP_BITSTREAM | UFT_RECIPE_CAP_CUSTOM_DECODER
};
const uft_recipe_decoder_t UFT_DECODER_MFM_INTERLEAVED = {
    "mfm-interleaved", "Generic interleaved odd/even MFM primitive", mfm_interleaved,
    UFT_RECIPE_CAP_BITSTREAM | UFT_RECIPE_CAP_CUSTOM_DECODER
};

void uft_recipe_engine_init(uft_recipe_engine_t *e)
{
    if (!e) return;
    memset(e, 0, sizeof(*e));
    (void)uft_recipe_register_decoder(e, &UFT_DECODER_RAW_COPY);
    (void)uft_recipe_register_decoder(e, &UFT_DECODER_MFM_ODD_EVEN);
    (void)uft_recipe_register_decoder(e, &UFT_DECODER_MFM_INTERLEAVED);
}

const uft_recipe_decoder_t *uft_recipe_find_decoder(
    const uft_recipe_engine_t *e, const char *id)
{
    if (!e || !id) return NULL;
    for (size_t i = 0; i < e->decoder_count; ++i)
        if (e->decoders[i] && e->decoders[i]->id &&
            strcmp(e->decoders[i]->id, id) == 0) return e->decoders[i];
    return NULL;
}

uft_recipe_status_t uft_recipe_register_decoder(
    uft_recipe_engine_t *e, const uft_recipe_decoder_t *d)
{
    if (!e || !d || !d->id || !d->decode) return UFT_R_EINVAL;
    if (uft_recipe_find_decoder(e, d->id)) return UFT_R_ECONFLICT;
    if (e->decoder_count >= UFT_RECIPE_MAX_DECODERS) return UFT_R_EBOUNDS;
    e->decoders[e->decoder_count++] = d;
    return UFT_R_OK;
}

static void set_message(char *p, size_t n, const char *s)
{
    if (!p || !n) return;
    snprintf(p, n, "%s", s ? s : "");
}

uft_recipe_status_t uft_recipe_validate(
    const uft_recipe_engine_t *e, const uft_imaging_recipe_t *r,
    char *message, size_t message_size)
{
    if (!e || !r || !r->id || !r->variants || !r->variant_count) {
        set_message(message, message_size, "recipe/header missing");
        return UFT_R_EINVAL;
    }
    /* Die obere Geometrieschranke ist kein erfundener Hoechstwert,
     * sondern eine Ueberlaufsperre: `cylinders * heads` wird gleich
     * nach `int` gewandelt, und `max_track - track_step` weiter unten
     * darf nicht ueberlaufen. */
    if (r->api_version != UFT_RECIPE_API_VERSION || !r->cylinders ||
        !r->heads || r->heads > 2 || r->cylinders > (unsigned)(INT_MAX / 4)) {
        set_message(message, message_size, "unsupported API or geometry");
        return UFT_R_EUNSUPPORTED;
    }
    const int max_track = (int)(r->cylinders * r->heads) - 1;
    for (size_t v = 0; v < r->variant_count; ++v) {
        const uft_recipe_variant_t *var = &r->variants[v];
        if (!var->id || !var->rules || !var->rule_count) {
            set_message(message, message_size, "variant has no rules");
            return UFT_R_EINVAL;
        }
        for (size_t i = 0; i < var->rule_count; ++i) {
            const uft_recipe_track_rule_t *x = &var->rules[i];
            if (x->track_step == 0 || x->first_track < 0 || x->last_track < 0 ||
                x->first_track > max_track || x->last_track > max_track ||
                x->track_step > max_track + 1 ||
                x->track_step < -(max_track + 1) ||
                (x->last_track != x->first_track &&
                 (((x->last_track - x->first_track) > 0) != (x->track_step > 0))) ||
                !x->decoder_id || x->sync_bits > UFT_RECIPE_MAX_SYNC_BYTES * 8u ||
                (x->raw_min_bytes && x->raw_max_bytes &&
                 x->raw_min_bytes > x->raw_max_bytes)) {
                set_message(message, message_size, "invalid track rule");
                return UFT_R_EINVAL;
            }
            if (!uft_recipe_find_decoder(e, x->decoder_id)) {
                set_message(message, message_size, x->decoder_id);
                return UFT_R_EUNSUPPORTED;
            }
            /* Eine Pruefsumme ohne Sollwert ist keine Pruefung, sondern
             * eine Absichtserklaerung. Bis hierher hatte `checksum_id`
             * ueberhaupt keinen Leser, und `checksum_ok` wurde unbedingt
             * auf true gesetzt — gemessen auch bei checksum_id == NULL. */
            if (x->checksum_id) {
                if (strcmp(x->checksum_id,
                           UFT_RECIPE_CHECKSUM_CRC16_CCITT) != 0) {
                    set_message(message, message_size, x->checksum_id);
                    return UFT_R_EUNSUPPORTED;
                }
                if (!x->checksum_expect_valid) {
                    set_message(message, message_size,
                                "checksum_id without checksum_expect");
                    return UFT_R_EINVAL;
                }
            }
        }
    }
    set_message(message, message_size, "ok");
    return UFT_R_OK;
}

uft_recipe_status_t uft_recipe_map_track(
    const uft_imaging_recipe_t *r, int t, int *c, int *h)
{
    if (!r || !c || !h || t < 0 || !r->cylinders || !r->heads)
        return UFT_R_EINVAL;
    unsigned total = r->cylinders * r->heads;
    if ((unsigned)t >= total) return UFT_R_EBOUNDS;
    switch (r->side_order) {
    case UFT_RECIPE_SIDE_INTERLEAVED: *c = t / (int)r->heads; *h = t % (int)r->heads; break;
    case UFT_RECIPE_SIDE_SWAPPED: *c = t / (int)r->heads; *h = (int)r->heads - 1 - t % (int)r->heads; break;
    case UFT_RECIPE_SIDE_SEQUENTIAL: *h = t / (int)r->cylinders; *c = t % (int)r->cylinders; break;
    case UFT_RECIPE_SIDE_SINGLE_0: *c = t; *h = 0; break;
    case UFT_RECIPE_SIDE_SINGLE_1: *c = t; *h = 1; break;
    default: return UFT_R_EINVAL;
    }
    if ((unsigned)*c >= r->cylinders || (unsigned)*h >= r->heads)
        return UFT_R_EBOUNDS;
    return UFT_R_OK;
}

static const uft_recipe_observation_t *observation_for(
    const uft_recipe_observation_t *o, size_t n, int track)
{
    for (size_t i = 0; i < n; ++i) if (o[i].track == track) return &o[i];
    return NULL;
}

static bool fingerprint_matches(const uft_recipe_fingerprint_t *f,
                                const uft_recipe_capture_t *c)
{
    if (!f || !c || !c->data) return false;
    /* Zweiter Eingang in die Maschine, und bis hierher der ungesicherte:
     * `run_track()` prueft `size_bits <= size_bytes * 8` (die eigene
     * Zusage der Doku), die Variantenwahl tat es nicht und reichte den
     * Wert der Quelle ungeprueft an die Bitsuche weiter. Dieselbe
     * Rechnung gehoert an beide Eingaenge. */
    if (c->size_bytes > SIZE_MAX / 8u) return false;
    size_t bits = c->size_bits ? c->size_bits : c->size_bytes * 8u;
    if (bits > c->size_bytes * 8u) return false;
    if ((f->fields & UFT_FP_CRC16) &&
        uft_recipe_crc16_ccitt(c->data, c->size_bytes, 0xffffu) != f->crc16)
        return false;
    if ((f->fields & UFT_FP_RAW_LENGTH) && c->size_bytes != f->raw_length)
        return false;
    if ((f->fields & UFT_FP_SYNC_PRESENT) &&
        uft_recipe_find_bits(c->data, bits, f->sync, f->sync_bits, 0, true) == NO_BIT)
        return false;
    if (f->fields & UFT_FP_INDEX_POSITION) {
        if (!c->index_valid) return false;
        uint64_t a = c->index_bit > f->index_bit ? c->index_bit - f->index_bit
                                                 : f->index_bit - c->index_bit;
        if (a > f->index_tolerance_bits) return false;
    }
    return true;
}

uft_recipe_status_t uft_recipe_select_variant(
    const uft_imaging_recipe_t *recipe,
    const uft_recipe_observation_t *observations, size_t observation_count,
    const uft_recipe_variant_t **selected, unsigned *score, bool *ambiguous)
{
    if (!recipe || !selected || !score || !ambiguous ||
        (!observations && observation_count)) return UFT_R_EINVAL;
    *selected = NULL; *score = 0; *ambiguous = false;
    unsigned best = 0;
    for (size_t vi = 0; vi < recipe->variant_count; ++vi) {
        const uft_recipe_variant_t *v = &recipe->variants[vi];
        unsigned s = 0, seen = 0;
        bool mismatch = false;
        for (size_t fi = 0; fi < v->fingerprint_count; ++fi) {
            const uft_recipe_fingerprint_t *f = &v->fingerprints[fi];
            const uft_recipe_observation_t *o =
                observation_for(observations, observation_count, f->track);
            if (!o) continue;
            ++seen;
            if (fingerprint_matches(f, &o->capture))
                s += f->weight ? f->weight : 1u;
            else mismatch = true;
        }
        if (mismatch || (!seen && recipe->variant_count > 1u)) continue;
        if (!*selected || s > best) {
            *selected = v; best = s; *ambiguous = false;
        } else if (s == best) {
            *ambiguous = true;
        }
    }
    if (!*selected) return UFT_R_EVERIFY;
    *score = best;
    return *ambiguous ? UFT_R_ECONFLICT : UFT_R_OK;
}

static uft_recipe_status_t append_report(uft_recipe_report_t *r,
                                         const uft_recipe_track_report_t *t)
{
    if (r->track_count == r->track_capacity) {
        size_t next = r->track_capacity ? r->track_capacity * 2u : 32u;
        if (next > SIZE_MAX / sizeof(*r->tracks)) return UFT_R_ENOMEM;
        void *p = realloc(r->tracks, next * sizeof(*r->tracks));
        if (!p) return UFT_R_ENOMEM;
        r->tracks = (uft_recipe_track_report_t *)p;
        r->track_capacity = next;
    }
    r->tracks[r->track_count++] = *t;
    return UFT_R_OK;
}

static uft_recipe_status_t find_sync(const uft_recipe_track_rule_t *rule,
                                     const uft_recipe_capture_t *cap,
                                     size_t *where)
{
    *where = NO_BIT;
    if (rule->sync_kind == UFT_RECIPE_SYNC_NONE) return UFT_R_OK;
    if (rule->sync_kind == UFT_RECIPE_SYNC_INDEX)
        return cap->index_valid ? (*where = (size_t)cap->index_bit, UFT_R_OK)
                                : UFT_R_ENOSYNC;
    if (!rule->sync_bits) return UFT_R_EINVAL;
    size_t pos = 0;
    unsigned occurrence = rule->sync_occurrence ? rule->sync_occurrence : 1u;
    for (unsigned i = 0; i < occurrence; ++i) {
        pos = uft_recipe_find_bits(cap->data, cap->size_bits, rule->sync,
                                   rule->sync_bits, pos, false);
        if (pos == NO_BIT) return UFT_R_ENOSYNC;
        if (i + 1u < occurrence) pos += rule->sync_bits;
    }
    *where = pos;
    return UFT_R_OK;
}

static uft_recipe_status_t run_track(
    const uft_recipe_engine_t *engine, const uft_imaging_recipe_t *recipe,
    const uft_recipe_track_rule_t *rule, int logical,
    const uft_recipe_source_t *source, const uft_recipe_sink_t *sink,
    size_t *append_offset, uft_recipe_track_report_t *tr)
{
    memset(tr, 0, sizeof(*tr));
    tr->logical_track = logical;
    tr->sync_bit_offset = NO_BIT;
    uft_recipe_status_t st = uft_recipe_map_track(recipe, logical,
                                                  &tr->cylinder, &tr->head);
    if (st != UFT_R_OK) return tr->status = st;
    const uft_recipe_decoder_t *decoder = uft_recipe_find_decoder(engine, rule->decoder_id);
    if (!decoder) return tr->status = UFT_R_EUNSUPPORTED;
    if ((source->capabilities & decoder->required_capabilities) !=
        decoder->required_capabilities)
        return tr->status = UFT_R_EUNSUPPORTED;

    unsigned tries = rule->retries + 1u;
    unsigned revs = rule->revolutions ? rule->revolutions : 1u;
    uint8_t *decoded = NULL;
    size_t decoded_cap = rule->decoded_bytes;
    if (!decoded_cap && rule->raw_max_bytes) decoded_cap = rule->raw_max_bytes;
    if (!decoded_cap) decoded_cap = 1024u * 1024u;
    decoded = (uint8_t *)malloc(decoded_cap);
    if (!decoded) return tr->status = UFT_R_ENOMEM;

    for (unsigned attempt = 0; attempt < tries; ++attempt) {
        for (unsigned rev = 0; rev < revs; ++rev) {
            if (source->cancelled && source->cancelled(source->user)) {
                st = UFT_R_ECANCELLED;
                goto done;
            }
            uft_recipe_capture_t cap;
            memset(&cap, 0, sizeof(cap));
            ++tr->attempts;
            st = source->read(source->user, tr->cylinder, tr->head,
                              rule->input_level, attempt, rev, &cap);
            if (st != UFT_R_OK) goto released;
            ++tr->successful_reads;
            tr->raw_bytes = cap.size_bytes;
            if (!cap.size_bits) cap.size_bits = cap.size_bytes * 8u;
            if (!cap.data || cap.size_bytes > SIZE_MAX / 8u ||
                cap.size_bits > cap.size_bytes * 8u ||
                (rule->raw_min_bytes && cap.size_bytes < rule->raw_min_bytes) ||
                (rule->raw_max_bytes && cap.size_bytes > rule->raw_max_bytes) ||
                (rule->require_exact_raw_length && rule->raw_min_bytes &&
                 cap.size_bytes != rule->raw_min_bytes)) {
                st = UFT_R_EBOUNDS;
                goto released;
            }
            st = find_sync(rule, &cap, &tr->sync_bit_offset);
            tr->sync_found = st == UFT_R_OK;
            if (st != UFT_R_OK) goto released;

            uft_recipe_decode_context_t ctx = {
                engine, logical, tr->cylinder, tr->head, tr->sync_bit_offset
            };
            size_t n = 0;
            st = decoder->decode(&ctx, rule, &cap, decoded, decoded_cap, &n);
            if (st != UFT_R_OK) goto released;
            if (rule->decoded_bytes && n != rule->decoded_bytes) {
                st = UFT_R_EVERIFY;
                goto released;
            }
            tr->decoded_bytes = n;
            tr->crc16 = uft_recipe_crc16_ccitt(decoded, n, 0xffffu);

            /* Was WIRKLICH geprueft wurde — und nur das.
             *
             * Hier stand `tr->checksum_ok = true;`, unbedingt, an genau
             * einer Stelle, ohne jeden Vergleich. Gemessen meldete eine
             * Regel mit `checksum_id == NULL` trotzdem `checksum_ok=true`,
             * und `checksum_id` hatte im ganzen Kit null Leser. Das ist
             * dieselbe Klasse wie ein Sektorleser, der jedem Sektor
             * unbedingt "in Ordnung" anheftet. */
            if (rule->checksum_id) {
                const bool gut = (tr->crc16 == rule->checksum_expect);
                tr->verify_kind = UFT_RECIPE_VERIFY_CRC16_CCITT;
                tr->checksum_state = gut ? UFT_RECIPE_CHECK_OK
                                         : UFT_RECIPE_CHECK_FAILED;
                tr->checksum_ok = gut;
                if (!gut) { st = UFT_R_ECHECKSUM; goto released; }
            } else if (rule->require_exact_raw_length && rule->raw_min_bytes) {
                /* Die Laengenprobe ist oben gelaufen; wer hierher kommt,
                 * hat sie bestanden. Eine Pruefsumme wurde dabei NICHT
                 * verglichen, also bleibt `checksum_state` auf NONE. */
                tr->verify_kind = UFT_RECIPE_VERIFY_EXACT_SIZE;
                tr->checksum_state = UFT_RECIPE_CHECK_NONE;
                tr->checksum_ok = false;
            } else {
                tr->verify_kind = UFT_RECIPE_VERIFY_NONE;
                tr->checksum_state = UFT_RECIPE_CHECK_NONE;
                tr->checksum_ok = false;
            }
            tr->retried = (attempt > 0u);
            if (rule->output_mode != UFT_RECIPE_OUTPUT_ANALYSIS_ONLY && sink && sink->write) {
                size_t offset;
                if (rule->output_mode == UFT_RECIPE_OUTPUT_APPEND) {
                    offset = *append_offset;
                    if (n > SIZE_MAX - offset) { st = UFT_R_EBOUNDS; goto released; }
                } else {
                    int delta = logical - rule->first_track;
                    if (!rule->track_step || delta % rule->track_step) {
                        st = UFT_R_EBOUNDS; goto released;
                    }
                    int index = delta / rule->track_step;
                    if (index < 0 || (n && (size_t)index >
                        (SIZE_MAX - rule->output_offset) / n)) {
                        st = UFT_R_EBOUNDS; goto released;
                    }
                    offset = rule->output_offset + (size_t)index * n;
                }
                st = sink->write(sink->user, offset, decoded, n);
                if (st != UFT_R_OK) goto released;
                if (rule->output_mode == UFT_RECIPE_OUTPUT_APPEND) *append_offset += n;
            }
released:
            if (cap.data && sink && sink->write_capture &&
                (st == UFT_R_OK || rule->preserve_bad_capture)) {
                uft_recipe_status_t saved = sink->write_capture(
                    sink->user, logical, tr->cylinder, tr->head,
                    attempt, rev, &cap, st);
                if (st != UFT_R_OK && saved == UFT_R_OK)
                    tr->retained_bad_capture = true;
                if (st == UFT_R_OK && saved != UFT_R_OK) st = saved;
            }
            if (source->release) source->release(source->user, &cap);
            if (st == UFT_R_OK) goto done;
        }
        if (rule->error_policy != UFT_RECIPE_ERROR_RETRY && attempt == 0) break;
    }
done:
    free(decoded);
    return tr->status = st;
}

uft_recipe_status_t uft_recipe_run(
    const uft_recipe_engine_t *engine, const uft_imaging_recipe_t *recipe,
    const uft_recipe_variant_t *variant, const uft_recipe_source_t *source,
    const uft_recipe_sink_t *sink, uft_recipe_report_t *report)
{
    if (!engine || !recipe || !variant || !source || !source->read || !report)
        return UFT_R_EINVAL;
    memset(report, 0, sizeof(*report));
    report->recipe = recipe;
    report->variant = variant;
    if ((source->capabilities & recipe->required_capabilities) !=
        recipe->required_capabilities) return UFT_R_EUNSUPPORTED;

    /* Eine Regel, die schreiben soll, braucht eine Senke, die schreiben
     * kann. Ohne sie wurde der Schreibblock still uebersprungen, `st`
     * blieb UFT_R_OK, jede Spur galt als gelungen und der Bericht
     * meldete `complete: true` — bei null geschriebenen Byte. Ein
     * Erfolg ohne Tat; hier wird er abgesagt, bevor ein Laufwerk
     * anlaeuft. */
    if (!sink || !sink->write) {
        for (size_t ri = 0; ri < variant->rule_count; ++ri) {
            if (variant->rules[ri].output_mode !=
                UFT_RECIPE_OUTPUT_ANALYSIS_ONLY)
                return UFT_R_EINVAL;
        }
    }
    size_t append_offset = 0;
    uft_recipe_status_t final = UFT_R_OK;
    for (size_t ri = 0; ri < variant->rule_count; ++ri) {
        const uft_recipe_track_rule_t *rule = &variant->rules[ri];
        for (int t = rule->first_track;; t += rule->track_step) {
            uft_recipe_track_report_t tr;
            uft_recipe_status_t st = run_track(engine, recipe, rule, t, source,
                                               sink, &append_offset, &tr);
            uft_recipe_status_t add = append_report(report, &tr);
            if (add != UFT_R_OK) { final = add; goto done; }
            if (st != UFT_R_OK) {
                ++report->errors;
                final = st;
                if (rule->error_policy == UFT_RECIPE_ERROR_ABORT ||
                    rule->error_policy == UFT_RECIPE_ERROR_RETRY) goto done;
            } else if (tr.retried) {
                /* Lesbar, aber nicht beim ersten Anlauf. Bis hierher war
                 * `warnings` ein Feld, das niemand erhoeht — eine Zahl
                 * im Bericht, die immer 0 war. */
                ++report->warnings;
            }
            if (t == rule->last_track) break;
            /* Der naechste Schritt laege hinter last_track: das ist das
             * ENDE der Regel, kein Fehler.
             *
             * Vorher stand hier `final = UFT_R_EBOUNDS; goto done;`.
             * Gemessen an der Regel "Spur 0 bis 5, jede zweite" — dem
             * gewoehnlichen Fall 80-Spur-Laufwerk auf 40-Spur-Diskette —
             * las der Lauf alle drei Spuren richtig, meldete `errors=0`
             * und endete trotzdem mit `bounds_error` und
             * `complete=false`. Der Bericht widersprach sich selbst. */
            if ((rule->track_step > 0 && t > rule->last_track - rule->track_step) ||
                (rule->track_step < 0 && t < rule->last_track - rule->track_step))
                break;
        }
    }
done:
    report->output_bytes = append_offset;
    report->complete = final == UFT_R_OK && report->errors == 0;
    return final;
}

void uft_recipe_report_free(uft_recipe_report_t *r)
{
    if (!r) return;
    free(r->tracks);
    memset(r, 0, sizeof(*r));
}

typedef struct { char *p; size_t cap, pos; } json_out_t;
static void jadd(json_out_t *o, const char *s)
{
    size_t n = strlen(s);
    if (o->p && o->pos < o->cap) {
        size_t room = o->cap - o->pos - 1u;
        size_t copy = n < room ? n : room;
        memcpy(o->p + o->pos, s, copy);
        o->p[o->pos + copy] = 0;
    }
    o->pos += n;
}
static void jnum(json_out_t *o, uint64_t n)
{
    char b[32]; snprintf(b, sizeof(b), "%llu", (unsigned long long)n); jadd(o, b);
}
static void jstr(json_out_t *o, const char *s)
{
    jadd(o, "\"");
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; ++p) {
        char b[8];
        if (*p == '"' || *p == '\\') { b[0] = '\\'; b[1] = (char)*p; b[2] = 0; jadd(o,b); }
        else if (*p < 0x20) { snprintf(b,sizeof(b),"\\u%04x",*p); jadd(o,b); }
        else { b[0] = (char)*p; b[1] = 0; jadd(o,b); }
    }
    jadd(o, "\"");
}

size_t uft_recipe_report_json(const uft_recipe_report_t *r, char *buf, size_t cap)
{
    json_out_t o = { buf, cap, 0 };
    if (buf && cap) buf[0] = 0;
    if (!r) return 0;
    jadd(&o,"{\n  \"schema\":\"uft.imaging-recipe.report/1\",\n  \"recipe\":");
    jstr(&o, r->recipe ? r->recipe->id : "");
    jadd(&o,",\n  \"variant\":"); jstr(&o, r->variant ? r->variant->id : "");
    jadd(&o,",\n  \"complete\":"); jadd(&o,r->complete?"true":"false");
    jadd(&o,",\n  \"errors\":"); jnum(&o,r->errors);
    jadd(&o,",\n  \"warnings\":"); jnum(&o,r->warnings);
    jadd(&o,",\n  \"outputBytes\":"); jnum(&o,r->output_bytes);
    jadd(&o,",\n  \"tracks\":[");
    for (size_t i=0;i<r->track_count;++i) {
        const uft_recipe_track_report_t *t=&r->tracks[i];
        jadd(&o,i?",\n    {":"\n    {");
        jadd(&o,"\"track\":"); jnum(&o,(uint64_t)t->logical_track);
        jadd(&o,",\"cylinder\":"); jnum(&o,(uint64_t)t->cylinder);
        jadd(&o,",\"head\":"); jnum(&o,(uint64_t)t->head);
        jadd(&o,",\"attempts\":"); jnum(&o,t->attempts);
        jadd(&o,",\"status\":"); jstr(&o,uft_recipe_status_string(t->status));
        jadd(&o,",\"rawBytes\":"); jnum(&o,t->raw_bytes);
        jadd(&o,",\"decodedBytes\":"); jnum(&o,t->decoded_bytes);
        jadd(&o,",\"syncFound\":"); jadd(&o,t->sync_found?"true":"false");
        jadd(&o,",\"crc16\":"); jnum(&o,t->crc16);
        /* Drei Zustaende, nicht zwei: wer nur `checksumOk` liest, kann
         * "nicht geprueft" nicht von "geprueft und falsch" trennen. */
        jadd(&o,",\"verify\":");
        jstr(&o, t->verify_kind == UFT_RECIPE_VERIFY_CRC16_CCITT
                     ? "crc16-ccitt"
                     : t->verify_kind == UFT_RECIPE_VERIFY_EXACT_SIZE
                           ? "exact-size"
                           : t->verify_kind == UFT_RECIPE_VERIFY_DECODER
                                 ? "decoder"
                                 : "none");
        jadd(&o,",\"checksum\":");
        jstr(&o, t->checksum_state == UFT_RECIPE_CHECK_OK ? "ok"
                 : t->checksum_state == UFT_RECIPE_CHECK_FAILED ? "failed"
                 : "not-checked");
        jadd(&o,",\"retried\":"); jadd(&o,t->retried?"true":"false");
        jadd(&o,"}");
    }
    jadd(&o,r->track_count?"\n  ]\n}\n":"]\n}\n");
    return o.pos;
}

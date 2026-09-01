/**
 * @file uft_flux_decoder.c
 * @brief Universal Flux-to-Sector Decoder Implementation
 * @version 3.9.0
 * 
 * Decodes raw flux timing data into sector data.
 */

#include "uft/flux/uft_flux_decoder.h"
#include "uft/flux/uft_flux_histogram.h"
#include "uft/flux/uft_flux_sync_search.h"
#include "uft/flux/uft_dewarp.h"
#include "uft/uft_log.h"
#include "uft/formats/uft_amiga_syncs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * CRC Tables
 * ============================================================================ */

static uint16_t crc16_table[256];
static bool crc16_table_init = false;

static void init_crc16_table(void) {
    if (crc16_table_init) return;
    
    for (int i = 0; i < 256; i++) {
        uint16_t crc = (uint16_t)(i << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
        crc16_table[i] = crc;
    }
    crc16_table_init = true;
}

/* ============================================================================
 * Initialization Functions
 * ============================================================================ */

void flux_decoder_options_init(flux_decoder_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->encoding = FLUX_ENC_AUTO;
    opts->bitcell_ns = 0;
    opts->use_pll = true;
    opts->pll_gain = FLUX_PLL_GAIN;
    opts->keep_raw_bits = false;
    /* MF-471: kein Medienprofil = Verhalten wie bisher. */
    opts->media = UFT_MEDIA_UNKNOWN;
    opts->media_adjust_pct = 100.0;
}

void flux_pll_init(flux_pll_t *pll, double initial_period) {
    if (!pll) return;
    memset(pll, 0, sizeof(*pll));
    pll->period = initial_period;
    pll->phase = 0;
    pll->freq_gain = 0.02;
    pll->phase_gain = 0.5;
    pll->last_transition = 0;
}

void flux_decoded_track_init(flux_decoded_track_t *track) {
    if (!track) return;
    memset(track, 0, sizeof(*track));
}

void flux_decoded_track_free(flux_decoded_track_t *track) {
    if (!track) return;
    
    for (size_t i = 0; i < track->sector_count; i++) {
        free(track->sectors[i].data);
    }
    free(track->raw_bits);
    
    memset(track, 0, sizeof(*track));
}

/* ============================================================================
 * CRC Functions
 * ============================================================================ */

uint16_t flux_crc16_ccitt(const uint8_t *data, size_t len) {
    init_crc16_table();
    
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ data[i]];
    }
    return crc;
}

uint16_t flux_crc16_mfm(const uint8_t *data, size_t len) {
    /* MFM CRC includes 3 sync bytes (A1 A1 A1) in calculation */
    init_crc16_table();
    
    uint16_t crc = 0xFFFF;
    /* Include sync bytes */
    for (int i = 0; i < 3; i++) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ 0xA1];
    }
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ data[i]];
    }
    return crc;
}

/* ============================================================================
 * MFM Encoding/Decoding
 * ============================================================================ */

uint8_t flux_mfm_decode_byte(uint16_t mfm_word) {
    uint8_t result = 0;
    /* MFM: clock bits at odd positions, data at even positions */
    for (int i = 0; i < 8; i++) {
        if (mfm_word & (1 << (14 - i*2))) {
            result |= (1 << (7 - i));
        }
    }
    return result;
}

uint16_t flux_mfm_encode_byte(uint8_t data, bool prev_bit) {
    uint16_t result = 0;
    
    for (int i = 7; i >= 0; i--) {
        bool bit = (data >> i) & 1;
        bool clock = !bit && !prev_bit;
        
        result = (result << 1) | clock;
        result = (result << 1) | bit;
        
        prev_bit = bit;
    }
    
    return result;
}

uint8_t flux_fm_decode_byte(uint16_t fm_word) {
    uint8_t result = 0;
    /* FM: clock bit, then data bit */
    for (int i = 0; i < 8; i++) {
        if (fm_word & (1 << (14 - i*2))) {
            result |= (1 << (7 - i));
        }
    }
    return result;
}

/* ============================================================================
 * PLL-based Flux to Bitstream Conversion
 * ============================================================================ */

/* Two representations of "flux" meet here, and they are not the same thing
 * (MF-438).
 *
 * Every SCP-family reader in the tree produces ns INTERVALS — the time since
 * the previous transition — because that is how the SCP container stores them.
 * flux_raw_data_t::transitions holds cumulative TIMES: flux_to_bitstream()
 * computes `delta = time - prev_time`. Handing intervals straight over yields
 * a stream of nonsense that decodes to nothing, which is precisely how it
 * presents: no sectors, no error.
 *
 * The conversion was previously hand-written at each call site
 * (tests/differential/uft_flux_decode.c has it, with a comment explaining the
 * trap). One fact, one place.
 *
 * Zero intervals are SCP overflow placeholders, not transitions, and are
 * skipped — dropping them is what makes the cumulative sum correct.
 *
 * The caller owns out->transitions and releases it with flux_raw_free().
 */
flux_status_t flux_raw_from_ns_intervals(const uint32_t *intervals,
                                         size_t count,
                                         flux_raw_data_t *out)
{
    return flux_raw_from_ns_intervals_indexed(intervals, count, 0, out);
}

flux_status_t flux_raw_from_ns_intervals_indexed(const uint32_t *intervals,
                                                 size_t count,
                                                 uint32_t revolution_ns,
                                                 flux_raw_data_t *out)
{
    if (!intervals || !out || count == 0) return FLUX_ERR_INVALID;

    uint32_t *trans = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!trans) return FLUX_ERR_OVERFLOW;

    size_t n = 0;
    uint64_t cum = 0;
    for (size_t i = 0; i < count; i++) {
        if (intervals[i] == 0) continue;      /* overflow placeholder */
        cum += intervals[i];
        trans[n++] = (uint32_t)cum;
    }
    if (n == 0) { free(trans); return FLUX_ERR_NO_DATA; }

    memset(out, 0, sizeof(*out));
    out->transitions      = trans;
    out->transition_count = n;
    out->sample_rate      = 1000000000u;      /* 1 GHz: one tick is one ns */

    /* Die gemessene Umdrehungsdauer wird nur uebernommen, wenn sie zum
     * Datenstrom passt (MF-475). `cum` ist die kumulierte Flusszeit, also die
     * Laenge des Stroms; liegt revolution_ns ausserhalb ±50 % davon, beschreibt
     * die Zahl etwas anderes als diesen Datensatz — mehrere Umdrehungen, einen
     * abgeschnittenen Rest, oder eine andere Zeitbasis. Zwei Marken zu setzen
     * hiesse dann, eine Struktur zu behaupten, die der Strom nicht hat. */
    if (revolution_ns > 0) {
        uint64_t lo = cum / 2, hi = cum + cum / 2;
        if ((uint64_t)revolution_ns >= lo && (uint64_t)revolution_ns <= hi) {
            uint32_t *idx = (uint32_t *)malloc(2 * sizeof(uint32_t));
            if (!idx) { free(trans); return FLUX_ERR_OVERFLOW; }
            idx[0] = 0;
            idx[1] = revolution_ns;
            out->index_times = idx;
            out->index_count = 2;
        }
    }
    return FLUX_OK;
}

/* Zeitachse umdrehen — Flippy-Rueckseite (MF-484).
 *
 * Eine Flippy-Diskette wurde beschrieben, indem man sie im einseitigen
 * Laufwerk umdrehte. Liest man sie spaeter im zweiseitigen Laufwerk vom
 * zweiten Kopf, laeuft dieselbe Spur RUECKWAERTS am Kopf vorbei: der
 * Datenstrom ist zeitlich gespiegelt, und kein Sync-Muster passt mehr.
 *
 * Wortgleiche Portierung von a8rawconv `reverse_track`
 * (`src/a8rawconv/disk.cpp:63-89`, GPL-2-or-later, Referenz-Orakel, wird
 * nicht gebaut):
 *
 *     max_time = max(letzte Indexzeit, letzte Uebergangszeit)
 *     t  ->  max_time - t        fuer Uebergaenge UND Indexmarken
 *     danach beide Folgen umdrehen
 *
 * Die Reihenfolge ist der Punkt: erst spiegeln, dann umdrehen. Das haelt
 * beide Folgen aufsteigend — worauf der ganze Messpfad seit MF-471 aufbaut
 * (`uft_media_rev_ns_from_index` weist eine nicht aufsteigende Indexreihe
 * ausdruecklich zurueck). Der Abstand zweier Indexmarken bleibt dabei
 * unveraendert, die gemessene Umdrehungsdauer ueberlebt die Spiegelung also.
 *
 * In-place, ohne Zuteilung: die Laengen aendern sich nicht.
 */
flux_status_t flux_raw_reverse(flux_raw_data_t *raw)
{
    if (!raw) return FLUX_ERR_INVALID;
    if (raw->transition_count == 0 && raw->index_count == 0)
        return FLUX_ERR_NO_DATA;

    uint32_t max_time = 0;
    if (raw->index_count > 0 && raw->index_times) {
        uint32_t last = raw->index_times[raw->index_count - 1];
        if (last > max_time) max_time = last;
    }
    if (raw->transition_count > 0 && raw->transitions) {
        uint32_t last = raw->transitions[raw->transition_count - 1];
        if (last > max_time) max_time = last;
    }

    if (raw->transitions) {
        for (size_t i = 0; i < raw->transition_count; i++)
            raw->transitions[i] = max_time - raw->transitions[i];
        for (size_t i = 0, j = raw->transition_count; i < j--; i++) {
            uint32_t t = raw->transitions[i];
            raw->transitions[i] = raw->transitions[j];
            raw->transitions[j] = t;
        }
    }
    if (raw->index_times) {
        for (size_t i = 0; i < raw->index_count; i++)
            raw->index_times[i] = max_time - raw->index_times[i];
        for (size_t i = 0, j = raw->index_count; i < j--; i++) {
            uint32_t t = raw->index_times[i];
            raw->index_times[i] = raw->index_times[j];
            raw->index_times[j] = t;
        }
    }
    return FLUX_OK;
}

void flux_raw_free(flux_raw_data_t *raw)
{
    if (!raw) return;
    free(raw->transitions);
    raw->transitions = NULL;
    raw->transition_count = 0;
    free(raw->index_times);
    raw->index_times = NULL;
    raw->index_count = 0;
}

flux_status_t flux_to_bitstream(const flux_raw_data_t *flux,
                                uint8_t *bits, size_t *bit_count,
                                double bitcell_ns, flux_pll_t *pll) {
    if (!flux || !bits || !bit_count || !pll) {
        return FLUX_ERR_INVALID;
    }
    
    /* Convert sample rate to nanoseconds per tick */
    double ns_per_tick = 1e9 / flux->sample_rate;
    
    size_t out_bits = 0;
    size_t max_bits = *bit_count;
    
    uint32_t prev_time = 0;
    
    for (size_t i = 0; i < flux->transition_count && out_bits < max_bits; i++) {
        uint32_t time = flux->transitions[i];
        uint32_t delta = time - prev_time;
        double delta_ns = delta * ns_per_tick;

        /* Apply accumulated phase as a one-shot timing correction
         * before discretising into cells. This is what makes
         * pll->phase observable: previously the integrator was
         * written every iteration but never read, leaving the loop a
         * pure P-controller on `period` only. With the consumption
         * here the loop becomes a full PI controller — phase tracks
         * sub-cell drift, period tracks long-term cell width. */
        if (pll->use_pll) {
            delta_ns -= pll->phase;
            if (delta_ns < 0.0) delta_ns = 0.0;
        }

        /* Calculate number of bit cells in this interval */
        double cells = delta_ns / pll->period;
        int num_cells = (int)(cells + 0.5);

        if (num_cells < 1) num_cells = 1;
        if (num_cells > 8) num_cells = 8;  /* Sanity limit */

        /* Output zeros for empty cells, then a one for the transition */
        for (int c = 0; c < num_cells - 1 && out_bits < max_bits; c++) {
            bits[out_bits / 8] &= ~(1 << (7 - (out_bits % 8)));
            out_bits++;
        }
        if (out_bits < max_bits) {
            bits[out_bits / 8] |= (1 << (7 - (out_bits % 8)));
            out_bits++;
        }

        /* PLL adjustment */
        if (pll->use_pll) {
            double expected = num_cells * pll->period;
            double error = delta_ns - expected;

            /* Phase: leaky integrator. Old phase decays at rate
             * phase_gain, new error integrates at the same rate.
             * Equivalent to:
             *   phase[n+1] = (1 - α) · phase[n] + α · error
             * with α = phase_gain. Stable for 0 < α < 1. */
            pll->phase = (1.0 - pll->phase_gain) * pll->phase
                       + pll->phase_gain * error;

            /* Bound phase to ±half a cell period to prevent runaway
             * when the input is grossly mistimed (e.g. corrupt flux
             * stream). Without this bound a single bad transition
             * could shift the next several iterations off the cell
             * grid and the loop never recovers. */
            double phase_bound = pll->period * 0.5;
            if (pll->phase >  phase_bound) pll->phase =  phase_bound;
            if (pll->phase < -phase_bound) pll->phase = -phase_bound;

            /* Frequency adjustment */
            pll->period += error * pll->freq_gain / num_cells;

            /* Clamp period to reasonable range */
            double min_period = bitcell_ns * 0.8;
            double max_period = bitcell_ns * 1.2;
            if (pll->period < min_period) pll->period = min_period;
            if (pll->period > max_period) pll->period = max_period;
        }

        prev_time = time;
    }
    
    *bit_count = out_bits;
    return FLUX_OK;
}

/* ============================================================================
 * Sync Pattern Finding
 * ============================================================================ */

int flux_find_sync(const uint8_t *bits, size_t bit_count,
                   uint16_t pattern, size_t start_pos) {
    if (!bits || bit_count < 16) return -1;
    
    uint16_t window = 0;
    
    for (size_t i = start_pos; i < bit_count; i++) {
        /* Shift in next bit */
        window = (window << 1) | ((bits[i / 8] >> (7 - (i % 8))) & 1);
        
        if (i >= start_pos + 15 && window == pattern) {
            return (int)(i - 15);
        }
    }
    
    return -1;
}

/* MF-453: ein Durchlauf fuer beliebig viele Muster.
 *
 * Das Schieberegister ist dasselbe wie in flux_find_sync(); neu ist nur, dass
 * pro Bitposition die ganze Liste geprueft wird. Damit ist der Treffer der
 * frueheste ueber alle Muster — bei N Einzelaufrufen bekaeme man den
 * fruehesten Treffer des ERSTEN Musters, was etwas anderes ist. */
int flux_find_sync_any(const uint8_t *bits, size_t bit_count,
                       const uint16_t *patterns, size_t pattern_count,
                       size_t start_pos, size_t *which) {
    if (which) *which = 0;
    if (!bits || !patterns || pattern_count == 0 || bit_count < 16) return -1;

    uint16_t window = 0;
    for (size_t i = start_pos; i < bit_count; i++) {
        window = (uint16_t)((window << 1) |
                            ((bits[i / 8] >> (7 - (i % 8))) & 1));
        if (i < start_pos + 15) continue;
        for (size_t p = 0; p < pattern_count; p++) {
            if (window == patterns[p]) {
                if (which) *which = p;
                return (int)(i - 15);
            }
        }
    }
    return -1;
}

/* ============================================================================
 * MFM Track Decoder
 * ============================================================================ */

/* MF-218: skip a run of consecutive MFM sync words (0x4489).
 *
 * flux_find_sync() returns the bit position of the FIRST 0x4489 of a
 * sync group. IBM System-34 MFM precedes every address mark with
 * THREE 0xA1 sync bytes (each the 0x4489 missing-clock word), so a
 * decoder that skips only one (the old `pos += 16`) lands on the
 * second 0xA1 instead of the address mark and bails with NO_SYNC.
 * This advances past every consecutive 0x4489 word and returns the
 * position of the first non-sync word — the address mark. It also
 * handles a 1- or 2-A1 prefix gracefully (Amiga / non-IBM), so it is
 * the right primitive regardless of sync count. */
/* MF-453: das Muster kommt als Parameter.
 *
 * Die Funktion verglich fest gegen MFM_SYNC_PATTERN. Solange der Decoder nur
 * 0x4489 suchte, war das dasselbe; sobald er auch 0x9521 oder 0xA245 findet,
 * wuerde ein Sync-Lauf aus zwei Custom-Syncs nicht uebersprungen und der
 * Decoder laese den zweiten Sync als Info-Long. Der IBM-Pfad ruft weiterhin
 * mit MFM_SYNC_PATTERN. */
static size_t mfm_skip_sync_run(const uint8_t *bits, size_t bit_count,
                                size_t first_sync_pos, uint16_t pattern) {
    size_t pos = first_sync_pos;
    while (pos + 16 <= bit_count) {
        uint16_t w = 0;
        for (int b = 0; b < 16; b++) {
            w = (uint16_t)((w << 1) |
                ((bits[(pos + b) / 8] >> (7 - ((pos + b) % 8))) & 1));
        }
        if (w != pattern) break;
        pos += 16;
    }
    return pos;
}

static flux_status_t decode_mfm_sector(const uint8_t *bits, size_t bit_count,
                                       size_t start_pos,
                                       flux_decoded_sector_t *sector,
                                       size_t *end_pos) {
    /* MF-218: skip the WHOLE sync run (A1 A1 A1), not just one word, so
     * `pos` lands on the address mark. */
    size_t pos = mfm_skip_sync_run(bits, bit_count, start_pos,
                                   MFM_SYNC_PATTERN);

    if (pos + 8 * 16 >= bit_count) return FLUX_ERR_UNDERFLOW;
    
    /* Read address mark and ID field: IDAM + C + H + S + N + CRC1 + CRC2 */
    uint8_t id_field[6];
    for (int i = 0; i < 6; i++) {
        uint16_t mfm_word = 0;
        for (size_t b = 0; b < 16 && pos < bit_count; b++, pos++) {
            mfm_word = (mfm_word << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
        }
        id_field[i] = flux_mfm_decode_byte(mfm_word);
    }
    
    /* Verify IDAM */
    if (id_field[0] != MFM_IDAM) {
        return FLUX_ERR_NO_SYNC;
    }
    
    sector->cylinder = id_field[1];
    sector->head = id_field[2];
    sector->sector = id_field[3];
    sector->size_code = id_field[4];
    sector->id_crc = (id_field[5] << 8);
    
    /* Read CRC high byte (already have low byte position) */
    uint16_t mfm_word = 0;
    for (size_t b = 0; b < 16 && pos < bit_count; b++, pos++) {
        mfm_word = (mfm_word << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
    }
    sector->id_crc |= flux_mfm_decode_byte(mfm_word);
    
    /* Verify ID CRC */
    uint16_t calc_crc = flux_crc16_mfm(id_field, 5);
    sector->id_crc_ok = (calc_crc == sector->id_crc);
    
    sector->id_position = (uint32_t)start_pos;
    
    /* Search for data sync (up to 43 bytes gap) */
    int data_sync = flux_find_sync(bits, bit_count, MFM_SYNC_PATTERN, pos);
    if (data_sync < 0 || (size_t)data_sync > pos + 43 * 16) {
        return FLUX_ERR_NO_DATA;
    }

    /* MF-218: skip the data field's full A1 A1 A1 sync run, same as the
     * ID field — `pos = data_sync + 16` skipped only one. */
    pos = mfm_skip_sync_run(bits, bit_count, (size_t)data_sync,
                            MFM_SYNC_PATTERN);
    sector->data_position = (uint32_t)data_sync;
    
    /* Read data address mark */
    mfm_word = 0;
    for (size_t b = 0; b < 16 && pos < bit_count; b++, pos++) {
        mfm_word = (mfm_word << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
    }
    uint8_t dam = flux_mfm_decode_byte(mfm_word);
    
    if (dam == MFM_DDAM) {
        sector->deleted = true;
    } else if (dam != MFM_DAM) {
        return FLUX_ERR_NO_DATA;
    }
    
    /* Read sector data */
    size_t data_size = flux_sector_size(sector->size_code);
    sector->data = malloc(data_size);
    if (!sector->data) return FLUX_ERR_OVERFLOW;
    sector->data_size = data_size;
    
    for (size_t i = 0; i < data_size && pos + 16 <= bit_count; i++) {
        mfm_word = 0;
        for (int b = 0; b < 16; b++, pos++) {
            mfm_word = (mfm_word << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
        }
        sector->data[i] = flux_mfm_decode_byte(mfm_word);
    }
    
    /* Read data CRC */
    uint8_t crc_bytes[2];
    for (size_t i = 0; i < 2 && pos + 16 <= bit_count; i++) {
        mfm_word = 0;
        for (int b = 0; b < 16; b++, pos++) {
            mfm_word = (mfm_word << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
        }
        crc_bytes[i] = flux_mfm_decode_byte(mfm_word);
    }
    sector->data_crc = (crc_bytes[0] << 8) | crc_bytes[1];
    
    /* Verify data CRC */
    uint8_t *crc_data = malloc(1 + data_size);
    if (crc_data) {
        crc_data[0] = dam;
        memcpy(crc_data + 1, sector->data, data_size);
        calc_crc = flux_crc16_mfm(crc_data, 1 + data_size);
        sector->data_crc_ok = (calc_crc == sector->data_crc);
        free(crc_data);
    }

    /* MF-218: report where this sector's data field ended so the
     * caller can resume the scan PAST it. Without this the caller
     * resumes at sync_pos+16 — inside this sector's own A1 run — and
     * re-decodes the same sector several times (duplicates). */
    if (end_pos) *end_pos = pos;
    return FLUX_OK;
}

/* Zellendauer bestimmen — fuer ALLE Decoder an einer Stelle (MF-475).
 *
 * Reihenfolge, absteigend nach Verlaesslichkeit:
 *   1. ausdruecklich vorgegebenes bitcell_ns — wer eine Zahl nennt, meint sie;
 *   2. Medienprofil + GEMESSENE Umdrehungsdauer aus den Index-Impulsen des
 *      Abbilds (MF-471);
 *   3. @p fallback_ns — der Nennwert, den der jeweilige Decoder annimmt.
 *
 * Stufe 2 ist der Fall, der ohne sie schweigend falsch laeuft: eine
 * Atari-Diskette (288 min^-1) in einem 300-min^-1-Laufwerk liefert Zellen, die
 * um 4 % kuerzer sind als ihr Nennwert.
 *
 * MF-471 hatte diese Reihenfolge nur in flux_decode_mfm(). Die vier anderen
 * Decoder — FM, GCR (C64 und Apple), AmigaDOS — trugen weiter ihre feste Zahl,
 * darunter ausgerechnet der FM-Pfad, fuer den das 288-min^-1-Profil gebaut
 * wurde. Ein Fakt an fuenf Stellen heisst: die Fassung, die laeuft, ist nicht
 * die bessere, sondern die zufaellig aufgerufene. Deshalb hier eine Funktion
 * und fuenf Aufrufer, jeder mit seinem eigenen Nennwert.
 *
 * Schlaegt die Messung fehl — zu wenige Index-Impulse, Abtastrate 0 — greift
 * Stufe 3. Keine halb gerechnete Zahl, die wie eine Messung aussaehe. */
static double flux_pick_bitcell_ns(const flux_raw_data_t *flux,
                                   const flux_decoder_options_t *opts,
                                   double fallback_ns)
{
    if (opts->bitcell_ns != 0) return opts->bitcell_ns;

    if (flux && opts->media != UFT_MEDIA_UNKNOWN) {
        double rev_ns = 0.0, cell_ns = 0.0;
        double pct = (opts->media_adjust_pct > 0.0) ? opts->media_adjust_pct
                                                    : 100.0;
        if (uft_media_rev_ns_from_index(flux->index_times, flux->index_count,
                                        flux->sample_rate, &rev_ns) &&
            uft_media_cell_ns_from_rev(opts->media, rev_ns, pct, &cell_ns)) {
            return cell_ns;
        }
    }

    /* Dritte Stufe: das Histogramm des Stroms selbst (MF-488).
     *
     * Ein MFM-Strom mit Zellendauer T traegt Abstaende von 2T, 3T und 4T.
     * Der erste Berg im Abstandshistogramm liegt also bei 2T — und das
     * braucht weder ein Medienprofil noch Index-Marken, nur die Daten.
     * Genau deshalb steht die Stufe HIER: sie greift, wo Stufe 2 nichts
     * hat, und ist trotzdem eine Messung und keine Annahme.
     *
     * `uft_flux_histogram_cell_ns_from_transitions()` liefert nur dann eine
     * Zahl, wenn das Histogramm auch wirklich wie MFM aussieht. Tut es das
     * nicht — GCR, Rauschen, zerschossener Strom —, faellt es durch auf den
     * Nennwert. Ein Schaetzer, der immer etwas sagt, waere schlimmer als
     * keiner.
     *
     * Der Feineinsteller (MF-480) gilt auch hier: er ist eine Aussage ueber
     * die Zellendauer, nicht ueber ihre Herkunft. */
    if (flux && flux->transitions && flux->transition_count > 0) {
        double hist_ns = 0.0;
        if (uft_flux_histogram_cell_ns_from_transitions(
                flux->transitions, flux->transition_count, &hist_ns)) {
            double pct = (opts->media_adjust_pct > 0.0) ? opts->media_adjust_pct
                                                        : 100.0;
            /* Das Histogramm rechnet in Abtastschritten, nicht in ns. Solange
             * jede Quelle im Baum 1 GHz meldet, ist der Faktor 1 — aber
             * `flux_to_bitstream()` skaliert seit jeher, und zwei Stellen mit
             * verschiedener Einheitenannahme sind eine Falle, die erst bei der
             * ersten Aufnahme mit anderer Abtastrate zuschlaegt. */
            double ns_per_tick = (flux->sample_rate > 0)
                                     ? (1e9 / flux->sample_rate) : 1.0;
            return hist_ns * ns_per_tick * (pct / 100.0);
        }
    }

    return fallback_ns;
}

/* Welche Marke traegt die Spur? (MF-768)
 *
 * Der Satz kommt aus UFT_AMIGA_SYNCS — fuenf Muster, drei davon benannt,
 * Herkunft im Kopf von include/uft/formats/uft_amiga_syncs.h. Kein neuer
 * Katalog, keine neue Zahl.
 *
 * ── Warum nicht „der erste Treffer" ──────────────────────────────────────
 *
 * Gesucht wird im ABSTANDSSTROM, und verschiedene Bitmuster erzeugen an
 * manchen Stellen dieselbe Abstandsfolge. Gemessen, je Strom mit acht
 * Wiederholungen einer Marke, Toleranz 0:
 *
 *              gesucht:  $4489  $9521  $A245  $448A
 *     Strom $9521           2      4      0      1
 *     Strom $4489           4      0      0      0
 *     Strom $4488           0      0      0      1     (kein Eintrag)
 *     Strom $A245           2      0      4      3
 *
 * Wer den ersten Treffer nimmt, benennt bei $A245 unter Umstaenden
 * $4489. Deshalb: der STAERKSTE Treffer gewinnt, und er braucht
 * mindestens zwei. Mit dieser Regel stimmen alle vier Zeilen — der
 * Strom $4488 (der in der Tabelle NICHT steht) faellt mit seinem einen
 * Streutreffer korrekt heraus.
 *
 * UNBELEGT und hiermit benannt: die Spanne bei $A245 ist duenn — 4 gegen
 * 3. Ob echte Aufnahmen sie halten, kann nur ein Korpus sagen. Die
 * Alternative, gar nicht zu benennen, ist gemessen schlechter.
 *
 * $448A ist KEIN Tippfehler und keine Verwechslung mit einem gekippten
 * Bit: die Vorlage fuehrt es als eigenen Eintrag in ihrer Suchschleife.
 * Das gekippte Bit waere $4488, und das steht nirgends.
 */
static bool marke_suchen(const flux_raw_data_t *flux,
                         uint16_t *out_marke, const char **out_name)
{
    *out_marke = 0;
    *out_name  = NULL;
    if (!flux || !flux->transitions || flux->transition_count < 64)
        return false;

    /* Abstaende in Nanosekunden bilden — die Suche rechnet in ns. */
    double ns_per_tick = (flux->sample_rate > 0)
                       ? (1e9 / (double)flux->sample_rate) : 1.0;
    size_t n = flux->transition_count - 1;
    uint32_t *iv = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!iv) return false;
    for (size_t i = 0; i < n; i++) {
        uint32_t d = flux->transitions[i + 1] - flux->transitions[i];
        iv[i] = (uint32_t)((double)d * ns_per_tick);
    }

    size_t bester = 0;
    size_t best_i = 0;
    for (size_t k = 0; k < UFT_AMIGA_SYNC_COUNT; k++) {
        uint16_t w[2] = { UFT_AMIGA_SYNCS[k].pattern,
                          UFT_AMIGA_SYNCS[k].pattern };
        uft_sync_pattern_t pat;
        if (!uft_sync_pattern_from_words(w, 2, &pat)) continue;

        uft_sync_hit_t hits[32];
        size_t t = uft_sync_search_intervals(iv, n, &pat, 0.0, hits, 32);
        if (t > bester) { bester = t; best_i = k; }
    }
    free(iv);

    /* Mindestens zwei Treffer — siehe die Zeile $4488 in der Tabelle oben. */
    if (bester < 2) return false;

    *out_marke = UFT_AMIGA_SYNCS[best_i].pattern;
    *out_name  = UFT_AMIGA_SYNCS[best_i].name;   /* darf NULL sein */
    return true;
}

/* Das Spurverdikt bilden — die EINE Stelle (MF-765).
 *
 * Vorher stand an fuenf Rueckgabestellen `return (sector_count > 0) ?
 * FLUX_OK : FLUX_ERR_NO_SYNC`. Jede Verfeinerung musste damit fuenfmal
 * gebaut werden, und MF-764 hat genau eine davon verfeinert, weil die
 * anderen vier ein anderes Bandmodell brauchen.
 *
 * Jetzt fuellt diese Funktion das Verdikt aus drei Feldern (Diagnose,
 * Folge, Reparierbarkeit) und leitet den Status daraus ab. Die fuenf
 * Stellen rufen sie; jede weitere Verfeinerung beruehrt eine Zeile.
 *
 * `bandmodell_passt` entscheidet der AUFRUFER. Das Histogrammodul ist
 * auf MFM gebaut (Abstaende 2T/3T/4T, drei Berge); ein FM-Strom traegt
 * zwei und kaeme mit `sicher == false` heraus — er wuerde als UNLESBAR
 * eingestuft, also eine gueltige Spur als defekt gemeldet. Falsch grob
 * ist besser als praezise falsch.
 *
 * `expected_sectors` bleibt 0 = UNBEKANNT: der Flussdekoder kennt die
 * Sollzahl nicht, sie steht in der OTDR-Konfiguration
 * (`floppy_otdr.c:113-140`, nach Plattformnamen). Sobald ein Aufrufer
 * sie mitgibt, greift X-Copys Klasse 1 — und zwar in BEIDE Richtungen,
 * wie das Handbuch sagt: „less or MORE than 11 sectors". */
static flux_status_t verdikt_bilden(flux_decoded_track_t *track,
                                    const flux_raw_data_t *flux,
                                    bool bandmodell_passt)
{
    uft_track_befunde_t b;
    memset(&b, 0, sizeof(b));

    if (track) {
        b.sector_count      = track->sector_count;
        b.bad_id_crc        = track->bad_id_crc;
        b.bad_header_format = track->bad_header_format;
        b.bad_data_crc      = track->bad_data_crc;
        b.missing_data      = track->missing_data;
    }
    b.expected_sectors = 0;      /* siehe oben: unbekannt, nicht null */

    /* Histogramm nur, wenn es hier ueberhaupt etwas aussagen kann. */
    if (bandmodell_passt && flux && flux->transitions &&
        flux->transition_count >= 8) {
        double ns_per_tick = (flux->sample_rate > 0)
                           ? (1e9 / (double)flux->sample_rate) : 1.0;
        size_t n = flux->transition_count - 1;
        uint32_t *iv = (uint32_t *)malloc(n * sizeof(uint32_t));
        if (iv) {
            for (size_t i = 0; i < n; i++) {
                uint32_t d = flux->transitions[i + 1] - flux->transitions[i];
                iv[i] = (uint32_t)((double)d * ns_per_tick);
            }
            uft_flux_hist_result_t h;
            memset(&h, 0, sizeof(h));
            uft_flux_histogram_analyze(iv, n, 100, &h);
            free(iv);

            b.histogramm_gueltig = true;
            b.histogramm_berge   = h.peak_count;
            b.histogramm_sicher  = h.confident;
        }
    } else if (bandmodell_passt) {
        /* Zu wenige Wechsel, um ein Histogramm zu bilden — das IST die
         * Aussage „keine Struktur", nicht ein fehlendes Ergebnis. */
        b.histogramm_gueltig = true;
        b.histogramm_berge   = 0;
    }

    /* MF-768: nur suchen, wenn nichts dekodiert wurde — bei erfolgreich
     * gelesenen Sektoren ist die Frage „welche Marke" beantwortet. */
    if (b.sector_count == 0)
        b.marke_gefunden = marke_suchen(flux, &b.marke, &b.marke_name);

    uft_track_verdikt_t v;
    uft_track_verdikt_bilden(&b, &v);
    if (track) track->verdikt = v;

    if (b.sector_count > 0) return FLUX_OK;
    switch (v.diagnose) {
    case UFT_DIAG_LEER:     return FLUX_ERR_UNFORMATTED;
    case UFT_DIAG_UNLESBAR: return FLUX_ERR_NOISE;
    default:                return FLUX_ERR_NO_SYNC;
    }
}

flux_status_t flux_decode_mfm(const flux_raw_data_t *flux,
                              flux_decoded_track_t *track,
                              const flux_decoder_options_t *opts) {
    if (!flux || !track) return FLUX_ERR_INVALID;

    flux_decoder_options_t default_opts;
    if (!opts) {
        flux_decoder_options_init(&default_opts);
        opts = &default_opts;
    }

    double bitcell_ns = flux_pick_bitcell_ns(flux, opts, FLUX_MFM_DD_BITCELL_NS);

    /* Allocate bitstream buffer */
    size_t max_bits = FLUX_MAX_TRACK_SIZE * 8;
    uint8_t *bits = calloc(max_bits / 8 + 1, 1);
    if (!bits) return FLUX_ERR_OVERFLOW;
    
    /* Initialize PLL */
    flux_pll_t pll;
    flux_pll_init(&pll, bitcell_ns);
    pll.use_pll = opts->use_pll;
    pll.freq_gain = opts->pll_gain;
    
    /* Convert flux to bitstream */
    size_t bit_count = max_bits;
    flux_status_t status = flux_to_bitstream(flux, bits, &bit_count, bitcell_ns, &pll);
    if (status != FLUX_OK) {
        free(bits);
        return status;
    }
    
    track->track_length_bits = (uint32_t)bit_count;
    track->detected_encoding = FLUX_ENC_MFM;
    track->avg_bitrate = 1e9 / pll.period;
    
    /* Find and decode sectors */
    size_t pos = 0;
    while (pos < bit_count && track->sector_count < FLUX_MAX_SECTORS) {
        /* Find next sync pattern */
        int sync_pos = flux_find_sync(bits, bit_count, MFM_SYNC_PATTERN, pos);
        if (sync_pos < 0) break;
        
        /* Try to decode sector */
        flux_decoded_sector_t *sector = &track->sectors[track->sector_count];
        memset(sector, 0, sizeof(*sector));
        
        size_t sector_end = 0;
        status = decode_mfm_sector(bits, bit_count, sync_pos, sector,
                                   &sector_end);
        if (status == FLUX_OK) {
            track->sector_count++;
            track->good_sectors++;
            if (!sector->id_crc_ok) track->bad_id_crc++;
            if (!sector->data_crc_ok) track->bad_data_crc++;
            /* MF-218: resume PAST the decoded sector's data field, not
             * at sync_pos+16 (which is still inside this sector's A1
             * run — that re-decoded the same sector 3+ times). */
            pos = (sector_end > (size_t)sync_pos + 16)
                      ? sector_end : (size_t)sync_pos + 16;
        } else {
            if (status == FLUX_ERR_NO_DATA) track->missing_data++;
            /* Decode failed at this sync — step past this one A1 word
             * and let flux_find_sync pick up the next candidate. */
            pos = (size_t)sync_pos + 16;
        }
    }
    
    /* Keep raw bits if requested */
    if (opts->keep_raw_bits) {
        track->raw_bits = bits;
        track->raw_bit_count = bit_count;
    } else {
        free(bits);
    }
    
    /* MF-764: nur hier verfeinert — siehe `no_sync_verfeinern`. */
    return verdikt_bilden(track, flux, /*bandmodell_passt=*/true);
}


/* ============================================================================
 * FM Track Decoder
 * ============================================================================ */

flux_status_t flux_decode_fm(const flux_raw_data_t *flux,
                             flux_decoded_track_t *track,
                             const flux_decoder_options_t *opts) {
    if (!flux || !track) return FLUX_ERR_INVALID;
    
    flux_decoder_options_t default_opts;
    if (!opts) {
        flux_decoder_options_init(&default_opts);
        default_opts.bitcell_ns = FLUX_FM_BITCELL_NS;
        opts = &default_opts;
    }
    
    double bitcell_ns = flux_pick_bitcell_ns(flux, opts, FLUX_FM_BITCELL_NS);
    
    /* Allocate bitstream buffer */
    size_t max_bits = FLUX_MAX_TRACK_SIZE * 8;
    uint8_t *bits = calloc(max_bits / 8 + 1, 1);
    if (!bits) return FLUX_ERR_OVERFLOW;
    
    flux_pll_t pll;
    flux_pll_init(&pll, bitcell_ns);
    /* MF-487: hier standen diese beiden Zeilen NICHT — als einziger der fuenf
     * Decoder. Die Folge ist groesser, als sie aussieht.
     *
     * `flux_pll_init()` beginnt mit `memset(pll, 0, sizeof(*pll))`, also ist
     * `use_pll` danach FALSE. Die uebrigen vier Decoder setzen es gleich
     * darauf aus `opts->use_pll` (Standard: true) — dieser hier nicht.
     * `flux_to_bitstream()` prueft `if (pll->use_pll)` an beiden
     * Regelstellen. Ergebnis: **der FM-Pfad lief ueberhaupt nie mit
     * Regelung.** Er zaehlte Zellen gegen eine feste Periode, und
     * `opts->pll_gain` war dort ohne jede Wirkung.
     *
     * Fuer FM-Medien ist das genau der falsche Pfad, um darauf zu
     * verzichten: Atari 810/1050 laufen mit 288 min^-1 und werden meist in
     * 300-min^-1-Laufwerken gelesen — 4 % Versatz, den eine abgeschaltete
     * Regelung nicht ausgleichen kann.
     *
     * Wieder derselbe Befund wie bei MF-475, MF-479 und MF-481: ein Fakt an
     * zwei Stellen, und es laeuft die zufaellig aufgerufene. */
    pll.use_pll = opts->use_pll;
    pll.freq_gain = opts->pll_gain;

    size_t bit_count = max_bits;
    flux_status_t status = flux_to_bitstream(flux, bits, &bit_count, bitcell_ns, &pll);
    if (status != FLUX_OK) {
        free(bits);
        return status;
    }
    
    track->track_length_bits = (uint32_t)bit_count;
    track->detected_encoding = FLUX_ENC_FM;
    track->avg_bitrate = 1e9 / pll.period;
    
    /* FM decoding - similar to MFM but with FM sync pattern */
    size_t pos = 0;
    while (pos < bit_count && track->sector_count < FLUX_MAX_SECTORS) {
        int sync_pos = flux_find_sync(bits, bit_count, FM_SYNC_PATTERN, pos);
        if (sync_pos < 0) break;
        
        /* FM sector decoding would go here - similar to MFM */
        /* For now, just note we found a sync */
        pos = sync_pos + 16;
    }
    
    free(bits);
    /* MF-765: bandmodell_passt=false — das Histogrammodul rechnet mit
     * MFM-Baendern; FM/GCR wuerden faelschlich als unlesbar gelten. */
    return verdikt_bilden(track, flux, /*bandmodell_passt=*/false);
}

/* ============================================================================
 * GCR Decoders (Stub implementations)
 * ============================================================================ */

/* C64 GCR tables */
static const uint8_t c64_gcr_decode[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 00-07 */
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  /* 08-0F */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  /* 10-17 */
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   /* 18-1F */
};

/* C64 sectors per track by speed zone */
static const int c64_sectors_per_track[40] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21, /* 1-17 */
    19,19,19,19,19,19,19,                                 /* 18-24 */
    18,18,18,18,18,18,                                     /* 25-30 */
    17,17,17,17,17,                                         /* 31-35 */
    17,17,17,17,17                                          /* 36-40 (extended) */
};

/* C64 speed zone + bitcell tables: see include/uft/uft_c64_gcr.h
 * (uft_c64_speed_zone, uft_c64_track_bitrate, uft_c64_bytes_per_track).
 * The previous duplicates here were dead code AND had inverted zone
 * numbers + off-by-one boundaries vs. the canonical definition. Removed.
 */

static uint8_t c64_gcr_decode_byte(const uint8_t *bits, size_t pos, size_t bit_count) {
    /* Decode two 5-bit GCR groups → one byte */
    if (pos + 10 > bit_count) return 0xFF;
    
    uint8_t hi_gcr = 0, lo_gcr = 0;
    for (int i = 0; i < 5; i++) {
        size_t bp = pos + i;
        hi_gcr = (hi_gcr << 1) | ((bits[bp / 8] >> (7 - (bp % 8))) & 1);
    }
    for (int i = 0; i < 5; i++) {
        size_t bp = pos + 5 + i;
        lo_gcr = (lo_gcr << 1) | ((bits[bp / 8] >> (7 - (bp % 8))) & 1);
    }
    
    uint8_t hi = c64_gcr_decode[hi_gcr & 0x1F];
    uint8_t lo = c64_gcr_decode[lo_gcr & 0x1F];
    if (hi == 0xFF || lo == 0xFF) return 0xFF;
    return (hi << 4) | lo;
}

/* Find C64 GCR sync: 10+ consecutive 1-bits */
static int c64_find_sync(const uint8_t *bits, size_t bit_count, size_t start) {
    int ones = 0;
    for (size_t i = start; i < bit_count; i++) {
        if ((bits[i / 8] >> (7 - (i % 8))) & 1) {
            ones++;
            if (ones >= 10) {
                /* Skip remaining 1-bits */
                while (i + 1 < bit_count && 
                       ((bits[(i+1) / 8] >> (7 - ((i+1) % 8))) & 1)) {
                    i++;
                }
                return (int)(i + 1);  /* First bit after sync */
            }
        } else {
            ones = 0;
        }
    }
    return -1;
}

static flux_status_t decode_c64_gcr_sector(const uint8_t *bits, size_t bit_count,
                                            size_t data_start, 
                                            flux_decoded_sector_t *sector) {
    size_t pos = data_start;
    
    /* Read header: 0x08, checksum, sector, track, id2, id1, 0x0F, 0x0F */
    uint8_t header[8];
    for (int i = 0; i < 8; i++) {
        header[i] = c64_gcr_decode_byte(bits, pos, bit_count);
        pos += 10;
        if (header[i] == 0xFF && i < 6) return FLUX_ERR_NO_SYNC;
    }
    
    if (header[0] != 0x08) return FLUX_ERR_NO_SYNC;  /* Not a header block */
    
    uint8_t hdr_checksum = header[1];
    sector->sector = header[2];
    sector->cylinder = header[3] > 0 ? header[3] - 1 : 0;  /* 1-based → 0-based */
    sector->head = 0;
    sector->size_code = 1;  /* 256 bytes */
    sector->id_position = (uint32_t)data_start;
    
    /* Verify header checksum: XOR of sector, track, id2, id1 */
    uint8_t calc_hdr = header[2] ^ header[3] ^ header[4] ^ header[5];
    sector->id_crc = hdr_checksum;
    sector->id_crc_ok = (calc_hdr == hdr_checksum);
    
    /* Find data block sync */
    int data_sync = c64_find_sync(bits, bit_count, pos);
    if (data_sync < 0) return FLUX_ERR_NO_DATA;
    pos = (size_t)data_sync;
    
    /* Read data block marker */
    uint8_t marker = c64_gcr_decode_byte(bits, pos, bit_count);
    pos += 10;
    if (marker != 0x07) return FLUX_ERR_NO_DATA;  /* Not a data block */
    
    sector->data_position = (uint32_t)pos;
    
    /* Read 256 data bytes */
    sector->data = malloc(256);
    if (!sector->data) return FLUX_ERR_OVERFLOW;
    sector->data_size = 256;
    
    uint8_t checksum = 0;
    for (int i = 0; i < 256; i++) {
        sector->data[i] = c64_gcr_decode_byte(bits, pos, bit_count);
        checksum ^= sector->data[i];
        pos += 10;
    }
    
    /* Read data checksum byte */
    uint8_t read_checksum = c64_gcr_decode_byte(bits, pos, bit_count);
    sector->data_crc = read_checksum;
    sector->data_crc_ok = (checksum == read_checksum);
    sector->deleted = false;
    
    return FLUX_OK;
}

flux_status_t flux_decode_gcr_c64(const flux_raw_data_t *flux,
                                  flux_decoded_track_t *track,
                                  const flux_decoder_options_t *opts) {
    if (!flux || !track) return FLUX_ERR_INVALID;
    
    flux_decoder_options_t default_opts;
    if (!opts) {
        flux_decoder_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* C64 default bitcell: zone 0 (tracks 1-17) = 4000ns */
    double bitcell_ns = flux_pick_bitcell_ns(flux, opts, 4000.0);
    
    /* Allocate bitstream buffer */
    size_t max_bits = FLUX_MAX_TRACK_SIZE * 8;
    uint8_t *bits = calloc(max_bits / 8 + 1, 1);
    if (!bits) return FLUX_ERR_OVERFLOW;
    
    flux_pll_t pll;
    flux_pll_init(&pll, bitcell_ns);
    pll.use_pll = opts->use_pll;
    pll.freq_gain = opts->pll_gain;
    
    size_t bit_count = max_bits;
    flux_status_t status = flux_to_bitstream(flux, bits, &bit_count, bitcell_ns, &pll);
    if (status != FLUX_OK) { free(bits); return status; }
    
    track->track_length_bits = (uint32_t)bit_count;
    track->detected_encoding = FLUX_ENC_GCR_C64;
    track->avg_bitrate = 1e9 / pll.period;
    
    /* Find and decode sectors */
    size_t pos = 0;
    while (pos < bit_count && track->sector_count < FLUX_MAX_SECTORS) {
        int sync_pos = c64_find_sync(bits, bit_count, pos);
        if (sync_pos < 0) break;
        
        flux_decoded_sector_t *sector = &track->sectors[track->sector_count];
        memset(sector, 0, sizeof(*sector));
        
        status = decode_c64_gcr_sector(bits, bit_count, (size_t)sync_pos, sector);
        if (status == FLUX_OK) {
            sector->bitrate = 1e9 / pll.period;
            track->sector_count++;
            track->good_sectors++;
            if (!sector->id_crc_ok) track->bad_id_crc++;
            if (!sector->data_crc_ok) track->bad_data_crc++;
            pos = sync_pos + 256 * 10;  /* Skip past data */
        } else {
            if (status == FLUX_ERR_NO_DATA) track->missing_data++;
            pos = sync_pos + 10;  /* Try next sync */
        }
    }
    
    if (opts->keep_raw_bits) {
        track->raw_bits = bits;
        track->raw_bit_count = bit_count;
    } else {
        free(bits);
    }
    
    /* MF-765: bandmodell_passt=false — das Histogrammodul rechnet mit
     * MFM-Baendern; FM/GCR wuerden faelschlich als unlesbar gelten. */
    return verdikt_bilden(track, flux, /*bandmodell_passt=*/false);
}

/* Apple II 6-and-2 GCR decode table (disk byte → 6-bit value) */
static const uint8_t apple_gcr62_decode[256] = {
    [0x96]=0x00,[0x97]=0x01,[0x9A]=0x02,[0x9B]=0x03,[0x9D]=0x04,[0x9E]=0x05,[0x9F]=0x06,
    [0xA6]=0x07,[0xA7]=0x08,[0xAB]=0x09,[0xAC]=0x0A,[0xAD]=0x0B,[0xAE]=0x0C,[0xAF]=0x0D,
    [0xB2]=0x0E,[0xB3]=0x0F,[0xB4]=0x10,[0xB5]=0x11,[0xB6]=0x12,[0xB7]=0x13,[0xB9]=0x14,
    [0xBA]=0x15,[0xBB]=0x16,[0xBC]=0x17,[0xBD]=0x18,[0xBE]=0x19,[0xBF]=0x1A,
    [0xCB]=0x1B,[0xCD]=0x1C,[0xCE]=0x1D,[0xCF]=0x1E,
    [0xD3]=0x1F,[0xD6]=0x20,[0xD7]=0x21,[0xD9]=0x22,[0xDA]=0x23,[0xDB]=0x24,
    [0xDC]=0x25,[0xDD]=0x26,[0xDE]=0x27,[0xDF]=0x28,
    [0xE5]=0x29,[0xE6]=0x2A,[0xE7]=0x2B,[0xE9]=0x2C,[0xEA]=0x2D,[0xEB]=0x2E,
    [0xEC]=0x2F,[0xED]=0x30,[0xEE]=0x31,[0xEF]=0x32,
    [0xF2]=0x33,[0xF3]=0x34,[0xF4]=0x35,[0xF5]=0x36,[0xF6]=0x37,[0xF7]=0x38,
    [0xF9]=0x39,[0xFA]=0x3A,[0xFB]=0x3B,[0xFC]=0x3C,[0xFD]=0x3D,[0xFE]=0x3E,[0xFF]=0x3F
};

/* Apple address prologue: D5 AA 96, data prologue: D5 AA AD */
#define APPLE_PROLOG1   0xD5
#define APPLE_PROLOG2   0xAA
#define APPLE_ADDR_P3   0x96
#define APPLE_DATA_P3   0xAD
#define APPLE_EPILOG1   0xDE
#define APPLE_EPILOG2   0xAA
#define APPLE_GCR_BITCELL_NS  4000.0

static uint8_t apple_read_byte(const uint8_t *bits, size_t pos, size_t bit_count) {
    if (pos + 8 > bit_count) return 0;
    uint8_t val = 0;
    for (int i = 0; i < 8; i++) {
        size_t bp = pos + i;
        val = (val << 1) | ((bits[bp / 8] >> (7 - (bp % 8))) & 1);
    }
    return val;
}

/* Find Apple 3-byte prologue sequence in bitstream */
static int apple_find_prologue(const uint8_t *bits, size_t bit_count, 
                                size_t start, uint8_t p3) {
    for (size_t i = start; i + 24 <= bit_count; i++) {
        uint8_t b1 = apple_read_byte(bits, i, bit_count);
        if (b1 == APPLE_PROLOG1) {
            uint8_t b2 = apple_read_byte(bits, i + 8, bit_count);
            if (b2 == APPLE_PROLOG2) {
                uint8_t b3 = apple_read_byte(bits, i + 16, bit_count);
                if (b3 == p3) return (int)i;
            }
        }
    }
    return -1;
}

static uint8_t apple_decode_44(const uint8_t *bits, size_t pos, size_t bit_count) {
    /* Apple 4-and-4 encoding: two bytes, even bits in first, odd in second */
    uint8_t b1 = apple_read_byte(bits, pos, bit_count);
    uint8_t b2 = apple_read_byte(bits, pos + 8, bit_count);
    return ((b1 << 1) | 1) & b2;
}

static flux_status_t decode_apple_gcr_sector(const uint8_t *bits, size_t bit_count,
                                              size_t addr_start,
                                              flux_decoded_sector_t *sector) {
    size_t pos = addr_start + 24;  /* Skip past D5 AA 96 */
    
    /* Address field: volume(4&4), track(4&4), sector(4&4), checksum(4&4) */
    uint8_t volume = apple_decode_44(bits, pos, bit_count); pos += 16;
    uint8_t track  = apple_decode_44(bits, pos, bit_count); pos += 16;
    uint8_t sect   = apple_decode_44(bits, pos, bit_count); pos += 16;
    uint8_t cksum  = apple_decode_44(bits, pos, bit_count); pos += 16;
    
    sector->cylinder = track;
    sector->head = 0;
    sector->sector = sect;
    sector->size_code = 1;  /* 256 bytes */
    sector->id_position = (uint32_t)addr_start;
    
    uint8_t calc_cksum = volume ^ track ^ sect;
    sector->id_crc = cksum;
    sector->id_crc_ok = (calc_cksum == cksum);
    
    /* Find data prologue: D5 AA AD */
    int data_start = apple_find_prologue(bits, bit_count, pos, APPLE_DATA_P3);
    if (data_start < 0 || (size_t)data_start > pos + 500 * 8) {
        return FLUX_ERR_NO_DATA;
    }
    pos = (size_t)data_start + 24;  /* Skip past D5 AA AD */
    sector->data_position = (uint32_t)data_start;
    
    /* Read 342 + 1 disk bytes (6-and-2 encoded) */
    uint8_t disk_bytes[343];
    for (int i = 0; i < 343; i++) {
        disk_bytes[i] = apple_read_byte(bits, pos, bit_count);
        pos += 8;
    }
    
    /* Decode 6-and-2: first XOR-decode the stream */
    uint8_t decoded[343];
    uint8_t prev = 0;
    for (int i = 0; i < 342; i++) {
        uint8_t val = apple_gcr62_decode[disk_bytes[i]];
        decoded[i] = val ^ prev;
        prev = decoded[i];
    }
    /* Checksum byte */
    uint8_t data_cksum = apple_gcr62_decode[disk_bytes[342]];
    sector->data_crc = data_cksum;
    sector->data_crc_ok = (prev == data_cksum);
    
    /* Reassemble 256 bytes from 6-and-2 */
    sector->data = malloc(256);
    if (!sector->data) return FLUX_ERR_OVERFLOW;
    sector->data_size = 256;
    
    for (int i = 0; i < 256; i++) {
        uint8_t lo2;
        /* Low 2 bits are in decoded[i % 86] */
        int aux_idx = i % 86;
        int shift = (i / 86) * 2;
        lo2 = (decoded[aux_idx] >> shift) & 0x03;
        /* Apple 6-and-2 stores each 2-bit group bit-REVERSED in the
         * auxiliary buffer — bit0 and bit1 are exchanged. Undo it, or
         * every data byte's low 2 bits come out swapped. */
        lo2 = (uint8_t)(((lo2 & 1) << 1) | ((lo2 >> 1) & 1));
        /* High 6 bits are in decoded[86 + i] */
        sector->data[i] = (decoded[86 + i] << 2) | lo2;
    }
    
    sector->deleted = false;
    return FLUX_OK;
}

flux_status_t flux_decode_gcr_apple(const flux_raw_data_t *flux,
                                    flux_decoded_track_t *track,
                                    const flux_decoder_options_t *opts) {
    if (!flux || !track) return FLUX_ERR_INVALID;
    
    flux_decoder_options_t default_opts;
    if (!opts) {
        flux_decoder_options_init(&default_opts);
        opts = &default_opts;
    }
    
    double bitcell_ns = flux_pick_bitcell_ns(flux, opts, APPLE_GCR_BITCELL_NS);
    
    size_t max_bits = FLUX_MAX_TRACK_SIZE * 8;
    uint8_t *bits = calloc(max_bits / 8 + 1, 1);
    if (!bits) return FLUX_ERR_OVERFLOW;
    
    flux_pll_t pll;
    flux_pll_init(&pll, bitcell_ns);
    pll.use_pll = opts->use_pll;
    pll.freq_gain = opts->pll_gain;
    
    size_t bit_count = max_bits;
    flux_status_t status = flux_to_bitstream(flux, bits, &bit_count, bitcell_ns, &pll);
    if (status != FLUX_OK) { free(bits); return status; }
    
    track->track_length_bits = (uint32_t)bit_count;
    track->detected_encoding = FLUX_ENC_GCR_APPLE;
    track->avg_bitrate = 1e9 / pll.period;
    
    /* Find and decode sectors */
    size_t pos = 0;
    while (pos < bit_count && track->sector_count < FLUX_MAX_SECTORS) {
        /* Find address prologue D5 AA 96 */
        int addr_pos = apple_find_prologue(bits, bit_count, pos, APPLE_ADDR_P3);
        if (addr_pos < 0) break;
        
        flux_decoded_sector_t *sector = &track->sectors[track->sector_count];
        memset(sector, 0, sizeof(*sector));
        
        status = decode_apple_gcr_sector(bits, bit_count, (size_t)addr_pos, sector);
        if (status == FLUX_OK) {
            sector->bitrate = 1e9 / pll.period;
            track->sector_count++;
            track->good_sectors++;
            if (!sector->id_crc_ok) track->bad_id_crc++;
            if (!sector->data_crc_ok) track->bad_data_crc++;
            pos = addr_pos + 343 * 8;  /* Skip past data */
        } else {
            if (status == FLUX_ERR_NO_DATA) track->missing_data++;
            pos = addr_pos + 24;  /* Skip prologue, try next */
        }
    }
    
    if (opts->keep_raw_bits) {
        track->raw_bits = bits;
        track->raw_bit_count = bit_count;
    } else {
        free(bits);
    }
    
    /* MF-765: bandmodell_passt=false — das Histogrammodul rechnet mit
     * MFM-Baendern; FM/GCR wuerden faelschlich als unlesbar gelten. */
    return verdikt_bilden(track, flux, /*bandmodell_passt=*/false);
}

/* ============================================================================
 * Encoding Detection
 * ============================================================================ */

/* Kodierung erkennen — am VERHAELTNISMUSTER, nicht am Mittelwert (MF-766).
 *
 * Hier stand eine Einteilung nach dem MITTLEREN Wechselabstand, in vier
 * handgewaehlten Schwellen. Gemessen an den Konstanten dieses Baums
 * selbst (uft_flux_decoder.h:35-40), mit dem Wechselmuster JE Kodierung:
 *
 *     MFM HD  2T/3T/4T x1000ns    Mittel  2973 ns -> MFM       ok
 *     MFM DD  2T/3T/4T x2000ns    Mittel  6022 ns -> GCR_C64   FALSCH
 *     FM      1T/2T    x4000ns    Mittel  5993 ns -> GCR_C64   FALSCH
 *     GCR C64 Sync + 1..3T        Mittel  5598 ns -> GCR_C64   ok
 *     GCR Apple 1..3T  x4000ns    Mittel  7943 ns -> GCR_C64   FALSCH
 *
 * Drei von fuenf falsch, und die schwerste ist DD-MFM: die haeufigste
 * Diskette ueberhaupt (PC 720K, Amiga 880K, Atari ST) ging an den
 * Commodore-GCR-Dekoder. `GCR_C64` war der Auffangwert fuer alles ueber
 * 5000 ns und hat deshalb nur zufaellig gestimmt.
 *
 * Der Grund ist nicht schlechte Einstellung, sondern das falsche Mass:
 * MFM DD (6022), FM (5993) und GCR C64 (5598) liegen innerhalb von 8 %.
 * KEINE Schwellenwahl trennt die.
 *
 * Das VERHANDENE Histogramm (MF-488) trennt sie. Gemessen, dieselben
 * Stroeme:
 *
 *     MFM DD     3 Berge  4000 / 6000 / 8000    -> 2 : 3 : 4
 *     MFM HD     3 Berge  2000 / 3000 / 4000    -> 2 : 3 : 4
 *     FM         2 Berge  4000 / 8000           -> 1 : 2
 *     GCR C64    3 Berge  3200 / 6400 / 9600    -> 1 : 2 : 3, Zelle 3200
 *     GCR Apple  3 Berge  4000 / 8000 / 12000   -> 1 : 2 : 3, Zelle 4000
 *
 * Kein neues Verfahren und keine neue Zahl: das Muster kommt aus einem
 * vorhandenen Modul, die Zellendauern aus den Konstanten dieses Baums.
 *
 * Und wo das Muster zu nichts passt, sagt die Funktion jetzt AUTO —
 * „unentschieden" — statt zu raten. Fuer ein Werkzeug, das keine Daten
 * erfinden darf, ist eine ehrliche Nichtauskunft besser als eine
 * zuversichtliche falsche. */
flux_encoding_t flux_detect_encoding(const flux_raw_data_t *flux) {
    if (!flux || !flux->transitions || flux->transition_count < 100) {
        return FLUX_ENC_AUTO;
    }

    uft_flux_hist_result_t h;
    memset(&h, 0, sizeof(h));
    if (!uft_flux_histogram_analyze_transitions(flux->transitions,
                                                flux->transition_count,
                                                100, &h) ||
        h.peak_count < 2) {
        return FLUX_ENC_AUTO;
    }

    /* `h.confident` wird hier ABSICHTLICH nicht als Tor benutzt. Gemessen
     * ist es nur fuer MFM wahr (siehe Tabelle oben) — es als Bedingung zu
     * nehmen hiesse, FM und GCR gar nicht erst zuzulassen. Das ist
     * dieselbe Bandmodell-Falle wie in MF-765. */

    const double p0 = h.peaks[0].center_ns;
    if (p0 <= 0.0) return FLUX_ENC_AUTO;

    /* Die Toleranz ist eine SETZUNG, keine Messung. Die Zentren oben sind
     * an synthetischen Stroemen exakt; echte Aufnahmen zittern. Welcher
     * Wert reale Aufnahmen traegt, kann nur ein Korpus sagen — bis dahin
     * ist 12 % gewaehlt, weil er die gemessenen Muster klar trennt
     * (naechster Nachbar: 1,5 gegen 2,0, also 33 % Abstand). */
    const double TOL = 0.12;
    #define NAH(a,b) (fabs((a) - (b)) <= (b) * TOL)

    const double r1 = h.peaks[1].center_ns / p0;

    if (h.peak_count == 2) {
        /* 1 : 2 -> FM. Die Zelle ist der erste Berg. */
        if (NAH(r1, 2.0)) return FLUX_ENC_FM;
        return FLUX_ENC_AUTO;
    }

    /* Ab hier gilt: GENAU drei Berge. Das ist kein Feinschliff — die
     * erste Fassung liess `>= 3` zu und nahm die ersten drei. Gemessen
     * an einem gleichverteilten Rauschstrom (1500..8500 ns), der acht
     * Berge traegt, sassen drei davon zufaellig bei 1 : 1,5 : 2 — und
     * die Erkennung meldete MFM fuer reines Rauschen.
     *
     * Eine Spur mit acht Baendern hat keine Bandstruktur. Ob echte
     * Aufnahmen mit ihrem Zittern hier zu streng behandelt werden,
     * kann nur ein Korpus sagen; die Gegenrichtung ist gemessen und
     * falsch. */
    if (h.peak_count != 3) return FLUX_ENC_AUTO;

    const double r2 = h.peaks[2].center_ns / p0;

    /* 2 : 3 : 4 -> MFM. Vom ersten Berg aus: 1 : 1,5 : 2. */
    if (NAH(r1, 1.5) && NAH(r2, 2.0)) {
        return FLUX_ENC_MFM;
    }

    /* 1 : 2 : 3 -> GCR. Welches, entscheidet die Zellendauer gegen die
     * Konstanten dieses Baums (FLUX_GCR_C64_BITCELL_NS 3200,
     * FLUX_GCR_APPLE_BITCELL_NS 4000) — nicht gegen eine neue Zahl. */
    if (NAH(r1, 2.0) && NAH(r2, 3.0)) {
        double d_c64   = fabs(p0 - (double)FLUX_GCR_C64_BITCELL_NS);
        double d_apple = fabs(p0 - (double)FLUX_GCR_APPLE_BITCELL_NS);
        return (d_c64 <= d_apple) ? FLUX_ENC_GCR_C64 : FLUX_ENC_GCR_APPLE;
    }

    #undef NAH

    /* Kein bekanntes Muster. Das ist eine AUSSAGE, keine Luecke: eine
     * beschaedigte oder geschuetzte Spur traegt oft gar kein sauberes
     * Verhaeltnis, und sie dann in den haeufigsten Zweig zu schieben war
     * genau der Fehler, den diese Funktion vorher machte. */
    return FLUX_ENC_AUTO;
}

/* ============================================================================
 * Main Decoder Entry Point
 * ============================================================================ */

/* ============================================================================
 * Amiga (AmigaDOS trackdisk) MFM Decoder
 * ============================================================================ */

#define AMIGA_CKSUM_MASK    0x55555555u
#define AMIGA_SECTOR_BYTES  512
/* info(4)+label(16)+hdrck(4)+datack(4)+data(512), each odd+even encoded
 * => 2*(4+16+4+4+512) = 1080 raw MFM bytes after the sync run, and a
 * raw MFM byte is 8 flux cells (the odd/even scheme has no clock-strip
 * step) => 8640 raw cells of payload per sector. */
#define AMIGA_PAYLOAD_CELLS  (1080 * 8)

/* MF-452: obere Grenzen fuer das Info-Long, abgeleitet aus der Hardware statt
 * aus dem haeufigsten Fall.
 *
 * AMIGA_MAX_SECTOR 21 -> HD (22 Sektoren, 0..21). DD nutzt 0..10; ein
 * DD-Sektor mit Nummer 15 ist damit nicht mehr per Grenze ausgeschlossen,
 * sondern faellt ueber die Header-Pruefsumme durch — was der richtige
 * Mechanismus dafuer ist.
 *
 * AMIGA_MAX_TRACK 167 -> 84 Zylinder x 2 Seiten. Amiga-Laufwerke erreichen
 * physisch 83..84 Zylinder; X-Copy steppt in `track0` bis 83. */
#define AMIGA_MAX_SECTOR    21
#define AMIGA_MAX_TRACK     167

/* Read 8 raw MFM cells at bit position `pos` as one byte.
 *
 * Unlike IBM MFM there is NO separate clock-strip step here: in the
 * Amiga odd/even scheme the data bits sit directly at the 0x55
 * positions of the RAW cells, and the odd/even merge consumes the raw
 * bytes as-is (see amiga_read_field). flux_mfm_decode_byte() must NOT
 * be applied — that would strip a layer the scheme does not have. */
static uint8_t amiga_read_raw_byte(const uint8_t *bits, size_t bit_count,
                                   size_t pos) {
    uint8_t r = 0;
    for (int b = 0; b < 8 && pos + (size_t)b < bit_count; b++)
        r = (uint8_t)((r << 1) |
            ((bits[(pos + b) / 8] >> (7 - ((pos + b) % 8))) & 1));
    return r;
}

/* Read an Amiga odd/even-split field of `nbytes` decoded bytes.
 *
 * On disk the field is `nbytes` raw MFM bytes carrying the ODD data
 * bits, immediately followed by `nbytes` raw MFM bytes carrying the
 * EVEN data bits. Each output byte = ((odd & 0x55) << 1) | (even & 0x55).
 * `pos` is advanced past the whole field (2*nbytes raw MFM bytes = 16
 * raw cells per output byte).
 *
 * If `csum` is non-NULL the field's raw MFM bytes are folded into the
 * running Amiga checksum (XOR of the big-endian raw MFM longs); the
 * caller masks with 0x55555555 at the end. The checksum FIELDS
 * themselves pass csum=NULL — a checksum does not checksum itself. */
static void amiga_read_field(const uint8_t *bits, size_t bit_count,
                             size_t *pos, size_t nbytes,
                             uint8_t *out, uint32_t *csum) {
    for (int half = 0; half < 2; half++) {          /* 0 = odd, 1 = even */
        uint32_t acc = 0;
        int      acc_n = 0;
        for (size_t j = 0; j < nbytes; j++) {
            uint8_t rb = amiga_read_raw_byte(bits, bit_count, *pos);
            *pos += 8;
            if (half == 0) out[j]  = (uint8_t)((rb & 0x55) << 1);
            else           out[j] |= (uint8_t)(rb & 0x55);
            if (csum) {
                acc = (acc << 8) | rb;
                if (++acc_n == 4) { *csum ^= acc; acc = 0; acc_n = 0; }
            }
        }
        if (csum && acc_n) {                         /* nbytes not a /4 */
            acc <<= 8 * (4 - acc_n);
            *csum ^= acc;
        }
    }
}

static flux_status_t decode_amiga_sector(const uint8_t *bits, size_t bit_count,
                                         size_t sync_pos, uint16_t sync_pattern,
                                         flux_decoded_sector_t *sector,
                                         size_t *end_pos) {
    /* Skip the sync run — Amiga writes two; mfm_skip_sync_run tolerates one or
     * more and lands on the info field. Das Muster ist das, das gefunden
     * wurde, nicht zwangslaeufig 0x4489 (MF-453). */
    size_t pos = mfm_skip_sync_run(bits, bit_count, sync_pos, sync_pattern);
    if (pos + (size_t)AMIGA_PAYLOAD_CELLS > bit_count)
        return FLUX_ERR_UNDERFLOW;

    uint8_t  info[4], label[16], hchk[4], dchk[4];
    uint32_t hdr_csum = 0, data_csum = 0;

    amiga_read_field(bits, bit_count, &pos,  4, info,  &hdr_csum);
    amiga_read_field(bits, bit_count, &pos, 16, label, &hdr_csum);
    amiga_read_field(bits, bit_count, &pos,  4, hchk,  NULL);
    amiga_read_field(bits, bit_count, &pos,  4, dchk,  NULL);

    /* info long = [0xFF][track][sector][sectors-to-gap].
     *
     * MF-452: die Grenzen standen auf `track > 159 || sec > 10` — richtig fuer
     * eine 80-Zylinder-DD-Diskette und falsch fuer alles andere, was AmigaDOS
     * kennt:
     *
     *   - HD hat 22 Sektoren pro Spur (0..21). Der Sektorpfad kann das laengst
     *     (uft_adf_plugin.c fuehrt "HD variant (1760 KB)" als SUPPORTED,
     *     uft_adf_parser_v2.c kennt ADF_SIZE_HD) — der Fluxpfad verwarf jeden
     *     Sektor ab Nummer 11, also lieferte SCP->ADF einer HD-Diskette nichts.
     *   - 82 Zylinder sind auf Amiga-Laufwerken normal; X-Copy erlaubt
     *     `endtrack DC.W 79 ; 0 - 81` und steppt bis 83. Track = cyl*2+head,
     *     also bis 163. Genau dort liegen Zusatzkapazitaet und Kopierschutz.
     *
     * Die Obergrenzen sind jetzt das, was das Feld physisch tragen kann bzw.
     * was ein Laufwerk erreichen kann — nicht das, was die haeufigste Diskette
     * hat. Ein Sektor mit unmoeglichem Info-Long wird weiterhin verworfen. */
    /* MF-454: FLUX_ERR_BAD_HEADER, nicht FLUX_ERR_NO_SYNC. Der Sync war da —
     * was dahinter steht, passt nicht. X-Copy trennt diese beiden Faelle seit
     * 1992 (Code 2 gegen Code 5, xcop.s:1165/1171), UFT warf sie zusammen. */
    if (info[0] != 0xFF) return FLUX_ERR_BAD_HEADER;
    uint8_t track = info[1], sec = info[2];
    if (track > AMIGA_MAX_TRACK || sec > AMIGA_MAX_SECTOR)
        return FLUX_ERR_BAD_HEADER;

    sector->cylinder    = track / 2;
    sector->head        = track % 2;
    sector->sector      = sec;
    sector->size_code   = 2;                 /* 512 bytes */
    sector->id_position = (uint32_t)sync_pos;

    /* MF-452: das Label wird behalten. Vorher stand hier `(void)label;` — 16
     * Byte, die von der Diskette gelesen, in die Header-Pruefsumme gerechnet
     * und dann fallen gelassen wurden. */
    memcpy(sector->label, label, sizeof(sector->label));
    sector->label_present = true;

    uint32_t hchk_stored = ((uint32_t)hchk[0] << 24) | ((uint32_t)hchk[1] << 16)
                         | ((uint32_t)hchk[2] <<  8) |  (uint32_t)hchk[3];
    sector->id_crc    = hchk_stored;
    sector->id_crc_ok = (hchk_stored == (hdr_csum & AMIGA_CKSUM_MASK));

    sector->data = malloc(AMIGA_SECTOR_BYTES);
    if (!sector->data) return FLUX_ERR_OVERFLOW;
    sector->data_size     = AMIGA_SECTOR_BYTES;
    sector->data_position = (uint32_t)pos;

    amiga_read_field(bits, bit_count, &pos, AMIGA_SECTOR_BYTES,
                     sector->data, &data_csum);

    uint32_t dchk_stored = ((uint32_t)dchk[0] << 24) | ((uint32_t)dchk[1] << 16)
                         | ((uint32_t)dchk[2] <<  8) |  (uint32_t)dchk[3];
    sector->data_crc    = dchk_stored;
    sector->data_crc_ok = (dchk_stored == (data_csum & AMIGA_CKSUM_MASK));
    sector->deleted     = false;

    if (end_pos) *end_pos = pos;
    return FLUX_OK;
}

/* Decode AmigaDOS sectors out of an already-recovered BITSTREAM (MF-437).
 *
 * Split out of flux_decode_amiga() so callers that already hold a bitstream
 * do not have to go through flux capture and a PLL to reach it. An HFE image
 * IS a bitstream — the container stores recovered cells, not flux — so the
 * conversion path has no business synthesising flux just to decode it.
 *
 * Factored, not copied: flux_decode_amiga() calls straight into this after
 * flux_to_bitstream(), so there remains exactly one AmigaDOS sector decoder.
 */
flux_status_t flux_decode_amiga_bits(const uint8_t *bits, size_t bit_count,
                                     flux_decoded_track_t *track,
                                     const flux_decoder_options_t *opts)
{
    if (!bits || !track) return FLUX_ERR_INVALID;

    flux_decoder_options_t default_opts;
    if (!opts) {
        flux_decoder_options_init(&default_opts);
        opts = &default_opts;
    }

    track->track_length_bits = (uint32_t)bit_count;
    track->detected_encoding = FLUX_ENC_AMIGA;

    /* MF-453: welche Syncs gesucht werden, entscheidet der Aufrufer. Ohne
     * Angabe bleibt es beim Standard — der Default aendert nichts. */
    static const uint16_t default_syncs[] = { MFM_SYNC_PATTERN };
    const uint16_t *syncs = opts->sync_patterns;
    size_t          nsyncs = opts->sync_count;
    if (!syncs || nsyncs == 0) { syncs = default_syncs; nsyncs = 1; }

    size_t pos = 0;
    while (pos < bit_count && track->sector_count < FLUX_MAX_SECTORS) {
        size_t hit = 0;
        int sync_pos = flux_find_sync_any(bits, bit_count, syncs, nsyncs,
                                          pos, &hit);
        if (sync_pos < 0) break;

        flux_decoded_sector_t *sector = &track->sectors[track->sector_count];
        memset(sector, 0, sizeof(*sector));

        size_t sector_end = 0;
        flux_status_t status = decode_amiga_sector(bits, bit_count,
                                                   (size_t)sync_pos,
                                                   syncs[hit],
                                                   sector, &sector_end);
        if (status == FLUX_OK) {
            track->sector_count++;
            track->good_sectors++;
            if (!sector->id_crc_ok)   track->bad_id_crc++;
            if (!sector->data_crc_ok) track->bad_data_crc++;
            pos = (sector_end > (size_t)sync_pos + 16)
                      ? sector_end : (size_t)sync_pos + 16;
        } else {
            if (status == FLUX_ERR_NO_DATA)    track->missing_data++;
            if (status == FLUX_ERR_BAD_HEADER) track->bad_header_format++;

            /* MF-454: ueber den GANZEN Sync-Lauf hinweg, nicht ein Wort weit.
             *
             * `pos = sync_pos + 16` liess den zweiten Sync des Amiga-Paares
             * stehen; der wurde beim naechsten Durchlauf gefunden, fuehrte auf
             * denselben kaputten Kopf und zaehlte ein zweites Mal. Ein Sektor,
             * zwei Fehler — der Zaehler haette Sync-Kandidaten gezaehlt statt
             * Sektoren. */
            size_t after = mfm_skip_sync_run(bits, bit_count,
                                             (size_t)sync_pos, syncs[hit]);
            pos = (after > (size_t)sync_pos) ? after : (size_t)sync_pos + 16;
        }
    }

    /* Ownership: the bitstream belongs to the CALLER. This function never
     * frees it and never stores it in track->raw_bits — flux_decode_amiga()
     * owns the buffer it allocated and decides that, and a caller passing a
     * plugin's raw_data must not have it freed under them. */
    /* MF-765: hier gibt es GAR KEINE Flussdaten — diese Funktion bekommt
     * einen fertigen Bitstrom. Ohne Wechsel-Abstaende laesst sich kein
     * Histogramm bilden; das Verdikt kann hier nur aus den Zaehlern
     * kommen. Das ist keine Nachlaessigkeit, sondern die Ebene: wer Bits
     * hereinreicht, hat die Zeitinformation schon weggeworfen. */
    return verdikt_bilden(track, NULL, /*bandmodell_passt=*/false);
}

/**
 * Zellendauer aus den Sync-Marken selbst messen (MF-492).
 *
 * Sucht die konfigurierten Sync-Muster direkt im Flussstrom — ohne PLL,
 * ohne Bitstrom, ohne die Zellendauer, die gerade in Frage steht. Jeder
 * Fund bringt seine eigene Taktschaetzung mit; zurueck kommt der Median,
 * weil ein einzelner Fund an einer verwackelten Stelle sonst die ganze Spur
 * verzoege.
 *
 * @return Zellendauer in ns, oder 0 wenn zu wenige Marken gefunden wurden.
 */
static double amiga_measured_cell_ns(const flux_raw_data_t *flux,
                                     const flux_decoder_options_t *opts)
{
    if (!flux || !flux->transitions || flux->transition_count < 32) return 0.0;

    static const uint16_t default_syncs[] = { MFM_SYNC_PATTERN };
    const uint16_t *syncs = opts->sync_patterns;
    size_t nsyncs = opts->sync_count;
    if (!syncs || nsyncs == 0) { syncs = default_syncs; nsyncs = 1; }

    enum { MAX_HITS = 64, MIN_HITS = 3 };
    uft_sync_hit_t hits[MAX_HITS];
    size_t n = 0;

    for (size_t s = 0; s < nsyncs && n < MIN_HITS; s++) {
        /* Das Amiga-Muster ist ein PAAR gleicher Sync-Woerter — ein einzelnes
         * Wort ergibt zu wenige Abstaende, um im Rauschen zu bestehen. */
        const uint16_t pair[2] = { syncs[s], syncs[s] };
        uft_sync_pattern_t pat;
        if (!uft_sync_pattern_from_words(pair, 2, &pat)) continue;

        n = uft_sync_search_transitions(flux->transitions,
                                        flux->transition_count, &pat,
                                        UFT_SYNC_DEFAULT_TOL,
                                        hits, MAX_HITS);
    }
    /* Unter drei Funden ist es keine Mehrheit, sondern ein Zufall. */
    if (n < MIN_HITS) return 0.0;

    double ns_per_tick = (flux->sample_rate > 0) ? (1e9 / flux->sample_rate)
                                                 : 1.0;
    return uft_sync_median_clock(hits, n) * ns_per_tick;
}

/** Ein Dekodier-Durchlauf mit fest vorgegebener Zellendauer. */
static flux_status_t amiga_decode_at(const flux_raw_data_t *flux,
                                     flux_decoded_track_t *track,
                                     const flux_decoder_options_t *opts,
                                     double bitcell_ns)
{
    size_t max_bits = FLUX_MAX_TRACK_SIZE * 8;
    uint8_t *bits = calloc(max_bits / 8 + 1, 1);
    if (!bits) return FLUX_ERR_OVERFLOW;

    flux_pll_t pll;
    flux_pll_init(&pll, bitcell_ns);
    pll.use_pll = opts->use_pll;
    pll.freq_gain = opts->pll_gain;

    size_t bit_count = max_bits;
    flux_status_t status = flux_to_bitstream(flux, bits, &bit_count,
                                             bitcell_ns, &pll);
    if (status != FLUX_OK) { free(bits); return status; }

    flux_status_t rc = flux_decode_amiga_bits(bits, bit_count, track, opts);
    track->avg_bitrate = 1e9 / pll.period;

    if (opts->keep_raw_bits) {
        track->raw_bits = bits;              /* handed over to the track */
        track->raw_bit_count = bit_count;
    } else {
        free(bits);
    }
    return rc;
}

/**
 * Was ist ein Durchlauf wert?
 *
 * Ein Sektor mit heilen Pruefsummen wiegt schwerer als einer, den der
 * Dekoder nur gefunden hat. Ohne diese Gewichtung wuerde ein Durchlauf
 * gewinnen, der viele kaputte Sektoren meldet, gegen einen mit wenigen
 * heilen — und die Auswahl waere eine Zaehlung von Kandidaten statt von
 * lesbaren Daten.
 *
 * Ehrlich dazu: **kein Test unterscheidet diese Gewichtung von einer
 * blossen Sektorzaehlung.** Der Rotbeweis (Faktor 2 auf 0 gesetzt) kippt
 * nichts. Auf synthetischem Flux liefern beide Durchlaeufe dieselbe
 * Mischung aus heilen und kaputten Sektoren; ein Fall, in dem sich die
 * beiden Masse trennen, braucht eine reale beschaedigte Aufnahme. Die
 * Gewichtung steht hier als forensische Vorgabe — heile Daten schlagen
 * Kandidaten —, nicht als gemessene Notwendigkeit.
 */
static size_t amiga_track_score(const flux_decoded_track_t *t)
{
    size_t whole = 0;
    for (size_t i = 0; i < t->sector_count; i++)
        if (t->sectors[i].id_crc_ok && t->sectors[i].data_crc_ok) whole++;
    return whole * 2u + t->sector_count;
}

/**
 * Einen Kandidaten dekodieren und nur uebernehmen, wenn er besser ist.
 *
 * Jeder Kandidat bekommt eine EIGENE Spur. So kann keiner ein besseres
 * frueheres Ergebnis ueberschreiben: gewinnen ja, kosten nein.
 */
static flux_status_t amiga_try_candidate(const flux_raw_data_t *flux,
                                         flux_decoded_track_t *best,
                                         const flux_decoder_options_t *opts,
                                         double bitcell_ns,
                                         flux_timing_source_t source,
                                         flux_status_t rc_best)
{
    flux_decoded_track_t alt;
    flux_decoded_track_init(&alt);
    flux_status_t rc_alt = amiga_decode_at(flux, &alt, opts, bitcell_ns);

    if (amiga_track_score(&alt) > amiga_track_score(best)) {
        /* Die Messwerte gehoeren der SPUR, nicht dem Durchlauf — sie ueber
         * den Wechsel hinweg zu retten ist der ganze Zweck von MF-496. */
        alt.initial_cell_ns  = best->initial_cell_ns;
        alt.measured_cell_ns = best->measured_cell_ns;
        alt.warp_span        = best->warp_span;
        alt.timing_source    = source;
        alt.used_cell_ns     = bitcell_ns;

        flux_decoded_track_free(best);
        *best = alt;                     /* Besitz wandert mit, `sectors`
                                          * liegt inline in der Struktur */
        return rc_alt;
    }
    flux_decoded_track_free(&alt);
    return rc_best;
}

/**
 * Denselben Strom entzerrt als weiteren Kandidaten anbieten (MF-495).
 *
 * Wo die Diskette nicht ueberall gleich schnell lief, gibt es keinen
 * richtigen EINZELNEN Taktwert — dagegen hilft kein besserer Startwert,
 * sondern nur, den Gleichlauffehler herauszurechnen.
 *
 * Der entzerrte Strom lebt ausschliesslich hier und wird nie weiter-
 * gereicht: er traegt veraenderte Zeiten, und veraenderte Zeiten sind
 * veraenderte Rohdaten.
 */
static flux_status_t amiga_try_dewarped(const flux_raw_data_t *flux,
                                        flux_decoded_track_t *best,
                                        const flux_decoder_options_t *opts,
                                        double t0_ns, flux_status_t rc_best)
{
    const size_t n = flux->transition_count;
    if (!flux->transitions || n < 64 || !(t0_ns > 0.0)) return rc_best;

    const double ns_per_tick = (flux->sample_rate > 0)
                                   ? (1e9 / flux->sample_rate) : 1.0;

    uint32_t *iv = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!iv) return rc_best;

    /* Der Entzerrer rechnet auf ABSTAENDEN, `flux_raw_data_t` haelt
     * kumulierte Zeiten (MF-438). */
    uint32_t prev = 0;
    for (size_t i = 0; i < n; i++) {
        uint32_t t = flux->transitions[i];
        iv[i] = (t > prev) ? (t - prev) : 0u;
        prev = t;
    }

    uft_dewarp_result_t dr;
    if (!uft_dewarp_intervals(iv, n, t0_ns / ns_per_tick, 0.0, iv, &dr)) {
        free(iv);
        return rc_best;
    }

    /* Die gemessene Spanne gehoert in die Spur, auch wenn die Stufe gleich
     * wieder aussteigt: „ich habe nachgesehen und nichts gefunden" ist ein
     * anderer Befund als „ich habe nicht nachgesehen" (MF-496). */
    best->warp_span = dr.warp_span;

    /* Ohne nennenswerten Gleichlauffehler ist der entzerrte Strom derselbe
     * Strom — ein Durchlauf darauf waere reine Rechenzeit. */
    if (dr.warp_span < 1.02) { free(iv); return rc_best; }

    uint32_t acc = 0;
    for (size_t i = 0; i < n; i++) { acc += iv[i]; iv[i] = acc; }

    flux_raw_data_t dw = *flux;
    dw.transitions = iv;
    /* Die Index-Marken zeigen auf die urspruengliche Zeitachse und passen
     * nicht mehr. Sie mitzugeben hiesse, eine Umdrehungsdauer zu behaupten,
     * die dieser Strom nicht mehr hat. */
    dw.index_times = NULL;
    dw.index_count = 0;

    flux_status_t rc = amiga_try_candidate(&dw, best, opts,
                                           dr.ref_clock * ns_per_tick,
                                           FLUX_TIMING_DEWARPED, rc_best);
    free(iv);
    return rc;
}

flux_status_t flux_decode_amiga(const flux_raw_data_t *flux,
                                flux_decoded_track_t *track,
                                const flux_decoder_options_t *opts) {
    if (!flux || !track) return FLUX_ERR_INVALID;

    flux_decoder_options_t default_opts;
    if (!opts) {
        flux_decoder_options_init(&default_opts);
        opts = &default_opts;
    }

    double bitcell_ns = flux_pick_bitcell_ns(flux, opts, FLUX_MFM_DD_BITCELL_NS);
    flux_status_t rc = amiga_decode_at(flux, track, opts, bitcell_ns);
    track->initial_cell_ns = bitcell_ns;
    track->used_cell_ns    = bitcell_ns;
    track->timing_source   = FLUX_TIMING_INITIAL;

    /* Zweiter Durchlauf mit gemessener Zellendauer (MF-492).
     *
     * Die angenommene Zellendauer kann falsch sein, und dann rastet die PLL
     * nicht ein. Gemessen (FLUX-11): bei 0,85 der wahren Zellendauer und
     * 4 % Zittern kommen 0 von 11 Sektoren zurueck, obwohl alle 11 Marken
     * unveraendert im Strom stehen. Die Sync-Suche findet sie ohne PLL und
     * sagt, wie lang eine Zelle wirklich ist — damit ist der zweite Versuch
     * keine Rateschleife, sondern eine Messung.
     *
     * Der Ausloeser ist bewusst NICHT „null Sektoren gefunden": gerade die
     * teilweise lesbare Diskette ist der forensisch interessante Fall, und
     * eine Bedingung auf „gar nichts gefunden" haette dort nie ausgeloest.
     *
     * Was der zweite Durchlauf nachweislich bringt, ist eng (MF-494): auf
     * einer Spur mit 20 % Zittern findet er 7 Sektoren statt keinem — aber
     * keiner davon traegt eine heile Pruefsumme. Gefunden ist nicht
     * gerettet. Die Datenrettung leistet erst der dritte Kandidat unten.
     *
     * Der zweite Durchlauf gehoert aber ausschliesslich dem AUTOMATISCHEN
     * Pfad. Wer eine Zellendauer vorgibt, meint sie (MF-471); wer die PLL
     * abschaltet, will die Periode eingefroren haben; wer am Feineinsteller
     * dreht, hat sich etwas dabei gedacht (MF-480). In allen drei Faellen
     * waere eine stille Korrektur genau das, was dieses Werkzeug nicht tut —
     * und sie hat, als sie noch drin war, drei bestehende Vertragstests
     * gekippt. Die Messung bleibt trotzdem erreichbar: Vorgabe weglassen. */
    const bool caller_pinned_the_timing =
        (opts->bitcell_ns != 0) ||
        (!opts->use_pll) ||
        (opts->media_adjust_pct > 0.0 && opts->media_adjust_pct != 100.0);
    if (caller_pinned_the_timing) return rc;

    double measured = amiga_measured_cell_ns(flux, opts);
    track->measured_cell_ns = measured;

    /* Kandidat 2: die gemessene Zellendauer. Nur wenn sie sich von der
     * gewaehlten unterscheidet — sonst waere es derselbe Lauf noch einmal. */
    if (measured > 0.0) {
        double rel = measured / bitcell_ns;
        if (rel <= 0.98 || rel >= 1.02)
            rc = amiga_try_candidate(flux, track, opts, measured,
                                     FLUX_TIMING_MEASURED, rc);
    }

    /* Kandidat 3: derselbe Strom, um den Gleichlauffehler entzerrt.
     *
     * Der Startwert ist hier nicht Beiwerk, sondern tragend: auf einer Spur
     * mit zwei Geschwindigkeiten liefert die Entzerrung mit dem GEMESSENEN
     * Startwert 9 heile Sektoren, mit einem anderen plausiblen Wert 3.
     * Deshalb steht die Sync-Suche vor der Entzerrung und nicht daneben. */
    rc = amiga_try_dewarped(flux, track, opts,
                            (measured > 0.0) ? measured : bitcell_ns, rc);
    return rc;
}

flux_status_t flux_decode_track(const flux_raw_data_t *flux,
                                flux_decoded_track_t *track,
                                const flux_decoder_options_t *opts) {
    if (!flux || !track) return FLUX_ERR_INVALID;
    
    flux_decoded_track_init(track);
    
    flux_decoder_options_t local_opts;
    if (!opts) {
        flux_decoder_options_init(&local_opts);
        opts = &local_opts;
    }
    
    flux_encoding_t encoding = opts->encoding;
    if (encoding == FLUX_ENC_AUTO) {
        encoding = flux_detect_encoding(flux);
    }
    
    switch (encoding) {
        case FLUX_ENC_MFM:
            return flux_decode_mfm(flux, track, opts);

        case FLUX_ENC_AMIGA:
            return flux_decode_amiga(flux, track, opts);

        case FLUX_ENC_FM:
            return flux_decode_fm(flux, track, opts);
        
        case FLUX_ENC_GCR_C64:
            return flux_decode_gcr_c64(flux, track, opts);
        
        case FLUX_ENC_GCR_APPLE:
            return flux_decode_gcr_apple(flux, track, opts);
        
        case FLUX_ENC_AUTO:
            /* MF-766: die Erkennung hat sich NICHT entschieden. Frueher
             * kam dieser Fall nie vor, weil sie immer riet — und in
             * drei von fuenf Faellen falsch. */
            UFT_WARN("Kodierung nicht bestimmbar — kein bekanntes Verhaeltnismuster im Wechsel-Histogramm");
            return FLUX_ERR_ENCODING_UNKNOWN;

        default:
            return FLUX_ERR_INVALID;
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* flux_encoding_name(flux_encoding_t enc) {
    switch (enc) {
        case FLUX_ENC_AUTO:      return "Auto";
        case FLUX_ENC_MFM:       return "MFM";
        case FLUX_ENC_FM:        return "FM";
        case FLUX_ENC_GCR_C64:   return "GCR (C64)";
        case FLUX_ENC_GCR_APPLE: return "GCR (Apple)";
        case FLUX_ENC_AMIGA:     return "Amiga MFM";
        case FLUX_ENC_RAW:       return "Raw";
        default:                 return "Unknown";
    }
}

const char* flux_status_name(flux_status_t status) {
    switch (status) {
        case FLUX_OK:           return "OK";
        case FLUX_ERR_NO_SYNC:  return "No sync pattern";
        case FLUX_ERR_BAD_CRC:  return "CRC error";
        case FLUX_ERR_NO_DATA:  return "No data field";
        case FLUX_ERR_WEAK_BITS: return "Weak/unreliable bits";
        case FLUX_ERR_OVERFLOW: return "Buffer overflow";
        case FLUX_ERR_UNDERFLOW: return "Not enough data";
        case FLUX_ERR_INVALID:  return "Invalid parameters";
        default:                return "Unknown error";
    }
}

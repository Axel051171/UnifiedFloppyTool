/**
 * @file ufm_c64_metrics.c
 * @brief Extract per-track protection metrics from raw C64 GCR bitstreams
 *
 * See include/uft/protection/ufm_c64_metrics.h for the contract, and
 * docs/KNOWN_ISSUES.md PROT-2 for why this file exists.
 *
 * SPDX-License-Identifier: MIT
 */

#include "uft/protection/ufm_c64_metrics.h"

#include <string.h>

/* GCR is a 5-bit code with 16 legal values; every other 5-bit group is an
 * illegal encoding. Table maps code -> nibble, 0xFF marks illegal. */
static const uint8_t UFM_GCR_DECODE[32] = {
    [0x0A] = 0x0, [0x0B] = 0x1, [0x12] = 0x2, [0x13] = 0x3,
    [0x0E] = 0x4, [0x0F] = 0x5, [0x16] = 0x6, [0x17] = 0x7,
    [0x09] = 0x8, [0x19] = 0x9, [0x1A] = 0xA, [0x1B] = 0xB,
    [0x0D] = 0xC, [0x1D] = 0xD, [0x1E] = 0xE, [0x15] = 0xF,
    /* every entry not listed above defaults to 0 — see UFM_GCR_LEGAL */
};

/* Bitmask of the 16 legal GCR codes, so a zero-valued table entry cannot be
 * confused with the legal code that decodes to nibble 0. */
static const uint32_t UFM_GCR_LEGAL =
    (1u << 0x0A) | (1u << 0x0B) | (1u << 0x12) | (1u << 0x13) |
    (1u << 0x0E) | (1u << 0x0F) | (1u << 0x16) | (1u << 0x17) |
    (1u << 0x09) | (1u << 0x19) | (1u << 0x1A) | (1u << 0x1B) |
    (1u << 0x0D) | (1u << 0x1D) | (1u << 0x1E) | (1u << 0x15);

/* Standard-Spurlaengen der 1541 in Byte, nach Speed-Zone (Zone 3 = Spuren
 * 1-17 ... Zone 0 = 31-42).
 *
 * MF-878: hier stand `{ 6250, 6667, 7143, 7692 }` mit der Zusage „A
 * VICE-written G64 stores exactly these lengths." Die Zusage war an ihrer
 * eigenen benannten Quelle falsch. Gemessen an
 * `tests/corpus_free/vice_c1541_35trk.g64` — einer von VICEs c1541
 * erzeugten Aufnahme —, Spurlaenge aus dem Kopf jeder Spur gelesen:
 *
 *     Spur  1 und 17 -> 7692   (Zone 3)
 *     Spur 18 und 24 -> 7142   (Zone 2)   <- nicht 7143
 *     Spur 25 und 30 -> 6666   (Zone 1)   <- nicht 6667
 *     Spur 31 und 35 -> 6250   (Zone 0)
 *
 * 6667 und 7143 sind die aufgerundeten NOMINALWERTE (7142,85 -> 7143), also
 * gerechnet statt gemessen. Die SSOT des Baums fuehrt seit MF-434 die
 * gemessenen Werte: `src/formats/cbm/uft_cbm_geometry.c:64`
 * `capacity_by_speed[4] = { 6250, 6666, 7142, 7692 }`.
 *
 * Folge des Fehlers, ehrlich beziffert: `track_length_ratio` wurde fuer
 * echte Spuren der Zonen 1 und 2 um 1,4e-4 zu klein. Die Schwelle fuer
 * „lange Spur" liegt bei 1,02 (`ufm_c64_scheme_detect.c:102`), die Anzeige
 * rundet auf drei Nachkommastellen — es hat sich also NICHTS falsch
 * verhalten. Falsch war die Aussage, nicht das Verhalten.
 *
 * Diese Tabelle bleibt eine Kopie der SSOT; sie zu einem Aufruf von
 * `uft_cbm_track_capacity()` zusammenzufuehren braucht eine Bindung, die
 * hier noch nicht liegt (P3-150). */
static const uint32_t UFM_ZONE_BYTES[4] = { 6250, 6666, 7142, 7692 };

#define UFM_SYNC_MIN_BITS   10   /* 1541 hardware sync: >= 10 one-bits */
/* nibtools tolerates a 3-byte glitch when calling a track "all sync"
 * (`syncs >= length - 3`), because mastering hardware produces one; 3 bytes
 * = 24 bits is that same tolerance expressed in this function's bit domain. */
#define UFM_KILLER_SLACK_BITS 24
#define UFM_BLOCK_ID_HEADER 0x08
#define UFM_BLOCK_ID_DATA   0x07
#define UFM_MAX_HEADER_IDS  64   /* generous: standard max is 21 per track */

uint32_t ufm_c64_zone_nominal_bytes(int zone)
{
    if (zone < 0 || zone > 3) return 0;
    return UFM_ZONE_BYTES[zone];
}

int ufm_c64_zone_for_track(int track)
{
    if (track < 1 || track > 42) return -1;
    if (track <= 17) return 3;
    if (track <= 24) return 2;
    if (track <= 30) return 1;
    return 0;
}

/** Read @p count bits starting at absolute bit position @p pos, MSB first. */
static inline uint32_t ufm_read_bits(const uint8_t *buf, size_t nbits,
                                     size_t pos, unsigned count)
{
    uint32_t v = 0;
    for (unsigned i = 0; i < count; i++) {
        size_t p = pos + i;
        uint32_t bit = (p < nbits) ? ((buf[p >> 3] >> (7 - (p & 7))) & 1u) : 0u;
        v = (v << 1) | bit;
    }
    return v;
}

/**
 * Decode up to @p want bytes of GCR starting at bit position @p pos.
 * Returns the number of bytes decoded; *bad receives illegal-code hits.
 */
static size_t ufm_gcr_decode_bytes(const uint8_t *buf, size_t nbits, size_t pos,
                                   uint8_t *out, size_t want, uint32_t *bad)
{
    size_t produced = 0;
    uint8_t hi = 0;
    bool have_hi = false;

    for (size_t i = 0; produced < want; i++) {
        size_t p = pos + i * 5;
        if (p + 5 > nbits) break;

        uint32_t code = ufm_read_bits(buf, nbits, p, 5);
        if (!((UFM_GCR_LEGAL >> code) & 1u)) {
            if (bad) (*bad)++;
            break;                      /* stop at the first illegal code */
        }
        uint8_t nib = UFM_GCR_DECODE[code];

        if (!have_hi) {
            hi = nib;
            have_hi = true;
        } else {
            out[produced++] = (uint8_t)((hi << 4) | nib);
            have_hi = false;
        }
    }
    return produced;
}

bool ufm_c64_metrics_from_gcr(const uint8_t *gcr, size_t gcr_len,
                              int track_x2, int speed_zone,
                              ufm_c64_track_metrics_t *out)
{
    if (!gcr || !out || gcr_len == 0) return false;

    memset(out, 0, sizeof(*out));

    const int track = (track_x2 / 2) + 1;
    out->track_x2 = track_x2;
    out->track = (uint8_t)track;
    out->side = 0;
    out->is_half_track = (track_x2 % 2) != 0;

    const size_t nbits = gcr_len * 8u;
    out->bitcell_count = (uint32_t)nbits;

    int zone = speed_zone;
    if (zone == UFM_C64_SPEED_ZONE_AUTO) zone = ufm_c64_zone_for_track(track);
    uint32_t nominal = ufm_c64_zone_nominal_bytes(zone);
    if (nominal > 0) {
        out->track_length_ratio = (float)gcr_len / (float)nominal;
    }

    /* Single pass: locate sync runs, decode the block that follows each. */
    uint16_t ids[UFM_MAX_HEADER_IDS];
    size_t id_count = 0;
    uint32_t run = 0, max_run = 0;

    for (size_t p = 0; p <= nbits; p++) {
        uint32_t bit = (p < nbits)
            ? ((gcr[p >> 3] >> (7 - (p & 7))) & 1u)
            : 0u;                       /* virtual trailing 0 closes a run */

        if (bit) {
            run++;
            if (run > max_run) max_run = run;
            continue;
        }
        if (run < UFM_SYNC_MIN_BITS) {
            run = 0;
            continue;
        }

        /* A sync ended at p; the block starts here. */
        out->sync_count++;
        run = 0;

        uint8_t blk[8];
        size_t got = ufm_gcr_decode_bytes(gcr, nbits, p, blk, sizeof(blk),
                                          &out->bad_gcr_count);
        if (got < 4) continue;

        if (blk[0] == UFM_BLOCK_ID_HEADER) {
            out->sector_count++;
            /* Header layout after the id: checksum, sector, track, ... */
            uint16_t id = (uint16_t)((blk[3] << 8) | blk[2]);
            if (id_count < UFM_MAX_HEADER_IDS) {
                for (size_t k = 0; k < id_count; k++) {
                    if (ids[k] == id) { out->duplicate_ids++; break; }
                }
                ids[id_count++] = id;
            }
        }
        /* UFM_BLOCK_ID_DATA blocks are counted only through sync_count —
         * sector_count must stay the header count so the standard geometry
         * (21/19/18/17) is what a caller sees. */
    }

    out->max_sync_run_bits = max_run;
    out->illegal_gcr_events = out->bad_gcr_count;
    out->has_meaningful_data = (out->sync_count > 0);
    out->has_half_track = out->is_half_track && out->has_meaningful_data;

    /* Killer track, per nibtools check_sync_flags(): a track that is sync
     * almost end to end (there, `syncs >= length - 3` over $FF bytes). Kept
     * separate from has_custom_sync because nibtools also keeps BM_FF_TRACK
     * and "non-standard headers" apart — and because folding them together
     * would make every killer track look like V-MAX! downstream. */
    const bool killer = (max_run + UFM_KILLER_SLACK_BITS >= (uint32_t)nbits);

    /* Custom sync: the track carries sync marks, but none of them introduces a
     * standard 1541 header block. That is the condition nibtools describes as
     * a "track w/non-standard headers" (find_track_cycle_headers falls through
     * to find_track_cycle_syncs for exactly this case). A normally formatted
     * track always yields 17..21 headers, so this cannot fire on one — the
     * corpus test proves that across all 35 tracks. */
    out->has_custom_sync = (out->sync_count > 0) &&
                           (out->sector_count == 0) &&
                           !killer;

    return true;
}

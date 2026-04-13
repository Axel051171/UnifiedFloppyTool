/**
 * @file uft_amiga_missing_schemes.c
 * @brief Amiga Copy Protection -- Missing Scheme Detectors
 * @version 1.0.0
 *
 * Implements detection for Amiga protection schemes not yet covered
 * by the existing 194-entry registry:
 *
 *   1. Timelord (timing-based, Digital Integration)
 *   2. SNOOP (Software Knights of the Round Table)
 *   3. Hexalock
 *   4. Vault Breaker
 *   5. FBI Protection
 *   6. Kaktus/Blue Byte Protection
 *
 * NOTE: CopyLock II/III variants are already covered by the existing
 * CopyLock detector which identifies old/new variants.
 *
 * Each detector analyses raw MFM track data.
 */

#include "uft/protection/uft_protection_unified.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

static bool amiga_find_sig(const uint8_t *data, size_t size,
                           const uint8_t *sig, size_t slen, size_t *pos)
{
    if (size < slen) return false;
    for (size_t i = 0; i + slen <= size; i++) {
        if (memcmp(data + i, sig, slen) == 0) {
            if (pos) *pos = i;
            return true;
        }
    }
    return false;
}

static bool amiga_find_word(const uint8_t *data, size_t size,
                            uint16_t word, int *count_out)
{
    int count = 0;
    for (size_t i = 0; i + 1 < size; i++) {
        uint16_t w = ((uint16_t)data[i] << 8) | data[i + 1];
        if (w == word) count++;
    }
    if (count_out) *count_out = count;
    return count > 0;
}

static int amiga_count_byte(const uint8_t *data, size_t size, uint8_t b)
{
    int count = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == b) count++;
    }
    return count;
}

/*===========================================================================
 * 1. Timelord Protection (Digital Integration)
 *
 * Technical background:
 * - Used by Digital Integration titles (F-16 Combat Pilot, etc.)
 * - Timing-based: measures exact read time between sectors
 * - Track has valid AmigaDOS sectors but with precise inter-sector gaps
 * - The gap timing is verified by the protection code
 * - Detection: look for valid AmigaDOS with unusual gap patterns
 *===========================================================================*/

float uft_amiga_detect_timelord(const uint8_t *track_data, size_t size,
                                 uint8_t track, char *detail,
                                 size_t detail_size)
{
    if (!track_data || size < 1000) return 0.0f;

    float confidence = 0.0f;

    /* Timelord uses standard $4489 sync but with precise gap lengths.
     * Count sync words -- should be exactly 11 (standard AmigaDOS) */
    int sync_count = 0;
    amiga_find_word(track_data, size, 0x4489, &sync_count);

    /* Must look like a valid AmigaDOS track */
    if (sync_count < 8 || sync_count > 13) return 0.0f;

    /* Timelord-specific: the gap between sectors is exactly controlled.
     * In standard AmigaDOS, gaps contain $AA (MFM zero).
     * Timelord tracks have very uniform gap lengths.
     * Approximate: measure gap consistency by finding $AA runs. */

    int gap_count = 0;
    int gap_sizes[16] = {0};
    int run_len = 0;
    bool in_gap = false;

    for (size_t i = 0; i < size; i++) {
        if (track_data[i] == 0xAA) {
            if (!in_gap) {
                in_gap = true;
                run_len = 0;
            }
            run_len++;
        } else {
            if (in_gap && run_len > 20) {
                if (gap_count < 16) {
                    gap_sizes[gap_count] = run_len;
                }
                gap_count++;
            }
            in_gap = false;
        }
    }

    /* Timelord: >8 gaps of similar length */
    if (gap_count >= 8) {
        /* Check gap uniformity */
        int avg = 0;
        for (int i = 0; i < gap_count && i < 16; i++) avg += gap_sizes[i];
        avg /= (gap_count < 16 ? gap_count : 16);

        int uniform = 0;
        for (int i = 0; i < gap_count && i < 16; i++) {
            int diff = gap_sizes[i] - avg;
            if (diff < 0) diff = -diff;
            if (diff < avg / 10) uniform++;  /* Within 10% */
        }

        /* If >80% of gaps are uniform, likely Timelord */
        if (uniform * 100 / (gap_count < 16 ? gap_count : 16) > 80) {
            confidence = 0.70f;
        }
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "Timelord timing protection: %d uniform inter-sector gaps",
                 gap_count);
    }

    return confidence;
}

/*===========================================================================
 * 2. SNOOP Protection (Software Knights)
 *
 * Technical background:
 * - Used by Software Knights of the Round Table
 * - Places protection data on a specific track (usually track 79 or 159)
 * - Uses non-standard sync word $8911
 * - Track data contains verification checksum
 *===========================================================================*/

float uft_amiga_detect_snoop(const uint8_t *track_data, size_t size,
                              uint8_t track, char *detail,
                              size_t detail_size)
{
    if (!track_data || size < 200) return 0.0f;

    float confidence = 0.0f;

    /* SNOOP sync word: $8911 */
    int snoop_syncs = 0;
    amiga_find_word(track_data, size, 0x8911, &snoop_syncs);

    if (snoop_syncs > 0) {
        confidence += 0.50f;

        /* Look for SNOOP data pattern following sync */
        size_t pos = 0;
        const uint8_t snoop_sync[] = { 0x89, 0x11 };
        if (amiga_find_sig(track_data, size, snoop_sync, 2, &pos)) {
            /* Check if data after sync has checksum structure */
            if (pos + 10 < size) {
                /* SNOOP stores a 4-byte checksum after the sync */
                uint32_t check = 0;
                for (int i = 0; i < 4; i++) {
                    check ^= track_data[pos + 2 + i];
                }
                /* Non-zero checksum data = likely valid SNOOP */
                if (check != 0 && check != 0xFF) {
                    confidence += 0.30f;
                }
            }
        }
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "SNOOP protection: sync $8911 found %d times on track %d",
                 snoop_syncs, track);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 3. Hexalock Protection
 *
 * Technical background:
 * - Hexalock used by several German publishers
 * - Protection track with 6 sectors using custom sync $A245
 * - Each sector contains a hex-encoded verification key
 * - Track is typically track 79 side 1 (physical track 159)
 *===========================================================================*/

float uft_amiga_detect_hexalock(const uint8_t *track_data, size_t size,
                                 uint8_t track, char *detail,
                                 size_t detail_size)
{
    if (!track_data || size < 200) return 0.0f;

    float confidence = 0.0f;

    /* Hexalock sync: $A245 */
    int hex_syncs = 0;
    amiga_find_word(track_data, size, 0xA245, &hex_syncs);

    if (hex_syncs >= 3) {
        confidence += 0.50f;

        /* Hexalock typically has exactly 6 sectors */
        if (hex_syncs >= 5 && hex_syncs <= 8) {
            confidence += 0.25f;
        }
    }

    /* Secondary indicator: track should be longer than standard
     * or have unusual data pattern */
    if (confidence > 0.0f) {
        /* Look for hex-encoded data (ASCII 0-9, A-F) */
        int hex_chars = 0;
        for (size_t i = 0; i < size; i++) {
            uint8_t c = track_data[i];
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
                (c >= 'a' && c <= 'f')) {
                hex_chars++;
            }
        }
        float hex_ratio = (float)hex_chars / (float)size;
        if (hex_ratio > 0.05f) {
            confidence += 0.15f;
        }
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "Hexalock: sync $A245 found %d times on track %d",
                 hex_syncs, track);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 4. Vault Breaker Protection
 *
 * Technical background:
 * - Vault Breaker uses weak bits on specific tracks
 * - The protection reads the same track multiple times and
 *   compares results -- weak bits should differ between reads
 * - Single-revolution analysis: look for areas with unusual
 *   flux density that suggest intentional magnetisation degradation
 *===========================================================================*/

float uft_amiga_detect_vault_breaker(const uint8_t *track_data, size_t size,
                                      uint8_t track, char *detail,
                                      size_t detail_size)
{
    if (!track_data || size < 500) return 0.0f;

    float confidence = 0.0f;

    /* Vault Breaker: look for regions of $00 bytes (no flux transitions)
     * which indicate weak/unmagnetised areas */
    int no_flux_runs = 0;
    int run = 0;
    for (size_t i = 0; i < size; i++) {
        if (track_data[i] == 0x00) {
            run++;
        } else {
            if (run > 8) no_flux_runs++;
            run = 0;
        }
    }
    if (run > 8) no_flux_runs++;

    /* Multiple no-flux regions suggest weak bits */
    if (no_flux_runs >= 3 && no_flux_runs <= 20) {
        confidence += 0.40f;

        /* Additional check: rest of track should have valid Amiga data */
        int std_syncs = 0;
        amiga_find_word(track_data, size, 0x4489, &std_syncs);
        if (std_syncs >= 5) {
            confidence += 0.25f;
        }
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "Vault Breaker: %d no-flux regions on track %d",
                 no_flux_runs, track);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 5. FBI Protection
 *
 * Technical background:
 * - FBI Protection used by several publishers
 * - Uses a combination of long track + specific data pattern
 * - Protection track contains "FBI" marker followed by checksum
 * - Typically on the last track of the disk
 *===========================================================================*/

float uft_amiga_detect_fbi(const uint8_t *track_data, size_t size,
                            uint8_t track, char *detail, size_t detail_size)
{
    if (!track_data || size < 200) return 0.0f;

    float confidence = 0.0f;

    /* Look for "FBI" marker in track data */
    const uint8_t fbi_marker[] = { 'F', 'B', 'I' };
    size_t pos = 0;
    if (amiga_find_sig(track_data, size, fbi_marker, 3, &pos)) {
        confidence += 0.40f;

        /* Check for checksum data following marker */
        if (pos + 10 < size) {
            /* FBI stores a verification word after the marker */
            uint16_t verify = ((uint16_t)track_data[pos + 3] << 8) |
                               track_data[pos + 4];
            if (verify != 0x0000 && verify != 0xFFFF) {
                confidence += 0.25f;
            }
        }
    }

    /* FBI also uses slightly long tracks */
    if (size > 13000) {  /* Standard Amiga DD ~12668 bytes */
        confidence += 0.15f;
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "FBI Protection marker found on track %d", track);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 6. Kaktus / Blue Byte Protection
 *
 * Technical background:
 * - Blue Byte (German publisher: Battle Isle, Settlers, etc.)
 * - Kaktus protection scheme
 * - Uses dual-format track: part AmigaDOS, part custom
 * - The custom part has a specific sync ($4124) followed by
 *   encoded verification data
 * - Similar to RNC dual-format but with different sync
 *===========================================================================*/

float uft_amiga_detect_kaktus(const uint8_t *track_data, size_t size,
                               uint8_t track, char *detail,
                               size_t detail_size)
{
    if (!track_data || size < 500) return 0.0f;

    float confidence = 0.0f;

    /* Kaktus/Blue Byte sync: $4124 (same as Factor5) */
    int kaktus_syncs = 0;
    amiga_find_word(track_data, size, 0x4124, &kaktus_syncs);

    /* Also check for standard AmigaDOS syncs on the same track */
    int std_syncs = 0;
    amiga_find_word(track_data, size, 0x4489, &std_syncs);

    /* Dual-format: both standard and custom syncs present */
    if (kaktus_syncs >= 1 && std_syncs >= 1) {
        confidence += 0.50f;

        /* Blue Byte specific: look for "BLUE" or "BYTE" marker */
        const uint8_t blue_str[] = { 'B', 'L', 'U', 'E' };
        const uint8_t byte_str[] = { 'B', 'Y', 'T', 'E' };
        if (amiga_find_sig(track_data, size, blue_str, 4, NULL) ||
            amiga_find_sig(track_data, size, byte_str, 4, NULL)) {
            confidence += 0.25f;
        }

        /* Track should be longer than standard due to dual format */
        if (size > 13000) {
            confidence += 0.10f;
        }
    }
    /* Only custom syncs, no AmigaDOS */
    else if (kaktus_syncs >= 3 && std_syncs == 0) {
        confidence += 0.40f;
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "Kaktus/Blue Byte: $4124 sync=%d, $4489 sync=%d, "
                 "track %d",
                 kaktus_syncs, std_syncs, track);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

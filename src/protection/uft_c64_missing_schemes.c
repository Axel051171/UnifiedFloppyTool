/**
 * @file uft_c64_missing_schemes.c
 * @brief C64 Copy Protection -- Missing Scheme Detectors
 * @version 1.0.0
 *
 * Implements detection for C64 protection schemes that were previously
 * not covered:
 *
 *   1. Epyx Fastload Protection
 *   2. Gremlin/Ocean Loader Variants
 *   3. Alien Technology Group (ATG)
 *   4. Triad Protection
 *   5. XEMAG Protection
 *   6. Abacus Protection
 *   7. Rainbird/Firebird Protection
 *
 * Each detector takes GCR track data and returns a confidence 0.0-1.0.
 */

#include "uft/protection/uft_protection_unified.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

static int count_byte_c64(const uint8_t *data, size_t size, uint8_t byte)
{
    int count = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == byte) count++;
    }
    return count;
}

static bool find_sig(const uint8_t *data, size_t size,
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

/** Count the number of $FF-byte runs of at least min_len */
static int count_sync_runs(const uint8_t *data, size_t size, int min_len)
{
    int runs = 0;
    int current = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == 0xFF) {
            current++;
        } else {
            if (current >= min_len) runs++;
            current = 0;
        }
    }
    if (current >= min_len) runs++;
    return runs;
}

/*===========================================================================
 * 1. Epyx Fastload Protection
 *
 * Technical background:
 * - Epyx Fastload cartridge provided a fast serial protocol
 * - Protection checks for the cartridge being present
 * - Disk protection: uses non-standard sector interleave on track 1
 * - Sector headers have modified checksum bytes
 * - Directory track (18) has specific layout with LOAD"*",8,1 autoboot
 *===========================================================================*/

float uft_c64_detect_epyx_fastload(const uint8_t *track_data, size_t size,
                                    uint8_t track, char *detail, size_t detail_size)
{
    if (!track_data || size < 200) return 0.0f;

    float confidence = 0.0f;

    /* Epyx Fastload disk signature: specific loader code at track 1 */
    if (track == 1) {
        /* Look for Epyx Fastload loader signature:
         * The boot code jumps to the fastload ROM at $DF00-$DFFF.
         * We look for the JSR $DFXX pattern in the decoded sector data.
         * In GCR: approximate by looking for byte patterns
         * $20 $xx $DF (JSR $DFxx) after GCR decode approximation. */

        /* Simplified: look for high byte $DF in GCR stream */
        int df_count = count_byte_c64(track_data, size, 0xDF);
        int high_count = 0;
        for (size_t i = 0; i + 1 < size; i++) {
            if (track_data[i] == 0x20 && track_data[i + 1] == 0xDF) {
                high_count++;
            }
        }

        if (df_count > 3 || high_count > 0) {
            confidence += 0.30f;
        }
    }

    /* Epyx disks often have non-standard interleave */
    if (track == 18) {
        /* Check for autoboot entry: "LOAD" string or $20 $2A marker */
        const uint8_t load_marker[] = { 0x4C, 0x4F, 0x41, 0x44 }; /* LOAD */
        if (find_sig(track_data, size, load_marker, 4, NULL)) {
            confidence += 0.20f;
        }
    }

    /* Epyx uses non-standard sync lengths (short syncs) */
    int short_syncs = count_sync_runs(track_data, size, 2);
    int long_syncs = count_sync_runs(track_data, size, 5);

    /* Epyx: many short syncs, few long syncs */
    if (short_syncs > 15 && long_syncs < 5) {
        confidence += 0.30f;
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "Epyx Fastload: short syncs=%d, long syncs=%d",
                 short_syncs, long_syncs);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 2. Gremlin/Ocean Loader Variants
 *
 * Technical background:
 * - Ocean/Gremlin used custom loaders with encrypted code blocks
 * - Protection: specific byte patterns in the boot sector
 * - Novaload variant used by Ocean (also implements as separate detector)
 * - The loader occupies track 1, sectors 0-8 with specific header
 *===========================================================================*/

float uft_c64_detect_gremlin_ocean(const uint8_t *track_data, size_t size,
                                    uint8_t track, char *detail,
                                    size_t detail_size)
{
    if (!track_data || size < 200) return 0.0f;

    float confidence = 0.0f;

    /* Ocean/Gremlin loader signature: specific byte sequence at
     * sector 0 of track 1. The loader starts with:
     * $A9 xx $85 $01 (LDA #xx / STA $01) to set memory config */
    if (track == 1) {
        const uint8_t ocean_sig1[] = { 0xA9, 0x34, 0x85, 0x01 };
        const uint8_t ocean_sig2[] = { 0xA9, 0x37, 0x85, 0x01 };
        const uint8_t ocean_sig3[] = { 0xA9, 0x35, 0x85, 0x01 };

        if (find_sig(track_data, size, ocean_sig1, 4, NULL) ||
            find_sig(track_data, size, ocean_sig2, 4, NULL) ||
            find_sig(track_data, size, ocean_sig3, 4, NULL)) {
            confidence += 0.35f;
        }

        /* Look for IEC bus timing loop (Ocean loader characteristic) */
        /* $DD00 port manipulation: $A9 xx $8D $00 $DD */
        const uint8_t iec_sig[] = { 0x8D, 0x00, 0xDD };
        size_t pos = 0;
        int iec_count = 0;
        const uint8_t *p = track_data;
        size_t remaining = size;
        while (find_sig(p, remaining, iec_sig, 3, &pos)) {
            iec_count++;
            p += pos + 3;
            remaining = (size_t)(track_data + size - p);
            if (remaining < 3) break;
        }

        if (iec_count >= 3) {
            confidence += 0.30f;
        }
    }

    /* Gremlin-specific: track 18 has modified directory with
     * specific file entries for the loader chain */
    if (track == 18) {
        /* Look for typical Gremlin file naming pattern */
        const uint8_t gremlin_mark[] = { 0x47, 0x52, 0x45, 0x4D }; /* GREM */
        if (find_sig(track_data, size, gremlin_mark, 4, NULL)) {
            confidence += 0.25f;
        }
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "Gremlin/Ocean loader variant detected");
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 3. Alien Technology Group (ATG) Protection
 *
 * Technical background:
 * - ATG protection used by games from Software Toolworks, etc.
 * - Uses extended tracks (36-40) with specific data patterns
 * - Track 36 contains verification data
 * - Non-standard GCR encoding on protection tracks
 *===========================================================================*/

float uft_c64_detect_atg(const uint8_t *track_data, size_t size,
                          uint8_t track, char *detail, size_t detail_size)
{
    if (!track_data || size < 100) return 0.0f;

    float confidence = 0.0f;

    /* ATG typically stores protection on tracks 36-38 */
    if (track >= 36 && track <= 38) {
        /* ATG signature: repeating pattern of $55/$AA (alternating bits) */
        int alt_count = 0;
        for (size_t i = 0; i + 1 < size; i++) {
            if ((track_data[i] == 0x55 && track_data[i + 1] == 0xAA) ||
                (track_data[i] == 0xAA && track_data[i + 1] == 0x55)) {
                alt_count++;
            }
        }

        /* ATG uses alternating bit patterns extensively */
        if (alt_count > 20) {
            confidence += 0.40f;
        }

        /* Also check for specific ATG header marker */
        const uint8_t atg_sig[] = { 0xA5, 0x5A, 0xA5, 0x5A };
        if (find_sig(track_data, size, atg_sig, 4, NULL)) {
            confidence += 0.40f;
        }
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "ATG protection on track %d, alt_pattern density=%.1f%%",
                 track, confidence * 100.0f);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 4. Triad Protection
 *
 * Technical background:
 * - Triad was a cracking group that also published originals
 * - Protection uses specific error patterns on track 35
 * - Killer tracks (all sync, no data) on tracks beyond 35
 *===========================================================================*/

float uft_c64_detect_triad(const uint8_t *track_data, size_t size,
                            uint8_t track, char *detail, size_t detail_size)
{
    if (!track_data || size < 50) return 0.0f;

    float confidence = 0.0f;

    /* Triad protection: "killer track" = all $FF (sync) */
    if (track >= 35 && track <= 40) {
        int ff_count = count_byte_c64(track_data, size, 0xFF);
        float ff_ratio = (float)ff_count / (float)size;

        /* A killer track is >90% $FF bytes */
        if (ff_ratio > 0.90f) {
            confidence += 0.50f;
        }

        /* Triad also uses specific pattern on track 35:
         * alternating error sectors */
        if (track == 35 && size > 200) {
            /* Check for intentional bad GCR (bytes < $04) */
            int bad_gcr = 0;
            for (size_t i = 0; i < size; i++) {
                if (track_data[i] < 0x04 && track_data[i] != 0x00) {
                    bad_gcr++;
                }
            }
            if (bad_gcr > 10) {
                confidence += 0.30f;
            }
        }
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "Triad protection: killer/error track %d", track);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 5. XEMAG Protection
 *
 * Technical background:
 * - XEMAG XELOK used specific sector patterns on track 18
 * - Modified BAM with signature bytes
 * - Often combined with custom DOS
 *===========================================================================*/

float uft_c64_detect_xemag(const uint8_t *track_data, size_t size,
                            uint8_t track, char *detail, size_t detail_size)
{
    if (!track_data || size < 200) return 0.0f;

    float confidence = 0.0f;

    /* XEMAG signature: look for "XEMAG" or "XELOK" string in track data */
    const uint8_t xemag_str[] = { 'X', 'E', 'M', 'A', 'G' };
    const uint8_t xelok_str[] = { 'X', 'E', 'L', 'O', 'K' };

    if (find_sig(track_data, size, xemag_str, 5, NULL)) {
        confidence += 0.60f;
    }
    if (find_sig(track_data, size, xelok_str, 5, NULL)) {
        confidence += 0.60f;
    }

    /* XEMAG modifies the BAM on track 18 */
    if (track == 18) {
        /* Check for unusual BAM entries: track 36-40 allocated
         * when they shouldn't be, or unusual disk name */
        /* XEMAG often uses disk ID $58 $45 (XE) */
        for (size_t i = 0; i + 1 < size; i++) {
            if (track_data[i] == 0x58 && track_data[i + 1] == 0x45) {
                confidence += 0.20f;
                break;
            }
        }
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "XEMAG XELOK protection detected");
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 6. Abacus Protection
 *
 * Technical background:
 * - Abacus Software used a custom DOS with non-standard sectors
 * - 10 sectors per track instead of standard
 * - Modified header checksums
 *===========================================================================*/

float uft_c64_detect_abacus(const uint8_t *track_data, size_t size,
                             uint8_t track, char *detail, size_t detail_size)
{
    if (!track_data || size < 200) return 0.0f;

    float confidence = 0.0f;

    /* Abacus uses 10 sectors on tracks that normally have fewer.
     * For zone 3 (tracks 25-30), standard is 18 sectors.
     * Abacus uses 10 sectors with custom encoding. */

    /* Count sector headers (GCR: look for $52 or $08 header markers) */
    int headers = 0;
    for (size_t i = 0; i < size; i++) {
        if (track_data[i] == 0x52) headers++;
    }

    /* Abacus pattern: exactly 10 headers on tracks that normally
     * have a different count */
    int expected = 0;
    if (track >= 1 && track <= 17) expected = 21;
    else if (track >= 18 && track <= 24) expected = 19;
    else if (track >= 25 && track <= 30) expected = 18;
    else if (track >= 31 && track <= 35) expected = 17;

    if (expected > 0 && headers >= 8 && headers <= 12 &&
        headers != expected) {
        confidence += 0.50f;
    }

    /* Abacus also uses non-standard GCR header format */
    if (track == 1) {
        /* Look for Abacus loader signature */
        const uint8_t abacus_sig[] = { 0xA2, 0x00, 0xBD }; /* LDX #0 / LDA */
        if (find_sig(track_data, size, abacus_sig, 3, NULL)) {
            confidence += 0.20f;
        }
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "Abacus protection: %d sectors (expected %d) on track %d",
                 headers, expected, track);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 7. Rainbird/Firebird Protection
 *
 * Technical background:
 * - Rainbird (a Telecomsoft label) and Firebird used similar protection
 * - Custom error patterns on specific sectors
 * - Track 35 verification with specific byte sequence
 * - Often combined with Speedlock loader
 *===========================================================================*/

float uft_c64_detect_rainbird(const uint8_t *track_data, size_t size,
                               uint8_t track, char *detail,
                               size_t detail_size)
{
    if (!track_data || size < 200) return 0.0f;

    float confidence = 0.0f;

    /* Rainbird protection: track 35, sector-level verification */
    if (track == 35) {
        /* Look for repeated verification pattern: $55 $AA $55 $AA */
        int pattern_count = 0;
        for (size_t i = 0; i + 3 < size; i++) {
            if (track_data[i] == 0x55 && track_data[i + 1] == 0xAA &&
                track_data[i + 2] == 0x55 && track_data[i + 3] == 0xAA) {
                pattern_count++;
            }
        }
        if (pattern_count > 5) {
            confidence += 0.40f;
        }
    }

    /* Rainbird uses specific error codes on sectors */
    if (track >= 33 && track <= 35) {
        /* Check for intentional CRC errors (bad checksum bytes) */
        /* In GCR, checksums are XOR of header/data bytes.
         * Intentional errors = high count of bad GCR */
        int bad_gcr = 0;
        for (size_t i = 0; i < size; i++) {
            /* Invalid GCR values: $00-$03 in a GCR stream */
            if (track_data[i] <= 0x03) bad_gcr++;
        }
        if (bad_gcr > 5 && bad_gcr < 50) {
            confidence += 0.30f;
        }
    }

    /* Rainbird/Firebird string in directory */
    if (track == 18) {
        const uint8_t rain_str[] = { 'R', 'A', 'I', 'N' };
        const uint8_t fire_str[] = { 'F', 'I', 'R', 'E' };
        if (find_sig(track_data, size, rain_str, 4, NULL) ||
            find_sig(track_data, size, fire_str, 4, NULL)) {
            confidence += 0.15f;
        }
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "Rainbird/Firebird protection on track %d", track);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

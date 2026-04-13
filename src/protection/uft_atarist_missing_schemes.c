/**
 * @file uft_atarist_missing_schemes.c
 * @brief Atari ST Copy Protection -- Missing Scheme Detectors
 * @version 1.0.0
 *
 * Implements detection for Atari ST protection schemes not yet covered:
 *
 *   1. Rob Northen Protection (Atari ST variant, beyond CopyLock)
 *   2. Automation Protection (demo scene)
 *   3. 5th Generation Protection
 *   4. ProCopy Protection
 *   5. Safe Disk Protection (Atari ST)
 *   6. MDDOS Protection
 *
 * NOTE: CopyLock, Macrodos, Speedlock, dec0de, and Flaschel are
 * already implemented in the existing Atari ST protection modules.
 *
 * Each detector analyses raw MFM track data and returns confidence.
 */

#include "uft/protection/uft_protection_unified.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*===========================================================================
 * Forward Declarations
 *===========================================================================*/

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

/** Check if position is a DAM (3x $A1 + $FB or $F8) */
static bool data_is_dam(const uint8_t *data, size_t pos, size_t size)
{
    if (pos < 3 || pos >= size) return false;
    return (data[pos] == 0xFB || data[pos] == 0xF8) &&
           data[pos - 1] == 0xA1 &&
           data[pos - 2] == 0xA1 &&
           data[pos - 3] == 0xA1;
}

static bool st_find_sig(const uint8_t *data, size_t size,
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

/** Count MFM IDAM (ID Address Mark): 3x $A1 + $FE */
static int count_idam(const uint8_t *data, size_t size)
{
    int count = 0;
    for (size_t i = 3; i < size; i++) {
        if (data[i] == 0xFE &&
            data[i - 1] == 0xA1 &&
            data[i - 2] == 0xA1 &&
            data[i - 3] == 0xA1) {
            count++;
        }
    }
    return count;
}

/** Count $4E gap fill bytes */
static int count_gap_fill(const uint8_t *data, size_t size)
{
    int count = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == 0x4E) count++;
    }
    return count;
}

/*===========================================================================
 * 1. Rob Northen Protection (Atari ST Variant)
 *
 * Technical background:
 * - Rob Northen developed CopyLock for both Amiga and Atari ST
 * - The Atari ST variant is detected by the existing CopyLock detector
 * - This detector covers the "Rob Northen Protect-Process" variant
 *   which is a secondary check used alongside CopyLock
 * - Uses a special track with encoded data in sector gaps
 * - Verification done via timing measurement of inter-sector gaps
 *===========================================================================*/

float uft_atarist_detect_rnc_protect(const uint8_t *track_data, size_t size,
                                      uint8_t track, char *detail,
                                      size_t detail_size)
{
    if (!track_data || size < 500) return 0.0f;

    float confidence = 0.0f;

    /* RNC Protect-Process places special data in sector gaps.
     * Look for non-$4E gap fill patterns that form an encoded
     * verification sequence. */

    int idams = count_idam(track_data, size);
    if (idams < 5 || idams > 12) return 0.0f;

    /* Count gap fill bytes */
    int gap_fill = count_gap_fill(track_data, size);

    /* Standard Atari ST: ~60-70% of non-sector data is $4E.
     * RNC Protect: significantly less, replaced with data */
    float gap_ratio = (float)gap_fill / (float)size;

    if (gap_ratio < 0.15f && idams >= 5) {
        /* Very low gap fill = data hidden in gaps */
        confidence += 0.40f;

        /* Additional check: look for $A1 bytes outside IDAM positions
         * (RNC uses $A1 patterns in gap data) */
        int extra_a1 = 0;
        for (size_t i = 4; i < size; i++) {
            if (track_data[i] == 0xA1 &&
                track_data[i + 1 < size ? i + 1 : i] != 0xFE) {
                extra_a1++;
            }
        }
        if (extra_a1 > 10) {
            confidence += 0.25f;
        }
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "RNC Protect-Process: gap_fill_ratio=%.1f%%, "
                 "%d sectors, track %d",
                 gap_ratio * 100.0f, idams, track);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 2. Automation Protection
 *
 * Technical background:
 * - Automation was a major Atari ST demo/cracking group
 * - Their protection uses non-standard sector sizes (128 or 1024 bytes)
 * - Sectors have modified ID fields (cylinder/head don't match physical)
 * - Often track 79 or 80 with unusual sector IDs
 *===========================================================================*/

float uft_atarist_detect_automation(const uint8_t *track_data, size_t size,
                                     uint8_t track, char *detail,
                                     size_t detail_size)
{
    if (!track_data || size < 200) return 0.0f;

    float confidence = 0.0f;

    int idams = count_idam(track_data, size);
    if (idams < 1) return 0.0f;

    /* Check sector ID fields for mismatched cylinder/head values */
    int mismatched = 0;
    int unusual_size = 0;
    for (size_t i = 3; i + 6 < size; i++) {
        if (track_data[i] == 0xFE &&
            track_data[i - 1] == 0xA1 &&
            track_data[i - 2] == 0xA1 &&
            track_data[i - 3] == 0xA1) {
            uint8_t cyl = track_data[i + 1];
            uint8_t head = track_data[i + 2];
            uint8_t sec = track_data[i + 3];
            uint8_t sz_code = track_data[i + 4];

            /* Mismatched cylinder (doesn't match physical track) */
            if (cyl != track && cyl != track / 2) {
                mismatched++;
            }

            /* Non-standard size code (not 2 = 512 bytes) */
            if (sz_code != 2) {
                unusual_size++;
            }
        }
    }

    if (mismatched > 0 && idams <= 12) {
        confidence += 0.35f;
    }
    if (unusual_size > 0) {
        confidence += 0.25f;
    }

    /* Automation often uses track 79+ */
    if (track >= 79 && confidence > 0.0f) {
        confidence += 0.10f;
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "Automation protection: %d mismatched IDs, "
                 "%d non-standard sizes on track %d",
                 mismatched, unusual_size, track);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 3. 5th Generation Protection
 *
 * Technical background:
 * - 5th Generation Systems used a protection that writes
 *   specific patterns to the FAT area of the boot sector
 * - The protection checks for a "magic number" computed from
 *   the serial number embedded in the media
 * - Track 0, sector 0 has the boot code; sector 1 has the key
 *===========================================================================*/

float uft_atarist_detect_5th_gen(const uint8_t *track_data, size_t size,
                                  uint8_t track, char *detail,
                                  size_t detail_size)
{
    if (!track_data || size < 500) return 0.0f;
    if (track != 0) return 0.0f;

    float confidence = 0.0f;

    /* 5th Generation embeds a serial number in the boot sector.
     * Look for typical boot sector signature: $60 $xx (BRA.S)
     * followed by OEM name containing "5th" or "FAST" */
    if (size > 20 && track_data[0] == 0x60) {
        /* Check OEM field (offset 3-10 in boot sector) */
        const uint8_t oem_5th[] = { '5', 'T', 'H' };
        const uint8_t oem_fast[] = { 'F', 'A', 'S', 'T' };

        if (st_find_sig(track_data + 3, 8, oem_5th, 3, NULL) ||
            st_find_sig(track_data + 3, 8, oem_fast, 4, NULL)) {
            confidence += 0.40f;
        }

        /* Check for serial number at specific offset (varies) */
        /* 5th Gen often stores at offset $1E0-$1FE in the boot sector */
        if (size > 0x200) {
            bool has_serial = false;
            for (size_t i = 0x1E0; i < 0x1FE && i < size; i++) {
                if (track_data[i] != 0x00 && track_data[i] != 0xF6) {
                    has_serial = true;
                    break;
                }
            }
            if (has_serial) {
                confidence += 0.25f;
            }
        }
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "5th Generation protection detected in boot sector");
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 4. ProCopy Protection
 *
 * Technical background:
 * - ProCopy was an Atari ST disk copier that included protection
 * - Uses weak sectors (fuzzy bits) on specific tracks
 * - The protection verifies that reads of the same sector
 *   produce different results (indicating real media, not copy)
 * - Typically track 0 or track 79, specific sectors
 *===========================================================================*/

float uft_atarist_detect_procopy(const uint8_t *track_data, size_t size,
                                  uint8_t track, char *detail,
                                  size_t detail_size)
{
    if (!track_data || size < 500) return 0.0f;

    float confidence = 0.0f;

    /* ProCopy weak sectors: look for areas of $00 (no flux) or
     * alternating $55/$AA that would produce unreliable reads */
    int no_flux_regions = 0;
    int run = 0;
    for (size_t i = 0; i < size; i++) {
        if (track_data[i] == 0x00) {
            run++;
        } else {
            if (run >= 10) no_flux_regions++;
            run = 0;
        }
    }
    if (run >= 10) no_flux_regions++;

    /* ProCopy: several no-flux regions within an otherwise valid track */
    if (no_flux_regions >= 2 && no_flux_regions <= 10) {
        /* Verify the track also has valid sectors */
        int idams = count_idam(track_data, size);
        if (idams >= 5) {
            confidence += 0.55f;
        } else if (idams >= 1) {
            confidence += 0.35f;
        }
    }

    /* ProCopy protection is often on track 0 or 79 */
    if (confidence > 0.0f && (track == 0 || track == 79)) {
        confidence += 0.10f;
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "ProCopy weak sector protection: %d no-flux regions "
                 "on track %d",
                 no_flux_regions, track);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 5. Safe Disk Protection (Atari ST)
 *
 * Technical background:
 * - Not to be confused with PC SafeDisc
 * - Atari ST "Safe Disk" protection (various publishers)
 * - Uses intentional CRC errors on specific sectors
 * - The DAM (Data Address Mark) is $F8 (deleted) instead of $FB
 * - Track has valid IDAMs but deleted DAMs
 *===========================================================================*/

float uft_atarist_detect_safedisk(const uint8_t *track_data, size_t size,
                                   uint8_t track, char *detail,
                                   size_t detail_size)
{
    if (!track_data || size < 500) return 0.0f;

    float confidence = 0.0f;

    /* Count DAM types */
    int normal_dam = 0;   /* $FB = normal data */
    int deleted_dam = 0;  /* $F8 = deleted data */

    for (size_t i = 3; i < size; i++) {
        if (data_is_dam(track_data, i, size)) {
            if (track_data[i] == 0xFB) normal_dam++;
            else if (track_data[i] == 0xF8) deleted_dam++;
        }
    }

    /* Mix of normal and deleted DAMs = protection */
    if (deleted_dam >= 1 && normal_dam >= 3) {
        confidence += 0.60f;
    }
    /* All deleted = might be intentional */
    else if (deleted_dam >= 5 && normal_dam == 0) {
        confidence += 0.40f;
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "Safe Disk: %d normal DAM, %d deleted DAM on track %d",
                 normal_dam, deleted_dam, track);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

/*===========================================================================
 * 6. MDDOS Protection
 *
 * Technical background:
 * - MDDOS (Modified Disk DOS) protection
 * - Uses 10 sectors per track with custom interleave
 * - Sector numbering starts at 0 instead of 1
 * - Modified gap lengths between sectors
 * - Similar to Macrodos but with different numbering
 *===========================================================================*/

float uft_atarist_detect_mddos(const uint8_t *track_data, size_t size,
                                uint8_t track, char *detail,
                                size_t detail_size)
{
    if (!track_data || size < 500) return 0.0f;

    float confidence = 0.0f;

    int idams = count_idam(track_data, size);

    /* MDDOS: exactly 10 sectors per track */
    if (idams == 10) {
        confidence += 0.30f;

        /* Check sector numbering: MDDOS uses 0-9 instead of 1-10 */
        bool has_sector_0 = false;
        bool has_sector_10 = false;
        for (size_t i = 3; i + 4 < size; i++) {
            if (track_data[i] == 0xFE &&
                track_data[i - 1] == 0xA1 &&
                track_data[i - 2] == 0xA1 &&
                track_data[i - 3] == 0xA1) {
                uint8_t sec = track_data[i + 3];
                if (sec == 0) has_sector_0 = true;
                if (sec == 10) has_sector_10 = true;
            }
        }

        if (has_sector_0 && !has_sector_10) {
            /* Sectors 0-9: MDDOS pattern */
            confidence += 0.35f;
        }

        /* MDDOS: reduced gap sizes between sectors */
        int gap_4e = count_gap_fill(track_data, size);
        float gap_ratio = (float)gap_4e / (float)size;
        /* Standard 9-sector ST: ~30% gap fill.
         * 10-sector with reduced gaps: <20% */
        if (gap_ratio < 0.20f) {
            confidence += 0.15f;
        }
    }

    if (confidence > 0.0f && detail) {
        snprintf(detail, detail_size,
                 "MDDOS protection: %d sectors, 0-based numbering "
                 "on track %d",
                 idams, track);
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

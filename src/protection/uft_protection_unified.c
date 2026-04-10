/**
 * @file uft_protection_unified.c
 * @brief Unified Cross-Platform Copy Protection Detection
 * @version 1.0.0
 *
 * Dispatches to all platform-specific detectors and normalises
 * results into a common uft_protection_hit_unified_t structure.
 */

#include "uft/protection/uft_protection_unified.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*===========================================================================
 * Platform Name Table
 *===========================================================================*/

static const char *platform_names[] = {
    [UFT_UNI_PLATFORM_UNKNOWN]   = "Unknown",
    [UFT_UNI_PLATFORM_C64]       = "C64",
    [UFT_UNI_PLATFORM_AMIGA]     = "Amiga",
    [UFT_UNI_PLATFORM_ATARI_ST]  = "Atari ST",
    [UFT_UNI_PLATFORM_APPLE2]    = "Apple II",
    [UFT_UNI_PLATFORM_ATARI_8BIT]= "Atari 8-bit",
    [UFT_UNI_PLATFORM_PC]        = "PC",
    [UFT_UNI_PLATFORM_SPECTRUM]  = "ZX Spectrum",
    [UFT_UNI_PLATFORM_CPC]       = "Amstrad CPC",
    [UFT_UNI_PLATFORM_BBC]       = "BBC Micro",
    [UFT_UNI_PLATFORM_MSX]       = "MSX",
};

const char *uft_uni_platform_name(uft_uni_platform_t platform)
{
    if (platform >= UFT_UNI_PLATFORM_COUNT)
        return "Unknown";
    return platform_names[platform];
}

/*===========================================================================
 * Internal Helper: Add a hit to result
 *===========================================================================*/

static bool add_hit(uft_protection_unified_result_t *result,
                    const char *platform, const char *scheme,
                    const char *variant, const char *details,
                    float confidence, uint8_t track, uint8_t head,
                    uint16_t tech_flags,
                    bool flux_ok, bool native_ok)
{
    if (result->hit_count >= UFT_PROT_UNI_MAX_HITS)
        return false;

    uft_protection_hit_unified_t *hit = &result->hits[result->hit_count];
    memset(hit, 0, sizeof(*hit));

    snprintf(hit->platform, sizeof(hit->platform), "%s", platform);
    snprintf(hit->scheme_name, sizeof(hit->scheme_name), "%s", scheme);
    if (variant)
        snprintf(hit->variant, sizeof(hit->variant), "%s", variant);
    if (details)
        snprintf(hit->details, sizeof(hit->details), "%s", details);

    hit->confidence = confidence;
    hit->track = track;
    hit->head = head;
    hit->technique_flags = tech_flags;
    hit->preservable_in_flux = flux_ok;
    hit->preservable_in_native = native_ok;

    result->hit_count++;
    result->is_protected = true;

    if (confidence > result->overall_confidence) {
        result->overall_confidence = confidence;
        snprintf(result->primary_scheme, sizeof(result->primary_scheme),
                 "%s", scheme);
    }

    return true;
}

/*===========================================================================
 * C64 Track Analysis Helpers
 *===========================================================================*/

/** Sectors per track for standard 1541 density zones */
static int c64_expected_sectors(int track)
{
    if (track >= 1 && track <= 17)  return 21;
    if (track >= 18 && track <= 24) return 19;
    if (track >= 25 && track <= 30) return 18;
    if (track >= 31 && track <= 40) return 17;
    return 0;
}

/** Count occurrences of a byte in buffer */
static int count_byte(const uint8_t *data, size_t size, uint8_t byte)
{
    int count = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == byte) count++;
    }
    return count;
}

/** Find 16-bit big-endian pattern in data */
static bool find_word(const uint8_t *data, size_t size,
                      uint16_t word, size_t *pos_out)
{
    if (size < 2) return false;
    uint8_t hi = (uint8_t)(word >> 8);
    uint8_t lo = (uint8_t)(word & 0xFF);
    for (size_t i = 0; i + 1 < size; i++) {
        if (data[i] == hi && data[i + 1] == lo) {
            if (pos_out) *pos_out = i;
            return true;
        }
    }
    return false;
}

/** Find byte pattern in data */
static bool find_bytes(const uint8_t *data, size_t size,
                       const uint8_t *pattern, size_t plen, size_t *pos)
{
    if (size < plen) return false;
    for (size_t i = 0; i + plen <= size; i++) {
        if (memcmp(data + i, pattern, plen) == 0) {
            if (pos) *pos = i;
            return true;
        }
    }
    return false;
}

/*===========================================================================
 * C64 Platform Detection
 *===========================================================================*/

static void detect_c64(uft_uni_track_accessor_t accessor, void *user,
                       int track_count,
                       uft_protection_unified_result_t *result)
{
    bool found_vmax = false;
    bool found_rapidlok = false;
    bool found_vorpal = false;
    bool found_long_track = false;
    bool found_half_track = false;
    bool found_custom_sync = false;
    bool found_density = false;

    int max_track = track_count > 40 ? 40 : track_count;

    for (int t = 1; t <= max_track; t++) {
        for (int h = 0; h < 1; h++) {
            size_t size = 0;
            const uint8_t *data = accessor((uint8_t)t, (uint8_t)h,
                                           &size, user);
            if (!data || size < 100) continue;

            int expected = c64_expected_sectors(t);

            /* --- V-MAX! detection ---
             * V-MAX uses custom sync patterns, density zone tricks,
             * and marker bytes $64/$46/$4E on track 20 */
            if (t == 20 && !found_vmax) {
                /* Look for V-MAX marker bytes */
                int marker_64 = count_byte(data, size, 0x64);
                int marker_46 = count_byte(data, size, 0x46);
                int marker_4E = count_byte(data, size, 0x4E);

                /* V-MAX v2: lots of $64/$46 markers */
                if (marker_64 > 5 && marker_46 > 5) {
                    found_vmax = true;
                    add_hit(result, "C64", "V-MAX!", "v2",
                            "V-MAX! v2 detected on track 20 "
                            "(custom sector markers $64/$46)",
                            0.85f, (uint8_t)t, (uint8_t)h,
                            UFT_UNI_TECH_CUSTOM_SYNC |
                            UFT_UNI_TECH_DENSITY |
                            UFT_UNI_TECH_CUSTOM_FORMAT,
                            true, false);
                }
                /* V-MAX v3: $49 headers */
                else if (count_byte(data, size, 0x49) > 8) {
                    found_vmax = true;
                    add_hit(result, "C64", "V-MAX!", "v3",
                            "V-MAX! v3 detected on track 20 "
                            "(header byte $49)",
                            0.80f, (uint8_t)t, (uint8_t)h,
                            UFT_UNI_TECH_CUSTOM_SYNC |
                            UFT_UNI_TECH_CUSTOM_FORMAT,
                            true, false);
                }
            }

            /* --- RapidLok detection ---
             * Track 36 contains the encrypted key sector.
             * Markers: $7B extra sector, $75 sector header */
            if (t == 36 && !found_rapidlok) {
                int marker_7B = count_byte(data, size, 0x7B);
                int marker_75 = count_byte(data, size, 0x75);

                if (marker_7B > 2 && marker_75 > 3) {
                    found_rapidlok = true;
                    add_hit(result, "C64", "RapidLok", NULL,
                            "RapidLok key track detected "
                            "(track 36, markers $7B/$75)",
                            0.90f, (uint8_t)t, (uint8_t)h,
                            UFT_UNI_TECH_CUSTOM_SYNC |
                            UFT_UNI_TECH_ENCRYPTED |
                            UFT_UNI_TECH_CUSTOM_FORMAT,
                            true, false);
                }
            }

            /* --- Vorpal detection ---
             * Excessive sync bytes (>35 on a track vs ~21 standard) */
            if (!found_vorpal) {
                int sync_count = count_byte(data, size, 0xFF);
                /* Vorpal uses MANY sync bytes */
                if (sync_count > 80 && expected > 0) {
                    found_vorpal = true;
                    add_hit(result, "C64", "Vorpal", NULL,
                            "Vorpal detected: excessive sync marks",
                            0.75f, (uint8_t)t, (uint8_t)h,
                            UFT_UNI_TECH_CUSTOM_SYNC |
                            UFT_UNI_TECH_HALF_TRACK,
                            true, false);
                }
            }

            /* --- Long track detection ---
             * Standard track is ~7692 bytes max (zone 1).
             * Anything >8000 bytes is suspicious. */
            if (!found_long_track && size > 8000 && expected > 0) {
                found_long_track = true;
                add_hit(result, "C64", "Long Track", NULL,
                        "Track exceeds standard length",
                        0.70f, (uint8_t)t, (uint8_t)h,
                        UFT_UNI_TECH_LONG_TRACK,
                        true, false);
            }

            /* --- Custom sync detection ---
             * Standard C64 sync: 5+ bytes of $FF followed by data.
             * Non-standard: less than 5 sync bytes or unusual patterns */
            if (!found_custom_sync && size > 200) {
                /* Look for non-$FF sync patterns before sector headers */
                for (size_t i = 2; i + 4 < size; i++) {
                    /* Standard GCR sector header starts $52 or $08 */
                    if ((data[i] == 0x52 || data[i] == 0x08) &&
                        data[i - 1] != 0xFF && data[i - 2] != 0xFF) {
                        /* Found sector header without standard sync */
                        found_custom_sync = true;
                        add_hit(result, "C64", "Custom Sync", NULL,
                                "Non-standard sync patterns before "
                                "sector headers",
                                0.60f, (uint8_t)t, (uint8_t)h,
                                UFT_UNI_TECH_CUSTOM_SYNC,
                                true, false);
                        break;
                    }
                }
            }
        }
    }
}

/*===========================================================================
 * Amiga Platform Detection
 *===========================================================================*/

static void detect_amiga(uft_uni_track_accessor_t accessor, void *user,
                         int track_count,
                         uft_protection_unified_result_t *result)
{
    bool found_copylock = false;
    bool found_speedlock = false;
    bool found_longtrack = false;

    /* Standard Amiga DD track: ~12668 bytes (101344 bits at 2us cells) */
    const size_t AMIGA_STD_TRACK_BYTES = 12668;
    /* Amiga standard sync word */
    const uint16_t AMIGA_SYNC = 0x4489;

    int max_track = track_count > 160 ? 160 : track_count;

    for (int t = 0; t < max_track; t++) {
        for (int h = 0; h < 2; h++) {
            size_t size = 0;
            const uint8_t *data = accessor((uint8_t)t, (uint8_t)h,
                                           &size, user);
            if (!data || size < 100) continue;

            /* --- CopyLock detection ---
             * CopyLock uses non-standard sync words.
             * Standard Amiga sync is $4489. CopyLock uses
             * other sync words derived from LFSR seed. */
            if (!found_copylock) {
                /* Count non-standard sync words */
                int std_syncs = 0;
                int custom_syncs = 0;
                for (size_t i = 0; i + 1 < size; i++) {
                    uint16_t w = ((uint16_t)data[i] << 8) | data[i + 1];
                    if (w == AMIGA_SYNC) {
                        std_syncs++;
                    } else if ((w & 0xFF00) == 0x4400 && w != AMIGA_SYNC) {
                        /* CopyLock-style sync: 0x44xx variants */
                        custom_syncs++;
                    }
                }

                if (custom_syncs >= 6 && std_syncs < 3) {
                    found_copylock = true;
                    add_hit(result, "Amiga", "CopyLock", NULL,
                            "CopyLock LFSR-based sync words detected",
                            0.90f, (uint8_t)t, (uint8_t)h,
                            UFT_UNI_TECH_LFSR |
                            UFT_UNI_TECH_CUSTOM_SYNC |
                            UFT_UNI_TECH_TIMING,
                            true, false);
                }
            }

            /* --- Speedlock detection ---
             * Speedlock uses variable density (variable bitcell timing).
             * The track data appears corrupted when read at standard rate
             * but the length is approximately standard. */
            if (!found_speedlock && !found_copylock) {
                /* Speedlock tracks have high entropy in the inter-sector gap
                 * region but normal sector data. Approximate by checking
                 * if we have very few valid syncs on a track that should
                 * have them. */
                int syncs = 0;
                for (size_t i = 0; i + 1 < size; i++) {
                    uint16_t w = ((uint16_t)data[i] << 8) | data[i + 1];
                    if (w == AMIGA_SYNC) syncs++;
                }
                /* Standard track has 11 syncs. Speedlock has ~0-2 */
                if (size > 10000 && syncs >= 1 && syncs <= 3 &&
                    size < AMIGA_STD_TRACK_BYTES + 2000) {
                    found_speedlock = true;
                    add_hit(result, "Amiga", "Speedlock", NULL,
                            "Speedlock variable-density track detected",
                            0.70f, (uint8_t)t, (uint8_t)h,
                            UFT_UNI_TECH_DENSITY |
                            UFT_UNI_TECH_TIMING,
                            true, false);
                }
            }

            /* --- Long track detection ---
             * Amiga long tracks are >106000 bits (~13250 bytes) */
            if (!found_longtrack &&
                size > AMIGA_STD_TRACK_BYTES + 600) {
                found_longtrack = true;

                /* Try to identify specific longtrack type by sync */
                const char *lt_name = "Long Track";
                const char *lt_var = NULL;
                uint16_t tech = UFT_UNI_TECH_LONG_TRACK;

                /* Check PROTEC sync: $4454 */
                if (find_word(data, size, 0x4454, NULL)) {
                    lt_name = "PROTEC";
                    lt_var = "long track";
                }
                /* Prolance sync: $8945 */
                else if (find_word(data, size, 0x8945, NULL)) {
                    lt_name = "Prolance";
                    lt_var = "B.A.T. long track";
                }
                /* Silmarils sync: $a144 */
                else if (find_word(data, size, 0xA144, NULL)) {
                    /* Check for ROD0 signature */
                    const uint8_t rod0[] = { 'R', 'O', 'D', '0' };
                    if (find_bytes(data, size, rod0, 4, NULL)) {
                        lt_name = "Silmarils";
                        lt_var = "ROD0 long track";
                    } else {
                        lt_name = "Infogrames";
                        lt_var = "long track";
                    }
                }
                /* APP sync: $924a */
                else if (find_word(data, size, 0x924A, NULL)) {
                    lt_name = "APP";
                    lt_var = "Amiga Power Pack";
                }

                add_hit(result, "Amiga", lt_name, lt_var,
                        "Long track protection detected",
                        0.85f, (uint8_t)t, (uint8_t)h,
                        tech, true, false);
            }
        }
    }
}

/*===========================================================================
 * Atari ST Platform Detection
 *===========================================================================*/

static void detect_atari_st(uft_uni_track_accessor_t accessor, void *user,
                            int track_count,
                            uft_protection_unified_result_t *result)
{
    bool found_copylock = false;
    bool found_macrodos = false;
    bool found_long = false;
    bool found_fuzzy = false;

    /* Standard Atari ST DD track: 6250 bytes (9 sectors * 512 + gaps) */
    const size_t ST_STD_BYTES = 6250;

    int max_track = track_count > 84 ? 84 : track_count;

    for (int t = 0; t < max_track; t++) {
        for (int h = 0; h < 2; h++) {
            size_t size = 0;
            const uint8_t *data = accessor((uint8_t)t, (uint8_t)h,
                                           &size, user);
            if (!data || size < 100) continue;

            /* --- CopyLock ST detection ---
             * CopyLock ST uses timing-based protection with
             * an LFSR seed on a specific track. Look for the
             * CopyLock signature pattern: a sequence of non-standard
             * sync words that don't match $4489. */
            if (!found_copylock) {
                /* CopyLock ST appears as a track with unusual
                 * MFM patterns and no valid sectors */
                int valid_idam = 0;
                /* Count $FE (ID Address Mark) preceded by $A1 sync */
                for (size_t i = 3; i < size; i++) {
                    if (data[i] == 0xFE &&
                        data[i - 1] == 0xA1 &&
                        data[i - 2] == 0xA1 &&
                        data[i - 3] == 0xA1) {
                        valid_idam++;
                    }
                }
                /* If a track has data but no valid sector headers,
                 * it might be CopyLock. CopyLock tracks typically
                 * on track 0 or 79. */
                if (size > 4000 && valid_idam == 0 &&
                    (t == 0 || t == 79 || t == 80 || t == 81)) {
                    found_copylock = true;
                    add_hit(result, "Atari ST", "CopyLock", NULL,
                            "CopyLock ST timing protection track "
                            "(no valid sectors, LFSR data)",
                            0.80f, (uint8_t)t, (uint8_t)h,
                            UFT_UNI_TECH_LFSR |
                            UFT_UNI_TECH_TIMING |
                            UFT_UNI_TECH_CUSTOM_FORMAT,
                            true, false);
                }
            }

            /* --- Macrodos detection ---
             * Macrodos uses non-standard sector numbering and
             * extra sectors per track (10-11 instead of 9). */
            if (!found_macrodos) {
                int idam_count = 0;
                for (size_t i = 3; i < size; i++) {
                    if (data[i] == 0xFE &&
                        data[i - 1] == 0xA1 &&
                        data[i - 2] == 0xA1) {
                        idam_count++;
                    }
                }
                /* Standard is 9 sectors. 10+ is Macrodos */
                if (idam_count >= 10 && idam_count <= 12) {
                    found_macrodos = true;
                    add_hit(result, "Atari ST", "Macrodos", NULL,
                            "Macrodos detected: extra sectors per track",
                            0.80f, (uint8_t)t, (uint8_t)h,
                            UFT_UNI_TECH_EXTRA_SECTORS |
                            UFT_UNI_TECH_CUSTOM_FORMAT,
                            true, true);
                }
            }

            /* --- Long track detection --- */
            if (!found_long && size > ST_STD_BYTES + 500) {
                found_long = true;
                add_hit(result, "Atari ST", "Long Track", NULL,
                        "Track exceeds standard length",
                        0.75f, (uint8_t)t, (uint8_t)h,
                        UFT_UNI_TECH_LONG_TRACK,
                        true, false);
            }
        }
    }
}

/*===========================================================================
 * Apple II Platform Detection
 *===========================================================================*/

static void detect_apple2(uft_uni_track_accessor_t accessor, void *user,
                          int track_count,
                          uft_protection_unified_result_t *result)
{
    bool found_custom_marks = false;
    bool found_nibble = false;
    bool found_half = false;

    /* Standard Apple II prologue: D5 AA 96 (address) / D5 AA AD (data) */
    const uint8_t std_addr_pro[] = { 0xD5, 0xAA, 0x96 };
    const uint8_t std_data_pro[] = { 0xD5, 0xAA, 0xAD };

    int max_track = track_count > 40 ? 40 : track_count;

    for (int t = 0; t < max_track; t++) {
        for (int h = 0; h < 1; h++) {
            size_t size = 0;
            const uint8_t *data = accessor((uint8_t)t, (uint8_t)h,
                                           &size, user);
            if (!data || size < 100) continue;

            /* --- Custom address/data marks ---
             * Many Apple II protections use non-standard D5 AA xx
             * prologues. */
            if (!found_custom_marks) {
                int std_addr = 0, std_data = 0;
                int custom_d5 = 0;
                for (size_t i = 0; i + 2 < size; i++) {
                    if (data[i] == 0xD5 && data[i + 1] == 0xAA) {
                        if (data[i + 2] == 0x96) std_addr++;
                        else if (data[i + 2] == 0xAD) std_data++;
                        else custom_d5++;
                    }
                }
                if (custom_d5 > 3 && std_addr < 5) {
                    found_custom_marks = true;
                    add_hit(result, "Apple II", "Custom Marks", NULL,
                            "Non-standard address/data prologues detected",
                            0.80f, (uint8_t)t, (uint8_t)h,
                            UFT_UNI_TECH_CUSTOM_FORMAT,
                            true, false);
                }
            }

            /* --- Nibble count detection ---
             * Protected tracks have unusual nibble counts
             * (more or fewer self-sync bytes than standard). */
            if (!found_nibble) {
                int sync_count = count_byte(data, size, 0xFF);
                /* Standard track: ~40-80 sync bytes.
                 * Protected: >120 or <20 */
                if (sync_count > 120 || (sync_count < 20 && size > 3000)) {
                    found_nibble = true;
                    add_hit(result, "Apple II", "Nibble Count", NULL,
                            "Unusual nibble count (sync byte anomaly)",
                            0.70f, (uint8_t)t, (uint8_t)h,
                            UFT_UNI_TECH_CUSTOM_FORMAT,
                            true, false);
                }
            }
        }

        /* --- Half track detection ---
         * Check for data on half tracks (e.g., track 0.5, 1.5) */
        if (!found_half && t < max_track) {
            size_t size = 0;
            /* Half tracks would be at track*2+1 in some formats.
             * For the accessor, we use track + 0x80 to signal half-track */
            const uint8_t *data = accessor((uint8_t)(t | 0x80), 0,
                                           &size, user);
            if (data && size > 200) {
                /* Check if half track has actual data (not all zeros) */
                int nonzero = 0;
                for (size_t i = 0; i < size && i < 200; i++) {
                    if (data[i] != 0x00) nonzero++;
                }
                if (nonzero > 50) {
                    found_half = true;
                    add_hit(result, "Apple II", "Half Track", NULL,
                            "Data found on half-track position",
                            0.75f, (uint8_t)t, 0,
                            UFT_UNI_TECH_HALF_TRACK,
                            true, false);
                }
            }
        }
    }
}

/*===========================================================================
 * IBM PC Platform Detection
 *===========================================================================*/

static void detect_pc(uft_uni_track_accessor_t accessor, void *user,
                      int track_count,
                      uft_protection_unified_result_t *result)
{
    bool found_weak = false;
    bool found_extra = false;
    bool found_long_sector = false;

    /* Standard PC DD: 9 sectors/track, 512 bytes/sector, 80 tracks */
    int max_track = track_count > 80 ? 80 : track_count;

    for (int t = 0; t < max_track; t++) {
        for (int h = 0; h < 2; h++) {
            size_t size = 0;
            const uint8_t *data = accessor((uint8_t)t, (uint8_t)h,
                                           &size, user);
            if (!data || size < 100) continue;

            /* --- Extra sectors detection ---
             * Count IDAM (ID address marks: $A1 $A1 $A1 $FE) */
            int idam_count = 0;
            for (size_t i = 3; i < size; i++) {
                if (data[i] == 0xFE &&
                    data[i - 1] == 0xA1 &&
                    data[i - 2] == 0xA1 &&
                    data[i - 3] == 0xA1) {
                    idam_count++;
                }
            }
            if (!found_extra && idam_count > 9) {
                found_extra = true;
                add_hit(result, "PC", "Extra Sectors", NULL,
                        "More sectors than standard format allows",
                        0.75f, (uint8_t)t, (uint8_t)h,
                        UFT_UNI_TECH_EXTRA_SECTORS,
                        true, false);
            }

            /* --- Long sector detection ---
             * Look for sector size codes > 2 ($03 = 1024 bytes,
             * $04 = 2048, etc.) in the IDAM fields */
            if (!found_long_sector) {
                for (size_t i = 3; i + 4 < size; i++) {
                    if (data[i] == 0xFE &&
                        data[i - 1] == 0xA1 &&
                        data[i - 2] == 0xA1 &&
                        data[i - 3] == 0xA1) {
                        uint8_t size_code = data[i + 4];
                        if (size_code > 2) {
                            found_long_sector = true;
                            add_hit(result, "PC", "Long Sector", NULL,
                                    "Non-standard sector size detected",
                                    0.70f, (uint8_t)t, (uint8_t)h,
                                    UFT_UNI_TECH_CUSTOM_FORMAT,
                                    true, false);
                            break;
                        }
                    }
                }
            }
        }
    }
}

/*===========================================================================
 * Spectrum / CPC / BBC / MSX Detection (Minimal)
 *
 * These platforms have limited floppy protection schemes documented.
 * We detect the known ones:
 *   Spectrum: Speedlock (tape protection adapted to disk)
 *   CPC: Non-standard sector sizes in EDSK
 *   BBC: Custom format detection
 *   MSX: Non-standard sectors
 *===========================================================================*/

static void detect_spectrum(uft_uni_track_accessor_t accessor, void *user,
                            int track_count,
                            uft_protection_unified_result_t *result)
{
    int max_track = track_count > 80 ? 80 : track_count;

    for (int t = 0; t < max_track; t++) {
        for (int h = 0; h < 2; h++) {
            size_t size = 0;
            const uint8_t *data = accessor((uint8_t)t, (uint8_t)h,
                                           &size, user);
            if (!data || size < 100) continue;

            /* Speedlock disk: Track 0, side 0 has unusual sector
             * layout (sectors 1-9 but sector 1 has special data) */
            /* Generic: look for non-standard sector sizes in EDSK */
            int idam_count = 0;
            bool odd_size = false;
            for (size_t i = 3; i + 4 < size; i++) {
                if (data[i] == 0xFE &&
                    data[i - 1] == 0xA1 &&
                    data[i - 2] == 0xA1) {
                    idam_count++;
                    uint8_t sc = data[i + 4]; /* size code */
                    if (sc != 1 && sc != 2) odd_size = true;
                }
            }
            if (odd_size && idam_count > 0) {
                add_hit(result, "ZX Spectrum", "Non-Standard Sectors", NULL,
                        "Non-standard sector sizes detected (possible "
                        "Speedlock or Alkatraz)",
                        0.60f, (uint8_t)t, (uint8_t)h,
                        UFT_UNI_TECH_CUSTOM_FORMAT,
                        true, false);
                return; /* One hit sufficient for detection */
            }
        }
    }
}

static void detect_cpc(uft_uni_track_accessor_t accessor, void *user,
                       int track_count,
                       uft_protection_unified_result_t *result)
{
    int max_track = track_count > 80 ? 80 : track_count;

    for (int t = 0; t < max_track; t++) {
        for (int h = 0; h < 2; h++) {
            size_t size = 0;
            const uint8_t *data = accessor((uint8_t)t, (uint8_t)h,
                                           &size, user);
            if (!data || size < 100) continue;

            /* CPC protection: look for weak sectors / extra sectors
             * or non-standard sector IDs (CPC uses $C1-$C9) */
            int idam_count = 0;
            bool non_standard_id = false;
            for (size_t i = 3; i + 3 < size; i++) {
                if (data[i] == 0xFE &&
                    data[i - 1] == 0xA1 &&
                    data[i - 2] == 0xA1) {
                    idam_count++;
                    /* CPC standard: sector IDs $C1-$C9 */
                    uint8_t sid = data[i + 3];
                    if (sid < 0xC1 || sid > 0xC9) {
                        non_standard_id = true;
                    }
                }
            }
            if (non_standard_id && idam_count > 0) {
                add_hit(result, "Amstrad CPC",
                        "Non-Standard Sector IDs", NULL,
                        "Sector IDs outside $C1-$C9 range (possible "
                        "Hexagonal or Pauline protection)",
                        0.60f, (uint8_t)t, (uint8_t)h,
                        UFT_UNI_TECH_CUSTOM_FORMAT,
                        true, false);
                return;
            }
            if (idam_count > 9) {
                add_hit(result, "Amstrad CPC",
                        "Extra Sectors", NULL,
                        "More sectors than standard 9 per track",
                        0.65f, (uint8_t)t, (uint8_t)h,
                        UFT_UNI_TECH_EXTRA_SECTORS,
                        true, false);
                return;
            }
        }
    }
}

static void detect_bbc(uft_uni_track_accessor_t accessor, void *user,
                       int track_count,
                       uft_protection_unified_result_t *result)
{
    /* BBC Micro disk protection is rare. Most BBC software
     * was tape-based. Disk protection mainly used non-standard
     * formats (non-DFS, custom ADFS). */
    int max_track = track_count > 80 ? 80 : track_count;

    for (int t = 0; t < max_track; t++) {
        size_t size = 0;
        const uint8_t *data = accessor((uint8_t)t, 0, &size, user);
        if (!data || size < 100) continue;

        /* Count sector headers - BBC DFS uses 10 sectors of 256 bytes */
        int idam_count = 0;
        for (size_t i = 3; i < size; i++) {
            if (data[i] == 0xFE &&
                data[i - 1] == 0xA1 &&
                data[i - 2] == 0xA1) {
                idam_count++;
            }
        }
        if (idam_count > 10) {
            add_hit(result, "BBC Micro", "Extra Sectors", NULL,
                    "More sectors than DFS standard (10/track)",
                    0.55f, (uint8_t)t, 0,
                    UFT_UNI_TECH_EXTRA_SECTORS | UFT_UNI_TECH_CUSTOM_FORMAT,
                    true, false);
            return;
        }
    }
}

static void detect_msx(uft_uni_track_accessor_t accessor, void *user,
                       int track_count,
                       uft_protection_unified_result_t *result)
{
    /* MSX floppy protection is uncommon.
     * Known technique: non-standard sector sizes, extra sectors. */
    int max_track = track_count > 80 ? 80 : track_count;

    for (int t = 0; t < max_track; t++) {
        for (int h = 0; h < 2; h++) {
            size_t size = 0;
            const uint8_t *data = accessor((uint8_t)t, (uint8_t)h,
                                           &size, user);
            if (!data || size < 100) continue;

            int idam_count = 0;
            for (size_t i = 3; i < size; i++) {
                if (data[i] == 0xFE &&
                    data[i - 1] == 0xA1 &&
                    data[i - 2] == 0xA1) {
                    idam_count++;
                }
            }
            if (idam_count > 9) {
                add_hit(result, "MSX", "Extra Sectors", NULL,
                        "More sectors than standard 9 per track",
                        0.55f, (uint8_t)t, (uint8_t)h,
                        UFT_UNI_TECH_EXTRA_SECTORS,
                        true, false);
                return;
            }
        }
    }
}

/*===========================================================================
 * Atari 8-bit Detection
 *===========================================================================*/

static void detect_atari_8bit(uft_uni_track_accessor_t accessor, void *user,
                              int track_count,
                              uft_protection_unified_result_t *result)
{
    int max_track = track_count > 40 ? 40 : track_count;

    for (int t = 0; t < max_track; t++) {
        size_t size = 0;
        const uint8_t *data = accessor((uint8_t)t, 0, &size, user);
        if (!data || size < 100) continue;

        /* Count sector headers (FM: $FE preceded by $F8-$FE pattern) */
        int idam_count = 0;
        bool bad_crc = false;
        for (size_t i = 1; i < size; i++) {
            if (data[i] == 0xFE && (data[i - 1] == 0x00 ||
                                     data[i - 1] == 0xFF)) {
                idam_count++;
            }
        }

        /* Standard Atari: 18 sectors (SD) or 26 sectors (ED) */
        if (idam_count > 0 && idam_count != 18 && idam_count != 26) {
            add_hit(result, "Atari 8-bit", "Non-Standard Sectors", NULL,
                    "Sector count differs from standard 18/26",
                    0.60f, (uint8_t)t, 0,
                    UFT_UNI_TECH_CUSTOM_FORMAT,
                    true, false);
            return;
        }
    }
}

/*===========================================================================
 * Main Dispatch
 *===========================================================================*/

int uft_protection_detect_tracks(uft_uni_track_accessor_t accessor,
                                  void *user,
                                  uft_uni_platform_t platform,
                                  int track_count,
                                  uft_protection_unified_result_t *result)
{
    if (!accessor || !result) return -1;

    memset(result, 0, sizeof(*result));
    result->detected_platform = platform;

    if (platform == UFT_UNI_PLATFORM_UNKNOWN) {
        /* Run all platform detectors */
        detect_c64(accessor, user, track_count, result);
        detect_amiga(accessor, user, track_count, result);
        detect_atari_st(accessor, user, track_count, result);
        detect_apple2(accessor, user, track_count, result);
        detect_atari_8bit(accessor, user, track_count, result);
        detect_pc(accessor, user, track_count, result);
        detect_spectrum(accessor, user, track_count, result);
        detect_cpc(accessor, user, track_count, result);
        detect_bbc(accessor, user, track_count, result);
        detect_msx(accessor, user, track_count, result);
    } else {
        switch (platform) {
        case UFT_UNI_PLATFORM_C64:
            detect_c64(accessor, user, track_count, result);
            break;
        case UFT_UNI_PLATFORM_AMIGA:
            detect_amiga(accessor, user, track_count, result);
            break;
        case UFT_UNI_PLATFORM_ATARI_ST:
            detect_atari_st(accessor, user, track_count, result);
            break;
        case UFT_UNI_PLATFORM_APPLE2:
            detect_apple2(accessor, user, track_count, result);
            break;
        case UFT_UNI_PLATFORM_ATARI_8BIT:
            detect_atari_8bit(accessor, user, track_count, result);
            break;
        case UFT_UNI_PLATFORM_PC:
            detect_pc(accessor, user, track_count, result);
            break;
        case UFT_UNI_PLATFORM_SPECTRUM:
            detect_spectrum(accessor, user, track_count, result);
            break;
        case UFT_UNI_PLATFORM_CPC:
            detect_cpc(accessor, user, track_count, result);
            break;
        case UFT_UNI_PLATFORM_BBC:
            detect_bbc(accessor, user, track_count, result);
            break;
        case UFT_UNI_PLATFORM_MSX:
            detect_msx(accessor, user, track_count, result);
            break;
        default:
            break;
        }
    }

    /* Generate summary */
    if (result->hit_count > 0) {
        snprintf(result->summary, sizeof(result->summary),
                 "%d protection(s) detected. Primary: %s (%.0f%%)",
                 result->hit_count, result->primary_scheme,
                 result->overall_confidence * 100.0f);
    } else {
        snprintf(result->summary, sizeof(result->summary),
                 "No copy protection detected.");
    }

    return 0;
}

/*===========================================================================
 * File-based Detection (stub -- delegates to track-based)
 *===========================================================================*/

int uft_protection_detect_all(const char *image_path,
                               uft_protection_hit_unified_t *hits,
                               int max_hits,
                               int *hit_count)
{
    if (!image_path || !hits || !hit_count) return -1;

    /* This would need to open the image file, parse it according
     * to its format, and provide a track accessor. That depends on
     * the full UFT image loading infrastructure. For now, return
     * a stub result indicating file-based detection requires
     * the image to be loaded first. */
    *hit_count = 0;
    return -2; /* Not implemented -- use uft_protection_detect_tracks() */
}

/*===========================================================================
 * Report Generation
 *===========================================================================*/

size_t uft_protection_unified_report(
    const uft_protection_unified_result_t *result,
    char *buffer, size_t buffer_size)
{
    if (!result || !buffer || buffer_size == 0) return 0;

    size_t written = 0;
    int n;

    n = snprintf(buffer + written, buffer_size - written,
                 "=== Unified Protection Analysis ===\n"
                 "Platform: %s\n"
                 "Protected: %s\n"
                 "Detections: %d\n\n",
                 uft_uni_platform_name(result->detected_platform),
                 result->is_protected ? "YES" : "NO",
                 result->hit_count);
    if (n > 0) written += (size_t)n;

    for (int i = 0; i < result->hit_count && written < buffer_size; i++) {
        const uft_protection_hit_unified_t *h = &result->hits[i];
        n = snprintf(buffer + written, buffer_size - written,
                     "[%d] %s: %s",
                     i + 1, h->platform, h->scheme_name);
        if (n > 0) written += (size_t)n;

        if (h->variant[0]) {
            n = snprintf(buffer + written, buffer_size - written,
                         " (%s)", h->variant);
            if (n > 0) written += (size_t)n;
        }

        n = snprintf(buffer + written, buffer_size - written,
                     "\n    Confidence: %.0f%%\n"
                     "    Track: %u, Head: %u\n"
                     "    Flux preservable: %s\n"
                     "    Native preservable: %s\n",
                     h->confidence * 100.0f,
                     h->track, h->head,
                     h->preservable_in_flux ? "YES" : "NO",
                     h->preservable_in_native ? "YES" : "NO");
        if (n > 0) written += (size_t)n;

        if (h->details[0]) {
            n = snprintf(buffer + written, buffer_size - written,
                         "    Detail: %s\n", h->details);
            if (n > 0) written += (size_t)n;
        }

        n = snprintf(buffer + written, buffer_size - written, "\n");
        if (n > 0) written += (size_t)n;
    }

    if (written < buffer_size) {
        n = snprintf(buffer + written, buffer_size - written,
                     "%s\n", result->summary);
        if (n > 0) written += (size_t)n;
    }

    return written;
}

size_t uft_protection_unified_to_json(
    const uft_protection_unified_result_t *result,
    char *buffer, size_t buffer_size)
{
    if (!result || !buffer || buffer_size == 0) return 0;

    size_t written = 0;
    int n;

    n = snprintf(buffer + written, buffer_size - written,
                 "{\n  \"platform\": \"%s\",\n"
                 "  \"protected\": %s,\n"
                 "  \"overall_confidence\": %.2f,\n"
                 "  \"primary_scheme\": \"%s\",\n"
                 "  \"hit_count\": %d,\n"
                 "  \"hits\": [\n",
                 uft_uni_platform_name(result->detected_platform),
                 result->is_protected ? "true" : "false",
                 result->overall_confidence,
                 result->primary_scheme,
                 result->hit_count);
    if (n > 0) written += (size_t)n;

    for (int i = 0; i < result->hit_count && written < buffer_size; i++) {
        const uft_protection_hit_unified_t *h = &result->hits[i];
        n = snprintf(buffer + written, buffer_size - written,
                     "    {\n"
                     "      \"platform\": \"%s\",\n"
                     "      \"scheme\": \"%s\",\n"
                     "      \"variant\": \"%s\",\n"
                     "      \"confidence\": %.2f,\n"
                     "      \"track\": %u,\n"
                     "      \"head\": %u,\n"
                     "      \"technique_flags\": %u,\n"
                     "      \"flux_preservable\": %s,\n"
                     "      \"native_preservable\": %s,\n"
                     "      \"details\": \"%s\"\n"
                     "    }%s\n",
                     h->platform, h->scheme_name,
                     h->variant, h->confidence,
                     h->track, h->head,
                     h->technique_flags,
                     h->preservable_in_flux ? "true" : "false",
                     h->preservable_in_native ? "true" : "false",
                     h->details,
                     (i + 1 < result->hit_count) ? "," : "");
        if (n > 0) written += (size_t)n;
    }

    n = snprintf(buffer + written, buffer_size - written,
                 "  ],\n  \"summary\": \"%s\"\n}\n",
                 result->summary);
    if (n > 0) written += (size_t)n;

    return written;
}

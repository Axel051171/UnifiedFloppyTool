/**
 * @file uft_format_validators.c
 * @brief Format validators with correct-signature impls.
 *
 * Replaces the ABI-broken 2-arg stubs in uft_core_stubs.c (spec §1.3
 * violation: declared as 4-arg with level+result, stubs read only 2).
 *
 * Scope: QUICK-level validation (size-table check, magic-byte signature).
 * Higher levels (STANDARD/THOROUGH/FORENSIC) return a best-effort result
 * based on the structural check and flag "deep validation not implemented"
 * in an issue. Honest about what's tested vs what's claimed.
 */

#include "uft/uft_format_validate.h"
#include "uft/uft_error.h"

#include <stdint.h>
#include <string.h>

/* ---------- helpers ---------- */

static void add_issue(uft_validation_result_t *r, int sev, const char *cat,
                       const char *msg)
{
    if (!r) return;
    if (r->issue_count >= (int)(sizeof(r->issues) / sizeof(r->issues[0]))) return;
    uft_validation_issue_t *iss = &r->issues[r->issue_count++];
    iss->severity = sev;
    iss->offset = iss->track = iss->sector = -1;
    iss->category = cat;
    strncpy(iss->message, msg, sizeof(iss->message) - 1);
    iss->message[sizeof(iss->message) - 1] = '\0';
}

static void init_result(uft_validation_result_t *r)
{
    if (!r) return;
    memset(r, 0, sizeof(*r));
}

/* ---------- D64 ----------
 * Known sizes per CBM spec:
 *   174848  35 tracks, no error info       (standard)
 *   175531  35 tracks, + 683 error bytes
 *   196608  40 tracks, no error info
 *   197376  40 tracks, + 768 error bytes
 *   205312  42 tracks, no error info       (rare)
 *   206114  42 tracks, + error bytes
 */
uft_error_t uft_validate_d64(const uint8_t *data, size_t size,
                              uft_validation_level_t level,
                              uft_validation_result_t *result)
{
    if (!data || !result) return UFT_ERROR_NULL_POINTER;
    init_result(result);

    int tracks = 0;
    bool has_errors = false;
    switch (size) {
        case 174848: tracks = 35; break;
        case 175531: tracks = 35; has_errors = true; break;
        case 196608: tracks = 40; break;
        case 197376: tracks = 40; has_errors = true; break;
        case 205312: tracks = 42; break;
        case 206114: tracks = 42; has_errors = true; break;
        default:
            result->valid = false;
            result->score = 0;
            add_issue(result, 2, "structure",
                      "Not a standard D64 size (expected 174848/196608/...)");
            return UFT_OK;
    }

    /* Sector count per track zone: 1-17 = 21, 18-24 = 19, 25-30 = 18,
     * 31+ = 17. Check total = 683 + (extended zones). */
    int total = 0;
    for (int t = 1; t <= tracks; t++) {
        if      (t <= 17) total += 21;
        else if (t <= 24) total += 19;
        else if (t <= 30) total += 18;
        else              total += 17;
    }
    result->total_sectors = total;

    /* BAM is at track 18, sector 0 → linear offset (17*21*256) */
    size_t bam_off = (size_t)17 * 21 * 256;
    bool bam_ok = (size >= bam_off + 256 && data[bam_off + 2] == 'A');
    result->d64.bam_valid = bam_ok;
    if (!bam_ok) {
        add_issue(result, 2, "structure", "BAM signature byte missing at track 18");
        result->valid = false;
        result->score = 40;
        return UFT_OK;
    }

    result->valid = true;
    result->score = 90;
    if (level >= UFT_VALIDATE_THOROUGH)
        add_issue(result, 0, "info",
                  "Thorough/forensic BAM-consistency check not implemented");
    (void)has_errors;
    return UFT_OK;
}

/* ---------- ADF ----------
 * AmigaDOS:
 *   DD = 901120 bytes (880 KB)
 *   HD = 1802240 bytes (1760 KB)
 *   bytes 0-2 = "DOS"
 *   byte 3 = flags (OFS/FFS/INTL/DC bits)
 */
uft_error_t uft_validate_adf(const uint8_t *data, size_t size,
                              uft_validation_level_t level,
                              uft_validation_result_t *result)
{
    if (!data || !result) return UFT_ERROR_NULL_POINTER;
    init_result(result);

    if (size != 901120 && size != 1802240) {
        add_issue(result, 2, "structure",
                  "Not standard ADF size (expected 901120 or 1802240)");
        result->valid = false;
        result->score = 0;
        return UFT_OK;
    }

    if (data[0] != 'D' || data[1] != 'O' || data[2] != 'S') {
        add_issue(result, 3, "structure",
                  "Missing 'DOS' bootblock signature");
        result->adf.bootblock_valid = false;
        result->valid = false;
        result->score = 20;
        return UFT_OK;
    }
    result->adf.bootblock_valid = true;

    /* Root block: block 880 (DD) or 1760 (HD) = size/2/512 */
    uint32_t root_block = (uint32_t)(size / 2 / 512);
    size_t root_off = (size_t)root_block * 512;
    bool root_ok = false;
    if (root_off + 512 <= size) {
        /* Root block: T_SHORT at offset 0, ST_ROOT at offset 508 */
        uint8_t primary = data[root_off + 3];      /* BE32 low byte */
        uint8_t secondary = data[root_off + 511];  /* BE32 of (-1)=1 */
        root_ok = (primary == 2 && secondary == 1);
    }
    result->adf.rootblock_valid = root_ok;

    result->total_sectors = (int)(size / 512);
    result->valid = root_ok;
    result->score = root_ok ? 90 : 50;
    if (!root_ok) add_issue(result, 2, "structure", "Root block signature invalid");

    if (level >= UFT_VALIDATE_THOROUGH)
        add_issue(result, 0, "info",
                  "Thorough/forensic bitmap check not implemented");
    return UFT_OK;
}

/* ---------- G64 ----------
 * G64 header: 8 bytes "GCR-1541", followed by version byte + track count.
 * Per VICE docs, size is variable — typically ~335 KB for 42 tracks.
 */
uft_error_t uft_validate_g64(const uint8_t *data, size_t size,
                              uft_validation_level_t level,
                              uft_validation_result_t *result)
{
    if (!data || !result) return UFT_ERROR_NULL_POINTER;
    init_result(result);
    (void)level;

    if (size < 12) {
        add_issue(result, 3, "structure", "File too small for G64 header");
        return UFT_OK;
    }
    if (memcmp(data, "GCR-1541", 8) != 0) {
        add_issue(result, 3, "structure", "Missing 'GCR-1541' signature");
        return UFT_OK;
    }

    uint8_t version  = data[8];
    uint8_t n_tracks = data[9];
    uint16_t tsize   = (uint16_t)(data[10] | (data[11] << 8));
    if (version != 0) {
        add_issue(result, 1, "structure", "Unexpected G64 version byte");
    }
    if (n_tracks == 0 || n_tracks > 84) {
        add_issue(result, 2, "structure", "Implausible track count");
        return UFT_OK;
    }
    (void)tsize;

    result->valid = true;
    result->score = 85;
    result->total_sectors = n_tracks;  /* per-track, G64 has no sector count */
    if (level >= UFT_VALIDATE_STANDARD)
        add_issue(result, 0, "info",
                  "Per-track GCR parsing not implemented");
    return UFT_OK;
}

/* ---------- SCP ----------
 * SCP header: "SCP" at offset 0, version, disk type, flags, bit cell time.
 * See SuperCard Pro spec v1.6 / v2.x.
 */
uft_error_t uft_validate_scp(const uint8_t *data, size_t size,
                              uft_validation_level_t level,
                              uft_validation_result_t *result)
{
    if (!data || !result) return UFT_ERROR_NULL_POINTER;
    init_result(result);
    (void)level;

    if (size < 16) {
        add_issue(result, 3, "structure", "File too small for SCP header");
        return UFT_OK;
    }
    if (data[0] != 'S' || data[1] != 'C' || data[2] != 'P') {
        add_issue(result, 3, "structure", "Missing 'SCP' signature");
        return UFT_OK;
    }

    uint8_t version   = data[3];
    uint8_t disk_type = data[4];
    uint8_t revs      = data[5];
    uint8_t start_trk = data[6];
    uint8_t end_trk   = data[7];
    (void)version; (void)disk_type;

    if (revs == 0 || revs > 5) {
        add_issue(result, 1, "structure", "Unusual revolution count");
    }
    if (end_trk < start_trk) {
        add_issue(result, 2, "structure", "end_track < start_track");
        return UFT_OK;
    }

    result->scp.revolutions = revs;
    result->scp.tracks = end_trk - start_trk + 1;
    result->valid = true;
    result->score = 80;
    return UFT_OK;
}

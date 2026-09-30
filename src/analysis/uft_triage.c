/**
 * @file uft_triage.c
 * @brief Triage Mode — Quick Disk Quality Assessment
 *
 * Reads only 3 key tracks (first, directory/middle, last) to give
 * a rapid quality assessment in ~10 seconds instead of full analysis.
 *
 * Supported formats:
 *   - D64 (Commodore): Track 1, Track 18 (BAM/dir), Track 35
 *   - ADF (Amiga):     Track 0/0 (bootblock), Track 40/0, Track 79/1
 *   - IMG/ST (PC):     Sector 0 (boot), middle sector, last sector
 *   - SCP/HFE (Flux):  Track 0, Track 40, Track 79 (or last)
 *
 * @author UFT Project
 * @date 2026
 */

#include "uft/analysis/uft_triage.h"
#include "uft/uft_error.h"
#include "uft/formats/uft_detect.h"
#include "uft/formats/cbm/uft_cbm_geometry.h"   /* uft_cbm_error_byte_ok */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define TRIAGE_READ_BUF     (1024 * 1024)  /* 1 MB max read for header probe */
#define D64_SECTOR_SIZE     256
#define ADF_SECTOR_SIZE     512
#define ADF_TRACK_SECTORS   11             /* Sectors per track (DD) */
#define ADF_TRACK_SIZE      (ADF_TRACK_SECTORS * ADF_SECTOR_SIZE)
#define IMG_SECTOR_SIZE     512

/* D64 track byte-offsets (1-indexed, tracks 1..35) */
static const uint32_t D64_TRACK_OFFSET[36] = {
    0,
    0x00000, 0x01500, 0x02A00, 0x03F00, 0x05400,
    0x06900, 0x07E00, 0x09300, 0x0A800, 0x0BD00,
    0x0D200, 0x0E700, 0x0FC00, 0x11100, 0x12600,
    0x13B00, 0x15000, 0x16500, 0x17800, 0x18B00,
    0x19E00, 0x1B100, 0x1C400, 0x1D700, 0x1EA00,
    0x1FC00, 0x20E00, 0x22000, 0x23200, 0x24400,
    0x25600, 0x26700, 0x27800, 0x28900, 0x29A00
};

/* Sectors per track on a 1541 (track 1..35) */
static int d64_sectors_on_track(int track) {
    if (track >=  1 && track <= 17) return 21;
    if (track >= 18 && track <= 24) return 19;
    if (track >= 25 && track <= 30) return 18;
    if (track >= 31 && track <= 40) return 17;
    return 0;
}

/* Magic bytes */
static const uint8_t SCP_MAGIC[3]  = { 'S', 'C', 'P' };
static const uint8_t HFE_MAGIC[8]  = { 'H','X','C','P','I','C','F','E' };

/*===========================================================================
 * Internal Format Enumeration (kept simple for triage)
 *===========================================================================*/

typedef enum {
    TRIAGE_FMT_UNKNOWN = 0,
    TRIAGE_FMT_D64,
    TRIAGE_FMT_ADF,
    TRIAGE_FMT_IMG,
    TRIAGE_FMT_SCP,
    TRIAGE_FMT_HFE
} triage_fmt_t;

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

/** Detect format from raw data + size (quick, no full probe registry). */
static triage_fmt_t triage_detect(const uint8_t *data, size_t size,
                                  const char *hint)
{
    /* Extension hint first */
    if (hint) {
        const char *dot = strrchr(hint, '.');
        if (dot) {
            dot++;
            if (!strcmp(dot, "d64") || !strcmp(dot, "D64")) return TRIAGE_FMT_D64;
            if (!strcmp(dot, "adf") || !strcmp(dot, "ADF")) return TRIAGE_FMT_ADF;
            if (!strcmp(dot, "img") || !strcmp(dot, "IMG") ||
                !strcmp(dot, "ima") || !strcmp(dot, "IMA") ||
                !strcmp(dot, "st")  || !strcmp(dot, "ST"))  return TRIAGE_FMT_IMG;
            if (!strcmp(dot, "scp") || !strcmp(dot, "SCP")) return TRIAGE_FMT_SCP;
            if (!strcmp(dot, "hfe") || !strcmp(dot, "HFE")) return TRIAGE_FMT_HFE;
        }
    }

    /* Magic-byte fallback */
    if (size >= 8 && memcmp(data, HFE_MAGIC, 8) == 0) return TRIAGE_FMT_HFE;
    if (size >= 3 && memcmp(data, SCP_MAGIC, 3) == 0) return TRIAGE_FMT_SCP;

    /* Size-based heuristics */
    if (size == 174848 || size == 175531 ||
        size == 196608 || size == 197376)              return TRIAGE_FMT_D64;
    if (size == 901120 || size == 1802240)             return TRIAGE_FMT_ADF;
    if (size == 163840 || size == 184320 ||
        size == 327680 || size == 368640 ||
        size == 737280 || size == 1228800 ||
        size == 1474560 || size == 2949120)            return TRIAGE_FMT_IMG;

    return TRIAGE_FMT_UNKNOWN;
}

/*
 * P3-708: a blank sector is not a damaged sector.
 *
 * Three fill checks stood here, one per format, and each counted a sector of
 * nothing but 0x00 (D64: also 0xFF) as BAD. That is exactly how an unused
 * sector of a sparse disk looks, so the free, undamaged xdftool ADF in
 * tests/corpus_free scored 9/100 and a clean VICE D64 3/100 ("severely
 * damaged"). A sector dump records no read status; the damage it can show
 * is data MISSING from the file, and for a D64 with a trailing error block
 * the 1541 error bytes. Blank sectors are counted and reported, never
 * scored.
 */
/* What a sector dump can say about itself. */
typedef struct {
    int  blank;        /* sampled sectors of nothing but fill — not damage */
    bool read_status;  /* the file carries per-sector read status (D64 error block) */
} dump_facts_t;

static bool sector_blank(const uint8_t *s, size_t n, bool ff_too) {
    bool zero = true, ff = ff_too;
    for (size_t i = 0; i < n && (zero || ff); i++) {
        if (s[i] != 0x00) zero = false;
        if (s[i] != 0xFF) ff = false;
    }
    return zero || ff;
}

/** Check if D64 BAM/directory track shows copy-protection signs. */
static bool d64_check_protection(const uint8_t *data, size_t size) {
    if (size < 0x16500 + D64_SECTOR_SIZE) return false;
    const uint8_t *bam = data + 0x16500;
    /* A blank BAM sector means "not formatted", not "protected" (P3-708). */
    if (sector_blank(bam, D64_SECTOR_SIZE, true)) return false;
    /* Non-standard BAM pointer (track != 18 or sector > 19) */
    if (bam[0] != 0x12) return true;
    if (bam[1] > 19) return true;
    /* DOS type byte at 0x02 should be 'A' (0x41) for standard 1541 */
    if (bam[2] != 0x41) return true;
    return false;
}

/** Check ADF bootblock for protection markers. */
static bool adf_check_protection(const uint8_t *data, size_t size) {
    if (size < 1024) return false;
    /* A blank bootblock means "no bootblock", not "protected" (P3-708). */
    if (sector_blank(data, 1024, false)) return false;
    /* Standard bootblock starts with "DOS\0" (OFS/FFS) */
    if (memcmp(data, "DOS", 3) != 0) return true;
    /* Filesystem type nibble should be 0..5 */
    if (data[3] > 5) return true;
    return false;
}

/** SCP: quick check of track header presence. */
static int scp_track_quality(const uint8_t *data, size_t size, int track_num) {
    /* SCP track offset table starts at byte 16, 4 bytes per track */
    size_t tbl_off = 16 + (size_t)track_num * 4;
    if (tbl_off + 4 > size) return 0;
    uint32_t trk_offset = (uint32_t)data[tbl_off]
                        | ((uint32_t)data[tbl_off + 1] << 8)
                        | ((uint32_t)data[tbl_off + 2] << 16)
                        | ((uint32_t)data[tbl_off + 3] << 24);
    if (trk_offset == 0) return 0;  /* Track not captured */
    if (trk_offset >= size) return 0;  /* Offset out of range */
    /* Track header: 3-byte "TRK" + track number */
    if (trk_offset + 4 <= size &&
        data[trk_offset] == 'T' &&
        data[trk_offset + 1] == 'R' &&
        data[trk_offset + 2] == 'K') {
        return 100;
    }
    /* Non-standard but data present */
    return 60;
}

/** SCP: check for unusual disk type suggesting copy protection. */
static bool scp_check_protection(const uint8_t *data, size_t size) {
    if (size < 16) return false;
    /* Byte 8: flags — bit 4 set means read/write (non-archival) */
    /* Byte 9: number of revolutions — >10 is unusual */
    uint8_t revs = data[5];
    if (revs > 10) return true;
    return false;
}

/*===========================================================================
 * Triage Core: D64
 *===========================================================================*/

static void triage_d64(const uint8_t *data, size_t size,
                       uft_triage_result_t *r, dump_facts_t *f)
{
    int tracks[3] = { 1, 18, 35 };
    int total_sectors = 0, ok = 0, bad = 0;

    snprintf(r->format_name, sizeof(r->format_name), "D64");
    r->format_confidence = (size == 174848 || size == 175531) ? 95.0f : 70.0f;
    r->tracks_sampled = 3;

    /* The only read status a D64 can carry: a trailing block of one 1541
     * error byte per sector (174848 + 683 = 175531, 196608 + 768 = 197376),
     * laid out as the D64 plugin reads it. */
    int tracks_in_file = (size >= 196608) ? 40 : 35;
    size_t sectors_in_file = 0;
    for (int t = 1; t <= tracks_in_file; t++)
        sectors_in_file += (size_t)d64_sectors_on_track(t);
    size_t err_off = sectors_in_file * D64_SECTOR_SIZE;
    f->read_status = size >= err_off + sectors_in_file;

    for (int i = 0; i < 3; i++) {
        int t = tracks[i];
        int spt = d64_sectors_on_track(t);
        uint32_t base = D64_TRACK_OFFSET[t];

        for (int s = 0; s < spt; s++) {
            uint32_t off = base + (uint32_t)s * D64_SECTOR_SIZE;
            total_sectors++;
            if (off + D64_SECTOR_SIZE > size) { bad++; continue; }
            if (f->read_status &&
                !uft_cbm_error_byte_ok(data[err_off + off / D64_SECTOR_SIZE])) {
                bad++;
                continue;
            }
            ok++;
            if (sector_blank(data + off, D64_SECTOR_SIZE, true)) f->blank++;
        }
    }

    r->sectors_ok  = ok;
    r->sectors_bad = bad;
    r->sectors_weak = 0;

    /* Protection check */
    r->protection_detected = d64_check_protection(data, size);
    if (r->protection_detected) {
        snprintf(r->protection_name, sizeof(r->protection_name),
                 "Non-standard BAM/DOS");
    }

    /* Quality score: share of sampled sectors without a finding (the comment
     * here said track 18 counts double; the code never weighted it). */
    if (total_sectors > 0) {
        r->quality_score = (ok * 100) / total_sectors;
    }
}

/*===========================================================================
 * Triage Core: ADF
 *===========================================================================*/

static void triage_adf(const uint8_t *data, size_t size,
                       uft_triage_result_t *r, dump_facts_t *f)
{
    /* Key tracks: 0/0 (bootblock), 40/0 (middle), 79/1 (last) */
    /* ADF layout: track = cyl*2 + head, each track = 11 * 512 bytes */
    int track_indices[3] = { 0, 80, 159 };  /* physical track numbers */
    int total_sectors = 0, ok = 0, bad = 0;

    snprintf(r->format_name, sizeof(r->format_name), "ADF");
    r->format_confidence = (size == 901120) ? 95.0f : 70.0f;
    r->tracks_sampled = 3;

    for (int i = 0; i < 3; i++) {
        uint32_t base = (uint32_t)track_indices[i] * ADF_TRACK_SIZE;
        for (int s = 0; s < ADF_TRACK_SECTORS; s++) {
            uint32_t off = base + (uint32_t)s * ADF_SECTOR_SIZE;
            total_sectors++;
            if (off + ADF_SECTOR_SIZE > size) { bad++; continue; }
            ok++;
            if (sector_blank(data + off, ADF_SECTOR_SIZE, false)) f->blank++;
        }
    }

    r->sectors_ok  = ok;
    r->sectors_bad = bad;
    r->sectors_weak = 0;

    r->protection_detected = adf_check_protection(data, size);
    if (r->protection_detected) {
        snprintf(r->protection_name, sizeof(r->protection_name),
                 "Non-standard bootblock");
    }

    if (total_sectors > 0) {
        r->quality_score = (ok * 100) / total_sectors;
    }
}

/*===========================================================================
 * Triage Core: IMG/ST (raw sector image)
 *===========================================================================*/

static void triage_img(const uint8_t *data, size_t size,
                       uft_triage_result_t *r, dump_facts_t *f)
{
    /* Sample 3 sectors: first, middle, last */
    size_t offsets[3];
    size_t num_sectors = size / IMG_SECTOR_SIZE;
    if (num_sectors == 0) { r->quality_score = 0; return; }

    offsets[0] = 0;
    offsets[1] = (num_sectors / 2) * IMG_SECTOR_SIZE;
    offsets[2] = (num_sectors - 1) * IMG_SECTOR_SIZE;

    int ok = 0, bad = 0;

    snprintf(r->format_name, sizeof(r->format_name), "IMG");
    r->format_confidence = 80.0f;
    r->tracks_sampled = 3;

    for (int i = 0; i < 3; i++) {
        if (offsets[i] + IMG_SECTOR_SIZE > size) { bad++; continue; }
        ok++;
        if (sector_blank(data + offsets[i], IMG_SECTOR_SIZE, false)) f->blank++;
    }

    /* Boot sector signature check */
    if (size >= IMG_SECTOR_SIZE) {
        if (data[510] == 0x55 && data[511] == 0xAA) {
            r->format_confidence = 95.0f;
        }
    }

    r->sectors_ok  = ok;
    r->sectors_bad = bad;
    r->sectors_weak = 0;
    r->protection_detected = false;

    if ((ok + bad) > 0) {
        r->quality_score = (ok * 100) / (ok + bad);
    }
}

/*===========================================================================
 * Triage Core: SCP (flux)
 *===========================================================================*/

static void triage_scp(const uint8_t *data, size_t size,
                       uft_triage_result_t *r)
{
    /* SCP header: byte 5 = start track, byte 6 = end track */
    int start_track = 0, end_track = 79;
    if (size >= 8) {
        start_track = data[6];
        end_track   = data[7];
    }
    int mid_track = (start_track + end_track) / 2;

    int tracks[3] = { start_track, mid_track, end_track };
    int ok = 0, bad = 0, weak = 0;

    snprintf(r->format_name, sizeof(r->format_name), "SCP");
    r->format_confidence = 95.0f;
    r->tracks_sampled = 3;

    for (int i = 0; i < 3; i++) {
        int q = scp_track_quality(data, size, tracks[i]);
        if (q >= 80) ok++;
        else if (q >= 40) weak++;
        else bad++;
    }

    r->sectors_ok  = ok;
    r->sectors_bad = bad;
    r->sectors_weak = weak;

    r->protection_detected = scp_check_protection(data, size);
    if (r->protection_detected) {
        snprintf(r->protection_name, sizeof(r->protection_name),
                 "Unusual SCP parameters");
    }

    if ((ok + bad + weak) > 0) {
        r->quality_score = ((ok * 100) + (weak * 50)) / (ok + bad + weak);
    }
}

/*===========================================================================
 * Triage Core: HFE (flux)
 *===========================================================================*/

static void triage_hfe(const uint8_t *data, size_t size,
                       uft_triage_result_t *r)
{
    /* HFE header: byte 9 = number of tracks */
    int num_tracks = 80;
    if (size >= 10) {
        num_tracks = data[9];
        if (num_tracks == 0) num_tracks = 80;
    }

    snprintf(r->format_name, sizeof(r->format_name), "HFE");
    r->format_confidence = 95.0f;
    r->tracks_sampled = 3;

    /* HFE track offset LUT starts at byte 512 (0x200), 4 bytes per entry */
    int tracks[3] = { 0, num_tracks / 2, num_tracks - 1 };
    int ok = 0, bad = 0;

    for (int i = 0; i < 3; i++) {
        size_t lut_off = 0x200 + (size_t)tracks[i] * 4;
        if (lut_off + 4 > size) { bad++; continue; }
        uint16_t trk_offset = (uint16_t)data[lut_off]
                             | ((uint16_t)data[lut_off + 1] << 8);
        uint16_t trk_len    = (uint16_t)data[lut_off + 2]
                             | ((uint16_t)data[lut_off + 3] << 8);
        if (trk_offset == 0 || trk_len == 0) { bad++; continue; }
        /* Offset is in units of 512-byte blocks */
        size_t byte_off = (size_t)trk_offset * 512;
        if (byte_off >= size) { bad++; continue; }
        ok++;
    }

    r->sectors_ok  = ok;
    r->sectors_bad = bad;
    r->sectors_weak = 0;
    r->protection_detected = false;

    if ((ok + bad) > 0) {
        r->quality_score = (ok * 100) / (ok + bad);
    }
}

/*===========================================================================
 * Classification & Summary
 *===========================================================================*/

static void triage_classify(uft_triage_result_t *r) {
    /* WHITE overrides everything if protection detected */
    if (r->protection_detected) {
        r->level = UFT_TRIAGE_WHITE;
        snprintf(r->summary, sizeof(r->summary),
                 "Copy protection detected (%s). Quality: %d/100.",
                 r->protection_name, r->quality_score);
        snprintf(r->recommendation, sizeof(r->recommendation),
                 "Use flux-level archiving (SCP/KryoFlux). "
                 "Standard sector copy will lose protection data.");
        return;
    }

    if (r->quality_score > 80) {
        r->level = UFT_TRIAGE_GREEN;
        snprintf(r->summary, sizeof(r->summary),
                 "%s image in good condition. Score: %d/100, "
                 "%d/%d sectors OK across %d sampled tracks.",
                 r->format_name, r->quality_score,
                 r->sectors_ok, r->sectors_ok + r->sectors_bad,
                 r->tracks_sampled);
        snprintf(r->recommendation, sizeof(r->recommendation),
                 "Standard imaging is sufficient. "
                 "Verify with full read for archival use.");
    } else if (r->quality_score >= 40) {
        r->level = UFT_TRIAGE_YELLOW;
        snprintf(r->summary, sizeof(r->summary),
                 "%s image has some issues. Score: %d/100, "
                 "%d bad sectors in %d sampled tracks.",
                 r->format_name, r->quality_score,
                 r->sectors_bad, r->tracks_sampled);
        snprintf(r->recommendation, sizeof(r->recommendation),
                 "DeepRead multi-pass recovery recommended. "
                 "Consider flux capture for best results.");
    } else {
        r->level = UFT_TRIAGE_RED;
        snprintf(r->summary, sizeof(r->summary),
                 "%s image severely damaged. Score: %d/100, "
                 "%d/%d sectors unreadable in %d sampled tracks.",
                 r->format_name, r->quality_score,
                 r->sectors_bad, r->sectors_ok + r->sectors_bad,
                 r->tracks_sampled);
        snprintf(r->recommendation, sizeof(r->recommendation),
                 "Professional recovery needed. Use DeepRead with "
                 "maximum retries and flux-level capture.");
    }
}

/*
 * P3-708: the texts above speak of "sectors unreadable" and "good
 * condition". For a sector dump neither is known: it holds what was read,
 * not how the reading went. Say what the dump shows instead. A protection
 * finding keeps its own text.
 */
static void triage_describe_dump(uft_triage_result_t *r, const dump_facts_t *f)
{
    if (r->protection_detected) return;
    int total = r->sectors_ok + r->sectors_bad;
    if (f->read_status) {
        snprintf(r->summary, sizeof(r->summary),
                 "%s sector dump with 1541 error bytes: %d of %d sampled "
                 "sectors without error code, %d with an error code or "
                 "missing, %d blank.",
                 r->format_name, r->sectors_ok, total, r->sectors_bad,
                 f->blank);
        return;
    }
    snprintf(r->summary, sizeof(r->summary),
             "%s sector dump: %d of %d sampled sectors present, %d missing "
             "from the file, %d blank. A sector dump records no read status; "
             "the original disk's condition cannot be judged from it.",
             r->format_name, r->sectors_ok, total, r->sectors_bad, f->blank);
    if (r->sectors_bad > 0)
        snprintf(r->recommendation, sizeof(r->recommendation),
                 "The file lacks data its format requires. Re-create it from "
                 "the original disk or its source; the gap says nothing about "
                 "damage on the disk itself.");
    else
        snprintf(r->recommendation, sizeof(r->recommendation),
                 "Every sampled sector is present. To learn the original "
                 "disk's condition, capture it at flux level.");
}

/*===========================================================================
 * Public API
 *===========================================================================*/

int uft_triage_analyze_buffer(const uint8_t *data, size_t size,
                               const char *format_hint,
                               uft_triage_result_t *result)
{
    if (!data || size == 0 || !result) return UFT_ERR_INVALID_PARAM;

    memset(result, 0, sizeof(*result));

    triage_fmt_t fmt = triage_detect(data, size, format_hint);
    dump_facts_t dump = { 0, false };

    switch (fmt) {
        case TRIAGE_FMT_D64: triage_d64(data, size, result, &dump); break;
        case TRIAGE_FMT_ADF: triage_adf(data, size, result, &dump); break;
        case TRIAGE_FMT_IMG: triage_img(data, size, result, &dump); break;
        case TRIAGE_FMT_SCP: triage_scp(data, size, result); break;
        case TRIAGE_FMT_HFE: triage_hfe(data, size, result); break;
        default:
            snprintf(result->format_name, sizeof(result->format_name),
                     "Unknown");
            result->quality_score = 0;
            result->level = UFT_TRIAGE_RED;
            snprintf(result->summary, sizeof(result->summary),
                     "Unrecognized format. Cannot perform triage.");
            snprintf(result->recommendation, sizeof(result->recommendation),
                     "Try manual format selection or use full analysis.");
            return UFT_ERR_NOT_SUPPORTED;
    }

    triage_classify(result);
    if (fmt == TRIAGE_FMT_D64 || fmt == TRIAGE_FMT_ADF || fmt == TRIAGE_FMT_IMG)
        triage_describe_dump(result, &dump);
    return 0;
}

int uft_triage_analyze(const char *path, uft_triage_result_t *result)
{
    if (!path || !result) return UFT_ERR_INVALID_PARAM;

    memset(result, 0, sizeof(*result));

    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_OPEN;

    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERR_IO; }
    long fsize = ftell(f);
    if (fsize <= 0) { fclose(f); return UFT_ERR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERR_IO; }

    size_t size = (size_t)fsize;

    /* Read entire file into memory (floppy images are small, typically <2 MB) */
    uint8_t *data = (uint8_t *)malloc(size);
    if (!data) { fclose(f); return UFT_ERR_MEMORY; }

    size_t nread = fread(data, 1, size, f);
    fclose(f);

    if (nread != size) {
        free(data);
        return UFT_ERR_FILE_READ;
    }

    int rc = uft_triage_analyze_buffer(data, size, path, result);
    free(data);
    return rc;
}

const char *uft_triage_level_name(uft_triage_level_t level)
{
    switch (level) {
        case UFT_TRIAGE_GREEN:  return "GREEN";
        case UFT_TRIAGE_YELLOW: return "YELLOW";
        case UFT_TRIAGE_RED:    return "RED";
        case UFT_TRIAGE_WHITE:  return "WHITE";
        default:                return "UNKNOWN";
    }
}

const char *uft_triage_level_emoji(uft_triage_level_t level)
{
    switch (level) {
        case UFT_TRIAGE_GREEN:  return "\xF0\x9F\x9F\xA2";  /* U+1F7E2 green circle */
        case UFT_TRIAGE_YELLOW: return "\xF0\x9F\x9F\xA1";  /* U+1F7E1 yellow circle */
        case UFT_TRIAGE_RED:    return "\xF0\x9F\x94\xB4";   /* U+1F534 red circle */
        case UFT_TRIAGE_WHITE:  return "\xE2\x9A\xAA";       /* U+26AA white circle */
        default:                return "\xE2\x9D\x93";       /* U+2753 question mark */
    }
}

/**
 * @file uft_sector_compare.c
 * @brief Sector-by-sector disk image comparison with detailed diff report
 *
 * Compares two disk images at the sector level, producing a structured
 * result with per-sector diff details, byte-level statistics, and
 * similarity scoring. Supports format-aware comparison for D64 (variable
 * sectors/track) and raw comparison for fixed-geometry formats (ADF, IMG, ST).
 *
 * Exports: plain-text report, JSON report.
 *
 * @date 2026-04-10
 */

#include "uft/analysis/uft_sector_compare.h"
#include "uft/uft_format_detect.h"
#include "uft/core/uft_error_codes.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * D64 Track/Sector Layout (Commodore 1541)
 *===========================================================================*/

/** Sectors per track for a standard 35-track D64 */
static uint8_t d64_sectors_for_track(int track)
{
    if (track >= 1 && track <= 17) return 21;
    if (track >= 18 && track <= 24) return 19;
    if (track >= 25 && track <= 30) return 18;
    if (track >= 31 && track <= 42) return 17;
    return 0;
}

/** Number of tracks for a D64 by file size (ignoring error bytes) */
static int d64_track_count(size_t file_size)
{
    /* 174848 / 175531  = 35 tracks
     * 196608 / 197376  = 40 tracks
     * 205312 / 206114  = 42 tracks */
    if (file_size <= 175531)  return 35;
    if (file_size <= 197376)  return 40;
    if (file_size <= 206114)  return 42;
    return 35; /* fallback */
}

/** Total sector-data bytes for a D64 with the given track count */
static size_t d64_data_size(int tracks)
{
    size_t total = 0;
    for (int t = 1; t <= tracks; t++)
        total += (size_t)d64_sectors_for_track(t) * 256;
    return total;
}

/*===========================================================================
 * Internal: record a sector diff
 *===========================================================================*/

static void record_diff(uft_compare_result_t *result,
                         uint8_t track, uint8_t head, uint8_t sector,
                         uint32_t offset, uint16_t diff_bytes,
                         uint16_t sector_size,
                         bool only_a, bool only_b)
{
    if (result->diff_count >= UFT_COMPARE_MAX_DIFFS)
        return;

    uft_sector_diff_t *d = &result->diffs[result->diff_count++];
    d->track       = track;
    d->head        = head;
    d->sector      = sector;
    d->offset      = offset;
    d->diff_count  = diff_bytes;
    d->sector_size = sector_size;
    d->only_in_a   = only_a;
    d->only_in_b   = only_b;
}

/*===========================================================================
 * Internal: format-aware D64 compare
 *===========================================================================*/

static void compare_d64(const uint8_t *a, size_t sa,
                         const uint8_t *b, size_t sb,
                         uft_compare_result_t *result)
{
    int tracks_a = d64_track_count(sa);
    int tracks_b = d64_track_count(sb);
    int max_tracks = (tracks_a > tracks_b) ? tracks_a : tracks_b;

    size_t data_a = d64_data_size(tracks_a);
    size_t data_b = d64_data_size(tracks_b);
    if (data_a > sa) data_a = sa;
    if (data_b > sb) data_b = sb;

    uint32_t off_a = 0, off_b = 0;

    for (int t = 1; t <= max_tracks; t++) {
        uint8_t spt = d64_sectors_for_track((uint8_t)t);
        if (spt == 0) break;

        for (uint8_t s = 0; s < spt; s++) {
            bool have_a = (off_a + 256 <= data_a);
            bool have_b = (off_b + 256 <= data_b);

            result->total_sectors++;
            result->total_bytes += 256;

            if (have_a && have_b) {
                /* Both images have this sector */
                if (memcmp(a + off_a, b + off_b, 256) == 0) {
                    result->identical_sectors++;
                    result->identical_bytes += 256;
                } else {
                    /* Count differing bytes */
                    uint16_t diffs = 0;
                    for (int i = 0; i < 256; i++) {
                        if (a[off_a + i] != b[off_b + i])
                            diffs++;
                    }
                    result->different_sectors++;
                    result->different_bytes += diffs;
                    result->identical_bytes += (256 - diffs);
                    record_diff(result, (uint8_t)t, 0, s, off_a,
                                diffs, 256, false, false);
                }
                off_a += 256;
                off_b += 256;
            } else if (have_a && !have_b) {
                result->different_sectors++;
                result->only_in_a++;
                result->different_bytes += 256;
                record_diff(result, (uint8_t)t, 0, s, off_a,
                            256, 256, true, false);
                off_a += 256;
            } else if (!have_a && have_b) {
                result->different_sectors++;
                result->only_in_b++;
                result->different_bytes += 256;
                record_diff(result, (uint8_t)t, 0, s, off_b,
                            256, 256, false, true);
                off_b += 256;
            } else {
                /* Neither has it — shouldn't happen but be safe */
                break;
            }
        }
    }
}

/*===========================================================================
 * Internal: raw sector compare (fixed sector size)
 *===========================================================================*/

static void compare_raw(const uint8_t *a, size_t sa,
                         const uint8_t *b, size_t sb,
                         int sector_size,
                         uft_compare_result_t *result)
{
    size_t max_size = (sa > sb) ? sa : sb;
    size_t total_sectors = max_size / (size_t)sector_size;
    /* Include a partial trailing sector if present */
    if (max_size % (size_t)sector_size)
        total_sectors++;

    for (size_t i = 0; i < total_sectors; i++) {
        size_t off = i * (size_t)sector_size;
        uint16_t chunk = (uint16_t)sector_size;

        /* Clamp last sector to actual remaining data */
        if (off + chunk > max_size)
            chunk = (uint16_t)(max_size - off);

        bool have_a = (off < sa);
        bool have_b = (off < sb);
        uint16_t len_a = have_a ? (uint16_t)((off + chunk <= sa) ? chunk : (sa - off)) : 0;
        uint16_t len_b = have_b ? (uint16_t)((off + chunk <= sb) ? chunk : (sb - off)) : 0;

        /* Compute track/head/sector for uniform geometries */
        uint8_t track = 0, head = 0, sector_num = 0;
        if (sector_size == 512) {
            /* Assume CHS with 18 sectors/track, 2 heads (PC 1.44 MB) or
             * 11 sectors/track, 2 heads (Amiga DD).
             * For ADF: 11 spt, 2 heads, 80 tracks.
             * For generic IMG: 18 spt, 2 heads.
             * Use whichever matches the image size. */
            int spt = 18;
            int heads = 2;
            /* ADF: 901120 bytes = 80*2*11*512 */
            if (max_size == 901120 || max_size == 1802240) {
                spt = 11;
            }
            /* Atari ST: 9 or 10 spt, typically 1 or 2 heads */
            else if (max_size == 368640 || max_size == 737280) {
                spt = 9;
            }
            sector_num = (uint8_t)(i % (size_t)spt);
            head       = (uint8_t)((i / (size_t)spt) % (size_t)heads);
            track      = (uint8_t)((i / (size_t)spt) / (size_t)heads);
        } else {
            /* For 256-byte sectors (Atari 8-bit, etc.) use flat numbering */
            sector_num = (uint8_t)(i % 256);
            track      = (uint8_t)(i / 256);
        }

        result->total_sectors++;
        result->total_bytes += chunk;

        if (have_a && have_b) {
            uint16_t cmp_len = (len_a < len_b) ? len_a : len_b;
            uint16_t diffs = 0;

            for (uint16_t j = 0; j < cmp_len; j++) {
                if (a[off + j] != b[off + j])
                    diffs++;
            }
            /* Bytes beyond shorter image count as different */
            if (len_a != len_b) {
                uint16_t extra = (len_a > len_b) ? (uint16_t)(len_a - len_b)
                                                  : (uint16_t)(len_b - len_a);
                diffs += extra;
            }

            if (diffs == 0) {
                result->identical_sectors++;
                result->identical_bytes += chunk;
            } else {
                result->different_sectors++;
                result->different_bytes += diffs;
                result->identical_bytes += (chunk - diffs);
                record_diff(result, track, head, sector_num,
                            (uint32_t)off, diffs, chunk, false, false);
            }
        } else if (have_a) {
            result->different_sectors++;
            result->only_in_a++;
            result->different_bytes += len_a;
            record_diff(result, track, head, sector_num,
                        (uint32_t)off, len_a, chunk, true, false);
        } else {
            result->different_sectors++;
            result->only_in_b++;
            result->different_bytes += len_b;
            record_diff(result, track, head, sector_num,
                        (uint32_t)off, len_b, chunk, false, true);
        }
    }
}

/*===========================================================================
 * Internal: detect sector size from format detection result
 *===========================================================================*/

static int detect_sector_size(const uft_detect_result_t *det)
{
    if (det->sector_size > 0)
        return (int)det->sector_size;

    switch (det->format) {
        case UFT_DETECT_D64:
        case UFT_DETECT_D71:
        case UFT_DETECT_D80:
        case UFT_DETECT_D82:
            return 256;
        case UFT_DETECT_ADF:
        case UFT_DETECT_IMG:
        case UFT_DETECT_ST:
            return 512;
        case UFT_DETECT_D81:
            return 256;
        default:
            return 512; /* sensible default */
    }
}

/** Map detect enum to short format name string */
static const char *format_short_name(uft_detect_format_t fmt)
{
    switch (fmt) {
        case UFT_DETECT_D64:        return "D64";
        case UFT_DETECT_D71:        return "D71";
        case UFT_DETECT_D81:        return "D81";
        case UFT_DETECT_D80:        return "D80";
        case UFT_DETECT_D82:        return "D82";
        case UFT_DETECT_G64:        return "G64";
        case UFT_DETECT_G71:        return "G71";
        case UFT_DETECT_ADF:        return "ADF";
        case UFT_DETECT_IMG:        return "IMG";
        case UFT_DETECT_ST:         return "ST";
        case UFT_DETECT_MSA:        return "MSA";
        case UFT_DETECT_DSK_APPLE:  return "DSK";
        case UFT_DETECT_NIB:        return "NIB";
        case UFT_DETECT_A2R:        return "A2R";
        case UFT_DETECT_SCP:        return "SCP";
        case UFT_DETECT_HFE:        return "HFE";
        case UFT_DETECT_IPF:        return "IPF";
        case UFT_DETECT_KRYOFLUX:   return "KFX";
        case UFT_DETECT_TD0:        return "TD0";
        case UFT_DETECT_IMD:        return "IMD";
        case UFT_DETECT_FDI:        return "FDI";
        default:                    return "RAW";
    }
}

/*===========================================================================
 * Public API: compare two files
 *===========================================================================*/

int uft_sector_compare_files(const char *path_a, const char *path_b,
                              uft_compare_result_t *result)
{
    if (!path_a || !path_b || !result)
        return UFT_E_NULL_PTR;

    memset(result, 0, sizeof(*result));

    /* Open and read file A */
    FILE *fa = fopen(path_a, "rb");
    if (!fa) return UFT_E_FILE_NOT_FOUND;

    if (fseek(fa, 0, SEEK_END) != 0) { fclose(fa); return UFT_E_FILE_SEEK; }
    long len_a = ftell(fa);
    if (len_a < 0) { fclose(fa); return UFT_E_FILE_READ; }
    if (fseek(fa, 0, SEEK_SET) != 0) { fclose(fa); return UFT_E_FILE_SEEK; }

    uint8_t *buf_a = (uint8_t *)malloc((size_t)len_a);
    if (!buf_a) { fclose(fa); return UFT_E_GENERIC; }
    if (fread(buf_a, 1, (size_t)len_a, fa) != (size_t)len_a) {
        free(buf_a); fclose(fa); return UFT_E_FILE_READ;
    }
    fclose(fa);

    /* Open and read file B */
    FILE *fb = fopen(path_b, "rb");
    if (!fb) { free(buf_a); return UFT_E_FILE_NOT_FOUND; }

    if (fseek(fb, 0, SEEK_END) != 0) { free(buf_a); fclose(fb); return UFT_E_FILE_SEEK; }
    long len_b = ftell(fb);
    if (len_b < 0) { free(buf_a); fclose(fb); return UFT_E_FILE_READ; }
    if (fseek(fb, 0, SEEK_SET) != 0) { free(buf_a); fclose(fb); return UFT_E_FILE_SEEK; }

    uint8_t *buf_b = (uint8_t *)malloc((size_t)len_b);
    if (!buf_b) { free(buf_a); fclose(fb); return UFT_E_GENERIC; }
    if (fread(buf_b, 1, (size_t)len_b, fb) != (size_t)len_b) {
        free(buf_a); free(buf_b); fclose(fb); return UFT_E_FILE_READ;
    }
    fclose(fb);

    /* Detect formats */
    uft_detect_result_t det_a, det_b;
    memset(&det_a, 0, sizeof(det_a));
    memset(&det_b, 0, sizeof(det_b));

    bool detected_a = uft_detect_buffer(buf_a, (size_t)len_a, path_a, &det_a);
    bool detected_b = uft_detect_buffer(buf_b, (size_t)len_b, path_b, &det_b);

    /* Fill format info */
    result->size_a = (uint32_t)len_a;
    result->size_b = (uint32_t)len_b;

    if (detected_a) {
        snprintf(result->format_a, sizeof(result->format_a), "%s",
                 format_short_name(det_a.format));
    } else {
        snprintf(result->format_a, sizeof(result->format_a), "RAW");
    }
    if (detected_b) {
        snprintf(result->format_b, sizeof(result->format_b), "%s",
                 format_short_name(det_b.format));
    } else {
        snprintf(result->format_b, sizeof(result->format_b), "RAW");
    }

    /* Choose comparison strategy */
    if (detected_a && det_a.format == UFT_DETECT_D64 &&
        detected_b && det_b.format == UFT_DETECT_D64) {
        /* Format-aware D64 comparison */
        compare_d64(buf_a, (size_t)len_a, buf_b, (size_t)len_b, result);
    } else {
        /* Raw sector comparison */
        int sec_size = 512;
        if (detected_a)
            sec_size = detect_sector_size(&det_a);
        else if (detected_b)
            sec_size = detect_sector_size(&det_b);
        compare_raw(buf_a, (size_t)len_a, buf_b, (size_t)len_b,
                    sec_size, result);
    }

    /* Compute similarity */
    if (result->total_sectors > 0) {
        result->similarity_percent =
            (float)result->identical_sectors * 100.0f / (float)result->total_sectors;
    }

    free(buf_a);
    free(buf_b);
    return UFT_OK;
}

/*===========================================================================
 * Public API: compare two in-memory buffers
 *===========================================================================*/

int uft_sector_compare_buffers(const uint8_t *data_a, size_t size_a,
                                const uint8_t *data_b, size_t size_b,
                                int sector_size,
                                uft_compare_result_t *result)
{
    if (!data_a || !data_b || !result)
        return UFT_E_NULL_PTR;
    if (sector_size <= 0 || sector_size > 8192)
        return UFT_E_INVALID_ARG;

    memset(result, 0, sizeof(*result));
    result->size_a = (uint32_t)size_a;
    result->size_b = (uint32_t)size_b;
    snprintf(result->format_a, sizeof(result->format_a), "RAW");
    snprintf(result->format_b, sizeof(result->format_b), "RAW");

    compare_raw(data_a, size_a, data_b, size_b, sector_size, result);

    if (result->total_sectors > 0) {
        result->similarity_percent =
            (float)result->identical_sectors * 100.0f / (float)result->total_sectors;
    }

    return UFT_OK;
}

/*===========================================================================
 * Public API: export text report
 *===========================================================================*/

int uft_compare_export_text(const uft_compare_result_t *result,
                             const char *output_path)
{
    if (!result || !output_path)
        return UFT_E_NULL_PTR;

    FILE *f = fopen(output_path, "w");
    if (!f) return UFT_E_FILE_CREATE;

    fprintf(f, "DISK COMPARE REPORT\n");
    fprintf(f, "===================\n\n");

    fprintf(f, "Image A: %s (%u bytes, %s)\n",
            result->format_a, result->size_a, result->format_a);
    fprintf(f, "Image B: %s (%u bytes, %s)\n\n",
            result->format_b, result->size_b, result->format_b);

    fprintf(f, "Summary: %u/%u sectors identical (%.1f%%)\n",
            result->identical_sectors, result->total_sectors,
            (double)result->similarity_percent);
    fprintf(f, "         %u sectors different, %u only in A, %u only in B\n\n",
            result->different_sectors, result->only_in_a, result->only_in_b);

    fprintf(f, "Byte-level: %llu total, %llu identical, %llu different\n\n",
            (unsigned long long)result->total_bytes,
            (unsigned long long)result->identical_bytes,
            (unsigned long long)result->different_bytes);

    if (result->diff_count > 0) {
        fprintf(f, "Different Sectors:\n");
        for (uint32_t i = 0; i < result->diff_count; i++) {
            const uft_sector_diff_t *d = &result->diffs[i];
            if (d->only_in_a) {
                fprintf(f, "  Track %u, Head %u, Sector %u: only in image A "
                        "(offset 0x%08X)\n",
                        d->track, d->head, d->sector, d->offset);
            } else if (d->only_in_b) {
                fprintf(f, "  Track %u, Head %u, Sector %u: only in image B "
                        "(offset 0x%08X)\n",
                        d->track, d->head, d->sector, d->offset);
            } else {
                fprintf(f, "  Track %u, Head %u, Sector %u: %u bytes differ "
                        "(offset 0x%08X, sector size %u)\n",
                        d->track, d->head, d->sector,
                        d->diff_count, d->offset, d->sector_size);
            }
        }
        if (result->diff_count >= UFT_COMPARE_MAX_DIFFS) {
            fprintf(f, "  ... (truncated at %u entries)\n",
                    UFT_COMPARE_MAX_DIFFS);
        }
    }

    fclose(f);
    return UFT_OK;
}

/*===========================================================================
 * Public API: export JSON report
 *===========================================================================*/

/** Write a JSON-safe string (escapes backslashes and quotes) */
static void json_write_string(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\')
            fputc('\\', f);
        fputc(*s, f);
    }
    fputc('"', f);
}

int uft_compare_export_json(const uft_compare_result_t *result,
                             const char *output_path)
{
    if (!result || !output_path)
        return UFT_E_NULL_PTR;

    FILE *f = fopen(output_path, "w");
    if (!f) return UFT_E_FILE_CREATE;

    fprintf(f, "{\n");
    fprintf(f, "  \"image_a\": { \"format\": ");
    json_write_string(f, result->format_a);
    fprintf(f, ", \"size\": %u },\n", result->size_a);

    fprintf(f, "  \"image_b\": { \"format\": ");
    json_write_string(f, result->format_b);
    fprintf(f, ", \"size\": %u },\n", result->size_b);

    fprintf(f, "  \"summary\": {\n");
    fprintf(f, "    \"total_sectors\": %u,\n", result->total_sectors);
    fprintf(f, "    \"identical_sectors\": %u,\n", result->identical_sectors);
    fprintf(f, "    \"different_sectors\": %u,\n", result->different_sectors);
    fprintf(f, "    \"only_in_a\": %u,\n", result->only_in_a);
    fprintf(f, "    \"only_in_b\": %u,\n", result->only_in_b);
    fprintf(f, "    \"similarity_percent\": %.2f,\n",
            (double)result->similarity_percent);
    fprintf(f, "    \"total_bytes\": %llu,\n",
            (unsigned long long)result->total_bytes);
    fprintf(f, "    \"identical_bytes\": %llu,\n",
            (unsigned long long)result->identical_bytes);
    fprintf(f, "    \"different_bytes\": %llu\n",
            (unsigned long long)result->different_bytes);
    fprintf(f, "  },\n");

    fprintf(f, "  \"diffs\": [\n");
    for (uint32_t i = 0; i < result->diff_count; i++) {
        const uft_sector_diff_t *d = &result->diffs[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"track\": %u,\n", d->track);
        fprintf(f, "      \"head\": %u,\n", d->head);
        fprintf(f, "      \"sector\": %u,\n", d->sector);
        fprintf(f, "      \"offset\": %u,\n", d->offset);
        fprintf(f, "      \"diff_count\": %u,\n", d->diff_count);
        fprintf(f, "      \"sector_size\": %u,\n", d->sector_size);
        fprintf(f, "      \"only_in_a\": %s,\n", d->only_in_a ? "true" : "false");
        fprintf(f, "      \"only_in_b\": %s\n", d->only_in_b ? "true" : "false");
        fprintf(f, "    }%s\n", (i + 1 < result->diff_count) ? "," : "");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    return UFT_OK;
}

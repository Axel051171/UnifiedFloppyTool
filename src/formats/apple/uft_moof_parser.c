/*
 * SPDX-FileCopyrightText: 2024-2026 Axel Kramer
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * MOOF (Macintosh Original Object Format) Parser
 *
 * Applesauce format for Macintosh 3.5" GCR/MFM disk preservation.
 * Reference: https://applesaucefdc.com/moof-reference/
 */

#include "uft/formats/apple/uft_moof.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * CRC32 (standard Applesauce polynomial)
 *============================================================================*/

static uint32_t moof_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

/*============================================================================
 * Little-endian Readers
 *============================================================================*/

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/*============================================================================
 * Mac GCR Speed Zone Tables
 *============================================================================*/

/* Sectors per track in each speed zone (zone 0 = outermost = fastest) */
static const int gcr_sectors_per_zone[UFT_MOOF_SPEED_ZONES] = {
    12, 11, 10, 9, 8
};

/* Track ranges for each zone (single side) */
static const int gcr_zone_start[UFT_MOOF_SPEED_ZONES] = {
    0, 16, 32, 48, 64
};

int uft_moof_gcr_speed_zone(int track)
{
    if (track < 0 || track > 79) return -1;
    if (track < 16) return 0;
    if (track < 32) return 1;
    if (track < 48) return 2;
    if (track < 64) return 3;
    return 4;
}

int uft_moof_gcr_sectors_per_zone(int zone)
{
    if (zone < 0 || zone >= UFT_MOOF_SPEED_ZONES) return 0;
    return gcr_sectors_per_zone[zone];
}

int uft_moof_tracks_for_type(uft_moof_disk_type_t disk_type)
{
    switch (disk_type) {
    case UFT_MOOF_DISK_SSDD_GCR_400K:  return 80;   /* 80 tracks, 1 side */
    case UFT_MOOF_DISK_DSDD_GCR_800K:  return 160;  /* 80 tracks, 2 sides */
    case UFT_MOOF_DISK_DSHD_MFM_1440K: return 160;  /* 80 tracks, 2 sides */
    case UFT_MOOF_DISK_TWIGGY:         return 92;   /* 46 tracks, 2 sides */
    default: return 0;
    }
}

/*============================================================================
 * Chunk Parsing
 *============================================================================*/

static uft_moof_error_t parse_info_chunk(uft_moof_disk_t *disk,
                                          const uint8_t *data, uint32_t size)
{
    if (size < 60) return UFT_MOOF_ERR_TRUNCATED;

    memcpy(&disk->info, data, sizeof(uft_moof_info_t));

    /* Version check */
    if (disk->info.info_version < 1) return UFT_MOOF_ERR_VERSION;

    disk->disk_type = (uft_moof_disk_type_t)disk->info.disk_type;
    disk->write_protected = (disk->info.write_protected != 0);
    disk->synchronized = (disk->info.synchronized != 0);

    /* Copy creator string (null-terminate) */
    memcpy(disk->creator, disk->info.creator, 32);
    disk->creator[32] = '\0';

    /* Trim trailing spaces */
    for (int i = 31; i >= 0 && disk->creator[i] == ' '; i--) {
        disk->creator[i] = '\0';
    }

    return UFT_MOOF_OK;
}

static uft_moof_error_t parse_tmap_chunk(uft_moof_disk_t *disk,
                                          const uint8_t *data, uint32_t size)
{
    if (size < UFT_MOOF_TMAP_ENTRIES) return UFT_MOOF_ERR_TRUNCATED;

    memcpy(disk->tmap, data, UFT_MOOF_TMAP_ENTRIES);

    /* Count valid tracks */
    disk->track_count = 0;
    for (int i = 0; i < UFT_MOOF_TMAP_ENTRIES; i++) {
        if (disk->tmap[i] != UFT_MOOF_TMAP_UNUSED) {
            int idx = disk->tmap[i];
            if (idx >= disk->track_count) {
                disk->track_count = idx + 1;
            }
        }
    }

    return UFT_MOOF_OK;
}

static uft_moof_error_t parse_trks_chunk(uft_moof_disk_t *disk,
                                          const uint8_t *data, uint32_t size,
                                          const uint8_t *file_data,
                                          size_t file_size)
{
    /* TRKS chunk starts with track entries (8 bytes each) */
    int max_tracks = UFT_MOOF_MAX_TRACKS;
    if (disk->track_count > 0 && disk->track_count < max_tracks)
        max_tracks = disk->track_count;

    size_t entries_size = (size_t)max_tracks * sizeof(uft_moof_trk_entry_t);
    if (size < entries_size) return UFT_MOOF_ERR_TRUNCATED;

    const uft_moof_trk_entry_t *entries = (const uft_moof_trk_entry_t *)data;

    for (int t = 0; t < max_tracks; t++) {
        uint16_t start_block = read_le16((const uint8_t *)&entries[t].start_block);
        uint16_t block_count = read_le16((const uint8_t *)&entries[t].block_count);
        uint32_t bit_count   = read_le32((const uint8_t *)&entries[t].bit_count);

        if (block_count == 0 || bit_count == 0) {
            disk->tracks[t].valid = false;
            continue;
        }

        size_t byte_offset = (size_t)start_block * UFT_MOOF_BLOCK_SIZE;
        size_t byte_count  = (size_t)block_count * UFT_MOOF_BLOCK_SIZE;

        if (byte_offset + byte_count > file_size) {
            disk->tracks[t].valid = false;
            continue;
        }

        /* Allocate and copy bitstream data */
        disk->tracks[t].bitstream = (uint8_t *)malloc(byte_count);
        if (!disk->tracks[t].bitstream) return UFT_MOOF_ERR_MEMORY;

        memcpy(disk->tracks[t].bitstream, file_data + byte_offset, byte_count);
        disk->tracks[t].bitstream_size = byte_count;
        disk->tracks[t].bit_count = bit_count;
        disk->tracks[t].start_block = start_block;
        disk->tracks[t].block_count = block_count;
        disk->tracks[t].valid = true;
    }

    return UFT_MOOF_OK;
}

static uft_moof_error_t parse_flux_chunk(uft_moof_disk_t *disk,
                                          const uint8_t *data, uint32_t size,
                                          const uint8_t *file_data,
                                          size_t file_size)
{
    /* FLUX chunk has same structure as TRKS but with flux timing data */
    int max_tracks = UFT_MOOF_MAX_TRACKS;
    if (disk->track_count > 0 && disk->track_count < max_tracks)
        max_tracks = disk->track_count;

    size_t entries_size = (size_t)max_tracks * sizeof(uft_moof_trk_entry_t);
    if (size < entries_size) return UFT_MOOF_ERR_TRUNCATED;

    const uft_moof_trk_entry_t *entries = (const uft_moof_trk_entry_t *)data;

    for (int t = 0; t < max_tracks; t++) {
        uint16_t start_block = read_le16((const uint8_t *)&entries[t].start_block);
        uint16_t block_count = read_le16((const uint8_t *)&entries[t].block_count);
        uint32_t bit_count   = read_le32((const uint8_t *)&entries[t].bit_count);

        if (block_count == 0 || bit_count == 0) continue;

        size_t byte_offset = (size_t)start_block * UFT_MOOF_BLOCK_SIZE;
        size_t byte_count  = (size_t)block_count * UFT_MOOF_BLOCK_SIZE;

        if (byte_offset + byte_count > file_size) continue;

        /* Flux data is stored as 16-bit timing values (125ns units) */
        size_t flux_count = byte_count / 2;
        disk->flux[t].timing = (uint16_t *)malloc(flux_count * sizeof(uint16_t));
        if (!disk->flux[t].timing) return UFT_MOOF_ERR_MEMORY;

        const uint8_t *src = file_data + byte_offset;
        for (size_t i = 0; i < flux_count; i++) {
            disk->flux[t].timing[i] = read_le16(src + i * 2);
        }

        disk->flux[t].count = flux_count;
        disk->flux[t].valid = true;
    }

    disk->has_flux = true;
    return UFT_MOOF_OK;
}

static uft_moof_error_t parse_meta_chunk(uft_moof_disk_t *disk,
                                          const uint8_t *data, uint32_t size)
{
    /* META chunk is UTF-8 text: "key\tvalue\n" pairs */
    if (size == 0) return UFT_MOOF_OK;

    /* Count lines */
    int line_count = 0;
    for (uint32_t i = 0; i < size; i++) {
        if (data[i] == '\n') line_count++;
    }
    if (line_count == 0) return UFT_MOOF_OK;

    disk->metadata = (uft_moof_meta_t *)calloc((size_t)line_count, sizeof(uft_moof_meta_t));
    if (!disk->metadata) return UFT_MOOF_ERR_MEMORY;

    /* Parse key-value pairs */
    const char *p = (const char *)data;
    const char *end = p + size;
    int idx = 0;

    while (p < end && idx < line_count) {
        const char *line_end = memchr(p, '\n', (size_t)(end - p));
        if (!line_end) line_end = end;

        const char *tab = memchr(p, '\t', (size_t)(line_end - p));
        if (tab) {
            size_t key_len = (size_t)(tab - p);
            size_t val_len = (size_t)(line_end - tab - 1);

            disk->metadata[idx].key = (char *)malloc(key_len + 1);
            disk->metadata[idx].value = (char *)malloc(val_len + 1);

            if (disk->metadata[idx].key && disk->metadata[idx].value) {
                memcpy(disk->metadata[idx].key, p, key_len);
                disk->metadata[idx].key[key_len] = '\0';
                memcpy(disk->metadata[idx].value, tab + 1, val_len);
                disk->metadata[idx].value[val_len] = '\0';
                idx++;
            }
        }

        p = line_end + 1;
    }

    disk->metadata_count = idx;
    return UFT_MOOF_OK;
}

/*============================================================================
 * Public API
 *============================================================================*/

uft_moof_error_t uft_moof_open(const char *path, uft_moof_disk_t *disk)
{
    if (!path || !disk) return UFT_MOOF_ERR_NULL;

    memset(disk, 0, sizeof(uft_moof_disk_t));
    memset(disk->tmap, UFT_MOOF_TMAP_UNUSED, sizeof(disk->tmap));

    /* Store path */
    size_t path_len = strlen(path);
    if (path_len >= sizeof(disk->filepath)) path_len = sizeof(disk->filepath) - 1;
    memcpy(disk->filepath, path, path_len);
    disk->filepath[path_len] = '\0';

    /* Read entire file into memory */
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_MOOF_ERR_OPEN;

    fseek(f, 0, SEEK_END);
    long file_len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_len < UFT_MOOF_HEADER_SIZE + 8) {
        fclose(f);
        return UFT_MOOF_ERR_TRUNCATED;
    }

    uint8_t *file_data = (uint8_t *)malloc((size_t)file_len);
    if (!file_data) {
        fclose(f);
        return UFT_MOOF_ERR_MEMORY;
    }

    if (fread(file_data, 1, (size_t)file_len, f) != (size_t)file_len) {
        free(file_data);
        fclose(f);
        return UFT_MOOF_ERR_READ;
    }
    fclose(f);

    /* Verify magic */
    if (memcmp(file_data, UFT_MOOF_MAGIC_STR, 4) != 0 ||
        file_data[4] != UFT_MOOF_SUFFIX_0 ||
        file_data[5] != UFT_MOOF_SUFFIX_1 ||
        file_data[6] != UFT_MOOF_SUFFIX_2 ||
        file_data[7] != UFT_MOOF_SUFFIX_3) {
        free(file_data);
        return UFT_MOOF_ERR_MAGIC;
    }

    /* Verify CRC32 (covers everything after the 12-byte header) */
    disk->file_crc32 = read_le32(file_data + 8);
    uint32_t computed_crc = moof_crc32(file_data + UFT_MOOF_HEADER_SIZE,
                                        (size_t)file_len - UFT_MOOF_HEADER_SIZE);
    disk->crc_valid = (computed_crc == disk->file_crc32);

    if (!disk->crc_valid) {
        free(file_data);
        return UFT_MOOF_ERR_CRC;
    }

    /* Parse chunks */
    bool has_info = false, has_tmap = false, has_trks = false;
    size_t offset = UFT_MOOF_HEADER_SIZE;
    uft_moof_error_t err = UFT_MOOF_OK;

    while (offset + 8 <= (size_t)file_len) {
        uint32_t chunk_id   = read_le32(file_data + offset);
        uint32_t chunk_size = read_le32(file_data + offset + 4);
        offset += 8;

        if (offset + chunk_size > (size_t)file_len) break;

        const uint8_t *chunk_data = file_data + offset;

        switch (chunk_id) {
        case UFT_MOOF_CHUNK_INFO:
            err = parse_info_chunk(disk, chunk_data, chunk_size);
            has_info = true;
            break;
        case UFT_MOOF_CHUNK_TMAP:
            err = parse_tmap_chunk(disk, chunk_data, chunk_size);
            has_tmap = true;
            break;
        case UFT_MOOF_CHUNK_TRKS:
            err = parse_trks_chunk(disk, chunk_data, chunk_size,
                                    file_data, (size_t)file_len);
            has_trks = true;
            break;
        case UFT_MOOF_CHUNK_FLUX:
            err = parse_flux_chunk(disk, chunk_data, chunk_size,
                                    file_data, (size_t)file_len);
            break;
        case UFT_MOOF_CHUNK_META:
            err = parse_meta_chunk(disk, chunk_data, chunk_size);
            break;
        default:
            /* Unknown chunk — skip */
            break;
        }

        if (err != UFT_MOOF_OK) {
            free(file_data);
            uft_moof_close(disk);
            return err;
        }

        offset += chunk_size;
    }

    free(file_data);

    /* Require mandatory chunks */
    if (!has_info) return UFT_MOOF_ERR_NO_INFO;
    if (!has_tmap) return UFT_MOOF_ERR_NO_TMAP;
    if (!has_trks) return UFT_MOOF_ERR_NO_TRKS;

    return UFT_MOOF_OK;
}

void uft_moof_close(uft_moof_disk_t *disk)
{
    if (!disk) return;

    /* Free track bitstream data */
    for (int t = 0; t < UFT_MOOF_MAX_TRACKS; t++) {
        free(disk->tracks[t].bitstream);
        disk->tracks[t].bitstream = NULL;
    }

    /* Free flux data */
    for (int t = 0; t < UFT_MOOF_MAX_TRACKS; t++) {
        free(disk->flux[t].timing);
        disk->flux[t].timing = NULL;
    }

    /* Free metadata */
    if (disk->metadata) {
        for (int i = 0; i < disk->metadata_count; i++) {
            free(disk->metadata[i].key);
            free(disk->metadata[i].value);
        }
        free(disk->metadata);
        disk->metadata = NULL;
    }
}

const char *uft_moof_get_info_string(const uft_moof_disk_t *disk)
{
    if (!disk) return NULL;

    switch (disk->disk_type) {
    case UFT_MOOF_DISK_SSDD_GCR_400K:  return "Macintosh 400K GCR (single-sided)";
    case UFT_MOOF_DISK_DSDD_GCR_800K:  return "Macintosh 800K GCR (double-sided)";
    case UFT_MOOF_DISK_DSHD_MFM_1440K: return "Macintosh 1.44MB MFM (HD)";
    case UFT_MOOF_DISK_TWIGGY:         return "Lisa Twiggy";
    default:                            return "Unknown MOOF disk type";
    }
}

uft_moof_error_t uft_moof_get_track_bitstream(const uft_moof_disk_t *disk,
                                                int track,
                                                const uint8_t **data,
                                                uint32_t *bit_count)
{
    if (!disk || !data || !bit_count) return UFT_MOOF_ERR_NULL;
    if (track < 0 || track >= UFT_MOOF_MAX_TRACKS) return UFT_MOOF_ERR_TRACK;

    /* Resolve through TMAP */
    uint8_t trk_idx = disk->tmap[track];
    if (trk_idx == UFT_MOOF_TMAP_UNUSED) return UFT_MOOF_ERR_TRACK;

    if (trk_idx >= UFT_MOOF_MAX_TRACKS || !disk->tracks[trk_idx].valid)
        return UFT_MOOF_ERR_TRACK;

    *data = disk->tracks[trk_idx].bitstream;
    *bit_count = disk->tracks[trk_idx].bit_count;
    return UFT_MOOF_OK;
}

uft_moof_error_t uft_moof_get_flux(const uft_moof_disk_t *disk,
                                    int track,
                                    const uint16_t **timing,
                                    size_t *count)
{
    if (!disk || !timing || !count) return UFT_MOOF_ERR_NULL;
    if (!disk->has_flux) return UFT_MOOF_ERR_TRACK;
    if (track < 0 || track >= UFT_MOOF_MAX_TRACKS) return UFT_MOOF_ERR_TRACK;

    uint8_t trk_idx = disk->tmap[track];
    if (trk_idx == UFT_MOOF_TMAP_UNUSED) return UFT_MOOF_ERR_TRACK;

    if (trk_idx >= UFT_MOOF_MAX_TRACKS || !disk->flux[trk_idx].valid)
        return UFT_MOOF_ERR_TRACK;

    *timing = disk->flux[trk_idx].timing;
    *count = disk->flux[trk_idx].count;
    return UFT_MOOF_OK;
}

uft_moof_error_t uft_moof_validate(const char *path)
{
    uft_moof_disk_t disk;
    uft_moof_error_t err = uft_moof_open(path, &disk);
    uft_moof_close(&disk);
    return err;
}

const char *uft_moof_strerror(uft_moof_error_t err)
{
    switch (err) {
    case UFT_MOOF_OK:            return "OK";
    case UFT_MOOF_ERR_NULL:      return "NULL parameter";
    case UFT_MOOF_ERR_OPEN:      return "Cannot open file";
    case UFT_MOOF_ERR_READ:      return "Read error";
    case UFT_MOOF_ERR_MAGIC:     return "Not a MOOF file (bad magic)";
    case UFT_MOOF_ERR_CRC:       return "CRC32 mismatch";
    case UFT_MOOF_ERR_TRUNCATED: return "File truncated";
    case UFT_MOOF_ERR_NO_INFO:   return "Missing INFO chunk";
    case UFT_MOOF_ERR_NO_TMAP:   return "Missing TMAP chunk";
    case UFT_MOOF_ERR_NO_TRKS:   return "Missing TRKS chunk";
    case UFT_MOOF_ERR_MEMORY:    return "Memory allocation failed";
    case UFT_MOOF_ERR_VERSION:   return "Unsupported MOOF version";
    case UFT_MOOF_ERR_TRACK:     return "Invalid track";
    default:                     return "Unknown MOOF error";
    }
}

/**
 * @file uft_woz_writer.c
 * @brief WOZ 2.0 Writer Implementation
 * 
 * P2-007: WOZ Writer
 */

#include "uft/uft_woz_writer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * 6-and-2 GCR Tables for Apple II
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uint8_t gcr_encode_6and2[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* DOS 3.3 sector interleave */
static const int dos_interleave[16] = {
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};

/* ProDOS sector interleave */
static const int prodos_interleave[16] = {
    0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC32 for WOZ
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint32_t crc32_table[256];
static bool crc32_initialized = false;

static void init_crc32_table(void) {
    if (crc32_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
        crc32_table[i] = crc;
    }
    crc32_initialized = true;
}

uint32_t woz_crc32(const uint8_t *data, size_t size) {
    init_crc32_table();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Writer Context
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct woz_writer {
    woz_writer_config_t config;
    
    /* Track map (160 entries) */
    uint8_t tmap[WOZ_TMAP_SIZE];
    
    /* Track data */
    woz_trk_entry_t trk_entries[WOZ_MAX_TRACKS_525];
    uint8_t *track_data[WOZ_MAX_TRACKS_525];
    size_t track_sizes[WOZ_MAX_TRACKS_525];
    int track_count;
    
    /* Flux data (optional) */
    uint32_t *flux_data[WOZ_MAX_TRACKS_525];
    size_t flux_counts[WOZ_MAX_TRACKS_525];
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

woz_writer_t* woz_writer_create(const woz_writer_config_t *config) {
    woz_writer_t *writer = calloc(1, sizeof(woz_writer_t));
    if (!writer) return NULL;
    
    if (config) {
        writer->config = *config;
    } else {
        woz_writer_config_t defaults = WOZ_WRITER_CONFIG_DEFAULT;
        writer->config = defaults;
    }
    
    /* Initialize TMAP to 0xFF (no track) */
    memset(writer->tmap, 0xFF, WOZ_TMAP_SIZE);
    
    return writer;
}

void woz_writer_destroy(woz_writer_t *writer) {
    if (!writer) return;
    
    for (int i = 0; i < WOZ_MAX_TRACKS_525; i++) {
        free(writer->track_data[i]);
        free(writer->flux_data[i]);
    }
    free(writer);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

int woz_writer_add_track(woz_writer_t *writer, const woz_track_data_t *track) {
    if (!writer || !track) return -1;
    
    int tmap_index = track->track_number * 4 + track->quarter_track;
    if (tmap_index >= WOZ_TMAP_SIZE) return -1;
    
    int trk_index = writer->track_count;
    if (trk_index >= WOZ_MAX_TRACKS_525) return -1;
    
    /* Copy bit data */
    size_t byte_count = (track->bit_count + 7) / 8;
    size_t block_count = (byte_count + 511) / 512;
    
    writer->track_data[trk_index] = malloc(block_count * 512);
    if (!writer->track_data[trk_index]) return -1;
    
    memset(writer->track_data[trk_index], 0, block_count * 512);
    memcpy(writer->track_data[trk_index], track->bit_data, byte_count);
    writer->track_sizes[trk_index] = block_count * 512;
    
    /* Set up TRK entry */
    writer->trk_entries[trk_index].block_count = block_count;
    writer->trk_entries[trk_index].bit_count = track->bit_count;
    
    /* Copy flux data if present */
    if (track->flux_data && track->flux_count > 0) {
        writer->flux_data[trk_index] = malloc(track->flux_count * sizeof(uint32_t));
        if (writer->flux_data[trk_index]) {
            memcpy(writer->flux_data[trk_index], track->flux_data, 
                   track->flux_count * sizeof(uint32_t));
            writer->flux_counts[trk_index] = track->flux_count;
        }
    }
    
    /* Update TMAP */
    writer->tmap[tmap_index] = trk_index;
    writer->track_count++;
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Writing
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void write_u16_le(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static void write_u32_le(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

int woz_writer_write(woz_writer_t *writer, const char *path) {
    if (!writer || !path) return -1;
    
    uint8_t *buffer = NULL;
    size_t size = 0;
    
    int ret = woz_writer_write_buffer(writer, NULL, &size);
    if (ret != 0) return ret;
    
    buffer = malloc(size);
    if (!buffer) return -1;
    
    ret = woz_writer_write_buffer(writer, buffer, &size);
    if (ret != 0) {
        free(buffer);
        return ret;
    }
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(buffer);
        return -1;
    }
    
    size_t written = fwrite(buffer, 1, size, f);
    fclose(f);
    free(buffer);
    
    return (written == size) ? 0 : -1;
}

int woz_writer_write_buffer(woz_writer_t *writer, uint8_t *buffer, size_t *size) {
    if (!writer || !size) return -1;
    
    /* Calculate sizes */
    size_t info_size = 60;  /* INFO chunk data size */
    size_t tmap_size = WOZ_TMAP_SIZE;
    size_t trks_header_size = WOZ_MAX_TRACKS_525 * 8;  /* TRK entries */
    
    size_t trks_data_size = 0;
    for (int i = 0; i < writer->track_count; i++) {
        trks_data_size += writer->track_sizes[i];
    }
    
    size_t total_size = 12;  /* Header */
    total_size += 8 + info_size;  /* INFO chunk */
    total_size += 8 + tmap_size;  /* TMAP chunk */
    total_size += 8 + trks_header_size + trks_data_size;  /* TRKS chunk */
    
    *size = total_size;
    
    if (!buffer) return 0;  /* Size query only */
    
    uint8_t *p = buffer;
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * Header
     * ═══════════════════════════════════════════════════════════════════════════ */
    
    memcpy(p, "WOZ2", 4);
    p += 4;
    p[0] = 0xFF;  /* High bit set */
    p[1] = 0x0A;  /* LF */
    p[2] = 0x0D;  /* CR */
    p[3] = 0x0A;  /* LF */
    p += 4;
    /* CRC32 placeholder (filled at end) */
    uint8_t *crc_pos = p;
    p += 4;
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * INFO Chunk
     * ═══════════════════════════════════════════════════════════════════════════ */
    
    write_u32_le(p, WOZ_CHUNK_INFO);
    p += 4;
    write_u32_le(p, info_size);
    p += 4;
    
    p[0] = WOZ_VERSION;
    p[1] = writer->config.disk_type;
    p[2] = writer->config.write_protected ? 1 : 0;
    p[3] = writer->config.synchronized ? 1 : 0;
    p[4] = 0;  /* cleaned */
    p += 5;
    
    memset(p, ' ', 32);
    size_t creator_len = strlen(writer->config.creator);
    if (creator_len > 32) creator_len = 32;
    memcpy(p, writer->config.creator, creator_len);
    p += 32;
    
    p[0] = writer->config.disk_sides;
    p[1] = writer->config.boot_format;
    p[2] = writer->config.bit_timing;
    p[3] = 0;  /* compatible_hardware high */
    p[4] = 0;  /* compatible_hardware low */
    p += 5;
    
    write_u16_le(p, 0);  /* required_ram */
    p += 2;
    
    /* Find largest track */
    uint16_t largest = 0;
    for (int i = 0; i < writer->track_count; i++) {
        uint16_t blocks = writer->trk_entries[i].block_count;
        if (blocks > largest) largest = blocks;
    }
    write_u16_le(p, largest);
    p += 2;
    
    write_u16_le(p, 0);  /* flux_block */
    p += 2;
    write_u16_le(p, 0);  /* largest_flux_track */
    p += 2;
    
    /* Padding to 60 bytes */
    memset(p, 0, 60 - (p - (crc_pos + 4 + 8)));
    p = crc_pos + 4 + 8 + info_size;
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * TMAP Chunk
     * ═══════════════════════════════════════════════════════════════════════════ */
    
    write_u32_le(p, WOZ_CHUNK_TMAP);
    p += 4;
    write_u32_le(p, tmap_size);
    p += 4;
    memcpy(p, writer->tmap, WOZ_TMAP_SIZE);
    p += WOZ_TMAP_SIZE;
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * TRKS Chunk
     * ═══════════════════════════════════════════════════════════════════════════ */
    
    write_u32_le(p, WOZ_CHUNK_TRKS);
    p += 4;
    write_u32_le(p, trks_header_size + trks_data_size);
    p += 4;
    
    /* Calculate block offsets */
    uint16_t current_block = 3;  /* After header (1) + INFO (1) + TMAP (<1) */
    current_block = (12 + 8 + info_size + 8 + tmap_size + 8 + trks_header_size + 511) / 512;
    
    /* Write TRK entries */
    for (int i = 0; i < WOZ_MAX_TRACKS_525; i++) {
        if (i < writer->track_count && writer->track_data[i]) {
            write_u16_le(p, current_block);
            p += 2;
            write_u16_le(p, writer->trk_entries[i].block_count);
            p += 2;
            write_u32_le(p, writer->trk_entries[i].bit_count);
            p += 4;
            current_block += writer->trk_entries[i].block_count;
        } else {
            memset(p, 0, 8);
            p += 8;
        }
    }
    
    /* Write track data */
    for (int i = 0; i < writer->track_count; i++) {
        if (writer->track_data[i]) {
            memcpy(p, writer->track_data[i], writer->track_sizes[i]);
            p += writer->track_sizes[i];
        }
    }
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * CRC32
     * ═══════════════════════════════════════════════════════════════════════════ */
    
    uint32_t crc = woz_crc32(buffer + 12, total_size - 12);
    write_u32_le(crc_pos, crc);
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * GCR Encoding
 * ═══════════════════════════════════════════════════════════════════════════════ */

void woz_gcr_encode_6and2(const uint8_t *data, uint8_t *gcr) {
    /* Apple II 6-and-2 encoding */
    /* 256 bytes -> 342 nibbles -> 342 GCR bytes */
    
    /* First, create auxiliary buffer and primary buffer */
    uint8_t aux[86];
    uint8_t prim[256];
    
    /* Extract 2-bit values for auxiliary buffer */
    for (int i = 0; i < 86; i++) {
        int idx = i;
        uint8_t val = 0;
        
        if (idx < 86) val |= (data[idx] & 0x01) << 1;
        if (idx < 86) val |= (data[idx] & 0x02) >> 1;
        if (idx + 86 < 256) {
            val |= (data[idx + 86] & 0x01) << 3;
            val |= (data[idx + 86] & 0x02) << 1;
        }
        if (idx + 172 < 256) {
            val |= (data[idx + 172] & 0x01) << 5;
            val |= (data[idx + 172] & 0x02) << 3;
        }
        aux[85 - i] = val;
    }
    
    /* Primary buffer (6 high bits) */
    for (int i = 0; i < 256; i++) {
        prim[i] = data[i] >> 2;
    }
    
    /* Encode with XOR checksumming */
    uint8_t prev = 0;
    int out_idx = 0;
    
    for (int i = 0; i < 86; i++) {
        gcr[out_idx++] = gcr_encode_6and2[aux[i] ^ prev];
        prev = aux[i];
    }
    
    for (int i = 0; i < 256; i++) {
        gcr[out_idx++] = gcr_encode_6and2[prim[i] ^ prev];
        prev = prim[i];
    }
    
    /* Checksum byte */
    gcr[out_idx++] = gcr_encode_6and2[prev];
}

int woz_write_address_field(uint8_t *output, int volume, int track, int sector) {
    uint8_t *p = output;
    
    /* Self-sync bytes */
    for (int i = 0; i < 21; i++) *p++ = 0xFF;
    
    /* Address prologue D5 AA 96 */
    *p++ = 0xD5;
    *p++ = 0xAA;
    *p++ = 0x96;
    
    /* 4-and-4 encoded volume, track, sector, checksum */
    *p++ = (volume >> 1) | 0xAA;
    *p++ = volume | 0xAA;
    *p++ = (track >> 1) | 0xAA;
    *p++ = track | 0xAA;
    *p++ = (sector >> 1) | 0xAA;
    *p++ = sector | 0xAA;
    
    uint8_t checksum = volume ^ track ^ sector;
    *p++ = (checksum >> 1) | 0xAA;
    *p++ = checksum | 0xAA;
    
    /* Address epilogue DE AA EB */
    *p++ = 0xDE;
    *p++ = 0xAA;
    *p++ = 0xEB;
    
    return p - output;
}

int woz_write_data_field(uint8_t *output, const uint8_t *sector_data) {
    uint8_t *p = output;
    
    /* Gap */
    for (int i = 0; i < 6; i++) *p++ = 0xFF;
    
    /* Data prologue D5 AA AD */
    *p++ = 0xD5;
    *p++ = 0xAA;
    *p++ = 0xAD;
    
    /* 6-and-2 encoded data (343 bytes) */
    woz_gcr_encode_6and2(sector_data, p);
    p += 343;
    
    /* Data epilogue DE AA EB */
    *p++ = 0xDE;
    *p++ = 0xAA;
    *p++ = 0xEB;
    
    return p - output;
}

int woz_from_dsk_track(
    const uint8_t *sector_data,
    int track_number,
    bool dos_order,
    uint8_t *bit_data,
    size_t *bit_count)
{
    if (!sector_data || !bit_data || !bit_count) return -1;
    
    uint8_t *p = bit_data;
    const int *interleave = dos_order ? dos_interleave : prodos_interleave;
    
    /* Write 16 sectors */
    for (int s = 0; s < 16; s++) {
        int physical_sector = interleave[s];
        
        /* Address field */
        p += woz_write_address_field(p, 254, track_number, s);
        
        /* Data field */
        p += woz_write_data_field(p, sector_data + physical_sector * 256);
    }
    
    /* Fill rest of track with sync */
    size_t track_bytes = 6656;  /* Standard track size */
    while ((size_t)(p - bit_data) < track_bytes) {
        *p++ = 0xFF;
    }
    
    *bit_count = (p - bit_data) * 8;
    return 0;
}

int woz_from_nib_track(
    const uint8_t *nib_data,
    size_t nib_size,
    uint8_t *bit_data,
    size_t *bit_count)
{
    if (!nib_data || !bit_data || !bit_count) return -1;
    
    /* NIB is already in disk byte format, just copy */
    memcpy(bit_data, nib_data, nib_size);
    *bit_count = nib_size * 8;
    
    return 0;
}

/**
 * @file uft_d64_writer.c
 * @brief D64 Writer Implementation with Gap Timing
 * 
 * P2-006: Writer Gap-Timing D64
 */

#include "uft/uft_d64_writer.h"
#include "uft/uft_cbm_gcr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Layout Tables
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Sectors per track (1-indexed) */
static const int sectors_per_track[41] = {
    0,  /* Track 0 doesn't exist */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* Tracks 1-10 */
    21, 21, 21, 21, 21, 21, 21,              /* Tracks 11-17 */
    19, 19, 19, 19, 19, 19, 19,              /* Tracks 18-24 */
    18, 18, 18, 18, 18, 18,                  /* Tracks 25-30 */
    17, 17, 17, 17, 17,                      /* Tracks 31-35 */
    17, 17, 17, 17, 17                       /* Tracks 36-40 (extended) */
};

/* Track start offsets in D64 file */
static const int track_offsets[41] = {
    0,
    0x00000, 0x01500, 0x02A00, 0x03F00, 0x05400,     /* 1-5 */
    0x06900, 0x07E00, 0x09300, 0x0A800, 0x0BD00,     /* 6-10 */
    0x0D200, 0x0E700, 0x0FC00, 0x11100, 0x12600,     /* 11-15 */
    0x13B00, 0x15000, 0x16500, 0x17800, 0x18B00,     /* 16-20 */
    0x19E00, 0x1B100, 0x1C400, 0x1D700, 0x1EA00,     /* 21-25 */
    0x1FC00, 0x20E00, 0x22000, 0x23200, 0x24400,     /* 26-30 */
    0x25600, 0x26700, 0x27800, 0x28900, 0x29A00,     /* 31-35 */
    0x2AB00, 0x2BC00, 0x2CD00, 0x2DE00, 0x2EF00      /* 36-40 */
};

/* GCR track lengths in bytes */
static const int track_gcr_length[4] = {
    7692,   /* Zone 0: 21 sectors */
    7142,   /* Zone 1: 19 sectors */
    6666,   /* Zone 2: 18 sectors */
    6250    /* Zone 3: 17 sectors */
};

/* Standard interleave table */
static const int standard_interleave[21] = {
    0, 10, 20, 9, 19, 8, 18, 7, 17, 6, 16, 5, 15, 4, 14, 3, 13, 2, 12, 1, 11
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Writer Context
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct d64_writer {
    d64_writer_config_t config;
    uint8_t *track_buffer;       /* Working buffer for track */
    size_t track_buffer_size;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

int d64_sectors_per_track(int track)
{
    if (track < 1 || track > 40) return 0;
    return sectors_per_track[track];
}

d64_speed_zone_t d64_track_zone(int track)
{
    if (track <= 17) return D64_ZONE_0;
    if (track <= 24) return D64_ZONE_1;
    if (track <= 30) return D64_ZONE_2;
    return D64_ZONE_3;
}

double d64_zone_bit_time(d64_speed_zone_t zone)
{
    switch (zone) {
        case D64_ZONE_0: return D64_ZONE0_BIT_TIME_US;
        case D64_ZONE_1: return D64_ZONE1_BIT_TIME_US;
        case D64_ZONE_2: return D64_ZONE2_BIT_TIME_US;
        case D64_ZONE_3: return D64_ZONE3_BIT_TIME_US;
        default: return D64_ZONE3_BIT_TIME_US;
    }
}

int d64_track_length_bits(int track)
{
    /* 200ms per revolution @ 300 RPM */
    double bit_time = d64_zone_bit_time(d64_track_zone(track));
    return (int)(200000.0 / bit_time);
}

int d64_track_length_gcr(int track)
{
    return track_gcr_length[d64_track_zone(track)];
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * GCR Encoding
 * ═══════════════════════════════════════════════════════════════════════════════ */

void d64_gcr_encode_4to5(const uint8_t *data, uint8_t *gcr)
{
    /* Encode 4 bytes to 5 GCR bytes */
    uint8_t n0 = cbm_gcr_encode_nibble(data[0] >> 4);
    uint8_t n1 = cbm_gcr_encode_nibble(data[0] & 0x0F);
    uint8_t n2 = cbm_gcr_encode_nibble(data[1] >> 4);
    uint8_t n3 = cbm_gcr_encode_nibble(data[1] & 0x0F);
    uint8_t n4 = cbm_gcr_encode_nibble(data[2] >> 4);
    uint8_t n5 = cbm_gcr_encode_nibble(data[2] & 0x0F);
    uint8_t n6 = cbm_gcr_encode_nibble(data[3] >> 4);
    uint8_t n7 = cbm_gcr_encode_nibble(data[3] & 0x0F);
    
    /* Pack 8 x 5-bit values into 5 bytes */
    gcr[0] = (n0 << 3) | (n1 >> 2);
    gcr[1] = (n1 << 6) | (n2 << 1) | (n3 >> 4);
    gcr[2] = (n3 << 4) | (n4 >> 1);
    gcr[3] = (n4 << 7) | (n5 << 2) | (n6 >> 3);
    gcr[4] = (n6 << 5) | n7;
}

int d64_gcr_decode_5to4(const uint8_t *gcr, uint8_t *data)
{
    /* Unpack 5 bytes to 8 x 5-bit values */
    uint8_t n0 = gcr[0] >> 3;
    uint8_t n1 = ((gcr[0] & 0x07) << 2) | (gcr[1] >> 6);
    uint8_t n2 = (gcr[1] >> 1) & 0x1F;
    uint8_t n3 = ((gcr[1] & 0x01) << 4) | (gcr[2] >> 4);
    uint8_t n4 = ((gcr[2] & 0x0F) << 1) | (gcr[3] >> 7);
    uint8_t n5 = (gcr[3] >> 2) & 0x1F;
    uint8_t n6 = ((gcr[3] & 0x03) << 3) | (gcr[4] >> 5);
    uint8_t n7 = gcr[4] & 0x1F;
    
    /* Decode nibbles */
    bool error = false;
    uint8_t d0 = cbm_gcr_decode_quintet(n0, &error);
    uint8_t d1 = cbm_gcr_decode_quintet(n1, &error);
    uint8_t d2 = cbm_gcr_decode_quintet(n2, &error);
    uint8_t d3 = cbm_gcr_decode_quintet(n3, &error);
    uint8_t d4 = cbm_gcr_decode_quintet(n4, &error);
    uint8_t d5 = cbm_gcr_decode_quintet(n5, &error);
    uint8_t d6 = cbm_gcr_decode_quintet(n6, &error);
    uint8_t d7 = cbm_gcr_decode_quintet(n7, &error);
    
    if (error) return -1;
    
    data[0] = (d0 << 4) | d1;
    data[1] = (d2 << 4) | d3;
    data[2] = (d4 << 4) | d5;
    data[3] = (d6 << 4) | d7;
    
    return 0;
}

uint8_t d64_header_checksum(int track, int sector, uint8_t id1, uint8_t id2)
{
    return (uint8_t)(track ^ sector ^ id1 ^ id2);
}

uint8_t d64_data_checksum(const uint8_t *data, int size)
{
    uint8_t checksum = 0;
    for (int i = 0; i < size; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

void d64_encode_header(const d64_header_t *header, uint8_t *gcr)
{
    uint8_t raw[8];
    raw[0] = header->block_id;
    raw[1] = header->checksum;
    raw[2] = header->sector;
    raw[3] = header->track;
    raw[4] = header->id2;
    raw[5] = header->id1;
    raw[6] = 0x0F;
    raw[7] = 0x0F;
    
    d64_gcr_encode_4to5(raw, gcr);
    d64_gcr_encode_4to5(raw + 4, gcr + 5);
}

void d64_encode_data_block(const d64_data_block_t *block, uint8_t *gcr)
{
    /* Data block: 1 (id) + 256 (data) + 1 (checksum) + 2 (padding) = 260 bytes
     * GCR encoded: 260 * 5 / 4 = 325 bytes */
    uint8_t raw[260];
    raw[0] = block->block_id;
    memcpy(raw + 1, block->data, 256);
    raw[257] = block->checksum;
    raw[258] = 0x00;
    raw[259] = 0x00;
    
    /* Encode in 4-byte chunks */
    for (int i = 0; i < 65; i++) {
        d64_gcr_encode_4to5(raw + i * 4, gcr + i * 5);
    }
}

void d64_write_sync(uint8_t *output, int count)
{
    memset(output, D64_SYNC_BYTE, count);
}

void d64_write_gap(uint8_t *output, int count)
{
    memset(output, 0x55, count);  /* Gap fill pattern */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Writer Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

d64_writer_t* d64_writer_create(const d64_writer_config_t *config)
{
    d64_writer_t *writer = calloc(1, sizeof(d64_writer_t));
    if (!writer) return NULL;
    
    if (config) {
        writer->config = *config;
    } else {
        d64_writer_config_t defaults = D64_WRITER_CONFIG_DEFAULT;
        writer->config = defaults;
    }
    
    /* Allocate track buffer (max track size) */
    writer->track_buffer_size = 8192;
    writer->track_buffer = malloc(writer->track_buffer_size);
    if (!writer->track_buffer) {
        free(writer);
        return NULL;
    }
    
    return writer;
}

void d64_writer_destroy(d64_writer_t *writer)
{
    if (writer) {
        free(writer->track_buffer);
        free(writer);
    }
}

int d64_write_track_gcr(
    d64_writer_t *writer,
    int track,
    const uint8_t *sector_data,
    int sector_count,
    uint8_t *gcr_output,
    size_t *gcr_size,
    d64_track_result_t *result)
{
    if (!writer || !sector_data || !gcr_output || !gcr_size)
        return -1;
    
    int expected_sectors = d64_sectors_per_track(track);
    if (sector_count != expected_sectors) {
        if (result) {
            result->errors = 1;
            snprintf(result->error_msg, sizeof(result->error_msg),
                    "Wrong sector count: %d (expected %d)", sector_count, expected_sectors);
        }
        return -1;
    }
    
    /* Calculate gap sizes */
    int gap1 = (writer->config.gap1_length >= 0) ? 
               writer->config.gap1_length : D64_GAP1_LENGTH;
    int gap2 = (writer->config.gap2_length >= 0) ? 
               writer->config.gap2_length : D64_GAP2_LENGTH;
    int sync_len = writer->config.sync_length;
    
    uint8_t *ptr = gcr_output;
    
    /* Write each sector with proper interleave */
    for (int i = 0; i < sector_count; i++) {
        int sector;
        if (writer->config.interleave == D64_INTERLEAVE_CUSTOM &&
            writer->config.custom_interleave) {
            sector = writer->config.custom_interleave[i % writer->config.custom_interleave_len];
        } else {
            sector = standard_interleave[i % 21];
            if (sector >= sector_count) sector = i;
        }
        
        /* Sync before header */
        d64_write_sync(ptr, sync_len);
        ptr += sync_len;
        
        /* Header */
        d64_header_t header = {0};
        header.block_id = D64_HEADER_MARK;
        header.track = track;
        header.sector = sector;
        header.id1 = writer->config.disk_id[0];
        header.id2 = writer->config.disk_id[1];
        header.checksum = d64_header_checksum(track, sector, header.id1, header.id2);
        header.padding[0] = 0x0F;
        header.padding[1] = 0x0F;
        
        d64_encode_header(&header, ptr);
        ptr += 10;  /* Header is 10 GCR bytes */
        
        /* Gap 1 */
        d64_write_gap(ptr, gap1);
        ptr += gap1;
        
        /* Sync before data */
        d64_write_sync(ptr, sync_len);
        ptr += sync_len;
        
        /* Data block */
        d64_data_block_t data_block = {0};
        data_block.block_id = D64_DATA_MARK;
        memcpy(data_block.data, sector_data + sector * 256, 256);
        data_block.checksum = d64_data_checksum(data_block.data, 256);
        
        d64_encode_data_block(&data_block, ptr);
        ptr += 325;  /* Data is 325 GCR bytes */
        
        /* Gap 2 */
        d64_write_gap(ptr, gap2);
        ptr += gap2;
    }
    
    *gcr_size = ptr - gcr_output;
    
    /* Fill result */
    if (result) {
        result->track = track;
        result->sectors_written = sector_count;
        result->gcr_bytes = *gcr_size;
        result->track_time_ms = (*gcr_size * 8) * d64_zone_bit_time(d64_track_zone(track)) / 1000.0;
        result->errors = 0;
        result->error_msg[0] = '\0';
    }
    
    return 0;
}

int d64_writer_write(
    d64_writer_t *writer,
    const uint8_t *sectors,
    int sector_count,
    uint8_t *output,
    size_t *output_size)
{
    if (!writer || !sectors || !output || !output_size)
        return -1;
    
    int track_count = writer->config.track_count;
    size_t total_size = 0;
    int sector_offset = 0;
    
    for (int track = 1; track <= track_count; track++) {
        int track_sectors = d64_sectors_per_track(track);
        
        if (sector_offset + track_sectors > sector_count) {
            break;  /* Not enough sectors */
        }
        
        size_t gcr_size = 0;
        d64_track_result_t result = {0};
        
        int ret = d64_write_track_gcr(
            writer,
            track,
            sectors + sector_offset * 256,
            track_sectors,
            output + total_size,
            &gcr_size,
            &result);
        
        if (ret != 0) {
            return ret;
        }
        
        total_size += gcr_size;
        sector_offset += track_sectors;
    }
    
    *output_size = total_size;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Flux Conversion
 * ═══════════════════════════════════════════════════════════════════════════════ */

int d64_gcr_to_flux(
    const uint8_t *gcr_data,
    size_t gcr_size,
    d64_speed_zone_t zone,
    uint32_t *flux_output,
    size_t *flux_count)
{
    if (!gcr_data || !flux_output || !flux_count)
        return -1;
    
    double bit_time = d64_zone_bit_time(zone);
    /* Convert to SCP ticks (25ns resolution) */
    double tick_per_bit = bit_time * 1000.0 / 25.0;
    
    size_t flux_idx = 0;
    double accumulator = 0;
    
    for (size_t i = 0; i < gcr_size; i++) {
        uint8_t byte = gcr_data[i];
        
        for (int bit = 7; bit >= 0; bit--) {
            accumulator += tick_per_bit;
            
            if (byte & (1 << bit)) {
                /* Flux transition */
                flux_output[flux_idx++] = (uint32_t)accumulator;
                accumulator = 0;
            }
        }
    }
    
    *flux_count = flux_idx;
    return 0;
}

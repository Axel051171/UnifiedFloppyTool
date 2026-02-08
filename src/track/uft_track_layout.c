/**
 * @file uft_track_layout.c
 * @brief Track Layout Generation Implementation
 * 
 * 
 * @copyright UFT Project 2026
 */

#include "uft/track/uft_track_layout.h"
#include "uft/gcr/uft_gcr_tables.h"
#include <string.h>

/*============================================================================
 * Sector Maps
 *============================================================================*/

const uint16_t uft_sector_map_b[4] = { 18, 19, 21, 22 };
const uint16_t uft_sector_map_c[4] = { 19, 20, 21, 23 };

/*============================================================================
 * Default Parameters
 *============================================================================*/

uft_track_params_t uft_track_params_default(void)
{
    uft_track_params_t p = {0};
    p.pattern = UFT_PATTERN_FIXED;
    p.no_tracks = 84;
    p.track_size = 7928;
    p.track_first = 1;
    p.track_last = 41;
    p.sector_first = 0;
    p.sector_last = 20;
    p.speed = 3;
    p.begin_at = 0;
    p.sync_len = 40;
    p.raw_stream_bytes = 0;
    return p;
}

uft_track_params_t uft_track_params_zoned(void)
{
    uft_track_params_t p = {0};
    p.pattern = UFT_PATTERN_ZONED;
    p.no_tracks = 84;
    p.track_size = 7928;
    p.track_first = 1;
    p.track_last = 35;
    p.sector_first = 0;
    p.sector_last = 0;
    p.speed = 0;
    p.begin_at = 0;
    p.sync_len = 16;
    p.raw_stream_bytes = 0;
    return p;
}

uft_track_params_t uft_track_params_zoned_data_only(void)
{
    uft_track_params_t p = {0};
    p.pattern = UFT_PATTERN_ZONED_DATA_ONLY;
    p.no_tracks = 84;
    p.track_size = 7928;
    p.track_first = 1;
    p.track_last = 35;
    p.sector_first = 0;
    p.sector_last = 0;
    p.speed = 0;
    p.begin_at = 0;
    p.sync_len = 16;
    p.raw_stream_bytes = 0;
    return p;
}

uft_track_params_t uft_track_params_raw_stream(void)
{
    uft_track_params_t p = {0};
    p.pattern = UFT_PATTERN_RAW_STREAM;
    p.no_tracks = 84;
    p.track_size = 7928;
    p.track_first = 1;
    p.track_last = 41;
    p.sector_first = 0;
    p.sector_last = 0;
    p.speed = 3;
    p.begin_at = 0;
    p.sync_len = 16;
    p.raw_stream_bytes = 24U * 256U;
    return p;
}

/*============================================================================
 * Zone Helper Functions
 *============================================================================*/

uint8_t uft_track_speed_zone(uint16_t track)
{
    if (track < 18) return 3;
    if (track < 25) return 2;
    if (track < 31) return 1;
    return 0;
}

uint16_t uft_track_sectors_map_b(uint16_t track)
{
    uint8_t zone = uft_track_speed_zone(track);
    return uft_sector_map_b[zone];
}

uint16_t uft_track_sectors_map_c(uint16_t track)
{
    uint8_t zone = uft_track_speed_zone(track);
    return uft_sector_map_c[zone];
}

/*============================================================================
 * DSL Output Helpers
 *============================================================================*/

static void hex_byte(FILE *out, uint8_t b)
{
    fprintf(out, "%02x", (unsigned)b);
}

static void emit_data_pairs(FILE *out, uint8_t track, uint8_t sector)
{
    for (int i = 0; i < 128; i++) {
        hex_byte(out, track);
        fputc(' ', out);
        hex_byte(out, sector);
        if (i != 127) fputc(' ', out);
    }
}

/*============================================================================
 * Sector Emission (DSL)
 *============================================================================*/

static void emit_sector_fixed(FILE *out, const uft_track_params_t *p,
                               uint16_t track, uint16_t sector)
{
    fprintf(out, "   sync %u\n", (unsigned)p->sync_len);
    fprintf(out, "   gcr 08\n");
    fprintf(out, "   begin-checksum\n");
    fprintf(out, "      checksum "); hex_byte(out, (uint8_t)(track ^ sector)); fprintf(out, "\n");
    fprintf(out, "      gcr "); hex_byte(out, (uint8_t)sector); fprintf(out, "\n");
    fprintf(out, "      gcr "); hex_byte(out, (uint8_t)track); fprintf(out, "\n");
    fprintf(out, "      gcr 30 30\n");
    fprintf(out, "   end-checksum\n");
    fprintf(out, "   gcr 0f 0f\n");
    fprintf(out, "   ; Trk %u Sec %u\n", (unsigned)track, (unsigned)sector);
    fprintf(out, "   bytes 55 55 55 55 55 55 55 55 55\n");
    
    fprintf(out, "   sync %u\n", (unsigned)p->sync_len);
    fprintf(out, "   gcr 07\n");
    fprintf(out, "   begin-checksum\n");
    fprintf(out, "      gcr ");
    for (int i = 0; i < 256; i++) {
        hex_byte(out, (uint8_t)i);
        if (i != 255) fputc(' ', out);
    }
    fprintf(out, "\n");
    fprintf(out, "      checksum 00\n");
    fprintf(out, "   end-checksum\n");
    fprintf(out, "   gcr 00 00\n");
    fprintf(out, "   bytes 55 55 55 55 55 55 55\n");
}

static void emit_sector_zoned(FILE *out, const uft_track_params_t *p,
                               uint16_t track, uint16_t sector)
{
    fprintf(out, "   sync %u\n", (unsigned)p->sync_len);
    fprintf(out, "   gcr 08\n");
    fprintf(out, "   begin-checksum\n");
    fprintf(out, "      checksum "); hex_byte(out, (uint8_t)(track ^ sector)); fprintf(out, "\n");
    fprintf(out, "      gcr "); hex_byte(out, (uint8_t)sector); fprintf(out, "\n");
    fprintf(out, "      gcr "); hex_byte(out, (uint8_t)track); fprintf(out, "\n");
    fprintf(out, "      gcr 30 30\n");
    fprintf(out, "   end-checksum\n");
    fprintf(out, "   gcr 0f 0f\n");
    fprintf(out, "   ; Trk %u Sec %u\n", (unsigned)track, (unsigned)sector);
    fprintf(out, "   bytes 55 55\n");
    
    fprintf(out, "   sync %u\n", (unsigned)p->sync_len);
    fprintf(out, "   gcr 07\n");
    fprintf(out, "   begin-checksum\n");
    fprintf(out, "      gcr ");
    emit_data_pairs(out, (uint8_t)track, (uint8_t)sector);
    fprintf(out, "\n");
    fprintf(out, "      checksum 00\n");
    fprintf(out, "   end-checksum\n");
    
    if (track >= 18) {
        fprintf(out, "   bits 1111\n");
    }
}

static void emit_sector_data_only(FILE *out, const uft_track_params_t *p,
                                   uint16_t track, uint16_t sector)
{
    fprintf(out, "   sync %u\n", (unsigned)p->sync_len);
    fprintf(out, "   gcr "); hex_byte(out, (uint8_t)sector); fprintf(out, "\n");
    fprintf(out, "   begin-checksum\n");
    fprintf(out, "      gcr ");
    emit_data_pairs(out, (uint8_t)track, (uint8_t)sector);
    fprintf(out, "\n");
    fprintf(out, "      checksum 00\n");
    fprintf(out, "   end-checksum\n");
    fprintf(out, "   bits 1111\n");
}

static void emit_raw_stream(FILE *out, const uft_track_params_t *p)
{
    fprintf(out, "   sync %u\n", (unsigned)p->sync_len);
    fprintf(out, "      gcr ");
    
    uint32_t n = (p->raw_stream_bytes == 0) ? (24U * 256U) : p->raw_stream_bytes;
    for (uint32_t i = 0; i < n; i++) {
        hex_byte(out, (uint8_t)(i & 0xFF));
        if (i + 1 != n) fputc(' ', out);
    }
    fprintf(out, "\n");
}

/*============================================================================
 * Track Layout DSL Generation
 *============================================================================*/

int uft_track_layout_write(FILE *out, const uft_track_params_t *p)
{
    if (!p) return 1;
    if (!out) out = stdout;
    if (p->track_first > p->track_last) return 2;
    
    fprintf(out, "no-tracks %u\n", (unsigned)p->no_tracks);
    fprintf(out, "track-size %u\n", (unsigned)p->track_size);
    
    for (uint16_t track = p->track_first; track <= p->track_last; track++) {
        fprintf(out, "track %u\n", (unsigned)track);
        
        /* Speed zone */
        if (p->pattern == UFT_PATTERN_ZONED || 
            p->pattern == UFT_PATTERN_ZONED_DATA_ONLY) {
            uint8_t sp = uft_track_speed_zone(track);
            fprintf(out, "   speed %u\n", (unsigned)sp);
        } else {
            fprintf(out, "   speed %u\n", (unsigned)p->speed);
        }
        
        fprintf(out, "   begin-at %u\n", (unsigned)p->begin_at);
        
        /* Emit sectors */
        switch (p->pattern) {
            case UFT_PATTERN_ZONED: {
                uint16_t smax = uft_track_sectors_map_b(track);
                for (uint16_t s = 0; s < smax; s++) {
                    emit_sector_zoned(out, p, track, s);
                }
                break;
            }
            
            case UFT_PATTERN_ZONED_DATA_ONLY: {
                uint16_t smax = uft_track_sectors_map_c(track);
                for (uint16_t s = 0; s < smax; s++) {
                    emit_sector_data_only(out, p, track, s);
                }
                break;
            }
            
            case UFT_PATTERN_RAW_STREAM:
                emit_raw_stream(out, p);
                break;
            
            case UFT_PATTERN_FIXED:
            default:
                if (p->sector_first > p->sector_last) return 3;
                for (uint16_t s = p->sector_first; s <= p->sector_last; s++) {
                    emit_sector_fixed(out, p, track, s);
                }
                break;
        }
        
        fprintf(out, "end-track\n");
    }
    
    return 0;
}

/*============================================================================
 * Raw Track Generation
 *============================================================================*/

size_t uft_sync_generate(uint16_t sync_len, uint8_t *out)
{
    if (!out) return sync_len;
    memset(out, 0xFF, sync_len);
    return sync_len;
}

size_t uft_gap_generate(uint16_t gap_len, uint8_t fill, uint8_t *out)
{
    if (!out) return gap_len;
    memset(out, fill, gap_len);
    return gap_len;
}

size_t uft_sector_header_encode(const uft_sector_header_t *hdr, uint8_t *out)
{
    if (!hdr || !out) return 0;
    
    uint8_t raw[8];
    raw[0] = hdr->id;
    raw[1] = hdr->checksum;
    raw[2] = hdr->sector;
    raw[3] = hdr->track;
    raw[4] = hdr->id2;
    raw[5] = hdr->id1;
    raw[6] = 0x0F;
    raw[7] = 0x0F;
    
    /* GCR encode 8 bytes to 10 bytes */
    uft_gcr5_encode_4to5(&raw[0], &out[0]);
    uft_gcr5_encode_4to5(&raw[4], &out[5]);
    
    return 10;
}

size_t uft_sector_data_encode(const uint8_t data[256], uint8_t *out)
{
    if (!data || !out) return 0;
    
    /* Build raw block: ID + data + checksum + pad */
    uint8_t raw[260];
    raw[0] = 0x07;  /* Data block ID */
    memcpy(&raw[1], data, 256);
    raw[257] = uft_gcr_checksum_xor(data, 256);
    raw[258] = 0x00;
    raw[259] = 0x00;
    
    /* GCR encode 260 bytes to 325 bytes (65 groups of 4→5) */
    for (int i = 0; i < 65; i++) {
        uft_gcr5_encode_4to5(&raw[i * 4], &out[i * 5]);
    }
    
    return 325;
}

int uft_track_generate(const uft_track_params_t *params, uint16_t track,
                       const uint8_t **sector_data, size_t num_sectors,
                       uint8_t *out, size_t *out_len)
{
    if (!params || !out || !out_len) return -1;
    
    size_t pos = 0;
    uint8_t disk_id1 = 0x30;  /* Default disk ID */
    uint8_t disk_id2 = 0x30;
    
    for (size_t s = 0; s < num_sectors; s++) {
        /* Sync */
        pos += uft_sync_generate(5, &out[pos]);
        
        /* Header block */
        uft_sector_header_t hdr = {
            .id = 0x08,
            .checksum = (uint8_t)(track ^ s ^ disk_id1 ^ disk_id2),
            .sector = (uint8_t)s,
            .track = (uint8_t)track,
            .id2 = disk_id2,
            .id1 = disk_id1
        };
        pos += uft_sector_header_encode(&hdr, &out[pos]);
        
        /* Header gap */
        pos += uft_gap_generate(9, 0x55, &out[pos]);
        
        /* Data sync */
        pos += uft_sync_generate(5, &out[pos]);
        
        /* Data block */
        const uint8_t *data = sector_data ? sector_data[s] : NULL;
        uint8_t empty[256] = {0};
        pos += uft_sector_data_encode(data ? data : empty, &out[pos]);
        
        /* Data gap */
        pos += uft_gap_generate(7, 0x55, &out[pos]);
    }
    
    *out_len = pos;
    return 0;
}

size_t uft_track_size_calc(const uft_track_params_t *params, uint16_t track)
{
    if (!params) return 0;
    
    uint16_t sectors;
    switch (params->pattern) {
        case UFT_PATTERN_ZONED:
            sectors = uft_track_sectors_map_b(track);
            break;
        case UFT_PATTERN_ZONED_DATA_ONLY:
            sectors = uft_track_sectors_map_c(track);
            break;
        case UFT_PATTERN_RAW_STREAM:
            return params->sync_len + params->raw_stream_bytes;
        case UFT_PATTERN_FIXED:
        default:
            sectors = params->sector_last - params->sector_first + 1;
            break;
    }
    
    return sectors * uft_sector_block_size(params);
}

size_t uft_sector_block_size(const uft_track_params_t *params)
{
    if (!params) return 0;
    
    /* Sync(5) + Header(10) + Gap(9) + Sync(5) + Data(325) + Gap(7) = 361 */
    return 5 + 10 + 9 + 5 + 325 + 7;
}

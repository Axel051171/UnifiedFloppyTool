/**
 * @file uft_samdisk.c
 * 
 * 
 * Features:
 * - SAD/SDF format support
 * - DSK (Extended) format support
 * - Raw sector reading/writing
 * - Track layout analysis
 * - Format conversion
 * - Copy protection handling
 */

#include "uft/samdisk/uft_samdisk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "uft/uft_compiler.h"

/*===========================================================================
 * Constants
 *===========================================================================*/

/* SAD format magic */
#define SAD_SIGNATURE       "Aley's disk backup"
#define SAD_SIG_LEN         18

/* Extended DSK magic */
#define EDSK_SIGNATURE      "EXTENDED CPC DSK File\r\nDisk-Info\r\n"
#define EDSK_SIG_LEN        34
#define DSK_SIGNATURE       "MV - CPC"
#define DSK_SIG_LEN         8

/* Track info block signature */
#define TRACK_INFO_SIG      "Track-Info\r\n"
#define TRACK_INFO_LEN      12

/* Maximum values */
#define MAX_TRACKS          84
#define MAX_SIDES           2
#define MAX_SECTORS         29
#define MAX_SECTOR_SIZE     8192

/* Sector size codes */
static const int SECTOR_SIZES[] = {128, 256, 512, 1024, 2048, 4096, 8192};

/*===========================================================================
 * SAD Format Structures
 *===========================================================================*/

typedef struct {
    uint8_t     signature[SAD_SIG_LEN];
    uint8_t     sides;
    uint8_t     tracks;
    uint8_t     sectors;
    uint8_t     sector_size;    /* In 64-byte units */
} sad_header_t;
UFT_PACK_END

/*===========================================================================
 * Extended DSK Structures
 *===========================================================================*/

typedef struct {
    uint8_t     signature[EDSK_SIG_LEN];
    uint8_t     creator[14];
    uint8_t     tracks;
    uint8_t     sides;
    uint8_t     unused[2];
    uint8_t     track_sizes[MAX_TRACKS * MAX_SIDES];  /* MSB of track size */
} edsk_header_t;
UFT_PACK_END

typedef struct {
    uint8_t     signature[TRACK_INFO_LEN];
    uint8_t     unused[4];
    uint8_t     track;
    uint8_t     side;
    uint8_t     unused2[2];
    uint8_t     sector_size;    /* Size code (0=128, 1=256, etc.) */
    uint8_t     sector_count;
    uint8_t     gap3_length;
    uint8_t     filler_byte;
} edsk_track_info_t;
UFT_PACK_END

typedef struct {
    uint8_t     track;
    uint8_t     side;
    uint8_t     sector_id;
    uint8_t     size_code;
    uint8_t     fdc_status1;
    uint8_t     fdc_status2;
    uint8_t     data_length_lo;
    uint8_t     data_length_hi;
} edsk_sector_info_t;
UFT_PACK_END

/*===========================================================================
 * Helpers
 *===========================================================================*/

static uint16_t read_le16(const uint8_t *p)
{
    return p[0] | (p[1] << 8);
}

static int sector_size_from_code(int code)
{
    if (code >= 0 && code < 7) {
        return SECTOR_SIZES[code];
    }
    return 512;
}

static int code_from_sector_size(int size)
{
    for (int i = 0; i < 7; i++) {
        if (SECTOR_SIZES[i] == size) return i;
    }
    return 2;  /* Default to 512 */
}

/*===========================================================================
 * SAD Format Support
 *===========================================================================*/

int uft_sad_open(uft_sad_ctx_t *ctx, const uint8_t *data, size_t size)
{
    if (!ctx || !data || size < sizeof(sad_header_t)) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    
    const sad_header_t *hdr = (const sad_header_t *)data;
    
    /* Verify signature */
    if (memcmp(hdr->signature, SAD_SIGNATURE, SAD_SIG_LEN) != 0) {
        return -1;
    }
    
    ctx->image = data;
    ctx->image_size = size;
    ctx->sides = hdr->sides;
    ctx->tracks = hdr->tracks;
    ctx->sectors = hdr->sectors;
    ctx->sector_size = hdr->sector_size * 64;
    
    /* Validate */
    size_t expected = sizeof(sad_header_t) + 
                      ctx->sides * ctx->tracks * ctx->sectors * ctx->sector_size;
    
    if (size < expected) {
        return -1;
    }
    
    ctx->valid = true;
    return 0;
}

int uft_sad_read_sector(const uft_sad_ctx_t *ctx, int track, int side, int sector,
                        uint8_t *buffer, size_t size)
{
    if (!ctx || !buffer || !ctx->valid) return -1;
    if (track < 0 || track >= ctx->tracks) return -1;
    if (side < 0 || side >= ctx->sides) return -1;
    if (sector < 0 || sector >= ctx->sectors) return -1;
    if (size < (size_t)ctx->sector_size) return -1;
    
    size_t offset = sizeof(sad_header_t);
    offset += ((track * ctx->sides + side) * ctx->sectors + sector) * ctx->sector_size;
    
    if (offset + ctx->sector_size > ctx->image_size) return -1;
    
    memcpy(buffer, ctx->image + offset, ctx->sector_size);
    return ctx->sector_size;
}

/*===========================================================================
 * Extended DSK Format Support
 *===========================================================================*/

int uft_edsk_open(uft_edsk_ctx_t *ctx, const uint8_t *data, size_t size)
{
    if (!ctx || !data || size < sizeof(edsk_header_t)) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    
    const edsk_header_t *hdr = (const edsk_header_t *)data;
    
    /* Check for extended DSK or standard DSK */
    if (memcmp(hdr->signature, EDSK_SIGNATURE, EDSK_SIG_LEN) == 0) {
        ctx->is_extended = true;
    } else if (memcmp(hdr->signature, DSK_SIGNATURE, DSK_SIG_LEN) == 0) {
        ctx->is_extended = false;
    } else {
        return -1;
    }
    
    ctx->image = data;
    ctx->image_size = size;
    ctx->tracks = hdr->tracks;
    ctx->sides = hdr->sides;
    
    /* Copy track sizes (for extended format) */
    memcpy(ctx->track_sizes, hdr->track_sizes, MAX_TRACKS * MAX_SIDES);
    
    /* Build track offset table */
    size_t offset = 0x100;  /* Header is always 256 bytes */
    
    for (int t = 0; t < ctx->tracks; t++) {
        for (int s = 0; s < ctx->sides; s++) {
            int idx = t * ctx->sides + s;
            ctx->track_offsets[idx] = offset;
            
            if (ctx->is_extended) {
                offset += ctx->track_sizes[idx] * 256;
            } else {
                /* Standard DSK: fixed track size */
                offset += read_le16((const uint8_t *)hdr + 0x32);
            }
        }
    }
    
    ctx->valid = true;
    return 0;
}

int uft_edsk_get_track_info(const uft_edsk_ctx_t *ctx, int track, int side,
                            uft_edsk_track_t *info)
{
    if (!ctx || !info || !ctx->valid) return -1;
    if (track < 0 || track >= ctx->tracks) return -1;
    if (side < 0 || side >= ctx->sides) return -1;
    
    memset(info, 0, sizeof(*info));
    
    int idx = track * ctx->sides + side;
    size_t offset = ctx->track_offsets[idx];
    
    /* Check for empty track */
    if (ctx->is_extended && ctx->track_sizes[idx] == 0) {
        info->track = track;
        info->side = side;
        info->sector_count = 0;
        return 0;
    }
    
    if (offset + sizeof(edsk_track_info_t) > ctx->image_size) return -1;
    
    const edsk_track_info_t *ti = (const edsk_track_info_t *)(ctx->image + offset);
    
    /* Verify track info signature */
    if (memcmp(ti->signature, TRACK_INFO_SIG, TRACK_INFO_LEN) != 0) {
        return -1;
    }
    
    info->track = ti->track;
    info->side = ti->side;
    info->sector_size_code = ti->sector_size;
    info->sector_count = ti->sector_count;
    info->gap3_length = ti->gap3_length;
    info->filler_byte = ti->filler_byte;
    
    /* Read sector info table */
    const edsk_sector_info_t *si = (const edsk_sector_info_t *)
                                   (ctx->image + offset + sizeof(edsk_track_info_t));
    
    for (int i = 0; i < info->sector_count && i < MAX_SECTORS; i++) {
        info->sectors[i].track = si[i].track;
        info->sectors[i].side = si[i].side;
        info->sectors[i].sector_id = si[i].sector_id;
        info->sectors[i].size_code = si[i].size_code;
        info->sectors[i].fdc_status1 = si[i].fdc_status1;
        info->sectors[i].fdc_status2 = si[i].fdc_status2;
        
        if (ctx->is_extended) {
            info->sectors[i].data_length = read_le16(&si[i].data_length_lo);
        } else {
            info->sectors[i].data_length = sector_size_from_code(si[i].size_code);
        }
    }
    
    return 0;
}

int uft_edsk_read_sector(const uft_edsk_ctx_t *ctx, int track, int side,
                         int sector_id, uint8_t *buffer, size_t *size)
{
    if (!ctx || !buffer || !size || !ctx->valid) return -1;
    
    uft_edsk_track_t info;
    if (uft_edsk_get_track_info(ctx, track, side, &info) != 0) return -1;
    
    /* Find sector in track */
    int idx = track * ctx->sides + side;
    size_t data_offset = ctx->track_offsets[idx] + 0x100;  /* Track info is 256 bytes */
    
    for (int i = 0; i < info.sector_count; i++) {
        if (info.sectors[i].sector_id == sector_id) {
            /* Found sector */
            size_t sector_size = info.sectors[i].data_length;
            
            if (*size < sector_size) {
                *size = sector_size;
                return -1;
            }
            
            if (data_offset + sector_size > ctx->image_size) return -1;
            
            memcpy(buffer, ctx->image + data_offset, sector_size);
            *size = sector_size;
            
            /* Return FDC status info */
            return (info.sectors[i].fdc_status1 << 8) | info.sectors[i].fdc_status2;
        }
        
        data_offset += info.sectors[i].data_length;
    }
    
    return -1;  /* Sector not found */
}

/*===========================================================================
 * EDSK Creation
 *===========================================================================*/

int uft_edsk_create(uft_edsk_writer_t *writer, int tracks, int sides,
                    const char *creator)
{
    if (!writer) return -1;
    
    memset(writer, 0, sizeof(*writer));
    
    writer->tracks = tracks;
    writer->sides = sides;
    
    /* Allocate buffer (estimate max size) */
    writer->buffer_size = 0x100 + tracks * sides * (0x100 + MAX_SECTORS * MAX_SECTOR_SIZE);
    writer->buffer = malloc(writer->buffer_size);
    if (!writer->buffer) return -1;
    
    memset(writer->buffer, 0, writer->buffer_size);
    
    /* Write header */
    edsk_header_t *hdr = (edsk_header_t *)writer->buffer;
    memcpy(hdr->signature, EDSK_SIGNATURE, EDSK_SIG_LEN);
    
    if (creator) {
        strncpy((char *)hdr->creator, creator, 14);
    } else {
        strncpy((char *)hdr->creator, "UFT v3.7.0", 14);
    }
    
    hdr->tracks = tracks;
    hdr->sides = sides;
    
    writer->current_offset = 0x100;
    
    return 0;
}

int uft_edsk_add_track(uft_edsk_writer_t *writer, const uft_edsk_track_t *track)
{
    if (!writer || !track || !writer->buffer) return -1;
    
    int idx = track->track * writer->sides + track->side;
    
    /* Write track info */
    edsk_track_info_t *ti = (edsk_track_info_t *)(writer->buffer + writer->current_offset);
    
    memcpy(ti->signature, TRACK_INFO_SIG, TRACK_INFO_LEN);
    ti->track = track->track;
    ti->side = track->side;
    ti->sector_size = track->sector_size_code;
    ti->sector_count = track->sector_count;
    ti->gap3_length = track->gap3_length;
    ti->filler_byte = track->filler_byte;
    
    /* Write sector info table */
    edsk_sector_info_t *si = (edsk_sector_info_t *)
                             (writer->buffer + writer->current_offset + sizeof(edsk_track_info_t));
    
    size_t data_offset = writer->current_offset + 0x100;
    
    for (int i = 0; i < track->sector_count && i < MAX_SECTORS; i++) {
        si[i].track = track->sectors[i].track;
        si[i].side = track->sectors[i].side;
        si[i].sector_id = track->sectors[i].sector_id;
        si[i].size_code = track->sectors[i].size_code;
        si[i].fdc_status1 = track->sectors[i].fdc_status1;
        si[i].fdc_status2 = track->sectors[i].fdc_status2;
        si[i].data_length_lo = track->sectors[i].data_length & 0xFF;
        si[i].data_length_hi = (track->sectors[i].data_length >> 8) & 0xFF;
        
        /* Copy sector data */
        if (track->sectors[i].data && track->sectors[i].data_length > 0) {
            memcpy(writer->buffer + data_offset, track->sectors[i].data,
                   track->sectors[i].data_length);
        }
        
        data_offset += track->sectors[i].data_length;
    }
    
    /* Update track size in header */
    size_t track_size = data_offset - writer->current_offset;
    edsk_header_t *hdr = (edsk_header_t *)writer->buffer;
    hdr->track_sizes[idx] = (track_size + 255) / 256;
    
    writer->current_offset = data_offset;
    
    return 0;
}

int uft_edsk_finish(uft_edsk_writer_t *writer, uint8_t **data, size_t *size)
{
    if (!writer || !data || !size) return -1;
    
    *data = writer->buffer;
    *size = writer->current_offset;
    
    writer->buffer = NULL;  /* Transfer ownership */
    
    return 0;
}

void uft_edsk_writer_free(uft_edsk_writer_t *writer)
{
    if (!writer) return;
    
    free(writer->buffer);
    memset(writer, 0, sizeof(*writer));
}

/*===========================================================================
 * Format Detection
 *===========================================================================*/

uft_uft_sam_format_t uft_uft_sam_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 64) return UFT_UFT_SAM_UNKNOWN;
    
    /* Check SAD signature */
    if (size >= sizeof(sad_header_t)) {
        if (memcmp(data, SAD_SIGNATURE, SAD_SIG_LEN) == 0) {
            return UFT_UFT_SAM_SAD;
        }
    }
    
    /* Check Extended DSK signature */
    if (size >= sizeof(edsk_header_t)) {
        if (memcmp(data, EDSK_SIGNATURE, EDSK_SIG_LEN) == 0) {
            return UFT_UFT_SAM_EDSK;
        }
    }
    
    /* Check standard DSK signature */
    if (memcmp(data, DSK_SIGNATURE, DSK_SIG_LEN) == 0) {
        return UFT_UFT_SAM_DSK;
    }
    
    return UFT_UFT_SAM_UNKNOWN;
}

/*===========================================================================
 * Format Conversion
 *===========================================================================*/

int uft_sad_to_edsk(const uft_sad_ctx_t *sad, uft_edsk_writer_t *writer)
{
    if (!sad || !writer || !sad->valid) return -1;
    
    if (uft_edsk_create(writer, sad->tracks, sad->sides, "UFT-SAD2EDSK") != 0) {
        return -1;
    }
    
    uint8_t *sector_buf = malloc(sad->sector_size);
    if (!sector_buf) return -1;
    
    for (int t = 0; t < sad->tracks; t++) {
        for (int s = 0; s < sad->sides; s++) {
            uft_edsk_track_t track;
            memset(&track, 0, sizeof(track));
            
            track.track = t;
            track.side = s;
            track.sector_count = sad->sectors;
            track.sector_size_code = code_from_sector_size(sad->sector_size);
            track.gap3_length = 0x52;
            track.filler_byte = 0xE5;
            
            for (int sec = 0; sec < sad->sectors; sec++) {
                track.sectors[sec].track = t;
                track.sectors[sec].side = s;
                track.sectors[sec].sector_id = sec + 1;
                track.sectors[sec].size_code = track.sector_size_code;
                track.sectors[sec].data_length = sad->sector_size;
                
                if (uft_sad_read_sector(sad, t, s, sec, sector_buf, sad->sector_size) > 0) {
                    track.sectors[sec].data = malloc(sad->sector_size);
                    if (track.sectors[sec].data) {
                        memcpy((void *)track.sectors[sec].data, sector_buf, sad->sector_size);
                    }
                }
            }
            
            uft_edsk_add_track(writer, &track);
            
            /* Free sector data */
            for (int sec = 0; sec < sad->sectors; sec++) {
                free((void *)track.sectors[sec].data);
            }
        }
    }
    
    free(sector_buf);
    return 0;
}

/*===========================================================================
 * Analysis Functions
 *===========================================================================*/

int uft_edsk_analyze(const uft_edsk_ctx_t *ctx, uft_edsk_analysis_t *analysis)
{
    if (!ctx || !analysis || !ctx->valid) return -1;
    
    memset(analysis, 0, sizeof(*analysis));
    
    analysis->tracks = ctx->tracks;
    analysis->sides = ctx->sides;
    analysis->is_extended = ctx->is_extended;
    
    int total_sectors = 0;
    int error_sectors = 0;
    int weak_sectors = 0;
    int size_variations = 0;
    int last_size = -1;
    
    for (int t = 0; t < ctx->tracks; t++) {
        for (int s = 0; s < ctx->sides; s++) {
            uft_edsk_track_t info;
            if (uft_edsk_get_track_info(ctx, t, s, &info) != 0) continue;
            
            total_sectors += info.sector_count;
            
            for (int i = 0; i < info.sector_count; i++) {
                /* Check for errors */
                if (info.sectors[i].fdc_status1 || info.sectors[i].fdc_status2) {
                    error_sectors++;
                    
                    /* Check for weak sector markers */
                    if (info.sectors[i].fdc_status2 & 0x40) {
                        weak_sectors++;
                    }
                }
                
                /* Check for size variations */
                int size = info.sectors[i].data_length;
                if (last_size >= 0 && size != last_size) {
                    size_variations++;
                }
                last_size = size;
            }
        }
    }
    
    analysis->total_sectors = total_sectors;
    analysis->error_sectors = error_sectors;
    analysis->weak_sectors = weak_sectors;
    analysis->has_size_variations = (size_variations > 0);
    analysis->has_errors = (error_sectors > 0);
    analysis->has_protection = (weak_sectors > 0 || size_variations > 5);
    
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_edsk_report_json(const uft_edsk_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || !ctx->valid) return -1;
    
    uft_edsk_analysis_t analysis;
    uft_edsk_analyze(ctx, &analysis);
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"format\": \"%s\",\n"
        "  \"tracks\": %d,\n"
        "  \"sides\": %d,\n"
        "  \"total_sectors\": %d,\n"
        "  \"error_sectors\": %d,\n"
        "  \"weak_sectors\": %d,\n"
        "  \"has_protection\": %s,\n"
        "  \"has_size_variations\": %s\n"
        "}",
        ctx->is_extended ? "Extended DSK" : "Standard DSK",
        ctx->tracks,
        ctx->sides,
        analysis.total_sectors,
        analysis.error_sectors,
        analysis.weak_sectors,
        analysis.has_protection ? "true" : "false",
        analysis.has_size_variations ? "true" : "false"
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}

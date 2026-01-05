/**
 * @file uft_amstrad_cpc.c
 * @brief Amstrad CPC DSK/EDSK Format Implementation
 * 
 * EXT3-002: Amstrad CPC disk format support
 * 
 * Supports:
 * - Standard DSK format
 * - Extended DSK (EDSK) format
 * - AMSDOS file headers
 * - CP/M directory structure
 */

#include "uft/formats/uft_amstrad_cpc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* DSK signatures */
static const char DSK_SIGNATURE[] = "MV - CPCEMU Disk-File\r\nDisk-Info\r\n";
static const char EDSK_SIGNATURE[] = "EXTENDED CPC DSK File\r\nDisk-Info\r\n";

/* Track header signature */
static const char TRACK_SIGNATURE[] = "Track-Info\r\n";

/* AMSDOS header checksum */
static uint16_t amsdos_checksum(const uint8_t *header)
{
    uint16_t sum = 0;
    for (int i = 0; i < 66; i++) {
        sum += header[i];
    }
    return sum;
}

/*===========================================================================
 * Format Detection
 *===========================================================================*/

uft_cpc_format_t uft_cpc_detect_format(const uint8_t *data, size_t size)
{
    if (!data || size < 256) {
        return UFT_CPC_FORMAT_UNKNOWN;
    }
    
    if (memcmp(data, EDSK_SIGNATURE, 22) == 0) {
        return UFT_CPC_FORMAT_EDSK;
    }
    
    if (memcmp(data, DSK_SIGNATURE, 22) == 0) {
        return UFT_CPC_FORMAT_DSK;
    }
    
    /* Check for raw sector image */
    if (size == 40 * 9 * 512 || size == 40 * 9 * 512 * 2 ||
        size == 80 * 9 * 512 || size == 80 * 9 * 512 * 2) {
        return UFT_CPC_FORMAT_RAW;
    }
    
    return UFT_CPC_FORMAT_UNKNOWN;
}

/*===========================================================================
 * DSK Parsing
 *===========================================================================*/

int uft_cpc_open(uft_cpc_ctx_t *ctx, const uint8_t *data, size_t size)
{
    if (!ctx || !data || size < 256) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->data = data;
    ctx->size = size;
    ctx->format = uft_cpc_detect_format(data, size);
    
    if (ctx->format == UFT_CPC_FORMAT_UNKNOWN) {
        return -1;
    }
    
    if (ctx->format == UFT_CPC_FORMAT_RAW) {
        /* Raw image - assume standard format */
        ctx->tracks = (size >= 80 * 9 * 512) ? 80 : 40;
        ctx->sides = (size >= ctx->tracks * 9 * 512 * 2) ? 2 : 1;
        ctx->sectors_per_track = 9;
        ctx->sector_size = 512;
        ctx->is_valid = true;
        return 0;
    }
    
    /* Parse DSK/EDSK header */
    const uint8_t *hdr = data;
    
    /* Creator name at offset 0x22 (14 bytes) */
    memcpy(ctx->creator, hdr + 0x22, 14);
    ctx->creator[14] = '\0';
    
    /* Disk geometry at offset 0x30 */
    ctx->tracks = hdr[0x30];
    ctx->sides = hdr[0x31];
    
    if (ctx->format == UFT_CPC_FORMAT_EDSK) {
        /* EDSK: track sizes at offset 0x34 */
        ctx->track_size_table = hdr + 0x34;
    } else {
        /* Standard DSK: fixed track size at offset 0x32 */
        ctx->track_size = hdr[0x32] | (hdr[0x33] << 8);
    }
    
    /* Set defaults */
    ctx->sectors_per_track = 9;
    ctx->sector_size = 512;
    ctx->is_valid = true;
    
    return 0;
}

void uft_cpc_close(uft_cpc_ctx_t *ctx)
{
    if (ctx) {
        memset(ctx, 0, sizeof(*ctx));
    }
}

/*===========================================================================
 * Track Access
 *===========================================================================*/

static size_t get_track_offset(const uft_cpc_ctx_t *ctx, uint8_t track, uint8_t side)
{
    if (!ctx) return 0;
    
    if (ctx->format == UFT_CPC_FORMAT_RAW) {
        return ((track * ctx->sides + side) * ctx->sectors_per_track * ctx->sector_size);
    }
    
    size_t offset = 0x100;  /* Skip disk header */
    
    if (ctx->format == UFT_CPC_FORMAT_EDSK) {
        /* EDSK: variable track sizes */
        int track_idx = 0;
        for (int t = 0; t < track; t++) {
            for (int s = 0; s < ctx->sides; s++) {
                uint8_t size_hi = ctx->track_size_table[track_idx++];
                offset += size_hi * 256;
            }
        }
        for (int s = 0; s < side; s++) {
            uint8_t size_hi = ctx->track_size_table[track * ctx->sides + s];
            offset += size_hi * 256;
        }
    } else {
        /* Standard DSK: fixed track size */
        offset += (track * ctx->sides + side) * ctx->track_size;
    }
    
    return offset;
}

int uft_cpc_read_track(uft_cpc_ctx_t *ctx, uint8_t track, uint8_t side,
                       uft_cpc_track_info_t *info)
{
    if (!ctx || !info) return -1;
    if (track >= ctx->tracks || side >= ctx->sides) return -1;
    
    memset(info, 0, sizeof(*info));
    info->track = track;
    info->side = side;
    
    size_t offset = get_track_offset(ctx, track, side);
    
    if (ctx->format == UFT_CPC_FORMAT_RAW) {
        /* Raw image */
        info->sector_count = ctx->sectors_per_track;
        info->sector_size = ctx->sector_size;
        info->gap3 = 0x4E;
        info->filler = 0xE5;
        
        for (int s = 0; s < info->sector_count; s++) {
            info->sectors[s].track = track;
            info->sectors[s].side = side;
            info->sectors[s].sector_id = s + 1;  /* 1-based */
            info->sectors[s].size_code = 2;      /* 512 bytes */
            info->sectors[s].fdc_status1 = 0;
            info->sectors[s].fdc_status2 = 0;
            info->sectors[s].data_offset = offset + s * 512;
            info->sectors[s].data_size = 512;
        }
        
        return 0;
    }
    
    /* DSK/EDSK track header */
    if (offset + 0x18 > ctx->size) return -1;
    
    const uint8_t *track_hdr = ctx->data + offset;
    
    /* Verify track signature */
    if (memcmp(track_hdr, TRACK_SIGNATURE, 12) != 0) {
        return -1;
    }
    
    info->sector_count = track_hdr[0x15];
    info->gap3 = track_hdr[0x16];
    info->filler = track_hdr[0x17];
    
    /* Sector info starts at offset 0x18 */
    const uint8_t *sect_info = track_hdr + 0x18;
    size_t data_offset = offset + 0x100;  /* Sector data after header */
    
    for (int s = 0; s < info->sector_count && s < 29; s++) {
        info->sectors[s].track = sect_info[s * 8 + 0];
        info->sectors[s].side = sect_info[s * 8 + 1];
        info->sectors[s].sector_id = sect_info[s * 8 + 2];
        info->sectors[s].size_code = sect_info[s * 8 + 3];
        info->sectors[s].fdc_status1 = sect_info[s * 8 + 4];
        info->sectors[s].fdc_status2 = sect_info[s * 8 + 5];
        
        if (ctx->format == UFT_CPC_FORMAT_EDSK) {
            /* EDSK: actual data size stored */
            info->sectors[s].data_size = sect_info[s * 8 + 6] | 
                                        (sect_info[s * 8 + 7] << 8);
        } else {
            /* Standard DSK: size from size code */
            info->sectors[s].data_size = 128 << info->sectors[s].size_code;
        }
        
        info->sectors[s].data_offset = data_offset;
        data_offset += info->sectors[s].data_size;
        
        /* Calculate sector size for info */
        info->sector_size = 128 << info->sectors[s].size_code;
    }
    
    return 0;
}

/*===========================================================================
 * Sector Access
 *===========================================================================*/

int uft_cpc_read_sector(uft_cpc_ctx_t *ctx, uint8_t track, uint8_t side,
                        uint8_t sector_id, uint8_t *buffer, size_t *size)
{
    if (!ctx || !buffer || !size) return -1;
    
    uft_cpc_track_info_t track_info;
    if (uft_cpc_read_track(ctx, track, side, &track_info) != 0) {
        return -1;
    }
    
    /* Find sector by ID */
    for (int s = 0; s < track_info.sector_count; s++) {
        if (track_info.sectors[s].sector_id == sector_id) {
            size_t data_size = track_info.sectors[s].data_size;
            
            if (*size < data_size) {
                *size = data_size;
                return -1;
            }
            
            if (track_info.sectors[s].data_offset + data_size > ctx->size) {
                return -1;
            }
            
            memcpy(buffer, ctx->data + track_info.sectors[s].data_offset, data_size);
            *size = data_size;
            return 0;
        }
    }
    
    return -1;  /* Sector not found */
}

/*===========================================================================
 * AMSDOS File Operations
 *===========================================================================*/

int uft_cpc_read_amsdos_header(const uint8_t *sector, uft_cpc_amsdos_header_t *header)
{
    if (!sector || !header) return -1;
    
    memset(header, 0, sizeof(*header));
    
    /* User number */
    header->user = sector[0];
    
    /* Filename (8 bytes) and extension (3 bytes) */
    memcpy(header->filename, sector + 1, 8);
    header->filename[8] = '\0';
    memcpy(header->extension, sector + 9, 3);
    header->extension[3] = '\0';
    
    /* Trim spaces */
    for (int i = 7; i >= 0 && header->filename[i] == ' '; i--) {
        header->filename[i] = '\0';
    }
    for (int i = 2; i >= 0 && header->extension[i] == ' '; i--) {
        header->extension[i] = '\0';
    }
    
    /* File type */
    header->file_type = sector[18];
    
    /* Addresses */
    header->load_address = sector[21] | (sector[22] << 8);
    header->length = sector[24] | (sector[25] << 8) | (sector[26] << 16);
    header->entry_address = sector[27] | (sector[28] << 8);
    
    /* Verify checksum */
    uint16_t stored_checksum = sector[67] | (sector[68] << 8);
    uint16_t calc_checksum = amsdos_checksum(sector);
    
    header->has_header = (stored_checksum == calc_checksum);
    
    return 0;
}

/*===========================================================================
 * Directory Operations
 *===========================================================================*/

int uft_cpc_read_directory(uft_cpc_ctx_t *ctx, uft_cpc_dirent_t *entries,
                           size_t max_entries, size_t *count)
{
    if (!ctx || !entries || !count) return -1;
    
    *count = 0;
    
    /* Directory is on tracks 0-1 (or just track 0 for single-sided) */
    /* Sectors &C1-&C9 on track 0, side 0 */
    
    uint8_t dir_buffer[512 * 4];  /* 4 directory sectors */
    
    for (int s = 0; s < 4 && s < 9; s++) {
        size_t size = 512;
        if (uft_cpc_read_sector(ctx, 0, 0, 0xC1 + s, dir_buffer + s * 512, &size) != 0) {
            /* Try standard sector IDs */
            if (uft_cpc_read_sector(ctx, 0, 0, 1 + s, dir_buffer + s * 512, &size) != 0) {
                break;
            }
        }
    }
    
    /* Parse directory entries (32 bytes each) */
    for (int i = 0; i < 64 && *count < max_entries; i++) {
        const uint8_t *entry = dir_buffer + i * 32;
        
        /* Skip deleted/empty entries */
        if (entry[0] == 0xE5) continue;
        if (entry[0] > 15) continue;  /* Invalid user number */
        
        uft_cpc_dirent_t *de = &entries[*count];
        memset(de, 0, sizeof(*de));
        
        de->user = entry[0];
        
        /* Filename */
        for (int j = 0; j < 8; j++) {
            de->filename[j] = entry[1 + j] & 0x7F;
        }
        de->filename[8] = '\0';
        
        /* Extension (with attribute bits) */
        for (int j = 0; j < 3; j++) {
            de->extension[j] = entry[9 + j] & 0x7F;
        }
        de->extension[3] = '\0';
        
        /* Attributes */
        de->read_only = (entry[9] & 0x80) != 0;
        de->system = (entry[10] & 0x80) != 0;
        de->archived = (entry[11] & 0x80) != 0;
        
        /* Extent info */
        de->extent = entry[12] | ((entry[14] & 0x3F) << 5);
        de->records = entry[15];
        
        /* Size (records * 128 bytes) */
        de->size = de->records * 128;
        
        /* Allocation blocks */
        for (int j = 0; j < 16; j++) {
            de->blocks[j] = entry[16 + j];
        }
        
        (*count)++;
    }
    
    return 0;
}

/*===========================================================================
 * File Operations
 *===========================================================================*/

int uft_cpc_read_file(uft_cpc_ctx_t *ctx, const char *filename,
                      uint8_t *buffer, size_t max_size, size_t *bytes_read)
{
    if (!ctx || !filename || !buffer || !bytes_read) return -1;
    
    /* Parse filename.ext */
    char name[9] = {0}, ext[4] = {0};
    const char *dot = strchr(filename, '.');
    
    if (dot) {
        size_t name_len = dot - filename;
        if (name_len > 8) name_len = 8;
        memcpy(name, filename, name_len);
        strncpy(ext, dot + 1, 3);
    } else {
        strncpy(name, filename, 8);
    }
    
    /* Convert to uppercase */
    for (int i = 0; name[i]; i++) name[i] = toupper(name[i]);
    for (int i = 0; ext[i]; i++) ext[i] = toupper(ext[i]);
    
    /* Read directory */
    uft_cpc_dirent_t entries[64];
    size_t entry_count = 0;
    
    if (uft_cpc_read_directory(ctx, entries, 64, &entry_count) != 0) {
        return -1;
    }
    
    /* Find matching entries and collect blocks */
    *bytes_read = 0;
    
    for (size_t i = 0; i < entry_count; i++) {
        /* Trim trailing spaces for comparison */
        char entry_name[9], entry_ext[4];
        strncpy(entry_name, entries[i].filename, 8);
        strncpy(entry_ext, entries[i].extension, 3);
        entry_name[8] = entry_ext[3] = '\0';
        
        for (int j = 7; j >= 0 && entry_name[j] == ' '; j--) entry_name[j] = '\0';
        for (int j = 2; j >= 0 && entry_ext[j] == ' '; j--) entry_ext[j] = '\0';
        
        if (strcmp(name, entry_name) == 0 && strcmp(ext, entry_ext) == 0) {
            /* Read blocks */
            for (int b = 0; b < 16 && entries[i].blocks[b] != 0; b++) {
                uint8_t block = entries[i].blocks[b];
                
                /* Convert block to track/sector */
                /* Block = 1KB = 2 sectors on Amstrad */
                int sector_base = block * 2;
                
                for (int s = 0; s < 2; s++) {
                    int abs_sector = sector_base + s;
                    int track = abs_sector / 9;
                    int sector = (abs_sector % 9) + 0xC1;
                    
                    size_t read_size = 512;
                    if (*bytes_read + 512 > max_size) {
                        read_size = max_size - *bytes_read;
                    }
                    
                    if (uft_cpc_read_sector(ctx, track, 0, sector,
                                           buffer + *bytes_read, &read_size) == 0) {
                        *bytes_read += read_size;
                    }
                }
            }
        }
    }
    
    return (*bytes_read > 0) ? 0 : -1;
}

/*===========================================================================
 * Format Detection Helpers
 *===========================================================================*/

const char *uft_cpc_format_name(uft_cpc_format_t format)
{
    switch (format) {
        case UFT_CPC_FORMAT_DSK:  return "Standard DSK";
        case UFT_CPC_FORMAT_EDSK: return "Extended DSK";
        case UFT_CPC_FORMAT_RAW:  return "Raw Sector Image";
        default:                  return "Unknown";
    }
}

int uft_cpc_report_json(const uft_cpc_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"format\": \"%s\",\n"
        "  \"valid\": %s,\n"
        "  \"creator\": \"%s\",\n"
        "  \"tracks\": %u,\n"
        "  \"sides\": %u,\n"
        "  \"sectors_per_track\": %u,\n"
        "  \"sector_size\": %u,\n"
        "  \"total_size\": %zu\n"
        "}",
        uft_cpc_format_name(ctx->format),
        ctx->is_valid ? "true" : "false",
        ctx->creator,
        ctx->tracks,
        ctx->sides,
        ctx->sectors_per_track,
        ctx->sector_size,
        ctx->size
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}

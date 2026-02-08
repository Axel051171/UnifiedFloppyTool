/**
 * @file uft_ibm_format.c
 * @brief IBM PC Floppy Format Implementation
 * 
 * EXT3-014: IBM PC disk format support
 * 
 * Features:
 * - IMG/IMA/DSK raw sector images
 * - IMD (ImageDisk) format
 * - FAT12/FAT16 boot sector parsing
 * - Multiple density support (SD/DD/HD/ED)
 * - Interleave handling
 */

#include "uft/formats/uft_ibm_format.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Standard PC Formats
 *===========================================================================*/

static const uft_ibm_geometry_t IBM_GEOMETRIES[] = {
    /* 5.25" formats */
    { 160,  40, 1,  8, 512, 300, 250 },   /* 160K SS/DD */
    { 180,  40, 1,  9, 512, 300, 250 },   /* 180K SS/DD */
    { 320,  40, 2,  8, 512, 300, 250 },   /* 320K DS/DD */
    { 360,  40, 2,  9, 512, 300, 250 },   /* 360K DS/DD */
    { 1200, 80, 2, 15, 512, 360, 500 },   /* 1.2M DS/HD */
    
    /* 3.5" formats */
    { 720,  80, 2,  9, 512, 300, 250 },   /* 720K DS/DD */
    { 1440, 80, 2, 18, 512, 300, 500 },   /* 1.44M DS/HD */
    { 2880, 80, 2, 36, 512, 300, 1000 },  /* 2.88M DS/ED */
    
    /* DMF format */
    { 1680, 80, 2, 21, 512, 300, 500 },   /* 1.68M DMF */
    { 1720, 80, 2, 22, 512, 300, 500 },   /* 1.72M */
    
    { 0, 0, 0, 0, 0, 0, 0 }  /* End marker */
};

/*===========================================================================
 * Helpers
 *===========================================================================*/

static uint16_t read_le16(const uint8_t *p)
{
    return p[0] | (p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/*===========================================================================
 * Geometry Detection
 *===========================================================================*/

const uft_ibm_geometry_t *uft_ibm_detect_geometry(size_t size)
{
    for (int i = 0; IBM_GEOMETRIES[i].total_kb != 0; i++) {
        size_t expected = IBM_GEOMETRIES[i].tracks * 
                          IBM_GEOMETRIES[i].heads *
                          IBM_GEOMETRIES[i].sectors *
                          IBM_GEOMETRIES[i].sector_size;
        
        if (size == expected) {
            return &IBM_GEOMETRIES[i];
        }
    }
    
    return NULL;
}

int uft_ibm_geometry_from_bpb(const uint8_t *boot, uft_ibm_geometry_t *geo)
{
    if (!boot || !geo) return -1;
    
    memset(geo, 0, sizeof(*geo));
    
    /* Check for valid boot sector */
    if (boot[0] != 0xEB && boot[0] != 0xE9 && boot[0] != 0x00) {
        return -1;
    }
    
    /* Parse BPB */
    geo->sector_size = read_le16(boot + 11);
    if (geo->sector_size != 512 && geo->sector_size != 1024 && 
        geo->sector_size != 2048 && geo->sector_size != 4096) {
        return -1;
    }
    
    geo->sectors = read_le16(boot + 24);  /* Sectors per track */
    geo->heads = read_le16(boot + 26);
    
    uint16_t total_sectors = read_le16(boot + 19);
    if (total_sectors == 0) {
        total_sectors = read_le32(boot + 32);  /* Large disk */
    }
    
    if (geo->sectors == 0 || geo->heads == 0) return -1;
    
    geo->tracks = total_sectors / (geo->sectors * geo->heads);
    geo->total_kb = (total_sectors * geo->sector_size) / 1024;
    
    return 0;
}

/*===========================================================================
 * IMD Format
 *===========================================================================*/

/* IMD header is ASCII text until 0x1A */
#define IMD_MAGIC "IMD "

int uft_ibm_imd_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 32) return 0;
    return memcmp(data, IMD_MAGIC, 4) == 0;
}

int uft_ibm_imd_parse_header(const uint8_t *data, size_t size,
                             uft_ibm_imd_header_t *header)
{
    if (!data || !header || size < 32) return -1;
    if (memcmp(data, IMD_MAGIC, 4) != 0) return -1;
    
    memset(header, 0, sizeof(*header));
    
    /* Find end of header (0x1A) */
    size_t header_end = 0;
    for (size_t i = 0; i < size && i < 256; i++) {
        if (data[i] == 0x1A) {
            header_end = i;
            break;
        }
    }
    
    if (header_end == 0) return -1;
    
    /* Copy header text */
    if (header_end < sizeof(header->comment)) {
        memcpy(header->comment, data, header_end);
        header->comment[header_end] = '\0';
    }
    
    header->data_offset = header_end + 1;
    
    return 0;
}

int uft_ibm_imd_read_track(const uint8_t *imd_data, size_t imd_size,
                           size_t *offset, uft_ibm_imd_track_t *track)
{
    if (!imd_data || !offset || !track || *offset >= imd_size) {
        return -1;
    }
    
    memset(track, 0, sizeof(*track));
    
    const uint8_t *p = imd_data + *offset;
    size_t remaining = imd_size - *offset;
    
    if (remaining < 5) return -1;
    
    /* Track header */
    track->mode = p[0];
    track->cylinder = p[1];
    track->head = p[2] & 0x0F;
    track->head_flags = p[2] & 0xF0;
    track->sector_count = p[3];
    track->sector_size_code = p[4];
    
    /* Calculate sector size */
    if (track->sector_size_code == 0xFF) {
        /* Variable sector sizes - not supported yet */
        return -1;
    }
    track->sector_size = 128 << track->sector_size_code;
    
    size_t pos = 5;
    
    /* Sector numbering map */
    if (pos + track->sector_count > remaining) return -1;
    memcpy(track->sector_map, p + pos, track->sector_count);
    pos += track->sector_count;
    
    /* Optional cylinder map */
    if (track->head_flags & 0x80) {
        if (pos + track->sector_count > remaining) return -1;
        memcpy(track->cylinder_map, p + pos, track->sector_count);
        pos += track->sector_count;
    }
    
    /* Optional head map */
    if (track->head_flags & 0x40) {
        if (pos + track->sector_count > remaining) return -1;
        memcpy(track->head_map, p + pos, track->sector_count);
        pos += track->sector_count;
    }
    
    /* Sector data */
    for (int s = 0; s < track->sector_count; s++) {
        if (pos >= remaining) return -1;
        
        uint8_t data_type = p[pos++];
        track->sector_types[s] = data_type;
        
        switch (data_type) {
            case 0x00:  /* Unavailable */
                break;
            case 0x01:  /* Normal data */
            case 0x03:  /* Normal deleted */
            case 0x05:  /* Normal error */
            case 0x07:  /* Deleted error */
                if (pos + track->sector_size > remaining) return -1;
                memcpy(track->sector_data[s], p + pos, track->sector_size);
                pos += track->sector_size;
                break;
            case 0x02:  /* Compressed */
            case 0x04:  /* Compressed deleted */
            case 0x06:  /* Compressed error */
            case 0x08:  /* Compressed deleted error */
                if (pos >= remaining) return -1;
                memset(track->sector_data[s], p[pos], track->sector_size);
                pos++;
                break;
            default:
                return -1;
        }
    }
    
    *offset += pos;
    return 0;
}

/*===========================================================================
 * Raw Image Operations
 *===========================================================================*/

int uft_ibm_open(uft_ibm_ctx_t *ctx, const uint8_t *data, size_t size)
{
    if (!ctx || !data) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->data = data;
    ctx->size = size;
    
    /* Check for IMD format */
    if (uft_ibm_imd_detect(data, size)) {
        ctx->format = UFT_IBM_FORMAT_IMD;
        
        uft_ibm_imd_header_t imd_hdr;
        if (uft_ibm_imd_parse_header(data, size, &imd_hdr) != 0) {
            return -1;
        }
        
        ctx->data_offset = imd_hdr.data_offset;
        
        /* Scan tracks to determine geometry */
        size_t offset = ctx->data_offset;
        uft_ibm_imd_track_t track;
        
        while (uft_ibm_imd_read_track(data, size, &offset, &track) == 0) {
            if (track.cylinder >= ctx->geometry.tracks) {
                ctx->geometry.tracks = track.cylinder + 1;
            }
            if (track.head >= ctx->geometry.heads) {
                ctx->geometry.heads = track.head + 1;
            }
            if (track.sector_count > ctx->geometry.sectors) {
                ctx->geometry.sectors = track.sector_count;
            }
            ctx->geometry.sector_size = track.sector_size;
        }
        
        ctx->geometry.total_kb = (ctx->geometry.tracks * ctx->geometry.heads *
                                  ctx->geometry.sectors * ctx->geometry.sector_size) / 1024;
    } else {
        /* Raw sector image */
        ctx->format = UFT_IBM_FORMAT_IMG;
        
        /* Try to detect geometry from BPB */
        if (uft_ibm_geometry_from_bpb(data, &ctx->geometry) != 0) {
            /* Try to detect by size */
            const uft_ibm_geometry_t *geo = uft_ibm_detect_geometry(size);
            if (geo) {
                ctx->geometry = *geo;
            } else {
                return -1;
            }
        }
    }
    
    ctx->is_valid = true;
    return 0;
}

void uft_ibm_close(uft_ibm_ctx_t *ctx)
{
    if (ctx) {
        memset(ctx, 0, sizeof(*ctx));
    }
}

int uft_ibm_read_sector(const uft_ibm_ctx_t *ctx,
                        uint8_t track, uint8_t head, uint8_t sector,
                        uint8_t *buffer, size_t *size)
{
    if (!ctx || !buffer || !size || !ctx->is_valid) return -1;
    
    if (track >= ctx->geometry.tracks || head >= ctx->geometry.heads) {
        return -1;
    }
    
    if (ctx->format == UFT_IBM_FORMAT_IMD) {
        /* IMD: need to scan for track */
        size_t offset = ctx->data_offset;
        uft_ibm_imd_track_t trk;
        
        while (uft_ibm_imd_read_track(ctx->data, ctx->size, &offset, &trk) == 0) {
            if (trk.cylinder == track && trk.head == head) {
                /* Find sector in map */
                for (int s = 0; s < trk.sector_count; s++) {
                    if (trk.sector_map[s] == sector) {
                        if (*size < trk.sector_size) {
                            *size = trk.sector_size;
                            return -1;
                        }
                        memcpy(buffer, trk.sector_data[s], trk.sector_size);
                        *size = trk.sector_size;
                        return 0;
                    }
                }
                return -1;  /* Sector not found */
            }
        }
        return -1;  /* Track not found */
    } else {
        /* Raw image */
        if (sector < 1 || sector > ctx->geometry.sectors) {
            return -1;
        }
        
        size_t lba = (track * ctx->geometry.heads + head) * ctx->geometry.sectors +
                     (sector - 1);
        size_t offset = lba * ctx->geometry.sector_size;
        
        if (offset + ctx->geometry.sector_size > ctx->size) {
            return -1;
        }
        
        if (*size < ctx->geometry.sector_size) {
            *size = ctx->geometry.sector_size;
            return -1;
        }
        
        memcpy(buffer, ctx->data + offset, ctx->geometry.sector_size);
        *size = ctx->geometry.sector_size;
        
        return 0;
    }
}

/*===========================================================================
 * Boot Sector Analysis
 *===========================================================================*/

int uft_ibm_read_boot_sector(const uft_ibm_ctx_t *ctx, uft_ibm_boot_t *boot)
{
    if (!ctx || !boot || !ctx->is_valid) return -1;
    
    memset(boot, 0, sizeof(*boot));
    
    uint8_t sector[4096];
    size_t size = sizeof(sector);
    
    if (uft_ibm_read_sector(ctx, 0, 0, 1, sector, &size) != 0) {
        return -1;
    }
    
    /* OEM name */
    memcpy(boot->oem_name, sector + 3, 8);
    boot->oem_name[8] = '\0';
    
    /* BPB */
    boot->bytes_per_sector = read_le16(sector + 11);
    boot->sectors_per_cluster = sector[13];
    boot->reserved_sectors = read_le16(sector + 14);
    boot->num_fats = sector[16];
    boot->root_entries = read_le16(sector + 17);
    boot->total_sectors = read_le16(sector + 19);
    boot->media_descriptor = sector[21];
    boot->sectors_per_fat = read_le16(sector + 22);
    boot->sectors_per_track = read_le16(sector + 24);
    boot->heads = read_le16(sector + 26);
    boot->hidden_sectors = read_le32(sector + 28);
    
    if (boot->total_sectors == 0) {
        boot->total_sectors = read_le32(sector + 32);
    }
    
    /* Volume info (FAT12/16) */
    if (sector[38] == 0x28 || sector[38] == 0x29) {
        boot->drive_number = sector[36];
        boot->volume_serial = read_le32(sector + 39);
        memcpy(boot->volume_label, sector + 43, 11);
        boot->volume_label[11] = '\0';
        memcpy(boot->fs_type, sector + 54, 8);
        boot->fs_type[8] = '\0';
    }
    
    /* Check signature */
    boot->has_signature = (sector[510] == 0x55 && sector[511] == 0xAA);
    
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

const char *uft_ibm_format_name(uft_ibm_format_t format)
{
    switch (format) {
        case UFT_IBM_FORMAT_IMG: return "IMG (Raw)";
        case UFT_IBM_FORMAT_IMD: return "IMD (ImageDisk)";
        case UFT_IBM_FORMAT_TD0: return "TD0 (Teledisk)";
        default:                return "Unknown";
    }
}

int uft_ibm_report_json(const uft_ibm_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    uft_ibm_boot_t boot;
    const char *fs_type = "Unknown";
    const char *vol_label = "";
    
    if (uft_ibm_read_boot_sector(ctx, &boot) == 0) {
        fs_type = boot.fs_type;
        vol_label = boot.volume_label;
    }
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"format\": \"%s\",\n"
        "  \"valid\": %s,\n"
        "  \"tracks\": %u,\n"
        "  \"heads\": %u,\n"
        "  \"sectors\": %u,\n"
        "  \"sector_size\": %u,\n"
        "  \"total_kb\": %u,\n"
        "  \"filesystem\": \"%s\",\n"
        "  \"volume_label\": \"%s\",\n"
        "  \"file_size\": %zu\n"
        "}",
        uft_ibm_format_name(ctx->format),
        ctx->is_valid ? "true" : "false",
        ctx->geometry.tracks,
        ctx->geometry.heads,
        ctx->geometry.sectors,
        ctx->geometry.sector_size,
        ctx->geometry.total_kb,
        fs_type,
        vol_label,
        ctx->size
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}

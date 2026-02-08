#include "uft/compat/uft_platform.h"
/**
 * @file uft_imd.c
 * @brief ImageDisk (IMD) Format Implementation for UFT
 * 
 * @copyright UFT Project - Based on public domain IMD format specification
 */

#include "uft_imd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*============================================================================
 * Gap Length Tables (from IMD source code)
 *============================================================================*/

/* 8" FM gap lengths */
const uft_imd_gap_entry_t uft_imd_gap_8inch_fm[] = {
    { 0, 0x1A, 0x07, 0x1B },  /* 128 bytes, max 26 sectors */
    { 1, 0x0F, 0x0E, 0x2A },  /* 256 bytes, max 15 sectors */
    { 2, 0x08, 0x1B, 0x3A },  /* 512 bytes, max 8 sectors */
    { 3, 0x04, 0x47, 0x8A },  /* 1024 bytes, max 4 sectors */
    { 4, 0x02, 0xC8, 0xFF },  /* 2048 bytes, max 2 sectors */
    { 5, 0x01, 0xC8, 0xFF },  /* 4096 bytes, max 1 sector */
    { 0xFF, 0, 0, 0 }         /* End marker */
};

/* 8" MFM gap lengths */
const uft_imd_gap_entry_t uft_imd_gap_8inch_mfm[] = {
    { 1, 0x1A, 0x0E, 0x36 },  /* 256 bytes, max 26 sectors */
    { 2, 0x0F, 0x1B, 0x54 },  /* 512 bytes, max 15 sectors */
    { 3, 0x08, 0x35, 0x74 },  /* 1024 bytes, max 8 sectors */
    { 4, 0x04, 0x99, 0xFF },  /* 2048 bytes, max 4 sectors */
    { 5, 0x02, 0xC8, 0xFF },  /* 4096 bytes, max 2 sectors */
    { 6, 0x01, 0xC8, 0xFF },  /* 8192 bytes, max 1 sector */
    { 0xFF, 0, 0, 0 }         /* End marker */
};

/* 5.25" FM gap lengths */
const uft_imd_gap_entry_t uft_imd_gap_5inch_fm[] = {
    { 0, 0x12, 0x07, 0x09 },  /* 128 bytes, max 18 sectors */
    { 0, 0x10, 0x10, 0x19 },  /* 128 bytes, max 16 sectors (alt) */
    { 1, 0x08, 0x18, 0x30 },  /* 256 bytes, max 8 sectors */
    { 2, 0x04, 0x46, 0x87 },  /* 512 bytes, max 4 sectors */
    { 3, 0x02, 0xC8, 0xFF },  /* 1024 bytes, max 2 sectors */
    { 4, 0x01, 0xC8, 0xFF },  /* 2048 bytes, max 1 sector */
    { 0xFF, 0, 0, 0 }         /* End marker */
};

/* 5.25"/3.5" MFM gap lengths */
const uft_imd_gap_entry_t uft_imd_gap_5inch_mfm[] = {
    { 1, 0x12, 0x0A, 0x0C },  /* 256 bytes, max 18 sectors */
    { 1, 0x10, 0x20, 0x32 },  /* 256 bytes, max 16 sectors (alt) */
    { 2, 0x08, 0x2A, 0x50 },  /* 512 bytes, max 8 sectors */
    { 2, 0x09, 0x18, 0x40 },  /* 512 bytes, max 9 sectors */
    { 2, 0x0A, 0x07, 0x0E },  /* 512 bytes, max 10 sectors */
    { 2, 0x12, 0x1B, 0x54 },  /* 512 bytes, max 18 sectors (1.44MB) */
    { 3, 0x04, 0x8D, 0xF0 },  /* 1024 bytes, max 4 sectors */
    { 4, 0x02, 0xC8, 0xFF },  /* 2048 bytes, max 2 sectors */
    { 5, 0x01, 0xC8, 0xFF },  /* 4096 bytes, max 1 sector */
    { 0xFF, 0, 0, 0 }         /* End marker */
};

/*============================================================================
 * IMD Initialization and Cleanup
 *============================================================================*/

int uft_imd_init(uft_imd_image_t* img)
{
    if (!img) return -1;
    memset(img, 0, sizeof(*img));
    return 0;
}

void uft_imd_free(uft_imd_image_t* img)
{
    if (!img) return;
    
    if (img->comment) {
        free(img->comment);
        img->comment = NULL;
    }
    
    if (img->tracks) {
        for (uint16_t i = 0; i < img->num_tracks; i++) {
            if (img->tracks[i].data) {
                free(img->tracks[i].data);
            }
        }
        free(img->tracks);
        img->tracks = NULL;
    }
    
    img->num_tracks = 0;
}

/*============================================================================
 * Header Parsing
 *============================================================================*/

int uft_imd_parse_header(const char* line, uft_imd_header_t* header)
{
    if (!line || !header) return -1;
    
    /* Expected format: "IMD v.vv: dd/mm/yyyy hh:mm:ss" */
    memset(header, 0, sizeof(*header));
    
    /* Check signature */
    if (strncmp(line, "IMD ", 4) != 0) return -1;
    
    /* Parse version */
    const char* p = line + 4;
    header->version_major = 0;
    header->version_minor = 0;
    
    while (isdigit(*p)) {
        header->version_major = header->version_major * 10 + (*p - '0');
        p++;
    }
    if (*p == '.') {
        p++;
        while (isdigit(*p)) {
            header->version_minor = header->version_minor * 10 + (*p - '0');
            p++;
        }
    }
    
    /* Skip to date */
    while (*p && *p != ':') p++;
    if (*p == ':') p++;
    while (*p == ' ') p++;
    
    /* Parse date: dd/mm/yyyy */
    if (sscanf(p, "%hhu/%hhu/%hu", &header->day, &header->month, &header->year) != 3) {
        /* Try alternate format mm/dd/yyyy */
        sscanf(p, "%hhu/%hhu/%hu", &header->month, &header->day, &header->year);
    }
    
    /* Skip to time */
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    
    /* Parse time: hh:mm:ss */
    sscanf(p, "%hhu:%hhu:%hhu", &header->hour, &header->minute, &header->second);
    
    return 0;
}

/*============================================================================
 * IMD Reading
 *============================================================================*/

int uft_imd_read_mem(const uint8_t* data, size_t size, uft_imd_image_t* img)
{
    if (!data || !img || size < 32) return -1;
    
    uft_imd_init(img);
    
    size_t pos = 0;
    
    /* Parse ASCII header line */
    const char* header_end = memchr(data, '\n', size);
    if (!header_end) header_end = memchr(data, '\r', size);
    if (!header_end) return -1;
    
    char header_line[128] = {0};
    size_t header_len = header_end - (const char*)data;
    if (header_len >= sizeof(header_line)) header_len = sizeof(header_line) - 1;
    memcpy(header_line, data, header_len);
    
    if (uft_imd_parse_header(header_line, &img->header) != 0) {
        return -1;
    }
    
    pos = header_end - (const char*)data;
    while (pos < size && (data[pos] == '\r' || data[pos] == '\n')) pos++;
    
    /* Read comment until 0x1A */
    size_t comment_start = pos;
    while (pos < size && data[pos] != UFT_IMD_COMMENT_END) pos++;
    
    if (pos > comment_start) {
        img->comment_len = pos - comment_start;
        img->comment = malloc(img->comment_len + 1);
        if (img->comment) {
            memcpy(img->comment, data + comment_start, img->comment_len);
            img->comment[img->comment_len] = '\0';
        }
    }
    
    /* Skip 0x1A terminator */
    if (pos < size && data[pos] == UFT_IMD_COMMENT_END) pos++;
    
    /* Count tracks first */
    size_t track_count = 0;
    size_t scan_pos = pos;
    
    while (scan_pos + sizeof(uft_imd_track_header_t) <= size) {
        const uft_imd_track_header_t* thdr = 
            (const uft_imd_track_header_t*)(data + scan_pos);
        
        if (thdr->mode > UFT_IMD_MODE_MAX) break;
        if (thdr->nsectors == 0) break;
        
        track_count++;
        scan_pos += sizeof(uft_imd_track_header_t);
        scan_pos += thdr->nsectors;  /* Sector map */
        
        uint8_t head_flags = thdr->head;
        if (head_flags & UFT_IMD_HEAD_CYLMAP) scan_pos += thdr->nsectors;
        if (head_flags & UFT_IMD_HEAD_HEADMAP) scan_pos += thdr->nsectors;
        
        /* Skip sector data */
        uint16_t sector_size = uft_imd_ssize_to_bytes(thdr->sector_size);
        for (uint8_t s = 0; s < thdr->nsectors && scan_pos < size; s++) {
            uint8_t stype = data[scan_pos++];
            if (uft_imd_sec_has_data(stype)) {
                if (uft_imd_sec_is_compressed(stype)) {
                    scan_pos++;  /* Just fill byte */
                } else {
                    scan_pos += sector_size;
                }
            }
        }
    }
    
    if (track_count == 0) return -1;
    
    /* Allocate tracks */
    img->tracks = calloc(track_count, sizeof(uft_imd_track_t));
    if (!img->tracks) return -1;
    img->num_tracks = track_count;
    
    /* Read tracks */
    uint8_t max_cyl = 0, max_head = 0;
    
    for (uint16_t t = 0; t < track_count && pos < size; t++) {
        uft_imd_track_t* track = &img->tracks[t];
        
        if (pos + sizeof(uft_imd_track_header_t) > size) break;
        
        memcpy(&track->header, data + pos, sizeof(uft_imd_track_header_t));
        pos += sizeof(uft_imd_track_header_t);
        
        uint8_t nsectors = track->header.nsectors;
        uint8_t head_flags = track->header.head;
        track->header.head &= UFT_IMD_HEAD_MASK;
        
        track->has_cylmap = (head_flags & UFT_IMD_HEAD_CYLMAP) != 0;
        track->has_headmap = (head_flags & UFT_IMD_HEAD_HEADMAP) != 0;
        
        /* Update geometry */
        if (track->header.cylinder > max_cyl) max_cyl = track->header.cylinder;
        if (track->header.head > max_head) max_head = track->header.head;
        
        /* Read sector map */
        if (pos + nsectors > size) break;
        memcpy(track->smap, data + pos, nsectors);
        pos += nsectors;
        
        /* Read optional cylinder map */
        if (track->has_cylmap) {
            if (pos + nsectors > size) break;
            memcpy(track->cmap, data + pos, nsectors);
            pos += nsectors;
        }
        
        /* Read optional head map */
        if (track->has_headmap) {
            if (pos + nsectors > size) break;
            memcpy(track->hmap, data + pos, nsectors);
            pos += nsectors;
        }
        
        /* Calculate data size needed */
        uint16_t sector_size = uft_imd_ssize_to_bytes(track->header.sector_size);
        size_t data_needed = 0;
        
        size_t temp_pos = pos;
        for (uint8_t s = 0; s < nsectors && temp_pos < size; s++) {
            uint8_t stype = data[temp_pos++];
            track->stype[s] = stype;
            
            if (uft_imd_sec_has_data(stype)) {
                data_needed += sector_size;
                if (uft_imd_sec_is_compressed(stype)) {
                    temp_pos++;
                } else {
                    temp_pos += sector_size;
                }
            }
            
            /* Update statistics */
            img->total_sectors++;
            if (uft_imd_sec_is_compressed(stype)) img->compressed_sectors++;
            if (uft_imd_sec_is_deleted(stype)) img->deleted_sectors++;
            if (uft_imd_sec_has_error(stype)) img->bad_sectors++;
            if (!uft_imd_sec_has_data(stype)) img->unavail_sectors++;
        }
        
        /* Allocate and read sector data */
        if (data_needed > 0) {
            track->data = malloc(data_needed);
            if (!track->data) continue;
            track->data_size = data_needed;
            
            size_t data_offset = 0;
            for (uint8_t s = 0; s < nsectors && pos < size; s++) {
                track->sector_offsets[s] = data_offset;
                uint8_t stype = data[pos++];
                
                if (uft_imd_sec_has_data(stype)) {
                    if (uft_imd_sec_is_compressed(stype)) {
                        uint8_t fill = data[pos++];
                        memset(track->data + data_offset, fill, sector_size);
                    } else {
                        if (pos + sector_size <= size) {
                            memcpy(track->data + data_offset, data + pos, sector_size);
                        }
                        pos += sector_size;
                    }
                    data_offset += sector_size;
                }
            }
        }
    }
    
    img->num_cylinders = max_cyl + 1;
    img->num_heads = max_head + 1;
    
    return 0;
}

int uft_imd_read(const char* filename, uft_imd_image_t* img)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size <= 0 || size > 64*1024*1024) {
        fclose(fp);
        return -1;
    }
    
    uint8_t* data = malloc(size);
    if (!data) {
        fclose(fp);
        return -1;
    }
    
    if (fread(data, 1, size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    
    int result = uft_imd_read_mem(data, size, img);
    free(data);
    
    return result;
}

/*============================================================================
 * Gap Length Lookup
 *============================================================================*/

int uft_imd_get_gap_lengths(uft_imd_mode_t mode, uint8_t sector_size,
                            uint8_t nsectors, uint8_t* gap_write,
                            uint8_t* gap_format)
{
    const uft_imd_gap_entry_t* table;
    bool mfm = uft_imd_mode_is_mfm(mode);
    uint16_t rate = uft_imd_mode_to_rate(mode);
    
    /* Select table based on rate and density */
    if (rate == 500) {
        /* 8" or HD - use 8" tables */
        table = mfm ? uft_imd_gap_8inch_mfm : uft_imd_gap_8inch_fm;
    } else {
        /* 5.25" or 3.5" */
        table = mfm ? uft_imd_gap_5inch_mfm : uft_imd_gap_5inch_fm;
    }
    
    /* Find matching entry */
    for (size_t i = 0; table[i].sector_size != 0xFF; i++) {
        if (table[i].sector_size == sector_size && 
            nsectors <= table[i].max_sectors) {
            if (gap_write) *gap_write = table[i].gap_write;
            if (gap_format) *gap_format = table[i].gap_format;
            return 0;
        }
    }
    
    /* Default gaps if no match */
    if (gap_write) *gap_write = 0x1B;
    if (gap_format) *gap_format = 0x54;
    return -1;
}

/*============================================================================
 * Track/Sector Access
 *============================================================================*/

uft_imd_track_t* uft_imd_get_track(uft_imd_image_t* img, 
                                    uint8_t cylinder, uint8_t head)
{
    if (!img || !img->tracks) return NULL;
    
    for (uint16_t i = 0; i < img->num_tracks; i++) {
        if (img->tracks[i].header.cylinder == cylinder &&
            (img->tracks[i].header.head & UFT_IMD_HEAD_MASK) == head) {
            return &img->tracks[i];
        }
    }
    return NULL;
}

int uft_imd_read_sector(const uft_imd_track_t* track, uint8_t sector_num,
                        uint8_t* buffer, size_t size)
{
    if (!track || !buffer) return -1;
    
    /* Find sector in map */
    int sector_idx = -1;
    for (uint8_t i = 0; i < track->header.nsectors; i++) {
        if (track->smap[i] == sector_num) {
            sector_idx = i;
            break;
        }
    }
    
    if (sector_idx < 0) return -1;
    
    /* Check if data available */
    if (!uft_imd_sec_has_data(track->stype[sector_idx])) {
        return -1;
    }
    
    uint16_t sector_size = uft_imd_ssize_to_bytes(track->header.sector_size);
    if (size < sector_size) return -1;
    
    if (!track->data) return -1;
    
    size_t offset = track->sector_offsets[sector_idx];
    memcpy(buffer, track->data + offset, sector_size);
    
    return sector_size;
}

/*============================================================================
 * Information Display
 *============================================================================*/

void uft_imd_print_info(const uft_imd_image_t* img, bool verbose)
{
    if (!img) return;
    
    printf("IMD Image Information:\n");
    printf("  Version: %d.%02d\n", img->header.version_major, img->header.version_minor);
    printf("  Date: %02d/%02d/%04d %02d:%02d:%02d\n",
           img->header.day, img->header.month, img->header.year,
           img->header.hour, img->header.minute, img->header.second);
    
    if (img->comment && img->comment_len > 0) {
        printf("  Comment: %.*s\n", (int)img->comment_len, img->comment);
    }
    
    printf("  Geometry: %d cylinders, %d heads, %d tracks\n",
           img->num_cylinders, img->num_heads, img->num_tracks);
    
    printf("  Statistics:\n");
    printf("    Total sectors:      %u\n", img->total_sectors);
    printf("    Compressed sectors: %u\n", img->compressed_sectors);
    printf("    Deleted sectors:    %u\n", img->deleted_sectors);
    printf("    Bad sectors:        %u\n", img->bad_sectors);
    printf("    Unavailable:        %u\n", img->unavail_sectors);
    
    if (verbose && img->tracks) {
        printf("\n  Track Details:\n");
        for (uint16_t i = 0; i < img->num_tracks; i++) {
            const uft_imd_track_t* t = &img->tracks[i];
            printf("    C%02d/H%d: Mode=%s, %d sectors, %d bytes/sector\n",
                   t->header.cylinder,
                   t->header.head & UFT_IMD_HEAD_MASK,
                   uft_imd_mode_name(t->header.mode),
                   t->header.nsectors,
                   uft_imd_ssize_to_bytes(t->header.sector_size));
        }
    }
}

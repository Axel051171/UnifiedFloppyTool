/**
 * @file uft_apple_gcr.c
 * @brief Apple II/Mac GCR Track Implementation
 * 
 * EXT3-006: Apple GCR encoding/decoding and track handling
 * 
 * Features:
 * - Apple II 5.25" (13/16 sector)
 * - Apple II 3.5" (Macintosh)
 * - GCR 6:2 encoding (Apple II)
 * - Variable speed zones (3.5")
 * - DSK/DO/PO/2IMG/WOZ format support
 */

#include "uft/formats/uft_apple_gcr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * GCR Tables - Apple II 6:2 Encoding
 *===========================================================================*/

/* 6:2 encoding: valid disk bytes (64 values) */
static const uint8_t APPLE_GCR_ENCODE[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* 6:2 decoding: disk byte -> 6-bit value (0xFF = invalid) */
static const uint8_t APPLE_GCR_DECODE[256] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x04, 0x05, 0x06,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x08,
    0xFF, 0xFF, 0xFF, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
    0xFF, 0xFF, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
    0xFF, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
    0xFF, 0xFF, 0xFF, 0x1B, 0xFF, 0x1C, 0x1D, 0x1E,
    0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0x20, 0x21,
    0xFF, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x29, 0x2A, 0x2B,
    0xFF, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
    0xFF, 0xFF, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0xFF, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

/* Address field markers */
#define APPLE_ADDR_PROLOG1  0xD5
#define APPLE_ADDR_PROLOG2  0xAA
#define APPLE_ADDR_PROLOG3  0x96
#define APPLE_DATA_PROLOG3  0xAD
#define APPLE_EPILOG1       0xDE
#define APPLE_EPILOG2       0xAA

/* 3.5" zone definitions */
static const struct {
    uint8_t start_track;
    uint8_t end_track;
    uint8_t sectors;
    uint32_t bit_rate;
} APPLE35_ZONES[] = {
    {  0, 15, 12, 394000 },   /* Zone 0 */
    { 16, 31, 11, 362000 },   /* Zone 1 */
    { 32, 47, 10, 330000 },   /* Zone 2 */
    { 48, 63,  9, 298000 },   /* Zone 3 */
    { 64, 79,  8, 266000 },   /* Zone 4 */
};

#define NUM_ZONES_35 5
#define APPLE_SECTOR_SIZE 256  /* 5.25" */
#define APPLE35_SECTOR_SIZE 512  /* 3.5" */

/*===========================================================================
 * Zone Helpers
 *===========================================================================*/

int uft_apple_get_zone_35(uint8_t track)
{
    for (int z = 0; z < NUM_ZONES_35; z++) {
        if (track >= APPLE35_ZONES[z].start_track && 
            track <= APPLE35_ZONES[z].end_track) {
            return z;
        }
    }
    return -1;
}

int uft_apple_sectors_per_track_35(uint8_t track)
{
    int zone = uft_apple_get_zone_35(track);
    if (zone < 0) return -1;
    return APPLE35_ZONES[zone].sectors;
}

int uft_apple_sectors_per_track_525(uint8_t track, bool dos33)
{
    (void)track;  /* All tracks same for 5.25" */
    return dos33 ? 16 : 13;  /* DOS 3.3 vs DOS 3.2 */
}

/*===========================================================================
 * 4-and-4 Encoding (Address Fields)
 *===========================================================================*/

static uint8_t decode_44(uint8_t odd, uint8_t even)
{
    return ((odd & 0x55) << 1) | (even & 0x55);
}

static void encode_44(uint8_t value, uint8_t *odd, uint8_t *even)
{
    *odd = ((value >> 1) & 0x55) | 0xAA;
    *even = (value & 0x55) | 0xAA;
}

/*===========================================================================
 * GCR 6:2 Encoding/Decoding
 *===========================================================================*/

int uft_apple_gcr_decode_sector(const uint8_t *gcr, size_t gcr_size,
                                uint8_t *data, size_t *data_size)
{
    if (!gcr || !data || !data_size) return -1;
    if (*data_size < 256 || gcr_size < 343) return -1;
    
    /* Decode 342 GCR bytes to 256 data bytes */
    uint8_t buffer[342];
    int errors = 0;
    
    /* Decode each GCR byte */
    for (int i = 0; i < 342; i++) {
        uint8_t decoded = APPLE_GCR_DECODE[gcr[i]];
        if (decoded == 0xFF) {
            errors++;
            decoded = 0;
        }
        buffer[i] = decoded;
    }
    
    /* XOR checksum stored in last byte */
    uint8_t checksum = 0;
    for (int i = 0; i < 342; i++) {
        checksum ^= buffer[i];
    }
    if (checksum != 0) errors++;
    
    /* "Prenibblize" reverse - combine 6-bit values into bytes */
    /* First 86 bytes contain 2-bit values for bottom of each byte */
    /* Next 256 bytes contain 6-bit values for top of each byte */
    
    for (int i = 0; i < 256; i++) {
        uint8_t val = buffer[86 + i] << 2;
        
        /* Add bottom 2 bits from first 86 bytes */
        int aux_idx = i % 86;
        int shift = (i / 86) * 2;
        val |= (buffer[aux_idx] >> shift) & 0x03;
        
        data[i] = val;
    }
    
    *data_size = 256;
    return errors;
}

int uft_apple_gcr_encode_sector(const uint8_t *data, size_t data_size,
                                uint8_t *gcr, size_t *gcr_size)
{
    if (!data || !gcr || !gcr_size) return -1;
    if (data_size != 256 || *gcr_size < 343) return -1;
    
    uint8_t buffer[343];
    memset(buffer, 0, sizeof(buffer));
    for (int i = 0; i < 256; i++) {
        /* Upper 6 bits */
        buffer[86 + i] = data[i] >> 2;
        
        /* Lower 2 bits into auxiliary area */
        int aux_idx = i % 86;
        int shift = (i / 86) * 2;
        buffer[aux_idx] |= (data[i] & 0x03) << shift;
    }
    
    /* XOR encode */
    uint8_t prev = 0;
    for (int i = 0; i < 342; i++) {
        uint8_t temp = buffer[i];
        buffer[i] ^= prev;
        prev = temp;
    }
    
    /* Add checksum */
    buffer[342] = prev;  /* Would need 343 bytes */
    
    /* Encode to GCR */
    for (int i = 0; i < 342; i++) {
        gcr[i] = APPLE_GCR_ENCODE[buffer[i] & 0x3F];
    }
    gcr[342] = APPLE_GCR_ENCODE[prev & 0x3F];
    
    *gcr_size = 343;
    return 0;
}

/*===========================================================================
 * Address Field Parsing
 *===========================================================================*/

int uft_apple_parse_address(const uint8_t *addr, uft_apple_addr_t *info)
{
    if (!addr || !info) return -1;
    
    /* Check prolog */
    if (addr[0] != APPLE_ADDR_PROLOG1 || 
        addr[1] != APPLE_ADDR_PROLOG2 ||
        addr[2] != APPLE_ADDR_PROLOG3) {
        return -1;
    }
    
    /* Decode 4-and-4 fields */
    info->volume = decode_44(addr[3], addr[4]);
    info->track = decode_44(addr[5], addr[6]);
    info->sector = decode_44(addr[7], addr[8]);
    info->checksum = decode_44(addr[9], addr[10]);
    
    /* Verify checksum */
    uint8_t calc = info->volume ^ info->track ^ info->sector;
    info->valid = (calc == info->checksum);
    
    /* Check epilog */
    if (addr[11] != APPLE_EPILOG1 || addr[12] != APPLE_EPILOG2) {
        info->valid = false;
    }
    
    return info->valid ? 0 : -1;
}

/*===========================================================================
 * DSK Format Operations
 *===========================================================================*/

int uft_apple_dsk_open(uft_apple_ctx_t *ctx, const uint8_t *data, size_t size)
{
    if (!ctx || !data) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->data = data;
    ctx->size = size;
    
    /* Detect format by size */
    switch (size) {
        case 143360:  /* 35 tracks * 16 sectors * 256 bytes */
            ctx->format = UFT_APPLE_DSK_DO;  /* DOS order */
            ctx->tracks = 35;
            ctx->sectors_per_track = 16;
            ctx->sector_size = 256;
            break;
        case 116480:  /* 35 tracks * 13 sectors * 256 bytes */
            ctx->format = UFT_APPLE_DSK_D13;
            ctx->tracks = 35;
            ctx->sectors_per_track = 13;
            ctx->sector_size = 256;
            break;
        case 819200:  /* 80 tracks * 2 sides * variable sectors * 512 bytes */
            ctx->format = UFT_APPLE_DSK_35;
            ctx->tracks = 80;
            ctx->sides = 2;
            ctx->sector_size = 512;
            break;
        default:
            /* Check for 2IMG header */
            if (size > 64 && memcmp(data, "2IMG", 4) == 0) {
                ctx->format = UFT_APPLE_2IMG;
                /* Parse 2IMG header */
                ctx->header_size = data[8] | (data[9] << 8);
                ctx->tracks = 35;
                ctx->sectors_per_track = 16;
                ctx->sector_size = 256;
            } else {
                return -1;
            }
    }
    
    /* Calculate total sectors */
    if (ctx->format == UFT_APPLE_DSK_35) {
        ctx->total_sectors = 0;
        for (int t = 0; t < 80; t++) {
            ctx->total_sectors += uft_apple_sectors_per_track_35(t) * ctx->sides;
        }
    } else {
        ctx->total_sectors = ctx->tracks * ctx->sectors_per_track;
    }
    
    ctx->is_valid = true;
    return 0;
}

void uft_apple_dsk_close(uft_apple_ctx_t *ctx)
{
    if (ctx) {
        memset(ctx, 0, sizeof(*ctx));
    }
}

/* DOS 3.3 sector interleaving */
static const uint8_t DOS_TO_PHYS[16] = {
    0x00, 0x07, 0x0E, 0x06, 0x0D, 0x05, 0x0C, 0x04,
    0x0B, 0x03, 0x0A, 0x02, 0x09, 0x01, 0x08, 0x0F
};

static const uint8_t PRODOS_TO_PHYS[16] = {
    0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E,
    0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F
};

int uft_apple_dsk_read_sector(const uft_apple_ctx_t *ctx, 
                              uint8_t track, uint8_t sector,
                              uint8_t *buffer, size_t *size)
{
    if (!ctx || !buffer || !size || !ctx->is_valid) return -1;
    if (track >= ctx->tracks) return -1;
    if (sector >= ctx->sectors_per_track) return -1;
    
    /* Apply sector interleaving if needed */
    uint8_t phys_sector = sector;
    if (ctx->format == UFT_APPLE_DSK_DO) {
        phys_sector = DOS_TO_PHYS[sector];
    } else if (ctx->format == UFT_APPLE_DSK_PO) {
        phys_sector = PRODOS_TO_PHYS[sector];
    }
    
    size_t offset = ctx->header_size;
    offset += (track * ctx->sectors_per_track + phys_sector) * ctx->sector_size;
    
    if (offset + ctx->sector_size > ctx->size) {
        return -1;
    }
    
    if (*size < ctx->sector_size) {
        *size = ctx->sector_size;
        return -1;
    }
    
    memcpy(buffer, ctx->data + offset, ctx->sector_size);
    *size = ctx->sector_size;
    
    return 0;
}

/*===========================================================================
 * Catalog Reading (DOS 3.3)
 *===========================================================================*/

int uft_apple_read_catalog(const uft_apple_ctx_t *ctx,
                           uft_apple_dirent_t *entries, size_t max_entries,
                           size_t *count)
{
    if (!ctx || !entries || !count || !ctx->is_valid) return -1;
    
    *count = 0;
    
    /* VTOC at track 17, sector 0 */
    uint8_t vtoc[256];
    size_t size = 256;
    
    if (uft_apple_dsk_read_sector(ctx, 17, 0, vtoc, &size) != 0) {
        return -1;
    }
    
    /* Catalog starts at track/sector in VTOC */
    uint8_t cat_track = vtoc[1];
    uint8_t cat_sector = vtoc[2];
    
    while (cat_track != 0 && *count < max_entries) {
        uint8_t cat[256];
        
        if (uft_apple_dsk_read_sector(ctx, cat_track, cat_sector, cat, &size) != 0) {
            break;
        }
        
        /* 7 entries per sector, starting at offset 0x0B */
        for (int e = 0; e < 7 && *count < max_entries; e++) {
            const uint8_t *entry = cat + 0x0B + e * 35;
            
            uint8_t first_track = entry[0];
            if (first_track == 0x00 || first_track == 0xFF) continue;
            
            uft_apple_dirent_t *de = &entries[*count];
            memset(de, 0, sizeof(*de));
            
            de->first_track = first_track;
            de->first_sector = entry[1];
            de->file_type = entry[2];
            de->locked = (de->file_type & 0x80) != 0;
            de->file_type &= 0x7F;
            
            /* Filename (30 bytes, high bit set) */
            for (int i = 0; i < 30; i++) {
                uint8_t c = entry[3 + i] & 0x7F;
                de->filename[i] = (c >= 0x20) ? c : ' ';
            }
            de->filename[30] = '\0';
            
            /* Trim trailing spaces */
            for (int i = 29; i >= 0 && de->filename[i] == ' '; i--) {
                de->filename[i] = '\0';
            }
            
            de->length = entry[33] | (entry[34] << 8);
            
            (*count)++;
        }
        
        cat_track = cat[1];
        cat_sector = cat[2];
    }
    
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

const char *uft_apple_format_name(uft_apple_format_t format)
{
    switch (format) {
        case UFT_APPLE_DSK_DO:  return "DSK (DOS Order)";
        case UFT_APPLE_DSK_PO:  return "DSK (ProDOS Order)";
        case UFT_APPLE_DSK_D13: return "D13 (13-sector)";
        case UFT_APPLE_DSK_35:  return "3.5\" (800K)";
        case UFT_APPLE_2IMG:    return "2IMG";
        case UFT_APPLE_WOZ:     return "WOZ";
        case UFT_APPLE_NIB:     return "NIB";
        default:               return "Unknown";
    }
}

const char *uft_apple_file_type_name(uint8_t type)
{
    switch (type & 0x7F) {
        case 0x00: return "TXT";
        case 0x01: return "INT";
        case 0x02: return "APP";
        case 0x04: return "BIN";
        case 0x08: return "S";
        case 0x10: return "REL";
        case 0x20: return "A";
        case 0x40: return "B";
        default:   return "???";
    }
}

int uft_apple_report_json(const uft_apple_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"format\": \"%s\",\n"
        "  \"valid\": %s,\n"
        "  \"tracks\": %u,\n"
        "  \"sectors_per_track\": %u,\n"
        "  \"sector_size\": %u,\n"
        "  \"total_sectors\": %u,\n"
        "  \"sides\": %u,\n"
        "  \"file_size\": %zu\n"
        "}",
        uft_apple_format_name(ctx->format),
        ctx->is_valid ? "true" : "false",
        ctx->tracks,
        ctx->sectors_per_track,
        ctx->sector_size,
        ctx->total_sectors,
        ctx->sides ? ctx->sides : 1,
        ctx->size
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}

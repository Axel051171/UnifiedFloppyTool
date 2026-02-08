/**
 * @file uft_atari_st.c
 * @brief Atari ST Disk Format Implementation
 * 
 * EXT3-009: Atari ST disk format support
 * 
 * Features:
 * - ST/MSA format support
 * - TOS filesystem (FAT12 variant)
 * - Boot sector parsing
 * - Copy protection detection
 */

#include "uft/formats/uft_atari_st.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* Standard disk sizes */
#define ST_SS_DD    (80 * 9 * 512)       /* 360K: SS/DD */
#define ST_DS_DD    (80 * 2 * 9 * 512)   /* 720K: DS/DD */
#define ST_DS_HD    (80 * 2 * 18 * 512)  /* 1.44M: DS/HD */
#define ST_DS_ED    (80 * 2 * 36 * 512)  /* 2.88M: DS/ED */

/* MSA header */
#define MSA_MAGIC   0x0E0F

/* Boot sector offsets */
#define BPB_BYTES_PER_SECTOR    11
#define BPB_SECTORS_PER_CLUSTER 13
#define BPB_RESERVED_SECTORS    14
#define BPB_NUM_FATS            16
#define BPB_ROOT_ENTRIES        17
#define BPB_TOTAL_SECTORS       19
#define BPB_MEDIA_DESCRIPTOR    21
#define BPB_SECTORS_PER_FAT     22
#define BPB_SECTORS_PER_TRACK   24
#define BPB_NUM_HEADS           26
#define BPB_HIDDEN_SECTORS      28
#define BPB_SERIAL              8

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

static uint16_t read_be16(const uint8_t *p)
{
    return (p[0] << 8) | p[1];
}

/*===========================================================================
 * Boot Sector Checksum
 *===========================================================================*/

static uint16_t boot_checksum(const uint8_t *boot)
{
    uint16_t sum = 0;
    for (int i = 0; i < 256; i++) {
        sum += read_be16(boot + i * 2);
    }
    return sum;
}

/*===========================================================================
 * MSA Decompression
 *===========================================================================*/

static int msa_decompress_track(const uint8_t *src, size_t src_size,
                                uint8_t *dst, size_t dst_size)
{
    size_t src_pos = 0;
    size_t dst_pos = 0;
    
    while (src_pos < src_size && dst_pos < dst_size) {
        uint8_t byte = src[src_pos++];
        
        if (byte == 0xE5 && src_pos + 3 <= src_size) {
            /* RLE: 0xE5 <byte> <count_hi> <count_lo> */
            uint8_t fill = src[src_pos++];
            uint16_t count = (src[src_pos] << 8) | src[src_pos + 1];
            src_pos += 2;
            
            for (uint16_t i = 0; i < count && dst_pos < dst_size; i++) {
                dst[dst_pos++] = fill;
            }
        } else {
            dst[dst_pos++] = byte;
        }
    }
    
    return dst_pos;
}

int uft_st_msa_decompress(const uint8_t *msa, size_t msa_size,
                          uint8_t *st, size_t *st_size)
{
    if (!msa || !st || !st_size) return -1;
    if (msa_size < 10) return -1;
    
    /* Check MSA header */
    uint16_t magic = read_be16(msa);
    if (magic != MSA_MAGIC) return -1;
    
    uint16_t sectors_per_track = read_be16(msa + 2);
    uint16_t sides = read_be16(msa + 4) + 1;
    uint16_t start_track = read_be16(msa + 6);
    uint16_t end_track = read_be16(msa + 8);
    
    size_t track_size = sectors_per_track * 512;
    size_t needed = (end_track - start_track + 1) * sides * track_size;
    
    if (*st_size < needed) {
        *st_size = needed;
        return -1;
    }
    
    size_t src_pos = 10;
    size_t dst_pos = 0;
    
    for (int t = start_track; t <= end_track; t++) {
        for (int s = 0; s < sides; s++) {
            if (src_pos + 2 > msa_size) return -1;
            
            uint16_t packed_len = read_be16(msa + src_pos);
            src_pos += 2;
            
            if (src_pos + packed_len > msa_size) return -1;
            
            if (packed_len == track_size) {
                /* Uncompressed track */
                memcpy(st + dst_pos, msa + src_pos, track_size);
            } else {
                /* Compressed track */
                msa_decompress_track(msa + src_pos, packed_len,
                                    st + dst_pos, track_size);
            }
            
            src_pos += packed_len;
            dst_pos += track_size;
        }
    }
    
    *st_size = dst_pos;
    return 0;
}

/*===========================================================================
 * Format Detection
 *===========================================================================*/

uft_st_format_t uft_st_detect_format(const uint8_t *data, size_t size)
{
    if (!data || size < 512) {
        return UFT_ST_FORMAT_UNKNOWN;
    }
    
    /* Check for MSA header */
    if (read_be16(data) == MSA_MAGIC) {
        return UFT_ST_FORMAT_MSA;
    }
    
    /* Check for raw ST image by size */
    if (size == ST_SS_DD || size == ST_DS_DD || 
        size == ST_DS_HD || size == ST_DS_ED) {
        return UFT_ST_FORMAT_ST;
    }
    
    /* Check boot sector */
    const uint8_t *boot = data;
    
    /* Check for x86 jump or NOP */
    if (boot[0] == 0xEB || boot[0] == 0xE9 || boot[0] == 0x00) {
        uint16_t bps = read_le16(boot + BPB_BYTES_PER_SECTOR);
        if (bps == 512 || bps == 1024) {
            return UFT_ST_FORMAT_ST;
        }
    }
    
    /* Check for bootable TOS disk */
    if (boot_checksum(data) == 0x1234) {
        return UFT_ST_FORMAT_ST;
    }
    
    return UFT_ST_FORMAT_UNKNOWN;
}

/*===========================================================================
 * ST File Open/Close
 *===========================================================================*/

int uft_st_open(uft_st_ctx_t *ctx, const uint8_t *data, size_t size)
{
    if (!ctx || !data) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->format = uft_st_detect_format(data, size);
    
    if (ctx->format == UFT_ST_FORMAT_UNKNOWN) {
        return -1;
    }
    
    if (ctx->format == UFT_ST_FORMAT_MSA) {
        /* Decompress MSA */
        ctx->decompressed = malloc(ST_DS_HD);  /* Max size */
        if (!ctx->decompressed) return -1;
        
        ctx->size = ST_DS_HD;
        if (uft_st_msa_decompress(data, size, ctx->decompressed, &ctx->size) != 0) {
            free(ctx->decompressed);
            return -1;
        }
        
        ctx->data = ctx->decompressed;
        ctx->owns_data = true;
    } else {
        ctx->data = data;
        ctx->size = size;
        ctx->owns_data = false;
    }
    
    /* Parse boot sector / BPB */
    const uint8_t *boot = ctx->data;
    
    ctx->bytes_per_sector = read_le16(boot + BPB_BYTES_PER_SECTOR);
    if (ctx->bytes_per_sector == 0) ctx->bytes_per_sector = 512;
    
    ctx->sectors_per_cluster = boot[BPB_SECTORS_PER_CLUSTER];
    if (ctx->sectors_per_cluster == 0) ctx->sectors_per_cluster = 2;
    
    ctx->reserved_sectors = read_le16(boot + BPB_RESERVED_SECTORS);
    if (ctx->reserved_sectors == 0) ctx->reserved_sectors = 1;
    
    ctx->num_fats = boot[BPB_NUM_FATS];
    if (ctx->num_fats == 0) ctx->num_fats = 2;
    
    ctx->root_entries = read_le16(boot + BPB_ROOT_ENTRIES);
    if (ctx->root_entries == 0) ctx->root_entries = 112;
    
    ctx->total_sectors = read_le16(boot + BPB_TOTAL_SECTORS);
    ctx->sectors_per_fat = read_le16(boot + BPB_SECTORS_PER_FAT);
    ctx->sectors_per_track = read_le16(boot + BPB_SECTORS_PER_TRACK);
    ctx->heads = read_le16(boot + BPB_NUM_HEADS);
    
    /* Detect geometry if not in BPB */
    if (ctx->sectors_per_track == 0) {
        if (ctx->size == ST_SS_DD) {
            ctx->sectors_per_track = 9;
            ctx->heads = 1;
            ctx->tracks = 80;
        } else if (ctx->size == ST_DS_DD) {
            ctx->sectors_per_track = 9;
            ctx->heads = 2;
            ctx->tracks = 80;
        } else if (ctx->size == ST_DS_HD) {
            ctx->sectors_per_track = 18;
            ctx->heads = 2;
            ctx->tracks = 80;
        }
    } else {
        ctx->tracks = ctx->total_sectors / (ctx->sectors_per_track * ctx->heads);
    }
    
    /* Calculate filesystem layout */
    ctx->fat_start = ctx->reserved_sectors;
    ctx->root_start = ctx->fat_start + ctx->num_fats * ctx->sectors_per_fat;
    ctx->root_sectors = (ctx->root_entries * 32 + ctx->bytes_per_sector - 1) / 
                        ctx->bytes_per_sector;
    ctx->data_start = ctx->root_start + ctx->root_sectors;
    
    /* Check if bootable */
    ctx->is_bootable = (boot_checksum(boot) == 0x1234);
    
    ctx->is_valid = true;
    return 0;
}

void uft_st_close(uft_st_ctx_t *ctx)
{
    if (ctx) {
        if (ctx->owns_data && ctx->decompressed) {
            free(ctx->decompressed);
        }
        memset(ctx, 0, sizeof(*ctx));
    }
}

/*===========================================================================
 * Sector Access
 *===========================================================================*/

int uft_st_read_sector(const uft_st_ctx_t *ctx, uint32_t lba,
                       uint8_t *buffer, size_t *size)
{
    if (!ctx || !buffer || !size || !ctx->is_valid) return -1;
    
    size_t offset = lba * ctx->bytes_per_sector;
    if (offset + ctx->bytes_per_sector > ctx->size) return -1;
    
    if (*size < ctx->bytes_per_sector) {
        *size = ctx->bytes_per_sector;
        return -1;
    }
    
    memcpy(buffer, ctx->data + offset, ctx->bytes_per_sector);
    *size = ctx->bytes_per_sector;
    
    return 0;
}

int uft_st_read_chs(const uft_st_ctx_t *ctx, uint8_t track, uint8_t head,
                    uint8_t sector, uint8_t *buffer, size_t *size)
{
    if (!ctx || !ctx->is_valid) return -1;
    
    uint32_t lba = (track * ctx->heads + head) * ctx->sectors_per_track + (sector - 1);
    return uft_st_read_sector(ctx, lba, buffer, size);
}

/*===========================================================================
 * FAT Operations
 *===========================================================================*/

static uint16_t get_fat_entry(const uft_st_ctx_t *ctx, uint16_t cluster)
{
    /* FAT12 */
    size_t fat_offset = ctx->fat_start * ctx->bytes_per_sector;
    size_t entry_offset = cluster + (cluster / 2);
    
    if (fat_offset + entry_offset + 2 > ctx->size) return 0xFFF;
    
    uint16_t entry = read_le16(ctx->data + fat_offset + entry_offset);
    
    if (cluster & 1) {
        return entry >> 4;
    } else {
        return entry & 0x0FFF;
    }
}

/*===========================================================================
 * Directory Operations
 *===========================================================================*/

int uft_st_read_directory(const uft_st_ctx_t *ctx, uint32_t cluster,
                          uft_st_dirent_t *entries, size_t max_entries,
                          size_t *count)
{
    if (!ctx || !entries || !count || !ctx->is_valid) return -1;
    
    *count = 0;
    
    const uint8_t *dir_data;
    size_t dir_size;
    
    if (cluster == 0) {
        /* Root directory */
        dir_data = ctx->data + ctx->root_start * ctx->bytes_per_sector;
        dir_size = ctx->root_entries * 32;
    } else {
        /* Subdirectory - follow cluster chain */
        /* For simplicity, just read first cluster */
        size_t cluster_size = ctx->sectors_per_cluster * ctx->bytes_per_sector;
        size_t offset = ctx->data_start * ctx->bytes_per_sector + 
                       (cluster - 2) * cluster_size;
        
        if (offset + cluster_size > ctx->size) return -1;
        
        dir_data = ctx->data + offset;
        dir_size = cluster_size;
    }
    
    /* Parse directory entries */
    for (size_t i = 0; i < dir_size && *count < max_entries; i += 32) {
        const uint8_t *entry = dir_data + i;
        
        if (entry[0] == 0x00) break;     /* End of directory */
        if (entry[0] == 0xE5) continue;  /* Deleted entry */
        if (entry[0] == 0x2E) continue;  /* . or .. */
        
        uft_st_dirent_t *de = &entries[*count];
        memset(de, 0, sizeof(*de));
        
        /* Filename (8.3 format) */
        for (int j = 0; j < 8; j++) {
            de->filename[j] = entry[j];
        }
        de->filename[8] = '\0';
        
        /* Extension */
        for (int j = 0; j < 3; j++) {
            de->extension[j] = entry[8 + j];
        }
        de->extension[3] = '\0';
        
        /* Trim spaces */
        for (int j = 7; j >= 0 && de->filename[j] == ' '; j--) {
            de->filename[j] = '\0';
        }
        for (int j = 2; j >= 0 && de->extension[j] == ' '; j--) {
            de->extension[j] = '\0';
        }
        
        de->attributes = entry[11];
        de->is_dir = (de->attributes & 0x10) != 0;
        de->is_hidden = (de->attributes & 0x02) != 0;
        de->is_system = (de->attributes & 0x04) != 0;
        de->is_readonly = (de->attributes & 0x01) != 0;
        
        de->start_cluster = read_le16(entry + 26);
        de->size = read_le32(entry + 28);
        
        /* Time/Date */
        uint16_t time = read_le16(entry + 22);
        uint16_t date = read_le16(entry + 24);
        
        de->time = time;
        de->date = date;
        
        (*count)++;
    }
    
    return 0;
}

/*===========================================================================
 * File Reading
 *===========================================================================*/

int uft_st_read_file(const uft_st_ctx_t *ctx, const uft_st_dirent_t *entry,
                     uint8_t *buffer, size_t max_size, size_t *bytes_read)
{
    if (!ctx || !entry || !buffer || !bytes_read || !ctx->is_valid) return -1;
    if (entry->is_dir) return -1;
    
    *bytes_read = 0;
    
    size_t file_size = entry->size;
    if (file_size > max_size) file_size = max_size;
    
    size_t cluster_size = ctx->sectors_per_cluster * ctx->bytes_per_sector;
    uint16_t cluster = entry->start_cluster;
    
    while (cluster >= 2 && cluster < 0xFF0 && *bytes_read < file_size) {
        size_t offset = ctx->data_start * ctx->bytes_per_sector + 
                       (cluster - 2) * cluster_size;
        
        if (offset + cluster_size > ctx->size) break;
        
        size_t to_read = file_size - *bytes_read;
        if (to_read > cluster_size) to_read = cluster_size;
        
        memcpy(buffer + *bytes_read, ctx->data + offset, to_read);
        *bytes_read += to_read;
        
        cluster = get_fat_entry(ctx, cluster);
    }
    
    return 0;
}

/*===========================================================================
 * Copy Protection Detection
 *===========================================================================*/

int uft_st_detect_protection(const uft_st_ctx_t *ctx, uint32_t *protection_flags)
{
    if (!ctx || !protection_flags || !ctx->is_valid) return -1;
    
    *protection_flags = 0;
    
    const uint8_t *boot = ctx->data;
    
    /* Check for copylock signature */
    for (int i = 0; i < 500; i++) {
        if (memcmp(boot + i, "Copylock", 8) == 0 ||
            memcmp(boot + i, "COPYLOCK", 8) == 0) {
            *protection_flags |= UFT_ST_PROT_COPYLOCK;
            break;
        }
    }
    
    /* Check for Macrodos/Speedlock patterns */
    /* These typically use non-standard sector counts */
    if (ctx->sectors_per_track > 10) {
        *protection_flags |= UFT_ST_PROT_MACRODOS;
    }
    
    /* Check for fuzzy bits / weak sectors */
    /* Would need flux-level data for accurate detection */
    
    /* Check for specific protection loaders */
    if (boot[0] == 0x60 && boot[1] == 0x38) {
        /* Common protection loader entry point */
        *protection_flags |= UFT_ST_PROT_GENERIC;
    }
    
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

const char *uft_st_format_name(uft_st_format_t format)
{
    switch (format) {
        case UFT_ST_FORMAT_ST:  return "ST (Raw)";
        case UFT_ST_FORMAT_MSA: return "MSA (Compressed)";
        case UFT_ST_FORMAT_STX: return "STX (Pasti)";
        default:               return "Unknown";
    }
}

int uft_st_report_json(const uft_st_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"format\": \"%s\",\n"
        "  \"valid\": %s,\n"
        "  \"tracks\": %u,\n"
        "  \"heads\": %u,\n"
        "  \"sectors_per_track\": %u,\n"
        "  \"bytes_per_sector\": %u,\n"
        "  \"total_sectors\": %u,\n"
        "  \"bootable\": %s,\n"
        "  \"file_size\": %zu\n"
        "}",
        uft_st_format_name(ctx->format),
        ctx->is_valid ? "true" : "false",
        ctx->tracks,
        ctx->heads,
        ctx->sectors_per_track,
        ctx->bytes_per_sector,
        ctx->total_sectors,
        ctx->is_bootable ? "true" : "false",
        ctx->size
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}

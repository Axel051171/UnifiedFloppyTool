/**
 * @file uft_atari_st.c
 * @brief Atari ST Disk Image Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/atari/uft_atari_st.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Helpers
 * ============================================================================ */

static uint16_t read_le16(const uint8_t *p)
{
    return p[0] | (p[1] << 8);
}

static uint16_t read_be16(const uint8_t *p)
{
    return (p[0] << 8) | p[1];
}

/* ============================================================================
 * Detection
 * ============================================================================ */

st_format_t st_detect_format(const uint8_t *data, size_t size)
{
    if (!data || size < 512) return ST_FORMAT_UNKNOWN;
    
    /* Check for MSA magic */
    if (size >= 10) {
        uint16_t magic = read_be16(data);
        if (magic == MSA_MAGIC) {
            return ST_FORMAT_MSA;
        }
    }
    
    /* Check for STX magic "RSY" */
    if (size >= 4 && memcmp(data, "RSY", 3) == 0) {
        return ST_FORMAT_STX;
    }
    
    /* Check for valid ST boot sector */
    uint16_t bps = read_le16(data + 11);
    uint16_t spt = read_le16(data + 24);
    uint16_t heads = read_le16(data + 26);
    
    if (bps == 512 && spt >= 9 && spt <= 11 && heads >= 1 && heads <= 2) {
        /* Verify size matches geometry */
        uint16_t nsects = read_le16(data + 19);
        if (nsects * 512 == size || (size % (spt * 512 * heads) == 0)) {
            return ST_FORMAT_ST;
        }
    }
    
    /* Fallback: check standard sizes */
    if (size == ST_SS_SD_SIZE || size == ST_SS_DD_SIZE ||
        size == ST_DS_DD_SIZE || size == ST_DS_HD_SIZE) {
        return ST_FORMAT_ST;
    }
    
    return ST_FORMAT_UNKNOWN;
}

bool st_validate(const uint8_t *data, size_t size)
{
    return st_detect_format(data, size) != ST_FORMAT_UNKNOWN;
}

const char *st_format_name(st_format_t format)
{
    switch (format) {
        case ST_FORMAT_ST:  return "ST (Raw)";
        case ST_FORMAT_MSA: return "MSA (Magic Shadow Archiver)";
        case ST_FORMAT_STX: return "STX (Pasti)";
        default:            return "Unknown";
    }
}

const char *st_disk_type_name(st_disk_type_t type)
{
    switch (type) {
        case ST_DISK_SS_SD: return "Single-sided SD (360KB)";
        case ST_DISK_SS_DD: return "Single-sided DD (400KB)";
        case ST_DISK_DS_DD: return "Double-sided DD (720KB)";
        case ST_DISK_DS_HD: return "Double-sided HD (1.44MB)";
        default:            return "Unknown";
    }
}

/* ============================================================================
 * MSA Decompression
 * ============================================================================ */

int st_msa_decompress(const uint8_t *msa_data, size_t msa_size,
                      uint8_t **st_data, size_t *st_size)
{
    if (!msa_data || msa_size < 10 || !st_data || !st_size) return -1;
    
    /* Parse MSA header */
    uint16_t magic = read_be16(msa_data);
    if (magic != MSA_MAGIC) return -2;
    
    uint16_t spt = read_be16(msa_data + 2);
    uint16_t sides = read_be16(msa_data + 4) + 1;
    uint16_t start_track = read_be16(msa_data + 6);
    uint16_t end_track = read_be16(msa_data + 8);
    
    uint16_t tracks = end_track - start_track + 1;
    size_t track_size = spt * ST_SECTOR_SIZE;
    *st_size = tracks * sides * track_size;
    
    *st_data = malloc(*st_size);
    if (!*st_data) return -3;
    
    const uint8_t *src = msa_data + 10;
    uint8_t *dst = *st_data;
    size_t remaining = msa_size - 10;
    
    for (int track = 0; track < tracks * sides; track++) {
        if (remaining < 2) {
            free(*st_data);
            return -4;
        }
        
        uint16_t data_len = read_be16(src);
        src += 2;
        remaining -= 2;
        
        if (data_len == track_size) {
            /* Uncompressed track */
            if (remaining < track_size) {
                free(*st_data);
                return -5;
            }
            memcpy(dst, src, track_size);
            src += track_size;
            remaining -= track_size;
        } else {
            /* RLE compressed track */
            size_t written = 0;
            while (written < track_size && data_len > 0) {
                uint8_t byte = *src++;
                data_len--;
                
                if (byte == 0xE5 && data_len >= 3) {
                    uint8_t fill = *src++;
                    uint16_t count = read_be16(src);
                    src += 2;
                    data_len -= 3;
                    
                    if (written + count > track_size) count = track_size - written;
                    memset(dst + written, fill, count);
                    written += count;
                } else {
                    dst[written++] = byte;
                }
            }
            
            /* Pad if needed */
            while (written < track_size) {
                dst[written++] = 0;
            }
        }
        
        dst += track_size;
    }
    
    return 0;
}

/* ============================================================================
 * Disk Operations
 * ============================================================================ */

int st_open(const uint8_t *data, size_t size, st_disk_t *disk)
{
    if (!data || !disk || size < 512) return -1;
    
    memset(disk, 0, sizeof(st_disk_t));
    
    disk->format = st_detect_format(data, size);
    if (disk->format == ST_FORMAT_UNKNOWN) return -2;
    
    if (disk->format == ST_FORMAT_MSA) {
        /* Decompress MSA */
        int ret = st_msa_decompress(data, size, &disk->data, &disk->size);
        if (ret != 0) return ret;
        disk->owns_data = true;
    } else if (disk->format == ST_FORMAT_STX) {
        /* STX not fully supported yet */
        return -3;
    } else {
        /* Raw ST */
        disk->data = malloc(size);
        if (!disk->data) return -4;
        memcpy(disk->data, data, size);
        disk->size = size;
        disk->owns_data = true;
    }
    
    /* Parse boot sector */
    disk->boot.bps = read_le16(disk->data + 11);
    disk->boot.spc = disk->data[13];
    disk->boot.res = read_le16(disk->data + 14);
    disk->boot.nfats = disk->data[16];
    disk->boot.ndirs = read_le16(disk->data + 17);
    disk->boot.nsects = read_le16(disk->data + 19);
    disk->boot.media = disk->data[21];
    disk->boot.spf = read_le16(disk->data + 22);
    disk->boot.spt = read_le16(disk->data + 24);
    disk->boot.nheads = read_le16(disk->data + 26);
    
    return 0;
}

int st_load(const char *filename, st_disk_t *disk)
{
    if (!filename || !disk) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -2;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -3;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -4;
    }
    fclose(fp);
    
    int result = st_open(data, size, disk);
    free(data);
    
    return result;
}

void st_close(st_disk_t *disk)
{
    if (!disk) return;
    
    if (disk->owns_data) {
        free(disk->data);
    }
    
    memset(disk, 0, sizeof(st_disk_t));
}

int st_get_info(const st_disk_t *disk, st_info_t *info)
{
    if (!disk || !info) return -1;
    
    memset(info, 0, sizeof(st_info_t));
    
    info->format = disk->format;
    info->format_name = st_format_name(disk->format);
    info->disk_size = disk->size;
    info->sector_size = disk->boot.bps ? disk->boot.bps : ST_SECTOR_SIZE;
    info->sectors_per_track = disk->boot.spt ? disk->boot.spt : 9;
    info->sides = disk->boot.nheads ? disk->boot.nheads : 2;
    
    /* Calculate tracks */
    if (info->sectors_per_track > 0 && info->sides > 0) {
        info->tracks = disk->size / (info->sectors_per_track * info->sector_size * info->sides);
    }
    
    /* Determine disk type */
    if (disk->size <= ST_SS_SD_SIZE) {
        info->disk_type = ST_DISK_SS_SD;
    } else if (disk->size <= ST_SS_DD_SIZE) {
        info->disk_type = ST_DISK_SS_DD;
    } else if (disk->size <= ST_DS_DD_SIZE) {
        info->disk_type = ST_DISK_DS_DD;
    } else {
        info->disk_type = ST_DISK_DS_HD;
    }
    info->disk_name = st_disk_type_name(info->disk_type);
    
    info->has_boot_sector = (disk->boot.bps == 512);
    info->is_bootable = (disk->data[0] == 0x60 || disk->data[0] == 0xE9);
    
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void st_print_info(const st_disk_t *disk, FILE *fp)
{
    if (!disk || !fp) return;
    
    st_info_t info;
    st_get_info(disk, &info);
    
    fprintf(fp, "Atari ST Disk:\n");
    fprintf(fp, "  Format: %s\n", info.format_name);
    fprintf(fp, "  Type: %s\n", info.disk_name);
    fprintf(fp, "  Size: %zu bytes (%.0f KB)\n", info.disk_size, info.disk_size / 1024.0);
    fprintf(fp, "  Geometry: %d tracks, %d sectors/track, %d sides\n",
            info.tracks, info.sectors_per_track, info.sides);
    fprintf(fp, "  Sector Size: %d bytes\n", info.sector_size);
    fprintf(fp, "  Bootable: %s\n", info.is_bootable ? "Yes" : "No");
}

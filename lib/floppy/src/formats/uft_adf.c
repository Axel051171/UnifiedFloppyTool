#define _POSIX_C_SOURCE 200809L
/**
 * @file uft_adf.c
 * @brief Amiga Disk File (ADF) support
 * 
 * Implementation of ADF format for Amiga DD (880K) and HD (1.76M) disks.
 */

#include "formats/uft_diskimage.h"
#include <sys/types.h>
#include "encoding/uft_mfm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * ADF Layout Constants
 * ============================================================================ */

#define ADF_SECTOR_SIZE     512
#define ADF_TRACK_GAP       0

/** DD disk: 80 tracks, 2 heads, 11 sectors */
#define ADF_DD_TRACKS       80
#define ADF_DD_HEADS        2
#define ADF_DD_SECTORS      11
#define ADF_DD_TOTAL        (ADF_DD_TRACKS * ADF_DD_HEADS * ADF_DD_SECTORS)

/** HD disk: 80 tracks, 2 heads, 22 sectors */
#define ADF_HD_TRACKS       80
#define ADF_HD_HEADS        2
#define ADF_HD_SECTORS      22
#define ADF_HD_TOTAL        (ADF_HD_TRACKS * ADF_HD_HEADS * ADF_HD_SECTORS)

/** AmigaDOS bootblock signature */
#define ADF_BOOTBLOCK_DOS   0x444F5300  /* "DOS\0" */
#define ADF_BOOTBLOCK_FFS   0x444F5301  /* "DOS\1" FFS */
#define ADF_BOOTBLOCK_INTL  0x444F5302  /* "DOS\2" International */
#define ADF_BOOTBLOCK_DCACHE 0x444F5303 /* "DOS\3" Dir cache */

/* ============================================================================
 * ADF Handle
 * ============================================================================ */

typedef struct {
    FILE *fp;
    bool readonly;
    bool is_hd;
    
    uint8_t tracks;
    uint8_t heads;
    uint8_t sectors;
    uint32_t total_sectors;
    
    uint8_t *data;
    size_t data_size;
    bool modified;
} adf_handle_t;

/* ============================================================================
 * ADF Functions
 * ============================================================================ */

uint32_t uft_adf_sector_offset(uint8_t track, uint8_t head, uint8_t sector, bool is_hd) {
    uint8_t sectors_per_track = is_hd ? ADF_HD_SECTORS : ADF_DD_SECTORS;
    
    if (track >= ADF_DD_TRACKS || head >= ADF_DD_HEADS || sector >= sectors_per_track) {
        return 0;
    }
    
    /* Amiga track layout: track 0 side 0, track 0 side 1, track 1 side 0, ... */
    uint32_t linear_sector = (track * ADF_DD_HEADS + head) * sectors_per_track + sector;
    return linear_sector * ADF_SECTOR_SIZE;
}

/**
 * @brief Validate ADF file size
 */
static bool adf_validate_size(size_t size, bool *is_hd) {
    if (size == UFT_ADF_SIZE_DD) {
        *is_hd = false;
        return true;
    }
    if (size == UFT_ADF_SIZE_HD) {
        *is_hd = true;
        return true;
    }
    return false;
}

/**
 * @brief Open ADF image
 */
int uft_adf_open(const char *path, bool readonly, adf_handle_t **handle) {
    if (!path || !handle) return -1;
    
    FILE *fp = fopen(path, readonly ? "rb" : "r+b");
    if (!fp) return -1;
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    size_t size = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* Validate size */
    bool is_hd;
    if (!adf_validate_size(size, &is_hd)) {
        fclose(fp);
        return -2;
    }
    
    /* Allocate handle */
    adf_handle_t *h = calloc(1, sizeof(adf_handle_t));
    if (!h) {
        fclose(fp);
        return -3;
    }
    
    h->fp = fp;
    h->readonly = readonly;
    h->is_hd = is_hd;
    h->tracks = ADF_DD_TRACKS;
    h->heads = ADF_DD_HEADS;
    h->sectors = is_hd ? ADF_HD_SECTORS : ADF_DD_SECTORS;
    h->total_sectors = is_hd ? ADF_HD_TOTAL : ADF_DD_TOTAL;
    h->data_size = size;
    h->modified = false;
    
    /* Load image into memory */
    h->data = malloc(size);
    if (!h->data) {
        free(h);
        fclose(fp);
        return -3;
    }
    
    if (fread(h->data, 1, size, fp) != size) {
        free(h->data);
        free(h);
        fclose(fp);
        return -4;
    }
    
    *handle = h;
    return 0;
}

/**
 * @brief Create new ADF image
 */
int uft_adf_create(const char *path, bool is_hd, adf_handle_t **handle) {
    if (!path || !handle) return -1;
    
    adf_handle_t *h = calloc(1, sizeof(adf_handle_t));
    if (!h) return -3;
    
    h->is_hd = is_hd;
    h->tracks = ADF_DD_TRACKS;
    h->heads = ADF_DD_HEADS;
    h->sectors = is_hd ? ADF_HD_SECTORS : ADF_DD_SECTORS;
    h->total_sectors = is_hd ? ADF_HD_TOTAL : ADF_DD_TOTAL;
    h->data_size = is_hd ? UFT_ADF_SIZE_HD : UFT_ADF_SIZE_DD;
    h->readonly = false;
    h->modified = true;
    
    /* Allocate and zero data */
    h->data = calloc(1, h->data_size);
    if (!h->data) {
        free(h);
        return -3;
    }
    
    /* Open file for writing */
    h->fp = fopen(path, "wb");
    if (!h->fp) {
        free(h->data);
        free(h);
        return -1;
    }
    
    *handle = h;
    return 0;
}

/**
 * @brief Close ADF image
 */
void uft_adf_close(adf_handle_t *h) {
    if (!h) return;
    
    /* Write back changes */
    if (!h->readonly && h->modified && h->fp && h->data) {
        fseek(h->fp, 0, SEEK_SET);
        fwrite(h->data, 1, h->data_size, h->fp);
    }
    
    if (h->fp) fclose(h->fp);
    if (h->data) free(h->data);
    free(h);
}

/**
 * @brief Read ADF sector
 */
ssize_t uft_adf_read_sector(adf_handle_t *h, uint8_t track, uint8_t head,
                            uint8_t sector, uint8_t *buffer) {
    if (!h || !buffer) return -1;
    
    uint32_t offset = uft_adf_sector_offset(track, head, sector, h->is_hd);
    if (offset + ADF_SECTOR_SIZE > h->data_size) return -1;
    
    memcpy(buffer, h->data + offset, ADF_SECTOR_SIZE);
    return ADF_SECTOR_SIZE;
}

/**
 * @brief Write ADF sector
 */
ssize_t uft_adf_write_sector(adf_handle_t *h, uint8_t track, uint8_t head,
                             uint8_t sector, const uint8_t *data) {
    if (!h || !data || h->readonly) return -1;
    
    uint32_t offset = uft_adf_sector_offset(track, head, sector, h->is_hd);
    if (offset + ADF_SECTOR_SIZE > h->data_size) return -1;
    
    memcpy(h->data + offset, data, ADF_SECTOR_SIZE);
    h->modified = true;
    return ADF_SECTOR_SIZE;
}

/**
 * @brief Read complete track
 */
ssize_t uft_adf_read_track(adf_handle_t *h, uint8_t track, uint8_t head,
                           uint8_t *buffer, size_t buf_size) {
    if (!h || !buffer) return -1;
    
    size_t track_size = (size_t)(h->sectors * ADF_SECTOR_SIZE);
    if (buf_size < track_size) return -1;
    
    for (uint8_t s = 0; s < h->sectors; s++) {
        ssize_t ret = uft_adf_read_sector(h, track, head, s,
                                           buffer + s * ADF_SECTOR_SIZE);
        if (ret != ADF_SECTOR_SIZE) return -1;
    }
    
    return (ssize_t)track_size;
}

/**
 * @brief Check if disk has valid bootblock
 */
bool uft_adf_has_bootblock(adf_handle_t *h) {
    if (!h || !h->data) return false;
    
    /* Check for "DOS" signature */
    uint32_t sig = ((uint32_t)h->data[0] << 24) |
                   ((uint32_t)h->data[1] << 16) |
                   ((uint32_t)h->data[2] << 8) |
                   (uint32_t)h->data[3];
    
    return (sig & 0xFFFFFF00) == ADF_BOOTBLOCK_DOS;
}

/**
 * @brief Get AmigaDOS filesystem type
 */
int uft_adf_get_filesystem_type(adf_handle_t *h) {
    if (!h || !h->data) return -1;
    
    uint32_t sig = ((uint32_t)h->data[0] << 24) |
                   ((uint32_t)h->data[1] << 16) |
                   ((uint32_t)h->data[2] << 8) |
                   (uint32_t)h->data[3];
    
    switch (sig) {
        case ADF_BOOTBLOCK_DOS:   return 0;  /* OFS */
        case ADF_BOOTBLOCK_FFS:   return 1;  /* FFS */
        case ADF_BOOTBLOCK_INTL:  return 2;  /* International */
        case ADF_BOOTBLOCK_DCACHE: return 3; /* Directory cache */
        default: return -1;
    }
}

/**
 * @brief Calculate bootblock checksum
 */
uint32_t uft_adf_bootblock_checksum(const uint8_t *bootblock) {
    if (!bootblock) return 0;
    
    uint32_t checksum = 0;
    
    for (int i = 0; i < 1024; i += 4) {
        if (i == 4) continue;  /* Skip checksum field */
        
        uint32_t word = ((uint32_t)bootblock[i] << 24) |
                        ((uint32_t)bootblock[i+1] << 16) |
                        ((uint32_t)bootblock[i+2] << 8) |
                        (uint32_t)bootblock[i+3];
        
        uint32_t old_checksum = checksum;
        checksum += word;
        if (checksum < old_checksum) checksum++;  /* Carry */
    }
    
    return ~checksum;
}

/**
 * @brief Verify bootblock checksum
 */
bool uft_adf_verify_bootblock(adf_handle_t *h) {
    if (!h || !h->data) return false;
    
    uint32_t stored = ((uint32_t)h->data[4] << 24) |
                      ((uint32_t)h->data[5] << 16) |
                      ((uint32_t)h->data[6] << 8) |
                      (uint32_t)h->data[7];
    
    uint32_t calc = uft_adf_bootblock_checksum(h->data);
    
    return stored == calc;
}

/**
 * @brief Format disk as AmigaDOS
 */
int uft_adf_format(adf_handle_t *h, const char *volume_name, bool ffs) {
    if (!h || h->readonly) return -1;
    
    /* Clear disk */
    memset(h->data, 0, h->data_size);
    
    /* Create bootblock */
    uint32_t sig = ffs ? ADF_BOOTBLOCK_FFS : ADF_BOOTBLOCK_DOS;
    h->data[0] = (uint8_t)(sig >> 24);
    h->data[1] = (uint8_t)(sig >> 16);
    h->data[2] = (uint8_t)(sig >> 8);
    h->data[3] = (uint8_t)sig;
    
    /* Calculate and store checksum */
    uint32_t checksum = uft_adf_bootblock_checksum(h->data);
    h->data[4] = (uint8_t)(checksum >> 24);
    h->data[5] = (uint8_t)(checksum >> 16);
    h->data[6] = (uint8_t)(checksum >> 8);
    h->data[7] = (uint8_t)checksum;
    
    /* Create root block at track 40, sector 0 (middle of disk) */
    uint32_t root_offset = uft_adf_sector_offset(40, 0, 0, h->is_hd);
    
    /* Root block header */
    h->data[root_offset + 0] = 0;
    h->data[root_offset + 1] = 0;
    h->data[root_offset + 2] = 0;
    h->data[root_offset + 3] = 2;  /* T_HEADER */
    
    /* Hash table size */
    h->data[root_offset + 12] = 0;
    h->data[root_offset + 13] = 0;
    h->data[root_offset + 14] = 0;
    h->data[root_offset + 15] = 0x48;  /* 72 entries */
    
    /* Volume name */
    if (volume_name) {
        size_t name_len = strlen(volume_name);
        if (name_len > 30) name_len = 30;
        h->data[root_offset + 432] = (uint8_t)name_len;
        memcpy(h->data + root_offset + 433, volume_name, name_len);
    }
    
    /* Block type at end */
    h->data[root_offset + 508] = 0;
    h->data[root_offset + 509] = 0;
    h->data[root_offset + 510] = 0;
    h->data[root_offset + 511] = 1;  /* ST_ROOT */
    
    h->modified = true;
    return 0;
}

/* ============================================================================
 * MFM Track Encoding (for ADF -> flux conversion)
 * ============================================================================ */

/**
 * @brief Encode ADF track to MFM for flux output
 */
size_t uft_adf_encode_track_mfm(adf_handle_t *h, uint8_t track, uint8_t head,
                                 uint8_t *mfm_out, size_t mfm_size) {
    if (!h || !mfm_out) return 0;
    
    uint8_t sector_buf[ADF_SECTOR_SIZE];
    uint8_t *out = mfm_out;
    size_t total = 0;
    
    /* Track gap */
    size_t gap_size = h->is_hd ? 200 : 100;
    memset(out, 0xAA, gap_size);
    out += gap_size;
    total += gap_size;
    
    /* Encode each sector */
    for (uint8_t s = 0; s < h->sectors; s++) {
        if (total + 1088 > mfm_size) break;
        
        /* Read sector data */
        ssize_t ret = uft_adf_read_sector(h, track, head, s, sector_buf);
        if (ret != ADF_SECTOR_SIZE) continue;
        
        /* Sync words */
        *out++ = 0x44; *out++ = 0x89;
        *out++ = 0x44; *out++ = 0x89;
        total += 4;
        
        /* Build Amiga sector header */
        uft_amiga_sector_header_t hdr;
        hdr.format = 0xFF;
        hdr.track = (uint8_t)(track * 2 + head);
        hdr.sector = s;
        hdr.sectors_to_gap = (uint8_t)(h->sectors - s);
        memset(hdr.label, 0, 16);
        
        /* Calculate checksums */
        hdr.header_checksum = 0;  /* Would need proper calculation */
        hdr.data_checksum = uft_amiga_checksum(sector_buf, ADF_SECTOR_SIZE);
        
        /* Encode sector */
        size_t enc_size = uft_amiga_encode_sector(&hdr, sector_buf, out);
        out += enc_size;
        total += enc_size;
    }
    
    return total;
}

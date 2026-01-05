/**
 * @file uft_bbc_dfs.c
 * @brief BBC Micro DFS Filesystem Implementation
 * 
 * DFS Disk Layout (per side):
 * - Track 0, Sector 0: First half of catalog (8 filename entries)
 * - Track 0, Sector 1: Second half of catalog + disk info
 * - Track 0, Sectors 2+: File data
 * - Remaining tracks: File data
 * 
 * Catalog Entry Format (8 bytes per file):
 * Sector 0: Filename (7 bytes) + Directory char (1 byte)
 * Sector 1: Load addr (2 bytes) + Exec addr (2 bytes) + 
 *           Length (2 bytes) + Start sector (2 bytes with extras)
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#include "uft/fs/uft_bbc_fs.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define DFS_SECTOR_SIZE     256
#define DFS_CATALOG_SECTORS 2
#define DFS_CATALOG_ENTRIES 31
#define DFS_FILE_ENTRY_SIZE 8

/* Sector 1 offsets */
#define DFS_S1_TITLE8       0x00    /* Last 4 chars of title (bytes 0-3) */
#define DFS_S1_FILE_COUNT   0x05    /* Number of files * 8 */
#define DFS_S1_BOOT_OPT     0x06    /* Boot option in bits 4-5 */
#define DFS_S1_SECTORS_HI   0x06    /* High bits of sector count (bits 0-1) */
#define DFS_S1_SECTORS_LO   0x07    /* Low 8 bits of sector count */

/*===========================================================================
 * Format Names
 *===========================================================================*/

static const char *format_names[] = {
    "DFS 40T SS (100KB)",
    "DFS 80T SS (200KB)",
    "DFS 40T DS (200KB)",
    "DFS 80T DS (400KB)",
    "ADFS S (160KB)",
    "ADFS M (320KB)",
    "ADFS L (640KB)",
    "ADFS D (800KB)",
    "ADFS E (800KB)",
    "ADFS F (1600KB)",
    "ADFS G (3200KB)",
    "Opus DDOS 40T",
    "Opus DDOS 80T",
    "Opus EDOS",
    "Watford DDFS"
};

const char *uft_bbc_format_name(uft_bbc_format_t format)
{
    if (format < 0 || format >= UFT_BBC_FORMAT_COUNT) {
        return "Unknown";
    }
    return format_names[format];
}

bool uft_bbc_is_dfs(uft_bbc_format_t format)
{
    return format <= UFT_BBC_DFS_80T_DS;
}

bool uft_bbc_is_adfs(uft_bbc_format_t format)
{
    return format >= UFT_BBC_ADFS_S && format <= UFT_BBC_ADFS_G;
}

/*===========================================================================
 * Detection
 *===========================================================================*/

/**
 * @brief Check if byte is valid DFS filename character
 */
static bool is_dfs_name_char(uint8_t c)
{
    /* Space, or printable ASCII 33-126 */
    return c == 0x20 || (c >= 0x21 && c <= 0x7E);
}

/**
 * @brief Check if data looks like valid DFS catalog
 */
static int check_dfs_catalog(const uint8_t *data, size_t size)
{
    if (size < 512) return 0;  /* Need at least 2 sectors */
    
    const uint8_t *s0 = data;       /* Sector 0 */
    const uint8_t *s1 = data + 256; /* Sector 1 */
    
    /* Check file count is reasonable */
    int file_count = s1[DFS_S1_FILE_COUNT] / 8;
    if (file_count > DFS_CATALOG_ENTRIES) return 0;
    
    /* Check boot option is valid (0-3) */
    int boot_opt = (s1[DFS_S1_BOOT_OPT] >> 4) & 0x03;
    (void)boot_opt;  /* Any value 0-3 is valid */
    
    /* Get sector count */
    int sectors = ((s1[DFS_S1_SECTORS_HI] & 0x03) << 8) | s1[DFS_S1_SECTORS_LO];
    if (sectors < 2 || sectors > 3200) return 0;  /* Sanity check */
    
    /* Check first 8 bytes of title for printable chars */
    int valid_title = 0;
    for (int i = 0; i < 8; i++) {
        if (is_dfs_name_char(s0[i]) || s0[i] == 0) {
            valid_title++;
        }
    }
    
    /* Check filenames are valid */
    int valid_names = 0;
    for (int f = 0; f < file_count; f++) {
        const uint8_t *entry = s0 + 8 + f * 8;
        bool name_valid = true;
        
        for (int i = 0; i < 7; i++) {
            if (!is_dfs_name_char(entry[i] & 0x7F)) {
                name_valid = false;
                break;
            }
        }
        
        /* Directory char should be A-Z or $ */
        uint8_t dir_char = entry[7] & 0x7F;
        if (dir_char != '$' && (dir_char < 'A' || dir_char > 'Z')) {
            name_valid = false;
        }
        
        if (name_valid) valid_names++;
    }
    
    /* Calculate confidence */
    int confidence = 0;
    
    if (valid_title >= 6) confidence += 30;
    else if (valid_title >= 4) confidence += 20;
    
    if (file_count == 0) {
        confidence += 40;  /* Empty disk is still valid */
    } else if (valid_names == file_count) {
        confidence += 50;
    } else if (valid_names > file_count / 2) {
        confidence += 30;
    }
    
    /* Check disk size matches sector count */
    size_t expected = (size_t)sectors * DFS_SECTOR_SIZE;
    if (size >= expected) {
        confidence += 20;
    }
    
    return confidence;
}

int uft_bbc_detect(const uint8_t *data, size_t size,
                   uft_bbc_format_t *format_out)
{
    if (!data || size < 512) return 0;
    
    /* Try DFS detection first */
    int dfs_confidence = check_dfs_catalog(data, size);
    
    if (dfs_confidence >= 50) {
        /* Determine specific DFS variant based on size */
        if (format_out) {
            if (size <= 102400) {
                *format_out = UFT_BBC_DFS_40T_SS;
            } else if (size <= 204800) {
                /* Could be 80T SS or 40T DS - check second side */
                /* For now assume 80T SS */
                *format_out = UFT_BBC_DFS_80T_SS;
            } else if (size <= 409600) {
                *format_out = UFT_BBC_DFS_80T_DS;
            } else {
                *format_out = UFT_BBC_DFS_80T_DS;
            }
        }
        return dfs_confidence;
    }
    
    /* TODO: Add ADFS detection */
    /* ADFS has different signatures - check for "Hugo", "Nick", etc. */
    
    return 0;
}

/*===========================================================================
 * DFS Catalog Reading
 *===========================================================================*/

int uft_dfs_read_catalog(const uint8_t *data, size_t size,
                          uft_dfs_info_t *info)
{
    if (!data || !info || size < 512) return -1;
    
    memset(info, 0, sizeof(*info));
    
    const uint8_t *s0 = data;
    const uint8_t *s1 = data + 256;
    
    /* Read disk title (first 8 chars in sector 0, last 4 in sector 1) */
    for (int i = 0; i < 8; i++) {
        uint8_t c = s0[i];
        info->title[i] = (c >= 32 && c < 127) ? c : ' ';
    }
    for (int i = 0; i < 4; i++) {
        uint8_t c = s1[i];
        info->title[8 + i] = (c >= 32 && c < 127) ? c : ' ';
    }
    /* Trim trailing spaces */
    for (int i = 11; i >= 0; i--) {
        if (info->title[i] != ' ') break;
        info->title[i] = '\0';
    }
    
    /* Read disk info from sector 1 */
    info->sequence = s1[4];
    info->num_files = s1[DFS_S1_FILE_COUNT] / 8;
    info->boot_option = (s1[DFS_S1_BOOT_OPT] >> 4) & 0x03;
    info->num_sectors = ((s1[DFS_S1_SECTORS_HI] & 0x03) << 8) | s1[DFS_S1_SECTORS_LO];
    
    /* Determine geometry */
    if (info->num_sectors <= 400) {
        info->tracks = 40;
    } else {
        info->tracks = 80;
    }
    
    /* Double-sided if image has two catalogs */
    info->double_sided = (size >= 2 * info->num_sectors * DFS_SECTOR_SIZE);
    
    /* Limit file count */
    if (info->num_files > DFS_CATALOG_ENTRIES) {
        info->num_files = DFS_CATALOG_ENTRIES;
    }
    
    /* Read file entries */
    for (int f = 0; f < info->num_files; f++) {
        uft_dfs_file_t *file = &info->files[f];
        const uint8_t *e0 = s0 + 8 + f * 8;  /* Name in sector 0 */
        const uint8_t *e1 = s1 + 8 + f * 8;  /* Info in sector 1 */
        
        /* Filename (7 chars, bit 7 clear) */
        for (int i = 0; i < 7; i++) {
            uint8_t c = e0[i] & 0x7F;
            file->name[i] = (c >= 32 && c < 127) ? c : '\0';
        }
        /* Trim trailing spaces */
        for (int i = 6; i >= 0; i--) {
            if (file->name[i] != ' ') break;
            file->name[i] = '\0';
        }
        
        /* Directory character (bit 7 of byte 7) */
        file->dir = e0[7] & 0x7F;
        file->locked = (e0[7] & 0x80) != 0;
        
        /* Load address (18-bit) */
        file->load_addr = e1[0] | (e1[1] << 8);
        file->load_addr |= ((uint32_t)(e1[6] & 0x0C) << 14);
        
        /* Exec address (18-bit) */
        file->exec_addr = e1[2] | (e1[3] << 8);
        file->exec_addr |= ((uint32_t)(e1[6] & 0xC0) << 10);
        
        /* Length (18-bit) */
        file->length = e1[4] | (e1[5] << 8);
        file->length |= ((uint32_t)(e1[6] & 0x30) << 12);
        
        /* Start sector (10-bit) */
        file->start_sector = e1[7] | ((e1[6] & 0x03) << 8);
    }
    
    return 0;
}

const uft_dfs_file_t *uft_dfs_find_file(const uft_dfs_info_t *info,
                                         const char *name, char dir)
{
    if (!info || !name) return NULL;
    if (dir == 0) dir = '$';
    
    for (int f = 0; f < info->num_files; f++) {
        const uft_dfs_file_t *file = &info->files[f];
        
        if (file->dir == dir) {
            /* Case-insensitive comparison */
            bool match = true;
            for (int i = 0; name[i] || file->name[i]; i++) {
                if (toupper((unsigned char)name[i]) != 
                    toupper((unsigned char)file->name[i])) {
                    match = false;
                    break;
                }
            }
            if (match) return file;
        }
    }
    
    return NULL;
}

/*===========================================================================
 * DFS File Extraction
 *===========================================================================*/

int uft_dfs_extract_file(const uint8_t *data, size_t size,
                          const uft_dfs_file_t *file,
                          uint8_t *buffer, size_t buf_size)
{
    if (!data || !file || !buffer) return -1;
    
    if (file->length > buf_size) return -1;
    
    /* Calculate byte offset from sector number */
    size_t offset = (size_t)file->start_sector * DFS_SECTOR_SIZE;
    
    if (offset + file->length > size) return -1;
    
    memcpy(buffer, data + offset, file->length);
    
    return (int)file->length;
}

/*===========================================================================
 * DFS Image Creation
 *===========================================================================*/

int uft_dfs_create(uint8_t *buffer, size_t buf_size,
                   int tracks, bool double_sided,
                   const char *title)
{
    if (!buffer) return -1;
    if (tracks != 40 && tracks != 80) tracks = 80;
    
    /* Calculate size */
    int sectors_per_side = tracks * 10;  /* 10 sectors per track */
    int sides = double_sided ? 2 : 1;
    size_t total_size = (size_t)sectors_per_side * sides * DFS_SECTOR_SIZE;
    
    if (buf_size < total_size) return -1;
    
    /* Clear buffer */
    memset(buffer, 0, total_size);
    
    /* Fill in catalog */
    uint8_t *s0 = buffer;
    uint8_t *s1 = buffer + 256;
    
    /* Title */
    if (title) {
        size_t tlen = strlen(title);
        if (tlen > 12) tlen = 12;
        
        /* First 8 chars to sector 0 */
        for (size_t i = 0; i < 8 && i < tlen; i++) {
            s0[i] = (uint8_t)title[i];
        }
        /* Last 4 chars to sector 1 */
        for (size_t i = 0; i < 4 && (i + 8) < tlen; i++) {
            s1[i] = (uint8_t)title[i + 8];
        }
    }
    
    /* Disk info in sector 1 */
    s1[4] = 0;  /* Sequence number */
    s1[5] = 0;  /* File count (empty) */
    s1[6] = 0;  /* Boot option 0, sector count high bits */
    s1[7] = (uint8_t)sectors_per_side;  /* Sector count low */
    s1[6] |= (sectors_per_side >> 8) & 0x03;
    
    /* If double-sided, create second catalog too */
    if (double_sided) {
        size_t side1_offset = (size_t)sectors_per_side * DFS_SECTOR_SIZE;
        uint8_t *s0_2 = buffer + side1_offset;
        uint8_t *s1_2 = buffer + side1_offset + 256;
        
        /* Copy title */
        memcpy(s0_2, s0, 8);
        memcpy(s1_2, s1, 8);
    }
    
    return (int)total_size;
}

int uft_dfs_set_boot(uint8_t *data, size_t size, uft_dfs_boot_t option)
{
    if (!data || size < 512) return -1;
    if (option > UFT_DFS_BOOT_EXEC) return -1;
    
    uint8_t *s1 = data + 256;
    s1[DFS_S1_BOOT_OPT] = (s1[DFS_S1_BOOT_OPT] & 0xCF) | ((option & 0x03) << 4);
    
    return 0;
}

/*===========================================================================
 * DFS Validation
 *===========================================================================*/

int uft_dfs_validate(const uint8_t *data, size_t size,
                      char *errors, size_t err_size)
{
    if (errors && err_size > 0) errors[0] = '\0';
    
    uft_dfs_info_t info;
    if (uft_dfs_read_catalog(data, size, &info) < 0) {
        if (errors && err_size > 0) {
            snprintf(errors, err_size, "Failed to read catalog");
        }
        return 1;
    }
    
    int error_count = 0;
    size_t err_pos = 0;
    
    /* Check for overlapping files */
    for (int i = 0; i < info.num_files; i++) {
        const uft_dfs_file_t *f1 = &info.files[i];
        uint32_t f1_end = f1->start_sector + (f1->length + 255) / 256;
        
        /* Check file doesn't overlap catalog */
        if (f1->start_sector < 2) {
            error_count++;
            if (errors && err_pos < err_size) {
                err_pos += snprintf(errors + err_pos, err_size - err_pos,
                    "File '%c.%s' overlaps catalog\n", f1->dir, f1->name);
            }
        }
        
        /* Check file doesn't extend past disk */
        if (f1_end > info.num_sectors) {
            error_count++;
            if (errors && err_pos < err_size) {
                err_pos += snprintf(errors + err_pos, err_size - err_pos,
                    "File '%c.%s' extends past disk end\n", f1->dir, f1->name);
            }
        }
        
        /* Check for overlap with other files */
        for (int j = i + 1; j < info.num_files; j++) {
            const uft_dfs_file_t *f2 = &info.files[j];
            uint32_t f2_end = f2->start_sector + (f2->length + 255) / 256;
            
            if (!(f1_end <= f2->start_sector || f2_end <= f1->start_sector)) {
                error_count++;
                if (errors && err_pos < err_size) {
                    err_pos += snprintf(errors + err_pos, err_size - err_pos,
                        "Files '%c.%s' and '%c.%s' overlap\n",
                        f1->dir, f1->name, f2->dir, f2->name);
                }
            }
        }
    }
    
    return error_count;
}

/*===========================================================================
 * Address Encoding/Decoding
 *===========================================================================*/

uint32_t uft_bbc_decode_addr(uint16_t base, uint8_t extra)
{
    /* Extra bits are in bits 2-3 (load) or 6-7 (exec) */
    return base | ((uint32_t)(extra & 0x03) << 16);
}

void uft_bbc_encode_addr(uint32_t addr, uint16_t *base, uint8_t *extra)
{
    *base = (uint16_t)(addr & 0xFFFF);
    *extra = (uint8_t)((addr >> 16) & 0x03);
}

/*===========================================================================
 * BBC String Handling
 *===========================================================================*/

void uft_bbc_string_decode(const uint8_t *src, char *dst, size_t max_len)
{
    if (!src || !dst || max_len == 0) return;
    
    size_t i;
    for (i = 0; i < max_len - 1; i++) {
        uint8_t c = src[i];
        dst[i] = c & 0x7F;
        
        if (c & 0x80) {
            /* Last character has bit 7 set */
            i++;
            break;
        }
        
        if (c == 0x0D || c == 0) break;
    }
    dst[i] = '\0';
}

void uft_bbc_string_encode(const char *src, uint8_t *dst, size_t max_len)
{
    if (!src || !dst || max_len == 0) return;
    
    size_t len = strlen(src);
    if (len > max_len) len = max_len;
    
    for (size_t i = 0; i < len; i++) {
        dst[i] = (uint8_t)src[i];
    }
    
    /* Set bit 7 on last character */
    if (len > 0) {
        dst[len - 1] |= 0x80;
    }
}

/*===========================================================================
 * Conversion Functions
 *===========================================================================*/

int uft_bbc_to_ssd(const uint8_t *data, size_t size,
                   uint8_t *ssd_out, size_t ssd_size)
{
    if (!data || !ssd_out) return -1;
    
    uft_dfs_info_t info;
    if (uft_dfs_read_catalog(data, size, &info) < 0) {
        return -1;
    }
    
    /* SSD is just raw sectors */
    size_t ssd_len = (size_t)info.num_sectors * DFS_SECTOR_SIZE;
    
    if (ssd_size < ssd_len) return -1;
    
    memcpy(ssd_out, data, ssd_len);
    
    return (int)ssd_len;
}

int uft_bbc_to_dsd(const uint8_t *data, size_t size,
                   uint8_t *dsd_out, size_t dsd_size)
{
    if (!data || !dsd_out) return -1;
    
    /* DSD interleaves sides: Track 0 Side 0, Track 0 Side 1, Track 1 Side 0, ... */
    /* First, check if input is already interleaved */
    
    uft_dfs_info_t info;
    if (uft_dfs_read_catalog(data, size, &info) < 0) {
        return -1;
    }
    
    if (!info.double_sided) {
        /* Single-sided: just copy */
        size_t len = (size_t)info.num_sectors * DFS_SECTOR_SIZE;
        if (dsd_size < len) return -1;
        memcpy(dsd_out, data, len);
        return (int)len;
    }
    
    /* Double-sided: interleave tracks */
    int sectors_per_track = 10;
    int bytes_per_track = sectors_per_track * DFS_SECTOR_SIZE;
    size_t side_size = (size_t)info.num_sectors * DFS_SECTOR_SIZE;
    
    size_t dsd_len = side_size * 2;
    if (dsd_size < dsd_len) return -1;
    
    for (int track = 0; track < info.tracks; track++) {
        /* Side 0 */
        size_t src_off = track * bytes_per_track;
        size_t dst_off = track * 2 * bytes_per_track;
        memcpy(dsd_out + dst_off, data + src_off, bytes_per_track);
        
        /* Side 1 */
        src_off = side_size + track * bytes_per_track;
        dst_off = (track * 2 + 1) * bytes_per_track;
        memcpy(dsd_out + dst_off, data + src_off, bytes_per_track);
    }
    
    return (int)dsd_len;
}

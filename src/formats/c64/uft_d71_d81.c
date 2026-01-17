/**
 * @file uft_d71_d81.c
 * @brief 1571/1581 Disk Image Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/c64/uft_d71_d81.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Static Data - D71
 * ============================================================================ */

/** Sectors per track for D71 (same as D64, but tracks 36-70 mirror 1-35) */
static const int d71_sectors[71] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /*  1-10 */
    21, 21, 21, 21, 21, 21, 21, 19, 19, 19,  /* 11-20 */
    19, 19, 19, 19, 18, 18, 18, 18, 18, 18,  /* 21-30 */
    17, 17, 17, 17, 17,                       /* 31-35 */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 36-45 (side 1) */
    21, 21, 21, 21, 21, 21, 21, 19, 19, 19,  /* 46-55 */
    19, 19, 19, 19, 18, 18, 18, 18, 18, 18,  /* 56-65 */
    17, 17, 17, 17, 17                        /* 66-70 */
};

/** Track offsets for D71 */
static int d71_offsets[71] = {0};
static bool d71_offsets_init = false;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Initialize D71 track offsets
 */
static void init_d71_offsets(void)
{
    if (d71_offsets_init) return;
    
    d71_offsets[0] = 0;
    for (int t = 1; t <= 70; t++) {
        d71_offsets[t] = d71_offsets[t - 1] + d71_sectors[t - 1];
    }
    d71_offsets_init = true;
}

/**
 * @brief Convert PETSCII to ASCII
 */
static void petscii_to_ascii(const uint8_t *petscii, char *ascii, int len)
{
    for (int i = 0; i < len; i++) {
        uint8_t c = petscii[i];
        if (c == 0xA0) {
            ascii[i] = '\0';
            return;
        } else if (c >= 0x41 && c <= 0x5A) {
            ascii[i] = c;
        } else if (c >= 0xC1 && c <= 0xDA) {
            ascii[i] = c - 0x80;
        } else if (c >= 0x20 && c <= 0x7E) {
            ascii[i] = c;
        } else {
            ascii[i] = '_';
        }
    }
    ascii[len] = '\0';
}

/**
 * @brief Convert ASCII to PETSCII
 */
static void ascii_to_petscii(const char *ascii, uint8_t *petscii, int len)
{
    int i;
    for (i = 0; i < len && ascii[i]; i++) {
        char c = ascii[i];
        if (c >= 'a' && c <= 'z') {
            petscii[i] = c - 32;
        } else {
            petscii[i] = c;
        }
    }
    for (; i < len; i++) {
        petscii[i] = 0xA0;
    }
}

/* ============================================================================
 * D71 Implementation
 * ============================================================================ */

/**
 * @brief Get D71 sector offset
 */
int d71_sector_offset(int track, int sector)
{
    if (track < 1 || track > 70) return -1;
    if (sector < 0 || sector >= d71_sectors[track]) return -1;
    
    init_d71_offsets();
    return (d71_offsets[track] + sector) * 256;
}

/**
 * @brief Get D71 sectors per track
 */
int d71_sectors_per_track(int track)
{
    if (track < 1 || track > 70) return 0;
    return d71_sectors[track];
}

/**
 * @brief Validate D71
 */
bool d71_validate(const uint8_t *data, size_t size)
{
    if (!data) return false;
    
    if (size == D71_SIZE_STANDARD || size == D71_SIZE_ERRORS) {
        /* Check BAM signature at track 18, sector 0 */
        int bam_offset = d71_sector_offset(18, 0);
        if (bam_offset < 0) return false;
        
        /* First byte should be 18 (directory track) */
        if (data[bam_offset] == 18 && data[bam_offset + 1] == 1) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Create D71 editor
 */
d71_editor_t *d71_editor_create(uint8_t *data, size_t size)
{
    if (!data || !d71_validate(data, size)) return NULL;
    
    d71_editor_t *editor = calloc(1, sizeof(d71_editor_t));
    if (!editor) return NULL;
    
    editor->data = data;
    editor->size = size;
    editor->has_errors = (size == D71_SIZE_ERRORS);
    
    return editor;
}

/**
 * @brief Free D71 editor
 */
void d71_editor_free(d71_editor_t *editor)
{
    free(editor);
}

/**
 * @brief Create empty D71
 */
int d71_create(const char *disk_name, const char *disk_id,
               uint8_t **data, size_t *size)
{
    if (!data || !size) return -1;
    
    *data = calloc(1, D71_SIZE_STANDARD);
    if (!*data) return -2;
    
    *size = D71_SIZE_STANDARD;
    
    d71_editor_t *editor = calloc(1, sizeof(d71_editor_t));
    if (!editor) {
        free(*data);
        return -3;
    }
    
    editor->data = *data;
    editor->size = *size;
    
    d71_format(editor, disk_name ? disk_name : "EMPTY DISK", 
               disk_id ? disk_id : "01");
    
    free(editor);
    return 0;
}

/**
 * @brief Format D71
 */
int d71_format(d71_editor_t *editor, const char *disk_name, const char *disk_id)
{
    if (!editor || !disk_name || !disk_id) return -1;
    
    /* Clear BAM sector (track 18, sector 0) */
    int bam_offset = d71_sector_offset(18, 0);
    uint8_t *bam = editor->data + bam_offset;
    memset(bam, 0, 256);
    
    /* Set up BAM header */
    bam[0] = 18;    /* Directory track */
    bam[1] = 1;     /* Directory sector */
    bam[2] = 0x41;  /* DOS version 'A' */
    bam[3] = 0x80;  /* Double-sided flag */
    
    /* Initialize BAM entries for side 0 (tracks 1-35) */
    for (int t = 1; t <= 35; t++) {
        int entry_offset = 4 + (t - 1) * 4;
        int sectors = d71_sectors[t];
        
        bam[entry_offset] = sectors;  /* Free sectors */
        bam[entry_offset + 1] = 0xFF;
        bam[entry_offset + 2] = 0xFF;
        bam[entry_offset + 3] = (sectors > 16) ? 0x1F : ((1 << (sectors - 16)) - 1);
    }
    
    /* Set disk name */
    memset(bam + 0x90, 0xA0, 16);
    ascii_to_petscii(disk_name, bam + 0x90, 16);
    
    /* Set disk ID */
    bam[0xA2] = disk_id[0];
    bam[0xA3] = disk_id[1];
    bam[0xA4] = 0xA0;
    bam[0xA5] = '2';
    bam[0xA6] = 'C';  /* 1571 DOS type */
    
    /* BAM for side 1 (track 53, sector 0) */
    int bam2_offset = d71_sector_offset(53, 0);
    uint8_t *bam2 = editor->data + bam2_offset;
    memset(bam2, 0, 256);
    
    for (int t = 36; t <= 70; t++) {
        int entry_offset = (t - 36) * 3;
        int sectors = d71_sectors[t];
        
        bam2[entry_offset] = 0xFF;
        bam2[entry_offset + 1] = 0xFF;
        bam2[entry_offset + 2] = (sectors > 16) ? 0x1F : ((1 << (sectors - 16)) - 1);
    }
    
    /* Free sector counts for side 1 at offset 0xDD */
    for (int t = 36; t <= 70; t++) {
        bam[0xDD + (t - 36)] = d71_sectors[t];
    }
    
    /* Allocate BAM and directory sectors */
    d71_allocate_block(editor, 18, 0);  /* BAM */
    d71_allocate_block(editor, 18, 1);  /* Directory */
    d71_allocate_block(editor, 53, 0);  /* BAM side 1 */
    
    /* Initialize directory sector */
    int dir_offset = d71_sector_offset(18, 1);
    memset(editor->data + dir_offset, 0, 256);
    editor->data[dir_offset + 1] = 0xFF;  /* End of directory chain */
    
    editor->modified = true;
    return 0;
}

/**
 * @brief Get D71 info
 */
int d71_get_info(const d71_editor_t *editor, d71_info_t *info)
{
    if (!editor || !info) return -1;
    
    memset(info, 0, sizeof(d71_info_t));
    
    int bam_offset = d71_sector_offset(18, 0);
    uint8_t *bam = editor->data + bam_offset;
    
    petscii_to_ascii(bam + 0x90, info->disk_name, 16);
    info->disk_id[0] = bam[0xA2];
    info->disk_id[1] = bam[0xA3];
    info->disk_id[2] = '\0';
    info->dos_type[0] = bam[0xA5];
    info->dos_type[1] = bam[0xA6];
    info->dos_type[2] = '\0';
    
    info->double_sided = (bam[3] & 0x80) != 0;
    info->total_blocks = D71_TOTAL_SECTORS - 4;  /* Minus BAM/DIR sectors */
    info->free_blocks = d71_get_free_blocks(editor);
    info->used_blocks = info->total_blocks - info->free_blocks;
    
    return 0;
}

/**
 * @brief Check if D71 block is free
 */
bool d71_is_block_free(const d71_editor_t *editor, int track, int sector)
{
    if (!editor || track < 1 || track > 70) return false;
    if (sector < 0 || sector >= d71_sectors[track]) return false;
    
    int bam_offset = d71_sector_offset(18, 0);
    uint8_t *bam = editor->data + bam_offset;
    
    int byte_idx = sector / 8;
    int bit_idx = sector % 8;
    
    if (track <= 35) {
        int entry_offset = 4 + (track - 1) * 4;
        return (bam[entry_offset + 1 + byte_idx] & (1 << bit_idx)) != 0;
    } else {
        /* Side 1 BAM at track 53 */
        int bam2_offset = d71_sector_offset(53, 0);
        int entry_offset = (track - 36) * 3;
        return (editor->data[bam2_offset + entry_offset + byte_idx] & (1 << bit_idx)) != 0;
    }
}

/**
 * @brief Allocate D71 block
 */
int d71_allocate_block(d71_editor_t *editor, int track, int sector)
{
    if (!d71_is_block_free(editor, track, sector)) return -1;
    
    int bam_offset = d71_sector_offset(18, 0);
    uint8_t *bam = editor->data + bam_offset;
    
    int byte_idx = sector / 8;
    int bit_idx = sector % 8;
    
    if (track <= 35) {
        int entry_offset = 4 + (track - 1) * 4;
        bam[entry_offset + 1 + byte_idx] &= ~(1 << bit_idx);
        bam[entry_offset]--;
    } else {
        int bam2_offset = d71_sector_offset(53, 0);
        int entry_offset = (track - 36) * 3;
        editor->data[bam2_offset + entry_offset + byte_idx] &= ~(1 << bit_idx);
        bam[0xDD + (track - 36)]--;
    }
    
    editor->modified = true;
    return 0;
}

/**
 * @brief Free D71 block
 */
int d71_free_block(d71_editor_t *editor, int track, int sector)
{
    if (d71_is_block_free(editor, track, sector)) return 0;
    
    int bam_offset = d71_sector_offset(18, 0);
    uint8_t *bam = editor->data + bam_offset;
    
    int byte_idx = sector / 8;
    int bit_idx = sector % 8;
    
    if (track <= 35) {
        int entry_offset = 4 + (track - 1) * 4;
        bam[entry_offset + 1 + byte_idx] |= (1 << bit_idx);
        bam[entry_offset]++;
    } else {
        int bam2_offset = d71_sector_offset(53, 0);
        int entry_offset = (track - 36) * 3;
        editor->data[bam2_offset + entry_offset + byte_idx] |= (1 << bit_idx);
        bam[0xDD + (track - 36)]++;
    }
    
    editor->modified = true;
    return 0;
}

/**
 * @brief Get D71 free blocks
 */
int d71_get_free_blocks(const d71_editor_t *editor)
{
    if (!editor) return 0;
    
    int free_count = 0;
    int bam_offset = d71_sector_offset(18, 0);
    uint8_t *bam = editor->data + bam_offset;
    
    /* Side 0 */
    for (int t = 1; t <= 35; t++) {
        if (t == 18) continue;  /* Skip directory track */
        free_count += bam[4 + (t - 1) * 4];
    }
    
    /* Side 1 */
    for (int t = 36; t <= 70; t++) {
        if (t == 53) continue;  /* Skip BAM track */
        free_count += bam[0xDD + (t - 36)];
    }
    
    return free_count;
}

/**
 * @brief Read D71 sector
 */
int d71_read_sector(const d71_editor_t *editor, int track, int sector,
                    uint8_t *buffer)
{
    if (!editor || !buffer) return -1;
    
    int offset = d71_sector_offset(track, sector);
    if (offset < 0 || (size_t)(offset + 256) > editor->size) return -2;
    
    memcpy(buffer, editor->data + offset, 256);
    return 0;
}

/**
 * @brief Write D71 sector
 */
int d71_write_sector(d71_editor_t *editor, int track, int sector,
                     const uint8_t *buffer)
{
    if (!editor || !buffer) return -1;
    
    int offset = d71_sector_offset(track, sector);
    if (offset < 0 || (size_t)(offset + 256) > editor->size) return -2;
    
    memcpy(editor->data + offset, buffer, 256);
    editor->modified = true;
    return 0;
}

/**
 * @brief Print D71 directory
 */
void d71_print_directory(const d71_editor_t *editor, FILE *fp)
{
    if (!editor || !fp) return;
    
    d71_info_t info;
    d71_get_info(editor, &info);
    
    fprintf(fp, "0 \"%-16s\" %s %s\n", info.disk_name, info.disk_id, info.dos_type);
    fprintf(fp, "%d BLOCKS FREE.\n", info.free_blocks);
}

/* ============================================================================
 * D81 Implementation
 * ============================================================================ */

/**
 * @brief Get D81 sector offset
 */
int d81_sector_offset(int track, int sector)
{
    if (track < 1 || track > 80) return -1;
    if (sector < 0 || sector >= D81_SECTORS_PER_TRACK) return -1;
    
    return ((track - 1) * D81_SECTORS_PER_TRACK + sector) * D81_SECTOR_SIZE;
}

/**
 * @brief Validate D81
 */
bool d81_validate(const uint8_t *data, size_t size)
{
    if (!data) return false;
    
    if (size == D81_SIZE_STANDARD || size == D81_SIZE_ERRORS) {
        /* Check header at track 40, sector 0 */
        int header_offset = d81_sector_offset(40, 0);
        
        /* Directory track should be 40 */
        if (data[header_offset] == 40 && data[header_offset + 1] == 3) {
            /* Check for 'D' format marker */
            if (data[header_offset + 2] == 'D') {
                return true;
            }
        }
    }
    
    return false;
}

/**
 * @brief Create D81 editor
 */
d81_editor_t *d81_editor_create(uint8_t *data, size_t size)
{
    if (!data || !d81_validate(data, size)) return NULL;
    
    d81_editor_t *editor = calloc(1, sizeof(d81_editor_t));
    if (!editor) return NULL;
    
    editor->data = data;
    editor->size = size;
    editor->has_errors = (size == D81_SIZE_ERRORS);
    
    int header_offset = d81_sector_offset(40, 0);
    editor->header = (d81_header_t *)(data + header_offset);
    
    return editor;
}

/**
 * @brief Free D81 editor
 */
void d81_editor_free(d81_editor_t *editor)
{
    free(editor);
}

/**
 * @brief Create empty D81
 */
int d81_create(const char *disk_name, const char *disk_id,
               uint8_t **data, size_t *size)
{
    if (!data || !size) return -1;
    
    *data = calloc(1, D81_SIZE_STANDARD);
    if (!*data) return -2;
    
    *size = D81_SIZE_STANDARD;
    
    d81_editor_t *editor = calloc(1, sizeof(d81_editor_t));
    if (!editor) {
        free(*data);
        return -3;
    }
    
    editor->data = *data;
    editor->size = *size;
    
    int header_offset = d81_sector_offset(40, 0);
    editor->header = (d81_header_t *)(*data + header_offset);
    
    d81_format(editor, disk_name ? disk_name : "EMPTY DISK",
               disk_id ? disk_id : "01");
    
    free(editor);
    return 0;
}

/**
 * @brief Format D81
 */
int d81_format(d81_editor_t *editor, const char *disk_name, const char *disk_id)
{
    if (!editor || !disk_name || !disk_id) return -1;
    
    /* Initialize header (track 40, sector 0) */
    int header_offset = d81_sector_offset(40, 0);
    uint8_t *header = editor->data + header_offset;
    memset(header, 0, 256);
    
    header[0] = 40;    /* Directory track */
    header[1] = 3;     /* First directory sector */
    header[2] = 'D';   /* Format ID */
    
    /* Disk name */
    memset(header + 4, 0xA0, 16);
    ascii_to_petscii(disk_name, header + 4, 16);
    
    header[0x16] = 0xA0;
    header[0x17] = 0xA0;
    header[0x18] = disk_id[0];
    header[0x19] = disk_id[1];
    header[0x1A] = 0xA0;
    header[0x1B] = '3';
    header[0x1C] = 'D';  /* 1581 DOS type */
    
    /* Initialize BAM sectors (40/1 and 40/2) */
    for (int bam_sec = 1; bam_sec <= 2; bam_sec++) {
        int bam_offset = d81_sector_offset(40, bam_sec);
        uint8_t *bam = editor->data + bam_offset;
        memset(bam, 0, 256);
        
        bam[0] = 40;
        bam[1] = (bam_sec == 1) ? 2 : 0xFF;
        bam[2] = 'D';
        bam[3] = 0xBB;  /* BAM version */
        
        /* Initialize track entries */
        int start_track = (bam_sec == 1) ? 1 : 41;
        int end_track = (bam_sec == 1) ? 40 : 80;
        
        for (int t = start_track; t <= end_track; t++) {
            int entry_offset = 16 + (t - start_track) * 6;
            bam[entry_offset] = D81_SECTORS_PER_TRACK;  /* Free sectors */
            /* All bits set = all free */
            bam[entry_offset + 1] = 0xFF;
            bam[entry_offset + 2] = 0xFF;
            bam[entry_offset + 3] = 0xFF;
            bam[entry_offset + 4] = 0xFF;
            bam[entry_offset + 5] = 0xFF;
        }
    }
    
    /* Allocate header and BAM sectors */
    d81_allocate_block(editor, 40, 0);
    d81_allocate_block(editor, 40, 1);
    d81_allocate_block(editor, 40, 2);
    d81_allocate_block(editor, 40, 3);  /* First directory sector */
    
    /* Initialize directory sector */
    int dir_offset = d81_sector_offset(40, 3);
    memset(editor->data + dir_offset, 0, 256);
    editor->data[dir_offset] = 40;
    editor->data[dir_offset + 1] = 0xFF;  /* End of directory */
    
    editor->modified = true;
    return 0;
}

/**
 * @brief Get D81 info
 */
int d81_get_info(const d81_editor_t *editor, d81_info_t *info)
{
    if (!editor || !info) return -1;
    
    memset(info, 0, sizeof(d81_info_t));
    
    int header_offset = d81_sector_offset(40, 0);
    uint8_t *header = editor->data + header_offset;
    
    petscii_to_ascii(header + 4, info->disk_name, 16);
    info->disk_id[0] = header[0x18];
    info->disk_id[1] = header[0x19];
    info->disk_id[2] = '\0';
    info->dos_version[0] = header[0x1B];
    info->dos_version[1] = header[0x1C];
    info->dos_version[2] = '\0';
    
    info->total_blocks = D81_TOTAL_SECTORS - 40;  /* Minus track 40 */
    info->free_blocks = d81_get_free_blocks(editor);
    info->used_blocks = info->total_blocks - info->free_blocks;
    
    return 0;
}

/**
 * @brief Check if D81 block is free
 */
bool d81_is_block_free(const d81_editor_t *editor, int track, int sector)
{
    if (!editor || track < 1 || track > 80) return false;
    if (sector < 0 || sector >= D81_SECTORS_PER_TRACK) return false;
    
    int bam_sector = (track <= 40) ? 1 : 2;
    int bam_offset = d81_sector_offset(40, bam_sector);
    int start_track = (bam_sector == 1) ? 1 : 41;
    
    int entry_offset = 16 + (track - start_track) * 6;
    int byte_idx = sector / 8;
    int bit_idx = sector % 8;
    
    return (editor->data[bam_offset + entry_offset + 1 + byte_idx] & (1 << bit_idx)) != 0;
}

/**
 * @brief Allocate D81 block
 */
int d81_allocate_block(d81_editor_t *editor, int track, int sector)
{
    if (!d81_is_block_free(editor, track, sector)) return -1;
    
    int bam_sector = (track <= 40) ? 1 : 2;
    int bam_offset = d81_sector_offset(40, bam_sector);
    int start_track = (bam_sector == 1) ? 1 : 41;
    
    int entry_offset = 16 + (track - start_track) * 6;
    int byte_idx = sector / 8;
    int bit_idx = sector % 8;
    
    editor->data[bam_offset + entry_offset + 1 + byte_idx] &= ~(1 << bit_idx);
    editor->data[bam_offset + entry_offset]--;
    
    editor->modified = true;
    return 0;
}

/**
 * @brief Free D81 block
 */
int d81_free_block(d81_editor_t *editor, int track, int sector)
{
    if (d81_is_block_free(editor, track, sector)) return 0;
    
    int bam_sector = (track <= 40) ? 1 : 2;
    int bam_offset = d81_sector_offset(40, bam_sector);
    int start_track = (bam_sector == 1) ? 1 : 41;
    
    int entry_offset = 16 + (track - start_track) * 6;
    int byte_idx = sector / 8;
    int bit_idx = sector % 8;
    
    editor->data[bam_offset + entry_offset + 1 + byte_idx] |= (1 << bit_idx);
    editor->data[bam_offset + entry_offset]++;
    
    editor->modified = true;
    return 0;
}

/**
 * @brief Get D81 free blocks
 */
int d81_get_free_blocks(const d81_editor_t *editor)
{
    if (!editor) return 0;
    
    int free_count = 0;
    
    /* BAM 1 (tracks 1-40) */
    int bam1_offset = d81_sector_offset(40, 1);
    for (int t = 1; t <= 40; t++) {
        if (t == 40) continue;  /* Skip header track */
        free_count += editor->data[bam1_offset + 16 + (t - 1) * 6];
    }
    
    /* BAM 2 (tracks 41-80) */
    int bam2_offset = d81_sector_offset(40, 2);
    for (int t = 41; t <= 80; t++) {
        free_count += editor->data[bam2_offset + 16 + (t - 41) * 6];
    }
    
    return free_count;
}

/**
 * @brief Read D81 sector
 */
int d81_read_sector(const d81_editor_t *editor, int track, int sector,
                    uint8_t *buffer)
{
    if (!editor || !buffer) return -1;
    
    int offset = d81_sector_offset(track, sector);
    if (offset < 0 || (size_t)(offset + 256) > editor->size) return -2;
    
    memcpy(buffer, editor->data + offset, 256);
    return 0;
}

/**
 * @brief Write D81 sector
 */
int d81_write_sector(d81_editor_t *editor, int track, int sector,
                     const uint8_t *buffer)
{
    if (!editor || !buffer) return -1;
    
    int offset = d81_sector_offset(track, sector);
    if (offset < 0 || (size_t)(offset + 256) > editor->size) return -2;
    
    memcpy(editor->data + offset, buffer, 256);
    editor->modified = true;
    return 0;
}

/**
 * @brief Print D81 directory
 */
void d81_print_directory(const d81_editor_t *editor, FILE *fp)
{
    if (!editor || !fp) return;
    
    d81_info_t info;
    d81_get_info(editor, &info);
    
    fprintf(fp, "0 \"%-16s\" %s %s\n", info.disk_name, info.disk_id, info.dos_version);
    fprintf(fp, "%d BLOCKS FREE.\n", info.free_blocks);
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Detect CBM disk type
 */
char detect_cbm_disk_type(const uint8_t *data, size_t size)
{
    if (!data) return 0;
    
    if (d81_validate(data, size)) return '8';
    if (d71_validate(data, size)) return '7';
    
    /* Check D64 */
    if (size >= 174848 && size <= 175531) {
        int bam_offset = 91 * 256;  /* Track 18, sector 0 for D64 */
        if (data[bam_offset] == 18 && data[bam_offset + 1] == 1) {
            return 'd';
        }
    }
    
    return 0;
}

/**
 * @brief Convert D64 to D71
 */
int d64_to_d71(const uint8_t *d64_data, size_t d64_size,
               uint8_t **d71_data, size_t *d71_size)
{
    if (!d64_data || !d71_data || !d71_size) return -1;
    if (d64_size < 174848) return -2;
    
    *d71_data = calloc(1, D71_SIZE_STANDARD);
    if (!*d71_data) return -3;
    
    *d71_size = D71_SIZE_STANDARD;
    
    /* Copy side 0 data */
    memcpy(*d71_data, d64_data, 174848);
    
    /* Update BAM to indicate double-sided */
    int bam_offset = d71_sector_offset(18, 0);
    (*d71_data)[bam_offset + 3] = 0x80;
    
    /* Initialize side 1 BAM */
    int bam2_offset = d71_sector_offset(53, 0);
    memset(*d71_data + bam2_offset, 0xFF, 256);
    
    return 0;
}

/**
 * @brief Convert D71 to D64
 */
int d71_to_d64(const uint8_t *d71_data, size_t d71_size,
               uint8_t **d64_data, size_t *d64_size)
{
    if (!d71_data || !d64_data || !d64_size) return -1;
    if (d71_size < D71_SIZE_STANDARD) return -2;
    
    *d64_size = 174848;
    *d64_data = malloc(*d64_size);
    if (!*d64_data) return -3;
    
    /* Copy side 0 */
    memcpy(*d64_data, d71_data, *d64_size);
    
    /* Clear double-sided flag */
    int bam_offset = 91 * 256;
    (*d64_data)[bam_offset + 3] = 0x00;
    
    return 0;
}

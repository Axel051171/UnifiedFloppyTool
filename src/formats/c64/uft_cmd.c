/**
 * @file uft_cmd.c
 * @brief CMD FD2000/FD4000 Disk Image Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/c64/uft_cmd.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Detection
 * ============================================================================ */

cmd_image_type_t cmd_detect_type(size_t size)
{
    if (size == D1M_SIZE) return CMD_TYPE_D1M;
    if (size == D2M_SIZE) return CMD_TYPE_D2M;
    if (size == D4M_SIZE) return CMD_TYPE_D4M;
    if (size > D4M_SIZE) return CMD_TYPE_DHD;
    return CMD_TYPE_UNKNOWN;
}

const char *cmd_type_name(cmd_image_type_t type)
{
    switch (type) {
        case CMD_TYPE_D1M: return "D1M (1581 Emulation)";
        case CMD_TYPE_D2M: return "D2M (FD2000)";
        case CMD_TYPE_D4M: return "D4M (FD4000)";
        case CMD_TYPE_DHD: return "DHD (Hard Drive)";
        default: return "Unknown";
    }
}

bool cmd_validate(const uint8_t *data, size_t size)
{
    if (!data || size < D1M_SIZE) return false;
    
    cmd_image_type_t type = cmd_detect_type(size);
    if (type == CMD_TYPE_UNKNOWN) return false;
    
    /* Check for valid BAM signature at track 1, sector 0 */
    /* BAM offset depends on type */
    int bam_offset = 0;  /* Track 1, sector 0 */
    
    /* Check directory track pointer */
    if (data[bam_offset] != CMD_DIR_TRACK) return false;
    
    return true;
}

size_t cmd_type_size(cmd_image_type_t type)
{
    switch (type) {
        case CMD_TYPE_D1M: return D1M_SIZE;
        case CMD_TYPE_D2M: return D2M_SIZE;
        case CMD_TYPE_D4M: return D4M_SIZE;
        default: return 0;
    }
}

int cmd_type_tracks(cmd_image_type_t type)
{
    switch (type) {
        case CMD_TYPE_D1M: return D1M_TRACKS;
        case CMD_TYPE_D2M: return D2M_TRACKS;
        case CMD_TYPE_D4M: return D4M_TRACKS;
        default: return 0;
    }
}

int cmd_type_sectors(cmd_image_type_t type)
{
    switch (type) {
        case CMD_TYPE_D1M: return D1M_SECTORS_PER_TRACK;
        case CMD_TYPE_D2M: return D2M_SECTORS_PER_TRACK;
        case CMD_TYPE_D4M: return D4M_SECTORS_PER_TRACK;
        default: return 0;
    }
}

/* ============================================================================
 * Editor Operations
 * ============================================================================ */

int cmd_editor_create(const uint8_t *data, size_t size, cmd_editor_t *editor)
{
    if (!data || !editor) return -1;
    
    cmd_image_type_t type = cmd_detect_type(size);
    if (type == CMD_TYPE_UNKNOWN) return -2;
    
    memset(editor, 0, sizeof(cmd_editor_t));
    
    editor->data = malloc(size);
    if (!editor->data) return -3;
    
    memcpy(editor->data, data, size);
    editor->size = size;
    editor->type = type;
    editor->tracks = cmd_type_tracks(type);
    editor->sectors_per_track = cmd_type_sectors(type);
    editor->owns_data = true;
    
    return 0;
}

int cmd_create(cmd_image_type_t type, cmd_editor_t *editor)
{
    if (!editor) return -1;
    
    size_t size = cmd_type_size(type);
    if (size == 0) return -2;
    
    memset(editor, 0, sizeof(cmd_editor_t));
    
    editor->data = calloc(1, size);
    if (!editor->data) return -3;
    
    editor->size = size;
    editor->type = type;
    editor->tracks = cmd_type_tracks(type);
    editor->sectors_per_track = cmd_type_sectors(type);
    editor->owns_data = true;
    
    return 0;
}

int cmd_format(cmd_editor_t *editor, const char *name, const char *id)
{
    if (!editor || !editor->data) return -1;
    
    /* Clear entire image */
    memset(editor->data, 0, editor->size);
    
    /* Setup BAM at track 1, sector 0 */
    uint8_t *bam = editor->data;
    
    /* Directory track/sector */
    bam[0] = CMD_DIR_TRACK;
    bam[1] = CMD_DIR_SECTOR;
    
    /* DOS version */
    bam[2] = '3';  /* CMD DOS 3.x */
    bam[3] = 0x00;
    
    /* Set disk name */
    for (int i = 0; i < 16; i++) {
        if (name && name[i] && name[i] != '\0') {
            char c = name[i];
            if (c >= 'a' && c <= 'z') c -= 0x20;
            bam[4 + i] = c;
        } else {
            bam[4 + i] = 0xA0;  /* Shifted space */
        }
    }
    
    /* Disk ID */
    bam[20] = 0xA0;
    bam[21] = (id && id[0]) ? id[0] : '0';
    bam[22] = (id && id[1]) ? id[1] : '0';
    bam[23] = 0xA0;
    
    /* DOS type */
    bam[24] = '3';
    bam[25] = 'D';
    bam[26] = 0xA0;
    bam[27] = 0xA0;
    
    /* Initialize BAM - mark all blocks as free */
    int bam_offset = 32;
    for (int t = 1; t <= editor->tracks; t++) {
        bam[bam_offset] = editor->sectors_per_track;  /* Free count */
        bam[bam_offset + 1] = 0xFF;
        bam[bam_offset + 2] = 0xFF;
        if (editor->sectors_per_track > 16) {
            bam[bam_offset + 3] = 0x0F;  /* 20 sectors */
        } else {
            bam[bam_offset + 3] = 0x03;  /* 10 sectors */
        }
        bam_offset += 4;
    }
    
    /* Allocate BAM sector itself (track 1, sector 0) */
    cmd_allocate_block(editor, 1, 0);
    
    /* Allocate directory sector (track 1, sector 1) */
    cmd_allocate_block(editor, 1, 1);
    
    /* Setup empty directory at track 1, sector 1 */
    int dir_offset = CMD_SECTOR_SIZE;  /* Second sector */
    editor->data[dir_offset] = 0x00;    /* No next track */
    editor->data[dir_offset + 1] = 0xFF; /* End marker */
    
    editor->modified = true;
    
    return 0;
}

void cmd_editor_close(cmd_editor_t *editor)
{
    if (!editor) return;
    
    if (editor->owns_data) {
        free(editor->data);
    }
    
    memset(editor, 0, sizeof(cmd_editor_t));
}

int cmd_get_info(const cmd_editor_t *editor, cmd_disk_info_t *info)
{
    if (!editor || !info) return -1;
    
    memset(info, 0, sizeof(cmd_disk_info_t));
    
    info->type = editor->type;
    info->total_tracks = editor->tracks;
    info->sectors_per_track = editor->sectors_per_track;
    info->total_size = editor->size;
    
    /* Read disk name from BAM */
    for (int i = 0; i < 16; i++) {
        uint8_t c = editor->data[4 + i];
        if (c == 0xA0) break;
        info->disk_name[i] = c;
    }
    
    /* Disk ID */
    info->disk_id[0] = editor->data[21];
    info->disk_id[1] = editor->data[22];
    
    info->free_blocks = cmd_get_free_blocks(editor);
    info->used_blocks = (editor->tracks * editor->sectors_per_track) - info->free_blocks;
    
    return 0;
}

/* ============================================================================
 * Sector Operations
 * ============================================================================ */

int cmd_sector_offset(const cmd_editor_t *editor, int track, int sector)
{
    if (!editor) return -1;
    if (track < 1 || track > editor->tracks) return -1;
    if (sector < 0 || sector >= editor->sectors_per_track) return -1;
    
    /* Linear mapping: (track-1) * sectors + sector */
    return ((track - 1) * editor->sectors_per_track + sector) * CMD_SECTOR_SIZE;
}

int cmd_read_sector(const cmd_editor_t *editor, int track, int sector,
                    uint8_t *buffer)
{
    if (!buffer) return -1;
    
    int offset = cmd_sector_offset(editor, track, sector);
    if (offset < 0 || (size_t)(offset + CMD_SECTOR_SIZE) > editor->size) {
        return -2;
    }
    
    memcpy(buffer, editor->data + offset, CMD_SECTOR_SIZE);
    return 0;
}

int cmd_write_sector(cmd_editor_t *editor, int track, int sector,
                     const uint8_t *buffer)
{
    if (!buffer) return -1;
    
    int offset = cmd_sector_offset(editor, track, sector);
    if (offset < 0 || (size_t)(offset + CMD_SECTOR_SIZE) > editor->size) {
        return -2;
    }
    
    memcpy(editor->data + offset, buffer, CMD_SECTOR_SIZE);
    editor->modified = true;
    return 0;
}

/* ============================================================================
 * BAM Operations
 * ============================================================================ */

static int bam_entry_offset(const cmd_editor_t *editor, int track)
{
    if (track < 1 || track > editor->tracks) return -1;
    return 32 + (track - 1) * 4;
}

bool cmd_is_block_free(const cmd_editor_t *editor, int track, int sector)
{
    if (!editor || !editor->data) return false;
    
    int offset = bam_entry_offset(editor, track);
    if (offset < 0) return false;
    
    int byte_idx = sector / 8;
    int bit_idx = sector % 8;
    
    return (editor->data[offset + 1 + byte_idx] & (1 << bit_idx)) != 0;
}

int cmd_allocate_block(cmd_editor_t *editor, int track, int sector)
{
    if (!editor || !editor->data) return -1;
    
    int offset = bam_entry_offset(editor, track);
    if (offset < 0) return -2;
    
    int byte_idx = sector / 8;
    int bit_idx = sector % 8;
    
    /* Check if already allocated */
    if (!(editor->data[offset + 1 + byte_idx] & (1 << bit_idx))) {
        return 0;  /* Already allocated */
    }
    
    /* Clear bit (allocate) */
    editor->data[offset + 1 + byte_idx] &= ~(1 << bit_idx);
    
    /* Decrement free count */
    if (editor->data[offset] > 0) {
        editor->data[offset]--;
    }
    
    editor->modified = true;
    return 0;
}

int cmd_free_block(cmd_editor_t *editor, int track, int sector)
{
    if (!editor || !editor->data) return -1;
    
    int offset = bam_entry_offset(editor, track);
    if (offset < 0) return -2;
    
    int byte_idx = sector / 8;
    int bit_idx = sector % 8;
    
    /* Set bit (free) */
    editor->data[offset + 1 + byte_idx] |= (1 << bit_idx);
    
    /* Increment free count */
    editor->data[offset]++;
    
    editor->modified = true;
    return 0;
}

int cmd_get_free_blocks(const cmd_editor_t *editor)
{
    if (!editor || !editor->data) return 0;
    
    int free_count = 0;
    for (int t = 1; t <= editor->tracks; t++) {
        int offset = bam_entry_offset(editor, t);
        if (offset >= 0) {
            free_count += editor->data[offset];
        }
    }
    
    return free_count;
}

/* ============================================================================
 * Directory Operations
 * ============================================================================ */

int cmd_get_dir_entry_count(const cmd_editor_t *editor)
{
    if (!editor || !editor->data) return 0;
    
    int count = 0;
    int track = CMD_DIR_TRACK;
    int sector = CMD_DIR_SECTOR;
    
    while (track != 0) {
        int offset = cmd_sector_offset(editor, track, sector);
        if (offset < 0) break;
        
        /* 8 entries per sector */
        for (int i = 0; i < 8; i++) {
            int entry_offset = offset + (i * 32);
            uint8_t type = editor->data[entry_offset + 2];
            if (type != 0) {
                count++;
            }
        }
        
        track = editor->data[offset];
        sector = editor->data[offset + 1];
        
        if (sector == 0xFF) break;
    }
    
    return count;
}

int cmd_get_dir_entry(const cmd_editor_t *editor, int index,
                      cmd_dir_entry_t *entry)
{
    if (!editor || !entry) return -1;
    
    int current = 0;
    int track = CMD_DIR_TRACK;
    int sector = CMD_DIR_SECTOR;
    
    while (track != 0) {
        int offset = cmd_sector_offset(editor, track, sector);
        if (offset < 0) return -2;
        
        for (int i = 0; i < 8; i++) {
            int entry_offset = offset + (i * 32);
            uint8_t type = editor->data[entry_offset + 2];
            
            if (type != 0) {
                if (current == index) {
                    memcpy(entry, editor->data + entry_offset, sizeof(cmd_dir_entry_t));
                    return 0;
                }
                current++;
            }
        }
        
        track = editor->data[offset];
        sector = editor->data[offset + 1];
        
        if (sector == 0xFF) break;
    }
    
    return -1;  /* Not found */
}

void cmd_print_directory(const cmd_editor_t *editor, FILE *fp)
{
    if (!editor || !fp) return;
    
    cmd_disk_info_t info;
    cmd_get_info(editor, &info);
    
    fprintf(fp, "0 \"%-16s\" %s\n", info.disk_name, info.disk_id);
    
    int count = cmd_get_dir_entry_count(editor);
    for (int i = 0; i < count; i++) {
        cmd_dir_entry_t entry;
        if (cmd_get_dir_entry(editor, i, &entry) == 0) {
            char name[17] = {0};
            for (int j = 0; j < 16; j++) {
                uint8_t c = entry.filename[j];
                if (c == 0xA0) break;
                name[j] = c;
            }
            
            const char *type_str = "???";
            uint8_t type = entry.file_type & 0x07;
            switch (type) {
                case 0: type_str = "DEL"; break;
                case 1: type_str = "SEQ"; break;
                case 2: type_str = "PRG"; break;
                case 3: type_str = "USR"; break;
                case 4: type_str = "REL"; break;
            }
            
            fprintf(fp, "%-5d \"%s\" %s\n", entry.blocks, name, type_str);
        }
    }
    
    fprintf(fp, "%d BLOCKS FREE.\n", info.free_blocks);
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void cmd_print_info(const cmd_editor_t *editor, FILE *fp)
{
    if (!editor || !fp) return;
    
    cmd_disk_info_t info;
    cmd_get_info(editor, &info);
    
    fprintf(fp, "CMD Disk Image:\n");
    fprintf(fp, "  Type: %s\n", cmd_type_name(info.type));
    fprintf(fp, "  Disk Name: \"%s\"\n", info.disk_name);
    fprintf(fp, "  Disk ID: %s\n", info.disk_id);
    fprintf(fp, "  Tracks: %d\n", info.total_tracks);
    fprintf(fp, "  Sectors/Track: %d\n", info.sectors_per_track);
    fprintf(fp, "  Total Size: %zu bytes\n", info.total_size);
    fprintf(fp, "  Free Blocks: %d\n", info.free_blocks);
    fprintf(fp, "  Used Blocks: %d\n", info.used_blocks);
}

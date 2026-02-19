/**
 * @file uft_ps1.c
 * @brief PlayStation 1 Disc Image Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/sony/uft_ps1.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * Detection
 * ============================================================================ */

ps1_image_type_t ps1_detect_type(const uint8_t *data, size_t size)
{
    if (!data || size < 2352) return PS1_IMAGE_UNKNOWN;
    
    /* Check for raw sector sync pattern */
    if (memcmp(data, PS1_SYNC_PATTERN, 12) == 0) {
        return PS1_IMAGE_BIN;
    }
    
    /* Check for ISO9660 signature at sector 16 */
    /* In ISO format (2048 byte sectors), CD001 is at offset 16*2048+1 */
    if (size >= 16 * 2048 + 6) {
        if (memcmp(data + 16 * 2048 + 1, "CD001", 5) == 0) {
            return PS1_IMAGE_ISO;
        }
    }
    
    /* Check for ISO9660 in raw format (2352 byte sectors) */
    /* CD001 is at sector 16, offset 16 in user data area */
    if (size >= 16 * 2352 + 16 + 6) {
        if (memcmp(data + 16 * 2352 + 16 + 1, "CD001", 5) == 0) {
            return PS1_IMAGE_BIN;
        }
    }
    
    return PS1_IMAGE_UNKNOWN;
}

int ps1_detect_sector_size(const uint8_t *data, size_t size)
{
    if (!data || size < 2352) return 0;
    
    /* Check for sync pattern indicating raw sectors */
    if (memcmp(data, PS1_SYNC_PATTERN, 12) == 0) {
        return PS1_SECTOR_RAW;
    }
    
    /* Check for ISO9660 at expected offset for 2048 */
    if (size >= 16 * 2048 + 6) {
        if (memcmp(data + 16 * 2048 + 1, "CD001", 5) == 0) {
            return PS1_SECTOR_MODE1;
        }
    }
    
    /* Default to raw if file size is multiple of 2352 */
    if (size % PS1_SECTOR_RAW == 0) {
        return PS1_SECTOR_RAW;
    }
    
    return PS1_SECTOR_MODE1;
}

const char *ps1_type_name(ps1_image_type_t type)
{
    switch (type) {
        case PS1_IMAGE_ISO:  return "ISO (2048-byte sectors)";
        case PS1_IMAGE_BIN:  return "BIN (2352-byte raw sectors)";
        case PS1_IMAGE_IMG:  return "IMG (raw sectors)";
        case PS1_IMAGE_MDF:  return "MDF (Alcohol 120%)";
        case PS1_IMAGE_ECM:  return "ECM (compressed)";
        default:             return "Unknown";
    }
}

const char *ps1_region_name(ps1_region_t region)
{
    switch (region) {
        case PS1_REGION_NTSC_J: return "NTSC-J (Japan)";
        case PS1_REGION_NTSC_U: return "NTSC-U (USA)";
        case PS1_REGION_PAL:    return "PAL (Europe)";
        default:                return "Unknown";
    }
}

bool ps1_validate(const uint8_t *data, size_t size)
{
    if (!data) return false;
    
    ps1_image_type_t type = ps1_detect_type(data, size);
    return type != PS1_IMAGE_UNKNOWN;
}

/* ============================================================================
 * Image Operations
 * ============================================================================ */

int ps1_open(const uint8_t *data, size_t size, ps1_image_t *image)
{
    if (!data || !image || size < 2048) return -1;
    
    memset(image, 0, sizeof(ps1_image_t));
    
    image->type = ps1_detect_type(data, size);
    if (image->type == PS1_IMAGE_UNKNOWN) return -2;
    
    image->data = malloc(size);
    if (!image->data) return -3;
    
    memcpy(image->data, data, size);
    image->size = size;
    image->owns_data = true;
    
    image->sector_size = ps1_detect_sector_size(data, size);
    image->num_sectors = size / image->sector_size;
    
    /* Try to parse game info */
    ps1_parse_system_cnf(image, &image->game);
    
    return 0;
}

int ps1_load(const char *filename, ps1_image_t *image)
{
    if (!filename || !image) return -1;
    
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
    
    int result = ps1_open(data, size, image);
    free(data);
    
    return result;
}

void ps1_close(ps1_image_t *image)
{
    if (!image) return;
    
    if (image->owns_data) {
        free(image->data);
    }
    
    memset(image, 0, sizeof(ps1_image_t));
}

int ps1_get_info(const ps1_image_t *image, ps1_info_t *info)
{
    if (!image || !info) return -1;
    
    memset(info, 0, sizeof(ps1_info_t));
    
    info->type = image->type;
    info->file_size = image->size;
    info->num_sectors = image->num_sectors;
    info->sector_size = image->sector_size;
    info->num_tracks = (image->cue.num_tracks > 0) ? image->cue.num_tracks : 1;
    info->game = image->game;
    
    return 0;
}

/* ============================================================================
 * Sector Access
 * ============================================================================ */

int ps1_read_sector(const ps1_image_t *image, uint32_t lba,
                    uint8_t *buffer, bool user_data_only)
{
    if (!image || !buffer) return -1;
    if (lba >= image->num_sectors) return -2;
    
    size_t offset = (size_t)lba * image->sector_size;
    
    if (user_data_only && image->sector_size == PS1_SECTOR_RAW) {
        /* Extract user data from raw sector */
        /* Skip 16 bytes header (12 sync + 4 header) */
        memcpy(buffer, image->data + offset + 16, PS1_SECTOR_MODE1);
        return PS1_SECTOR_MODE1;
    } else if (user_data_only && image->sector_size == PS1_SECTOR_MODE1) {
        memcpy(buffer, image->data + offset, PS1_SECTOR_MODE1);
        return PS1_SECTOR_MODE1;
    } else {
        memcpy(buffer, image->data + offset, image->sector_size);
        return image->sector_size;
    }
}

int ps1_read_sectors(const ps1_image_t *image, uint32_t start_lba,
                     uint32_t count, uint8_t *buffer, bool user_data_only)
{
    if (!image || !buffer) return -1;
    
    int total = 0;
    int data_size = user_data_only ? PS1_SECTOR_MODE1 : image->sector_size;
    
    for (uint32_t i = 0; i < count; i++) {
        int result = ps1_read_sector(image, start_lba + i,
                                     buffer + i * data_size, user_data_only);
        if (result < 0) break;
        total += result;
    }
    
    return total;
}

void ps1_lba_to_msf(uint32_t lba, ps1_msf_t *msf)
{
    if (!msf) return;
    
    /* Add 2 seconds (150 frames) for lead-in */
    lba += 150;
    
    msf->frames = lba % 75;
    lba /= 75;
    msf->seconds = lba % 60;
    msf->minutes = lba / 60;
}

uint32_t ps1_msf_to_lba(const ps1_msf_t *msf)
{
    if (!msf) return 0;
    
    uint32_t lba = (msf->minutes * 60 + msf->seconds) * 75 + msf->frames;
    return lba - 150;  /* Subtract lead-in */
}

/* ============================================================================
 * Game Info
 * ============================================================================ */

int ps1_parse_system_cnf(const ps1_image_t *image, ps1_game_info_t *game)
{
    if (!image || !game) return -1;
    
    memset(game, 0, sizeof(ps1_game_info_t));
    
    /* SYSTEM.CNF is typically in root directory */
    /* We need to find and parse it from ISO9660 */
    
    /* For now, try to read sector 24 which often contains SYSTEM.CNF */
    uint8_t sector[2048];
    if (ps1_read_sector(image, 24, sector, true) < 0) {
        return -2;
    }
    
    /* Look for "BOOT" keyword */
    char *boot = NULL;
    for (int i = 0; i < 2048 - 4; i++) {
        if (memcmp(sector + i, "BOOT", 4) == 0) {
            boot = (char *)sector + i;
            break;
        }
    }
    
    if (!boot) {
        /* Try reading root directory to find SYSTEM.CNF */
        return -3;
    }
    
    /* Parse BOOT = cdrom:\XXXX_XXX.XX;1 */
    char *eq = strchr(boot, '=');
    if (eq) {
        char *start = eq + 1;
        while (*start == ' ' || *start == '\t') start++;
        
        /* Copy boot file */
        int j = 0;
        while (start[j] && start[j] != '\r' && start[j] != '\n' && j < 63) {
            game->boot_file[j] = start[j];
            j++;
        }
        game->boot_file[j] = '\0';
        
        /* Extract game ID from boot file (e.g., SLUS_012.34 -> SLUS-01234) */
        char *id_start = strrchr(game->boot_file, '\\');
        if (!id_start) id_start = strrchr(game->boot_file, '/');
        if (!id_start) id_start = game->boot_file;
        else id_start++;
        
        /* Copy and format game ID */
        j = 0;
        for (int i = 0; id_start[i] && j < 15; i++) {
            char c = id_start[i];
            if (c == '_') c = '-';
            if (c == '.' || c == ';') break;
            if (isalnum(c) || c == '-') {
                game->game_id[j++] = toupper(c);
            }
        }
        game->game_id[j] = '\0';
        
        /* Detect region from game ID */
        game->region = ps1_detect_region(game->game_id);
    }
    
    return 0;
}

ps1_region_t ps1_detect_region(const char *game_id)
{
    if (!game_id || strlen(game_id) < 4) return PS1_REGION_UNKNOWN;
    
    /* Japan: SCPS, SLPS, SLPM, PCPX */
    if (strncmp(game_id, "SCPS", 4) == 0 ||
        strncmp(game_id, "SLPS", 4) == 0 ||
        strncmp(game_id, "SLPM", 4) == 0 ||
        strncmp(game_id, "PCPX", 4) == 0) {
        return PS1_REGION_NTSC_J;
    }
    
    /* USA: SCUS, SLUS, PAPX */
    if (strncmp(game_id, "SCUS", 4) == 0 ||
        strncmp(game_id, "SLUS", 4) == 0 ||
        strncmp(game_id, "PAPX", 4) == 0) {
        return PS1_REGION_NTSC_U;
    }
    
    /* Europe: SCES, SLES, PEPX */
    if (strncmp(game_id, "SCES", 4) == 0 ||
        strncmp(game_id, "SLES", 4) == 0 ||
        strncmp(game_id, "PEPX", 4) == 0) {
        return PS1_REGION_PAL;
    }
    
    return PS1_REGION_UNKNOWN;
}

int ps1_get_game_id(const ps1_image_t *image, char *game_id)
{
    if (!image || !game_id) return -1;
    
    if (image->game.game_id[0]) {
        strncpy(game_id, image->game.game_id, 15); game_id[15] = '\0';
        return 0;
    }
    
    /* Try to parse if not cached */
    ps1_game_info_t game;
    if (ps1_parse_system_cnf(image, &game) == 0 && game.game_id[0]) {
        strncpy(game_id, game.game_id, 15); game_id[15] = '\0';
        return 0;
    }
    
    game_id[0] = '\0';
    return -2;
}

/* ============================================================================
 * CUE Sheet
 * ============================================================================ */

int ps1_parse_cue(const char *cue_data, size_t cue_size, ps1_cue_t *cue)
{
    if (!cue_data || !cue) return -1;
    
    memset(cue, 0, sizeof(ps1_cue_t));
    
    /* Simple CUE parser */
    const char *p = cue_data;
    const char *end = cue_data + cue_size;
    
    while (p < end) {
        /* Skip whitespace */
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
        if (p >= end) break;
        
        /* Parse FILE command */
        if (strncmp(p, "FILE", 4) == 0) {
            p += 4;
            while (p < end && (*p == ' ' || *p == '"')) p++;
            
            int i = 0;
            while (p < end && *p != '"' && *p != '\r' && *p != '\n' && i < 255) {
                cue->filename[i++] = *p++;
            }
            cue->filename[i] = '\0';
        }
        
        /* Parse TRACK command */
        else if (strncmp(p, "TRACK", 5) == 0) {
            p += 5;
            while (p < end && *p == ' ') p++;
            
            int track_num = (int)strtol(p, NULL, 10);
            if (track_num > 0 && track_num <= 99) {
                cue->tracks[cue->num_tracks].number = track_num;
                
                /* Find track type */
                while (p < end && *p != ' ' && *p != '\r' && *p != '\n') p++;
                while (p < end && *p == ' ') p++;
                
                if (strncmp(p, "MODE1", 5) == 0) {
                    cue->tracks[cue->num_tracks].type = PS1_TRACK_MODE1;
                    cue->tracks[cue->num_tracks].sector_size = PS1_SECTOR_RAW;
                } else if (strncmp(p, "MODE2", 5) == 0) {
                    cue->tracks[cue->num_tracks].type = PS1_TRACK_MODE2_RAW;
                    cue->tracks[cue->num_tracks].sector_size = PS1_SECTOR_RAW;
                } else if (strncmp(p, "AUDIO", 5) == 0) {
                    cue->tracks[cue->num_tracks].type = PS1_TRACK_AUDIO;
                    cue->tracks[cue->num_tracks].sector_size = PS1_SECTOR_AUDIO;
                }
                
                cue->num_tracks++;
            }
        }
        
        /* Parse INDEX command */
        else if (strncmp(p, "INDEX", 5) == 0 && cue->num_tracks > 0) {
            p += 5;
            while (p < end && *p == ' ') p++;
            
            int index_num = (int)strtol(p, NULL, 10);
            while (p < end && *p != ' ') p++;
            while (p < end && *p == ' ') p++;
            
            /* Parse MSF time MM:SS:FF */
            int m = 0, s = 0, f = 0;
            sscanf(p, "%d:%d:%d", &m, &s, &f);
            
            if (index_num == 1) {
                ps1_track_t *track = &cue->tracks[cue->num_tracks - 1];
                track->start_msf.minutes = m;
                track->start_msf.seconds = s;
                track->start_msf.frames = f;
                track->start_lba = ps1_msf_to_lba(&track->start_msf);
            }
        }
        
        /* Skip to next line */
        while (p < end && *p != '\n') p++;
    }
    
    return 0;
}

int ps1_get_track(const ps1_image_t *image, int track_num, ps1_track_t *track)
{
    if (!image || !track || track_num < 1) return -1;
    
    if (image->cue.num_tracks > 0) {
        for (int i = 0; i < image->cue.num_tracks; i++) {
            if (image->cue.tracks[i].number == track_num) {
                *track = image->cue.tracks[i];
                return 0;
            }
        }
    }
    
    /* Default single data track */
    if (track_num == 1) {
        memset(track, 0, sizeof(ps1_track_t));
        track->number = 1;
        track->type = PS1_TRACK_MODE2_RAW;
        track->start_lba = 0;
        track->length = image->num_sectors;
        track->sector_size = image->sector_size;
        return 0;
    }
    
    return -2;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void ps1_print_info(const ps1_image_t *image, FILE *fp)
{
    if (!image || !fp) return;
    
    ps1_info_t info;
    ps1_get_info(image, &info);
    
    fprintf(fp, "PlayStation 1 Disc Image:\n");
    fprintf(fp, "  Type: %s\n", ps1_type_name(info.type));
    fprintf(fp, "  File Size: %zu bytes (%.1f MB)\n", 
            info.file_size, info.file_size / 1048576.0);
    fprintf(fp, "  Sectors: %u\n", info.num_sectors);
    fprintf(fp, "  Sector Size: %d bytes\n", info.sector_size);
    fprintf(fp, "  Tracks: %d\n", info.num_tracks);
    
    if (info.game.game_id[0]) {
        fprintf(fp, "\nGame Info:\n");
        fprintf(fp, "  Game ID: %s\n", info.game.game_id);
        fprintf(fp, "  Region: %s\n", ps1_region_name(info.game.region));
        if (info.game.boot_file[0]) {
            fprintf(fp, "  Boot File: %s\n", info.game.boot_file);
        }
    }
}

void ps1_print_tracks(const ps1_image_t *image, FILE *fp)
{
    if (!image || !fp) return;
    
    fprintf(fp, "Track List:\n");
    
    int num_tracks = (image->cue.num_tracks > 0) ? image->cue.num_tracks : 1;
    
    for (int i = 0; i < num_tracks; i++) {
        ps1_track_t track;
        if (ps1_get_track(image, i + 1, &track) == 0) {
            const char *type_str = "Data";
            if (track.type == PS1_TRACK_AUDIO) type_str = "Audio";
            
            fprintf(fp, "  Track %02d: %s, LBA %u, %02d:%02d:%02d\n",
                    track.number, type_str, track.start_lba,
                    track.start_msf.minutes, track.start_msf.seconds,
                    track.start_msf.frames);
        }
    }
}

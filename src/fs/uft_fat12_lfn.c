/**
 * @file uft_fat12_lfn.c
 * @brief FAT12/FAT16 Long Filename Support
 * @version 3.7.0
 * 
 * LFN generation, checksum, SFN conversion
 */

#include "uft/fs/uft_fat12.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*===========================================================================
 * Character Classification
 *===========================================================================*/

/** Characters invalid in short filenames */
static const char sfn_invalid[] = "\"*+,/:;<=>?[\\]|";

/** Characters that require LFN */
static const char lfn_trigger[] = " +,;=[]";

/**
 * @brief Check if character is valid in SFN
 */
static bool is_valid_sfn_char(unsigned char c) {
    if (c < 0x20) return false;
    if (c == 0x7F) return false;
    if (strchr(sfn_invalid, c)) return false;
    return true;
}

/**
 * @brief Check if character triggers LFN requirement
 */
static bool triggers_lfn(unsigned char c) {
    if (islower(c)) return true;  /* Lowercase requires LFN */
    if (strchr(lfn_trigger, c)) return true;
    return false;
}

/*===========================================================================
 * LFN Detection
 *===========================================================================*/

bool uft_fat_needs_lfn(const char *name) {
    if (!name) return false;
    
    size_t len = strlen(name);
    
    /* Too long for 8.3 */
    if (len > 12) return true;
    
    /* Find extension */
    const char *dot = strrchr(name, '.');
    size_t base_len, ext_len;
    
    if (dot == NULL) {
        base_len = len;
        ext_len = 0;
    } else if (dot == name) {
        /* Starts with dot - treat as base name */
        base_len = len;
        ext_len = 0;
    } else {
        base_len = dot - name;
        ext_len = len - base_len - 1;
    }
    
    /* Check lengths */
    if (base_len > 8) return true;
    if (ext_len > 3) return true;
    
    /* Multiple dots */
    int dots = 0;
    for (const char *p = name; *p; p++) {
        if (*p == '.') dots++;
    }
    if (dots > 1) return true;
    
    /* Check characters */
    for (const char *p = name; *p; p++) {
        if (triggers_lfn((unsigned char)*p)) return true;
        if (!is_valid_sfn_char((unsigned char)*p)) return true;
    }
    
    return false;
}

/*===========================================================================
 * LFN Checksum
 *===========================================================================*/

uint8_t uft_fat_lfn_checksum(const char sfn[11]) {
    uint8_t sum = 0;
    
    for (int i = 0; i < 11; i++) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + (uint8_t)sfn[i];
    }
    
    return sum;
}

/*===========================================================================
 * SFN Generation
 *===========================================================================*/

/**
 * @brief Convert character for SFN
 */
static char sfn_char(char c) {
    if (!is_valid_sfn_char((unsigned char)c)) {
        return '_';
    }
    return toupper((unsigned char)c);
}

int uft_fat_lfn_to_sfn(const char *lfn, char sfn[11], char sfn_display[13]) {
    if (!lfn || !sfn) return UFT_FAT_ERR_INVALID;
    
    /* Initialize with spaces */
    memset(sfn, ' ', 11);
    
    /* Skip leading dots and spaces */
    while (*lfn == '.' || *lfn == ' ') lfn++;
    
    if (*lfn == '\0') {
        return UFT_FAT_ERR_INVALID;
    }
    
    /* Find last dot (extension separator) */
    const char *ext_start = NULL;
    const char *last_dot = strrchr(lfn, '.');
    
    if (last_dot && last_dot != lfn) {
        ext_start = last_dot + 1;
    }
    
    /* Build base name (up to 8 chars) */
    int base_pos = 0;
    const char *p = lfn;
    const char *end = ext_start ? (last_dot) : (lfn + strlen(lfn));
    
    while (p < end && base_pos < 8) {
        char c = *p++;
        if (c == ' ' || c == '.') continue;  /* Skip spaces/dots in base */
        sfn[base_pos++] = sfn_char(c);
    }
    
    /* Build extension (up to 3 chars) */
    if (ext_start) {
        int ext_pos = 8;
        p = ext_start;
        while (*p && ext_pos < 11) {
            char c = *p++;
            if (c == ' ' || c == '.') continue;
            sfn[ext_pos++] = sfn_char(c);
        }
    }
    
    /* Generate display form (8.3 with dot) */
    if (sfn_display) {
        int i, j = 0;
        
        /* Base name */
        for (i = 0; i < 8 && sfn[i] != ' '; i++) {
            sfn_display[j++] = sfn[i];
        }
        
        /* Extension */
        if (sfn[8] != ' ') {
            sfn_display[j++] = '.';
            for (i = 8; i < 11 && sfn[i] != ' '; i++) {
                sfn_display[j++] = sfn[i];
            }
        }
        
        sfn_display[j] = '\0';
    }
    
    return 0;
}

/*===========================================================================
 * Unique SFN Generation
 *===========================================================================*/

/**
 * @brief Generate numeric tail for SFN collision avoidance
 */
static void add_numeric_tail(char sfn[11], int num) {
    char tail[8];
    snprintf(tail, sizeof(tail), "~%d", num);
    size_t tail_len = strlen(tail);
    
    /* Find end of base name */
    int base_end = 8;
    while (base_end > 0 && sfn[base_end - 1] == ' ') base_end--;
    
    /* Truncate base to fit tail */
    if (base_end + tail_len > 8) {
        base_end = 8 - tail_len;
    }
    
    /* Insert tail */
    memcpy(sfn + base_end, tail, tail_len);
}

int uft_fat_generate_sfn(const uft_fat_ctx_t *ctx, uint32_t dir_cluster,
                         const char *lfn, char sfn[11]) {
    if (!ctx || !lfn || !sfn) return UFT_FAT_ERR_INVALID;
    
    /* Generate base SFN */
    char display[13];
    int rc = uft_fat_lfn_to_sfn(lfn, sfn, display);
    if (rc != 0) return rc;
    
    /* Check for collision */
    uft_fat_entry_t entry;
    if (uft_fat_find_entry(ctx, dir_cluster, display, &entry) != 0) {
        /* No collision */
        return 0;
    }
    
    /* Generate unique name with numeric tail */
    char base_sfn[11];
    memcpy(base_sfn, sfn, 11);
    
    for (int i = 1; i < 1000000; i++) {
        memcpy(sfn, base_sfn, 11);
        add_numeric_tail(sfn, i);
        
        /* Build display form */
        int j = 0;
        for (int k = 0; k < 8 && sfn[k] != ' '; k++) {
            display[j++] = sfn[k];
        }
        if (sfn[8] != ' ') {
            display[j++] = '.';
            for (int k = 8; k < 11 && sfn[k] != ' '; k++) {
                display[j++] = sfn[k];
            }
        }
        display[j] = '\0';
        
        if (uft_fat_find_entry(ctx, dir_cluster, display, &entry) != 0) {
            /* Found unique name */
            return 0;
        }
    }
    
    return UFT_FAT_ERR_INVALID;  /* Couldn't find unique name */
}

/*===========================================================================
 * LFN Entry Creation (helpers for file injection with LFN)
 *===========================================================================*/

/**
 * @brief Convert UTF-8 to UCS-2
 * @return Number of UCS-2 characters produced
 */
static size_t utf8_to_ucs2(const char *src, uint16_t *dst, size_t dst_len) {
    size_t out = 0;
    const unsigned char *p = (const unsigned char *)src;
    
    while (*p && out < dst_len) {
        uint32_t code;
        
        if (*p < 0x80) {
            code = *p++;
        } else if ((*p & 0xE0) == 0xC0) {
            code = (*p++ & 0x1F) << 6;
            if ((*p & 0xC0) == 0x80) code |= (*p++ & 0x3F);
        } else if ((*p & 0xF0) == 0xE0) {
            code = (*p++ & 0x0F) << 12;
            if ((*p & 0xC0) == 0x80) code |= (*p++ & 0x3F) << 6;
            if ((*p & 0xC0) == 0x80) code |= (*p++ & 0x3F);
        } else if ((*p & 0xF8) == 0xF0) {
            /* 4-byte UTF-8 (surrogate pair needed) - skip for now */
            p += 4;
            dst[out++] = 0xFFFD;  /* Replacement char */
            continue;
        } else {
            p++;  /* Invalid, skip */
            continue;
        }
        
        if (code <= 0xFFFF) {
            dst[out++] = (uint16_t)code;
        } else {
            dst[out++] = 0xFFFD;  /* Replacement char */
        }
    }
    
    return out;
}

/**
 * @brief Calculate number of LFN entries needed
 */
size_t uft_fat_lfn_entries_needed(const char *lfn) {
    if (!lfn) return 0;
    
    /* Convert to UCS-2 to count actual characters */
    uint16_t ucs2[256];
    size_t len = utf8_to_ucs2(lfn, ucs2, 255);
    
    /* 13 characters per LFN entry */
    return (len + 12) / 13;
}

/**
 * @brief Build LFN directory entries
 * @param lfn Long filename (UTF-8)
 * @param sfn Short filename (11 bytes)
 * @param entries Output buffer for LFN entries
 * @param max_entries Maximum entries to generate
 * @return Number of LFN entries created
 */
size_t uft_fat_build_lfn_entries(const char *lfn, const char sfn[11],
                                  uint8_t *entries, size_t max_entries) {
    if (!lfn || !sfn || !entries) return 0;
    
    /* Convert LFN to UCS-2 */
    uint16_t ucs2[256];
    size_t lfn_len = utf8_to_ucs2(lfn, ucs2, 255);
    
    /* Pad with 0x0000 and 0xFFFF */
    ucs2[lfn_len] = 0x0000;  /* Null terminator */
    for (size_t i = lfn_len + 1; i < 256; i++) {
        ucs2[i] = 0xFFFF;  /* Padding */
    }
    
    /* Calculate number of entries */
    size_t num_entries = (lfn_len + 12) / 13;
    if (num_entries > max_entries) num_entries = max_entries;
    if (num_entries == 0) return 0;
    
    /* Calculate checksum */
    uint8_t checksum = uft_fat_lfn_checksum(sfn);
    
    /* Build entries (in reverse order: last entry has highest sequence) */
    for (size_t i = 0; i < num_entries; i++) {
        uint8_t *entry = entries + (num_entries - 1 - i) * 32;
        uft_fat_lfn_t *lfn_entry = (uft_fat_lfn_t *)entry;
        
        /* Sequence number */
        uint8_t seq = (uint8_t)(i + 1);
        if (i == num_entries - 1) {
            seq |= UFT_FAT_LFN_LAST;  /* Mark last (first physical) entry */
        }
        lfn_entry->sequence = seq;
        
        /* Attributes */
        lfn_entry->attributes = UFT_FAT_ATTR_LFN;
        lfn_entry->type = 0;
        lfn_entry->checksum = checksum;
        lfn_entry->cluster = 0;
        
        /* Characters (13 per entry) */
        size_t offset = i * 13;
        
        /* name1: chars 1-5 */
        for (int j = 0; j < 5; j++) {
            lfn_entry->name1[j] = ucs2[offset + j];
        }
        
        /* name2: chars 6-11 */
        for (int j = 0; j < 6; j++) {
            lfn_entry->name2[j] = ucs2[offset + 5 + j];
        }
        
        /* name3: chars 12-13 */
        for (int j = 0; j < 2; j++) {
            lfn_entry->name3[j] = ucs2[offset + 11 + j];
        }
    }
    
    return num_entries;
}

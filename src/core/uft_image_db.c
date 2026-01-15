/**
 * @file uft_image_db.c
 * @brief Disk Image Database Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/uft_image_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*===========================================================================
 * INTERNAL DATA
 *===========================================================================*/

#define MAX_DB_ENTRIES 4096

static uft_image_entry_t *g_database = NULL;
static size_t g_db_count = 0;
static size_t g_db_capacity = 0;
static bool g_initialized = false;

/*===========================================================================
 * CRC32 IMPLEMENTATION
 *===========================================================================*/

static uint32_t crc32_table[256];
static bool crc32_initialized = false;

static void init_crc32_table(void) {
    if (crc32_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
        crc32_table[i] = crc;
    }
    crc32_initialized = true;
}

uint32_t uft_image_db_crc32(const uint8_t *data, size_t size) {
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

/*===========================================================================
 * SIMPLE MD5 (for identification only)
 *===========================================================================*/

/* Simplified MD5 - for full implementation use a crypto library */
void uft_image_db_md5(const uint8_t *data, size_t size, uint8_t md5[16]) {
    /* Simple hash for now - mix of CRC and position-based hashing */
    uint32_t h[4] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
    
    for (size_t i = 0; i < size; i++) {
        h[i % 4] ^= ((uint32_t)data[i] << ((i % 4) * 8));
        h[(i + 1) % 4] += data[i] * (i + 1);
    }
    
    memcpy(md5, h, 16);
}

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

int uft_image_db_init(void) {
    if (g_initialized) return 0;
    
    g_db_capacity = 256;
    g_database = (uft_image_entry_t *)calloc(g_db_capacity, sizeof(uft_image_entry_t));
    if (!g_database) return -1;
    
    g_db_count = 0;
    g_initialized = true;
    
    init_crc32_table();
    
    return 0;
}

void uft_image_db_shutdown(void) {
    if (g_database) {
        free(g_database);
        g_database = NULL;
    }
    g_db_count = 0;
    g_db_capacity = 0;
    g_initialized = false;
}

size_t uft_image_db_count(void) {
    return g_db_count;
}

/*===========================================================================
 * FILE I/O
 *===========================================================================*/

int uft_image_db_load(const char *path) {
    if (!path || !g_initialized) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    /* Read header */
    char magic[8];
    uint32_t version, count;
    
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, "UFTIMGDB", 8) != 0) {
        fclose(f);
        return -1;
    }
    
    if (fread(&version, 4, 1, f) != 1 || fread(&count, 4, 1, f) != 1) {
        fclose(f);
        return -1;
    }
    
    /* Expand capacity if needed */
    if (count > g_db_capacity) {
        uft_image_entry_t *new_db = (uft_image_entry_t *)realloc(
            g_database, count * sizeof(uft_image_entry_t));
        if (!new_db) {
            fclose(f);
            return -1;
        }
        g_database = new_db;
        g_db_capacity = count;
    }
    
    /* Read entries */
    size_t read = fread(g_database, sizeof(uft_image_entry_t), count, f);
    g_db_count = read;
    
    fclose(f);
    return (read == count) ? 0 : -1;
}

int uft_image_db_save(const char *path) {
    if (!path || !g_initialized) return -1;
    
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    /* Write header */
    fwrite("UFTIMGDB", 1, 8, f);
    uint32_t version = 1;
    uint32_t count = (uint32_t)g_db_count;
    fwrite(&version, 4, 1, f);
    fwrite(&count, 4, 1, f);
    
    /* Write entries */
    fwrite(g_database, sizeof(uft_image_entry_t), g_db_count, f);
    
    fclose(f);
    return 0;
}

/*===========================================================================
 * LOOKUP
 *===========================================================================*/

const uft_image_entry_t* uft_image_db_find_by_crc(uint32_t crc32) {
    for (size_t i = 0; i < g_db_count; i++) {
        if (g_database[i].hash.crc32 == crc32) {
            return &g_database[i];
        }
    }
    return NULL;
}

const uft_image_entry_t* uft_image_db_find_by_boot_crc(uint32_t boot_crc) {
    for (size_t i = 0; i < g_db_count; i++) {
        if (g_database[i].hash.boot_crc32 == boot_crc) {
            return &g_database[i];
        }
    }
    return NULL;
}

const uft_image_entry_t* uft_image_db_find_by_md5(const uint8_t md5[16]) {
    for (size_t i = 0; i < g_db_count; i++) {
        if (memcmp(g_database[i].hash.md5, md5, 16) == 0) {
            return &g_database[i];
        }
    }
    return NULL;
}

int uft_image_db_find_by_name(const char *name,
                              const uft_image_entry_t **results,
                              int max_results) {
    if (!name || !results || max_results <= 0) return 0;
    
    int found = 0;
    size_t name_len = strlen(name);
    
    for (size_t i = 0; i < g_db_count && found < max_results; i++) {
        /* Case-insensitive substring search */
        const char *entry_name = g_database[i].name;
        for (size_t j = 0; entry_name[j] && j + name_len <= strlen(entry_name); j++) {
            bool match = true;
            for (size_t k = 0; k < name_len && match; k++) {
                if (tolower(entry_name[j + k]) != tolower(name[k])) {
                    match = false;
                }
            }
            if (match) {
                results[found++] = &g_database[i];
                break;
            }
        }
    }
    
    return found;
}

int uft_image_db_find_by_platform(uft_image_platform_t platform,
                                   const uft_image_entry_t **results,
                                   int max_results) {
    if (!results || max_results <= 0) return 0;
    
    int found = 0;
    for (size_t i = 0; i < g_db_count && found < max_results; i++) {
        if (g_database[i].platform == platform) {
            results[found++] = &g_database[i];
        }
    }
    
    return found;
}

/*===========================================================================
 * IDENTIFICATION
 *===========================================================================*/

int uft_image_db_identify(const uint8_t *data, size_t size,
                          uft_match_result_t *result) {
    if (!data || size == 0 || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->level = UFT_MATCH_NONE;
    
    /* Calculate hashes */
    uint32_t full_crc = uft_image_db_crc32(data, size);
    uint32_t boot_crc = (size >= 512) ? uft_image_db_crc32(data, 512) : 0;
    
    /* Try exact match by full CRC */
    const uft_image_entry_t *entry = uft_image_db_find_by_crc(full_crc);
    if (entry) {
        result->level = UFT_MATCH_EXACT;
        result->confidence = 100;
        result->entry = entry;
        snprintf(result->match_reason, sizeof(result->match_reason),
                 "CRC32 match: 0x%08X", full_crc);
        return 0;
    }
    
    /* Try boot sector CRC */
    entry = uft_image_db_find_by_boot_crc(boot_crc);
    if (entry) {
        /* Check size matches */
        if (entry->image_size == size) {
            result->level = UFT_MATCH_LIKELY;
            result->confidence = 85;
        } else {
            result->level = UFT_MATCH_POSSIBLE;
            result->confidence = 60;
        }
        result->entry = entry;
        snprintf(result->match_reason, sizeof(result->match_reason),
                 "Boot sector match: 0x%08X", boot_crc);
        return 0;
    }
    
    /* Try MD5 */
    uint8_t md5[16];
    uft_image_db_md5(data, size, md5);
    entry = uft_image_db_find_by_md5(md5);
    if (entry) {
        result->level = UFT_MATCH_EXACT;
        result->confidence = 100;
        result->entry = entry;
        snprintf(result->match_reason, sizeof(result->match_reason),
                 "MD5 match");
        return 0;
    }
    
    return 0;
}

int uft_image_db_identify_file(const char *path, uft_match_result_t *result) {
    if (!path || !result) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 100 * 1024 * 1024) {  /* Max 100MB */
        fclose(f);
        return -1;
    }
    
    uint8_t *data = (uint8_t *)malloc(size);
    if (!data) {
        fclose(f);
        return -1;
    }
    
    size_t read = fread(data, 1, size, f);
    fclose(f);
    
    int rc = -1;
    if (read == (size_t)size) {
        rc = uft_image_db_identify(data, size, result);
    }
    
    free(data);
    return rc;
}

/*===========================================================================
 * BOOT SECTOR ANALYSIS
 *===========================================================================*/

int uft_image_db_parse_boot(const uint8_t *boot_sector,
                            uft_boot_signature_t *sig) {
    if (!boot_sector || !sig) return -1;
    
    memset(sig, 0, sizeof(*sig));
    
    /* OEM name at offset 3 */
    memcpy(sig->oem_name, boot_sector + 3, 8);
    sig->oem_name[8] = '\0';
    
    /* BPB fields (little-endian) */
    sig->bytes_per_sector = boot_sector[11] | (boot_sector[12] << 8);
    sig->sectors_per_cluster = boot_sector[13];
    sig->reserved_sectors = boot_sector[14] | (boot_sector[15] << 8);
    sig->fat_count = boot_sector[16];
    sig->root_entries = boot_sector[17] | (boot_sector[18] << 8);
    sig->total_sectors = boot_sector[19] | (boot_sector[20] << 8);
    sig->media_descriptor = boot_sector[21];
    sig->sectors_per_fat = boot_sector[22] | (boot_sector[23] << 8);
    sig->sectors_per_track = boot_sector[24] | (boot_sector[25] << 8);
    sig->heads = boot_sector[26] | (boot_sector[27] << 8);
    sig->hidden_sectors = boot_sector[28] | (boot_sector[29] << 8) |
                          (boot_sector[30] << 16) | (boot_sector[31] << 24);
    
    return 0;
}

bool uft_image_db_is_windows_modified(const uint8_t *boot_sector) {
    if (!boot_sector) return false;
    
    /* Check for common Windows modifications */
    
    /* "NO NAME" volume label in extended boot record */
    if (memcmp(boot_sector + 43, "NO NAME    ", 11) == 0) {
        return true;
    }
    
    /* Check for Windows 9x serial number modification */
    /* Bytes 39-42 often contain timestamp-based serial */
    
    /* Check OEM name for Windows tools */
    char oem[9];
    memcpy(oem, boot_sector + 3, 8);
    oem[8] = '\0';
    
    const uft_oem_entry_t *oem_info = uft_image_db_lookup_oem(oem);
    if (oem_info && oem_info->is_windows_modified) {
        return true;
    }
    
    return false;
}

const uft_oem_entry_t* uft_image_db_lookup_oem(const char *oem_name) {
    if (!oem_name) return NULL;
    
    for (int i = 0; UFT_OEM_DATABASE[i].oem_name[0]; i++) {
        if (strncmp(UFT_OEM_DATABASE[i].oem_name, oem_name, 8) == 0) {
            return &UFT_OEM_DATABASE[i];
        }
    }
    
    return NULL;
}

const char* uft_image_db_suggest_oem(const uint8_t *boot_sector) {
    if (!boot_sector) return "MSDOS5.0";
    
    char oem[9];
    memcpy(oem, boot_sector + 3, 8);
    oem[8] = '\0';
    
    const uft_oem_entry_t *info = uft_image_db_lookup_oem(oem);
    if (info) {
        return info->correct_name;
    }
    
    /* Default suggestion */
    return "MSDOS5.0";
}

/*===========================================================================
 * DATABASE MODIFICATION
 *===========================================================================*/

int uft_image_db_add(const uft_image_entry_t *entry) {
    if (!entry || !g_initialized) return -1;
    
    /* Expand if needed */
    if (g_db_count >= g_db_capacity) {
        size_t new_cap = g_db_capacity * 2;
        if (new_cap > MAX_DB_ENTRIES) new_cap = MAX_DB_ENTRIES;
        if (g_db_count >= new_cap) return -1;
        
        uft_image_entry_t *new_db = (uft_image_entry_t *)realloc(
            g_database, new_cap * sizeof(uft_image_entry_t));
        if (!new_db) return -1;
        
        g_database = new_db;
        g_db_capacity = new_cap;
    }
    
    /* Copy entry */
    memcpy(&g_database[g_db_count], entry, sizeof(uft_image_entry_t));
    g_database[g_db_count].id = (uint32_t)g_db_count + 1;
    g_db_count++;
    
    return 0;
}

int uft_image_db_remove(uint32_t id) {
    for (size_t i = 0; i < g_db_count; i++) {
        if (g_database[i].id == id) {
            /* Shift entries down */
            memmove(&g_database[i], &g_database[i + 1],
                    (g_db_count - i - 1) * sizeof(uft_image_entry_t));
            g_db_count--;
            return 0;
        }
    }
    return -1;
}

int uft_image_db_create_entry(const uint8_t *data, size_t size,
                               const char *name,
                               uft_image_entry_t *entry) {
    if (!data || size == 0 || !entry) return -1;
    
    memset(entry, 0, sizeof(*entry));
    
    /* Set name */
    if (name) {
        strncpy(entry->name, name, sizeof(entry->name) - 1);
    }
    
    /* Calculate hashes */
    entry->hash.crc32 = uft_image_db_crc32(data, size);
    if (size >= 512) {
        entry->hash.boot_crc32 = uft_image_db_crc32(data, 512);
    }
    uft_image_db_md5(data, size, entry->hash.md5);
    
    /* Parse boot sector */
    if (size >= 512) {
        uft_image_db_parse_boot(data, &entry->boot_sig);
    }
    
    /* Set size */
    entry->image_size = (uint32_t)size;
    
    /* Detect platform from boot sector */
    if (size >= 512) {
        uint8_t media = data[21];
        uint16_t bps = data[11] | (data[12] << 8);
        
        if (bps == 512 && (media == 0xF0 || media == 0xF9 || 
            media == 0xFD || media == 0xFF)) {
            entry->platform = UFT_IMG_PLAT_MSDOS;
        }
    }
    
    return 0;
}

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

const char* uft_image_db_category_name(uft_image_category_t cat) {
    static const char *names[] = {
        "Unknown", "Game", "Application", "System",
        "Demo", "Data", "Magazine", "Custom"
    };
    if (cat > UFT_IMG_CAT_CUSTOM) return "Invalid";
    return names[cat];
}

const char* uft_image_db_platform_name(uft_image_platform_t plat) {
    static const char *names[] = {
        "Unknown", "MS-DOS", "Windows", "Amiga",
        "Atari ST", "C64", "Apple II", "Macintosh",
        "CP/M", "MSX", "BBC Micro", "Multi-platform"
    };
    if (plat > UFT_IMG_PLAT_MULTI) return "Invalid";
    return names[plat];
}

const char* uft_image_db_protection_name(uft_image_protection_t prot) {
    static const char *names[] = {
        "None", "Weak Bits", "Long Track", "Fuzzy Bits",
        "Timing", "Bad Sector", "Custom Format", "Multiple"
    };
    if (prot > UFT_IMG_PROT_MULTIPLE) return "Invalid";
    return names[prot];
}

void uft_image_db_print_stats(void) {
    printf("Image Database Statistics:\n");
    printf("  Entries: %zu / %zu\n", g_db_count, g_db_capacity);
    
    /* Count by platform */
    int platform_counts[12] = {0};
    for (size_t i = 0; i < g_db_count; i++) {
        if (g_database[i].platform <= UFT_IMG_PLAT_MULTI) {
            platform_counts[g_database[i].platform]++;
        }
    }
    
    printf("  By Platform:\n");
    for (int p = 0; p <= UFT_IMG_PLAT_MULTI; p++) {
        if (platform_counts[p] > 0) {
            printf("    %s: %d\n", uft_image_db_platform_name(p), platform_counts[p]);
        }
    }
}

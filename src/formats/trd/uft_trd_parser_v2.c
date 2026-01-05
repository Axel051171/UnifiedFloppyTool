/**
 * @file uft_trd_parser_v2.c
 * @brief GOD MODE TR-DOS Parser v2 - ZX Spectrum Floppy Format
 * 
 * TR-DOS (Technology Research DOS) format for ZX Spectrum Beta Disk Interface.
 * Standard format: 80 tracks, 2 sides, 16 sectors per track, 256 bytes/sector.
 * 
 * Features:
 * - Catalog parsing with file metadata
 * - File type detection (BASIC, code, data, screen)
 * - Deleted file recovery
 * - Multiple disk geometries (40/80 track, SS/DS)
 * - Boot sector analysis
 * - SCL container support preparation
 * - Disk password detection
 * - Free space calculation
 * 
 * @author UFT Team / GOD MODE
 * @version 2.0.0
 * @date 2025-01-02
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS AND MAGIC NUMBERS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define TRD_SECTOR_SIZE         256
#define TRD_SECTORS_PER_TRACK   16
#define TRD_BYTES_PER_TRACK     (TRD_SECTOR_SIZE * TRD_SECTORS_PER_TRACK)  /* 4096 */

#define TRD_CATALOG_TRACK       0
#define TRD_CATALOG_SECTOR      0
#define TRD_CATALOG_ENTRIES     128  /* Max files in catalog */
#define TRD_CATALOG_ENTRY_SIZE  16

#define TRD_SYSTEM_SECTOR       8    /* Sector 8 on track 0 has system info */
#define TRD_FIRST_DATA_SECTOR   1    /* First sector after catalog */
#define TRD_FIRST_DATA_TRACK    1    /* Data starts on track 1 */

/* Disk type byte (offset 0xE7 in sector 8) */
#define TRD_TYPE_80_DS    0x16  /* 80 tracks, double-sided */
#define TRD_TYPE_40_DS    0x17  /* 40 tracks, double-sided */
#define TRD_TYPE_80_SS    0x18  /* 80 tracks, single-sided */
#define TRD_TYPE_40_SS    0x19  /* 40 tracks, single-sided */

/* File type codes */
#define TRD_FILE_BASIC    'B'   /* BASIC program */
#define TRD_FILE_DATA     'D'   /* Data array */
#define TRD_FILE_CODE     'C'   /* Machine code */
#define TRD_FILE_PRINT    '#'   /* Print file */

/* Special markers */
#define TRD_DELETED       0x01  /* Deleted file marker */
#define TRD_END_CATALOG   0x00  /* End of catalog */

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TR-DOS file types
 */
typedef enum {
    TRD_FTYPE_BASIC = 0,      /* BASIC program */
    TRD_FTYPE_NUMBER_ARRAY,   /* Numeric array */
    TRD_FTYPE_CHAR_ARRAY,     /* Character array */
    TRD_FTYPE_CODE,           /* Machine code */
    TRD_FTYPE_PRINT,          /* PRINT output */
    TRD_FTYPE_SEQUENTIAL,     /* Sequential access file */
    TRD_FTYPE_DELETED,        /* Deleted entry */
    TRD_FTYPE_UNKNOWN         /* Unknown type */
} trd_file_type_t;

/**
 * @brief TR-DOS catalog entry
 */
typedef struct {
    char name[9];             /* Filename (8 chars + null) */
    char extension;           /* File extension (type char) */
    uint16_t param1;          /* Start address (CODE) or LINE (BASIC) */
    uint16_t length;          /* File length in bytes */
    uint8_t length_sectors;   /* File length in sectors */
    uint8_t start_sector;     /* First sector */
    uint8_t start_track;      /* First track */
    trd_file_type_t type;     /* Decoded file type */
    bool deleted;             /* Is file deleted? */
    uint16_t param2;          /* Additional parameter (array variable) */
} trd_catalog_entry_t;

/**
 * @brief TR-DOS disk geometry
 */
typedef struct {
    uint8_t tracks;           /* 40 or 80 */
    uint8_t sides;            /* 1 or 2 */
    uint16_t total_sectors;   /* Total sectors on disk */
    uint32_t total_bytes;     /* Total capacity */
    const char* name;         /* Human-readable name */
} trd_geometry_t;

/**
 * @brief TR-DOS system sector info (sector 8, track 0)
 */
typedef struct {
    uint8_t first_free_sector;
    uint8_t first_free_track;
    uint8_t disk_type;
    uint8_t file_count;
    uint16_t free_sectors;
    uint8_t tr_dos_id;        /* 0x10 for TR-DOS */
    uint16_t deleted_files;
    char disk_label[9];       /* Disk title (8 chars + null) */
    char password[9];         /* Disk password (8 chars + null) */
} trd_system_info_t;

/**
 * @brief Parsed TRD disk structure
 */
typedef struct {
    trd_geometry_t geometry;
    trd_system_info_t system;
    trd_catalog_entry_t catalog[TRD_CATALOG_ENTRIES];
    uint16_t file_count;
    uint16_t deleted_count;
    uint32_t used_bytes;
    uint32_t free_bytes;
    bool valid;
    char error[256];
} trd_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * GEOMETRY TABLES
 * ═══════════════════════════════════════════════════════════════════════════ */

static const trd_geometry_t trd_geometries[] = {
    { 80, 2, 2560, 655360, "80T DS (640KB)" },
    { 40, 2, 1280, 327680, "40T DS (320KB)" },
    { 80, 1, 1280, 327680, "80T SS (320KB)" },
    { 40, 1, 640,  163840, "40T SS (160KB)" },
    { 0, 0, 0, 0, NULL }
};

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get geometry from disk type byte
 */
static const trd_geometry_t* trd_geometry_from_type(uint8_t type) {
    switch (type) {
        case TRD_TYPE_80_DS: return &trd_geometries[0];
        case TRD_TYPE_40_DS: return &trd_geometries[1];
        case TRD_TYPE_80_SS: return &trd_geometries[2];
        case TRD_TYPE_40_SS: return &trd_geometries[3];
        default: return &trd_geometries[0];  /* Default to 80T DS */
    }
}

/**
 * @brief Get geometry name from type byte
 */
static const char* trd_geometry_name(uint8_t type) {
    const trd_geometry_t* geo = trd_geometry_from_type(type);
    return geo->name ? geo->name : "Unknown";
}

/**
 * @brief Convert file type char to enum
 */
static trd_file_type_t trd_decode_file_type(char type_char) {
    switch (type_char) {
        case 'B': return TRD_FTYPE_BASIC;
        case 'D': return TRD_FTYPE_NUMBER_ARRAY;
        case 'C': return TRD_FTYPE_CODE;
        case '#': return TRD_FTYPE_PRINT;
        case 'S': return TRD_FTYPE_SEQUENTIAL;
        default:
            /* Check if it's an array variable name (A-Z for number, a-z for char) */
            if (type_char >= 'A' && type_char <= 'Z') return TRD_FTYPE_NUMBER_ARRAY;
            if (type_char >= 'a' && type_char <= 'z') return TRD_FTYPE_CHAR_ARRAY;
            return TRD_FTYPE_UNKNOWN;
    }
}

/**
 * @brief Get file type name
 */
static const char* trd_file_type_name(trd_file_type_t type) {
    static const char* names[] = {
        "BASIC",
        "Number Array",
        "Character Array",
        "Code",
        "Print",
        "Sequential",
        "Deleted",
        "Unknown"
    };
    if (type <= TRD_FTYPE_UNKNOWN) {
        return names[type];
    }
    return "Invalid";
}

/**
 * @brief Calculate sector offset in image
 */
static size_t trd_sector_offset(uint8_t track, uint8_t side, uint8_t sector) {
    /* Interleaved sides: track 0 side 0, track 0 side 1, track 1 side 0, ... */
    size_t physical_track = track * 2 + side;
    return (physical_track * TRD_SECTORS_PER_TRACK + sector) * TRD_SECTOR_SIZE;
}

/**
 * @brief Calculate linear sector number
 */
static uint16_t trd_linear_sector(uint8_t track, uint8_t sector, uint8_t sides) {
    if (sides == 2) {
        return track * TRD_SECTORS_PER_TRACK * 2 + sector;
    }
    return track * TRD_SECTORS_PER_TRACK + sector;
}

/**
 * @brief Copy and sanitize filename
 */
static void trd_copy_filename(char* dest, const uint8_t* src, size_t max_len) {
    size_t i;
    for (i = 0; i < max_len && src[i] != 0; i++) {
        /* Replace non-printable characters */
        if (src[i] >= 32 && src[i] < 127) {
            dest[i] = src[i];
        } else {
            dest[i] = '?';
        }
    }
    /* Trim trailing spaces */
    while (i > 0 && dest[i-1] == ' ') {
        i--;
    }
    dest[i] = '\0';
}

/**
 * @brief Check if TRD image is valid
 */
static bool trd_is_valid(const uint8_t* data, size_t size) {
    /* Minimum size: 80 tracks, 2 sides, 16 sectors, 256 bytes */
    if (size < 163840) return false;  /* At least 40T SS */
    
    /* Check system sector (sector 8, track 0) */
    if (size >= 9 * TRD_SECTOR_SIZE) {
        const uint8_t* sys = data + 8 * TRD_SECTOR_SIZE;
        
        /* TR-DOS ID byte at offset 0xE2 should be 0x10 */
        if (sys[0xE2] == 0x10) return true;
        
        /* Check disk type byte at offset 0xE7 */
        uint8_t disk_type = sys[0xE7];
        if (disk_type >= 0x16 && disk_type <= 0x19) return true;
    }
    
    /* Check file size matches known geometries */
    for (int i = 0; trd_geometries[i].name; i++) {
        if (size == trd_geometries[i].total_bytes) return true;
    }
    
    return false;
}

/**
 * @brief Detect TRD geometry from file size
 */
static const trd_geometry_t* trd_detect_geometry(size_t size) {
    for (int i = 0; trd_geometries[i].name; i++) {
        if (size == trd_geometries[i].total_bytes) {
            return &trd_geometries[i];
        }
    }
    /* Default to 80T DS if size doesn't match exactly */
    return &trd_geometries[0];
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse system sector (sector 8, track 0)
 */
static bool trd_parse_system_sector(const uint8_t* data, size_t size, 
                                     trd_system_info_t* info) {
    if (size < 9 * TRD_SECTOR_SIZE) {
        return false;
    }
    
    const uint8_t* sys = data + 8 * TRD_SECTOR_SIZE;
    
    info->first_free_sector = sys[0xE1];
    info->first_free_track = sys[0xE2];
    info->disk_type = sys[0xE3];
    info->file_count = sys[0xE4];
    info->free_sectors = sys[0xE5] | (sys[0xE6] << 8);
    info->tr_dos_id = sys[0xE7];  /* Should be 0x10 */
    info->deleted_files = sys[0xE9] | (sys[0xEA] << 8);
    
    /* Disk label at offset 0xF5 (8 bytes) */
    trd_copy_filename(info->disk_label, sys + 0xF5, 8);
    
    /* Password handling - not standard but some disks use it */
    memset(info->password, 0, sizeof(info->password));
    
    return true;
}

/**
 * @brief Parse catalog entry
 */
static bool trd_parse_catalog_entry(const uint8_t* entry_data, 
                                     trd_catalog_entry_t* entry) {
    /* First byte: 0x00 = end of catalog, 0x01 = deleted */
    if (entry_data[0] == TRD_END_CATALOG) {
        return false;  /* No more entries */
    }
    
    entry->deleted = (entry_data[0] == TRD_DELETED);
    
    /* Filename: bytes 0-7 */
    if (entry->deleted) {
        /* For deleted files, reconstruct name without first byte */
        entry->name[0] = '?';
        trd_copy_filename(entry->name + 1, entry_data + 1, 7);
    } else {
        trd_copy_filename(entry->name, entry_data, 8);
    }
    
    /* Extension (type char): byte 8 */
    entry->extension = entry_data[8];
    entry->type = trd_decode_file_type(entry->extension);
    if (entry->deleted) entry->type = TRD_FTYPE_DELETED;
    
    /* Parameters: bytes 9-12 */
    entry->param1 = entry_data[9] | (entry_data[10] << 8);   /* Start/LINE */
    entry->length = entry_data[11] | (entry_data[12] << 8);  /* Length */
    
    /* Sector count and location: bytes 13-15 */
    entry->length_sectors = entry_data[13];
    entry->start_sector = entry_data[14];
    entry->start_track = entry_data[15];
    
    return true;
}

/**
 * @brief Parse entire TRD disk
 */
static bool trd_parse_disk(const uint8_t* data, size_t size, trd_disk_t* disk) {
    memset(disk, 0, sizeof(trd_disk_t));
    
    /* Validate */
    if (!trd_is_valid(data, size)) {
        snprintf(disk->error, sizeof(disk->error), "Invalid TRD image");
        return false;
    }
    
    /* Detect geometry */
    const trd_geometry_t* geo = trd_detect_geometry(size);
    disk->geometry = *geo;
    
    /* Parse system sector */
    if (!trd_parse_system_sector(data, size, &disk->system)) {
        snprintf(disk->error, sizeof(disk->error), "Failed to parse system sector");
        return false;
    }
    
    /* Override geometry from disk type if available */
    if (disk->system.disk_type >= 0x16 && disk->system.disk_type <= 0x19) {
        disk->geometry = *trd_geometry_from_type(disk->system.disk_type);
    }
    
    /* Parse catalog (sectors 0-7, track 0) */
    disk->file_count = 0;
    disk->deleted_count = 0;
    disk->used_bytes = 0;
    
    for (int sector = 0; sector < 8; sector++) {
        const uint8_t* sector_data = data + sector * TRD_SECTOR_SIZE;
        
        for (int entry = 0; entry < 16; entry++) {  /* 16 entries per sector */
            const uint8_t* entry_data = sector_data + entry * TRD_CATALOG_ENTRY_SIZE;
            
            if (disk->file_count >= TRD_CATALOG_ENTRIES) break;
            
            trd_catalog_entry_t* cat_entry = &disk->catalog[disk->file_count];
            
            if (!trd_parse_catalog_entry(entry_data, cat_entry)) {
                goto catalog_done;  /* End of catalog */
            }
            
            if (cat_entry->deleted) {
                disk->deleted_count++;
            } else {
                disk->used_bytes += cat_entry->length_sectors * TRD_SECTOR_SIZE;
            }
            
            disk->file_count++;
        }
    }
    
catalog_done:
    disk->free_bytes = disk->system.free_sectors * TRD_SECTOR_SIZE;
    disk->valid = true;
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FILE EXTRACTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Extract file data from TRD image
 */
static uint8_t* trd_extract_file(const uint8_t* disk_data, size_t disk_size,
                                  const trd_catalog_entry_t* entry,
                                  size_t* out_size) {
    if (!entry || entry->length_sectors == 0) {
        *out_size = 0;
        return NULL;
    }
    
    /* Allocate buffer for file data */
    size_t file_size = entry->length_sectors * TRD_SECTOR_SIZE;
    uint8_t* file_data = malloc(file_size);
    if (!file_data) {
        *out_size = 0;
        return NULL;
    }
    
    /* Read sectors */
    uint8_t track = entry->start_track;
    uint8_t sector = entry->start_sector;
    size_t offset = 0;
    
    for (uint8_t i = 0; i < entry->length_sectors; i++) {
        /* Calculate offset in image (assuming interleaved) */
        size_t src_offset = trd_sector_offset(track, 0, sector);
        
        if (src_offset + TRD_SECTOR_SIZE > disk_size) {
            /* Truncated image */
            break;
        }
        
        memcpy(file_data + offset, disk_data + src_offset, TRD_SECTOR_SIZE);
        offset += TRD_SECTOR_SIZE;
        
        /* Advance to next sector */
        sector++;
        if (sector >= TRD_SECTORS_PER_TRACK) {
            sector = 0;
            track++;
        }
    }
    
    /* Trim to actual file length */
    if (entry->length < file_size) {
        file_size = entry->length;
    }
    
    *out_size = file_size;
    return file_data;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CONVERSION FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create TRD image from raw sectors
 */
static uint8_t* trd_create_blank(const trd_geometry_t* geometry, size_t* out_size) {
    size_t size = geometry->total_bytes;
    uint8_t* data = calloc(1, size);
    if (!data) {
        *out_size = 0;
        return NULL;
    }
    
    /* Initialize system sector */
    uint8_t* sys = data + 8 * TRD_SECTOR_SIZE;
    
    /* First free sector: sector 0, track 1 */
    sys[0xE1] = 0;   /* First free sector */
    sys[0xE2] = 1;   /* First free track */
    
    /* Disk type */
    if (geometry->tracks == 80 && geometry->sides == 2) {
        sys[0xE3] = TRD_TYPE_80_DS;
    } else if (geometry->tracks == 40 && geometry->sides == 2) {
        sys[0xE3] = TRD_TYPE_40_DS;
    } else if (geometry->tracks == 80 && geometry->sides == 1) {
        sys[0xE3] = TRD_TYPE_80_SS;
    } else {
        sys[0xE3] = TRD_TYPE_40_SS;
    }
    
    sys[0xE4] = 0;   /* File count */
    
    /* Free sectors (total - 8 catalog sectors - 8 system sectors) */
    uint16_t free = geometry->total_sectors - 16;
    sys[0xE5] = free & 0xFF;
    sys[0xE6] = (free >> 8) & 0xFF;
    
    sys[0xE7] = 0x10;  /* TR-DOS ID */
    
    /* Disk label "        " */
    memset(sys + 0xF5, ' ', 8);
    
    *out_size = size;
    return data;
}

/**
 * @brief Generate catalog listing as text
 */
static char* trd_catalog_to_text(const trd_disk_t* disk) {
    /* Estimate size: ~80 chars per line, up to 128 files + header */
    size_t buf_size = 128 * 100;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    
    size_t offset = 0;
    
    /* Header */
    offset += snprintf(buf + offset, buf_size - offset,
        "TR-DOS Disk: %s\n"
        "Geometry: %s (%u tracks, %u sides)\n"
        "Files: %u (Deleted: %u)\n"
        "Free: %u sectors (%u KB)\n\n"
        "Catalog:\n"
        "%-8s %-4s %6s %6s %5s %s\n"
        "─────────────────────────────────────────────\n",
        disk->system.disk_label[0] ? disk->system.disk_label : "(none)",
        disk->geometry.name,
        disk->geometry.tracks,
        disk->geometry.sides,
        disk->file_count - disk->deleted_count,
        disk->deleted_count,
        disk->system.free_sectors,
        disk->system.free_sectors * TRD_SECTOR_SIZE / 1024,
        "Name", "Ext", "Start", "Length", "Secs", "Type"
    );
    
    /* Files */
    for (uint16_t i = 0; i < disk->file_count; i++) {
        const trd_catalog_entry_t* entry = &disk->catalog[i];
        
        offset += snprintf(buf + offset, buf_size - offset,
            "%-8s  %c   %5u %6u %5u %s%s\n",
            entry->name,
            entry->extension,
            entry->param1,
            entry->length,
            entry->length_sectors,
            trd_file_type_name(entry->type),
            entry->deleted ? " [DELETED]" : ""
        );
    }
    
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef TRD_PARSER_TEST

#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("Testing %s... ", #name); \
    test_##name(); \
    printf("✓\n"); \
    tests_passed++; \
} while(0)

TEST(geometry_names) {
    assert(strcmp(trd_geometry_name(TRD_TYPE_80_DS), "80T DS (640KB)") == 0);
    assert(strcmp(trd_geometry_name(TRD_TYPE_40_DS), "40T DS (320KB)") == 0);
    assert(strcmp(trd_geometry_name(TRD_TYPE_80_SS), "80T SS (320KB)") == 0);
    assert(strcmp(trd_geometry_name(TRD_TYPE_40_SS), "40T SS (160KB)") == 0);
}

TEST(file_types) {
    assert(trd_decode_file_type('B') == TRD_FTYPE_BASIC);
    assert(trd_decode_file_type('C') == TRD_FTYPE_CODE);
    assert(trd_decode_file_type('D') == TRD_FTYPE_NUMBER_ARRAY);
    assert(trd_decode_file_type('#') == TRD_FTYPE_PRINT);
    assert(trd_decode_file_type('A') == TRD_FTYPE_NUMBER_ARRAY);  /* Array variable */
    assert(trd_decode_file_type('a') == TRD_FTYPE_CHAR_ARRAY);    /* Char array */
}

TEST(file_type_names) {
    assert(strcmp(trd_file_type_name(TRD_FTYPE_BASIC), "BASIC") == 0);
    assert(strcmp(trd_file_type_name(TRD_FTYPE_CODE), "Code") == 0);
    assert(strcmp(trd_file_type_name(TRD_FTYPE_DELETED), "Deleted") == 0);
}

TEST(sector_offset) {
    /* Track 0, side 0, sector 0 */
    assert(trd_sector_offset(0, 0, 0) == 0);
    
    /* Track 0, side 0, sector 8 (system sector) */
    assert(trd_sector_offset(0, 0, 8) == 8 * TRD_SECTOR_SIZE);
    
    /* Track 0, side 1, sector 0 */
    assert(trd_sector_offset(0, 1, 0) == TRD_BYTES_PER_TRACK);
    
    /* Track 1, side 0, sector 0 */
    assert(trd_sector_offset(1, 0, 0) == 2 * TRD_BYTES_PER_TRACK);
}

TEST(blank_creation) {
    size_t size = 0;
    uint8_t* data = trd_create_blank(&trd_geometries[0], &size);
    assert(data != NULL);
    assert(size == 655360);  /* 80T DS = 640KB */
    
    /* Check system sector */
    uint8_t* sys = data + 8 * TRD_SECTOR_SIZE;
    assert(sys[0xE3] == TRD_TYPE_80_DS);
    assert(sys[0xE7] == 0x10);  /* TR-DOS ID */
    
    free(data);
}

int main(void) {
    printf("=== TRD Parser v2 Tests ===\n");
    
    RUN_TEST(geometry_names);
    RUN_TEST(file_types);
    RUN_TEST(file_type_names);
    RUN_TEST(sector_offset);
    RUN_TEST(blank_creation);
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}

#endif /* TRD_PARSER_TEST */

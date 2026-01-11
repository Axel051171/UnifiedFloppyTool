/**
 * @file uft_kc85_format.h
 * @brief KC85/Z1013 DDR Computer Disk Formats
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * Disk format support for East German (DDR) home computers:
 * - KC85/4, KC85/5 (Robotron)
 * - Z1013 (Hobby-Computer)
 * - KC87
 * - PC/M (CP/M-kompatibel)
 * - KC compact (Amstrad CPC clone)
 *
 * These systems used Z80 CPUs with various floppy controllers:
 * - D004 Floppy Module (KC85/4, KC85/5)
 * - MicroDOS
 * - CP/M 2.2 compatible
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_KC85_FORMAT_H
#define UFT_KC85_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * KC85 Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief KC85 D004 sector size */
#define UFT_KC85_SECTOR_SIZE        512

/** @brief KC85 D004 sectors per track (5.25" DD) */
#define UFT_KC85_SPT_DD             5

/** @brief KC85 D004 sectors per track (5.25" QD - 80 track) */
#define UFT_KC85_SPT_QD             9

/** @brief KC85 tracks (40 track drive) */
#define UFT_KC85_TRACKS_40          40

/** @brief KC85 tracks (80 track drive) */
#define UFT_KC85_TRACKS_80          80

/** @brief Z1013 sector size */
#define UFT_Z1013_SECTOR_SIZE       256

/** @brief Z1013 sectors per track */
#define UFT_Z1013_SPT               16

/** @brief MicroDOS boot signature */
#define UFT_KC85_MICRODOS_SIG       "MICRODOS"

/** @brief D004 module ID */
#define UFT_KC85_D004_MODULE        0xD4

/* ═══════════════════════════════════════════════════════════════════════════
 * KC85 System Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief DDR computer system type
 */
typedef enum {
    UFT_KC_SYSTEM_UNKNOWN = 0,
    UFT_KC_SYSTEM_KC85_4,       /**< KC85/4 with D004 */
    UFT_KC_SYSTEM_KC85_5,       /**< KC85/5 with D004 */
    UFT_KC_SYSTEM_KC87,         /**< KC87 */
    UFT_KC_SYSTEM_Z1013,        /**< Z1013 Hobby-Computer */
    UFT_KC_SYSTEM_Z9001,        /**< Z9001 / KC85/1 */
    UFT_KC_SYSTEM_PC_M,         /**< PC/M (CP/M compatible) */
    UFT_KC_SYSTEM_KC_COMPACT,   /**< KC compact (CPC clone) */
    UFT_KC_SYSTEM_LLC2,         /**< LLC2 */
    UFT_KC_SYSTEM_BCS3,         /**< BCS3 */
    UFT_KC_SYSTEM_POLY880       /**< Poly880 */
} uft_kc_system_t;

/**
 * @brief KC85 disk type
 */
typedef enum {
    UFT_KC_DISK_UNKNOWN = 0,
    UFT_KC_DISK_MICRODOS,       /**< MicroDOS format */
    UFT_KC_DISK_CPM,            /**< CP/M 2.2 format */
    UFT_KC_DISK_CAOS,           /**< CAOS native format */
    UFT_KC_DISK_EDSK,           /**< KC compact EDSK */
    UFT_KC_DISK_RAW             /**< Raw sector dump */
} uft_kc_disk_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * KC85 Disk Geometry
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief KC85/Z1013 disk geometry profile
 */
typedef struct {
    const char* name;
    uft_kc_system_t system;
    uft_kc_disk_type_t disk_type;
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    uint8_t reserved_tracks;    /**< System tracks */
    uint16_t dir_entries;       /**< Directory entries */
    uint16_t block_size;        /**< Allocation block size */
    uint32_t total_size;        /**< Total disk size in bytes */
    const char* description;
} uft_kc_geometry_t;

/**
 * @brief KC85/Z1013 geometry profiles
 * 
 * Note: KC85_D004_MICRODOS uses 22disk definition:
 * DENSITY MFM,LOW | CYLINDERS 80 | SIDES 2 | SECTORS 5,1024
 * SKEW 2 | ORDER SIDES
 * BSH 4 | BLM 15 | EXM 0 | DSM 389 | DRM 127 | AL0 0C0H | OFS 4
 */
static const uft_kc_geometry_t UFT_KC_GEOMETRIES[] = {
    /* KC85/4 and KC85/5 with D004 module - from 22disk definition */
    {
        .name = "KC85_D004_MICRODOS",
        .system = UFT_KC_SYSTEM_KC85_4,
        .disk_type = UFT_KC_DISK_MICRODOS,
        .tracks = 80,
        .sides = 2,
        .sectors_per_track = 5,
        .sector_size = 1024,
        .reserved_tracks = 4,       /* OFS 4 from 22disk */
        .dir_entries = 128,         /* DRM 127 + 1 */
        .block_size = 2048,         /* BSH 4 = 2^4 * 128 = 2048 */
        .total_size = 780 * 1024,   /* DSM 389 * 2KB = ~780KB usable */
        .description = "KC85 MicroDOS 80T DS (22disk compatible)"
    },
    {
        .name = "KC85_D004_40T",
        .system = UFT_KC_SYSTEM_KC85_4,
        .disk_type = UFT_KC_DISK_MICRODOS,
        .tracks = 40,
        .sides = 2,
        .sectors_per_track = 5,
        .sector_size = 512,
        .reserved_tracks = 2,
        .dir_entries = 64,
        .block_size = 2048,
        .total_size = 200 * 1024,
        .description = "KC85/4 D004 40T DS DD (200KB)"
    },
    {
        .name = "KC85_D004_80T",
        .system = UFT_KC_SYSTEM_KC85_5,
        .disk_type = UFT_KC_DISK_MICRODOS,
        .tracks = 80,
        .sides = 2,
        .sectors_per_track = 5,
        .sector_size = 512,
        .reserved_tracks = 2,
        .dir_entries = 128,
        .block_size = 2048,
        .total_size = 400 * 1024,
        .description = "KC85/5 D004 80T DS DD (400KB)"
    },
    {
        .name = "KC85_D004_QD",
        .system = UFT_KC_SYSTEM_KC85_5,
        .disk_type = UFT_KC_DISK_MICRODOS,
        .tracks = 80,
        .sides = 2,
        .sectors_per_track = 9,
        .sector_size = 512,
        .reserved_tracks = 2,
        .dir_entries = 128,
        .block_size = 2048,
        .total_size = 720 * 1024,
        .description = "KC85/5 D004 80T DS QD (720KB)"
    },
    
    /* Z1013 */
    {
        .name = "Z1013_SD",
        .system = UFT_KC_SYSTEM_Z1013,
        .disk_type = UFT_KC_DISK_CPM,
        .tracks = 40,
        .sides = 1,
        .sectors_per_track = 16,
        .sector_size = 256,
        .reserved_tracks = 3,
        .dir_entries = 64,
        .block_size = 1024,
        .total_size = 160 * 1024,
        .description = "Z1013 SS SD (160KB)"
    },
    {
        .name = "Z1013_DD",
        .system = UFT_KC_SYSTEM_Z1013,
        .disk_type = UFT_KC_DISK_CPM,
        .tracks = 40,
        .sides = 2,
        .sectors_per_track = 16,
        .sector_size = 256,
        .reserved_tracks = 3,
        .dir_entries = 128,
        .block_size = 2048,
        .total_size = 320 * 1024,
        .description = "Z1013 DS DD (320KB)"
    },
    
    /* KC87 */
    {
        .name = "KC87_SD",
        .system = UFT_KC_SYSTEM_KC87,
        .disk_type = UFT_KC_DISK_CPM,
        .tracks = 40,
        .sides = 1,
        .sectors_per_track = 16,
        .sector_size = 256,
        .reserved_tracks = 2,
        .dir_entries = 64,
        .block_size = 1024,
        .total_size = 160 * 1024,
        .description = "KC87 SS SD (160KB)"
    },
    
    /* Z9001 / KC85/1 */
    {
        .name = "Z9001_SD",
        .system = UFT_KC_SYSTEM_Z9001,
        .disk_type = UFT_KC_DISK_CPM,
        .tracks = 40,
        .sides = 1,
        .sectors_per_track = 16,
        .sector_size = 256,
        .reserved_tracks = 2,
        .dir_entries = 64,
        .block_size = 1024,
        .total_size = 160 * 1024,
        .description = "Z9001/KC85-1 SS SD (160KB)"
    },
    
    /* PC/M - CP/M compatible */
    {
        .name = "PCM_SD",
        .system = UFT_KC_SYSTEM_PC_M,
        .disk_type = UFT_KC_DISK_CPM,
        .tracks = 77,
        .sides = 1,
        .sectors_per_track = 26,
        .sector_size = 128,
        .reserved_tracks = 2,
        .dir_entries = 64,
        .block_size = 1024,
        .total_size = 250 * 1024,
        .description = "PC/M 8\" SS SD (250KB)"
    },
    {
        .name = "PCM_DD",
        .system = UFT_KC_SYSTEM_PC_M,
        .disk_type = UFT_KC_DISK_CPM,
        .tracks = 77,
        .sides = 2,
        .sectors_per_track = 26,
        .sector_size = 256,
        .reserved_tracks = 2,
        .dir_entries = 128,
        .block_size = 2048,
        .total_size = 1000 * 1024,
        .description = "PC/M 8\" DS DD (1MB)"
    },
    
    /* KC compact (CPC clone) - uses EDSK format */
    {
        .name = "KC_COMPACT_SYS",
        .system = UFT_KC_SYSTEM_KC_COMPACT,
        .disk_type = UFT_KC_DISK_EDSK,
        .tracks = 40,
        .sides = 1,
        .sectors_per_track = 9,
        .sector_size = 512,
        .reserved_tracks = 2,
        .dir_entries = 64,
        .block_size = 1024,
        .total_size = 180 * 1024,
        .description = "KC compact System (180KB)"
    },
    {
        .name = "KC_COMPACT_DATA",
        .system = UFT_KC_SYSTEM_KC_COMPACT,
        .disk_type = UFT_KC_DISK_EDSK,
        .tracks = 40,
        .sides = 1,
        .sectors_per_track = 9,
        .sector_size = 512,
        .reserved_tracks = 0,
        .dir_entries = 64,
        .block_size = 1024,
        .total_size = 180 * 1024,
        .description = "KC compact Data (180KB)"
    },
    
    /* LLC2 */
    {
        .name = "LLC2_DD",
        .system = UFT_KC_SYSTEM_LLC2,
        .disk_type = UFT_KC_DISK_CPM,
        .tracks = 80,
        .sides = 2,
        .sectors_per_track = 5,
        .sector_size = 1024,
        .reserved_tracks = 2,
        .dir_entries = 128,
        .block_size = 2048,
        .total_size = 800 * 1024,
        .description = "LLC2 DS DD (800KB)"
    },
    
    /* BCS3 */
    {
        .name = "BCS3_DD",
        .system = UFT_KC_SYSTEM_BCS3,
        .disk_type = UFT_KC_DISK_CPM,
        .tracks = 40,
        .sides = 2,
        .sectors_per_track = 16,
        .sector_size = 256,
        .reserved_tracks = 2,
        .dir_entries = 64,
        .block_size = 2048,
        .total_size = 320 * 1024,
        .description = "BCS3 DS DD (320KB)"
    },
    
    /* Sentinel */
    { .name = NULL }
};

/* ═══════════════════════════════════════════════════════════════════════════
 * MicroDOS Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief MicroDOS boot sector
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t jump[3];            /**< Jump instruction (C3 xx xx or E9 xx xx) */
    char oem_name[8];           /**< OEM name (often "MICRODOS") */
    uint16_t bytes_per_sector;  /**< Bytes per sector */
    uint8_t sectors_per_cluster;/**< Sectors per allocation unit */
    uint16_t reserved_sectors;  /**< Reserved sectors */
    uint8_t fat_copies;         /**< Number of FAT copies */
    uint16_t root_entries;      /**< Root directory entries */
    uint16_t total_sectors;     /**< Total sectors */
    uint8_t media_type;         /**< Media descriptor */
    uint16_t sectors_per_fat;   /**< Sectors per FAT */
    uint16_t sectors_per_track; /**< Sectors per track */
    uint16_t heads;             /**< Number of heads */
    uint16_t hidden_sectors;    /**< Hidden sectors */
} uft_microdos_boot_t;
#pragma pack(pop)

/**
 * @brief CAOS directory entry
 */
#pragma pack(push, 1)
typedef struct {
    char filename[8];           /**< Filename (space-padded) */
    char extension[3];          /**< Extension */
    uint8_t attributes;         /**< File attributes */
    uint8_t reserved[10];       /**< Reserved */
    uint16_t start_cluster;     /**< Starting cluster */
    uint32_t file_size;         /**< File size */
} uft_caos_dir_entry_t;
#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get system name
 */
static inline const char* uft_kc_system_name(uft_kc_system_t system) {
    switch (system) {
        case UFT_KC_SYSTEM_KC85_4:     return "KC85/4";
        case UFT_KC_SYSTEM_KC85_5:     return "KC85/5";
        case UFT_KC_SYSTEM_KC87:       return "KC87";
        case UFT_KC_SYSTEM_Z1013:      return "Z1013";
        case UFT_KC_SYSTEM_Z9001:      return "Z9001/KC85-1";
        case UFT_KC_SYSTEM_PC_M:       return "PC/M";
        case UFT_KC_SYSTEM_KC_COMPACT: return "KC compact";
        case UFT_KC_SYSTEM_LLC2:       return "LLC2";
        case UFT_KC_SYSTEM_BCS3:       return "BCS3";
        case UFT_KC_SYSTEM_POLY880:    return "Poly880";
        default: return "Unknown";
    }
}

/**
 * @brief Get disk type name
 */
static inline const char* uft_kc_disk_type_name(uft_kc_disk_type_t type) {
    switch (type) {
        case UFT_KC_DISK_MICRODOS: return "MicroDOS";
        case UFT_KC_DISK_CPM:      return "CP/M";
        case UFT_KC_DISK_CAOS:     return "CAOS";
        case UFT_KC_DISK_EDSK:     return "EDSK";
        case UFT_KC_DISK_RAW:      return "Raw";
        default: return "Unknown";
    }
}

/**
 * @brief Find geometry by name
 */
static inline const uft_kc_geometry_t* uft_kc_find_geometry(const char* name) {
    for (int i = 0; UFT_KC_GEOMETRIES[i].name != NULL; i++) {
        if (strcmp(UFT_KC_GEOMETRIES[i].name, name) == 0) {
            return &UFT_KC_GEOMETRIES[i];
        }
    }
    return NULL;
}

/**
 * @brief Find geometry by system and size
 */
static inline const uft_kc_geometry_t* uft_kc_find_by_size(uft_kc_system_t system,
                                                           uint32_t size) {
    for (int i = 0; UFT_KC_GEOMETRIES[i].name != NULL; i++) {
        if (UFT_KC_GEOMETRIES[i].system == system &&
            UFT_KC_GEOMETRIES[i].total_size == size) {
            return &UFT_KC_GEOMETRIES[i];
        }
    }
    return NULL;
}

/**
 * @brief Get all geometries for a system
 */
static inline int uft_kc_get_geometries(uft_kc_system_t system,
                                         const uft_kc_geometry_t** geoms, int max) {
    int count = 0;
    for (int i = 0; UFT_KC_GEOMETRIES[i].name != NULL && count < max; i++) {
        if (UFT_KC_GEOMETRIES[i].system == system) {
            geoms[count++] = &UFT_KC_GEOMETRIES[i];
        }
    }
    return count;
}

/**
 * @brief Count total geometries
 */
static inline int uft_kc_count_geometries(void) {
    int count = 0;
    for (int i = 0; UFT_KC_GEOMETRIES[i].name != NULL; i++) {
        count++;
    }
    return count;
}

/**
 * @brief Probe for MicroDOS signature
 */
static inline bool uft_kc_is_microdos(const uint8_t* data, size_t size) {
    if (!data || size < 16) return false;
    
    /* Check for MICRODOS string at offset 3 */
    if (memcmp(data + 3, "MICRODOS", 8) == 0) return true;
    
    /* Alternative: check for jump instruction and valid BPB */
    if (data[0] == 0xC3 || data[0] == 0xE9) {
        const uft_microdos_boot_t* boot = (const uft_microdos_boot_t*)data;
        if (boot->bytes_per_sector == 512 &&
            boot->sectors_per_track >= 5 &&
            boot->sectors_per_track <= 18) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Probe KC85/Z1013 format
 * @return Confidence score (0-100)
 */
static inline int uft_kc85_probe(const uint8_t* data, size_t size) {
    if (!data || size < 512) return 0;
    
    int score = 0;
    
    /* Check for MicroDOS */
    if (uft_kc_is_microdos(data, size)) {
        score += 60;
    }
    
    /* Check size matches known geometries */
    for (int i = 0; UFT_KC_GEOMETRIES[i].name != NULL; i++) {
        if (UFT_KC_GEOMETRIES[i].total_size == size) {
            score += 20;
            break;
        }
    }
    
    /* KC85 D004 specific: check for CAOS structures */
    if (size == 200 * 1024 || size == 400 * 1024 || size == 720 * 1024) {
        score += 10;
    }
    
    /* Z1013/KC87 specific sizes */
    if (size == 160 * 1024 || size == 320 * 1024) {
        score += 10;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Detect specific KC system from disk image
 */
static inline uft_kc_system_t uft_kc_detect_system(const uint8_t* data, size_t size) {
    if (!data || size < 512) return UFT_KC_SYSTEM_UNKNOWN;
    
    /* Check for KC compact EDSK header */
    if (size > 256 && memcmp(data, "EXTENDED", 8) == 0) {
        return UFT_KC_SYSTEM_KC_COMPACT;
    }
    if (size > 256 && memcmp(data, "MV - CPC", 8) == 0) {
        return UFT_KC_SYSTEM_KC_COMPACT;
    }
    
    /* Check size-based detection */
    switch (size) {
        case 200 * 1024:
            return UFT_KC_SYSTEM_KC85_4;
        case 400 * 1024:
            return UFT_KC_SYSTEM_KC85_5;
        case 720 * 1024:
            return UFT_KC_SYSTEM_KC85_5;
        case 160 * 1024:
            /* Could be Z1013, KC87, or Z9001 */
            return UFT_KC_SYSTEM_Z1013;
        case 320 * 1024:
            return UFT_KC_SYSTEM_Z1013;
        case 250 * 1024:
        case 1000 * 1024:
            return UFT_KC_SYSTEM_PC_M;
        case 800 * 1024:
            return UFT_KC_SYSTEM_LLC2;
        default:
            break;
    }
    
    return UFT_KC_SYSTEM_UNKNOWN;
}

/**
 * @brief Print geometry info
 */
static inline void uft_kc_print_geometry(const uft_kc_geometry_t* geom) {
    if (!geom) return;
    printf("KC/Z1013 Disk Geometry:\n");
    printf("  Name:        %s\n", geom->name);
    printf("  System:      %s\n", uft_kc_system_name(geom->system));
    printf("  Disk Type:   %s\n", uft_kc_disk_type_name(geom->disk_type));
    printf("  Tracks:      %d\n", geom->tracks);
    printf("  Sides:       %d\n", geom->sides);
    printf("  Sect/Track:  %d\n", geom->sectors_per_track);
    printf("  Sector Size: %d\n", geom->sector_size);
    printf("  Total Size:  %d KB\n", geom->total_size / 1024);
    printf("  Description: %s\n", geom->description);
}

/**
 * @brief List all geometries
 */
static inline void uft_kc_list_geometries(void) {
    printf("KC85/Z1013 Disk Geometries:\n");
    printf("%-18s  %-12s  %-10s  %s\n", "Name", "System", "Size", "Description");
    printf("─────────────────────────────────────────────────────────────────────────\n");
    
    for (int i = 0; UFT_KC_GEOMETRIES[i].name != NULL; i++) {
        const uft_kc_geometry_t* g = &UFT_KC_GEOMETRIES[i];
        printf("%-18s  %-12s  %4d KB     %s\n",
               g->name,
               uft_kc_system_name(g->system),
               g->total_size / 1024,
               g->description);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_KC85_FORMAT_H */

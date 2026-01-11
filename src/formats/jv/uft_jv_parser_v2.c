/**
 * @file uft_jv_parser_v2.c
 * @brief GOD MODE JV1/JV3 Parser v2 - TRS-80 Disk Formats
 * 
 * JV1: Simple sector dump (single density only)
 * - 256 bytes per sector
 * - 10 sectors per track
 * - Single density FM encoding
 * - No header/metadata
 * 
 * JV3: Extended format with sector headers
 * - Mixed density support (FM/MFM)
 * - Variable sector sizes
 * - DAM (Data Address Mark) preservation
 * - CRC error flags
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

/* ═══════════════════════════════════════════════════════════════════════════
 * JV1 CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define JV1_SECTOR_SIZE         256
#define JV1_SECTORS_PER_TRACK   10
#define JV1_TRACK_SIZE          (JV1_SECTOR_SIZE * JV1_SECTORS_PER_TRACK)  /* 2560 */

/* Common JV1 sizes */
#define JV1_SIZE_35T_SS         (35 * JV1_TRACK_SIZE)   /* 89600 - SSSD */
#define JV1_SIZE_40T_SS         (40 * JV1_TRACK_SIZE)   /* 102400 - SSSD */
#define JV1_SIZE_35T_DS         (35 * 2 * JV1_TRACK_SIZE)  /* 179200 - DSSD */
#define JV1_SIZE_40T_DS         (40 * 2 * JV1_TRACK_SIZE)  /* 204800 - DSSD */

/* ═══════════════════════════════════════════════════════════════════════════
 * JV3 CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define JV3_HEADER_SECTORS      2901    /* Max sectors in header */
#define JV3_HEADER_SIZE         (JV3_HEADER_SECTORS * 3 + 1)  /* 8704 bytes */
#define JV3_ENTRY_SIZE          3

/* Sector sizes */
#define JV3_SIZE_128            0
#define JV3_SIZE_256            1
#define JV3_SIZE_512            2
#define JV3_SIZE_1024           3

/* Flags in sector entry */
#define JV3_FLAG_DOUBLE_DENSITY 0x80    /* MFM if set, FM if clear */
#define JV3_FLAG_DAM            0x60    /* Data Address Mark bits */
#define JV3_FLAG_SIDE           0x10    /* Side 1 if set */
#define JV3_FLAG_CRC_ERROR      0x08    /* CRC error flag */
#define JV3_FLAG_NON_IBM        0x04    /* Non-IBM sector */
#define JV3_FLAG_SIZE_MASK      0x03    /* Sector size code */

/* DAM types */
#define JV3_DAM_NORMAL_SD       0xFB    /* Normal data (SD) */
#define JV3_DAM_DELETED_SD      0xF8    /* Deleted data (SD) */
#define JV3_DAM_NORMAL_DD       0xFB    /* Normal data (DD) */
#define JV3_DAM_DELETED_DD      0xF8    /* Deleted data (DD) */

/* Special marker */
#define JV3_FREE_ENTRY          0xFF    /* Free sector entry */

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    JV_FORMAT_JV1 = 1,
    JV_FORMAT_JV3 = 3,
    JV_FORMAT_UNKNOWN = 0
} jv_format_t;

/**
 * @brief JV3 sector entry (3 bytes)
 */
typedef struct {
    uint8_t track;            /* Physical track */
    uint8_t sector;           /* Sector number */
    uint8_t flags;            /* Flags and size */
} jv3_sector_entry_t;

/**
 * @brief Parsed sector info
 */
typedef struct {
    uint8_t track;
    uint8_t sector;
    uint8_t side;
    uint16_t size;            /* 128, 256, 512, or 1024 */
    bool double_density;      /* MFM */
    uint8_t dam;              /* Data Address Mark */
    bool crc_error;
    bool non_ibm;
    uint32_t data_offset;     /* Offset in file to sector data */
    bool present;
} jv_sector_t;

/**
 * @brief Track info
 */
typedef struct {
    uint8_t track_num;
    uint8_t side;
    uint8_t sector_count;
    bool double_density;
    jv_sector_t sectors[26];  /* Max sectors per track */
} jv_track_t;

/**
 * @brief JV disk structure
 */
typedef struct {
    jv_format_t format;
    
    /* Geometry */
    uint8_t num_tracks;
    uint8_t num_sides;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    bool double_density;
    
    /* JV3 specific */
    uint16_t total_sectors;
    jv_sector_t* sectors;     /* Sector table for JV3 */
    uint8_t write_protect;    /* JV3 write protect byte */
    
    /* Track info */
    jv_track_t* tracks;
    uint16_t track_count;
    
    /* Statistics */
    uint16_t sd_sectors;      /* Single density count */
    uint16_t dd_sectors;      /* Double density count */
    uint16_t error_sectors;   /* Sectors with CRC errors */
    
    /* Raw data */
    const uint8_t* raw_data;
    size_t raw_size;
    
    bool valid;
    char error[256];
} jv_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint16_t jv3_size_from_code(uint8_t code) {
    static const uint16_t sizes[] = { 256, 128, 1024, 512 };
    return sizes[code & 0x03];
}

static const char* jv3_size_name(uint8_t code) {
    static const char* names[] = { "256", "128", "1024", "512" };
    return names[code & 0x03];
}

static bool jv1_is_valid_size(size_t size, uint8_t* tracks, uint8_t* sides) {
    /* JV1 must be multiple of track size */
    if (size % JV1_TRACK_SIZE != 0) return false;
    if (size < JV1_TRACK_SIZE) return false;
    
    uint32_t total_tracks = size / JV1_TRACK_SIZE;
    
    /* Common single-sided formats */
    if (total_tracks <= 80) {
        *tracks = total_tracks;
        *sides = 1;
        return true;
    }
    
    return false;
}

static bool jv3_is_valid(const uint8_t* data, size_t size) {
    if (size < JV3_HEADER_SIZE) return false;
    
    /* Check for valid sector entries */
    int valid_entries = 0;
    int free_entries = 0;
    
    for (int i = 0; i < JV3_HEADER_SECTORS && i * 3 < (int)size - 3; i++) {
        uint8_t track = data[i * 3];
        uint8_t sector = data[i * 3 + 1];
        
        if (track == JV3_FREE_ENTRY && sector == JV3_FREE_ENTRY) {
            free_entries++;
        } else if (track < 80 && sector < 30) {
            valid_entries++;
        }
    }
    
    /* JV3 should have at least some valid entries */
    return valid_entries > 10;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * JV1 PARSING
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool jv1_parse(const uint8_t* data, size_t size, jv_disk_t* disk) {
    memset(disk, 0, sizeof(jv_disk_t));
    
    if (!jv1_is_valid_size(size, &disk->num_tracks, &disk->num_sides)) {
        snprintf(disk->error, sizeof(disk->error),
                 "Invalid JV1 size: %zu bytes", size);
        return false;
    }
    
    disk->format = JV_FORMAT_JV1;
    disk->sectors_per_track = JV1_SECTORS_PER_TRACK;
    disk->sector_size = JV1_SECTOR_SIZE;
    disk->double_density = false;  /* JV1 is always SD */
    disk->sd_sectors = disk->num_tracks * disk->num_sides * disk->sectors_per_track;
    disk->total_sectors = disk->sd_sectors;
    
    disk->raw_data = data;
    disk->raw_size = size;
    disk->valid = true;
    
    return true;
}

static const uint8_t* jv1_read_sector(const jv_disk_t* disk,
                                       uint8_t track, uint8_t side, uint8_t sector) {
    if (!disk->valid || disk->format != JV_FORMAT_JV1) return NULL;
    if (track >= disk->num_tracks) return NULL;
    if (side >= disk->num_sides) return NULL;
    if (sector >= disk->sectors_per_track) return NULL;
    
    size_t offset;
    if (disk->num_sides == 1) {
        offset = (track * JV1_SECTORS_PER_TRACK + sector) * JV1_SECTOR_SIZE;
    } else {
        /* Interleaved sides */
        offset = ((track * 2 + side) * JV1_SECTORS_PER_TRACK + sector) * JV1_SECTOR_SIZE;
    }
    
    if (offset + JV1_SECTOR_SIZE > disk->raw_size) return NULL;
    
    return disk->raw_data + offset;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * JV3 PARSING
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool jv3_parse(const uint8_t* data, size_t size, jv_disk_t* disk) {
    memset(disk, 0, sizeof(jv_disk_t));
    
    if (!jv3_is_valid(data, size)) {
        snprintf(disk->error, sizeof(disk->error), "Invalid JV3 format");
        return false;
    }
    
    disk->format = JV_FORMAT_JV3;
    disk->raw_data = data;
    disk->raw_size = size;
    
    /* Count sectors and calculate data offset */
    disk->total_sectors = 0;
    uint32_t data_offset = JV3_HEADER_SIZE;
    uint8_t max_track = 0;
    uint8_t max_side = 0;
    
    /* Allocate sector table */
    disk->sectors = calloc(JV3_HEADER_SECTORS, sizeof(jv_sector_t));
    if (!disk->sectors) {
        snprintf(disk->error, sizeof(disk->error), "Memory allocation failed");
        return false;
    }
    
    /* Parse sector entries */
    for (int i = 0; i < JV3_HEADER_SECTORS; i++) {
        const uint8_t* entry = data + i * 3;
        
        if (entry[0] == JV3_FREE_ENTRY && entry[1] == JV3_FREE_ENTRY) {
            break;  /* End of entries */
        }
        
        jv_sector_t* sec = &disk->sectors[disk->total_sectors];
        
        sec->track = entry[0];
        sec->sector = entry[1];
        sec->side = (entry[2] & JV3_FLAG_SIDE) ? 1 : 0;
        sec->double_density = (entry[2] & JV3_FLAG_DOUBLE_DENSITY) != 0;
        sec->crc_error = (entry[2] & JV3_FLAG_CRC_ERROR) != 0;
        sec->non_ibm = (entry[2] & JV3_FLAG_NON_IBM) != 0;
        sec->size = jv3_size_from_code(entry[2]);
        sec->data_offset = data_offset;
        sec->present = true;
        
        /* Extract DAM */
        uint8_t dam_bits = (entry[2] & JV3_FLAG_DAM) >> 5;
        if (sec->double_density) {
            sec->dam = (dam_bits == 0) ? JV3_DAM_NORMAL_DD : JV3_DAM_DELETED_DD;
        } else {
            sec->dam = (dam_bits == 0) ? JV3_DAM_NORMAL_SD : JV3_DAM_DELETED_SD;
        }
        
        /* Track statistics */
        if (sec->track > max_track) max_track = sec->track;
        if (sec->side > max_side) max_side = sec->side;
        if (sec->double_density) {
            disk->dd_sectors++;
        } else {
            disk->sd_sectors++;
        }
        if (sec->crc_error) {
            disk->error_sectors++;
        }
        
        data_offset += sec->size;
        disk->total_sectors++;
    }
    
    disk->num_tracks = max_track + 1;
    disk->num_sides = max_side + 1;
    disk->double_density = (disk->dd_sectors > disk->sd_sectors);
    
    /* Write protect byte */
    if (JV3_HEADER_SIZE - 1 < size) {
        disk->write_protect = data[JV3_HEADER_SIZE - 1];
    }
    
    disk->valid = true;
    return true;
}

static const uint8_t* jv3_read_sector(const jv_disk_t* disk,
                                       uint8_t track, uint8_t side, uint8_t sector) {
    if (!disk->valid || disk->format != JV_FORMAT_JV3) return NULL;
    if (!disk->sectors) return NULL;
    
    /* Find sector in table */
    for (uint16_t i = 0; i < disk->total_sectors; i++) {
        const jv_sector_t* sec = &disk->sectors[i];
        if (sec->track == track && sec->side == side && sec->sector == sector) {
            if (sec->data_offset + sec->size > disk->raw_size) return NULL;
            return disk->raw_data + sec->data_offset;
        }
    }
    
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * UNIFIED INTERFACE
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool jv_parse(const uint8_t* data, size_t size, jv_disk_t* disk) {
    /* Try JV3 first (has header structure) */
    if (jv3_is_valid(data, size)) {
        return jv3_parse(data, size, disk);
    }
    
    /* Fall back to JV1 */
    return jv1_parse(data, size, disk);
}

static const uint8_t* jv_read_sector(const jv_disk_t* disk,
                                      uint8_t track, uint8_t side, uint8_t sector) {
    if (disk->format == JV_FORMAT_JV1) {
        return jv1_read_sector(disk, track, side, sector);
    } else if (disk->format == JV_FORMAT_JV3) {
        return jv3_read_sector(disk, track, side, sector);
    }
    return NULL;
}

static void jv_free(jv_disk_t* disk) {
    if (disk) {
        if (disk->sectors) {
            free(disk->sectors);
            disk->sectors = NULL;
        }
        if (disk->tracks) {
            free(disk->tracks);
            disk->tracks = NULL;
        }
    }
}

static char* jv_info_to_text(const jv_disk_t* disk) {
    size_t buf_size = 2048;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    
    snprintf(buf, buf_size,
        "TRS-80 Disk Image\n"
        "═════════════════\n"
        "Format: JV%d\n"
        "Tracks: %u\n"
        "Sides: %u\n"
        "Total Sectors: %u\n"
        "SD Sectors: %u\n"
        "DD Sectors: %u\n"
        "Error Sectors: %u\n"
        "Density: %s\n",
        disk->format,
        disk->num_tracks,
        disk->num_sides,
        disk->total_sectors,
        disk->sd_sectors,
        disk->dd_sectors,
        disk->error_sectors,
        disk->double_density ? "Double (MFM)" : "Single (FM)"
    );
    
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef JV_PARSER_TEST

#include <assert.h>

int main(void) {
    printf("=== JV1/JV3 Parser v2 Tests ===\n");
    
    printf("Testing JV1 size validation... ");
    uint8_t tracks, sides;
    assert(jv1_is_valid_size(JV1_SIZE_35T_SS, &tracks, &sides));
    assert(tracks == 35 && sides == 1);
    assert(jv1_is_valid_size(JV1_SIZE_40T_SS, &tracks, &sides));
    assert(tracks == 40 && sides == 1);
    assert(!jv1_is_valid_size(12345, &tracks, &sides));
    printf("✓\n");
    
    printf("Testing JV3 size codes... ");
    assert(jv3_size_from_code(JV3_SIZE_128) == 128);
    assert(jv3_size_from_code(JV3_SIZE_256) == 256);
    assert(jv3_size_from_code(JV3_SIZE_512) == 512);
    assert(jv3_size_from_code(JV3_SIZE_1024) == 1024);
    printf("✓\n");
    
    printf("Testing JV1 parsing... ");
    uint8_t* jv1_data = calloc(1, JV1_SIZE_35T_SS);
    assert(jv1_data != NULL);
    
    jv_disk_t disk;
    assert(jv1_parse(jv1_data, JV1_SIZE_35T_SS, &disk));
    assert(disk.valid);
    assert(disk.format == JV_FORMAT_JV1);
    assert(disk.num_tracks == 35);
    assert(disk.num_sides == 1);
    
    jv_free(&disk);
    free(jv1_data);
    printf("✓\n");
    
    printf("Testing JV3 validation... ");
    uint8_t jv3_header[JV3_HEADER_SIZE + 1000];
    memset(jv3_header, 0xFF, sizeof(jv3_header));
    /* Add some valid entries */
    for (int i = 0; i < 20; i++) {
        jv3_header[i * 3] = 0;      /* Track 0 */
        jv3_header[i * 3 + 1] = i;  /* Sector i */
        jv3_header[i * 3 + 2] = JV3_SIZE_256;  /* 256 bytes, SD */
    }
    assert(jv3_is_valid(jv3_header, sizeof(jv3_header)));
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* JV_PARSER_TEST */

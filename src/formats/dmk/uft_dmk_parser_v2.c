/**
 * @file uft_dmk_parser_v2.c
 * @brief GOD MODE DMK Parser v2 - TRS-80 Raw Track Format
 * 
 * DMK stores raw track data with timing information, preserving
 * copy protection and non-standard formats.
 * 
 * Structure:
 * - 16-byte file header
 * - Track data (with IDAM pointer table)
 * - Supports mixed SD/DD sectors
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
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DMK_HEADER_SIZE         16
#define DMK_IDAM_TABLE_SIZE     128     /* 64 IDAM pointers (2 bytes each) */
#define DMK_MAX_TRACK_SIZE      0x2940  /* Maximum raw track length (10560) */
#define DMK_SD_TRACK_SIZE       0x0CC0  /* Single density track (3264) */
#define DMK_DD_TRACK_SIZE       0x1900  /* Double density track (6400) */

/* Header byte offsets */
#define DMK_HDR_WRITE_PROTECT   0x00
#define DMK_HDR_TRACKS          0x01
#define DMK_HDR_TRACK_LEN_LO    0x02
#define DMK_HDR_TRACK_LEN_HI    0x03
#define DMK_HDR_FLAGS           0x04

/* Flags */
#define DMK_FLAG_SINGLE_SIDED   0x10
#define DMK_FLAG_SINGLE_DENSITY 0x40
#define DMK_FLAG_IGNORE_DENSITY 0x80

/* IDAM pointer flags */
#define DMK_IDAM_DOUBLE_DENSITY 0x8000
#define DMK_IDAM_OFFSET_MASK    0x3FFF

/* Sector size codes (from ID field) */
#define DMK_SIZE_128            0
#define DMK_SIZE_256            1
#define DMK_SIZE_512            2
#define DMK_SIZE_1024           3
#define DMK_SIZE_2048           4
#define DMK_SIZE_4096           5
#define DMK_SIZE_8192           6
#define DMK_SIZE_16384          7

/* FM/MFM marks */
#define DMK_FM_IDAM             0xFE
#define DMK_FM_DAM              0xFB
#define DMK_FM_DAM_DELETED      0xF8
#define DMK_MFM_IDAM            0xFE
#define DMK_MFM_DAM             0xFB
#define DMK_MFM_DAM_DELETED     0xF8

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief DMK file header
 */
typedef struct {
    uint8_t write_protect;    /* 0xFF = protected, 0x00 = writable */
    uint8_t num_tracks;       /* Number of tracks (per side) */
    uint16_t track_length;    /* Track length including IDAM table */
    uint8_t flags;            /* Option flags */
    uint8_t reserved[11];     /* Reserved */
} dmk_header_t;

/**
 * @brief Sector ID field (IDAM)
 */
typedef struct {
    uint8_t track;            /* Track number */
    uint8_t side;             /* Side number */
    uint8_t sector;           /* Sector number */
    uint8_t size_code;        /* Size code (0=128, 1=256, etc.) */
    uint16_t crc;             /* Header CRC */
    bool crc_valid;
} dmk_sector_id_t;

/**
 * @brief Sector data
 */
typedef struct {
    dmk_sector_id_t id;
    uint16_t idam_offset;     /* Offset in track */
    bool double_density;      /* MFM sector */
    uint8_t dam;              /* Data Address Mark */
    bool deleted;             /* Deleted data mark */
    uint8_t* data;            /* Pointer to sector data */
    uint16_t data_size;       /* Sector data size */
    uint16_t data_crc;
    bool data_crc_valid;
    bool present;
} dmk_sector_t;

/**
 * @brief Track data
 */
typedef struct {
    uint8_t track_num;
    uint8_t side;
    uint16_t raw_length;      /* Raw track data length */
    uint8_t sector_count;
    dmk_sector_t sectors[64]; /* Max sectors per track */
    bool double_density;
    uint8_t* raw_data;        /* Raw track data (after IDAM table) */
} dmk_track_t;

/**
 * @brief DMK disk structure
 */
typedef struct {
    dmk_header_t header;
    
    /* Parsed info */
    uint8_t num_tracks;
    uint8_t num_sides;
    uint16_t track_length;
    bool single_sided;
    bool single_density;
    bool write_protected;
    
    /* Tracks */
    dmk_track_t* tracks;
    uint16_t track_count;
    
    /* Statistics */
    uint16_t total_sectors;
    uint16_t sd_sectors;
    uint16_t dd_sectors;
    uint16_t deleted_sectors;
    uint16_t error_sectors;
    
    /* Raw data */
    const uint8_t* raw_data;
    size_t raw_size;
    
    bool valid;
    char error[256];
} dmk_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint16_t read_le16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

static uint16_t dmk_size_from_code(uint8_t code) {
    if (code > 7) return 256;  /* Default */
    return 128 << code;
}

static bool dmk_is_valid(const uint8_t* data, size_t size) {
    if (size < DMK_HEADER_SIZE) return false;
    
    /* Check for reasonable values */
    uint8_t tracks = data[DMK_HDR_TRACKS];
    uint16_t track_len = read_le16(data + DMK_HDR_TRACK_LEN_LO);
    
    if (tracks == 0 || tracks > 96) return false;
    if (track_len < DMK_IDAM_TABLE_SIZE || track_len > DMK_MAX_TRACK_SIZE) return false;
    
    /* Verify file size */
    uint8_t sides = (data[DMK_HDR_FLAGS] & DMK_FLAG_SINGLE_SIDED) ? 1 : 2;
    size_t expected = DMK_HEADER_SIZE + (size_t)tracks * sides * track_len;
    
    /* Allow some tolerance */
    if (size < expected - track_len) return false;
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint16_t dmk_crc_init(bool double_density) {
    return double_density ? 0xCDB4 : 0xFFFF;
}

static uint16_t dmk_crc_update(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool dmk_parse_track(const uint8_t* track_data, uint16_t track_len,
                            uint8_t track_num, uint8_t side, dmk_track_t* track) {
    memset(track, 0, sizeof(dmk_track_t));
    
    track->track_num = track_num;
    track->side = side;
    track->raw_length = track_len - DMK_IDAM_TABLE_SIZE;
    track->raw_data = (uint8_t*)(track_data + DMK_IDAM_TABLE_SIZE);
    
    /* Parse IDAM pointer table */
    for (int i = 0; i < 64; i++) {
        uint16_t idam_ptr = read_le16(track_data + i * 2);
        
        if (idam_ptr == 0) break;  /* End of table */
        
        dmk_sector_t* sec = &track->sectors[track->sector_count];
        
        sec->double_density = (idam_ptr & DMK_IDAM_DOUBLE_DENSITY) != 0;
        sec->idam_offset = idam_ptr & DMK_IDAM_OFFSET_MASK;
        
        /* Validate offset */
        if (sec->idam_offset < DMK_IDAM_TABLE_SIZE ||
            sec->idam_offset >= track_len - 10) {
            continue;
        }
        
        /* Read sector ID (offset points to IDAM byte, ID follows) */
        const uint8_t* id_data = track_data + sec->idam_offset;
        
        if (*id_data == DMK_FM_IDAM || *id_data == DMK_MFM_IDAM) {
            sec->id.track = id_data[1];
            sec->id.side = id_data[2];
            sec->id.sector = id_data[3];
            sec->id.size_code = id_data[4];
            sec->id.crc = (id_data[5] << 8) | id_data[6];
            
            sec->data_size = dmk_size_from_code(sec->id.size_code);
            sec->present = true;
            
            /* Find DAM (should be within ~43 bytes of IDAM in MFM, ~30 in FM) */
            int search_limit = sec->double_density ? 60 : 40;
            const uint8_t* search = id_data + 7;
            
            for (size_t j = 0; j < search_limit && search < track_data + track_len - 1; j++, search++) {
                if (*search == DMK_FM_DAM || *search == DMK_MFM_DAM) {
                    sec->dam = *search;
                    sec->deleted = false;
                    sec->data = (uint8_t*)(search + 1);
                    break;
                } else if (*search == DMK_FM_DAM_DELETED || *search == DMK_MFM_DAM_DELETED) {
                    sec->dam = *search;
                    sec->deleted = true;
                    sec->data = (uint8_t*)(search + 1);
                    break;
                }
            }
            
            track->sector_count++;
            
            if (sec->double_density) {
                track->double_density = true;
            }
        }
    }
    
    return true;
}

static bool dmk_parse(const uint8_t* data, size_t size, dmk_disk_t* disk) {
    memset(disk, 0, sizeof(dmk_disk_t));
    
    if (!dmk_is_valid(data, size)) {
        snprintf(disk->error, sizeof(disk->error), "Invalid DMK format");
        return false;
    }
    
    disk->raw_data = data;
    disk->raw_size = size;
    
    /* Parse header */
    disk->header.write_protect = data[DMK_HDR_WRITE_PROTECT];
    disk->header.num_tracks = data[DMK_HDR_TRACKS];
    disk->header.track_length = read_le16(data + DMK_HDR_TRACK_LEN_LO);
    disk->header.flags = data[DMK_HDR_FLAGS];
    
    disk->num_tracks = disk->header.num_tracks;
    disk->track_length = disk->header.track_length;
    disk->single_sided = (disk->header.flags & DMK_FLAG_SINGLE_SIDED) != 0;
    disk->single_density = (disk->header.flags & DMK_FLAG_SINGLE_DENSITY) != 0;
    disk->write_protected = (disk->header.write_protect == 0xFF);
    disk->num_sides = disk->single_sided ? 1 : 2;
    
    /* Allocate tracks */
    disk->track_count = disk->num_tracks * disk->num_sides;
    disk->tracks = calloc(disk->track_count, sizeof(dmk_track_t));
    if (!disk->tracks) {
        snprintf(disk->error, sizeof(disk->error), "Memory allocation failed");
        return false;
    }
    
    /* Parse tracks */
    size_t offset = DMK_HEADER_SIZE;
    int track_idx = 0;
    
    for (uint8_t t = 0; t < disk->num_tracks; t++) {
        for (uint8_t s = 0; s < disk->num_sides; s++) {
            if (offset + disk->track_length > size) break;
            
            dmk_track_t* track = &disk->tracks[track_idx];
            dmk_parse_track(data + offset, disk->track_length, t, s, track);
            
            /* Update statistics */
            for (int i = 0; i < track->sector_count; i++) {
                disk->total_sectors++;
                if (track->sectors[i].double_density) {
                    disk->dd_sectors++;
                } else {
                    disk->sd_sectors++;
                }
                if (track->sectors[i].deleted) {
                    disk->deleted_sectors++;
                }
            }
            
            offset += disk->track_length;
            track_idx++;
        }
    }
    
    disk->valid = true;
    return true;
}

static const uint8_t* dmk_read_sector(const dmk_disk_t* disk,
                                       uint8_t track, uint8_t side, uint8_t sector) {
    if (!disk->valid || !disk->tracks) return NULL;
    
    /* Find track */
    for (uint16_t t = 0; t < disk->track_count; t++) {
        const dmk_track_t* trk = &disk->tracks[t];
        if (trk->track_num != track || trk->side != side) continue;
        
        /* Find sector */
        for (int s = 0; s < trk->sector_count; s++) {
            if (trk->sectors[s].id.sector == sector) {
                return trk->sectors[s].data;
            }
        }
    }
    
    return NULL;
}

static void dmk_free(dmk_disk_t* disk) {
    if (disk && disk->tracks) {
        free(disk->tracks);
        disk->tracks = NULL;
    }
}

static char* dmk_info_to_text(const dmk_disk_t* disk) {
    size_t buf_size = 4096;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    
    size_t offset = 0;
    
    offset += snprintf(buf + offset, buf_size - offset,
        "TRS-80 DMK Disk Image\n"
        "═════════════════════\n"
        "Tracks: %u\n"
        "Sides: %u\n"
        "Track Length: %u bytes\n"
        "Write Protected: %s\n"
        "Total Sectors: %u\n"
        "SD Sectors: %u\n"
        "DD Sectors: %u\n"
        "Deleted Sectors: %u\n"
        "Density: %s\n\n",
        disk->num_tracks,
        disk->num_sides,
        disk->track_length,
        disk->write_protected ? "Yes" : "No",
        disk->total_sectors,
        disk->sd_sectors,
        disk->dd_sectors,
        disk->deleted_sectors,
        disk->dd_sectors > disk->sd_sectors ? "Mixed/Double" : "Single"
    );
    
    /* Track summary */
    offset += snprintf(buf + offset, buf_size - offset, "Track Map:\n");
    
    for (uint16_t t = 0; t < disk->track_count && offset < buf_size - 100; t++) {
        const dmk_track_t* track = &disk->tracks[t];
        
        offset += snprintf(buf + offset, buf_size - offset,
            "  T%02u.%u: %2u sectors%s\n",
            track->track_num,
            track->side,
            track->sector_count,
            track->double_density ? " (DD)" : " (SD)"
        );
    }
    
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef DMK_PARSER_TEST

#include <assert.h>

int main(void) {
    printf("=== DMK Parser v2 Tests ===\n");
    
    printf("Testing size_from_code... ");
    assert(dmk_size_from_code(0) == 128);
    assert(dmk_size_from_code(1) == 256);
    assert(dmk_size_from_code(2) == 512);
    assert(dmk_size_from_code(3) == 1024);
    printf("✓\n");
    
    printf("Testing CRC init... ");
    assert(dmk_crc_init(false) == 0xFFFF);
    assert(dmk_crc_init(true) == 0xCDB4);
    printf("✓\n");
    
    printf("Testing header validation... ");
    /* Valid header */
    uint8_t valid_header[16] = {
        0x00,       /* Write protect */
        35,         /* Tracks */
        0x00, 0x19, /* Track length = 0x1900 */
        0x00,       /* Flags (double-sided, double-density) */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  /* Reserved */
    };
    
    /* Create minimal valid DMK */
    size_t data_size = 16 + 35 * 2 * 0x1900;
    uint8_t* dmk_data = calloc(1, data_size);
    memcpy(dmk_data, valid_header, 16);
    
    assert(dmk_is_valid(dmk_data, data_size));
    free(dmk_data);
    printf("✓\n");
    
    printf("Testing invalid detection... ");
    uint8_t invalid_header[16] = { 0 };
    assert(!dmk_is_valid(invalid_header, 16));
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* DMK_PARSER_TEST */

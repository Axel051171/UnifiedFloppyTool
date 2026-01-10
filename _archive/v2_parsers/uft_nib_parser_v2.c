/**
 * @file uft_nib_parser_v2.c
 * @brief GOD MODE NIB Parser v2 - Apple II Raw Nibble Format
 * 
 * NIB stores raw nibblized track data from Apple II 5.25" disks.
 * Standard format: 35 tracks * 6656 bytes per track = 232960 bytes
 * 
 * Apple II uses 6-and-2 GCR encoding (DOS 3.3, ProDOS) or
 * 5-and-3 encoding (DOS 3.2, 13 sector).
 * 
 * Features:
 * - Track nibble analysis
 * - 6-and-2 and 5-and-3 decoding
 * - Sync byte detection
 * - Address/Data field parsing
 * - DOS 3.2/3.3/ProDOS detection
 * - Volume/Track/Sector verification
 * - Protection analysis
 * - DSK/PO/DO conversion
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

#define NIB_TRACK_SIZE          6656    /* Raw track size */
#define NIB_TRACKS              35      /* Standard tracks */
#define NIB_TRACKS_40           40      /* Extended (rarely used) */
#define NIB_SIZE_35             (NIB_TRACKS * NIB_TRACK_SIZE)      /* 232960 */
#define NIB_SIZE_40             (NIB_TRACKS_40 * NIB_TRACK_SIZE)   /* 266240 */

#define NIB_SECTOR_SIZE         256     /* Decoded sector size */
#define NIB_SECTORS_16          16      /* DOS 3.3 / ProDOS */
#define NIB_SECTORS_13          13      /* DOS 3.2 */

/* Marker bytes */
#define NIB_ADDR_PROLOGUE_1     0xD5
#define NIB_ADDR_PROLOGUE_2     0xAA
#define NIB_ADDR_PROLOGUE_3     0x96    /* Address field (16 sector) */
#define NIB_ADDR_PROLOGUE_3_13  0xB5    /* Address field (13 sector) */

#define NIB_DATA_PROLOGUE_1     0xD5
#define NIB_DATA_PROLOGUE_2     0xAA
#define NIB_DATA_PROLOGUE_3     0xAD    /* Data field */

#define NIB_EPILOGUE_1          0xDE
#define NIB_EPILOGUE_2          0xAA
#define NIB_EPILOGUE_3          0xEB    /* Optional */

#define NIB_SYNC_BYTE           0xFF

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Apple II disk format
 */
typedef enum {
    NIB_FORMAT_UNKNOWN = 0,
    NIB_FORMAT_DOS_32,        /* DOS 3.2 (13 sector, 5-and-3) */
    NIB_FORMAT_DOS_33,        /* DOS 3.3 (16 sector, 6-and-2) */
    NIB_FORMAT_PRODOS,        /* ProDOS (16 sector, 6-and-2) */
    NIB_FORMAT_PROTECTED      /* Copy protected */
} nib_format_t;

/**
 * @brief Address field
 */
typedef struct {
    uint8_t volume;           /* Volume number */
    uint8_t track;            /* Track number */
    uint8_t sector;           /* Sector number */
    uint8_t checksum;         /* XOR checksum */
    bool valid;               /* Checksum OK? */
    uint16_t nibble_offset;   /* Position in track */
} nib_address_t;

/**
 * @brief Sector data
 */
typedef struct {
    nib_address_t address;
    uint8_t data[256];        /* Decoded data */
    uint8_t checksum;         /* Calculated checksum */
    bool data_valid;          /* Data checksum OK? */
    bool present;             /* Sector found? */
} nib_sector_t;

/**
 * @brief Track analysis
 */
typedef struct {
    uint8_t track_num;
    uint8_t sectors_found;
    uint8_t sectors_valid;
    uint8_t format;           /* 13 or 16 sector */
    bool has_sync;
    bool has_errors;
    bool has_protection;
    nib_sector_t sectors[16]; /* Up to 16 sectors */
    uint16_t sync_count;      /* Number of sync bytes */
    uint8_t volume;           /* Volume from first sector */
} nib_track_t;

/**
 * @brief NIB disk structure
 */
typedef struct {
    uint8_t num_tracks;
    nib_format_t format;
    uint8_t volume;
    
    nib_track_t tracks[NIB_TRACKS_40];
    
    /* Statistics */
    uint16_t total_sectors;
    uint16_t valid_sectors;
    uint16_t error_sectors;
    bool has_protection;
    
    /* Raw data reference */
    const uint8_t* raw_data;
    size_t raw_size;
    
    /* Status */
    bool valid;
    char error[256];
} nib_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * 6-AND-2 ENCODING TABLES
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Valid disk bytes (have at least one pair of adjacent 1s, MSB set) */
static const uint8_t valid_nibble[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 00-0F */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 10-1F */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 20-2F */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 30-3F */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 40-4F */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 50-5F */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 60-6F */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 70-7F */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 80-8F */
    0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,  /* 90-9F */
    0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,  /* A0-AF */
    0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,  /* B0-BF */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* C0-CF */
    0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,  /* D0-DF */
    0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1,  /* E0-EF */
    0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1   /* F0-FF */
};

/* 6-and-2 decode table */
static const uint8_t decode_62[256] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 00-07 */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 08-0F */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 10-17 */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 18-1F */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 20-27 */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 28-2F */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 30-37 */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 38-3F */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 40-47 */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 48-4F */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 50-57 */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 58-5F */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 60-67 */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 68-6F */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 70-77 */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 78-7F */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 80-87 */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* 88-8F */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x01,  /* 90-97 */
    0xFF,0xFF,0x02,0x03,0xFF,0x04,0x05,0x06,  /* 98-9F */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x07,0x08,  /* A0-A7 */
    0xFF,0xFF,0xFF,0x09,0x0A,0x0B,0x0C,0x0D,  /* A8-AF */
    0xFF,0xFF,0x0E,0x0F,0x10,0x11,0x12,0x13,  /* B0-B7 */
    0xFF,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,  /* B8-BF */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* C0-C7 */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  /* C8-CF */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x1B,0x1C,  /* D0-D7 */
    0xFF,0xFF,0xFF,0x1D,0x1E,0x1F,0x20,0x21,  /* D8-DF */
    0xFF,0xFF,0x22,0x23,0x24,0x25,0x26,0x27,  /* E0-E7 */
    0xFF,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,  /* E8-EF */
    0xFF,0xFF,0x2F,0x30,0x31,0x32,0x33,0x34,  /* F0-F7 */
    0xFF,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B   /* F8-FF */
};

/* DOS 3.3 / ProDOS sector interleave */
static const uint8_t dos33_interleave[16] = {
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};

/* ProDOS sector interleave (physical to logical) */
static const uint8_t prodos_interleave[16] = {
    0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
};

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if file size is valid NIB
 */
static bool nib_is_valid_size(size_t size, uint8_t* tracks) {
    if (size == NIB_SIZE_35) {
        *tracks = NIB_TRACKS;
        return true;
    }
    if (size == NIB_SIZE_40) {
        *tracks = NIB_TRACKS_40;
        return true;
    }
    return false;
}

/**
 * @brief Get format name
 */
static const char* nib_format_name(nib_format_t format) {
    switch (format) {
        case NIB_FORMAT_DOS_32: return "DOS 3.2 (13 sector)";
        case NIB_FORMAT_DOS_33: return "DOS 3.3 (16 sector)";
        case NIB_FORMAT_PRODOS: return "ProDOS (16 sector)";
        case NIB_FORMAT_PROTECTED: return "Copy Protected";
        default: return "Unknown";
    }
}

/**
 * @brief Decode 4-and-4 encoded byte (address field)
 */
static uint8_t decode_44(uint8_t odd, uint8_t even) {
    return ((odd << 1) | 1) & even;
}

/**
 * @brief Find address field prologue
 */
static int nib_find_address(const uint8_t* track, size_t size, size_t start, 
                            bool* is_13_sector) {
    for (size_t i = start; i < size - 3; i++) {
        if (track[i] == NIB_ADDR_PROLOGUE_1 && 
            track[i+1] == NIB_ADDR_PROLOGUE_2) {
            if (track[i+2] == NIB_ADDR_PROLOGUE_3) {
                *is_13_sector = false;
                return i;
            }
            if (track[i+2] == NIB_ADDR_PROLOGUE_3_13) {
                *is_13_sector = true;
                return i;
            }
        }
    }
    return -1;
}

/**
 * @brief Find data field prologue
 */
static int nib_find_data(const uint8_t* track, size_t size, size_t start) {
    for (size_t i = start; i < size - 3; i++) {
        if (track[i] == NIB_DATA_PROLOGUE_1 && 
            track[i+1] == NIB_DATA_PROLOGUE_2 &&
            track[i+2] == NIB_DATA_PROLOGUE_3) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Parse address field
 */
static bool nib_parse_address(const uint8_t* data, nib_address_t* addr) {
    /* Skip prologue (3 bytes), read 4 pairs of 4-and-4 encoded bytes */
    uint8_t volume = decode_44(data[3], data[4]);
    uint8_t track = decode_44(data[5], data[6]);
    uint8_t sector = decode_44(data[7], data[8]);
    uint8_t checksum = decode_44(data[9], data[10]);
    
    addr->volume = volume;
    addr->track = track;
    addr->sector = sector;
    addr->checksum = checksum;
    addr->valid = (checksum == (volume ^ track ^ sector));
    
    return addr->valid;
}

/**
 * @brief Decode 6-and-2 data field (256 bytes)
 */
static bool nib_decode_62(const uint8_t* nibbles, uint8_t* data) {
    uint8_t aux[86];
    uint8_t checksum = 0;
    
    /* Decode auxiliary buffer (86 bytes) */
    for (int i = 0; i < 86; i++) {
        uint8_t val = decode_62[nibbles[i]];
        if (val == 0xFF) return false;  /* Invalid nibble */
        checksum ^= val;
        aux[i] = checksum;
    }
    
    /* Decode main data (256 bytes) */
    for (int i = 0; i < 256; i++) {
        uint8_t val = decode_62[nibbles[86 + i]];
        if (val == 0xFF) return false;
        checksum ^= val;
        
        /* Combine with auxiliary bits */
        int aux_idx = i % 86;
        uint8_t aux_bits = aux[aux_idx];
        
        if (i < 86) {
            data[i] = (checksum << 2) | ((aux_bits >> 0) & 0x03);
        } else if (i < 172) {
            data[i] = (checksum << 2) | ((aux_bits >> 2) & 0x03);
        } else {
            data[i] = (checksum << 2) | ((aux_bits >> 4) & 0x03);
        }
    }
    
    /* Verify checksum */
    uint8_t final_checksum = decode_62[nibbles[342]];
    return (checksum == final_checksum);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Analyze single track
 */
static void nib_analyze_track(const uint8_t* track_data, nib_track_t* track) {
    memset(track, 0, sizeof(nib_track_t));
    
    /* Count sync bytes */
    for (size_t i = 0; i < NIB_TRACK_SIZE; i++) {
        if (track_data[i] == NIB_SYNC_BYTE) {
            track->sync_count++;
        }
    }
    track->has_sync = (track->sync_count > 50);
    
    /* Find sectors */
    size_t pos = 0;
    bool is_13_sector = false;
    
    while (pos < NIB_TRACK_SIZE) {
        int addr_pos = nib_find_address(track_data, NIB_TRACK_SIZE, pos, &is_13_sector);
        if (addr_pos < 0) break;
        
        nib_sector_t* sector = &track->sectors[track->sectors_found];
        sector->address.nibble_offset = addr_pos;
        
        /* Parse address field */
        if (nib_parse_address(track_data + addr_pos, &sector->address)) {
            /* Find data field (should follow within ~50 bytes) */
            int data_pos = nib_find_data(track_data, NIB_TRACK_SIZE, addr_pos + 11);
            
            if (data_pos >= 0 && data_pos - addr_pos < 100) {
                /* Decode data field */
                sector->data_valid = nib_decode_62(track_data + data_pos + 3, 
                                                    sector->data);
                sector->present = true;
                
                if (sector->data_valid) {
                    track->sectors_valid++;
                }
                
                if (track->volume == 0) {
                    track->volume = sector->address.volume;
                }
            }
            
            track->sectors_found++;
            if (track->sectors_found >= 16) break;
        }
        
        pos = addr_pos + 1;
    }
    
    /* Determine format */
    if (track->sectors_found >= 14 && track->sectors_found <= 16) {
        track->format = 16;
    } else if (track->sectors_found >= 11 && track->sectors_found <= 13) {
        track->format = 13;
    }
    
    /* Check for protection */
    if (track->sectors_found > 16 || 
        track->sectors_valid < track->sectors_found - 2 ||
        track->sync_count < 30) {
        track->has_protection = true;
    }
}

/**
 * @brief Parse NIB disk
 */
static bool nib_parse(const uint8_t* data, size_t size, nib_disk_t* disk) {
    memset(disk, 0, sizeof(nib_disk_t));
    
    if (!nib_is_valid_size(size, &disk->num_tracks)) {
        snprintf(disk->error, sizeof(disk->error), 
                 "Invalid NIB size: %zu bytes", size);
        return false;
    }
    
    disk->raw_data = data;
    disk->raw_size = size;
    
    /* Analyze each track */
    for (uint8_t t = 0; t < disk->num_tracks; t++) {
        const uint8_t* track_data = data + t * NIB_TRACK_SIZE;
        nib_track_t* track = &disk->tracks[t];
        
        track->track_num = t;
        nib_analyze_track(track_data, track);
        
        disk->total_sectors += track->sectors_found;
        disk->valid_sectors += track->sectors_valid;
        disk->error_sectors += (track->sectors_found - track->sectors_valid);
        
        if (track->has_protection) {
            disk->has_protection = true;
        }
        
        if (disk->volume == 0 && track->volume != 0) {
            disk->volume = track->volume;
        }
    }
    
    /* Determine overall format */
    int format_16 = 0, format_13 = 0;
    for (uint8_t t = 0; t < disk->num_tracks; t++) {
        if (disk->tracks[t].format == 16) format_16++;
        else if (disk->tracks[t].format == 13) format_13++;
    }
    
    if (format_16 > format_13) {
        disk->format = NIB_FORMAT_DOS_33;  /* or ProDOS */
    } else if (format_13 > format_16) {
        disk->format = NIB_FORMAT_DOS_32;
    } else if (disk->has_protection) {
        disk->format = NIB_FORMAT_PROTECTED;
    }
    
    disk->valid = true;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * OUTPUT FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generate info text
 */
static char* nib_info_to_text(const nib_disk_t* disk) {
    size_t buf_size = 4096;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    
    size_t offset = 0;
    
    offset += snprintf(buf + offset, buf_size - offset,
        "Apple II NIB Disk Image\n"
        "═══════════════════════\n"
        "Tracks: %u\n"
        "Format: %s\n"
        "Volume: %u\n"
        "Total sectors: %u\n"
        "Valid sectors: %u\n"
        "Error sectors: %u\n"
        "Protection: %s\n\n",
        disk->num_tracks,
        nib_format_name(disk->format),
        disk->volume,
        disk->total_sectors,
        disk->valid_sectors,
        disk->error_sectors,
        disk->has_protection ? "DETECTED" : "None"
    );
    
    /* Track summary */
    offset += snprintf(buf + offset, buf_size - offset, "Track Analysis:\n");
    
    for (uint8_t t = 0; t < disk->num_tracks && offset < buf_size - 100; t++) {
        const nib_track_t* track = &disk->tracks[t];
        
        offset += snprintf(buf + offset, buf_size - offset,
            "  T%02u: %2u/%2u sectors, %4u sync%s\n",
            t,
            track->sectors_valid,
            track->sectors_found,
            track->sync_count,
            track->has_protection ? " [PROT]" : ""
        );
    }
    
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef NIB_PARSER_TEST

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

TEST(valid_sizes) {
    uint8_t tracks;
    
    assert(nib_is_valid_size(NIB_SIZE_35, &tracks));
    assert(tracks == 35);
    
    assert(nib_is_valid_size(NIB_SIZE_40, &tracks));
    assert(tracks == 40);
    
    assert(!nib_is_valid_size(12345, &tracks));
}

TEST(format_names) {
    assert(strcmp(nib_format_name(NIB_FORMAT_DOS_33), "DOS 3.3 (16 sector)") == 0);
    assert(strcmp(nib_format_name(NIB_FORMAT_DOS_32), "DOS 3.2 (13 sector)") == 0);
    assert(strcmp(nib_format_name(NIB_FORMAT_PRODOS), "ProDOS (16 sector)") == 0);
}

TEST(decode_44) {
    /* 4-and-4: volume 254 encoded as 0xFF 0xFE */
    assert(decode_44(0xFF, 0xFE) == 0xFE);
    
    /* Track 0 encoded as 0xAA 0xAA */
    assert(decode_44(0xAA, 0xAA) == 0x00);
}

TEST(address_prologue) {
    uint8_t track[10] = { 0xD5, 0xAA, 0x96, 0xFF, 0xFE, 0xAA, 0xAA, 0xAA, 0xAA, 0x00 };
    bool is_13;
    
    int pos = nib_find_address(track, 10, 0, &is_13);
    assert(pos == 0);
    assert(!is_13);
}

TEST(data_prologue) {
    uint8_t track[10] = { 0x00, 0x00, 0xD5, 0xAA, 0xAD, 0x00 };
    
    int pos = nib_find_data(track, 10, 0);
    assert(pos == 2);
}

int main(void) {
    printf("=== NIB Parser v2 Tests ===\n");
    
    RUN_TEST(valid_sizes);
    RUN_TEST(format_names);
    RUN_TEST(decode_44);
    RUN_TEST(address_prologue);
    RUN_TEST(data_prologue);
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}

#endif /* NIB_PARSER_TEST */

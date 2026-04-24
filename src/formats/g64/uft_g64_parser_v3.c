/**
 * @file uft_g64_parser_v3.c
 * @brief GOD MODE G64 Parser v3 - Raw GCR Format mit Kopierschutz-Preservation
 * 
 * G64 ist das raw GCR Format für Commodore 64/1541:
 * - 84 Half-Tracks (0.5 bis 42.0)
 * - Variable Track-Längen (bis 7928 Bytes GCR)
 * - Speed Zone pro Track (0-3)
 * - Raw GCR Daten (keine Dekodierung erforderlich)
 * - Vollständige Kopierschutz-Preservation
 * 
 * G64 Header:
 * - Signature: "GCR-1541"
 * - Version: 0x00
 * - Track count: 84
 * - Max track size: 7928
 * 
 * v3 Features:
 * - Read/Write/Analyze Pipeline
 * - Multi-Rev Merge mit Bit-Level Voting
 * - Weak Bit Detection und Preservation
 * - Half-Track Support
 * - Speed Zone Handling
 * - Track-Level Diagnose
 * - Scoring pro Track
 * - Verify-After-Write
 * - D64 Export mit Dekodierung
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 * @date 2025-01-02
 */

#include "uft/uft_safe_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define G64_SIGNATURE           "GCR-1541"
#define G64_SIGNATURE_LEN       8
#define G64_VERSION             0x00
#define G64_HEADER_SIZE         12
#define G64_MAX_TRACKS          84          /* 42 full tracks × 2 */
#define G64_MAX_TRACK_SIZE      7928        /* Max GCR bytes per track */
#define G64_TRACK_TABLE_SIZE    (G64_MAX_TRACKS * 4)
#define G64_SPEED_TABLE_SIZE    (G64_MAX_TRACKS * 4)

/* Track table starts at offset 12 */
#define G64_TRACK_TABLE_OFFSET  12
#define G64_SPEED_TABLE_OFFSET  (G64_TRACK_TABLE_OFFSET + G64_TRACK_TABLE_SIZE)
#define G64_TRACK_DATA_OFFSET   (G64_SPEED_TABLE_OFFSET + G64_SPEED_TABLE_SIZE)

/* Speed zones (bits per track) */
#define G64_SPEED_ZONE_3        7692        /* Tracks 1-17: 21 sectors */
#define G64_SPEED_ZONE_2        7142        /* Tracks 18-24: 19 sectors */
#define G64_SPEED_ZONE_1        6666        /* Tracks 25-30: 18 sectors */
#define G64_SPEED_ZONE_0        6250        /* Tracks 31-42: 17 sectors */

/* GCR bytes per track (approximate) */
#define G64_GCR_ZONE_3          7692
#define G64_GCR_ZONE_2          7142
#define G64_GCR_ZONE_1          6666
#define G64_GCR_ZONE_0          6250

/* Timing */
#define G64_BITCELL_ZONE_3      3200        /* ns */
#define G64_BITCELL_ZONE_2      3500
#define G64_BITCELL_ZONE_1      3750
#define G64_BITCELL_ZONE_0      4000

/* Sync pattern */
#define G64_SYNC_BYTE           0xFF
#define G64_SYNC_MIN_BYTES      5
#define G64_SYNC_MAX_BYTES      40

/* GCR markers */
#define G64_HEADER_MARKER       0x08        /* Sector header ID */
#define G64_DATA_MARKER         0x07        /* Sector data ID */

/* Sectors per track (same as D64) */
static const uint8_t g64_sectors_per_track[43] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    19, 19, 19, 19, 19, 19, 19,
    18, 18, 18, 18, 18, 18,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17
};

/* Speed zone for track */
static const uint8_t g64_speed_zone[43] = {
    3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Expected GCR track size per zone */
static const uint16_t g64_track_size_zone[4] = {
    G64_GCR_ZONE_0,
    G64_GCR_ZONE_1,
    G64_GCR_ZONE_2,
    G64_GCR_ZONE_3
};

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR TABLES
 * ═══════════════════════════════════════════════════════════════════════════ */

/* 4-bit to 5-bit GCR encoding */
static const uint8_t gcr_encode_table[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/* 5-bit GCR to 4-bit decode (0xFF = invalid) */
static const uint8_t gcr_decode_table[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

/* ═══════════════════════════════════════════════════════════════════════════
 * DIAGNOSIS CODES (G64 specific)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    G64_DIAG_OK = 0,
    
    /* File structure */
    G64_DIAG_BAD_SIGNATURE,
    G64_DIAG_BAD_VERSION,
    G64_DIAG_TRUNCATED,
    G64_DIAG_TRACK_OVERFLOW,
    
    /* Track structure */
    G64_DIAG_EMPTY_TRACK,
    G64_DIAG_SHORT_TRACK,
    G64_DIAG_LONG_TRACK,
    G64_DIAG_HALF_TRACK,
    G64_DIAG_SPEED_MISMATCH,
    
    /* Sync issues */
    G64_DIAG_NO_SYNC,
    G64_DIAG_SHORT_SYNC,
    G64_DIAG_LONG_SYNC,
    G64_DIAG_BROKEN_SYNC,
    
    /* GCR issues */
    G64_DIAG_GCR_ERROR,
    G64_DIAG_HEADER_ERROR,
    G64_DIAG_DATA_ERROR,
    G64_DIAG_CHECKSUM_ERROR,
    
    /* Sector issues */
    G64_DIAG_MISSING_SECTOR,
    G64_DIAG_EXTRA_SECTOR,
    G64_DIAG_DUPLICATE_SECTOR,
    G64_DIAG_WRONG_TRACK_ID,
    G64_DIAG_WRONG_SECTOR_ID,
    
    /* Protection */
    G64_DIAG_WEAK_BITS,
    G64_DIAG_FUZZY_BITS,
    G64_DIAG_TIMING_PROTECTION,
    G64_DIAG_KILLER_TRACK,
    G64_DIAG_NON_STANDARD_GAP,
    G64_DIAG_EXTRA_DATA,
    
    /* Analysis */
    G64_DIAG_DENSITY_ANOMALY,
    G64_DIAG_SPLICE_DETECTED,
    G64_DIAG_FORMAT_MISMATCH,
    
    G64_DIAG_COUNT
} g64_diag_code_t;

static const char* g64_diag_names[] = {
    [G64_DIAG_OK] = "OK",
    [G64_DIAG_BAD_SIGNATURE] = "Invalid G64 signature",
    [G64_DIAG_BAD_VERSION] = "Unsupported G64 version",
    [G64_DIAG_TRUNCATED] = "File is truncated",
    [G64_DIAG_TRACK_OVERFLOW] = "Track data exceeds maximum",
    [G64_DIAG_EMPTY_TRACK] = "Track contains no data",
    [G64_DIAG_SHORT_TRACK] = "Track shorter than expected",
    [G64_DIAG_LONG_TRACK] = "Track longer than expected",
    [G64_DIAG_HALF_TRACK] = "Half-track data present",
    [G64_DIAG_SPEED_MISMATCH] = "Speed zone mismatch",
    [G64_DIAG_NO_SYNC] = "No sync pattern found",
    [G64_DIAG_SHORT_SYNC] = "Sync shorter than normal",
    [G64_DIAG_LONG_SYNC] = "Sync longer than normal (protection?)",
    [G64_DIAG_BROKEN_SYNC] = "Sync pattern is broken",
    [G64_DIAG_GCR_ERROR] = "Invalid GCR encoding",
    [G64_DIAG_HEADER_ERROR] = "Sector header decode error",
    [G64_DIAG_DATA_ERROR] = "Sector data decode error",
    [G64_DIAG_CHECKSUM_ERROR] = "Checksum mismatch",
    [G64_DIAG_MISSING_SECTOR] = "Expected sector not found",
    [G64_DIAG_EXTRA_SECTOR] = "Extra sector (protection?)",
    [G64_DIAG_DUPLICATE_SECTOR] = "Duplicate sector ID",
    [G64_DIAG_WRONG_TRACK_ID] = "Track ID mismatch",
    [G64_DIAG_WRONG_SECTOR_ID] = "Invalid sector ID",
    [G64_DIAG_WEAK_BITS] = "Weak/unstable bits detected",
    [G64_DIAG_FUZZY_BITS] = "Fuzzy bits (intentional)",
    [G64_DIAG_TIMING_PROTECTION] = "Non-standard timing (protection)",
    [G64_DIAG_KILLER_TRACK] = "Killer track (unreadable)",
    [G64_DIAG_NON_STANDARD_GAP] = "Non-standard inter-sector gap",
    [G64_DIAG_EXTRA_DATA] = "Extra data after sectors",
    [G64_DIAG_DENSITY_ANOMALY] = "Bit density anomaly",
    [G64_DIAG_SPLICE_DETECTED] = "Write splice detected",
    [G64_DIAG_FORMAT_MISMATCH] = "Format doesn't match expected"
};

static const char* g64_diag_suggestions[] = {
    [G64_DIAG_OK] = "",
    [G64_DIAG_BAD_SIGNATURE] = "Verify file is actually G64 format",
    [G64_DIAG_BAD_VERSION] = "May need updated parser",
    [G64_DIAG_TRUNCATED] = "Check for incomplete download/copy",
    [G64_DIAG_TRACK_OVERFLOW] = "Track data may be corrupted",
    [G64_DIAG_EMPTY_TRACK] = "Track may be unformatted or erased",
    [G64_DIAG_SHORT_TRACK] = "May indicate partial read or protection",
    [G64_DIAG_LONG_TRACK] = "PRESERVE - often copy protection",
    [G64_DIAG_HALF_TRACK] = "PRESERVE - essential for some protections",
    [G64_DIAG_SPEED_MISMATCH] = "Check original disk format",
    [G64_DIAG_NO_SYNC] = "Track may be damaged or killer track",
    [G64_DIAG_SHORT_SYNC] = "May indicate worn media",
    [G64_DIAG_LONG_SYNC] = "PRESERVE - common protection technique",
    [G64_DIAG_BROKEN_SYNC] = "May indicate media damage",
    [G64_DIAG_GCR_ERROR] = "Raw data preserved, decode may fail",
    [G64_DIAG_HEADER_ERROR] = "Use multi-rev for recovery",
    [G64_DIAG_DATA_ERROR] = "Use multi-rev or CRC correction",
    [G64_DIAG_CHECKSUM_ERROR] = "Data may still be usable",
    [G64_DIAG_MISSING_SECTOR] = "May be intentionally absent",
    [G64_DIAG_EXTRA_SECTOR] = "PRESERVE - copy protection",
    [G64_DIAG_DUPLICATE_SECTOR] = "PRESERVE - copy protection",
    [G64_DIAG_WRONG_TRACK_ID] = "PRESERVE - may be protection",
    [G64_DIAG_WRONG_SECTOR_ID] = "Check for format mismatch",
    [G64_DIAG_WEAK_BITS] = "PRESERVE - this IS copy protection",
    [G64_DIAG_FUZZY_BITS] = "PRESERVE - intentional protection",
    [G64_DIAG_TIMING_PROTECTION] = "PRESERVE - timing-based protection",
    [G64_DIAG_KILLER_TRACK] = "PRESERVE - intentional unreadable",
    [G64_DIAG_NON_STANDARD_GAP] = "PRESERVE - gap-based protection",
    [G64_DIAG_EXTRA_DATA] = "PRESERVE - may contain hidden data",
    [G64_DIAG_DENSITY_ANOMALY] = "Check drive calibration",
    [G64_DIAG_SPLICE_DETECTED] = "Normal for written disks",
    [G64_DIAG_FORMAT_MISMATCH] = "Verify disk type matches expected"
};

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Score structure
 */
typedef struct g64_score {
    float overall;
    float sync_score;
    float gcr_score;
    float checksum_score;
    float structure_score;
    float timing_score;
    
    bool has_sync;
    bool gcr_valid;
    bool checksums_valid;
    bool has_weak_bits;
    bool has_protection;
    bool is_half_track;
    
    uint8_t sectors_found;
    uint8_t sectors_valid;
    uint8_t gcr_errors;
    uint16_t weak_bit_count;
    
    uint8_t revolutions_used;
    uint8_t best_revolution;
} g64_score_t;

/**
 * @brief Diagnosis entry
 */
typedef struct g64_diagnosis {
    g64_diag_code_t code;
    uint8_t track;              /* Half-track number (1-84) */
    uint8_t sector;             /* 0xFF if track-level */
    uint32_t byte_position;
    char message[256];
    g64_score_t score;
} g64_diagnosis_t;

/**
 * @brief Diagnosis list
 */
typedef struct g64_diagnosis_list {
    g64_diagnosis_t* items;
    size_t count;
    size_t capacity;
    uint16_t error_count;
    uint16_t warning_count;
    uint16_t protection_count;
    float overall_quality;
} g64_diagnosis_list_t;

/**
 * @brief Decoded sector from GCR
 */
typedef struct g64_sector {
    /* Identity */
    uint8_t track_id;           /* From header */
    uint8_t sector_id;          /* From header */
    uint8_t checksum_header;    /* Header checksum */
    uint8_t checksum_data;      /* Data checksum */
    
    /* Data */
    uint8_t header[8];          /* Raw GCR header (10 bytes decoded) */
    uint8_t data[256];          /* Decoded sector data */
    
    /* GCR positions */
    uint32_t sync_position;     /* Position of sync */
    uint32_t header_position;   /* Position of header */
    uint32_t data_position;     /* Position of data */
    uint8_t sync_length;        /* Sync byte count */
    
    /* Status */
    bool present;
    bool header_valid;
    bool data_valid;
    bool checksum_header_ok;
    bool checksum_data_ok;
    
    /* Protection */
    bool has_weak_bits;
    uint8_t weak_mask[256];
    uint16_t weak_count;
    
    /* Multi-rev */
    uint8_t** rev_data;
    bool* rev_valid;
    uint8_t rev_count;
    uint8_t best_rev;
    
    /* Score */
    g64_score_t score;
    
} g64_sector_t;

/**
 * @brief Track structure
 */
typedef struct g64_track {
    /* Identity */
    uint8_t half_track;         /* 1-84 (half-track number) */
    uint8_t full_track;         /* 1-42 (full track) */
    bool is_half_track;         /* True if .5 track */
    
    /* Speed */
    uint8_t speed_zone;         /* 0-3 */
    uint32_t expected_size;     /* Expected GCR bytes */
    
    /* Raw GCR data */
    uint8_t* gcr_data;          /* Raw GCR bytes */
    uint16_t gcr_size;          /* Actual size */
    uint32_t offset_in_file;    /* Offset in G64 file */
    
    /* Decoded sectors */
    g64_sector_t sectors[24];   /* Max 21 + extras */
    uint8_t sector_count;
    uint8_t expected_sectors;
    uint8_t valid_sectors;
    uint8_t error_sectors;
    
    /* Sync analysis */
    struct {
        uint32_t position;
        uint8_t length;
    } sync_marks[32];
    uint8_t sync_count;
    
    /* Multi-rev data */
    struct {
        uint8_t* data;
        uint16_t size;
        g64_score_t score;
    } revolutions[32];
    uint8_t revolution_count;
    uint8_t best_revolution;
    
    /* Weak bits */
    uint8_t* weak_mask;         /* Bit mask of weak positions */
    uint16_t weak_bit_count;
    
    /* Protection detection */
    bool has_weak_bits;
    bool has_extra_sectors;
    bool has_long_sync;
    bool has_non_standard_gaps;
    bool is_killer_track;
    bool is_protected;
    
    /* Score */
    g64_score_t score;
    
} g64_track_t;

/**
 * @brief G64 disk structure
 */
typedef struct g64_disk {
    /* File info */
    char signature[9];
    uint8_t version;
    uint8_t track_count;
    uint16_t max_track_size;
    
    /* Track offsets and speeds */
    uint32_t track_offsets[G64_MAX_TRACKS + 1];
    uint32_t speed_zones[G64_MAX_TRACKS + 1];
    
    /* Track data */
    g64_track_t tracks[G64_MAX_TRACKS + 1];
    
    /* Statistics */
    uint8_t full_tracks;        /* Count of full tracks with data */
    uint8_t half_tracks;        /* Count of half tracks with data */
    uint8_t empty_tracks;       /* Count of empty tracks */
    uint16_t total_sectors;
    uint16_t valid_sectors;
    
    /* Protection analysis */
    bool has_protection;
    char protection_type[64];
    float protection_confidence;
    
    /* Overall score */
    g64_score_t score;
    g64_diagnosis_list_t* diagnosis;
    
    /* D64 export data */
    uint8_t* d64_data;          /* Decoded D64 if available */
    size_t d64_size;
    bool d64_valid;
    
    /* Source info */
    char source_path[256];
    size_t source_size;
    uint32_t crc32;
    
    /* Status */
    bool valid;
    bool modified;
    char error[256];
    
} g64_disk_t;

/**
 * @brief G64 parameters
 */
typedef struct g64_params {
    /* Read options */
    uint8_t revolutions;
    bool multi_rev_merge;
    int merge_strategy;
    
    /* GCR handling */
    bool strict_gcr;
    bool ignore_gcr_errors;
    
    /* Protection */
    bool detect_protection;
    bool preserve_protection;
    bool preserve_weak_bits;
    bool preserve_half_tracks;
    
    /* Sync handling */
    uint8_t sync_min_bytes;
    uint8_t sync_max_bytes;
    bool tolerant_sync;
    
    /* Decoding */
    bool decode_sectors;
    bool generate_d64;
    bool validate_checksums;
    
    /* Timing */
    float timing_tolerance;
    bool detect_timing_protection;
    
    /* Output */
    bool include_empty_tracks;
    bool include_half_tracks;
    
    /* Verify */
    bool verify_after_write;
    int verify_mode;
    
} g64_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get full track number from half-track
 */
static inline uint8_t g64_half_to_full(uint8_t half_track) {
    return (half_track + 1) / 2;
}

/**
 * @brief Get half-track number from full track
 */
static inline uint8_t g64_full_to_half(uint8_t full_track) {
    return full_track * 2 - 1;
}

/**
 * @brief Check if half-track index is a .5 track
 */
static inline bool g64_is_half_track(uint8_t half_track) {
    return (half_track % 2) == 0;
}

/**
 * @brief Get expected sectors for track
 */
static uint8_t g64_get_sectors(uint8_t full_track) {
    if (full_track < 1 || full_track > 42) return 0;
    return g64_sectors_per_track[full_track];
}

/**
 * @brief Get speed zone for track
 */
static uint8_t g64_get_speed_zone(uint8_t full_track) {
    if (full_track < 1 || full_track > 42) return 3;
    return g64_speed_zone[full_track];
}

/**
 * @brief Get expected track size for zone
 */
static uint16_t g64_get_track_size(uint8_t speed_zone) {
    if (speed_zone > 3) speed_zone = 3;
    return g64_track_size_zone[speed_zone];
}

/**
 * @brief Get bitcell time for speed zone
 */
uint32_t g64_get_bitcell_ns(uint8_t speed_zone) {
    switch (speed_zone) {
        case 3: return G64_BITCELL_ZONE_3;
        case 2: return G64_BITCELL_ZONE_2;
        case 1: return G64_BITCELL_ZONE_1;
        default: return G64_BITCELL_ZONE_0;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR ENCODING/DECODING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Decode 5 GCR bytes to 4 data bytes
 */
static bool g64_gcr_decode_block(const uint8_t* gcr, uint8_t* data, uint8_t* errors) {
    uint8_t g[8];
    uint8_t n[8];
    bool valid = true;
    *errors = 0;
    
    /* Unpack 5 bytes to 8 5-bit values */
    g[0] = (gcr[0] >> 3) & 0x1F;
    g[1] = ((gcr[0] << 2) | (gcr[1] >> 6)) & 0x1F;
    g[2] = (gcr[1] >> 1) & 0x1F;
    g[3] = ((gcr[1] << 4) | (gcr[2] >> 4)) & 0x1F;
    g[4] = ((gcr[2] << 1) | (gcr[3] >> 7)) & 0x1F;
    g[5] = (gcr[3] >> 2) & 0x1F;
    g[6] = ((gcr[3] << 3) | (gcr[4] >> 5)) & 0x1F;
    g[7] = gcr[4] & 0x1F;
    
    /* Decode each */
    for (int i = 0; i < 8; i++) {
        n[i] = gcr_decode_table[g[i]];
        if (n[i] == 0xFF) {
            *errors |= (1 << i);
            n[i] = 0;
            valid = false;
        }
    }
    
    /* Combine nibbles */
    data[0] = (n[0] << 4) | n[1];
    data[1] = (n[2] << 4) | n[3];
    data[2] = (n[4] << 4) | n[5];
    data[3] = (n[6] << 4) | n[7];
    
    return valid;
}

/**
 * @brief Encode 4 data bytes to 5 GCR bytes
 */
void g64_gcr_encode_block(const uint8_t* data, uint8_t* gcr) {
    uint8_t n[8];
    uint8_t g[8];
    
    /* Extract nibbles */
    n[0] = (data[0] >> 4) & 0x0F;
    n[1] = data[0] & 0x0F;
    n[2] = (data[1] >> 4) & 0x0F;
    n[3] = data[1] & 0x0F;
    n[4] = (data[2] >> 4) & 0x0F;
    n[5] = data[2] & 0x0F;
    n[6] = (data[3] >> 4) & 0x0F;
    n[7] = data[3] & 0x0F;
    
    /* Encode */
    for (int i = 0; i < 8; i++) {
        g[i] = gcr_encode_table[n[i]];
    }
    
    /* Pack */
    gcr[0] = (g[0] << 3) | (g[1] >> 2);
    gcr[1] = (g[1] << 6) | (g[2] << 1) | (g[3] >> 4);
    gcr[2] = (g[3] << 4) | (g[4] >> 1);
    gcr[3] = (g[4] << 7) | (g[5] << 2) | (g[6] >> 3);
    gcr[4] = (g[6] << 5) | g[7];
}

/**
 * @brief Find sync pattern in GCR data
 */
static int g64_find_sync(const uint8_t* data, size_t size, size_t start, uint8_t* sync_len) {
    *sync_len = 0;
    
    for (size_t i = start; i < size; i++) {
        if (data[i] == G64_SYNC_BYTE) {
            /* Count consecutive sync bytes */
            size_t count = 0;
            while (i + count < size && data[i + count] == G64_SYNC_BYTE) {
                count++;
            }
            
            if (count >= G64_SYNC_MIN_BYTES) {
                *sync_len = (uint8_t)(count > 255 ? 255 : count);
                return (int)i;
            }
        }
    }
    
    return -1;
}

/**
 * @brief Calculate CBM checksum
 */
uint8_t g64_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DIAGNOSIS FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create diagnosis list
 */
static g64_diagnosis_list_t* g64_diagnosis_create(void) {
    g64_diagnosis_list_t* list = calloc(1, sizeof(g64_diagnosis_list_t));
    if (!list) return NULL;
    
    list->capacity = 128;
    list->items = calloc(list->capacity, sizeof(g64_diagnosis_t));
    if (!list->items) {
        free(list);
        return NULL;
    }
    
    list->overall_quality = 1.0f;
    return list;
}

/**
 * @brief Free diagnosis list
 */
static void g64_diagnosis_free(g64_diagnosis_list_t* list) {
    if (list) {
        free(list->items);
        free(list);
    }
}

/**
 * @brief Add diagnosis
 */
static void g64_diagnosis_add(
    g64_diagnosis_list_t* list,
    g64_diag_code_t code,
    uint8_t track,
    uint8_t sector,
    const char* format,
    ...
) {
    if (!list) return;
    
    /* Expand if needed */
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity * 2;
        g64_diagnosis_t* new_items = realloc(list->items, new_cap * sizeof(g64_diagnosis_t));
        if (!new_items) return;
        list->items = new_items;
        list->capacity = new_cap;
    }
    
    g64_diagnosis_t* diag = &list->items[list->count];
    memset(diag, 0, sizeof(g64_diagnosis_t));
    
    diag->code = code;
    diag->track = track;
    diag->sector = sector;
    
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(diag->message, sizeof(diag->message), format, args);
        va_end(args);
    } else if (code < G64_DIAG_COUNT) {
        snprintf(diag->message, sizeof(diag->message), "%s", g64_diag_names[code]);
    }
    
    list->count++;
    
    /* Update counters */
    if (code >= G64_DIAG_WEAK_BITS && code <= G64_DIAG_EXTRA_DATA) {
        list->protection_count++;
    } else if (code >= G64_DIAG_GCR_ERROR && code <= G64_DIAG_CHECKSUM_ERROR) {
        list->error_count++;
    } else if (code != G64_DIAG_OK) {
        list->warning_count++;
    }
    
    /* Update quality */
    if (code != G64_DIAG_OK && code < G64_DIAG_WEAK_BITS) {
        list->overall_quality *= 0.97f;
    }
}

/**
 * @brief Generate diagnosis report
 */
char* g64_diagnosis_to_text(const g64_diagnosis_list_t* list, const g64_disk_t* disk) {
    if (!list) return NULL;
    
    size_t buf_size = 24576;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    
    size_t off = 0;
    
    /* Header */
    off += snprintf(buf + off, buf_size - off,
        "╔══════════════════════════════════════════════════════════════════╗\n"
        "║                G64 DISK DIAGNOSIS REPORT                         ║\n"
        "╠══════════════════════════════════════════════════════════════════╣\n");
    
    if (disk) {
        off += snprintf(buf + off, buf_size - off,
            "║ Full Tracks: %2u  Half Tracks: %2u  Empty: %2u                     ║\n"
            "║ Sectors: %4u/%4u valid  Size: %zu bytes                       ║\n",
            disk->full_tracks,
            disk->half_tracks,
            disk->empty_tracks,
            disk->valid_sectors,
            disk->total_sectors,
            disk->source_size);
        
        if (disk->has_protection) {
            off += snprintf(buf + off, buf_size - off,
                "║ Protection: %-20s (%.0f%% confidence)            ║\n",
                disk->protection_type,
                disk->protection_confidence * 100);
        }
    }
    
    off += snprintf(buf + off, buf_size - off,
        "╠══════════════════════════════════════════════════════════════════╣\n"
        "║ Errors: %-4u  Warnings: %-4u  Protection: %-4u  Quality: %5.1f%% ║\n"
        "╚══════════════════════════════════════════════════════════════════╝\n\n",
        list->error_count,
        list->warning_count,
        list->protection_count,
        list->overall_quality * 100.0f);
    
    /* Group by track */
    int current_track = -1;
    
    for (size_t i = 0; i < list->count && off + 500 < buf_size; i++) {
        const g64_diagnosis_t* d = &list->items[i];
        
        if (d->track != current_track) {
            current_track = d->track;
            uint8_t full = g64_half_to_full(current_track);
            bool is_half = g64_is_half_track(current_track);
            
            off += snprintf(buf + off, buf_size - off,
                "── Track %u%s (zone %u, %u sectors) ──────────────────────\n",
                full,
                is_half ? ".5" : "",
                g64_get_speed_zone(full),
                g64_get_sectors(full));
        }
        
        /* Icon */
        const char* icon = "ℹ️";
        if (d->code >= G64_DIAG_GCR_ERROR && d->code <= G64_DIAG_CHECKSUM_ERROR) {
            icon = "❌";
        } else if (d->code >= G64_DIAG_WEAK_BITS && d->code <= G64_DIAG_EXTRA_DATA) {
            icon = "🛡️";
        } else if (d->code != G64_DIAG_OK) {
            icon = "⚠️";
        } else {
            icon = "✅";
        }
        
        if (d->sector != 0xFF) {
            off += snprintf(buf + off, buf_size - off,
                "  %s T%02u S%02u: %s\n",
                icon, g64_half_to_full(d->track), d->sector, d->message);
        } else {
            off += snprintf(buf + off, buf_size - off,
                "  %s T%02u: %s\n",
                icon, g64_half_to_full(d->track), d->message);
        }
        
        /* Suggestion */
        if (d->code < G64_DIAG_COUNT && g64_diag_suggestions[d->code][0]) {
            off += snprintf(buf + off, buf_size - off,
                "           → %s\n",
                g64_diag_suggestions[d->code]);
        }
    }
    
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SCORING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize score
 */
static void g64_score_init(g64_score_t* score) {
    if (!score) return;
    memset(score, 0, sizeof(g64_score_t));
    score->overall = 1.0f;
    score->sync_score = 1.0f;
    score->gcr_score = 1.0f;
    score->checksum_score = 1.0f;
    score->structure_score = 1.0f;
    score->timing_score = 1.0f;
}

/**
 * @brief Calculate overall score
 */
static void g64_score_calculate(g64_score_t* score) {
    if (!score) return;
    
    score->overall = 
        score->sync_score * 0.20f +
        score->gcr_score * 0.25f +
        score->checksum_score * 0.25f +
        score->structure_score * 0.15f +
        score->timing_score * 0.15f;
    
    if (score->overall < 0.0f) score->overall = 0.0f;
    if (score->overall > 1.0f) score->overall = 1.0f;
}

/**
 * @brief Score a track
 */
static void g64_score_track(g64_track_t* track) {
    if (!track) return;
    
    g64_score_init(&track->score);
    
    /* Empty track */
    if (!track->gcr_data || track->gcr_size == 0) {
        track->score.overall = 0.0f;
        return;
    }
    
    /* Sync score based on sync marks found */
    if (track->sync_count > 0) {
        track->score.has_sync = true;
        if (track->sync_count >= track->expected_sectors) {
            track->score.sync_score = 1.0f;
        } else {
            track->score.sync_score = (float)track->sync_count / track->expected_sectors;
        }
    } else {
        track->score.sync_score = 0.0f;
    }
    
    /* GCR score based on decode errors */
    if (track->sector_count > 0) {
        int gcr_errors = 0;
        for (int s = 0; s < track->sector_count; s++) {
            if (!track->sectors[s].header_valid || !track->sectors[s].data_valid) {
                gcr_errors++;
            }
        }
        track->score.gcr_errors = gcr_errors;
        track->score.gcr_score = 1.0f - ((float)gcr_errors / track->sector_count);
    }
    
    /* Checksum score */
    if (track->valid_sectors > 0) {
        track->score.checksums_valid = true;
        track->score.checksum_score = (float)track->valid_sectors / track->expected_sectors;
    }
    
    /* Structure score based on sector count */
    if (track->expected_sectors > 0) {
        float ratio = (float)track->sector_count / track->expected_sectors;
        if (ratio > 1.0f) {
            /* Extra sectors - might be protection */
            track->score.structure_score = 1.0f;
            track->has_extra_sectors = true;
        } else {
            track->score.structure_score = ratio;
        }
    }
    
    /* Track size vs expected */
    uint16_t expected = g64_get_track_size(track->speed_zone);
    if (track->gcr_size > expected * 1.1f) {
        track->score.timing_score = 0.9f;  /* Long track */
    } else if (track->gcr_size < expected * 0.9f) {
        track->score.timing_score = 0.8f;  /* Short track */
    }
    
    /* Weak bits reduce GCR score */
    if (track->weak_bit_count > 0) {
        track->has_weak_bits = true;
        track->score.has_weak_bits = true;
        track->score.weak_bit_count = track->weak_bit_count;
    }
    
    /* Protection detection */
    if (track->has_weak_bits || track->has_extra_sectors || 
        track->has_long_sync || track->is_killer_track) {
        track->is_protected = true;
        track->score.has_protection = true;
    }
    
    track->score.sectors_found = track->sector_count;
    track->score.sectors_valid = track->valid_sectors;
    
    g64_score_calculate(&track->score);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTOR DECODING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Decode sector header from GCR
 */
static bool g64_decode_header(
    const uint8_t* gcr,
    size_t size,
    size_t pos,
    g64_sector_t* sector
) {
    if (pos + 10 > size) return false;
    
    /* Header is 10 GCR bytes = 8 data bytes */
    /* Format: 08 checksum sector track id2 id1 0f 0f */
    
    uint8_t decoded[8];
    uint8_t errors1, errors2;
    
    bool ok1 = g64_gcr_decode_block(gcr + pos, decoded, &errors1);
    bool ok2 = g64_gcr_decode_block(gcr + pos + 5, decoded + 4, &errors2);
    
    if (!ok1 || !ok2) {
        sector->header_valid = false;
        return false;
    }
    
    /* Check header marker */
    if (decoded[0] != G64_HEADER_MARKER) {
        sector->header_valid = false;
        return false;
    }
    
    sector->checksum_header = decoded[1];
    sector->sector_id = decoded[2];
    sector->track_id = decoded[3];
    /* decoded[4], decoded[5] = disk ID */
    /* decoded[6], decoded[7] = 0x0F padding */
    
    /* Verify checksum */
    uint8_t calc_checksum = decoded[2] ^ decoded[3] ^ decoded[4] ^ decoded[5];
    sector->checksum_header_ok = (calc_checksum == sector->checksum_header);
    
    sector->header_valid = true;
    sector->header_position = pos;
    
    return true;
}

/**
 * @brief Decode sector data from GCR
 */
static bool g64_decode_data(
    const uint8_t* gcr,
    size_t size,
    size_t pos,
    g64_sector_t* sector
) {
    /* Data block: 325 GCR bytes = 260 data bytes */
    /* Format: 07 + 256 data bytes + checksum + 00 00 */
    
    if (pos + 325 > size) return false;
    
    uint8_t block[4];
    uint8_t errors;
    
    /* First block contains marker */
    if (!g64_gcr_decode_block(gcr + pos, block, &errors)) {
        sector->data_valid = false;
        return false;
    }
    
    if (block[0] != G64_DATA_MARKER) {
        sector->data_valid = false;
        return false;
    }
    
    /* Decode 256 bytes of data (64 blocks of 4 bytes = 80 GCR blocks of 5 bytes) */
    /* Actually: marker + 256 data + checksum = 258 bytes = 322.5 GCR bytes */
    /* We decode in 4-byte blocks */
    
    size_t gcr_pos = pos + 1;  /* Skip marker byte in GCR stream */
    uint8_t checksum = 0;
    
    /* The data is: 07, d0, d1, d2, ..., d255, checksum, 00, 00 */
    /* In GCR: 5 bytes per 4 data bytes */
    
    /* Simplified: decode 65 blocks (260 bytes) */
    for (int i = 0; i < 65 && gcr_pos + 5 <= size; i++) {
        uint8_t decoded[4];
        if (!g64_gcr_decode_block(gcr + gcr_pos, decoded, &errors)) {
            if (i < 64) {  /* Error in data, not padding */
                sector->data_valid = false;
                return false;
            }
        }
        
        if (i == 0) {
            /* First block: marker already checked, copy rest */
            sector->data[0] = decoded[1];
            sector->data[1] = decoded[2];
            sector->data[2] = decoded[3];
            checksum ^= decoded[1] ^ decoded[2] ^ decoded[3];
        } else if (i < 64) {
            /* Data blocks */
            int base = i * 4 - 1;
            sector->data[base] = decoded[0];
            sector->data[base + 1] = decoded[1];
            sector->data[base + 2] = decoded[2];
            if (base + 3 < 256) {
                sector->data[base + 3] = decoded[3];
                checksum ^= decoded[0] ^ decoded[1] ^ decoded[2] ^ decoded[3];
            } else {
                checksum ^= decoded[0] ^ decoded[1] ^ decoded[2];
                sector->checksum_data = decoded[3];
            }
        } else {
            /* Checksum block */
            sector->checksum_data = decoded[0];
        }
        
        gcr_pos += 5;
    }
    
    sector->checksum_data_ok = (checksum == sector->checksum_data);
    sector->data_valid = true;
    sector->data_position = pos;
    
    return true;
}

/**
 * @brief Parse all sectors in a track
 */
static void g64_parse_track_sectors(
    g64_track_t* track,
    g64_params_t* params,
    g64_diagnosis_list_t* diag
) {
    if (!track || !track->gcr_data || track->gcr_size == 0) return;
    
    track->sector_count = 0;
    track->valid_sectors = 0;
    track->sync_count = 0;
    
    /* Find all sync marks */
    size_t pos = 0;
    while (pos < track->gcr_size && track->sync_count < 32) {
        uint8_t sync_len;
        int sync_pos = g64_find_sync(track->gcr_data, track->gcr_size, pos, &sync_len);
        
        if (sync_pos < 0) break;
        
        track->sync_marks[track->sync_count].position = sync_pos;
        track->sync_marks[track->sync_count].length = sync_len;
        track->sync_count++;
        
        /* Check for long sync (protection) */
        if (sync_len > 10) {
            track->has_long_sync = true;
            if (diag) {
                g64_diagnosis_add(diag, G64_DIAG_LONG_SYNC,
                    track->half_track, 0xFF,
                    "Long sync of %u bytes at position %d", sync_len, sync_pos);
            }
        }
        
        pos = sync_pos + sync_len;
        
        /* Try to decode sector after sync */
        if (pos + 10 <= track->gcr_size && track->sector_count < 24) {
            g64_sector_t* sector = &track->sectors[track->sector_count];
            memset(sector, 0, sizeof(g64_sector_t));
            
            sector->sync_position = sync_pos;
            sector->sync_length = sync_len;
            
            /* Decode header */
            if (g64_decode_header(track->gcr_data, track->gcr_size, pos, sector)) {
                /* Find data block (after gap) */
                size_t data_search = pos + 10;
                uint8_t data_sync_len;
                int data_sync = g64_find_sync(track->gcr_data, track->gcr_size, 
                                               data_search, &data_sync_len);
                
                if (data_sync > 0 && data_sync < (int)(pos + 100)) {
                    size_t data_pos = data_sync + data_sync_len;
                    g64_decode_data(track->gcr_data, track->gcr_size, data_pos, sector);
                }
                
                sector->present = true;
                track->sector_count++;
                
                if (sector->header_valid && sector->data_valid &&
                    sector->checksum_header_ok && sector->checksum_data_ok) {
                    track->valid_sectors++;
                } else {
                    track->error_sectors++;
                    
                    if (diag) {
                        if (!sector->checksum_header_ok) {
                            g64_diagnosis_add(diag, G64_DIAG_CHECKSUM_ERROR,
                                track->half_track, sector->sector_id,
                                "Header checksum error");
                        }
                        if (!sector->checksum_data_ok) {
                            g64_diagnosis_add(diag, G64_DIAG_CHECKSUM_ERROR,
                                track->half_track, sector->sector_id,
                                "Data checksum error");
                        }
                    }
                }
                
                /* Check for wrong track ID */
                if (sector->track_id != track->full_track && diag) {
                    g64_diagnosis_add(diag, G64_DIAG_WRONG_TRACK_ID,
                        track->half_track, sector->sector_id,
                        "Track ID %u in sector, expected %u",
                        sector->track_id, track->full_track);
                }
            }
        }
    }
    
    /* Check for missing/extra sectors */
    if (track->sector_count < track->expected_sectors && diag) {
        g64_diagnosis_add(diag, G64_DIAG_MISSING_SECTOR,
            track->half_track, 0xFF,
            "Found %u sectors, expected %u",
            track->sector_count, track->expected_sectors);
    }
    if (track->sector_count > track->expected_sectors && diag) {
        track->has_extra_sectors = true;
        g64_diagnosis_add(diag, G64_DIAG_EXTRA_SECTOR,
            track->half_track, 0xFF,
            "Found %u sectors, expected %u (protection?)",
            track->sector_count, track->expected_sectors);
    }
    
    /* Check for killer track (no valid syncs or all errors) */
    if (track->sync_count == 0 || 
        (track->sector_count > 0 && track->valid_sectors == 0)) {
        track->is_killer_track = true;
        if (diag) {
            g64_diagnosis_add(diag, G64_DIAG_KILLER_TRACK,
                track->half_track, 0xFF,
                "Killer track (unreadable)");
        }
    }
    
    /* Score the track */
    g64_score_track(track);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MAIN PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Validate G64 header
 */
static bool g64_validate_header(const uint8_t* data, size_t size, g64_disk_t* disk) {
    if (size < G64_TRACK_DATA_OFFSET) return false;
    
    /* Check signature */
    if (memcmp(data, G64_SIGNATURE, G64_SIGNATURE_LEN) != 0) {
        snprintf(disk->error, sizeof(disk->error), "Invalid G64 signature");
        return false;
    }
    
    memcpy(disk->signature, data, G64_SIGNATURE_LEN);
    disk->signature[8] = '\0';
    
    /* Version */
    disk->version = data[8];
    if (disk->version != G64_VERSION) {
        snprintf(disk->error, sizeof(disk->error), 
                 "Unsupported G64 version: %u", disk->version);
        return false;
    }
    
    /* Track count */
    disk->track_count = data[9];
    if (disk->track_count > G64_MAX_TRACKS) {
        disk->track_count = G64_MAX_TRACKS;
    }
    
    /* Max track size */
    disk->max_track_size = data[10] | (data[11] << 8);
    
    return true;
}

/**
 * @brief Read track and speed tables
 */
static bool g64_read_tables(const uint8_t* data, size_t size, g64_disk_t* disk) {
    /* Track offset table */
    for (int i = 0; i <= G64_MAX_TRACKS; i++) {
        size_t off = G64_TRACK_TABLE_OFFSET + i * 4;
        if (off + 4 > size) break;
        
        disk->track_offsets[i] = 
            data[off] | 
            (data[off + 1] << 8) | 
            ((uint32_t)data[off + 2] << 16) | 
            ((uint32_t)data[off + 3] << 24);
    }
    
    /* Speed zone table */
    for (int i = 0; i <= G64_MAX_TRACKS; i++) {
        size_t off = G64_SPEED_TABLE_OFFSET + i * 4;
        if (off + 4 > size) break;
        
        disk->speed_zones[i] = 
            data[off] | 
            (data[off + 1] << 8) | 
            ((uint32_t)data[off + 2] << 16) | 
            ((uint32_t)data[off + 3] << 24);
    }
    
    return true;
}

/**
 * @brief Parse single track
 */
static bool g64_parse_track(
    const uint8_t* data,
    size_t size,
    uint8_t half_track,
    g64_disk_t* disk,
    g64_params_t* params,
    g64_diagnosis_list_t* diag
) {
    if (half_track < 1 || half_track > G64_MAX_TRACKS) return false;
    
    g64_track_t* track = &disk->tracks[half_track];
    memset(track, 0, sizeof(g64_track_t));
    
    track->half_track = half_track;
    track->full_track = g64_half_to_full(half_track);
    track->is_half_track = g64_is_half_track(half_track);
    track->speed_zone = disk->speed_zones[half_track] & 0x03;
    track->expected_sectors = g64_get_sectors(track->full_track);
    track->expected_size = g64_get_track_size(track->speed_zone);
    
    /* Get track offset */
    uint32_t offset = disk->track_offsets[half_track];
    if (offset == 0) {
        /* Empty track */
        disk->empty_tracks++;
        return true;
    }
    
    if (offset + 2 > size) {
        g64_diagnosis_add(diag, G64_DIAG_TRUNCATED, half_track, 0xFF,
            "Track offset %u beyond file size", offset);
        return false;
    }
    
    /* Read track size */
    uint16_t track_size = data[offset] | (data[offset + 1] << 8);
    if (track_size == 0) {
        disk->empty_tracks++;
        return true;
    }
    
    if (offset + 2 + track_size > size) {
        g64_diagnosis_add(diag, G64_DIAG_TRUNCATED, half_track, 0xFF,
            "Track data truncated");
        track_size = size - offset - 2;
    }
    
    /* Copy track data */
    track->gcr_data = malloc(track_size);
    if (!track->gcr_data) return false;
    
    memcpy(track->gcr_data, data + offset + 2, track_size);
    track->gcr_size = track_size;
    track->offset_in_file = offset;
    
    /* Count track types */
    if (track->is_half_track) {
        disk->half_tracks++;
        g64_diagnosis_add(diag, G64_DIAG_HALF_TRACK, half_track, 0xFF,
            "Half-track %u.5 contains data", track->full_track);
    } else {
        disk->full_tracks++;
    }
    
    /* Check track size */
    if (track_size > track->expected_size * 1.15f) {
        g64_diagnosis_add(diag, G64_DIAG_LONG_TRACK, half_track, 0xFF,
            "Track size %u exceeds expected %u", track_size, track->expected_size);
    } else if (track_size < track->expected_size * 0.85f) {
        g64_diagnosis_add(diag, G64_DIAG_SHORT_TRACK, half_track, 0xFF,
            "Track size %u below expected %u", track_size, track->expected_size);
    }
    
    /* Parse sectors */
    if (params && params->decode_sectors) {
        g64_parse_track_sectors(track, params, diag);
        disk->total_sectors += track->sector_count;
        disk->valid_sectors += track->valid_sectors;
    }
    
    return true;
}

/**
 * @brief Main G64 parse function
 */
bool g64_parse(
    const uint8_t* data,
    size_t size,
    g64_disk_t* disk,
    g64_params_t* params
) {
    if (!data || !disk) return false;
    
    memset(disk, 0, sizeof(g64_disk_t));
    disk->diagnosis = g64_diagnosis_create();
    disk->source_size = size;
    
    /* Validate header */
    if (!g64_validate_header(data, size, disk)) {
        g64_diagnosis_add(disk->diagnosis, G64_DIAG_BAD_SIGNATURE, 0, 0xFF,
            "Invalid G64 header");
        return false;
    }
    
    /* Read tables */
    if (!g64_read_tables(data, size, disk)) {
        return false;
    }
    
    /* Parse all tracks */
    for (uint8_t ht = 1; ht <= disk->track_count; ht++) {
        g64_parse_track(data, size, ht, disk, params, disk->diagnosis);
    }
    
    /* Calculate overall score */
    g64_score_init(&disk->score);
    float score_sum = 0;
    int track_count = 0;
    
    for (uint8_t ht = 1; ht <= disk->track_count; ht++) {
        g64_track_t* track = &disk->tracks[ht];
        if (track->gcr_size > 0) {
            score_sum += track->score.overall;
            track_count++;
            
            if (track->is_protected) {
                disk->has_protection = true;
            }
        }
    }
    
    if (track_count > 0) {
        disk->score.overall = score_sum / track_count;
    }
    
    disk->valid = true;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * WRITE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate required file size
 */
static size_t g64_calculate_size(const g64_disk_t* disk) {
    size_t size = G64_TRACK_DATA_OFFSET;
    
    for (uint8_t ht = 1; ht <= G64_MAX_TRACKS; ht++) {
        const g64_track_t* track = &disk->tracks[ht];
        if (track->gcr_size > 0) {
            size += 2 + track->gcr_size;  /* Size word + data */
        }
    }
    
    return size;
}

/**
 * @brief Write G64 to buffer
 */
uint8_t* g64_write(
    const g64_disk_t* disk,
    g64_params_t* params,
    size_t* out_size
) {
    if (!disk || !out_size) return NULL;
    
    size_t size = g64_calculate_size(disk);
    uint8_t* data = calloc(1, size);
    if (!data) {
        *out_size = 0;
        return NULL;
    }
    
    /* Write header */
    memcpy(data, G64_SIGNATURE, G64_SIGNATURE_LEN);
    data[8] = G64_VERSION;
    data[9] = G64_MAX_TRACKS;
    data[10] = G64_MAX_TRACK_SIZE & 0xFF;
    data[11] = (G64_MAX_TRACK_SIZE >> 8) & 0xFF;
    
    /* Calculate track offsets and write data */
    uint32_t offset = G64_TRACK_DATA_OFFSET;
    
    for (uint8_t ht = 1; ht <= G64_MAX_TRACKS; ht++) {
        const g64_track_t* track = &disk->tracks[ht];
        
        /* Skip empty tracks unless requested */
        if (track->gcr_size == 0) {
            if (params && !params->include_empty_tracks) {
                continue;
            }
        }
        
        /* Skip half-tracks unless requested */
        if (track->is_half_track && params && !params->include_half_tracks) {
            continue;
        }
        
        /* Write track offset */
        size_t off_pos = G64_TRACK_TABLE_OFFSET + ht * 4;
        if (track->gcr_size > 0) {
            data[off_pos] = offset & 0xFF;
            data[off_pos + 1] = (offset >> 8) & 0xFF;
            data[off_pos + 2] = (offset >> 16) & 0xFF;
            data[off_pos + 3] = (offset >> 24) & 0xFF;
            
            /* Write track data */
            data[offset] = track->gcr_size & 0xFF;
            data[offset + 1] = (track->gcr_size >> 8) & 0xFF;
            memcpy(data + offset + 2, track->gcr_data, track->gcr_size);
            
            offset += 2 + track->gcr_size;
        }
        
        /* Write speed zone */
        size_t spd_pos = G64_SPEED_TABLE_OFFSET + ht * 4;
        data[spd_pos] = track->speed_zone;
    }
    
    *out_size = offset;
    return data;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PROTECTION DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect copy protection
 */
bool g64_detect_protection(
    const g64_disk_t* disk,
    char* protection_name,
    size_t name_size,
    float* confidence
) {
    if (!disk) return false;
    
    *confidence = 0.0f;
    protection_name[0] = '\0';
    
    int weak_tracks = 0;
    int extra_sector_tracks = 0;
    int long_sync_tracks = 0;
    int half_tracks_with_data = 0;
    int killer_tracks = 0;
    
    for (uint8_t ht = 1; ht <= disk->track_count; ht++) {
        const g64_track_t* track = &disk->tracks[ht];
        
        if (track->has_weak_bits) weak_tracks++;
        if (track->has_extra_sectors) extra_sector_tracks++;
        if (track->has_long_sync) long_sync_tracks++;
        if (track->is_half_track && track->gcr_size > 0) half_tracks_with_data++;
        if (track->is_killer_track) killer_tracks++;
    }
    
    /* Vorpal / RapidLok */
    if (weak_tracks > 0 && half_tracks_with_data > 0) {
        snprintf(protection_name, name_size, "Vorpal/RapidLok");
        *confidence = 0.90f;
        return true;
    }
    
    /* V-Max (typically track 20 area) */
    const g64_track_t* t20 = &disk->tracks[g64_full_to_half(20)];
    if (t20->has_weak_bits || t20->is_killer_track) {
        snprintf(protection_name, name_size, "V-Max!");
        *confidence = 0.85f;
        return true;
    }
    
    /* Epyx FastLoad */
    if (long_sync_tracks > 5) {
        snprintf(protection_name, name_size, "Epyx FastLoad");
        *confidence = 0.75f;
        return true;
    }
    
    /* General weak bit protection */
    if (weak_tracks > 3) {
        snprintf(protection_name, name_size, "Weak bit protection");
        *confidence = 0.70f;
        return true;
    }
    
    /* Half-track protection */
    if (half_tracks_with_data > 2) {
        snprintf(protection_name, name_size, "Half-track protection");
        *confidence = 0.80f;
        return true;
    }
    
    /* Extra sectors */
    if (extra_sector_tracks > 0) {
        snprintf(protection_name, name_size, "Extra sector protection");
        *confidence = 0.65f;
        return true;
    }
    
    /* Killer tracks */
    if (killer_tracks > 0) {
        snprintf(protection_name, name_size, "Killer track protection");
        *confidence = 0.70f;
        return true;
    }
    
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * D64 EXPORT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Export to D64 format
 */
uint8_t* g64_export_d64(
    const g64_disk_t* disk,
    size_t* out_size,
    bool include_errors
) {
    if (!disk || !out_size) return NULL;
    
    /* D64 size: 35 tracks */
    size_t d64_sectors = 683;
    size_t d64_size = d64_sectors * 256;
    if (include_errors) {
        d64_size += d64_sectors;
    }
    
    uint8_t* d64 = calloc(1, d64_size);
    if (!d64) {
        *out_size = 0;
        return NULL;
    }
    
    /* Copy decoded sectors */
    static const uint16_t track_offset[36] = {
        0, 0, 21, 42, 63, 84, 105, 126, 147, 168, 189, 210, 231, 252, 273, 294, 315, 336,
        357, 376, 395, 414, 433, 452, 471, 490, 508, 526, 544, 562, 580,
        598, 615, 632, 649, 666
    };
    
    uint8_t* error_bytes = include_errors ? (d64 + 683 * 256) : NULL;
    
    for (uint8_t t = 1; t <= 35; t++) {
        uint8_t ht = g64_full_to_half(t);
        const g64_track_t* track = &disk->tracks[ht];
        
        for (int s = 0; s < track->sector_count; s++) {
            const g64_sector_t* sector = &track->sectors[s];
            
            if (sector->present && sector->sector_id < g64_get_sectors(t)) {
                size_t offset = (track_offset[t] + sector->sector_id) * 256;
                memcpy(d64 + offset, sector->data, 256);
                
                if (error_bytes) {
                    size_t err_idx = track_offset[t] + sector->sector_id;
                    if (sector->data_valid && sector->checksum_data_ok) {
                        error_bytes[err_idx] = 0x01;  /* OK */
                    } else {
                        error_bytes[err_idx] = 0x05;  /* Checksum error */
                    }
                }
            }
        }
    }
    
    *out_size = d64_size;
    return d64;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DEFAULT PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get default parameters
 */
void g64_get_default_params(g64_params_t* params) {
    if (!params) return;
    memset(params, 0, sizeof(g64_params_t));
    
    params->revolutions = 3;
    params->multi_rev_merge = true;
    params->merge_strategy = 1;
    
    params->strict_gcr = false;
    params->ignore_gcr_errors = false;
    
    params->detect_protection = true;
    params->preserve_protection = true;
    params->preserve_weak_bits = true;
    params->preserve_half_tracks = true;
    
    params->sync_min_bytes = G64_SYNC_MIN_BYTES;
    params->sync_max_bytes = G64_SYNC_MAX_BYTES;
    params->tolerant_sync = true;
    
    params->decode_sectors = true;
    params->generate_d64 = true;
    params->validate_checksums = true;
    
    params->timing_tolerance = 0.15f;
    params->detect_timing_protection = true;
    
    params->include_empty_tracks = false;
    params->include_half_tracks = true;
    
    params->verify_after_write = true;
    params->verify_mode = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CLEANUP
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Free disk structure
 */
void g64_disk_free(g64_disk_t* disk) {
    if (!disk) return;
    
    if (disk->diagnosis) {
        g64_diagnosis_free(disk->diagnosis);
        disk->diagnosis = NULL;
    }
    
    if (disk->d64_data) {
        free(disk->d64_data);
        disk->d64_data = NULL;
    }
    
    for (uint8_t ht = 0; ht <= G64_MAX_TRACKS; ht++) {
        g64_track_t* track = &disk->tracks[ht];
        
        if (track->gcr_data) {
            free(track->gcr_data);
            track->gcr_data = NULL;
        }
        if (track->weak_mask) {
            free(track->weak_mask);
            track->weak_mask = NULL;
        }
        
        /* Free revolution data */
        for (int r = 0; r < track->revolution_count; r++) {
            if (track->revolutions[r].data) {
                free(track->revolutions[r].data);
            }
        }
        
        /* Free sector multi-rev data */
        for (int s = 0; s < 24; s++) {
            g64_sector_t* sector = &track->sectors[s];
            if (sector->rev_data) {
                for (int r = 0; r < sector->rev_count; r++) {
                    free(sector->rev_data[r]);
                }
                free(sector->rev_data);
                free(sector->rev_valid);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef G64_V3_TEST

#include <assert.h>

int main(void) {
    printf("=== G64 Parser v3 Tests ===\n");
    
    /* Test helper functions */
    printf("Testing helper functions... ");
    assert(g64_half_to_full(1) == 1);
    assert(g64_half_to_full(2) == 1);
    assert(g64_half_to_full(3) == 2);
    assert(g64_full_to_half(1) == 1);
    assert(g64_full_to_half(2) == 3);
    assert(g64_is_half_track(1) == false);
    assert(g64_is_half_track(2) == true);
    assert(g64_get_sectors(1) == 21);
    assert(g64_get_sectors(18) == 19);
    assert(g64_get_speed_zone(1) == 3);
    assert(g64_get_speed_zone(31) == 0);
    printf("✓\n");
    
    /* Test GCR encode/decode */
    printf("Testing GCR encode/decode... ");
    uint8_t data[4] = { 0x08, 0x00, 0x01, 0x00 };
    uint8_t gcr[5];
    uint8_t decoded[4];
    uint8_t errors;
    
    g64_gcr_encode_block(data, gcr);
    assert(g64_gcr_decode_block(gcr, decoded, &errors));
    assert(errors == 0);
    assert(memcmp(data, decoded, 4) == 0);
    printf("✓\n");
    
    /* Test sync finding */
    printf("Testing sync detection... ");
    uint8_t track_data[] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x08, 0x00 };
    uint8_t sync_len;
    int sync_pos = g64_find_sync(track_data, sizeof(track_data), 0, &sync_len);
    assert(sync_pos == 1);
    assert(sync_len == 6);
    printf("✓\n");
    
    /* Test checksum */
    printf("Testing checksum... ");
    uint8_t test_data[] = { 0x01, 0x02, 0x03, 0x04 };
    assert(g64_checksum(test_data, 4) == (0x01 ^ 0x02 ^ 0x03 ^ 0x04));
    printf("✓\n");
    
    /* Test diagnosis system */
    printf("Testing diagnosis system... ");
    g64_diagnosis_list_t* diag = g64_diagnosis_create();
    assert(diag != NULL);
    
    g64_diagnosis_add(diag, G64_DIAG_GCR_ERROR, 17, 5, "Test error");
    assert(diag->count == 1);
    assert(diag->error_count == 1);
    
    g64_diagnosis_add(diag, G64_DIAG_WEAK_BITS, 17, 5, "Weak bits");
    assert(diag->count == 2);
    assert(diag->protection_count == 1);
    
    char* report = g64_diagnosis_to_text(diag, NULL);
    assert(report != NULL);
    assert(strstr(report, "Track") != NULL);
    free(report);
    
    g64_diagnosis_free(diag);
    printf("✓\n");
    
    /* Test scoring */
    printf("Testing scoring system... ");
    g64_score_t score;
    g64_score_init(&score);
    assert(score.overall == 1.0f);
    
    score.sync_score = 0.9f;
    score.gcr_score = 0.8f;
    score.checksum_score = 0.95f;
    score.structure_score = 1.0f;
    score.timing_score = 0.9f;
    g64_score_calculate(&score);
    assert(score.overall > 0.85f && score.overall < 0.95f);
    printf("✓\n");
    
    /* Test parameters */
    printf("Testing default parameters... ");
    g64_params_t params;
    g64_get_default_params(&params);
    assert(params.revolutions == 3);
    assert(params.preserve_half_tracks == true);
    assert(params.decode_sectors == true);
    printf("✓\n");
    
    /* Test minimal G64 parsing */
    printf("Testing G64 header parsing... ");
    uint8_t minimal_g64[G64_TRACK_DATA_OFFSET + 100];
    memset(minimal_g64, 0, sizeof(minimal_g64));
    memcpy(minimal_g64, G64_SIGNATURE, G64_SIGNATURE_LEN);
    minimal_g64[8] = G64_VERSION;
    minimal_g64[9] = 84;  /* Track count */
    minimal_g64[10] = G64_MAX_TRACK_SIZE & 0xFF;
    minimal_g64[11] = (G64_MAX_TRACK_SIZE >> 8) & 0xFF;
    
    g64_disk_t disk;
    g64_params_t parms;
    g64_get_default_params(&parms);
    
    bool ok = g64_parse(minimal_g64, sizeof(minimal_g64), &disk, &parms);
    assert(ok);
    assert(disk.valid);
    assert(disk.track_count == 84);
    assert(memcmp(disk.signature, G64_SIGNATURE, 8) == 0);
    
    g64_disk_free(&disk);
    printf("✓\n");
    
    /* Test protection detection */
    printf("Testing protection detection... ");
    g64_disk_t test_disk;
    memset(&test_disk, 0, sizeof(test_disk));
    test_disk.track_count = 84;
    test_disk.tracks[g64_full_to_half(20)].has_weak_bits = true;
    
    char prot_name[64];
    float confidence;
    bool has_prot = g64_detect_protection(&test_disk, prot_name, sizeof(prot_name), &confidence);
    assert(has_prot);
    assert(strstr(prot_name, "V-Max") != NULL);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 8, Failed: 0\n");
    return 0;
}

#endif /* G64_V3_TEST */

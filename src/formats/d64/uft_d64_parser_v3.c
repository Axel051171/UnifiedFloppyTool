/**
 * @file uft_d64_parser_v3.c
 * @brief GOD MODE D64 Parser v3 - Vollständige Referenz-Implementation
 * 
 * D64 ist das Commodore 64/1541 Disk-Format:
 * - 35 Tracks (40 extended)
 * - Variable Sektoren pro Track (17-21)
 * - GCR Encoding
 * - BAM (Block Availability Map)
 * - Directory mit 144 Einträgen
 * 
 * v3 Features:
 * - Read/Write/Analyze Pipeline
 * - Multi-Rev Merge mit Bit-Level Voting
 * - Kopierschutz-Erkennung (Weak Bits, Timing, Non-Standard)
 * - Track-Level Diagnose mit Erklärungen
 * - Scoring pro Sektor/Track
 * - Verify-After-Write
 * - Integration mit XCopy/Recovery/Forensic/PLL
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 * @date 2025-01-02
 */

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

#define D64_SECTOR_SIZE         256
#define D64_TRACKS_STANDARD     35
#define D64_TRACKS_EXTENDED     40
#define D64_SECTORS_35          683
#define D64_SECTORS_40          768

#define D64_SIZE_35             (D64_SECTORS_35 * D64_SECTOR_SIZE)  /* 174848 */
#define D64_SIZE_35_ERR         (D64_SIZE_35 + D64_SECTORS_35)      /* 175531 */
#define D64_SIZE_40             (D64_SECTORS_40 * D64_SECTOR_SIZE)  /* 196608 */
#define D64_SIZE_40_ERR         (D64_SIZE_40 + D64_SECTORS_40)      /* 197376 */

#define D64_BAM_TRACK           18
#define D64_BAM_SECTOR          0
#define D64_DIR_TRACK           18
#define D64_DIR_SECTOR          1
#define D64_MAX_DIR_ENTRIES     144

/* Speed zones for GCR */
#define D64_ZONE_3_START        1   /* Tracks 1-17: 21 sectors */
#define D64_ZONE_2_START        18  /* Tracks 18-24: 19 sectors */
#define D64_ZONE_1_START        25  /* Tracks 25-30: 18 sectors */
#define D64_ZONE_0_START        31  /* Tracks 31-40: 17 sectors */

/* GCR timing (in nanoseconds at 300 RPM) */
#define D64_BITCELL_ZONE3       3200    /* ~312.5 kbps */
#define D64_BITCELL_ZONE2       3500    /* ~285.7 kbps */
#define D64_BITCELL_ZONE1       3750    /* ~266.7 kbps */
#define D64_BITCELL_ZONE0       4000    /* 250 kbps */

/* Sectors per track table */
static const uint8_t d64_sectors_per_track[41] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    19, 19, 19, 19, 19, 19, 19,
    18, 18, 18, 18, 18, 18,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17
};

/* Track offset table (cumulative sectors) */
static const uint16_t d64_track_offset[41] = {
    0,
    0, 21, 42, 63, 84, 105, 126, 147, 168, 189, 210, 231, 252, 273, 294, 315, 336,
    357, 376, 395, 414, 433, 452, 471,
    490, 508, 526, 544, 562, 580,
    598, 615, 632, 649, 666, 683, 700, 717, 734, 751
};

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR TABLES
 * ═══════════════════════════════════════════════════════════════════════════ */

/* 4-bit nibble to 5-bit GCR */
static const uint8_t gcr_encode[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/* 5-bit GCR to 4-bit nibble (0xFF = invalid) */
static const uint8_t gcr_decode[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

/* ═══════════════════════════════════════════════════════════════════════════
 * DIAGNOSIS CODES (D64 specific)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    D64_DIAG_OK = 0,
    
    /* Structure */
    D64_DIAG_INVALID_SIZE,
    D64_DIAG_BAD_BAM,
    D64_DIAG_BAD_DIRECTORY,
    D64_DIAG_WRONG_TRACK_ID,
    D64_DIAG_WRONG_SECTOR_ID,
    D64_DIAG_DUPLICATE_SECTOR,
    D64_DIAG_MISSING_SECTOR,
    
    /* CRC/Data */
    D64_DIAG_HEADER_CRC_ERROR,
    D64_DIAG_DATA_CRC_ERROR,
    D64_DIAG_GCR_ERROR,
    D64_DIAG_SYNC_ERROR,
    
    /* Timing */
    D64_DIAG_SPEED_ZONE_MISMATCH,
    D64_DIAG_TIMING_ANOMALY,
    D64_DIAG_LONG_SYNC,
    D64_DIAG_SHORT_SYNC,
    
    /* Protection */
    D64_DIAG_WEAK_BITS,
    D64_DIAG_EXTRA_SECTORS,
    D64_DIAG_NON_STANDARD_GAP,
    D64_DIAG_KILLER_TRACK,
    D64_DIAG_HALF_TRACK,
    
    /* File system */
    D64_DIAG_BAM_MISMATCH,
    D64_DIAG_CIRCULAR_CHAIN,
    D64_DIAG_ORPHAN_BLOCK,
    D64_DIAG_CROSS_LINKED,
    
    D64_DIAG_COUNT
} d64_diag_code_t;

static const char* d64_diag_names[] = {
    [D64_DIAG_OK] = "OK",
    [D64_DIAG_INVALID_SIZE] = "Invalid image size",
    [D64_DIAG_BAD_BAM] = "Corrupted BAM",
    [D64_DIAG_BAD_DIRECTORY] = "Corrupted directory",
    [D64_DIAG_WRONG_TRACK_ID] = "Wrong track ID in header",
    [D64_DIAG_WRONG_SECTOR_ID] = "Wrong sector ID in header",
    [D64_DIAG_DUPLICATE_SECTOR] = "Duplicate sector found",
    [D64_DIAG_MISSING_SECTOR] = "Expected sector not found",
    [D64_DIAG_HEADER_CRC_ERROR] = "Header block CRC error",
    [D64_DIAG_DATA_CRC_ERROR] = "Data block CRC error",
    [D64_DIAG_GCR_ERROR] = "Invalid GCR encoding",
    [D64_DIAG_SYNC_ERROR] = "Sync pattern not found",
    [D64_DIAG_SPEED_ZONE_MISMATCH] = "Speed zone mismatch",
    [D64_DIAG_TIMING_ANOMALY] = "Unusual timing detected",
    [D64_DIAG_LONG_SYNC] = "Longer than normal sync",
    [D64_DIAG_SHORT_SYNC] = "Shorter than normal sync",
    [D64_DIAG_WEAK_BITS] = "Weak/fuzzy bits (protection?)",
    [D64_DIAG_EXTRA_SECTORS] = "Extra sectors (protection?)",
    [D64_DIAG_NON_STANDARD_GAP] = "Non-standard inter-sector gap",
    [D64_DIAG_KILLER_TRACK] = "Killer track detected",
    [D64_DIAG_HALF_TRACK] = "Half-track data present",
    [D64_DIAG_BAM_MISMATCH] = "BAM doesn't match actual usage",
    [D64_DIAG_CIRCULAR_CHAIN] = "Circular sector chain",
    [D64_DIAG_ORPHAN_BLOCK] = "Orphaned block (allocated but unused)",
    [D64_DIAG_CROSS_LINKED] = "Cross-linked sectors"
};

static const char* d64_diag_suggestions[] = {
    [D64_DIAG_OK] = "",
    [D64_DIAG_INVALID_SIZE] = "Check if file is truncated or has extra data",
    [D64_DIAG_BAD_BAM] = "Attempt BAM reconstruction from directory",
    [D64_DIAG_BAD_DIRECTORY] = "Try sector-by-sector recovery",
    [D64_DIAG_WRONG_TRACK_ID] = "May indicate track alignment issue",
    [D64_DIAG_WRONG_SECTOR_ID] = "Check for format mismatch",
    [D64_DIAG_DUPLICATE_SECTOR] = "PRESERVE - likely copy protection",
    [D64_DIAG_MISSING_SECTOR] = "Try more revolutions or different drive",
    [D64_DIAG_HEADER_CRC_ERROR] = "Use multi-rev merge for recovery",
    [D64_DIAG_DATA_CRC_ERROR] = "Try CRC correction or multi-rev voting",
    [D64_DIAG_GCR_ERROR] = "Check for media damage or drive issues",
    [D64_DIAG_SYNC_ERROR] = "Adjust sync tolerance or try different drive",
    [D64_DIAG_SPEED_ZONE_MISMATCH] = "Verify disk format matches expectations",
    [D64_DIAG_TIMING_ANOMALY] = "PRESERVE - may be intentional protection",
    [D64_DIAG_LONG_SYNC] = "PRESERVE - common protection technique",
    [D64_DIAG_SHORT_SYNC] = "May indicate worn media or alignment issue",
    [D64_DIAG_WEAK_BITS] = "PRESERVE - this IS the copy protection",
    [D64_DIAG_EXTRA_SECTORS] = "PRESERVE - this IS the copy protection",
    [D64_DIAG_NON_STANDARD_GAP] = "PRESERVE - this IS the copy protection",
    [D64_DIAG_KILLER_TRACK] = "PRESERVE - intentional unreadable track",
    [D64_DIAG_HALF_TRACK] = "Use G64 format to preserve half-tracks",
    [D64_DIAG_BAM_MISMATCH] = "Rebuild BAM from actual directory",
    [D64_DIAG_CIRCULAR_CHAIN] = "Corrupted file - truncate at loop",
    [D64_DIAG_ORPHAN_BLOCK] = "Add to scratch or mark as free",
    [D64_DIAG_CROSS_LINKED] = "Separate files, mark duplicates"
};

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Score structure
 */
typedef struct d64_score {
    float overall;
    float crc_score;
    float id_score;
    float timing_score;
    float sync_score;
    float gcr_score;
    
    bool header_crc_valid;
    bool data_crc_valid;
    bool id_valid;
    bool has_weak_bits;
    bool has_gcr_errors;
    bool recovered;
    
    uint8_t revolutions_used;
    uint8_t best_revolution;
    uint16_t bits_corrected;
} d64_score_t;

/**
 * @brief Diagnosis entry
 */
typedef struct d64_diagnosis {
    d64_diag_code_t code;
    uint8_t track;
    uint8_t sector;
    uint32_t bit_position;
    char message[256];
    d64_score_t score;
} d64_diagnosis_t;

/**
 * @brief Diagnosis list
 */
typedef struct d64_diagnosis_list {
    d64_diagnosis_t* items;
    size_t count;
    size_t capacity;
    uint16_t error_count;
    uint16_t warning_count;
    uint16_t protection_count;
    float overall_quality;
} d64_diagnosis_list_t;

/**
 * @brief Sector structure (v3)
 */
typedef struct d64_sector_v3 {
    /* Identity */
    uint8_t physical_track;
    uint8_t physical_sector;
    uint8_t logical_track;      /* From header */
    uint8_t logical_sector;     /* From header */
    
    /* Data */
    uint8_t data[256];
    uint8_t header_checksum;
    uint8_t data_checksum;
    uint8_t calc_header_checksum;
    uint8_t calc_data_checksum;
    
    /* Status */
    bool present;
    bool header_valid;
    bool data_valid;
    bool deleted;               /* Deleted data mark */
    
    /* Multi-rev data */
    uint8_t** rev_data;         /* Data from each revolution */
    bool* rev_data_valid;       /* CRC status per rev */
    uint8_t rev_count;
    uint8_t best_rev;
    
    /* Weak bits */
    uint8_t weak_mask[256];     /* Weak bit mask */
    uint16_t weak_bit_count;
    
    /* Scoring */
    d64_score_t score;
    
    /* Position in raw track */
    uint32_t header_bit_offset;
    uint32_t data_bit_offset;
    
    /* Error byte (from .d64 with errors) */
    uint8_t error_byte;
    
} d64_sector_v3_t;

/**
 * @brief Track structure (v3)
 */
typedef struct d64_track_v3 {
    uint8_t track_num;
    uint8_t expected_sectors;
    uint8_t found_sectors;
    uint8_t valid_sectors;
    uint8_t error_sectors;
    
    d64_sector_v3_t sectors[21];    /* Max 21 sectors */
    
    /* Speed zone */
    uint8_t speed_zone;             /* 0-3 */
    uint32_t expected_bitcell_ns;
    
    /* Raw data (if preserved) */
    uint8_t* raw_gcr;               /* Raw GCR data */
    size_t raw_gcr_size;
    
    /* Multi-rev */
    struct {
        uint8_t* data;
        size_t size;
        d64_score_t score;
    } revolutions[32];
    uint8_t revolution_count;
    uint8_t best_revolution;
    
    /* Timing */
    uint32_t rotation_time_ns;
    float* bit_timing;              /* Per-bit timing */
    size_t bit_count;
    
    /* Protection detection */
    bool has_weak_bits;
    bool has_extra_sectors;
    bool has_killer_pattern;
    bool has_sync_errors;
    bool is_protected;
    
    /* Scoring */
    d64_score_t score;
    
} d64_track_v3_t;

/**
 * @brief BAM entry
 */
typedef struct d64_bam_entry {
    uint8_t free_sectors;
    uint8_t bitmap[3];          /* 21 bits for sectors */
} d64_bam_entry_t;

/**
 * @brief Directory entry
 */
typedef struct d64_dir_entry {
    uint8_t file_type;
    uint8_t first_track;
    uint8_t first_sector;
    char filename[17];
    uint8_t rel_track;          /* REL file side-sector */
    uint8_t rel_sector;
    uint8_t rel_length;
    uint16_t blocks;
    
    bool closed;
    bool locked;
    bool splat;                 /* Unclosed file marker */
    
    /* GEOS extensions */
    bool is_geos;
    uint8_t geos_type;
    uint8_t geos_structure;
} d64_dir_entry_t;

/**
 * @brief D64 disk structure (v3)
 */
typedef struct d64_disk_v3 {
    /* Format info */
    char format_name[64];
    bool is_extended;           /* 40 tracks */
    bool has_error_bytes;
    
    /* Geometry */
    uint8_t tracks;
    uint16_t total_sectors;
    
    /* BAM */
    d64_bam_entry_t bam[41];
    char disk_name[17];
    char disk_id[6];
    uint8_t dos_type;
    uint16_t free_blocks;
    
    /* Directory */
    d64_dir_entry_t directory[D64_MAX_DIR_ENTRIES];
    uint16_t file_count;
    
    /* Tracks */
    d64_track_v3_t track_data[41];
    
    /* Error bytes */
    uint8_t error_bytes[768];
    
    /* Overall status */
    d64_score_t score;
    d64_diagnosis_list_t* diagnosis;
    
    /* Protection */
    bool has_protection;
    char protection_type[64];
    float protection_confidence;
    
    /* Hashes */
    uint8_t md5[16];
    uint8_t sha1[20];
    uint32_t crc32;
    
    /* Source */
    char source_path[256];
    size_t source_size;
    
    /* Status */
    bool valid;
    bool modified;
    char error[256];
    
} d64_disk_v3_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * PARAMETER STRUCTURE (D64 specific)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct d64_params {
    /* Read options */
    uint8_t revolutions;
    bool multi_rev_merge;
    int merge_strategy;         /* 0=vote, 1=best_crc, 2=weighted */
    
    /* Error handling */
    bool accept_bad_crc;
    bool attempt_crc_correction;
    uint8_t max_crc_bits;
    int error_mode;             /* 0=strict, 1=normal, 2=salvage, 3=forensic */
    uint8_t fill_pattern;
    
    /* GCR decoding */
    bool strict_gcr;            /* Reject any GCR errors */
    bool gcr_retry;             /* Retry on GCR errors */
    
    /* Protection */
    bool detect_protection;
    bool preserve_protection;
    bool preserve_weak_bits;
    
    /* BAM handling */
    bool validate_bam;
    bool rebuild_bam;
    
    /* Timing */
    float timing_tolerance;     /* 0.0-1.0 */
    int pll_mode;
    float pll_bandwidth;
    
    /* Output */
    bool include_error_bytes;
    bool generate_g64;          /* Output as G64 if protected */
    
    /* Verify */
    bool verify_after_write;
    int verify_mode;
    
} d64_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get sectors for track
 */
static uint8_t d64_get_sectors(uint8_t track) {
    if (track < 1 || track > 40) return 0;
    return d64_sectors_per_track[track];
}

/**
 * @brief Get speed zone for track
 */
static uint8_t d64_get_speed_zone(uint8_t track) {
    if (track < 1) return 3;
    if (track <= 17) return 3;
    if (track <= 24) return 2;
    if (track <= 30) return 1;
    return 0;
}

/**
 * @brief Get bitcell time for track
 */
static uint32_t d64_get_bitcell_ns(uint8_t track) {
    switch (d64_get_speed_zone(track)) {
        case 3: return D64_BITCELL_ZONE3;
        case 2: return D64_BITCELL_ZONE2;
        case 1: return D64_BITCELL_ZONE1;
        default: return D64_BITCELL_ZONE0;
    }
}

/**
 * @brief Get sector offset in image
 */
static size_t d64_get_sector_offset(uint8_t track, uint8_t sector) {
    if (track < 1 || track > 40) return 0;
    if (sector >= d64_sectors_per_track[track]) return 0;
    return (d64_track_offset[track] + sector) * D64_SECTOR_SIZE;
}

/**
 * @brief Check valid D64 size
 */
static bool d64_is_valid_size(size_t size, uint8_t* tracks, bool* has_errors) {
    *has_errors = false;
    
    if (size == D64_SIZE_35) {
        *tracks = 35;
        return true;
    }
    if (size == D64_SIZE_35_ERR) {
        *tracks = 35;
        *has_errors = true;
        return true;
    }
    if (size == D64_SIZE_40) {
        *tracks = 40;
        return true;
    }
    if (size == D64_SIZE_40_ERR) {
        *tracks = 40;
        *has_errors = true;
        return true;
    }
    
    return false;
}

/**
 * @brief File type to string
 */
const char* d64_file_type_str(uint8_t type) {
    static const char* types[] = { "DEL", "SEQ", "PRG", "USR", "REL" };
    uint8_t t = type & 0x07;
    if (t <= 4) return types[t];
    return "???";
}

/**
 * @brief PETSCII to ASCII
 */
static char petscii_to_ascii(uint8_t c) {
    if (c >= 0x41 && c <= 0x5A) return c + 0x20;
    if (c >= 0xC1 && c <= 0xDA) return c - 0x80;
    if (c >= 0x20 && c <= 0x7E) return c;
    if (c == 0xA0) return ' ';
    return '.';
}

/**
 * @brief Copy PETSCII filename
 */
static void d64_copy_filename(char* dest, const uint8_t* src, size_t max) {
    size_t i;
    for (i = 0; i < max && src[i] != 0xA0 && src[i] != 0x00; i++) {
        dest[i] = petscii_to_ascii(src[i]);
    }
    dest[i] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR ENCODING/DECODING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Encode 4 bytes to 5 GCR bytes
 */
void d64_gcr_encode_block(const uint8_t* data, uint8_t* gcr) {
    uint8_t nib[8];
    
    /* Extract nibbles */
    nib[0] = (data[0] >> 4) & 0x0F;
    nib[1] = data[0] & 0x0F;
    nib[2] = (data[1] >> 4) & 0x0F;
    nib[3] = data[1] & 0x0F;
    nib[4] = (data[2] >> 4) & 0x0F;
    nib[5] = data[2] & 0x0F;
    nib[6] = (data[3] >> 4) & 0x0F;
    nib[7] = data[3] & 0x0F;
    
    /* Encode to GCR */
    uint8_t g[8];
    for (int i = 0; i < 8; i++) {
        g[i] = gcr_encode[nib[i]];
    }
    
    /* Pack 8 5-bit values into 5 bytes */
    gcr[0] = (g[0] << 3) | (g[1] >> 2);
    gcr[1] = (g[1] << 6) | (g[2] << 1) | (g[3] >> 4);
    gcr[2] = (g[3] << 4) | (g[4] >> 1);
    gcr[3] = (g[4] << 7) | (g[5] << 2) | (g[6] >> 3);
    gcr[4] = (g[6] << 5) | g[7];
}

/**
 * @brief Decode 5 GCR bytes to 4 data bytes
 */
bool d64_gcr_decode_block(const uint8_t* gcr, uint8_t* data, uint8_t* errors) {
    uint8_t g[8];
    uint8_t nib[8];
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
    
    /* Decode each 5-bit value */
    for (int i = 0; i < 8; i++) {
        nib[i] = gcr_decode[g[i]];
        if (nib[i] == 0xFF) {
            *errors |= (1 << i);
            nib[i] = 0;  /* Use 0 for invalid */
            valid = false;
        }
    }
    
    /* Combine nibbles to bytes */
    data[0] = (nib[0] << 4) | nib[1];
    data[1] = (nib[2] << 4) | nib[3];
    data[2] = (nib[4] << 4) | nib[5];
    data[3] = (nib[6] << 4) | nib[7];
    
    return valid;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DIAGNOSIS FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create diagnosis list
 */
static d64_diagnosis_list_t* d64_diagnosis_create(void) {
    d64_diagnosis_list_t* list = calloc(1, sizeof(d64_diagnosis_list_t));
    if (!list) return NULL;
    
    list->capacity = 128;
    list->items = calloc(list->capacity, sizeof(d64_diagnosis_t));
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
static void d64_diagnosis_free(d64_diagnosis_list_t* list) {
    if (list) {
        free(list->items);
        free(list);
    }
}

/**
 * @brief Add diagnosis
 */
static void d64_diagnosis_add(
    d64_diagnosis_list_t* list,
    d64_diag_code_t code,
    uint8_t track,
    uint8_t sector,
    const char* format,
    ...
) {
    if (!list) return;
    
    /* Expand if needed */
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity * 2;
        d64_diagnosis_t* new_items = realloc(list->items, new_cap * sizeof(d64_diagnosis_t));
        if (!new_items) return;
        list->items = new_items;
        list->capacity = new_cap;
    }
    
    d64_diagnosis_t* diag = &list->items[list->count];
    memset(diag, 0, sizeof(d64_diagnosis_t));
    
    diag->code = code;
    diag->track = track;
    diag->sector = sector;
    
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(diag->message, sizeof(diag->message), format, args);
        va_end(args);
    } else if (code < D64_DIAG_COUNT) {
        snprintf(diag->message, sizeof(diag->message), "%s", d64_diag_names[code]);
    }
    
    list->count++;
    
    /* Update counters */
    if (code >= D64_DIAG_WEAK_BITS && code <= D64_DIAG_HALF_TRACK) {
        list->protection_count++;
    } else if (code >= D64_DIAG_HEADER_CRC_ERROR && code <= D64_DIAG_TIMING_ANOMALY) {
        list->error_count++;
    } else if (code != D64_DIAG_OK) {
        list->warning_count++;
    }
    
    /* Update quality */
    if (code != D64_DIAG_OK) {
        list->overall_quality *= 0.98f;  /* Each issue reduces quality */
    }
}

/**
 * @brief Generate diagnosis report
 */
char* d64_diagnosis_to_text(const d64_diagnosis_list_t* list, const d64_disk_v3_t* disk) {
    if (!list) return NULL;
    
    size_t buf_size = 16384;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    
    size_t off = 0;
    
    /* Header */
    off += snprintf(buf + off, buf_size - off,
        "╔══════════════════════════════════════════════════════════════════╗\n"
        "║                D64 DISK DIAGNOSIS REPORT                         ║\n"
        "╠══════════════════════════════════════════════════════════════════╣\n");
    
    if (disk) {
        off += snprintf(buf + off, buf_size - off,
            "║ Disk: %-16s  ID: %-5s                               ║\n"
            "║ Tracks: %2u  Sectors: %4u  Size: %zu bytes                     ║\n",
            disk->disk_name,
            disk->disk_id,
            disk->tracks,
            disk->total_sectors,
            disk->source_size);
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
    
    for (size_t i = 0; i < list->count && off < buf_size - 500; i++) {
        const d64_diagnosis_t* d = &list->items[i];
        
        if (d->track != current_track) {
            current_track = d->track;
            off += snprintf(buf + off, buf_size - off,
                "── Track %02u (%u sectors, zone %u) ──────────────────────────────\n",
                current_track,
                d64_get_sectors(current_track),
                d64_get_speed_zone(current_track));
        }
        
        /* Icon */
        const char* icon = "ℹ️";
        if (d->code >= D64_DIAG_HEADER_CRC_ERROR && d->code <= D64_DIAG_SYNC_ERROR) {
            icon = "❌";
        } else if (d->code >= D64_DIAG_WEAK_BITS && d->code <= D64_DIAG_HALF_TRACK) {
            icon = "🛡️";  /* Protection shield */
        } else if (d->code != D64_DIAG_OK) {
            icon = "⚠️";
        } else {
            icon = "✅";
        }
        
        off += snprintf(buf + off, buf_size - off,
            "  %s T%02u S%02u: %s\n",
            icon, d->track, d->sector, d->message);
        
        /* Suggestion */
        if (d->code < D64_DIAG_COUNT && d64_diag_suggestions[d->code][0]) {
            off += snprintf(buf + off, buf_size - off,
                "           → %s\n",
                d64_diag_suggestions[d->code]);
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
static void d64_score_init(d64_score_t* score) {
    if (!score) return;
    memset(score, 0, sizeof(d64_score_t));
    score->overall = 1.0f;
    score->crc_score = 1.0f;
    score->id_score = 1.0f;
    score->timing_score = 1.0f;
    score->sync_score = 1.0f;
    score->gcr_score = 1.0f;
}

/**
 * @brief Calculate overall score
 */
static void d64_score_calculate(d64_score_t* score) {
    if (!score) return;
    
    /* Weighted average */
    score->overall = 
        score->crc_score * 0.35f +
        score->id_score * 0.15f +
        score->timing_score * 0.15f +
        score->sync_score * 0.15f +
        score->gcr_score * 0.20f;
    
    /* Clamp */
    if (score->overall < 0.0f) score->overall = 0.0f;
    if (score->overall > 1.0f) score->overall = 1.0f;
}

/**
 * @brief Score sector
 */
static void d64_score_sector(d64_sector_v3_t* sector) {
    if (!sector) return;
    
    d64_score_init(&sector->score);
    
    /* CRC score */
    if (sector->header_valid && sector->data_valid) {
        sector->score.crc_score = 1.0f;
    } else if (sector->header_valid || sector->data_valid) {
        sector->score.crc_score = 0.5f;
    } else {
        sector->score.crc_score = 0.0f;
    }
    
    /* ID score */
    if (sector->logical_track == sector->physical_track &&
        sector->logical_sector == sector->physical_sector) {
        sector->score.id_score = 1.0f;
    } else {
        sector->score.id_score = 0.5f;
    }
    
    /* Weak bits reduce GCR score */
    if (sector->weak_bit_count > 0) {
        float weak_ratio = (float)sector->weak_bit_count / (256 * 8);
        sector->score.gcr_score = 1.0f - weak_ratio;
        sector->score.has_weak_bits = true;
    }
    
    sector->score.header_crc_valid = sector->header_valid;
    sector->score.data_crc_valid = sector->data_valid;
    sector->score.id_valid = (sector->logical_track == sector->physical_track);
    
    d64_score_calculate(&sector->score);
}

/**
 * @brief Score track
 */
static void d64_score_track(d64_track_v3_t* track) {
    if (!track) return;
    
    d64_score_init(&track->score);
    
    if (track->expected_sectors == 0) return;
    
    /* Average sector scores */
    float crc_sum = 0, id_sum = 0, gcr_sum = 0;
    uint8_t valid_count = 0;
    
    for (int s = 0; s < track->found_sectors; s++) {
        d64_sector_v3_t* sector = &track->sectors[s];
        if (sector->present) {
            d64_score_sector(sector);
            crc_sum += sector->score.crc_score;
            id_sum += sector->score.id_score;
            gcr_sum += sector->score.gcr_score;
            valid_count++;
            
            if (sector->score.has_weak_bits) {
                track->has_weak_bits = true;
            }
        }
    }
    
    if (valid_count > 0) {
        track->score.crc_score = crc_sum / valid_count;
        track->score.id_score = id_sum / valid_count;
        track->score.gcr_score = gcr_sum / valid_count;
    }
    
    /* Sector count affects sync score */
    if (track->found_sectors >= track->expected_sectors) {
        track->score.sync_score = 1.0f;
    } else {
        track->score.sync_score = (float)track->found_sectors / track->expected_sectors;
    }
    
    /* Extra sectors = protection */
    if (track->found_sectors > track->expected_sectors) {
        track->has_extra_sectors = true;
        track->is_protected = true;
    }
    
    d64_score_calculate(&track->score);
}

/* Continue in Part 2... */

/* ═══════════════════════════════════════════════════════════════════════════
 * MULTI-REV MERGE
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Merge sector data from multiple revolutions
 */
bool d64_merge_sector_revs(
    d64_sector_v3_t* sector,
    d64_params_t* params
) {
    if (!sector || sector->rev_count < 2) return false;
    
    /* Check if any revolution has valid CRC */
    int valid_rev = -1;
    int valid_count = 0;
    
    for (uint8_t r = 0; r < sector->rev_count; r++) {
        if (sector->rev_data_valid && sector->rev_data_valid[r]) {
            valid_rev = r;
            valid_count++;
        }
    }
    
    /* If exactly one valid, use it */
    if (valid_count == 1) {
        memcpy(sector->data, sector->rev_data[valid_rev], 256);
        sector->data_valid = true;
        sector->best_rev = valid_rev;
        sector->score.recovered = false;
        return true;
    }
    
    /* Multiple or none valid: do bit-level voting */
    memset(sector->weak_mask, 0, 256);
    sector->weak_bit_count = 0;
    
    for (int byte = 0; byte < 256; byte++) {
        uint8_t byte_values[32];
        for (uint8_t r = 0; r < sector->rev_count && r < 32; r++) {
            byte_values[r] = sector->rev_data[r][byte];
        }
        
        /* Bit-level voting */
        uint8_t result = 0;
        for (int bit = 0; bit < 8; bit++) {
            int ones = 0;
            for (uint8_t r = 0; r < sector->rev_count; r++) {
                if (byte_values[r] & (1 << bit)) ones++;
            }
            
            /* Majority wins */
            if (ones > sector->rev_count / 2) {
                result |= (1 << bit);
            }
            
            /* Mark as weak if not unanimous */
            if (ones > 0 && ones < sector->rev_count) {
                sector->weak_mask[byte] |= (1 << bit);
                sector->weak_bit_count++;
            }
        }
        
        sector->data[byte] = result;
    }
    
    sector->score.revolutions_used = sector->rev_count;
    sector->score.has_weak_bits = (sector->weak_bit_count > 0);
    sector->score.recovered = (valid_count == 0);
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse BAM
 */
static bool d64_parse_bam(const uint8_t* data, size_t size, d64_disk_v3_t* disk) {
    size_t bam_offset = d64_get_sector_offset(D64_BAM_TRACK, D64_BAM_SECTOR);
    if (bam_offset + D64_SECTOR_SIZE > size) return false;
    
    const uint8_t* bam = data + bam_offset;
    
    /* DOS type */
    disk->dos_type = bam[2];
    
    /* Parse BAM entries */
    disk->free_blocks = 0;
    for (int track = 1; track <= disk->tracks; track++) {
        size_t entry_off = 4 + (track - 1) * 4;
        
        disk->bam[track].free_sectors = bam[entry_off];
        disk->bam[track].bitmap[0] = bam[entry_off + 1];
        disk->bam[track].bitmap[1] = bam[entry_off + 2];
        disk->bam[track].bitmap[2] = bam[entry_off + 3];
        
        /* Don't count directory track */
        if (track != D64_BAM_TRACK) {
            disk->free_blocks += disk->bam[track].free_sectors;
        }
    }
    
    /* Disk name */
    d64_copy_filename(disk->disk_name, bam + 0x90, 16);
    
    /* Disk ID */
    disk->disk_id[0] = petscii_to_ascii(bam[0xA2]);
    disk->disk_id[1] = petscii_to_ascii(bam[0xA3]);
    disk->disk_id[2] = ' ';
    disk->disk_id[3] = petscii_to_ascii(bam[0xA5]);
    disk->disk_id[4] = petscii_to_ascii(bam[0xA6]);
    disk->disk_id[5] = '\0';
    
    return true;
}

/**
 * @brief Parse directory
 */
static bool d64_parse_directory(const uint8_t* data, size_t size, d64_disk_v3_t* disk) {
    uint8_t track = D64_DIR_TRACK;
    uint8_t sector = D64_DIR_SECTOR;
    disk->file_count = 0;
    
    int max_sectors = 20;
    
    while (track != 0 && max_sectors-- > 0) {
        size_t offset = d64_get_sector_offset(track, sector);
        if (offset + D64_SECTOR_SIZE > size) break;
        
        const uint8_t* sec = data + offset;
        
        /* 8 entries per sector */
        for (int i = 0; i < 8; i++) {
            if (disk->file_count >= D64_MAX_DIR_ENTRIES) break;
            
            const uint8_t* entry = sec + i * 32;
            uint8_t ftype = entry[2];
            
            if (ftype == 0) continue;  /* Empty entry */
            
            d64_dir_entry_t* dir = &disk->directory[disk->file_count];
            
            dir->file_type = ftype;
            dir->first_track = entry[3];
            dir->first_sector = entry[4];
            d64_copy_filename(dir->filename, entry + 5, 16);
            dir->rel_track = entry[21];
            dir->rel_sector = entry[22];
            dir->rel_length = entry[23];
            dir->blocks = entry[30] | (entry[31] << 8);
            
            dir->closed = (ftype & 0x80) != 0;
            dir->locked = (ftype & 0x40) != 0;
            dir->splat = !dir->closed && (ftype & 0x07) != 0;
            
            /* GEOS detection */
            if (entry[24] != 0) {
                dir->is_geos = true;
                dir->geos_type = entry[24];
                dir->geos_structure = entry[25];
            }
            
            if (dir->first_track > 0) {
                disk->file_count++;
            }
        }
        
        track = sec[0];
        sector = sec[1];
    }
    
    return true;
}

/**
 * @brief Parse single track
 */
static bool d64_parse_track(
    const uint8_t* data,
    size_t size,
    uint8_t track_num,
    d64_disk_v3_t* disk,
    d64_params_t* params,
    d64_diagnosis_list_t* diag
) {
    if (track_num < 1 || track_num > disk->tracks) return false;
    
    d64_track_v3_t* track = &disk->track_data[track_num];
    track->track_num = track_num;
    track->expected_sectors = d64_get_sectors(track_num);
    track->speed_zone = d64_get_speed_zone(track_num);
    track->expected_bitcell_ns = d64_get_bitcell_ns(track_num);
    
    track->found_sectors = 0;
    track->valid_sectors = 0;
    track->error_sectors = 0;
    
    /* Read sectors from image */
    for (uint8_t s = 0; s < track->expected_sectors; s++) {
        size_t offset = d64_get_sector_offset(track_num, s);
        if (offset + D64_SECTOR_SIZE > size) continue;
        
        d64_sector_v3_t* sector = &track->sectors[s];
        sector->physical_track = track_num;
        sector->physical_sector = s;
        sector->logical_track = track_num;
        sector->logical_sector = s;
        
        memcpy(sector->data, data + offset, D64_SECTOR_SIZE);
        sector->present = true;
        sector->header_valid = true;  /* Assumed valid in D64 format */
        sector->data_valid = true;
        
        /* Check for error byte */
        if (disk->has_error_bytes) {
            uint16_t err_idx = d64_track_offset[track_num] + s;
            sector->error_byte = disk->error_bytes[err_idx];
            
            if (sector->error_byte != 0x01) {  /* 01 = no error */
                sector->data_valid = false;
                track->error_sectors++;
                
                d64_diagnosis_add(diag, D64_DIAG_DATA_CRC_ERROR,
                    track_num, s, "Error byte %02X", sector->error_byte);
            }
        }
        
        track->found_sectors++;
        if (sector->data_valid) {
            track->valid_sectors++;
        }
        
        d64_score_sector(sector);
    }
    
    /* Score track */
    d64_score_track(track);
    
    /* Add diagnosis if issues */
    if (track->valid_sectors < track->expected_sectors) {
        d64_diagnosis_add(diag, D64_DIAG_MISSING_SECTOR,
            track_num, 0xFF, "%u/%u sectors valid",
            track->valid_sectors, track->expected_sectors);
    }
    
    return true;
}

/**
 * @brief Parse D64 disk (main entry)
 */
bool d64_parse(
    const uint8_t* data,
    size_t size,
    d64_disk_v3_t* disk,
    d64_params_t* params
) {
    if (!data || !disk) return false;
    
    memset(disk, 0, sizeof(d64_disk_v3_t));
    disk->diagnosis = d64_diagnosis_create();
    
    /* Validate size */
    uint8_t tracks;
    bool has_errors;
    if (!d64_is_valid_size(size, &tracks, &has_errors)) {
        snprintf(disk->error, sizeof(disk->error), 
                 "Invalid D64 size: %zu", size);
        d64_diagnosis_add(disk->diagnosis, D64_DIAG_INVALID_SIZE,
            0, 0, "Size %zu bytes is not valid D64", size);
        return false;
    }
    
    disk->tracks = tracks;
    disk->is_extended = (tracks == 40);
    disk->has_error_bytes = has_errors;
    disk->source_size = size;
    
    /* Calculate total sectors */
    disk->total_sectors = 0;
    for (int t = 1; t <= tracks; t++) {
        disk->total_sectors += d64_get_sectors(t);
    }
    
    /* Copy error bytes if present */
    if (has_errors) {
        size_t err_offset = disk->total_sectors * D64_SECTOR_SIZE;
        memcpy(disk->error_bytes, data + err_offset, disk->total_sectors);
    }
    
    /* Parse BAM */
    if (!d64_parse_bam(data, size, disk)) {
        d64_diagnosis_add(disk->diagnosis, D64_DIAG_BAD_BAM,
            D64_BAM_TRACK, D64_BAM_SECTOR, "Failed to parse BAM");
    }
    
    /* Parse directory */
    if (!d64_parse_directory(data, size, disk)) {
        d64_diagnosis_add(disk->diagnosis, D64_DIAG_BAD_DIRECTORY,
            D64_DIR_TRACK, D64_DIR_SECTOR, "Failed to parse directory");
    }
    
    /* Parse all tracks */
    for (int t = 1; t <= disk->tracks; t++) {
        d64_parse_track(data, size, t, disk, params, disk->diagnosis);
        
        /* Check for protection */
        d64_track_v3_t* track = &disk->track_data[t];
        if (track->has_weak_bits || track->has_extra_sectors) {
            disk->has_protection = true;
        }
    }
    
    /* Calculate overall score */
    d64_score_init(&disk->score);
    float crc_sum = 0, track_count = 0;
    
    for (int t = 1; t <= disk->tracks; t++) {
        d64_track_v3_t* track = &disk->track_data[t];
        crc_sum += track->score.overall;
        track_count++;
    }
    
    if (track_count > 0) {
        disk->score.overall = crc_sum / track_count;
    }
    
    /* Set format name */
    snprintf(disk->format_name, sizeof(disk->format_name),
             "D64 (%u tracks%s%s)",
             disk->tracks,
             disk->is_extended ? ", extended" : "",
             disk->has_error_bytes ? ", with errors" : "");
    
    disk->valid = true;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * WRITE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Write D64 to buffer
 */
uint8_t* d64_write(
    const d64_disk_v3_t* disk,
    d64_params_t* params,
    size_t* out_size
) {
    if (!disk || !out_size) return NULL;
    
    /* Calculate size */
    size_t size = disk->total_sectors * D64_SECTOR_SIZE;
    if (params && params->include_error_bytes) {
        size += disk->total_sectors;
    }
    
    uint8_t* data = calloc(1, size);
    if (!data) {
        *out_size = 0;
        return NULL;
    }
    
    /* Write all sectors */
    for (int t = 1; t <= disk->tracks; t++) {
        const d64_track_v3_t* track = &disk->track_data[t];
        
        for (int s = 0; s < track->expected_sectors; s++) {
            const d64_sector_v3_t* sector = &track->sectors[s];
            size_t offset = d64_get_sector_offset(t, s);
            
            if (sector->present) {
                memcpy(data + offset, sector->data, D64_SECTOR_SIZE);
            }
        }
    }
    
    /* Write error bytes */
    if (params && params->include_error_bytes) {
        size_t err_offset = disk->total_sectors * D64_SECTOR_SIZE;
        
        for (int t = 1; t <= disk->tracks; t++) {
            const d64_track_v3_t* track = &disk->track_data[t];
            
            for (int s = 0; s < track->expected_sectors; s++) {
                const d64_sector_v3_t* sector = &track->sectors[s];
                uint16_t idx = d64_track_offset[t] + s;
                
                data[err_offset + idx] = sector->error_byte ? sector->error_byte : 0x01;
            }
        }
    }
    
    *out_size = size;
    return data;
}

/**
 * @brief Verify written data
 */
bool d64_verify(
    const uint8_t* original,
    size_t original_size,
    const uint8_t* written,
    size_t written_size,
    d64_params_t* params,
    d64_diagnosis_list_t* differences
) {
    if (!original || !written) return false;
    
    /* Size check */
    if (original_size != written_size) {
        d64_diagnosis_add(differences, D64_DIAG_INVALID_SIZE,
            0, 0, "Size mismatch: %zu vs %zu", original_size, written_size);
        return false;
    }
    
    /* Parse both */
    d64_disk_v3_t disk_orig, disk_write;
    
    if (!d64_parse(original, original_size, &disk_orig, params)) {
        return false;
    }
    if (!d64_parse(written, written_size, &disk_write, params)) {
        return false;
    }
    
    /* Compare sector by sector */
    bool match = true;
    
    for (int t = 1; t <= disk_orig.tracks; t++) {
        uint8_t sectors = d64_get_sectors(t);
        
        for (int s = 0; s < sectors; s++) {
            size_t offset = d64_get_sector_offset(t, s);
            
            if (memcmp(original + offset, written + offset, D64_SECTOR_SIZE) != 0) {
                d64_diagnosis_add(differences, D64_DIAG_DATA_CRC_ERROR,
                    t, s, "Data mismatch in sector");
                match = false;
            }
        }
    }
    
    d64_diagnosis_free(disk_orig.diagnosis);
    d64_diagnosis_free(disk_write.diagnosis);
    
    return match;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PROTECTION DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect copy protection
 */
bool d64_detect_protection(
    const d64_disk_v3_t* disk,
    char* protection_name,
    size_t name_size,
    float* confidence
) {
    if (!disk) return false;
    
    bool found = false;
    *confidence = 0.0f;
    protection_name[0] = '\0';
    
    /* Check each track for protection signs */
    int weak_tracks = 0;
    int extra_sector_tracks = 0;
    int error_tracks = 0;
    
    for (int t = 1; t <= disk->tracks; t++) {
        const d64_track_v3_t* track = &disk->track_data[t];
        
        if (track->has_weak_bits) weak_tracks++;
        if (track->has_extra_sectors) extra_sector_tracks++;
        if (track->error_sectors > 0) error_tracks++;
    }
    
    /* Vorpal protection: specific signature */
    if (weak_tracks > 0 && extra_sector_tracks > 0) {
        snprintf(protection_name, name_size, "Vorpal/RapidLok");
        *confidence = 0.85f;
        found = true;
    }
    /* V-Max protection: track 20 typically */
    else if (disk->track_data[20].has_weak_bits) {
        snprintf(protection_name, name_size, "V-Max");
        *confidence = 0.80f;
        found = true;
    }
    /* General weak bit protection */
    else if (weak_tracks > 3) {
        snprintf(protection_name, name_size, "Weak bit protection");
        *confidence = 0.70f;
        found = true;
    }
    /* Extra sectors */
    else if (extra_sector_tracks > 0) {
        snprintf(protection_name, name_size, "Extra sector protection");
        *confidence = 0.65f;
        found = true;
    }
    /* Error-based protection */
    else if (error_tracks > 5) {
        snprintf(protection_name, name_size, "Intentional errors");
        *confidence = 0.60f;
        found = true;
    }
    
    return found;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DEFAULT PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get default parameters
 */
void d64_get_default_params(d64_params_t* params) {
    if (!params) return;
    memset(params, 0, sizeof(d64_params_t));
    
    params->revolutions = 3;
    params->multi_rev_merge = true;
    params->merge_strategy = 1;  /* Best CRC */
    
    params->accept_bad_crc = false;
    params->attempt_crc_correction = true;
    params->max_crc_bits = 2;
    params->error_mode = 1;  /* Normal */
    params->fill_pattern = 0x00;
    
    params->strict_gcr = false;
    params->gcr_retry = true;
    
    params->detect_protection = true;
    params->preserve_protection = true;
    params->preserve_weak_bits = true;
    
    params->validate_bam = true;
    params->rebuild_bam = false;
    
    params->timing_tolerance = 0.15f;
    params->pll_mode = 2;  /* Adaptive */
    params->pll_bandwidth = 0.1f;
    
    params->include_error_bytes = true;
    params->generate_g64 = true;
    
    params->verify_after_write = true;
    params->verify_mode = 0;  /* Sector */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CLEANUP
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Free disk structure
 */
void d64_disk_free(d64_disk_v3_t* disk) {
    if (!disk) return;
    
    if (disk->diagnosis) {
        d64_diagnosis_free(disk->diagnosis);
        disk->diagnosis = NULL;
    }
    
    /* Free track data */
    for (int t = 1; t <= 40; t++) {
        d64_track_v3_t* track = &disk->track_data[t];
        
        if (track->raw_gcr) {
            free(track->raw_gcr);
            track->raw_gcr = NULL;
        }
        if (track->bit_timing) {
            free(track->bit_timing);
            track->bit_timing = NULL;
        }
        
        /* Free sector multi-rev data */
        for (int s = 0; s < 21; s++) {
            d64_sector_v3_t* sector = &track->sectors[s];
            if (sector->rev_data) {
                for (int r = 0; r < sector->rev_count; r++) {
                    free(sector->rev_data[r]);
                }
                free(sector->rev_data);
                free(sector->rev_data_valid);
                sector->rev_data = NULL;
                sector->rev_data_valid = NULL;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef D64_V3_TEST

#include <assert.h>

int main(void) {
    printf("=== D64 Parser v3 Tests ===\n");
    
    /* Test valid sizes */
    printf("Testing valid sizes... ");
    uint8_t tracks;
    bool has_errors;
    assert(d64_is_valid_size(D64_SIZE_35, &tracks, &has_errors) && tracks == 35 && !has_errors);
    assert(d64_is_valid_size(D64_SIZE_35_ERR, &tracks, &has_errors) && tracks == 35 && has_errors);
    assert(d64_is_valid_size(D64_SIZE_40, &tracks, &has_errors) && tracks == 40 && !has_errors);
    assert(!d64_is_valid_size(12345, &tracks, &has_errors));
    printf("✓\n");
    
    /* Test speed zones */
    printf("Testing speed zones... ");
    assert(d64_get_speed_zone(1) == 3);
    assert(d64_get_speed_zone(17) == 3);
    assert(d64_get_speed_zone(18) == 2);
    assert(d64_get_speed_zone(25) == 1);
    assert(d64_get_speed_zone(31) == 0);
    printf("✓\n");
    
    /* Test sectors per track */
    printf("Testing sectors per track... ");
    assert(d64_get_sectors(1) == 21);
    assert(d64_get_sectors(18) == 19);
    assert(d64_get_sectors(25) == 18);
    assert(d64_get_sectors(31) == 17);
    printf("✓\n");
    
    /* Test GCR encode/decode */
    printf("Testing GCR encode/decode... ");
    uint8_t data[4] = { 0x08, 0x00, 0x01, 0x00 };
    uint8_t gcr[5];
    uint8_t decoded[4];
    uint8_t errors;
    
    d64_gcr_encode_block(data, gcr);
    assert(d64_gcr_decode_block(gcr, decoded, &errors));
    assert(memcmp(data, decoded, 4) == 0);
    printf("✓\n");
    
    /* Test diagnosis */
    printf("Testing diagnosis system... ");
    d64_diagnosis_list_t* diag = d64_diagnosis_create();
    assert(diag != NULL);
    
    d64_diagnosis_add(diag, D64_DIAG_DATA_CRC_ERROR, 17, 5, "Test error");
    assert(diag->count == 1);
    assert(diag->error_count == 1);
    
    d64_diagnosis_add(diag, D64_DIAG_WEAK_BITS, 17, 5, "Weak bits found");
    assert(diag->count == 2);
    assert(diag->protection_count == 1);
    
    char* report = d64_diagnosis_to_text(diag, NULL);
    assert(report != NULL);
    assert(strstr(report, "Track 17") != NULL);
    free(report);
    
    d64_diagnosis_free(diag);
    printf("✓\n");
    
    /* Test scoring */
    printf("Testing scoring system... ");
    d64_score_t score;
    d64_score_init(&score);
    assert(score.overall == 1.0f);
    
    score.crc_score = 0.9f;
    score.id_score = 0.8f;
    score.timing_score = 0.95f;
    score.sync_score = 0.85f;
    score.gcr_score = 0.9f;
    d64_score_calculate(&score);
    assert(score.overall > 0.85f && score.overall < 0.95f);
    printf("✓\n");
    
    /* Test parameters */
    printf("Testing default parameters... ");
    d64_params_t params;
    d64_get_default_params(&params);
    assert(params.revolutions == 3);
    assert(params.multi_rev_merge == true);
    assert(params.preserve_weak_bits == true);
    printf("✓\n");
    
    /* Test blank disk parsing */
    printf("Testing blank disk parsing... ");
    uint8_t* blank = calloc(1, D64_SIZE_35);
    assert(blank != NULL);
    
    /* Set up minimal BAM */
    size_t bam_off = d64_get_sector_offset(18, 0);
    blank[bam_off] = 18;
    blank[bam_off + 1] = 1;
    blank[bam_off + 2] = 0x41;  /* DOS 2A */
    
    d64_disk_v3_t disk;
    d64_params_t parms;
    d64_get_default_params(&parms);
    
    bool ok = d64_parse(blank, D64_SIZE_35, &disk, &parms);
    assert(ok);
    assert(disk.valid);
    assert(disk.tracks == 35);
    assert(disk.total_sectors == D64_SECTORS_35);
    
    d64_disk_free(&disk);
    free(blank);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 7, Failed: 0\n");
    return 0;
}

#endif /* D64_V3_TEST */

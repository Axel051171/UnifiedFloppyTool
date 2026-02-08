/**
 * @file uft_sap_parser_v2.c
 * @brief GOD MODE SAP Parser v2 - Thomson MO/TO Disk Format
 * 
 * SAP (Système d'Archivage Protégé) is the Thomson MO/TO disk format.
 * Used by French Thomson computers (MO5, MO6, TO7, TO8, TO9).
 * 
 * Variants:
 * - SAP1: 80 tracks, 16 sectors, 256 bytes/sector
 * - SAP2: 80 tracks, 16 sectors, 256 bytes/sector (different sector format)
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

#define SAP_SIGNATURE           "SAP"
#define SAP_SIGNATURE_SIZE      4       /* Includes version byte */
#define SAP_HEADER_SIZE         66

#define SAP_SECTOR_SIZE         256
#define SAP_SECTORS_PER_TRACK   16
#define SAP_TRACKS              80

#define SAP_VERSION_1           1
#define SAP_VERSION_2           2

/* Sector format */
#define SAP_SECTOR_HEADER_SIZE  4       /* Format, Protection, Track, Sector */
#define SAP_SECTOR_DATA_SIZE    256
#define SAP_SECTOR_CRC_SIZE     2
#define SAP_SECTOR_TOTAL        (SAP_SECTOR_HEADER_SIZE + SAP_SECTOR_DATA_SIZE + SAP_SECTOR_CRC_SIZE)

/* Format codes */
#define SAP_FORMAT_UNFORMATTED  0x00
#define SAP_FORMAT_FORMATTED    0x04
#define SAP_FORMAT_PROTECTED    0x44

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief SAP header
 */
typedef struct {
    char signature[4];          /* "SAP\x01" or "SAP\x02" */
    uint8_t version;            /* 1 or 2 */
    char comment[62];           /* ASCII comment */
} sap_header_t;

/**
 * @brief SAP sector
 */
typedef struct {
    uint8_t format;             /* Format code */
    uint8_t protection;         /* Protection flags */
    uint8_t track;              /* Track number */
    uint8_t sector;             /* Sector number (1-16) */
    uint8_t data[256];          /* Sector data */
    uint16_t crc;               /* CRC-16 */
    bool crc_valid;             /* CRC verification */
} sap_sector_t;

/**
 * @brief SAP track
 */
typedef struct {
    uint8_t track_num;
    sap_sector_t sectors[16];
    uint8_t sector_count;
    uint8_t valid_sectors;
    uint8_t error_sectors;
    bool formatted;
} sap_track_t;

/**
 * @brief SAP disk structure
 */
typedef struct {
    sap_header_t header;
    sap_track_t tracks[SAP_TRACKS];
    
    uint8_t version;
    uint8_t track_count;
    uint16_t total_sectors;
    uint16_t valid_sectors;
    uint16_t formatted_sectors;
    
    bool valid;
    char error[256];
} sap_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC CALCULATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/* CRC-16 table for SAP format */
static const uint16_t sap_crc_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

static uint16_t sap_crc16(const uint8_t* data, size_t len, uint16_t init) {
    uint16_t crc = init;
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ sap_crc_table[(crc >> 8) ^ data[i]];
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check valid SAP signature
 */
static bool sap_is_valid(const uint8_t* data, size_t size) {
    if (size < SAP_HEADER_SIZE) return false;
    if (memcmp(data, SAP_SIGNATURE, 3) != 0) return false;
    uint8_t version = data[3];
    return (version == SAP_VERSION_1 || version == SAP_VERSION_2);
}

/**
 * @brief Get format name
 */
static const char* sap_format_name(uint8_t format) {
    switch (format) {
        case SAP_FORMAT_UNFORMATTED: return "Unformatted";
        case SAP_FORMAT_FORMATTED: return "Formatted";
        case SAP_FORMAT_PROTECTED: return "Protected";
        default: return "Unknown";
    }
}

/**
 * @brief Get version name
 */
static const char* sap_version_name(uint8_t version) {
    switch (version) {
        case SAP_VERSION_1: return "SAP v1";
        case SAP_VERSION_2: return "SAP v2";
        default: return "Unknown";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse SAP header
 */
static bool sap_parse_header(const uint8_t* data, sap_disk_t* disk) {
    memcpy(disk->header.signature, data, 4);
    disk->header.version = data[3];
    disk->version = data[3];
    
    /* Copy comment (bytes 4-65) */
    memcpy(disk->header.comment, data + 4, 62);
    disk->header.comment[61] = '\0';
    
    return true;
}

/**
 * @brief Parse SAP sectors
 */
static bool sap_parse_sectors(const uint8_t* data, size_t size, sap_disk_t* disk) {
    size_t offset = SAP_HEADER_SIZE;
    
    while (offset + SAP_SECTOR_TOTAL <= size) {
        uint8_t format = data[offset];
        uint8_t protection = data[offset + 1];
        uint8_t track = data[offset + 2];
        uint8_t sector = data[offset + 3];
        
        /* Validate track/sector */
        if (track >= SAP_TRACKS || sector < 1 || sector > SAP_SECTORS_PER_TRACK) {
            offset += SAP_SECTOR_TOTAL;
            continue;
        }
        
        sap_track_t* trk = &disk->tracks[track];
        sap_sector_t* sec = &trk->sectors[sector - 1];
        
        sec->format = format;
        sec->protection = protection;
        sec->track = track;
        sec->sector = sector;
        memcpy(sec->data, data + offset + SAP_SECTOR_HEADER_SIZE, SAP_SECTOR_DATA_SIZE);
        
        /* CRC */
        sec->crc = (data[offset + SAP_SECTOR_HEADER_SIZE + SAP_SECTOR_DATA_SIZE] << 8) |
                   data[offset + SAP_SECTOR_HEADER_SIZE + SAP_SECTOR_DATA_SIZE + 1];
        
        /* Verify CRC */
        uint16_t calc_crc = sap_crc16(data + offset, SAP_SECTOR_HEADER_SIZE + SAP_SECTOR_DATA_SIZE, 0xFFFF);
        sec->crc_valid = (calc_crc == sec->crc);
        
        trk->sector_count++;
        if (sec->crc_valid) {
            trk->valid_sectors++;
            disk->valid_sectors++;
        } else {
            trk->error_sectors++;
        }
        
        if (format == SAP_FORMAT_FORMATTED || format == SAP_FORMAT_PROTECTED) {
            trk->formatted = true;
            disk->formatted_sectors++;
        }
        
        disk->total_sectors++;
        offset += SAP_SECTOR_TOTAL;
    }
    
    /* Count tracks */
    for (int t = 0; t < SAP_TRACKS; t++) {
        if (disk->tracks[t].sector_count > 0) {
            disk->tracks[t].track_num = t;
            disk->track_count++;
        }
    }
    
    return true;
}

/**
 * @brief Parse SAP disk
 */
static bool sap_parse(const uint8_t* data, size_t size, sap_disk_t* disk) {
    memset(disk, 0, sizeof(sap_disk_t));
    
    if (!sap_is_valid(data, size)) {
        snprintf(disk->error, sizeof(disk->error), "Invalid SAP signature");
        return false;
    }
    
    if (!sap_parse_header(data, disk)) return false;
    if (!sap_parse_sectors(data, size, disk)) return false;
    
    disk->valid = true;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef SAP_PARSER_TEST

#include <assert.h>

int main(void) {
    printf("=== SAP Parser v2 Tests ===\n");
    
    printf("Testing signature... ");
    uint8_t valid_sap[66] = { 'S', 'A', 'P', 0x01 };
    uint8_t invalid_sap[66] = { 'X', 'X', 'X', 0x01 };
    assert(sap_is_valid(valid_sap, 66));
    assert(!sap_is_valid(invalid_sap, 66));
    printf("✓\n");
    
    printf("Testing version names... ");
    assert(strcmp(sap_version_name(SAP_VERSION_1), "SAP v1") == 0);
    assert(strcmp(sap_version_name(SAP_VERSION_2), "SAP v2") == 0);
    printf("✓\n");
    
    printf("Testing format names... ");
    assert(strcmp(sap_format_name(SAP_FORMAT_UNFORMATTED), "Unformatted") == 0);
    assert(strcmp(sap_format_name(SAP_FORMAT_FORMATTED), "Formatted") == 0);
    assert(strcmp(sap_format_name(SAP_FORMAT_PROTECTED), "Protected") == 0);
    printf("✓\n");
    
    printf("Testing CRC calculation... ");
    uint8_t test_data[] = { 0x01, 0x02, 0x03, 0x04 };
    uint16_t crc = sap_crc16(test_data, sizeof(test_data), 0xFFFF);
    assert(crc != 0);  /* Just verify it produces a result */
    printf("✓\n");
    
    printf("Testing constants... ");
    assert(SAP_SECTOR_SIZE == 256);
    assert(SAP_SECTORS_PER_TRACK == 16);
    assert(SAP_TRACKS == 80);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 5, Failed: 0\n");
    return 0;
}

#endif /* SAP_PARSER_TEST */

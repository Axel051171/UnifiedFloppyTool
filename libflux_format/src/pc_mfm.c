// SPDX-License-Identifier: MIT
/*
 * pc_mfm.c - PC DOS MFM Sector Decoder
 * 
 * Ported from FloppyControl (C# -> C)
 * Original: https://github.com/Skaizo/FloppyControl
 * 
 * Supports:
 *   - PC DOS DD (720K, 360K)
 *   - PC DOS HD (1.44M, 1.2M)
 *   - MSX-DOS
 *   - Atari ST
 *   - Extended 2M format
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*============================================================================*
 * PC MFM CONSTANTS
 *============================================================================*/

/* IBM MFM sync pattern: 3x 0xA1 with missing clock */
/* 0xA1 with missing clock = 0x4489 in MFM */
static const uint8_t PC_A1_MARKER[] = {
    0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,1,  /* 0x4489 */
    0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,1,  /* 0x4489 */
    0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,1   /* 0x4489 */
};
#define PC_A1_MARKER_LEN 48

/* Address marks */
#define IDAM    0xFE    /* ID Address Mark */
#define DAM     0xFB    /* Data Address Mark */
#define DDAM    0xF8    /* Deleted Data Address Mark */

/* Sector sizes */
#define SECTOR_SIZE_128     128
#define SECTOR_SIZE_256     256
#define SECTOR_SIZE_512     512
#define SECTOR_SIZE_1024    1024

/*============================================================================*
 * CRC-16-CCITT
 *============================================================================*/

/* CRC-16 lookup table (polynomial 0x1021, init 0xFFFF) */
static const uint16_t crc16_table[256] = {
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

/**
 * @brief Calculate CRC-16-CCITT
 * 
 * @param data Input data
 * @param len Data length
 * @return CRC-16 value
 */
uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    
    return crc;
}

/**
 * @brief Calculate CRC including sync bytes (0xA1 x3)
 * 
 * For PC MFM, CRC is calculated starting from the sync bytes.
 */
uint16_t crc16_with_sync(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    
    /* Include 3x 0xA1 sync bytes */
    for (int i = 0; i < 3; i++) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ 0xA1) & 0xFF];
    }
    
    /* Then the actual data */
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    
    return crc;
}

/*============================================================================*
 * DATA STRUCTURES
 *============================================================================*/

typedef enum {
    PC_FORMAT_UNKNOWN = 0,
    PC_FORMAT_DD,           /* 720K (80 tracks, 9 sectors) */
    PC_FORMAT_HD,           /* 1.44M (80 tracks, 18 sectors) */
    PC_FORMAT_DD_360,       /* 360K (40 tracks, 9 sectors) */
    PC_FORMAT_HD_1200,      /* 1.2M (80 tracks, 15 sectors) */
    PC_FORMAT_2M,           /* Extended 2M format */
    PC_FORMAT_MSX,          /* MSX-DOS */
    PC_FORMAT_ATARI_ST      /* Atari ST */
} pc_format_t;

typedef struct {
    uint8_t track;          /* Cylinder number */
    uint8_t head;           /* Head (0 or 1) */
    uint8_t sector;         /* Sector number (1-based) */
    uint8_t size_code;      /* Size code: 0=128, 1=256, 2=512, 3=1024 */
    uint16_t crc;           /* CRC from disk */
    bool header_ok;         /* Header CRC valid */
    bool data_ok;           /* Data CRC valid */
    bool deleted;           /* Deleted data mark */
    int sector_size;        /* Actual sector size in bytes */
    uint8_t *data;          /* Sector data (dynamically allocated) */
} pc_sector_t;

typedef struct {
    int marker_position;    /* Position in MFM stream */
    pc_sector_t sector;     /* Decoded sector */
} pc_marker_t;

/*============================================================================*
 * MFM DECODING
 *============================================================================*/

/**
 * @brief Decode MFM bitstream to bytes
 * 
 * Standard MFM decoding: extract data bits, ignore clock bits.
 * 
 * @param mfm MFM bitstream
 * @param offset Bit offset
 * @param out Output buffer
 * @param len Number of bytes to decode
 */
static void mfm_decode_bytes(const uint8_t *mfm, int offset, uint8_t *out, int len) {
    for (int i = 0; i < len; i++) {
        uint8_t byte = 0;
        
        for (int b = 0; b < 8; b++) {
            /* Skip clock bit (odd positions), read data bit (even positions) */
            int bit_pos = offset + i * 16 + b * 2 + 1;
            byte = (byte << 1) | (mfm[bit_pos] & 1);
        }
        
        out[i] = byte;
    }
}

/**
 * @brief Find all PC A1 markers in MFM stream
 * 
 * @param mfm MFM bitstream
 * @param mfm_len MFM length
 * @param markers Output marker array
 * @param max_markers Maximum markers to find
 * @return Number of markers found
 */
int pc_find_markers(
    const uint8_t *mfm,
    size_t mfm_len,
    pc_marker_t *markers,
    int max_markers)
{
    if (!mfm || !markers || mfm_len < PC_A1_MARKER_LEN) return 0;
    
    int found = 0;
    
    for (size_t i = 0; i < mfm_len - PC_A1_MARKER_LEN && found < max_markers; i++) {
        bool match = true;
        
        for (int j = 0; j < PC_A1_MARKER_LEN && match; j++) {
            if (mfm[i + j] != PC_A1_MARKER[j]) {
                match = false;
            }
        }
        
        if (match) {
            markers[found].marker_position = i;
            memset(&markers[found].sector, 0, sizeof(pc_sector_t));
            found++;
            
            /* Skip past this marker */
            i += PC_A1_MARKER_LEN;
        }
    }
    
    return found;
}

/**
 * @brief Decode PC sector header (IDAM)
 * 
 * @param mfm MFM bitstream
 * @param marker_pos Marker position
 * @param sector Output sector info
 * @return 0 on success, -1 if not IDAM
 */
int pc_decode_header(
    const uint8_t *mfm,
    int marker_pos,
    pc_sector_t *sector)
{
    if (!mfm || !sector) return -1;
    
    /* Header starts after sync (3x A1) */
    int header_offset = marker_pos + PC_A1_MARKER_LEN;
    
    /* Decode 6 bytes: IDAM + track + head + sector + size + CRC(2) */
    uint8_t header[7];
    mfm_decode_bytes(mfm, header_offset, header, 7);
    
    /* Check for IDAM */
    if (header[0] != IDAM) {
        return -1;  /* Not an ID field */
    }
    
    sector->track = header[1];
    sector->head = header[2];
    sector->sector = header[3];
    sector->size_code = header[4];
    sector->crc = (header[5] << 8) | header[6];
    
    /* Calculate sector size from size code */
    sector->sector_size = 128 << sector->size_code;
    if (sector->sector_size > 8192) {
        sector->sector_size = 512;  /* Sanity check */
    }
    
    /* Verify header CRC */
    uint16_t calc_crc = crc16_with_sync(header, 5);  /* IDAM + 4 header bytes */
    sector->header_ok = (calc_crc == sector->crc);
    
    return 0;
}

/**
 * @brief Decode PC sector data
 * 
 * @param mfm MFM bitstream
 * @param data_marker_pos Position of data marker (after A1 sync)
 * @param sector Sector to fill (must have header decoded)
 * @return 0 on success
 */
int pc_decode_data(
    const uint8_t *mfm,
    int data_marker_pos,
    pc_sector_t *sector)
{
    if (!mfm || !sector) return -1;
    
    int data_offset = data_marker_pos + PC_A1_MARKER_LEN;
    
    /* Decode address mark (DAM or DDAM) */
    uint8_t dam;
    mfm_decode_bytes(mfm, data_offset, &dam, 1);
    
    sector->deleted = (dam == DDAM);
    
    if (dam != DAM && dam != DDAM) {
        return -1;  /* Not a data field */
    }
    
    /* Allocate data buffer */
    if (sector->data) {
        free(sector->data);
    }
    sector->data = malloc(sector->sector_size);
    if (!sector->data) return -1;
    
    /* Decode sector data + CRC */
    uint8_t *buffer = malloc(sector->sector_size + 2);
    if (!buffer) {
        free(sector->data);
        sector->data = NULL;
        return -1;
    }
    
    mfm_decode_bytes(mfm, data_offset + 16, buffer, sector->sector_size + 2);
    
    memcpy(sector->data, buffer, sector->sector_size);
    uint16_t stored_crc = (buffer[sector->sector_size] << 8) | buffer[sector->sector_size + 1];
    
    /* Verify data CRC */
    uint16_t calc_crc = crc16_with_sync(&dam, 1);
    for (int i = 0; i < sector->sector_size; i++) {
        calc_crc = (calc_crc << 8) ^ crc16_table[((calc_crc >> 8) ^ sector->data[i]) & 0xFF];
    }
    
    sector->data_ok = (calc_crc == stored_crc);
    
    free(buffer);
    return 0;
}

/**
 * @brief Free sector data
 */
void pc_free_sector(pc_sector_t *sector) {
    if (sector && sector->data) {
        free(sector->data);
        sector->data = NULL;
    }
}

/*============================================================================*
 * DISK IMAGE CREATION
 *============================================================================*/

typedef struct {
    pc_format_t format;
    int tracks;
    int heads;
    int sectors_per_track;
    int sector_size;
    int total_size;
} pc_geometry_t;

/**
 * @brief Get geometry for format
 */
void pc_get_geometry(pc_format_t format, pc_geometry_t *geom) {
    if (!geom) return;
    
    switch (format) {
        case PC_FORMAT_DD:
            geom->tracks = 80;
            geom->heads = 2;
            geom->sectors_per_track = 9;
            geom->sector_size = 512;
            break;
            
        case PC_FORMAT_HD:
            geom->tracks = 80;
            geom->heads = 2;
            geom->sectors_per_track = 18;
            geom->sector_size = 512;
            break;
            
        case PC_FORMAT_DD_360:
            geom->tracks = 40;
            geom->heads = 2;
            geom->sectors_per_track = 9;
            geom->sector_size = 512;
            break;
            
        case PC_FORMAT_HD_1200:
            geom->tracks = 80;
            geom->heads = 2;
            geom->sectors_per_track = 15;
            geom->sector_size = 512;
            break;
            
        case PC_FORMAT_MSX:
            geom->tracks = 80;
            geom->heads = 2;
            geom->sectors_per_track = 9;
            geom->sector_size = 512;
            break;
            
        case PC_FORMAT_ATARI_ST:
            geom->tracks = 80;
            geom->heads = 2;
            geom->sectors_per_track = 9;
            geom->sector_size = 512;
            break;
            
        default:
            geom->tracks = 80;
            geom->heads = 2;
            geom->sectors_per_track = 9;
            geom->sector_size = 512;
            break;
    }
    
    geom->format = format;
    geom->total_size = geom->tracks * geom->heads * geom->sectors_per_track * geom->sector_size;
}

/**
 * @brief Create raw disk image from decoded sectors
 * 
 * @param sectors Array of decoded sectors
 * @param num_sectors Number of sectors
 * @param format Disk format
 * @param image_out Output image buffer
 * @param image_size Image buffer size
 * @return Bytes written, or -1 on error
 */
int pc_create_image(
    const pc_sector_t *sectors,
    int num_sectors,
    pc_format_t format,
    uint8_t *image_out,
    size_t image_size)
{
    if (!sectors || !image_out) return -1;
    
    pc_geometry_t geom;
    pc_get_geometry(format, &geom);
    
    if (image_size < (size_t)geom.total_size) return -1;
    
    /* Initialize to 0xF6 (standard fill pattern) */
    memset(image_out, 0xF6, geom.total_size);
    
    int written = 0;
    
    for (int i = 0; i < num_sectors; i++) {
        const pc_sector_t *s = &sectors[i];
        
        if (!s->data_ok || !s->data) continue;
        
        /* Calculate offset */
        size_t offset = ((s->track * geom.heads + s->head) * geom.sectors_per_track 
                        + (s->sector - 1)) * geom.sector_size;
        
        if (offset + geom.sector_size <= image_size) {
            memcpy(&image_out[offset], s->data, geom.sector_size);
            written++;
        }
    }
    
    return written;
}

/*============================================================================*
 * FORMAT DETECTION
 *============================================================================*/

/**
 * @brief Detect PC disk format from sectors
 * 
 * @param sectors Decoded sectors
 * @param num_sectors Number of sectors
 * @return Detected format
 */
pc_format_t pc_detect_format(const pc_sector_t *sectors, int num_sectors) {
    if (!sectors || num_sectors == 0) return PC_FORMAT_UNKNOWN;
    
    int max_track = 0;
    int max_sector = 0;
    int sector_size = 512;
    
    for (int i = 0; i < num_sectors; i++) {
        if (sectors[i].track > max_track) max_track = sectors[i].track;
        if (sectors[i].sector > max_sector) max_sector = sectors[i].sector;
        if (sectors[i].sector_size > sector_size) sector_size = sectors[i].sector_size;
    }
    
    /* Determine format based on geometry */
    if (max_track >= 79) {
        if (max_sector >= 18) return PC_FORMAT_HD;
        if (max_sector >= 15) return PC_FORMAT_HD_1200;
        if (max_sector >= 9) return PC_FORMAT_DD;
    } else if (max_track >= 39) {
        if (max_sector >= 9) return PC_FORMAT_DD_360;
    }
    
    return PC_FORMAT_DD;  /* Default */
}

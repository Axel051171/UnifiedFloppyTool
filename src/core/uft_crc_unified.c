/**
 * @file uft_crc_unified.c
 * @brief Unified CRC Implementation (P2-ARCH-002)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft/core/uft_crc_unified.h"
#include <string.h>

/*===========================================================================
 * CRC Parameter Database
 *===========================================================================*/

static const uft_crc_params_t g_crc_params[UFT_CRC_TYPE_COUNT] = {
    /* CRC-16 variants */
    [UFT_CRC_CCITT_FALSE] = { 0x1021, 0xFFFF, 0x0000, 16, false, false },
    [UFT_CRC_CCITT_TRUE]  = { 0x1021, 0x0000, 0x0000, 16, true,  true  },
    [UFT_CRC_IBM_ARC]     = { 0x8005, 0x0000, 0x0000, 16, true,  true  },
    [UFT_CRC_MODBUS]      = { 0x8005, 0xFFFF, 0x0000, 16, true,  true  },
    [UFT_CRC_XMODEM]      = { 0x1021, 0x0000, 0x0000, 16, false, false },
    
    /* Format-specific */
    [UFT_CRC_MFM]         = { 0x1021, 0xCDB4, 0x0000, 16, false, false }, /* After 3x 0xA1 */
    [UFT_CRC_AMIGA]       = { 0x0000, 0x0000, 0x0000, 32, false, false }, /* XOR-based */
    [UFT_CRC_GCR_C64]     = { 0x0000, 0x0000, 0x0000, 8,  false, false }, /* XOR */
    [UFT_CRC_GCR_APPLE]   = { 0x0000, 0x0000, 0x0000, 8,  false, false }, /* XOR */
    [UFT_CRC_OMTI16]      = { 0x1021, 0x7107, 0x0000, 16, false, false },
    
    /* CRC-32 variants */
    [UFT_CRC32_ISO]       = { 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, 32, true,  true  },
    [UFT_CRC32_POSIX]     = { 0x04C11DB7, 0x00000000, 0xFFFFFFFF, 32, false, false },
    [UFT_CRC32_CCSDS]     = { 0x00A00805, 0xFFFFFFFF, 0x00000000, 32, false, false },
    [UFT_CRC32_WD]        = { 0x140A0445, 0xFFFFFFFF, 0x00000000, 32, false, false },
    [UFT_CRC32_SEAGATE]   = { 0x41044185, 0x00000000, 0x00000000, 32, false, false },
    [UFT_CRC32_OMTI_HDR]  = { 0x0104C981, 0x2605FB9C, 0x00000000, 32, false, false },
    [UFT_CRC32_OMTI_DATA] = { 0x0104C981, 0x34B6E5E2, 0x00000000, 32, false, false },
    
    /* ECC */
    [UFT_CRC_ECC_FIRE]    = { 0x04418105, 0x00000000, 0x00000000, 32, false, false },
};

static const char* g_crc_names[UFT_CRC_TYPE_COUNT] = {
    [UFT_CRC_CCITT_FALSE] = "CRC-16/CCITT-FALSE",
    [UFT_CRC_CCITT_TRUE]  = "CRC-16/CCITT",
    [UFT_CRC_IBM_ARC]     = "CRC-16/IBM-ARC",
    [UFT_CRC_MODBUS]      = "CRC-16/MODBUS",
    [UFT_CRC_XMODEM]      = "CRC-16/XMODEM",
    [UFT_CRC_MFM]         = "CRC-16/MFM",
    [UFT_CRC_AMIGA]       = "Amiga Checksum",
    [UFT_CRC_GCR_C64]     = "C64 GCR XOR",
    [UFT_CRC_GCR_APPLE]   = "Apple II GCR",
    [UFT_CRC_OMTI16]      = "OMTI CRC-16",
    [UFT_CRC32_ISO]       = "CRC-32/ISO",
    [UFT_CRC32_POSIX]     = "CRC-32/POSIX",
    [UFT_CRC32_CCSDS]     = "CRC-32/CCSDS",
    [UFT_CRC32_WD]        = "CRC-32/WD",
    [UFT_CRC32_SEAGATE]   = "CRC-32/Seagate",
    [UFT_CRC32_OMTI_HDR]  = "CRC-32/OMTI-HDR",
    [UFT_CRC32_OMTI_DATA] = "CRC-32/OMTI-DATA",
    [UFT_CRC_ECC_FIRE]    = "ECC/FIRE",
};

/*===========================================================================
 * Lookup Tables (pre-computed for CCITT)
 *===========================================================================*/

static const uint16_t g_crc16_ccitt_table[256] = {
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

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

static inline uint8_t reflect8(uint8_t val)
{
    val = (uint8_t)(((val & 0xF0) >> 4) | ((val & 0x0F) << 4));
    val = (uint8_t)(((val & 0xCC) >> 2) | ((val & 0x33) << 2));
    val = (uint8_t)(((val & 0xAA) >> 1) | ((val & 0x55) << 1));
    return val;
}

static inline uint16_t reflect16(uint16_t val)
{
    return ((uint16_t)reflect8((uint8_t)val) << 8) | 
            reflect8((uint8_t)(val >> 8));
}

static inline uint32_t reflect32(uint32_t val)
{
    return ((uint32_t)reflect16((uint16_t)val) << 16) | 
            reflect16((uint16_t)(val >> 16));
}

/*===========================================================================
 * Core CRC Calculation
 *===========================================================================*/

uint32_t uft_crc_init(uft_crc_type_t type)
{
    if (type >= UFT_CRC_TYPE_COUNT) return 0;
    return g_crc_params[type].init;
}

uint32_t uft_crc_update_byte(uft_crc_type_t type, uint32_t crc, uint8_t byte)
{
    if (type >= UFT_CRC_TYPE_COUNT) return crc;
    
    const uft_crc_params_t *p = &g_crc_params[type];
    
    if (p->reflect_in) {
        byte = reflect8(byte);
    }
    
    if (p->width == 16) {
        /* CRC-16 */
        if (type == UFT_CRC_CCITT_FALSE || type == UFT_CRC_MFM ||
            type == UFT_CRC_XMODEM || type == UFT_CRC_OMTI16) {
            /* Use lookup table for CCITT-type */
            crc = (crc << 8) ^ g_crc16_ccitt_table[((crc >> 8) ^ byte) & 0xFF];
        } else {
            /* Bit-by-bit for others */
            crc ^= ((uint32_t)byte << 8);
            for (int i = 0; i < 8; i++) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ p->poly;
                } else {
                    crc <<= 1;
                }
            }
        }
        crc &= 0xFFFF;
    } else if (p->width == 32) {
        /* CRC-32 */
        crc ^= ((uint32_t)byte << 24);
        for (int i = 0; i < 8; i++) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ p->poly;
            } else {
                crc <<= 1;
            }
        }
    } else if (p->width == 8) {
        /* Simple XOR (C64, Apple) */
        crc ^= byte;
    }
    
    return crc;
}

uint32_t uft_crc_update(uft_crc_type_t type, uint32_t crc, 
                        const void *data, size_t len)
{
    const uint8_t *ptr = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        crc = uft_crc_update_byte(type, crc, ptr[i]);
    }
    return crc;
}

uint32_t uft_crc_final(uft_crc_type_t type, uint32_t crc)
{
    if (type >= UFT_CRC_TYPE_COUNT) return crc;
    
    const uft_crc_params_t *p = &g_crc_params[type];
    
    if (p->reflect_out) {
        if (p->width == 16) {
            crc = reflect16((uint16_t)crc);
        } else if (p->width == 32) {
            crc = reflect32(crc);
        }
    }
    
    return crc ^ p->xor_out;
}

uint32_t uft_crc_calc(uft_crc_type_t type, const void *data, size_t len)
{
    uint32_t crc = uft_crc_init(type);
    crc = uft_crc_update(type, crc, data, len);
    return uft_crc_final(type, crc);
}

/*===========================================================================
 * Format-Specific Functions
 *===========================================================================*/

uint16_t uft_crc_mfm_sector(uint8_t address_mark, const void *data, size_t len)
{
    /* MFM CRC includes 3x 0xA1 sync bytes + address mark */
    uint32_t crc = 0xFFFF;
    
    /* Pre-calculate CRC for 0xA1 0xA1 0xA1 */
    crc = uft_crc_update_byte(UFT_CRC_CCITT_FALSE, crc, 0xA1);
    crc = uft_crc_update_byte(UFT_CRC_CCITT_FALSE, crc, 0xA1);
    crc = uft_crc_update_byte(UFT_CRC_CCITT_FALSE, crc, 0xA1);
    crc = uft_crc_update_byte(UFT_CRC_CCITT_FALSE, crc, address_mark);
    
    crc = uft_crc_update(UFT_CRC_CCITT_FALSE, crc, data, len);
    
    return (uint16_t)uft_crc_final(UFT_CRC_CCITT_FALSE, crc);
}

uint32_t uft_crc_amiga_checksum(const void *data, size_t len)
{
    const uint32_t *ptr = (const uint32_t *)data;
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < len / 4; i++) {
        checksum ^= ptr[i];
    }
    
    return checksum;
}

uint8_t uft_crc_gcr_c64(const void *data, size_t len)
{
    const uint8_t *ptr = (const uint8_t *)data;
    uint8_t checksum = 0;
    
    for (size_t i = 0; i < len; i++) {
        checksum ^= ptr[i];
    }
    
    return checksum;
}

uint8_t uft_crc_gcr_apple(const void *data, size_t len)
{
    /* Apple uses running XOR with rotation */
    const uint8_t *ptr = (const uint8_t *)data;
    uint8_t checksum = 0;
    
    for (size_t i = 0; i < len; i++) {
        checksum = (uint8_t)((checksum << 1) | (checksum >> 7));
        checksum ^= ptr[i];
    }
    
    return checksum;
}

uint16_t uft_crc_fm_sector(uint8_t address_mark, const void *data, size_t len)
{
    /* FM CRC starts with 0xFF 0xFF + clock marks (simplified) */
    uint32_t crc = 0xFFFF;
    
    crc = uft_crc_update_byte(UFT_CRC_CCITT_FALSE, crc, address_mark);
    crc = uft_crc_update(UFT_CRC_CCITT_FALSE, crc, data, len);
    
    return (uint16_t)uft_crc_final(UFT_CRC_CCITT_FALSE, crc);
}

/*===========================================================================
 * CRC Correction
 *===========================================================================*/

bool uft_crc_correct_1bit(uft_crc_type_t type, uint8_t *data, size_t len,
                          uint32_t stored_crc, size_t *bit_pos)
{
    if (!data || len == 0 || type >= UFT_CRC_TYPE_COUNT) return false;
    
    const uft_crc_params_t *p = &g_crc_params[type];
    uint32_t mask = (p->width == 16) ? 0xFFFF : 0xFFFFFFFF;
    
    /* Try flipping each bit */
    for (size_t byte_idx = 0; byte_idx < len; byte_idx++) {
        for (int bit = 0; bit < 8; bit++) {
            data[byte_idx] ^= (1 << bit);
            
            uint32_t calc_crc = uft_crc_calc(type, data, len) & mask;
            
            if (calc_crc == (stored_crc & mask)) {
                if (bit_pos) *bit_pos = byte_idx * 8 + bit;
                return true;
            }
            
            /* Undo flip */
            data[byte_idx] ^= (1 << bit);
        }
    }
    
    return false;
}

bool uft_crc_correct_2bit(uft_crc_type_t type, uint8_t *data, size_t len,
                          uint32_t stored_crc, size_t *pos1, size_t *pos2)
{
    if (!data || len == 0 || len > 256 || type >= UFT_CRC_TYPE_COUNT) {
        return false;
    }
    
    const uft_crc_params_t *p = &g_crc_params[type];
    uint32_t mask = (p->width == 16) ? 0xFFFF : 0xFFFFFFFF;
    
    size_t total_bits = len * 8;
    
    for (size_t b1 = 0; b1 < total_bits; b1++) {
        size_t byte1 = b1 / 8;
        int bit1 = b1 % 8;
        
        data[byte1] ^= (1 << bit1);
        
        for (size_t b2 = b1 + 1; b2 < total_bits; b2++) {
            size_t byte2 = b2 / 8;
            int bit2 = b2 % 8;
            
            data[byte2] ^= (1 << bit2);
            
            uint32_t calc_crc = uft_crc_calc(type, data, len) & mask;
            
            if (calc_crc == (stored_crc & mask)) {
                if (pos1) *pos1 = b1;
                if (pos2) *pos2 = b2;
                return true;
            }
            
            data[byte2] ^= (1 << bit2);
        }
        
        data[byte1] ^= (1 << bit1);
    }
    
    return false;
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

const char* uft_crc_type_name(uft_crc_type_t type)
{
    if (type >= UFT_CRC_TYPE_COUNT) return "Unknown";
    return g_crc_names[type] ? g_crc_names[type] : "Unknown";
}

const uft_crc_params_t* uft_crc_get_params(uft_crc_type_t type)
{
    if (type >= UFT_CRC_TYPE_COUNT) return NULL;
    return &g_crc_params[type];
}

uint32_t uft_crc_calc_custom(const uft_crc_params_t *params,
                             const void *data, size_t len)
{
    if (!params || !data) return 0;
    
    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t crc = params->init;
    
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = ptr[i];
        if (params->reflect_in) byte = reflect8(byte);
        
        if (params->width == 16) {
            crc ^= ((uint32_t)byte << 8);
            for (int j = 0; j < 8; j++) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ params->poly;
                } else {
                    crc <<= 1;
                }
            }
            crc &= 0xFFFF;
        } else if (params->width == 32) {
            crc ^= ((uint32_t)byte << 24);
            for (int j = 0; j < 8; j++) {
                if (crc & 0x80000000) {
                    crc = (crc << 1) ^ params->poly;
                } else {
                    crc <<= 1;
                }
            }
        }
    }
    
    if (params->reflect_out) {
        if (params->width == 16) {
            crc = reflect16((uint16_t)crc);
        } else if (params->width == 32) {
            crc = reflect32(crc);
        }
    }
    
    return crc ^ params->xor_out;
}

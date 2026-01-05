/**
 * @file uft_crc_reveng.h
 * @brief CRC Preset Database derived from CRC RevEng v3.0.6
 * 
 * Source: CRC RevEng by Gregory Cook
 * License: GPL-3.0
 * 
 * Contains 113 CRC presets covering all common CRC algorithms.
 * Particularly important for floppy disk format verification.
 */

#ifndef UFT_CRC_REVENG_H
#define UFT_CRC_REVENG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * CRC Model Definition
 *===========================================================================*/

/**
 * @brief CRC algorithm model
 * 
 * All parameters needed to compute a CRC according to the
 * Rocksoft^TM Model CRC Algorithm.
 */
typedef struct {
    const char *name;       /**< Algorithm name (e.g., "CRC-16/IBM-SDLC") */
    uint8_t width;          /**< CRC width in bits (3-64) */
    uint64_t poly;          /**< Polynomial (MSB-first, high term omitted) */
    uint64_t init;          /**< Initial register value */
    uint64_t xorout;        /**< Final XOR value */
    uint64_t check;         /**< Check value for "123456789" */
    uint64_t residue;       /**< Residue of valid codeword */
    bool refin;             /**< Reflect input bytes */
    bool refout;            /**< Reflect output CRC */
} uft_crc_model_t;

/*===========================================================================
 * Floppy-Critical CRC Presets
 *===========================================================================*/

/** CRC-16/IBM-SDLC (CRC-CCITT) - Standard floppy CRC */
#define UFT_CRC16_IBM_SDLC { \
    .name = "CRC-16/IBM-SDLC", \
    .width = 16, \
    .poly = 0x1021, \
    .init = 0xFFFF, \
    .xorout = 0xFFFF, \
    .check = 0x906E, \
    .residue = 0xF0B8, \
    .refin = true, \
    .refout = true \
}

/** CRC-16/XMODEM - Alternative floppy CRC */
#define UFT_CRC16_XMODEM { \
    .name = "CRC-16/XMODEM", \
    .width = 16, \
    .poly = 0x1021, \
    .init = 0x0000, \
    .xorout = 0x0000, \
    .check = 0x31C3, \
    .residue = 0x0000, \
    .refin = false, \
    .refout = false \
}

/** CRC-16/KERMIT - Another CCITT variant */
#define UFT_CRC16_KERMIT { \
    .name = "CRC-16/KERMIT", \
    .width = 16, \
    .poly = 0x1021, \
    .init = 0x0000, \
    .xorout = 0x0000, \
    .check = 0x2189, \
    .residue = 0x0000, \
    .refin = true, \
    .refout = true \
}

/** CRC-32/ISO-HDLC - Standard CRC-32 */
#define UFT_CRC32_ISO_HDLC { \
    .name = "CRC-32/ISO-HDLC", \
    .width = 32, \
    .poly = 0x04C11DB7, \
    .init = 0xFFFFFFFF, \
    .xorout = 0xFFFFFFFF, \
    .check = 0xCBF43926, \
    .residue = 0xDEBB20E3, \
    .refin = true, \
    .refout = true \
}

/*===========================================================================
 * Complete CRC Preset Catalog (113 algorithms)
 *===========================================================================*/

static const uft_crc_model_t uft_crc_presets[] = {
    /* CRC-3 */
    { "CRC-3/GSM", 3, 0x3, 0x0, 0x7, 0x4, 0x2, false, false },
    { "CRC-3/ROHC", 3, 0x3, 0x7, 0x0, 0x6, 0x0, true, true },
    
    /* CRC-4 */
    { "CRC-4/G-704", 4, 0x3, 0x0, 0x0, 0x7, 0x0, true, true },
    { "CRC-4/INTERLAKEN", 4, 0x3, 0xF, 0xF, 0xB, 0x2, false, false },
    
    /* CRC-5 */
    { "CRC-5/EPC-C1G2", 5, 0x09, 0x09, 0x00, 0x00, 0x00, false, false },
    { "CRC-5/G-704", 5, 0x15, 0x00, 0x00, 0x07, 0x00, true, true },
    { "CRC-5/USB", 5, 0x05, 0x1F, 0x1F, 0x19, 0x06, true, true },
    
    /* CRC-6 */
    { "CRC-6/CDMA2000-A", 6, 0x27, 0x3F, 0x00, 0x0D, 0x00, false, false },
    { "CRC-6/CDMA2000-B", 6, 0x07, 0x3F, 0x00, 0x3B, 0x00, false, false },
    { "CRC-6/DARC", 6, 0x19, 0x00, 0x00, 0x26, 0x00, true, true },
    { "CRC-6/G-704", 6, 0x03, 0x00, 0x00, 0x06, 0x00, true, true },
    { "CRC-6/GSM", 6, 0x2F, 0x00, 0x3F, 0x13, 0x3A, false, false },
    
    /* CRC-7 */
    { "CRC-7/MMC", 7, 0x09, 0x00, 0x00, 0x75, 0x00, false, false },
    { "CRC-7/ROHC", 7, 0x4F, 0x7F, 0x00, 0x53, 0x00, true, true },
    { "CRC-7/UMTS", 7, 0x45, 0x00, 0x00, 0x61, 0x00, false, false },
    
    /* CRC-8 */
    { "CRC-8/AUTOSAR", 8, 0x2F, 0xFF, 0xFF, 0xDF, 0x42, false, false },
    { "CRC-8/BLUETOOTH", 8, 0xA7, 0x00, 0x00, 0x26, 0x00, true, true },
    { "CRC-8/CDMA2000", 8, 0x9B, 0xFF, 0x00, 0xDA, 0x00, false, false },
    { "CRC-8/DARC", 8, 0x39, 0x00, 0x00, 0x15, 0x00, true, true },
    { "CRC-8/DVB-S2", 8, 0xD5, 0x00, 0x00, 0xBC, 0x00, false, false },
    { "CRC-8/GSM-A", 8, 0x1D, 0x00, 0x00, 0x37, 0x00, false, false },
    { "CRC-8/GSM-B", 8, 0x49, 0x00, 0xFF, 0x94, 0x53, false, false },
    { "CRC-8/HITAG", 8, 0x1D, 0xFF, 0x00, 0xB4, 0x00, false, false },
    { "CRC-8/I-432-1", 8, 0x07, 0x00, 0x55, 0xA1, 0xAC, false, false },
    { "CRC-8/I-CODE", 8, 0x1D, 0xFD, 0x00, 0x7E, 0x00, false, false },
    { "CRC-8/LTE", 8, 0x9B, 0x00, 0x00, 0xEA, 0x00, false, false },
    { "CRC-8/MAXIM-DOW", 8, 0x31, 0x00, 0x00, 0xA1, 0x00, true, true },
    { "CRC-8/MIFARE-MAD", 8, 0x1D, 0xC7, 0x00, 0x99, 0x00, false, false },
    { "CRC-8/NRSC-5", 8, 0x31, 0xFF, 0x00, 0xF7, 0x00, false, false },
    { "CRC-8/OPENSAFETY", 8, 0x2F, 0x00, 0x00, 0x3E, 0x00, false, false },
    { "CRC-8/ROHC", 8, 0x07, 0xFF, 0x00, 0xD0, 0x00, true, true },
    { "CRC-8/SAE-J1850", 8, 0x1D, 0xFF, 0xFF, 0x4B, 0xC4, false, false },
    { "CRC-8/SMBUS", 8, 0x07, 0x00, 0x00, 0xF4, 0x00, false, false },
    { "CRC-8/TECH-3250", 8, 0x1D, 0xFF, 0x00, 0x97, 0x00, true, true },
    { "CRC-8/WCDMA", 8, 0x9B, 0x00, 0x00, 0x25, 0x00, true, true },
    
    /* CRC-10 */
    { "CRC-10/ATM", 10, 0x233, 0x000, 0x000, 0x199, 0x000, false, false },
    { "CRC-10/CDMA2000", 10, 0x3D9, 0x3FF, 0x000, 0x233, 0x000, false, false },
    { "CRC-10/GSM", 10, 0x175, 0x000, 0x3FF, 0x12A, 0x0C6, false, false },
    
    /* CRC-11 */
    { "CRC-11/FLEXRAY", 11, 0x385, 0x01A, 0x000, 0x5A3, 0x000, false, false },
    { "CRC-11/UMTS", 11, 0x307, 0x000, 0x000, 0x061, 0x000, false, false },
    
    /* CRC-12 */
    { "CRC-12/CDMA2000", 12, 0xF13, 0xFFF, 0x000, 0xD4D, 0x000, false, false },
    { "CRC-12/DECT", 12, 0x80F, 0x000, 0x000, 0xF5B, 0x000, false, false },
    { "CRC-12/GSM", 12, 0xD31, 0x000, 0xFFF, 0xB34, 0x178, false, false },
    { "CRC-12/UMTS", 12, 0x80F, 0x000, 0x000, 0xDAF, 0x000, false, true },
    
    /* CRC-13 */
    { "CRC-13/BBC", 13, 0x1CF5, 0x0000, 0x0000, 0x04FA, 0x0000, false, false },
    
    /* CRC-14 */
    { "CRC-14/DARC", 14, 0x0805, 0x0000, 0x0000, 0x082D, 0x0000, true, true },
    { "CRC-14/GSM", 14, 0x202D, 0x0000, 0x3FFF, 0x30AE, 0x031E, false, false },
    
    /* CRC-15 */
    { "CRC-15/CAN", 15, 0x4599, 0x0000, 0x0000, 0x059E, 0x0000, false, false },
    { "CRC-15/MPT1327", 15, 0x6815, 0x0000, 0x0001, 0x2566, 0x6815, false, false },
    
    /* CRC-16 - Most important for floppy! */
    { "CRC-16/ARC", 16, 0x8005, 0x0000, 0x0000, 0xBB3D, 0x0000, true, true },
    { "CRC-16/CDMA2000", 16, 0xC867, 0xFFFF, 0x0000, 0x4C06, 0x0000, false, false },
    { "CRC-16/CMS", 16, 0x8005, 0xFFFF, 0x0000, 0xAEE7, 0x0000, false, false },
    { "CRC-16/DDS-110", 16, 0x8005, 0x800D, 0x0000, 0x9ECF, 0x0000, false, false },
    { "CRC-16/DECT-R", 16, 0x0589, 0x0000, 0x0001, 0x007E, 0x0589, false, false },
    { "CRC-16/DECT-X", 16, 0x0589, 0x0000, 0x0000, 0x007F, 0x0000, false, false },
    { "CRC-16/DNP", 16, 0x3D65, 0x0000, 0xFFFF, 0xEA82, 0x66C5, true, true },
    { "CRC-16/EN-13757", 16, 0x3D65, 0x0000, 0xFFFF, 0xC2B7, 0xA366, false, false },
    { "CRC-16/GENIBUS", 16, 0x1021, 0xFFFF, 0xFFFF, 0xD64E, 0x1D0F, false, false },
    { "CRC-16/GSM", 16, 0x1021, 0x0000, 0xFFFF, 0xCE3C, 0x1D0F, false, false },
    { "CRC-16/IBM-3740", 16, 0x1021, 0xFFFF, 0x0000, 0x29B1, 0x0000, false, false },
    { "CRC-16/IBM-SDLC", 16, 0x1021, 0xFFFF, 0xFFFF, 0x906E, 0xF0B8, true, true },
    { "CRC-16/ISO-IEC-14443-3-A", 16, 0x1021, 0x6363, 0x0000, 0xBF05, 0x0000, true, true },
    { "CRC-16/KERMIT", 16, 0x1021, 0x0000, 0x0000, 0x2189, 0x0000, true, true },
    { "CRC-16/LJ1200", 16, 0x6F63, 0x0000, 0x0000, 0xBDF4, 0x0000, false, false },
    { "CRC-16/M17", 16, 0x5935, 0xFFFF, 0x0000, 0x772B, 0x0000, false, false },
    { "CRC-16/MAXIM-DOW", 16, 0x8005, 0x0000, 0xFFFF, 0x44C2, 0xB001, true, true },
    { "CRC-16/MCRF4XX", 16, 0x1021, 0xFFFF, 0x0000, 0x6F91, 0x0000, true, true },
    { "CRC-16/MODBUS", 16, 0x8005, 0xFFFF, 0x0000, 0x4B37, 0x0000, true, true },
    { "CRC-16/NRSC-5", 16, 0x080B, 0xFFFF, 0x0000, 0xA066, 0x0000, true, true },
    { "CRC-16/OPENSAFETY-A", 16, 0x5935, 0x0000, 0x0000, 0x5D38, 0x0000, false, false },
    { "CRC-16/OPENSAFETY-B", 16, 0x755B, 0x0000, 0x0000, 0x20FE, 0x0000, false, false },
    { "CRC-16/PROFIBUS", 16, 0x1DCF, 0xFFFF, 0xFFFF, 0xA819, 0xE394, false, false },
    { "CRC-16/RIELLO", 16, 0x1021, 0xB2AA, 0x0000, 0x63D0, 0x0000, true, true },
    { "CRC-16/SPI-FUJITSU", 16, 0x1021, 0x1D0F, 0x0000, 0xE5CC, 0x0000, false, false },
    { "CRC-16/T10-DIF", 16, 0x8BB7, 0x0000, 0x0000, 0xD0DB, 0x0000, false, false },
    { "CRC-16/TELEDISK", 16, 0xA097, 0x0000, 0x0000, 0x0FB3, 0x0000, false, false },
    { "CRC-16/TMS37157", 16, 0x1021, 0x89EC, 0x0000, 0x26B1, 0x0000, true, true },
    { "CRC-16/UMTS", 16, 0x8005, 0x0000, 0x0000, 0xFEE8, 0x0000, false, false },
    { "CRC-16/USB", 16, 0x8005, 0xFFFF, 0xFFFF, 0xB4C8, 0xB001, true, true },
    { "CRC-16/XMODEM", 16, 0x1021, 0x0000, 0x0000, 0x31C3, 0x0000, false, false },
    
    /* CRC-17, CRC-21 */
    { "CRC-17/CAN-FD", 17, 0x1685B, 0x00000, 0x00000, 0x04F03, 0x00000, false, false },
    { "CRC-21/CAN-FD", 21, 0x102899, 0x000000, 0x000000, 0x0ED841, 0x000000, false, false },
    
    /* CRC-24 */
    { "CRC-24/BLE", 24, 0x00065B, 0x555555, 0x000000, 0xC25A56, 0x000000, true, true },
    { "CRC-24/FLEXRAY-A", 24, 0x5D6DCB, 0xFEDCBA, 0x000000, 0x7979BD, 0x000000, false, false },
    { "CRC-24/FLEXRAY-B", 24, 0x5D6DCB, 0xABCDEF, 0x000000, 0x1F23B8, 0x000000, false, false },
    { "CRC-24/INTERLAKEN", 24, 0x328B63, 0xFFFFFF, 0xFFFFFF, 0xB4F3E6, 0x144E63, false, false },
    { "CRC-24/LTE-A", 24, 0x864CFB, 0x000000, 0x000000, 0xCDE703, 0x000000, false, false },
    { "CRC-24/LTE-B", 24, 0x800063, 0x000000, 0x000000, 0x23EF52, 0x000000, false, false },
    { "CRC-24/OPENPGP", 24, 0x864CFB, 0xB704CE, 0x000000, 0x21CF02, 0x000000, false, false },
    { "CRC-24/OS-9", 24, 0x800063, 0xFFFFFF, 0xFFFFFF, 0x200FA5, 0x800FE3, false, false },
    
    /* CRC-30, CRC-31 */
    { "CRC-30/CDMA", 30, 0x2030B9C7, 0x3FFFFFFF, 0x3FFFFFFF, 0x04C34ABF, 0x34EFA55A, false, false },
    { "CRC-31/PHILIPS", 31, 0x04C11DB7, 0x7FFFFFFF, 0x7FFFFFFF, 0x0CE9E46C, 0x4EAF26F1, false, false },
    
    /* CRC-32 */
    { "CRC-32/AIXM", 32, 0x814141AB, 0x00000000, 0x00000000, 0x3010BF7F, 0x00000000, false, false },
    { "CRC-32/AUTOSAR", 32, 0xF4ACFB13, 0xFFFFFFFF, 0xFFFFFFFF, 0x1697D06A, 0x904CDDBF, true, true },
    { "CRC-32/BASE91-D", 32, 0xA833982B, 0xFFFFFFFF, 0xFFFFFFFF, 0x87315576, 0x45270551, true, true },
    { "CRC-32/BZIP2", 32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, 0xFC891918, 0xC704DD7B, false, false },
    { "CRC-32/CD-ROM-EDC", 32, 0x8001801B, 0x00000000, 0x00000000, 0x6EC2EDC4, 0x00000000, true, true },
    { "CRC-32/CKSUM", 32, 0x04C11DB7, 0x00000000, 0xFFFFFFFF, 0x765E7680, 0xC704DD7B, false, false },
    { "CRC-32/ISCSI", 32, 0x1EDC6F41, 0xFFFFFFFF, 0xFFFFFFFF, 0xE3069283, 0xB798B438, true, true },
    { "CRC-32/ISO-HDLC", 32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, 0xCBF43926, 0xDEBB20E3, true, true },
    { "CRC-32/JAMCRC", 32, 0x04C11DB7, 0xFFFFFFFF, 0x00000000, 0x340BC6D9, 0x00000000, true, true },
    { "CRC-32/MEF", 32, 0x741B8CD7, 0xFFFFFFFF, 0x00000000, 0xD2C22F51, 0x00000000, true, true },
    { "CRC-32/MPEG-2", 32, 0x04C11DB7, 0xFFFFFFFF, 0x00000000, 0x0376E6E7, 0x00000000, false, false },
    { "CRC-32/XFER", 32, 0x000000AF, 0x00000000, 0x00000000, 0xBD0BE338, 0x00000000, false, false },
    
    /* CRC-40 */
    { "CRC-40/GSM", 40, 0x0004820009ULL, 0x0000000000ULL, 0xFFFFFFFFFFULL, 0xD4164FC646ULL, 0xC4FF8071FFULL, false, false },
    
    /* CRC-64 */
    { "CRC-64/ECMA-182", 64, 0x42F0E1EBA9EA3693ULL, 0x0ULL, 0x0ULL, 0x6C40DF5F0B497347ULL, 0x0ULL, false, false },
    { "CRC-64/GO-ISO", 64, 0x000000000000001BULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0xB90956C775A41001ULL, 0x5300000000000000ULL, true, true },
    { "CRC-64/MS", 64, 0x259C84CBA6426349ULL, 0xFFFFFFFFFFFFFFFFULL, 0x0ULL, 0x75D4B74F024ECEEAULL, 0x0ULL, true, true },
    { "CRC-64/REDIS", 64, 0xAD93D23594C935A9ULL, 0x0ULL, 0x0ULL, 0xE9C6D914C4B8D9CAULL, 0x0ULL, true, true },
    { "CRC-64/WE", 64, 0x42F0E1EBA9EA3693ULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0x62EC59E3F1A4F00AULL, 0xFCACBEBD5931A992ULL, false, false },
    { "CRC-64/XZ", 64, 0x42F0E1EBA9EA3693ULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0x995DC9BBDF1939FAULL, 0x49958C9ABD7D353FULL, true, true },
    
    /* Terminator */
    { NULL, 0, 0, 0, 0, 0, 0, false, false }
};

#define UFT_CRC_PRESET_COUNT (sizeof(uft_crc_presets) / sizeof(uft_crc_presets[0]) - 1)

/*===========================================================================
 * CRC Computation Functions
 *===========================================================================*/

/**
 * @brief Reflect/reverse bits in a value
 * @param data Value to reflect
 * @param width Number of bits to reflect
 * @return Reflected value
 */
static inline uint64_t uft_crc_reflect(uint64_t data, uint8_t width) {
    uint64_t result = 0;
    for (uint8_t i = 0; i < width; i++) {
        if (data & 1) {
            result |= (1ULL << (width - 1 - i));
        }
        data >>= 1;
    }
    return result;
}

/**
 * @brief Compute CRC using specified model
 * @param model CRC model to use
 * @param data Data buffer
 * @param len Data length in bytes
 * @return Computed CRC value
 */
static inline uint64_t uft_crc_compute(const uft_crc_model_t *model,
                                       const uint8_t *data, size_t len) {
    uint64_t crc = model->init;
    uint64_t top_bit = 1ULL << (model->width - 1);
    uint64_t mask = (model->width < 64) ? ((1ULL << model->width) - 1) : ~0ULL;
    
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        if (model->refin) {
            byte = (uint8_t)uft_crc_reflect(byte, 8);
        }
        
        crc ^= ((uint64_t)byte << (model->width - 8));
        
        for (int bit = 0; bit < 8; bit++) {
            if (crc & top_bit) {
                crc = (crc << 1) ^ model->poly;
            } else {
                crc <<= 1;
            }
        }
    }
    
    if (model->refout) {
        crc = uft_crc_reflect(crc, model->width);
    }
    
    return (crc ^ model->xorout) & mask;
}

/**
 * @brief Find CRC model by name
 * @param name Algorithm name (case-insensitive partial match)
 * @return Pointer to model or NULL if not found
 */
const uft_crc_model_t *uft_crc_find_model(const char *name);

/**
 * @brief Verify CRC model using check value
 * @param model Model to verify
 * @return true if model passes check, false otherwise
 */
static inline bool uft_crc_verify_model(const uft_crc_model_t *model) {
    const uint8_t check_data[] = "123456789";
    uint64_t result = uft_crc_compute(model, check_data, 9);
    return result == model->check;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_REVENG_H */

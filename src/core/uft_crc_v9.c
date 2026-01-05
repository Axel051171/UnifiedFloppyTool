/**
 * @file uft_crc.c
 * @brief CRC computation implementation
 * 
 * Based on CRC RevEng by Gregory Cook
 * Optimized for UFT floppy disk verification
 */

#include "uft_crc_reveng.h"
#include <string.h>
#include <ctype.h>

/*===========================================================================
 * Lookup Table Generation
 *===========================================================================*/

/**
 * @brief Generate CRC-16 lookup table
 */
void uft_crc16_init_table(uint16_t *table, uint16_t poly, bool reflect) {
    for (int i = 0; i < 256; i++) {
        uint16_t crc;
        if (reflect) {
            crc = i;
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ ((crc & 1) ? uft_crc_reflect(poly, 16) : 0);
            }
        } else {
            crc = (uint16_t)(i << 8);
            for (int j = 0; j < 8; j++) {
                crc = (crc << 1) ^ ((crc & 0x8000) ? poly : 0);
            }
        }
        table[i] = crc;
    }
}

/**
 * @brief Generate CRC-32 lookup table
 */
void uft_crc32_init_table(uint32_t *table, uint32_t poly, bool reflect) {
    for (int i = 0; i < 256; i++) {
        uint32_t crc;
        if (reflect) {
            crc = i;
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ ((crc & 1) ? uft_crc_reflect(poly, 32) : 0);
            }
        } else {
            crc = (uint32_t)(i << 24);
            for (int j = 0; j < 8; j++) {
                crc = (crc << 1) ^ ((crc & 0x80000000) ? poly : 0);
            }
        }
        table[i] = crc;
    }
}

/*===========================================================================
 * Pre-computed Tables for Common CRCs
 *===========================================================================*/

/** CRC-16/IBM-SDLC (CCITT) table - reflected */
static uint16_t crc16_sdlc_table[256];
static bool crc16_sdlc_initialized = false;

/** CRC-32/ISO-HDLC table - reflected */
static uint32_t crc32_table[256];
static bool crc32_initialized = false;

/*===========================================================================
 * Fast CRC Functions for Floppy
 *===========================================================================*/

/**
 * @brief Fast CRC-16/IBM-SDLC (standard floppy CRC)
 * 
 * This is the most common CRC used in MFM floppy formats.
 * Poly: 0x1021, Init: 0xFFFF, RefIn/Out: true, XorOut: 0xFFFF
 */
uint16_t uft_crc16_ibm_sdlc(const uint8_t *data, size_t len) {
    if (!crc16_sdlc_initialized) {
        uft_crc16_init_table(crc16_sdlc_table, 0x1021, true);
        crc16_sdlc_initialized = true;
    }
    
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc = (crc >> 8) ^ crc16_sdlc_table[(crc ^ *data++) & 0xFF];
    }
    return crc ^ 0xFFFF;
}

/**
 * @brief CRC-16/IBM-SDLC with custom init value
 */
uint16_t uft_crc16_ibm_sdlc_init(const uint8_t *data, size_t len, uint16_t init) {
    if (!crc16_sdlc_initialized) {
        uft_crc16_init_table(crc16_sdlc_table, 0x1021, true);
        crc16_sdlc_initialized = true;
    }
    
    uint16_t crc = init;
    while (len--) {
        crc = (crc >> 8) ^ crc16_sdlc_table[(crc ^ *data++) & 0xFF];
    }
    return crc ^ 0xFFFF;
}

/**
 * @brief Fast CRC-16/XMODEM (alternative floppy CRC)
 * 
 * Poly: 0x1021, Init: 0x0000, RefIn/Out: false, XorOut: 0x0000
 */
uint16_t uft_crc16_xmodem(const uint8_t *data, size_t len) {
    uint16_t crc = 0x0000;
    
    while (len--) {
        crc ^= ((uint16_t)*data++) << 8;
        for (int i = 0; i < 8; i++) {
            crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);
        }
    }
    return crc;
}

/**
 * @brief Fast CRC-32/ISO-HDLC (standard CRC-32)
 */
uint32_t uft_crc32_iso_hdlc(const uint8_t *data, size_t len) {
    if (!crc32_initialized) {
        uft_crc32_init_table(crc32_table, 0x04C11DB7, true);
        crc32_initialized = true;
    }
    
    uint32_t crc = 0xFFFFFFFF;
    while (len--) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ *data++) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

/*===========================================================================
 * MFM Sector CRC Verification
 *===========================================================================*/

/**
 * @brief Verify MFM sector header CRC
 * 
 * Standard MFM header is 4 bytes: C H R N
 * CRC is initialized with 0xFFFF after sync bytes
 */
bool uft_verify_mfm_header_crc(uint8_t cyl, uint8_t head, uint8_t sector, 
                               uint8_t size_code, uint16_t crc) {
    uint8_t header[4] = { cyl, head, sector, size_code };
    uint16_t calc = uft_crc16_ibm_sdlc(header, 4);
    return calc == crc;
}

/**
 * @brief Verify MFM sector data CRC
 * 
 * Data CRC covers sector_size bytes after data address mark
 */
bool uft_verify_mfm_data_crc(const uint8_t *data, size_t len, uint16_t crc) {
    uint16_t calc = uft_crc16_ibm_sdlc(data, len);
    return calc == crc;
}

/**
 * @brief Calculate MFM sector CRC with address marks included
 * 
 * Full CRC calculation including address mark bytes.
 * A1 A1 A1 FE C H R N [CRC-16]  - Header
 * A1 A1 A1 FB data... [CRC-16]  - Data
 */
uint16_t uft_calc_mfm_crc_with_mark(uint8_t mark, const uint8_t *data, size_t len) {
    /* CRC starts after A1 sync bytes with pre-calculated init */
    /* After 3x A1 (0xA1 0xA1 0xA1), CRC = 0xCDB4 */
    static const uint16_t crc_after_sync = 0xCDB4;
    
    /* Include address mark in CRC */
    uint16_t crc = uft_crc16_ibm_sdlc_init(&mark, 1, crc_after_sync);
    
    /* Continue with data (but XorOut already applied, so adjust) */
    if (!crc16_sdlc_initialized) {
        uft_crc16_init_table(crc16_sdlc_table, 0x1021, true);
        crc16_sdlc_initialized = true;
    }
    
    crc ^= 0xFFFF;  /* Remove XorOut to continue CRC */
    while (len--) {
        crc = (crc >> 8) ^ crc16_sdlc_table[(crc ^ *data++) & 0xFF];
    }
    return crc ^ 0xFFFF;
}

/*===========================================================================
 * Model Lookup
 *===========================================================================*/

/**
 * @brief Find CRC model by name (case-insensitive)
 */
const uft_crc_model_t *uft_crc_find_model(const char *name) {
    for (size_t i = 0; i < UFT_CRC_PRESET_COUNT; i++) {
        const char *p = uft_crc_presets[i].name;
        const char *q = name;
        
        /* Case-insensitive compare */
        while (*p && *q) {
            if (toupper((unsigned char)*p) != toupper((unsigned char)*q)) {
                break;
            }
            p++;
            q++;
        }
        
        if (*p == '\0' && *q == '\0') {
            return &uft_crc_presets[i];
        }
    }
    return NULL;
}

/**
 * @brief Find CRC model by parameters
 */
const uft_crc_model_t *uft_crc_find_by_params(uint8_t width, uint64_t poly,
                                              bool refin, bool refout) {
    for (size_t i = 0; i < UFT_CRC_PRESET_COUNT; i++) {
        const uft_crc_model_t *m = &uft_crc_presets[i];
        if (m->width == width && m->poly == poly &&
            m->refin == refin && m->refout == refout) {
            return m;
        }
    }
    return NULL;
}

/*===========================================================================
 * CRC Correction (1-2 bit errors)
 *===========================================================================*/

/**
 * @brief Attempt to correct single-bit error in data
 * 
 * For a CRC with known correct value, finds the bit position
 * that would make the CRC valid.
 * 
 * @param model CRC model
 * @param data Data buffer (will be modified if correction found)
 * @param len Data length
 * @param expected_crc Expected CRC value
 * @return Bit position corrected (0 = first bit), or -1 if no single-bit error
 */
int uft_crc_correct_single_bit(const uft_crc_model_t *model,
                               uint8_t *data, size_t len,
                               uint64_t expected_crc) {
    uint64_t orig_crc = uft_crc_compute(model, data, len);
    
    if (orig_crc == expected_crc) {
        return -2;  /* Already correct */
    }
    
    /* Try flipping each bit */
    for (size_t byte = 0; byte < len; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            data[byte] ^= (1 << bit);
            uint64_t test_crc = uft_crc_compute(model, data, len);
            if (test_crc == expected_crc) {
                return (int)(byte * 8 + (7 - bit));
            }
            data[byte] ^= (1 << bit);  /* Restore */
        }
    }
    
    return -1;  /* No single-bit error found */
}

/*===========================================================================
 * Self-Test
 *===========================================================================*/

/**
 * @brief Verify all CRC presets using check values
 * @return Number of failed presets (0 = all pass)
 */
int uft_crc_self_test(void) {
    int failures = 0;
    const uint8_t check_data[] = "123456789";
    
    for (size_t i = 0; i < UFT_CRC_PRESET_COUNT; i++) {
        const uft_crc_model_t *m = &uft_crc_presets[i];
        uint64_t result = uft_crc_compute(m, check_data, 9);
        if (result != m->check) {
            failures++;
        }
    }
    
    return failures;
}

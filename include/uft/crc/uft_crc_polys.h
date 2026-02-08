/**
 * @file uft_crc_polys.h
 * @brief CRC/ECC Polynomial Database for Floppy/Hard Drive Formats
 * 
 * Comprehensive collection of CRC and ECC polynomials used by various
 * floppy disk and hard drive controllers.
 * 
 * Sources:
 * - raszpl/sigrok-disk
 * - dgesswein/mfm
 * - Various controller datasheets
 * 
 * @copyright GPL-3.0
 */

#ifndef UFT_CRC_POLYS_H
#define UFT_CRC_POLYS_H

#include <stdint.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CRC-16 Polynomials
 * ============================================================================ */

/**
 * CRC-CCITT (x16 + x12 + x5 + 1)
 * Used by: IBM PC floppy, most standard formats
 * Init: 0xFFFF (standard) or 0x0000 (some formats)
 */
#define UFT_CRC16_CCITT_POLY    0x1021

/**
 * OMTI 8247 Header CRC-16
 * Polynomial: 0x1021 (same as CCITT)
 * Init: 0x7107 (non-standard)
 */
#define UFT_CRC16_OMTI_POLY     0x1021
#define UFT_CRC16_OMTI_INIT     0x7107

/* ============================================================================
 * CRC-32 Polynomials
 * ============================================================================ */

/**
 * CRC32-CCSDS (x32 + x23 + x21 + x11 + x2 + 1)
 * Used by: DEC VAX (RQDX3, HDC9224), CCSDS space protocols
 * Init: 0xFFFFFFFF
 */
#define UFT_CRC32_CCSDS_POLY    0x00A00805UL
#define UFT_CRC32_CCSDS_INIT    0xFFFFFFFFUL

/**
 * Western Digital CRC-32 (X32 + X28 + X26 + X19 + X17 + X10 + X6 + X2 + 1)
 * Used by: WD1003, WD1006, WD1100
 * Init: 0xFFFFFFFF
 */
#define UFT_CRC32_WD_POLY       0x140A0445UL
#define UFT_CRC32_WD_INIT       0xFFFFFFFFUL

/**
 * Seagate CRC-32 (x32 + x30 + x24 + x18 + x14 + x8 + x7 + x2 + 1)
 * Used by: Seagate ST11, ST21
 * Init: 0x00000000
 */
#define UFT_CRC32_SEAGATE_POLY  0x41044185UL
#define UFT_CRC32_SEAGATE_INIT  0x00000000UL

/**
 * OMTI 8240/5510 Header CRC-32 (x32 + x24 + x18 + x15 + x14 + x11 + x8 + x7 + 1)
 * Init: 0x2605FB9C (non-standard)
 */
#define UFT_CRC32_OMTI_HDR_POLY 0x0104C981UL
#define UFT_CRC32_OMTI_HDR_INIT 0x2605FB9CUL

/**
 * OMTI 8240/5510 Data CRC-32
 * Same polynomial, different init
 * Init: 0xD4D7CA20
 */
#define UFT_CRC32_OMTI_DAT_POLY 0x0104C981UL
#define UFT_CRC32_OMTI_DAT_INIT 0xD4D7CA20UL

/* ============================================================================
 * ECC-48 Polynomials
 * ============================================================================ */

/**
 * OMTI 8247 ECC-48
 * Init: 0x6062EBBF22B4 (non-standard)
 */
#define UFT_ECC48_OMTI_POLY     0x181814503011ULL
#define UFT_ECC48_OMTI_INIT     0x6062EBBF22B4ULL

/**
 * Adaptec ECC-48
 * Init: 0x010000000000
 */
#define UFT_ECC48_ADAPTEC_POLY  0x181814503011ULL
#define UFT_ECC48_ADAPTEC_INIT  0x010000000000ULL

/* ============================================================================
 * ECC-56 Polynomials
 * ============================================================================ */

/**
 * Western Digital ECC-56 (WD40C22)
 * X56 + X52 + X50 + X43 + X41 + X34 + X30 + X26 + X24 + X8 + 1
 */
#define UFT_ECC56_WD_POLY       0x140A0445000101ULL
#define UFT_ECC56_WD_INIT       0xFFFFFFFFFFFFFFULL

/* ============================================================================
 * Controller-Specific Presets
 * ============================================================================ */

/**
 * @brief CRC/ECC configuration structure
 */
typedef struct {
    const char *name;           /**< Controller name */
    uint64_t header_poly;       /**< Header CRC polynomial */
    uint64_t header_init;       /**< Header CRC initial value */
    int header_bits;            /**< Header CRC size (16 or 32) */
    uint64_t data_poly;         /**< Data CRC/ECC polynomial */
    uint64_t data_init;         /**< Data CRC/ECC initial value */
    int data_bits;              /**< Data CRC/ECC size (16, 32, 48, 56) */
} uft_crc_preset_t;

/**
 * Preset configurations for known controllers
 */
static const uft_crc_preset_t UFT_CRC_PRESETS[] = {
    /* Standard floppy */
    {
        .name = "IBM PC Floppy",
        .header_poly = UFT_CRC16_CCITT_POLY,
        .header_init = 0xFFFF,
        .header_bits = 16,
        .data_poly = UFT_CRC16_CCITT_POLY,
        .data_init = 0xFFFF,
        .data_bits = 16
    },
    
    /* DEC VAX */
    {
        .name = "DEC RQDX3 (HDC9224)",
        .header_poly = UFT_CRC16_CCITT_POLY,
        .header_init = 0xFFFF,
        .header_bits = 16,
        .data_poly = UFT_CRC32_CCSDS_POLY,
        .data_init = UFT_CRC32_CCSDS_INIT,
        .data_bits = 32
    },
    
    /* Western Digital MFM */
    {
        .name = "WD1003/1006 MFM",
        .header_poly = UFT_CRC16_CCITT_POLY,
        .header_init = 0xFFFF,
        .header_bits = 16,
        .data_poly = UFT_CRC32_WD_POLY,
        .data_init = UFT_CRC32_WD_INIT,
        .data_bits = 32
    },
    
    /* Western Digital RLL */
    {
        .name = "WD1003/1006 RLL",
        .header_poly = UFT_CRC16_CCITT_POLY,
        .header_init = 0xFFFF,
        .header_bits = 16,
        .data_poly = UFT_ECC56_WD_POLY,
        .data_init = UFT_ECC56_WD_INIT,
        .data_bits = 56
    },
    
    /* Seagate */
    {
        .name = "Seagate ST11/21",
        .header_poly = UFT_CRC32_SEAGATE_POLY,
        .header_init = UFT_CRC32_SEAGATE_INIT,
        .header_bits = 32,
        .data_poly = UFT_CRC32_SEAGATE_POLY,
        .data_init = UFT_CRC32_SEAGATE_INIT,
        .data_bits = 32
    },
    
    /* OMTI MFM */
    {
        .name = "OMTI 8240/5510 MFM",
        .header_poly = UFT_CRC32_OMTI_HDR_POLY,
        .header_init = UFT_CRC32_OMTI_HDR_INIT,
        .header_bits = 32,
        .data_poly = UFT_CRC32_OMTI_DAT_POLY,
        .data_init = UFT_CRC32_OMTI_DAT_INIT,
        .data_bits = 32
    },
    
    /* OMTI RLL */
    {
        .name = "OMTI 8247 RLL",
        .header_poly = UFT_CRC16_OMTI_POLY,
        .header_init = UFT_CRC16_OMTI_INIT,
        .header_bits = 16,
        .data_poly = UFT_ECC48_OMTI_POLY,
        .data_init = UFT_ECC48_OMTI_INIT,
        .data_bits = 48
    },
    
    /* Adaptec RLL */
    {
        .name = "Adaptec ACB-2370/2372",
        .header_poly = UFT_CRC16_CCITT_POLY,
        .header_init = 0x0000,
        .header_bits = 16,
        .data_poly = UFT_ECC48_ADAPTEC_POLY,
        .data_init = UFT_ECC48_ADAPTEC_INIT,
        .data_bits = 48
    },
    
    /* Sentinel */
    { .name = NULL }
};

/* ============================================================================
 * Polynomial Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert polynomial notation to hex
 * 
 * Example: x32 + x23 + x21 + x11 + x2 + 1
 *          = 0b10000000101000000000100000000101
 *          Drop MSB = 0xA00805
 * 
 * @param exponents Array of exponents (highest to lowest)
 * @param count Number of exponents
 * @return Polynomial in hex format (MSB dropped)
 */
static inline uint64_t uft_poly_from_exponents(const int *exponents, int count) {
    uint64_t poly = 0;
    for (int i = 0; i < count; i++) {
        poly |= (1ULL << exponents[i]);
    }
    /* Drop MSB (the highest exponent represents the polynomial degree) */
    int highest = exponents[0];
    poly &= ~(1ULL << highest);
    return poly;
}

/**
 * @brief Convert hex polynomial to string notation
 * @param poly Polynomial value
 * @param bits Polynomial degree
 * @param buffer Output buffer
 * @param size Buffer size
 */
void uft_poly_to_string(uint64_t poly, int bits, char *buffer, size_t size);

/**
 * @brief Find CRC preset by controller name
 * @param name Controller name (case-insensitive partial match)
 * @return Preset pointer or NULL if not found
 */
const uft_crc_preset_t *uft_crc_find_preset(const char *name);

/**
 * @brief List all available CRC presets
 * @param callback Function called for each preset
 * @param user_data User data passed to callback
 */
void uft_crc_list_presets(void (*callback)(const uft_crc_preset_t *, void *),
                          void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_POLYS_H */

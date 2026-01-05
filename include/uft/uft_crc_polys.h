/**
 * @file uft_crc_polys.h
 * @brief CRC Polynomial Database for Storage Devices
 * 
 * @version 4.2.0
 * @date 2026-01-03
 * 
 * COVERAGE:
 * - Floppy disk CRCs (IBM, Amiga, Commodore, Apple)
 * - Hard disk CRCs (OMTI, Seagate, Western Digital, Adaptec)
 * - Tape drive CRCs (QIC, DAT)
 * - Optical media CRCs (CD, DVD)
 * - Network/Protocol CRCs (Ethernet, CAN, USB)
 * 
 * USAGE:
 * ```c
 * // Get CRC configuration
 * const uft_crc_config_t *cfg = uft_crc_get_config(UFT_CRC_IBM_MFM);
 * 
 * // Calculate CRC
 * uint32_t crc = uft_crc_calc(cfg, data, length);
 * 
 * // Or use convenience function
 * uint16_t crc = uft_crc_ibm_mfm(data, length);
 * ```
 */

#ifndef UFT_CRC_POLYS_H
#define UFT_CRC_POLYS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    /* === Floppy Disk CRCs === */
    UFT_CRC_IBM_MFM,            /* IBM PC MFM: CRC-16-CCITT */
    UFT_CRC_IBM_FM,             /* IBM PC FM: CRC-16-CCITT */
    UFT_CRC_AMIGA_MFM,          /* Amiga MFM: Custom checksum */
    UFT_CRC_COMMODORE_GCR,      /* C64 GCR: XOR checksum */
    UFT_CRC_APPLE_GCR,          /* Apple II GCR: Custom */
    UFT_CRC_ATARI_FM,           /* Atari 810: CRC-16 */
    UFT_CRC_BBC_FM,             /* BBC Micro FM: CRC-16 */
    
    /* === Hard Disk CRCs === */
    UFT_CRC_OMTI,               /* OMTI controllers */
    UFT_CRC_OMTI_5100,          /* OMTI 5100 series */
    UFT_CRC_SEAGATE_ST506,      /* Seagate ST-506 */
    UFT_CRC_SEAGATE_ESDI,       /* Seagate ESDI */
    UFT_CRC_WD1003,             /* Western Digital WD1003 */
    UFT_CRC_WD1006,             /* Western Digital WD1006 */
    UFT_CRC_ADAPTEC,            /* Adaptec RLL */
    UFT_CRC_XEBEC,              /* Xebec controllers */
    UFT_CRC_DTC,                /* Data Technology Corp */
    UFT_CRC_SCSI,               /* SCSI disk */
    UFT_CRC_IDE_ATA,            /* IDE/ATA */
    
    /* === Tape Drive CRCs === */
    UFT_CRC_QIC40,              /* QIC-40 tape */
    UFT_CRC_QIC80,              /* QIC-80 tape */
    UFT_CRC_QIC3010,            /* QIC-3010 tape */
    UFT_CRC_DAT_DDS,            /* DAT DDS */
    UFT_CRC_LTO,                /* LTO tape */
    UFT_CRC_8MM,                /* 8mm Exabyte */
    
    /* === Optical Media === */
    UFT_CRC_CD_ROM,             /* CD-ROM EDC */
    UFT_CRC_CD_ECC,             /* CD-ROM ECC */
    UFT_CRC_DVD,                /* DVD */
    UFT_CRC_BD,                 /* Blu-ray */
    
    /* === Network/Protocol === */
    UFT_CRC_ETHERNET,           /* Ethernet CRC-32 */
    UFT_CRC_CAN,                /* CAN bus */
    UFT_CRC_USB,                /* USB */
    UFT_CRC_HDLC,               /* HDLC */
    UFT_CRC_MODBUS,             /* Modbus */
    
    /* === Standard Polynomials === */
    UFT_CRC_8,                  /* CRC-8 */
    UFT_CRC_8_DALLAS,           /* CRC-8 Dallas/Maxim */
    UFT_CRC_16,                 /* CRC-16 IBM */
    UFT_CRC_16_CCITT,           /* CRC-16 CCITT */
    UFT_CRC_16_XMODEM,          /* CRC-16 XMODEM */
    UFT_CRC_16_MODBUS,          /* CRC-16 Modbus */
    UFT_CRC_32,                 /* CRC-32 */
    UFT_CRC_32C,                /* CRC-32C (Castagnoli) */
    UFT_CRC_64_ECMA,            /* CRC-64 ECMA */
    UFT_CRC_64_ISO,             /* CRC-64 ISO */
    
    UFT_CRC_TYPE_COUNT
} uft_crc_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC Configuration
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *name;           /* Human-readable name */
    uft_crc_type_t type;        /* CRC type enum */
    
    uint8_t width;              /* CRC width in bits (8, 16, 32, 64) */
    uint64_t polynomial;        /* Generator polynomial */
    uint64_t init;              /* Initial value */
    uint64_t xor_out;           /* Final XOR value */
    
    bool reflect_in;            /* Reflect input bytes */
    bool reflect_out;           /* Reflect output */
    
    const char *description;    /* Usage description */
} uft_crc_config_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Predefined CRC Configurations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Floppy Disk CRCs */
static const uft_crc_config_t UFT_CRC_CONFIG_IBM_MFM = {
    .name = "IBM MFM",
    .type = UFT_CRC_IBM_MFM,
    .width = 16,
    .polynomial = 0x1021,       /* x^16 + x^12 + x^5 + 1 */
    .init = 0xFFFF,
    .xor_out = 0x0000,
    .reflect_in = false,
    .reflect_out = false,
    .description = "IBM PC MFM floppy sector CRC"
};

static const uft_crc_config_t UFT_CRC_CONFIG_IBM_FM = {
    .name = "IBM FM",
    .type = UFT_CRC_IBM_FM,
    .width = 16,
    .polynomial = 0x1021,
    .init = 0xFFFF,
    .xor_out = 0x0000,
    .reflect_in = false,
    .reflect_out = false,
    .description = "IBM PC FM floppy sector CRC"
};

/* Hard Disk CRCs */
static const uft_crc_config_t UFT_CRC_CONFIG_OMTI = {
    .name = "OMTI",
    .type = UFT_CRC_OMTI,
    .width = 32,
    .polynomial = 0x140A0445,   /* OMTI proprietary */
    .init = 0xFFFFFFFF,
    .xor_out = 0x00000000,
    .reflect_in = false,
    .reflect_out = false,
    .description = "OMTI hard disk controller CRC"
};

static const uft_crc_config_t UFT_CRC_CONFIG_SEAGATE_ST506 = {
    .name = "Seagate ST-506",
    .type = UFT_CRC_SEAGATE_ST506,
    .width = 16,
    .polynomial = 0x8005,       /* CRC-16 */
    .init = 0x0000,
    .xor_out = 0x0000,
    .reflect_in = true,
    .reflect_out = true,
    .description = "Seagate ST-506/412 MFM hard disk"
};

static const uft_crc_config_t UFT_CRC_CONFIG_WD1003 = {
    .name = "WD1003",
    .type = UFT_CRC_WD1003,
    .width = 16,
    .polynomial = 0x8005,
    .init = 0xFFFF,
    .xor_out = 0x0000,
    .reflect_in = true,
    .reflect_out = true,
    .description = "Western Digital WD1003 controller"
};

static const uft_crc_config_t UFT_CRC_CONFIG_ADAPTEC = {
    .name = "Adaptec RLL",
    .type = UFT_CRC_ADAPTEC,
    .width = 32,
    .polynomial = 0x04C11DB7,   /* CRC-32 */
    .init = 0xFFFFFFFF,
    .xor_out = 0xFFFFFFFF,
    .reflect_in = true,
    .reflect_out = true,
    .description = "Adaptec RLL hard disk controller"
};

/* Tape Drive CRCs */
static const uft_crc_config_t UFT_CRC_CONFIG_QIC80 = {
    .name = "QIC-80",
    .type = UFT_CRC_QIC80,
    .width = 16,
    .polynomial = 0x8005,
    .init = 0x0000,
    .xor_out = 0x0000,
    .reflect_in = true,
    .reflect_out = true,
    .description = "QIC-80 tape block CRC"
};

/* Optical Media */
static const uft_crc_config_t UFT_CRC_CONFIG_CD_ROM = {
    .name = "CD-ROM EDC",
    .type = UFT_CRC_CD_ROM,
    .width = 32,
    .polynomial = 0x8001801B,   /* CD-ROM EDC polynomial */
    .init = 0x00000000,
    .xor_out = 0x00000000,
    .reflect_in = true,
    .reflect_out = true,
    .description = "CD-ROM Error Detection Code"
};

/* Standard CRCs */
static const uft_crc_config_t UFT_CRC_CONFIG_16_CCITT = {
    .name = "CRC-16-CCITT",
    .type = UFT_CRC_16_CCITT,
    .width = 16,
    .polynomial = 0x1021,
    .init = 0xFFFF,
    .xor_out = 0x0000,
    .reflect_in = false,
    .reflect_out = false,
    .description = "CRC-16-CCITT (X.25, HDLC)"
};

static const uft_crc_config_t UFT_CRC_CONFIG_32 = {
    .name = "CRC-32",
    .type = UFT_CRC_32,
    .width = 32,
    .polynomial = 0x04C11DB7,
    .init = 0xFFFFFFFF,
    .xor_out = 0xFFFFFFFF,
    .reflect_in = true,
    .reflect_out = true,
    .description = "CRC-32 (Ethernet, ZIP, PNG)"
};

static const uft_crc_config_t UFT_CRC_CONFIG_32C = {
    .name = "CRC-32C",
    .type = UFT_CRC_32C,
    .width = 32,
    .polynomial = 0x1EDC6F41,   /* Castagnoli */
    .init = 0xFFFFFFFF,
    .xor_out = 0xFFFFFFFF,
    .reflect_in = true,
    .reflect_out = true,
    .description = "CRC-32C (iSCSI, SCTP, Btrfs)"
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get CRC configuration by type
 * @param type CRC type
 * @return Configuration pointer or NULL if not found
 */
const uft_crc_config_t *uft_crc_get_config(uft_crc_type_t type);

/**
 * @brief Get CRC configuration by name
 * @param name CRC name (case-insensitive)
 * @return Configuration pointer or NULL if not found
 */
const uft_crc_config_t *uft_crc_get_config_by_name(const char *name);

/**
 * @brief Calculate CRC using configuration
 * @param config CRC configuration
 * @param data Input data
 * @param length Data length
 * @return CRC value (cast to appropriate width)
 */
uint64_t uft_crc_calc(const uft_crc_config_t *config, 
                      const uint8_t *data, size_t length);

/**
 * @brief Calculate CRC with initial value
 */
uint64_t uft_crc_calc_init(const uft_crc_config_t *config,
                           const uint8_t *data, size_t length,
                           uint64_t init);

/**
 * @brief Continue CRC calculation
 */
uint64_t uft_crc_update(const uft_crc_config_t *config,
                        uint64_t crc,
                        const uint8_t *data, size_t length);

/**
 * @brief Finalize CRC calculation
 */
uint64_t uft_crc_finalize(const uft_crc_config_t *config, uint64_t crc);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Convenience Functions - Floppy
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** IBM MFM CRC-16 */
uint16_t uft_crc_ibm_mfm(const uint8_t *data, size_t length);

/** IBM FM CRC-16 */
uint16_t uft_crc_ibm_fm(const uint8_t *data, size_t length);

/** Amiga MFM checksum */
uint32_t uft_crc_amiga_mfm(const uint8_t *data, size_t length);

/** Commodore GCR checksum (XOR) */
uint8_t uft_crc_commodore_gcr(const uint8_t *data, size_t length);

/** Apple GCR checksum */
uint8_t uft_crc_apple_gcr(const uint8_t *data, size_t length);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Convenience Functions - Hard Disk
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** OMTI controller CRC */
uint32_t uft_crc_omti(const uint8_t *data, size_t length);

/** Seagate ST-506 CRC */
uint16_t uft_crc_seagate_st506(const uint8_t *data, size_t length);

/** Western Digital WD1003 CRC */
uint16_t uft_crc_wd1003(const uint8_t *data, size_t length);

/** Adaptec RLL CRC */
uint32_t uft_crc_adaptec(const uint8_t *data, size_t length);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Convenience Functions - Standard
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** CRC-8 */
uint8_t uft_crc8(const uint8_t *data, size_t length);

/** CRC-16 IBM */
uint16_t uft_crc16(const uint8_t *data, size_t length);

/** CRC-16 CCITT */
uint16_t uft_crc16_ccitt(const uint8_t *data, size_t length);

/** CRC-16 XMODEM */
uint16_t uft_crc16_xmodem(const uint8_t *data, size_t length);

/** CRC-32 */
uint32_t uft_crc32(const uint8_t *data, size_t length);

/** CRC-32C (Castagnoli) */
uint32_t uft_crc32c(const uint8_t *data, size_t length);

/** CRC-64 ECMA */
uint64_t uft_crc64(const uint8_t *data, size_t length);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Table Generation
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generate CRC lookup table
 * @param config CRC configuration
 * @param table Output table (256 entries of appropriate width)
 */
void uft_crc_generate_table(const uft_crc_config_t *config, void *table);

/**
 * @brief Get precomputed CRC table
 * @param type CRC type
 * @return Pointer to table or NULL
 */
const void *uft_crc_get_table(uft_crc_type_t type);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Verification
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Verify data against expected CRC
 * @param config CRC configuration
 * @param data Input data (including CRC at end)
 * @param length Data length including CRC
 * @return true if CRC matches
 */
bool uft_crc_verify(const uft_crc_config_t *config,
                    const uint8_t *data, size_t length);

/**
 * @brief Find CRC type that matches given data
 * @param data Input data
 * @param length Data length
 * @param expected Expected CRC value
 * @param width CRC width (8, 16, 32, or 64)
 * @return Matching CRC type or -1 if not found
 */
int uft_crc_identify(const uint8_t *data, size_t length,
                     uint64_t expected, int width);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Reverse Engineering
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Try to determine CRC polynomial from samples
 * @param samples Array of data/CRC pairs
 * @param sample_count Number of samples
 * @param width Expected CRC width
 * @param polynomial Output detected polynomial
 * @return 0 on success, negative on failure
 */
typedef struct {
    const uint8_t *data;
    size_t length;
    uint64_t crc;
} uft_crc_sample_t;

int uft_crc_reverse_polynomial(const uft_crc_sample_t *samples,
                               int sample_count,
                               int width,
                               uint64_t *polynomial);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_POLYS_H */

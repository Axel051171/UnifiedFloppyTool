/**
 * @file uft_copy_protection.h
 * @brief UnifiedFloppyTool - Copy Protection Detection
 * @version 3.1.4.007
 *
 * Detection of various copy protection schemes used on PC floppy disks.
 * Based on analysis of bad sector patterns, file signatures, and layout.
 *
 * Sources analyzed:
 * - DiskImageTool CopyProtection.vb
 *
 * Supported protection schemes:
 * - H.L.S. Duplication
 * - Softguard Superlok v2/v3
 * - Origin Systems OSI-1
 * - KBI (L. Tournier)
 * - MicroProse Cloak
 * - XEMAG XELOK v2
 */

#ifndef UFT_COPY_PROTECTION_H
#define UFT_COPY_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Copy Protection Types
 *============================================================================*/

/**
 * @brief Copy protection scheme types
 */
typedef enum {
    UFT_PROT_NONE              = 0,
    UFT_PROT_HLS_DUPLICATION   = 1,  /**< H.L.S. Duplication */
    UFT_PROT_SOFTGUARD_V2      = 2,  /**< Softguard Superlok v2 */
    UFT_PROT_SOFTGUARD_V2_V3   = 3,  /**< Softguard Superlok v2/v3 */
    UFT_PROT_ORIGIN_OSI1       = 4,  /**< Origin Systems OSI-1 */
    UFT_PROT_KBI               = 5,  /**< KBI (L. Tournier) */
    UFT_PROT_MICROPROSE_CLOAK  = 6,  /**< MicroProse Cloak */
    UFT_PROT_XEMAG_XELOK_V2    = 7,  /**< XEMAG XELOK v2 */
    UFT_PROT_VAULT_PROLOK      = 8,  /**< Vault PROLOK */
    UFT_PROT_EVERLOCK          = 9,  /**< Everlock */
    UFT_PROT_ISM               = 10, /**< ISM (International Software Marketing) */
    UFT_PROT_CUSTOM            = 255 /**< Custom/Unknown protection */
} uft_copy_protection_t;

/*============================================================================
 * Bad Sector Patterns
 *============================================================================*/

/**
 * @brief Bad sector pattern for HLS Duplication (360K, 2 heads)
 * Sectors 708, 709 contain duplicate data
 */
static const uint16_t uft_prot_hls_sectors_2head[] = { 708, 709 };
static const size_t   uft_prot_hls_sectors_2head_count = 2;

/**
 * @brief Bad sector pattern for HLS Duplication (360K, 1 head)
 * Sectors 357, 358 contain duplicate data
 */
static const uint16_t uft_prot_hls_sectors_1head[] = { 357, 358 };
static const size_t   uft_prot_hls_sectors_1head_count = 2;

/**
 * @brief Bad sector pattern for Softguard Superlok v2
 * Sectors 108-117 are bad
 */
static const uint16_t uft_prot_softguard_v2_sectors[] = {
    108, 109, 110, 111, 112, 113, 114, 115, 116, 117
};
static const size_t   uft_prot_softguard_v2_sectors_count = 10;

/**
 * @brief Bad sector pattern for MicroProse Cloak (360K, 0xFD media)
 * Sectors 684-687, 702-705 are bad
 */
static const uint16_t uft_prot_microprose_360k_sectors[] = {
    684, 685, 686, 687, 702, 703, 704, 705
};
static const size_t   uft_prot_microprose_360k_count = 8;
static const uint16_t uft_prot_microprose_360k_exclude[] = { 683, 706 };
static const size_t   uft_prot_microprose_360k_exclude_count = 2;

/**
 * @brief Bad sector pattern for MicroProse Cloak (1.2M, 0xF9 media)
 * Sectors 1406-1409, 1424-1427 are bad
 */
static const uint16_t uft_prot_microprose_12m_sectors[] = {
    1406, 1407, 1408, 1409, 1424, 1425, 1426, 1427
};
static const size_t   uft_prot_microprose_12m_count = 8;
static const uint16_t uft_prot_microprose_12m_exclude[] = { 1405, 1428 };
static const size_t   uft_prot_microprose_12m_exclude_count = 2;

/**
 * @brief Alternate MicroProse Cloak pattern (1.2M)
 */
static const uint16_t uft_prot_microprose_12m_alt_sectors[] = {
    1404, 1405, 1406, 1407, 1422, 1423, 1424, 1425
};
static const size_t   uft_prot_microprose_12m_alt_count = 8;
static const uint16_t uft_prot_microprose_12m_alt_exclude[] = { 1403, 1426 };
static const size_t   uft_prot_microprose_12m_alt_exclude_count = 2;

/**
 * @brief KBI protection search range (sectors 710-729)
 */
#define UFT_PROT_KBI_START_SECTOR   710
#define UFT_PROT_KBI_END_SECTOR     729
static const char uft_prot_kbi_signature[] = "(c) 1986 for KBI by L. TOURNIER";
#define UFT_PROT_KBI_SIGNATURE_LEN  31

/*============================================================================
 * File-Based Detection
 *============================================================================*/

/**
 * @brief Softguard file indicators
 */
static const char *uft_prot_softguard_files[] = {
    "CPC.COM",
    "CML0300.FCL"
};
#define UFT_PROT_SOFTGUARD_FILE_COUNT 2

/**
 * @brief Origin Systems OSI-1 file indicators
 */
static const char *uft_prot_origin_files[] = {
    "2400AD.EXE",
    "ULTIMA.COM",
    "ULTIMA.EXE",
    "ULTIMAII.EXE",
    "LORE.EXE"
};
#define UFT_PROT_ORIGIN_FILE_COUNT 5

/**
 * @brief XEMAG XELOK v2 indicator
 * File "XEMAG.SYS" at sector 162
 */
static const char uft_prot_xemag_file[] = "XEMAG.SYS";
#define UFT_PROT_XEMAG_SECTOR       162

/*============================================================================
 * Detection Context
 *============================================================================*/

/**
 * @brief Copy protection detection context
 */
typedef struct {
    /* Disk info */
    uint8_t  media_descriptor;     /**< FAT media descriptor */
    uint8_t  heads;                /**< Number of heads */
    uint32_t total_sectors;        /**< Total sectors */
    
    /* Bad sector set */
    bool     *bad_sector_map;      /**< Bad sector bitmap */
    size_t   bad_sector_count;     /**< Number of bad sectors */
    
    /* File lookup callback */
    bool     (*file_exists)(const char *filename, void *ctx);
    void     *file_ctx;
    
    /* Sector lookup callback */
    uint16_t (*file_start_sector)(const char *filename, void *ctx);
    
    /* Sector data read callback */
    const uint8_t *(*read_sector)(uint32_t sector, void *ctx);
} uft_copy_protection_ctx_t;

/**
 * @brief Copy protection detection result
 */
typedef struct {
    uft_copy_protection_t type;
    const char           *name;
    int                   confidence;  /**< 0-100 confidence level */
    char                  details[256]; /**< Additional details */
} uft_protection_result_t;

/*============================================================================
 * Detection Functions
 *============================================================================*/

/**
 * @brief Check if all sectors in list are bad
 */
static inline bool uft_prot_check_bad_sectors(const uft_copy_protection_ctx_t *ctx,
                                               const uint16_t *sectors, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (sectors[i] >= ctx->total_sectors) return false;
        if (!ctx->bad_sector_map[sectors[i]]) return false;
    }
    return true;
}

/**
 * @brief Check if all sectors in list are NOT bad
 */
static inline bool uft_prot_check_good_sectors(const uft_copy_protection_ctx_t *ctx,
                                                const uint16_t *sectors, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (sectors[i] >= ctx->total_sectors) continue;
        if (ctx->bad_sector_map[sectors[i]]) return false;
    }
    return true;
}

/**
 * @brief Detect H.L.S. Duplication protection
 */
static inline bool uft_prot_detect_hls(const uft_copy_protection_ctx_t *ctx)
{
    if (ctx->bad_sector_count < 2) return false;
    
    const uint16_t *sectors;
    size_t count;
    
    if (ctx->heads == 1) {
        sectors = uft_prot_hls_sectors_1head;
        count = uft_prot_hls_sectors_1head_count;
    } else {
        sectors = uft_prot_hls_sectors_2head;
        count = uft_prot_hls_sectors_2head_count;
    }
    
    if (!uft_prot_check_bad_sectors(ctx, sectors, count)) return false;
    
    /* Verify duplicate data pattern */
    const uint8_t *s1 = ctx->read_sector(sectors[0], (void*)ctx);
    const uint8_t *s2 = ctx->read_sector(sectors[1], (void*)ctx);
    
    if (!s1 || !s2) return false;
    
    /* Check for duplicate data with numeric pattern at offset 3 */
    if (memcmp(s1, s2, 8) != 0) return false;
    
    /* Verify numeric pattern (4 digits at offset 3) */
    for (int i = 3; i < 7; i++) {
        if (s1[i] < '0' || s1[i] > '9') return false;
    }
    
    return true;
}

/**
 * @brief Detect Softguard Superlok v2 protection
 */
static inline bool uft_prot_detect_softguard_v2(const uft_copy_protection_ctx_t *ctx)
{
    if (ctx->bad_sector_count < 10) return false;
    
    if (!uft_prot_check_bad_sectors(ctx, uft_prot_softguard_v2_sectors,
                                     uft_prot_softguard_v2_sectors_count))
        return false;
    
    /* Need CPC.COM file */
    return ctx->file_exists && ctx->file_exists("CPC.COM", ctx->file_ctx);
}

/**
 * @brief Detect Softguard Superlok v2/v3 protection
 */
static inline bool uft_prot_detect_softguard_v2_v3(const uft_copy_protection_ctx_t *ctx)
{
    return ctx->file_exists && ctx->file_exists("CML0300.FCL", ctx->file_ctx);
}

/**
 * @brief Detect Origin Systems OSI-1 protection
 */
static inline bool uft_prot_detect_origin_osi1(const uft_copy_protection_ctx_t *ctx)
{
    if (ctx->bad_sector_count < 10) return false;
    
    if (!uft_prot_check_bad_sectors(ctx, uft_prot_softguard_v2_sectors,
                                     uft_prot_softguard_v2_sectors_count))
        return false;
    
    /* Check for Origin game files */
    if (!ctx->file_exists) return false;
    
    for (int i = 0; i < UFT_PROT_ORIGIN_FILE_COUNT; i++) {
        if (ctx->file_exists(uft_prot_origin_files[i], ctx->file_ctx))
            return true;
    }
    
    return false;
}

/**
 * @brief Detect KBI protection
 */
static inline bool uft_prot_detect_kbi(const uft_copy_protection_ctx_t *ctx)
{
    if (ctx->bad_sector_count < 10) return false;
    
    for (uint16_t sector = UFT_PROT_KBI_START_SECTOR; 
         sector <= UFT_PROT_KBI_END_SECTOR; sector++) {
        
        if (sector >= ctx->total_sectors) break;
        if (!ctx->bad_sector_map[sector]) continue;
        
        const uint8_t *data = ctx->read_sector(sector, (void*)ctx);
        if (!data) continue;
        
        if (memcmp(data, uft_prot_kbi_signature, UFT_PROT_KBI_SIGNATURE_LEN) == 0)
            return true;
    }
    
    return false;
}

/**
 * @brief Detect MicroProse Cloak protection
 */
static inline bool uft_prot_detect_microprose_cloak(const uft_copy_protection_ctx_t *ctx)
{
    if (ctx->bad_sector_count < 8) return false;
    
    /* Check 360K pattern (0xFD media) */
    if (ctx->media_descriptor == 0xFD) {
        if (uft_prot_check_bad_sectors(ctx, uft_prot_microprose_360k_sectors,
                                        uft_prot_microprose_360k_count) &&
            uft_prot_check_good_sectors(ctx, uft_prot_microprose_360k_exclude,
                                         uft_prot_microprose_360k_exclude_count))
            return true;
    }
    
    /* Check 1.2M pattern (0xF9 media) */
    if (ctx->media_descriptor == 0xF9) {
        if (uft_prot_check_bad_sectors(ctx, uft_prot_microprose_12m_sectors,
                                        uft_prot_microprose_12m_count) &&
            uft_prot_check_good_sectors(ctx, uft_prot_microprose_12m_exclude,
                                         uft_prot_microprose_12m_exclude_count))
            return true;
        
        /* Check alternate pattern */
        if (uft_prot_check_bad_sectors(ctx, uft_prot_microprose_12m_alt_sectors,
                                        uft_prot_microprose_12m_alt_count) &&
            uft_prot_check_good_sectors(ctx, uft_prot_microprose_12m_alt_exclude,
                                         uft_prot_microprose_12m_alt_exclude_count))
            return true;
    }
    
    return false;
}

/**
 * @brief Detect XEMAG XELOK v2 protection
 */
static inline bool uft_prot_detect_xemag_xelok_v2(const uft_copy_protection_ctx_t *ctx)
{
    if (!ctx->file_exists) return false;
    if (!ctx->file_start_sector) return false;
    
    if (!ctx->file_exists(uft_prot_xemag_file, ctx->file_ctx)) return false;
    
    uint16_t sector = ctx->file_start_sector(uft_prot_xemag_file, ctx->file_ctx);
    return sector == UFT_PROT_XEMAG_SECTOR;
}

/**
 * @brief Detect all copy protection schemes
 * @param ctx Detection context
 * @param result Output result
 * @return true if protection detected
 */
bool uft_copy_protection_detect(const uft_copy_protection_ctx_t *ctx,
                                uft_protection_result_t *result);

/**
 * @brief Get protection scheme name
 */
static inline const char *uft_copy_protection_name(uft_copy_protection_t type)
{
    static const char *names[] = {
        [UFT_PROT_NONE]             = "None",
        [UFT_PROT_HLS_DUPLICATION]  = "H.L.S. Duplication",
        [UFT_PROT_SOFTGUARD_V2]     = "Softguard Superlok v2",
        [UFT_PROT_SOFTGUARD_V2_V3]  = "Softguard Superlok v2/v3",
        [UFT_PROT_ORIGIN_OSI1]      = "Origin Systems OSI-1",
        [UFT_PROT_KBI]              = "KBI",
        [UFT_PROT_MICROPROSE_CLOAK] = "MicroProse Cloak",
        [UFT_PROT_XEMAG_XELOK_V2]   = "Xidex Magnetics (XEMAG) XELOK v2",
        [UFT_PROT_VAULT_PROLOK]     = "Vault PROLOK",
        [UFT_PROT_EVERLOCK]         = "Everlock",
        [UFT_PROT_ISM]              = "ISM",
        [UFT_PROT_CUSTOM]           = "Unknown/Custom"
    };
    
    if (type < sizeof(names)/sizeof(names[0]) && names[type])
        return names[type];
    return "Unknown";
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_COPY_PROTECTION_H */

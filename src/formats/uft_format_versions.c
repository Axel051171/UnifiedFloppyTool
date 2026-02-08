/**
 * @file uft_format_versions.c
 * @brief Format Versioning API - Load and Save with Version Support
 * 
 * Provides unified API for loading and saving disk images with
 * explicit version/variant selection.
 * 
 * @version 5.31.1
 * 
 * PERF-FIX: P2-PERF-001 - Buffered padding writes instead of byte-by-byte
 */

#include "uft/core/uft_error_compat.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Padding Helper (PERF-FIX: replaces byte-by-byte loops)
 * ============================================================================ */

/**
 * @brief Write padding bytes efficiently
 * @param fp File pointer
 * @param byte Byte value to repeat
 * @param count Number of bytes to write
 */
static inline void write_padding(FILE *fp, uint8_t byte, size_t count) {
    uint8_t buf[512];
    memset(buf, byte, sizeof(buf));
    
    while (count > 0) {
        size_t chunk = (count < sizeof(buf)) ? count : sizeof(buf);
        if (fwrite(buf, 1, chunk, fp) != chunk) { /* I/O error */ }
        count -= chunk;
    }
}

/* ============================================================================
 * Version Definitions per Format
 * ============================================================================ */

/* D64 Versions */
typedef enum {
    D64_35_TRACK        = 0,    /* Standard 35 track (174848 bytes) */
    D64_35_TRACK_ERRORS = 1,    /* 35 track with error info (175531 bytes) */
    D64_40_TRACK        = 2,    /* Extended 40 track (196608 bytes) */
    D64_40_TRACK_ERRORS = 3,    /* 40 track with error info (197376 bytes) */
    D64_42_TRACK        = 4,    /* SpeedDOS 42 track */
    D64_VERSION_COUNT
} d64_version_t;

/* ADF Versions */
typedef enum {
    ADF_OFS_DD          = 0,    /* Original File System, DD 880K */
    ADF_FFS_DD          = 1,    /* Fast File System, DD 880K */
    ADF_OFS_HD          = 2,    /* OFS, HD 1760K */
    ADF_FFS_HD          = 3,    /* FFS, HD 1760K */
    ADF_INTL_DD         = 4,    /* International mode */
    ADF_INTL_HD         = 5,
    ADF_DIRC_DD         = 6,    /* Directory cache */
    ADF_DIRC_HD         = 7,
    ADF_VERSION_COUNT
} adf_version_t;

/* WOZ Versions */
typedef enum {
    WOZ_V1              = 0,    /* WOZ 1.0 */
    WOZ_V2              = 1,    /* WOZ 2.0 */
    WOZ_VERSION_COUNT
} woz_version_t;

/* ATR Versions */
typedef enum {
    ATR_SD_90K          = 0,    /* Single Density 90K (720×128) */
    ATR_ED_130K         = 1,    /* Enhanced Density 130K (1040×128) */
    ATR_DD_180K         = 2,    /* Double Density 180K (720×256) */
    ATR_QD_360K         = 3,    /* Quad Density 360K (1440×256) */
    ATR_VERSION_COUNT
} atr_version_t;

/* HFE Versions */
typedef enum {
    HFE_V1              = 0,    /* HFE v1 (HXCPICFE) */
    HFE_V3              = 1,    /* HFE v3 (HXCHFEV3) */
    HFE_VERSION_COUNT
} hfe_version_t;

/* NFD Versions */
typedef enum {
    NFD_R0              = 0,    /* Simple format */
    NFD_R1              = 1,    /* Extended format with sector headers */
    NFD_VERSION_COUNT
} nfd_version_t;

/* D88 Versions */
typedef enum {
    D88_2D              = 0,    /* 2D (320K) */
    D88_2DD             = 1,    /* 2DD (640K/720K) */
    D88_2HD             = 2,    /* 2HD (1.2M) */
    D88_VERSION_COUNT
} d88_version_t;

/* TD0 Versions */
typedef enum {
    TD0_NORMAL          = 0,    /* Uncompressed (TD) */
    TD0_ADVANCED        = 1,    /* LZSS compressed (td) */
    TD0_VERSION_COUNT
} td0_version_t;

/* DMS Versions (Compression modes) */
typedef enum {
    DMS_NONE            = 0,    /* No compression */
    DMS_SIMPLE          = 1,    /* RLE only */
    DMS_QUICK           = 2,    /* Quick LZ */
    DMS_MEDIUM          = 3,    /* Medium LZ */
    DMS_DEEP            = 4,    /* Deep LZ */
    DMS_HEAVY1          = 5,    /* Heavy compression 1 */
    DMS_HEAVY2          = 6,    /* Heavy compression 2 */
    DMS_VERSION_COUNT
} dms_version_t;

/* IMG/IMA Versions */
typedef enum {
    IMG_160K            = 0,    /* 160K 5.25" SS/SD */
    IMG_180K            = 1,    /* 180K 5.25" SS/SD */
    IMG_320K            = 2,    /* 320K 5.25" DS/SD */
    IMG_360K            = 3,    /* 360K 5.25" DS/DD */
    IMG_720K            = 4,    /* 720K 3.5" DS/DD */
    IMG_1200K           = 5,    /* 1.2M 5.25" DS/HD */
    IMG_1440K           = 6,    /* 1.44M 3.5" DS/HD */
    IMG_2880K           = 7,    /* 2.88M 3.5" DS/ED */
    IMG_VERSION_COUNT
} img_version_t;

/* SSD/DSD Versions */
typedef enum {
    SSD_40T             = 0,    /* 40 track single-sided */
    SSD_80T             = 1,    /* 80 track single-sided */
    DSD_40T             = 2,    /* 40 track double-sided */
    DSD_80T             = 3,    /* 80 track double-sided */
    SSD_VERSION_COUNT
} ssd_version_t;

/* TRD Versions */
typedef enum {
    TRD_SS_40T          = 0,    /* Single-sided 40 track */
    TRD_DS_80T          = 1,    /* Double-sided 80 track (standard) */
    TRD_DS_40T          = 2,    /* Double-sided 40 track */
    TRD_VERSION_COUNT
} trd_version_t;

/* DSK/EDSK Versions */
typedef enum {
    DSK_STANDARD        = 0,    /* MV-CPC standard DSK */
    DSK_EXTENDED        = 1,    /* EXTENDED DSK (EDSK) */
    DSK_VERSION_COUNT
} dsk_version_t;

/* G64 Versions */
typedef enum {
    G64_1541            = 0,    /* Standard 1541 */
    G64_1541_40T        = 1,    /* 1541 40 track */
    G71_1571            = 2,    /* 1571 double-sided */
    G64_VERSION_COUNT
} g64_version_t;

/* ============================================================================
 * Version Info Structure
 * ============================================================================ */

typedef struct {
    int          version_id;
    const char  *name;
    const char  *description;
    uint64_t     typical_size;
    int          tracks;
    int          sides;
    int          sectors;
    int          sector_size;
} version_info_t;

/* ============================================================================
 * Version Tables
 * ============================================================================ */

static const version_info_t D64_VERSIONS[] = {
    { D64_35_TRACK,        "35 Track",        "Standard 1541",     174848,  35, 1, 683, 256 },
    { D64_35_TRACK_ERRORS, "35 Track+Err",    "With error info",   175531,  35, 1, 683, 256 },
    { D64_40_TRACK,        "40 Track",        "Extended",          196608,  40, 1, 768, 256 },
    { D64_40_TRACK_ERRORS, "40 Track+Err",    "Extended+errors",   197376,  40, 1, 768, 256 },
    { D64_42_TRACK,        "42 Track",        "SpeedDOS",          205312,  42, 1, 802, 256 },
};

static const version_info_t ADF_VERSIONS[] = {
    { ADF_OFS_DD,  "OFS DD",     "Original File System 880K",  901120,  80, 2, 11, 512 },
    { ADF_FFS_DD,  "FFS DD",     "Fast File System 880K",      901120,  80, 2, 11, 512 },
    { ADF_OFS_HD,  "OFS HD",     "Original File System 1760K", 1802240, 80, 2, 22, 512 },
    { ADF_FFS_HD,  "FFS HD",     "Fast File System 1760K",     1802240, 80, 2, 22, 512 },
    { ADF_INTL_DD, "INTL DD",    "International 880K",         901120,  80, 2, 11, 512 },
    { ADF_INTL_HD, "INTL HD",    "International 1760K",        1802240, 80, 2, 22, 512 },
    { ADF_DIRC_DD, "DIRC DD",    "Dir Cache 880K",             901120,  80, 2, 11, 512 },
    { ADF_DIRC_HD, "DIRC HD",    "Dir Cache 1760K",            1802240, 80, 2, 22, 512 },
};

static const version_info_t WOZ_VERSIONS[] = {
    { WOZ_V1, "WOZ 1.0", "Original Applesauce format", 0, 35, 1, 16, 256 },
    { WOZ_V2, "WOZ 2.0", "Extended with flux timing",  0, 35, 1, 16, 256 },
};

static const version_info_t ATR_VERSIONS[] = {
    { ATR_SD_90K,  "SD 90K",   "Single Density",   92176,  40, 1, 18, 128 },
    { ATR_ED_130K, "ED 130K",  "Enhanced Density", 133136, 40, 1, 26, 128 },
    { ATR_DD_180K, "DD 180K",  "Double Density",   184336, 40, 1, 18, 256 },
    { ATR_QD_360K, "QD 360K",  "Quad Density",     368656, 80, 1, 18, 256 },
};

static const version_info_t HFE_VERSIONS[] = {
    { HFE_V1, "HFE v1", "Standard HxC format",   0, 80, 2, 0, 0 },
    { HFE_V3, "HFE v3", "Extended with metadata", 0, 80, 2, 0, 0 },
};

static const version_info_t NFD_VERSIONS[] = {
    { NFD_R0, "NFD r0", "Simple fixed geometry",    0, 77, 2, 8, 1024 },
    { NFD_R1, "NFD r1", "Extended per-sector info", 0, 77, 2, 8, 1024 },
};

static const version_info_t D88_VERSIONS[] = {
    { D88_2D,  "2D",  "320K (40T×2H×16S×256B)",  348160,  40, 2, 16, 256 },
    { D88_2DD, "2DD", "640K (80T×2H×16S×256B)",  696320,  80, 2, 16, 256 },
    { D88_2HD, "2HD", "1.2M (77T×2H×8S×1024B)", 1261568,  77, 2,  8, 1024 },
};

static const version_info_t TD0_VERSIONS[] = {
    { TD0_NORMAL,   "Normal",   "Uncompressed (TD signature)", 0, 80, 2, 0, 0 },
    { TD0_ADVANCED, "Advanced", "LZSS compressed (td)",        0, 80, 2, 0, 0 },
};

static const version_info_t DMS_VERSIONS[] = {
    { DMS_NONE,   "None",   "No compression",           0, 80, 2, 11, 512 },
    { DMS_SIMPLE, "Simple", "RLE only",                 0, 80, 2, 11, 512 },
    { DMS_QUICK,  "Quick",  "Fast LZ",                  0, 80, 2, 11, 512 },
    { DMS_MEDIUM, "Medium", "Balanced LZ",              0, 80, 2, 11, 512 },
    { DMS_DEEP,   "Deep",   "Best ratio LZ",            0, 80, 2, 11, 512 },
    { DMS_HEAVY1, "Heavy1", "Maximum compression",      0, 80, 2, 11, 512 },
    { DMS_HEAVY2, "Heavy2", "Maximum+ compression",     0, 80, 2, 11, 512 },
};

static const version_info_t IMG_VERSIONS[] = {
    { IMG_160K,  "160K",  "5.25\" SS/SD",  163840,  40, 1,  8, 512 },
    { IMG_180K,  "180K",  "5.25\" SS/SD",  184320,  40, 1,  9, 512 },
    { IMG_320K,  "320K",  "5.25\" DS/SD",  327680,  40, 2,  8, 512 },
    { IMG_360K,  "360K",  "5.25\" DS/DD",  368640,  40, 2,  9, 512 },
    { IMG_720K,  "720K",  "3.5\" DS/DD",   737280,  80, 2,  9, 512 },
    { IMG_1200K, "1.2M",  "5.25\" DS/HD", 1228800,  80, 2, 15, 512 },
    { IMG_1440K, "1.44M", "3.5\" DS/HD",  1474560,  80, 2, 18, 512 },
    { IMG_2880K, "2.88M", "3.5\" DS/ED",  2949120,  80, 2, 36, 512 },
};

static const version_info_t SSD_VERSIONS[] = {
    { SSD_40T, "SSD 40T", "40 track single-sided", 102400, 40, 1, 10, 256 },
    { SSD_80T, "SSD 80T", "80 track single-sided", 204800, 80, 1, 10, 256 },
    { DSD_40T, "DSD 40T", "40 track double-sided", 204800, 40, 2, 10, 256 },
    { DSD_80T, "DSD 80T", "80 track double-sided", 409600, 80, 2, 10, 256 },
};

static const version_info_t TRD_VERSIONS[] = {
    { TRD_SS_40T, "SS 40T", "Single-sided 40 track", 163840, 40, 1, 16, 256 },
    { TRD_DS_80T, "DS 80T", "Double-sided 80 track", 655360, 80, 2, 16, 256 },
    { TRD_DS_40T, "DS 40T", "Double-sided 40 track", 327680, 40, 2, 16, 256 },
};

static const version_info_t DSK_VERSIONS[] = {
    { DSK_STANDARD, "Standard", "MV-CPC format",    0, 40, 1, 9, 512 },
    { DSK_EXTENDED, "Extended", "EDSK with extras", 0, 40, 1, 9, 512 },
};

static const version_info_t G64_VERSIONS[] = {
    { G64_1541,    "1541",    "Standard 35 track",     0, 35, 1, 0, 0 },
    { G64_1541_40T,"1541 40T","Extended 40 track",     0, 40, 1, 0, 0 },
    { G71_1571,    "1571",    "Double-sided 70 track", 0, 70, 2, 0, 0 },
};

/* ============================================================================
 * API Functions - Get Version Info
 * ============================================================================ */

/**
 * @brief Get available versions for a format
 */
int uft_get_versions(const char *format, const version_info_t **versions, int *count)
{
    if (!format || !versions || !count) return -1;
    
    if (strcasecmp(format, "d64") == 0) {
        *versions = D64_VERSIONS;
        *count = D64_VERSION_COUNT;
    } else if (strcasecmp(format, "adf") == 0) {
        *versions = ADF_VERSIONS;
        *count = ADF_VERSION_COUNT;
    } else if (strcasecmp(format, "woz") == 0) {
        *versions = WOZ_VERSIONS;
        *count = WOZ_VERSION_COUNT;
    } else if (strcasecmp(format, "atr") == 0) {
        *versions = ATR_VERSIONS;
        *count = ATR_VERSION_COUNT;
    } else if (strcasecmp(format, "hfe") == 0) {
        *versions = HFE_VERSIONS;
        *count = HFE_VERSION_COUNT;
    } else if (strcasecmp(format, "nfd") == 0) {
        *versions = NFD_VERSIONS;
        *count = NFD_VERSION_COUNT;
    } else if (strcasecmp(format, "d88") == 0) {
        *versions = D88_VERSIONS;
        *count = D88_VERSION_COUNT;
    } else if (strcasecmp(format, "td0") == 0) {
        *versions = TD0_VERSIONS;
        *count = TD0_VERSION_COUNT;
    } else if (strcasecmp(format, "dms") == 0) {
        *versions = DMS_VERSIONS;
        *count = DMS_VERSION_COUNT;
    } else if (strcasecmp(format, "img") == 0 || strcasecmp(format, "ima") == 0) {
        *versions = IMG_VERSIONS;
        *count = IMG_VERSION_COUNT;
    } else if (strcasecmp(format, "ssd") == 0 || strcasecmp(format, "dsd") == 0) {
        *versions = SSD_VERSIONS;
        *count = SSD_VERSION_COUNT;
    } else if (strcasecmp(format, "trd") == 0) {
        *versions = TRD_VERSIONS;
        *count = TRD_VERSION_COUNT;
    } else if (strcasecmp(format, "dsk") == 0 || strcasecmp(format, "edsk") == 0) {
        *versions = DSK_VERSIONS;
        *count = DSK_VERSION_COUNT;
    } else if (strcasecmp(format, "g64") == 0 || strcasecmp(format, "g71") == 0) {
        *versions = G64_VERSIONS;
        *count = G64_VERSION_COUNT;
    } else {
        *versions = NULL;
        *count = 0;
        return -1;
    }
    
    return 0;
}

/**
 * @brief Print available versions for a format
 */
void uft_print_versions(const char *format)
{
    const version_info_t *versions;
    int count;
    
    if (uft_get_versions(format, &versions, &count) != 0) {
        printf("Format '%s' not found or has no versions.\n", format);
        return;
    }
    
    printf("╔═══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║ Available versions for %s                                                     \n", format);
    printf("╠═══════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║ ID │ Name          │ Description                  │ Size      │ Geometry     ║\n");
    printf("╠═══════════════════════════════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < count; i++) {
        printf("║ %2d │ %-13s │ %-28s │ %9llu │ %2dT×%dH×%2dS  ║\n",
               versions[i].version_id,
               versions[i].name,
               versions[i].description,
               (unsigned long long)versions[i].typical_size,
               versions[i].tracks,
               versions[i].sides,
               versions[i].sectors);
    }
    
    printf("╚═══════════════════════════════════════════════════════════════════════════════╝\n");
}

/* ============================================================================
 * Generic Disk Image Structure
 * ============================================================================ */

typedef struct {
    uint8_t *data;
    size_t   size;
    int      tracks;
    int      sides;
    int      sectors_per_track;
    int      sector_size;
    int      version;
    char     format[32];  /* Increased from 16 to match strncpy usage */
    bool     write_protected;
    bool     has_errors;
    uint8_t *error_info;
} uft_disk_image_t;

/* ============================================================================
 * Load Functions with Version Detection
 * ============================================================================ */

/**
 * @brief Load disk image with automatic version detection
 */
int uft_load_image(const char *path, uft_disk_image_t *image, int *detected_version)
{
    if (!path || !image) return -1;
    
    memset(image, 0, sizeof(*image));
    
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    image->size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    image->data = malloc(image->size);
    if (!image->data) {
        fclose(fp);
        return -1;
    }
    
    if (fread(image->data, 1, image->size, fp) != image->size) { /* I/O error */ }
    fclose(fp);
    
    /* Detect format and version */
    const char *ext = strrchr(path, '.');
    if (ext) ext++;
    
    /* D64 version detection */
    if (ext && strcasecmp(ext, "d64") == 0) {
        strncpy(image->format, "D64", 31); image->format[31] = '\0';
        image->sector_size = 256;
        
        switch (image->size) {
            case 174848:
                *detected_version = D64_35_TRACK;
                image->tracks = 35;
                break;
            case 175531:
                *detected_version = D64_35_TRACK_ERRORS;
                image->tracks = 35;
                image->has_errors = true;
                image->error_info = image->data + 174848;
                break;
            case 196608:
                *detected_version = D64_40_TRACK;
                image->tracks = 40;
                break;
            case 197376:
                *detected_version = D64_40_TRACK_ERRORS;
                image->tracks = 40;
                image->has_errors = true;
                image->error_info = image->data + 196608;
                break;
            default:
                *detected_version = D64_35_TRACK;
                image->tracks = 35;
        }
        image->sides = 1;
    }
    /* ADF version detection */
    else if (ext && strcasecmp(ext, "adf") == 0) {
        strncpy(image->format, "ADF", 31); image->format[31] = '\0';
        image->sector_size = 512;
        image->sides = 2;
        image->sectors_per_track = (image->size > 1000000) ? 22 : 11;
        image->tracks = 80;
        
        /* Check filesystem type from bootblock */
        if (image->size >= 4) {
            if (memcmp(image->data, "DOS", 3) == 0) {
                int fs_type = image->data[3];
                bool is_hd = (image->size > 1000000);
                
                switch (fs_type & 0x07) {
                    case 0: *detected_version = is_hd ? ADF_OFS_HD : ADF_OFS_DD; break;
                    case 1: *detected_version = is_hd ? ADF_FFS_HD : ADF_FFS_DD; break;
                    case 2: *detected_version = is_hd ? ADF_INTL_HD : ADF_INTL_DD; break;
                    case 3: *detected_version = is_hd ? ADF_INTL_HD : ADF_INTL_DD; break;
                    case 4: *detected_version = is_hd ? ADF_DIRC_HD : ADF_DIRC_DD; break;
                    case 5: *detected_version = is_hd ? ADF_DIRC_HD : ADF_DIRC_DD; break;
                    default: *detected_version = is_hd ? ADF_OFS_HD : ADF_OFS_DD;
                }
            } else {
                *detected_version = (image->size > 1000000) ? ADF_OFS_HD : ADF_OFS_DD;
            }
        }
    }
    /* ATR version detection */
    else if (ext && strcasecmp(ext, "atr") == 0) {
        strncpy(image->format, "ATR", 31); image->format[31] = '\0';
        
        if (image->size >= 16 && image->data[0] == 0x96 && image->data[1] == 0x02) {
            uint16_t sect_size = image->data[4] | (image->data[5] << 8);
            image->sector_size = sect_size;
            
            if (sect_size == 128) {
                if (image->size < 100000) {
                    *detected_version = ATR_SD_90K;
                } else {
                    *detected_version = ATR_ED_130K;
                }
            } else {
                if (image->size < 200000) {
                    *detected_version = ATR_DD_180K;
                } else {
                    *detected_version = ATR_QD_360K;
                }
            }
        }
    }
    /* WOZ version detection */
    else if (ext && strcasecmp(ext, "woz") == 0) {
        strncpy(image->format, "WOZ", 31); image->format[31] = '\0';
        
        if (image->size >= 4) {
            if (memcmp(image->data, "WOZ1", 4) == 0) {
                *detected_version = WOZ_V1;
            } else if (memcmp(image->data, "WOZ2", 4) == 0) {
                *detected_version = WOZ_V2;
            }
        }
    }
    /* Add more format detection as needed... */
    
    image->version = *detected_version;
    
    return 0;
}

/* ============================================================================
 * Save Functions with Version Selection
 * ============================================================================ */

/**
 * @brief Save D64 with specific version
 */
int uft_save_d64(const char *path, const uft_disk_image_t *image, d64_version_t version)
{
    if (!path || !image || !image->data) return -1;
    
    size_t output_size;
    bool include_errors = false;
    int tracks = 35;
    
    switch (version) {
        case D64_35_TRACK:
            output_size = 174848;
            tracks = 35;
            break;
        case D64_35_TRACK_ERRORS:
            output_size = 175531;
            tracks = 35;
            include_errors = true;
            break;
        case D64_40_TRACK:
            output_size = 196608;
            tracks = 40;
            break;
        case D64_40_TRACK_ERRORS:
            output_size = 197376;
            tracks = 40;
            include_errors = true;
            break;
        case D64_42_TRACK:
            output_size = 205312;
            tracks = 42;
            break;
        default:
            return -1;
    }
    
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    
    /* Write data (truncate or pad as needed) */
    size_t data_size = output_size;
    if (include_errors) {
        data_size = output_size - (tracks == 35 ? 683 : 768);
    }
    
    if (image->size >= data_size) {
        if (fwrite(image->data, 1, data_size, fp) != data_size) { /* I/O error */ }
    } else {
        if (fwrite(image->data, 1, image->size, fp) != image->size) { /* I/O error */ }
        /* Pad with zeros - PERF-FIX: buffered padding */
        write_padding(fp, 0x00, data_size - image->size);
    }
    
    /* Write error info if requested */
    if (include_errors) {
        int error_count = (tracks == 35) ? 683 : 768;
        if (image->error_info) {
            if (fwrite(image->error_info, 1, error_count, fp) != error_count) { /* I/O error */ }
        } else {
            /* Write default "no error" values - PERF-FIX: buffered padding */
            write_padding(fp, 0x01, error_count);
        }
    }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return 0;
}

/**
 * @brief Save ADF with specific version
 */
int uft_save_adf(const char *path, const uft_disk_image_t *image, adf_version_t version)
{
    if (!path || !image || !image->data) return -1;
    
    size_t output_size;
    uint8_t fs_type;
    
    switch (version) {
        case ADF_OFS_DD:  output_size = 901120;  fs_type = 0x00; break;
        case ADF_FFS_DD:  output_size = 901120;  fs_type = 0x01; break;
        case ADF_OFS_HD:  output_size = 1802240; fs_type = 0x00; break;
        case ADF_FFS_HD:  output_size = 1802240; fs_type = 0x01; break;
        case ADF_INTL_DD: output_size = 901120;  fs_type = 0x02; break;
        case ADF_INTL_HD: output_size = 1802240; fs_type = 0x02; break;
        case ADF_DIRC_DD: output_size = 901120;  fs_type = 0x04; break;
        case ADF_DIRC_HD: output_size = 1802240; fs_type = 0x04; break;
        default: return -1;
    }
    
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    
    /* Prepare bootblock with filesystem type */
    uint8_t bootblock[1024];
    if (image->size >= 1024) {
        memcpy(bootblock, image->data, 1024);
    } else {
        memset(bootblock, 0, 1024);
    }
    
    /* Set filesystem type */
    bootblock[0] = 'D';
    bootblock[1] = 'O';
    bootblock[2] = 'S';
    bootblock[3] = fs_type;
    
    if (fwrite(bootblock, 1, 1024, fp) != 1024) { /* I/O error */ }
    /* Write rest of data */
    if (image->size > 1024) {
        size_t remaining = (image->size - 1024 < output_size - 1024) ? 
                           image->size - 1024 : output_size - 1024;
        if (fwrite(image->data + 1024, 1, remaining, fp) != remaining) { /* I/O error */ }
        /* Pad if needed - PERF-FIX: buffered padding */
        if (remaining < output_size - 1024) {
            write_padding(fp, 0x00, (output_size - 1024) - remaining);
        }
    }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return 0;
}

/**
 * @brief Save ATR with specific version
 */
int uft_save_atr(const char *path, const uft_disk_image_t *image, atr_version_t version)
{
    if (!path || !image || !image->data) return -1;
    
    int sector_count, sector_size;
    
    switch (version) {
        case ATR_SD_90K:
            sector_count = 720;
            sector_size = 128;
            break;
        case ATR_ED_130K:
            sector_count = 1040;
            sector_size = 128;
            break;
        case ATR_DD_180K:
            sector_count = 720;
            sector_size = 256;
            break;
        case ATR_QD_360K:
            sector_count = 1440;
            sector_size = 256;
            break;
        default:
            return -1;
    }
    
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    
    /* Calculate data size (first 3 sectors are always 128 bytes) */
    size_t data_size = 3 * 128 + (sector_count - 3) * sector_size;
    
    /* Write ATR header */
    uint8_t header[16] = {0};
    header[0] = 0x96;
    header[1] = 0x02;
    
    uint32_t paragraphs = data_size / 16;
    header[2] = paragraphs & 0xFF;
    header[3] = (paragraphs >> 8) & 0xFF;
    header[4] = sector_size & 0xFF;
    header[5] = (sector_size >> 8) & 0xFF;
    header[6] = (paragraphs >> 16) & 0xFF;
    
    if (fwrite(header, 1, 16, fp) != 16) { /* I/O error */ }
    /* Write data */
    size_t write_size = (image->size < data_size) ? image->size : data_size;
    if (fwrite(image->data, 1, write_size, fp) != write_size) { /* I/O error */ }
    /* Pad if needed - PERF-FIX: buffered padding */
    if (write_size < data_size) {
        write_padding(fp, 0x00, data_size - write_size);
    }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return 0;
}

/**
 * @brief Generic save with format and version
 */
int uft_save_image(const char *path, const uft_disk_image_t *image, 
                   const char *format, int version)
{
    if (!path || !image || !format) return -1;
    
    if (strcasecmp(format, "d64") == 0) {
        return uft_save_d64(path, image, (d64_version_t)version);
    } else if (strcasecmp(format, "adf") == 0) {
        return uft_save_adf(path, image, (adf_version_t)version);
    } else if (strcasecmp(format, "atr") == 0) {
        return uft_save_atr(path, image, (atr_version_t)version);
    }
    /* Add more formats... */
    
    return -1;
}

/**
 * @brief Free disk image
 */
void uft_free_image(uft_disk_image_t *image)
{
    if (image) {
        free(image->data);
        memset(image, 0, sizeof(*image));
    }
}

/* ============================================================================
 * Print All Supported Formats and Versions
 * ============================================================================ */

void uft_print_all_versions(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                      UFT FORMAT VERSION SUPPORT                                          ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════════════════╝\n\n");
    
    const char *formats[] = { "d64", "adf", "woz", "atr", "hfe", "nfd", "d88", "td0", "dms", "img", "ssd", "trd", "dsk", "g64", NULL };
    
    for (int i = 0; formats[i]; i++) {
        uft_print_versions(formats[i]);
        printf("\n");
    }
}

/*
 * uft_preset_japanese.h - Japanese Computer Format Presets
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on MAME format implementations (BSD-3-Clause)
 *
 * Covers Japanese-specific disk formats:
 * - DIM (DIFC Header format)
 * - NFD (T98-NEXT format, Rev 0 & 1)
 * - FDD (PC-98 sector map format)
 * - XDF (X68000 format)
 * - D88 (PC-88/98 format)
 */

#ifndef UFT_PRESET_JAPANESE_H
#define UFT_PRESET_JAPANESE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Format IDs
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_japanese_format_id {
    /* DIM Format */
    UFT_DIM_2HD = 0,            /* 1.2M 2HD (8 sec 1024 byte) */
    UFT_DIM_2HS,                /* 1.44M 2HS (9 sec 1024 byte) */
    UFT_DIM_2HC,                /* 1.2M 2HC (15 sec 512 byte) */
    UFT_DIM_2HQ,                /* 1.44M 2HQ (18 sec 512 byte) */
    UFT_DIM_2DD,                /* 720K 2DD (9 sec 512 byte) */
    UFT_DIM_2D,                 /* 320K 2D (16 sec 256 byte) */
    
    /* NFD Format (T98-NEXT) */
    UFT_NFD_R0,                 /* NFD Revision 0 */
    UFT_NFD_R1,                 /* NFD Revision 1 */
    
    /* FDD Format */
    UFT_FDD,                    /* PC-98 FDD */
    
    /* XDF Format (Sharp X68000) */
    UFT_XDF_2HD,                /* X68000 2HD */
    UFT_XDF_2DD,                /* X68000 2DD */
    
    /* D88 Format (PC-88/98 standard) */
    UFT_D88_2D,                 /* D88 2D (320K) */
    UFT_D88_2DD,                /* D88 2DD (640K) */
    UFT_D88_2HD,                /* D88 2HD (1.2M) */
    
    UFT_JAPANESE_FORMAT_COUNT
} uft_japanese_format_id_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * DIM Format Structures
 * Signature: "DIFC HEADER" at offset 0xAB
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_DIM_SIGNATURE       "DIFC HEADER"
#define UFT_DIM_SIG_OFFSET      0xAB
#define UFT_DIM_DATA_OFFSET     0x100

typedef struct uft_dim_header {
    uint8_t  media_type;        /* Media type code (see table) */
    uint8_t  reserved[0xAA];
    char     signature[11];     /* "DIFC HEADER" */
    uint8_t  padding[0x44];
} uft_dim_header_t;

/* DIM media type codes */
typedef enum uft_dim_media_type {
    UFT_DIM_TYPE_2HD = 0,       /* 8 sectors, 1024 bytes */
    UFT_DIM_TYPE_2HS = 1,       /* 9 sectors, 1024 bytes */
    UFT_DIM_TYPE_2HC = 2,       /* 15 sectors, 512 bytes */
    UFT_DIM_TYPE_2DD_8 = 3,     /* 8 sectors, 512 bytes */
    UFT_DIM_TYPE_2HQ = 9,       /* 18 sectors, 512 bytes */
    UFT_DIM_TYPE_2DD_9 = 17,    /* 26 sectors, 256 bytes (2D) */
} uft_dim_media_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * NFD Format Structures (T98-NEXT)
 * Signature: "T98FDDIMAGE.R0" or "T98FDDIMAGE.R1"
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_NFD_SIGNATURE       "T98FDDIMAGE.R"
#define UFT_NFD_HEADER_MIN      0x120

typedef struct uft_nfd_header {
    char     signature[16];     /* "T98FDDIMAGE.R*\0\0" */
    char     comment[256];      /* Image info/comments */
    uint32_t header_length;     /* Total header length */
    uint8_t  write_protect;     /* 0 = writeable */
    uint8_t  heads;             /* Number of heads */
    uint8_t  reserved[10];
    /* Followed by sector map */
} uft_nfd_header_t;

typedef struct uft_nfd_sector_map {
    uint8_t  track;             /* Track number */
    uint8_t  head;              /* Head number */
    uint8_t  sector;            /* Sector number */
    uint8_t  size;              /* Size in 128-byte units */
    uint8_t  mfm;               /* 1 = MFM, 0 = FM */
    uint8_t  ddam;              /* 1 = Deleted, 0 = Normal */
    uint8_t  status[4];         /* FDC status bytes */
    uint8_t  retry;             /* Rev 1: retry data flag */
    uint8_t  pda;               /* Disk type */
    uint8_t  reserved[4];
} uft_nfd_sector_map_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * FDD Format Structures
 * Header size: 0xC3FC
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_FDD_HEADER_SIZE     0xC3FC
#define UFT_FDD_SECTOR_MAP_OFF  0xDC
#define UFT_FDD_SECTOR_ENTRY    12

typedef struct uft_fdd_sector_map {
    uint8_t  track;             /* 0xFF = unformatted */
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size;              /* 128 << size */
    uint8_t  fill_byte;         /* 0xFF = normal, else fill value */
    uint8_t  reserved[3];
    uint32_t data_offset;       /* Absolute offset, 0xFFFFFFFF = use fill */
} uft_fdd_sector_map_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * D88 Format Structures
 * Used by many PC-88/98 emulators
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_D88_HEADER_SIZE     0x2B0
#define UFT_D88_TRACK_MAX       164     /* 82 tracks * 2 sides */

typedef struct uft_d88_header {
    char     name[17];          /* Disk name (null terminated) */
    uint8_t  reserved1[9];
    uint8_t  write_protect;     /* 0x00 = normal, 0x10 = protected */
    uint8_t  media_type;        /* 0x00=2D, 0x10=2DD, 0x20=2HD */
    uint32_t disk_size;         /* Total file size */
    uint32_t track_offset[UFT_D88_TRACK_MAX];
} uft_d88_header_t;

typedef struct uft_d88_sector {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size;              /* 128 << size */
    uint16_t sectors_in_track;
    uint8_t  density;           /* 0x00=double, 0x40=single */
    uint8_t  deleted;           /* 0x00=normal, 0x10=deleted */
    uint8_t  status;            /* FDC status */
    uint8_t  reserved[5];
    uint16_t data_size;         /* Actual data size */
    /* Followed by sector data */
} uft_d88_sector_t;

/* D88 media types */
typedef enum uft_d88_media {
    UFT_D88_MEDIA_2D = 0x00,
    UFT_D88_MEDIA_2DD = 0x10,
    UFT_D88_MEDIA_2HD = 0x20,
} uft_d88_media_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_japanese_preset {
    uft_japanese_format_id_t id;
    const char *name;
    const char *description;
    const char *signature;
    uint16_t sig_offset;
    
    /* Geometry (for fixed formats) */
    uint8_t  cyls;
    uint8_t  heads;
    uint8_t  secs;
    uint16_t bps;
    
    /* Extensions */
    const char *extensions;
} uft_japanese_preset_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_japanese_preset_t UFT_JAPANESE_PRESETS[] = {
    /* DIM formats */
    {
        .id = UFT_DIM_2HD, .name = "DIM 2HD",
        .description = "DIM 1.2M (8 sec 1024 byte)",
        .signature = UFT_DIM_SIGNATURE, .sig_offset = UFT_DIM_SIG_OFFSET,
        .cyls = 77, .heads = 2, .secs = 8, .bps = 1024,
        .extensions = ".dim"
    },
    {
        .id = UFT_DIM_2HS, .name = "DIM 2HS",
        .description = "DIM 1.44M (9 sec 1024 byte)",
        .signature = UFT_DIM_SIGNATURE, .sig_offset = UFT_DIM_SIG_OFFSET,
        .cyls = 80, .heads = 2, .secs = 9, .bps = 1024,
        .extensions = ".dim"
    },
    {
        .id = UFT_DIM_2HC, .name = "DIM 2HC",
        .description = "DIM 1.2M (15 sec 512 byte)",
        .signature = UFT_DIM_SIGNATURE, .sig_offset = UFT_DIM_SIG_OFFSET,
        .cyls = 80, .heads = 2, .secs = 15, .bps = 512,
        .extensions = ".dim"
    },
    {
        .id = UFT_DIM_2HQ, .name = "DIM 2HQ",
        .description = "DIM 1.44M (18 sec 512 byte)",
        .signature = UFT_DIM_SIGNATURE, .sig_offset = UFT_DIM_SIG_OFFSET,
        .cyls = 80, .heads = 2, .secs = 18, .bps = 512,
        .extensions = ".dim"
    },
    {
        .id = UFT_DIM_2DD, .name = "DIM 2DD",
        .description = "DIM 720K (9 sec 512 byte)",
        .signature = UFT_DIM_SIGNATURE, .sig_offset = UFT_DIM_SIG_OFFSET,
        .cyls = 80, .heads = 2, .secs = 9, .bps = 512,
        .extensions = ".dim"
    },
    {
        .id = UFT_DIM_2D, .name = "DIM 2D",
        .description = "DIM 320K (16 sec 256 byte)",
        .signature = UFT_DIM_SIGNATURE, .sig_offset = UFT_DIM_SIG_OFFSET,
        .cyls = 40, .heads = 2, .secs = 16, .bps = 256,
        .extensions = ".dim"
    },
    
    /* NFD formats */
    {
        .id = UFT_NFD_R0, .name = "NFD R0",
        .description = "T98-NEXT NFD Revision 0",
        .signature = "T98FDDIMAGE.R0", .sig_offset = 0,
        .cyls = 77, .heads = 2, .secs = 0, .bps = 0,
        .extensions = ".nfd"
    },
    {
        .id = UFT_NFD_R1, .name = "NFD R1",
        .description = "T98-NEXT NFD Revision 1",
        .signature = "T98FDDIMAGE.R1", .sig_offset = 0,
        .cyls = 77, .heads = 2, .secs = 0, .bps = 0,
        .extensions = ".nfd"
    },
    
    /* FDD format */
    {
        .id = UFT_FDD, .name = "FDD",
        .description = "PC-98 FDD format",
        .signature = NULL, .sig_offset = 0,
        .cyls = 77, .heads = 2, .secs = 0, .bps = 0,
        .extensions = ".fdd"
    },
    
    /* XDF formats */
    {
        .id = UFT_XDF_2HD, .name = "XDF 2HD",
        .description = "X68000 2HD (8 sec 1024 byte)",
        .signature = NULL, .sig_offset = 0,
        .cyls = 77, .heads = 2, .secs = 8, .bps = 1024,
        .extensions = ".xdf;.hdm;.2hd"
    },
    {
        .id = UFT_XDF_2DD, .name = "XDF 2DD",
        .description = "X68000 2DD",
        .signature = NULL, .sig_offset = 0,
        .cyls = 80, .heads = 2, .secs = 9, .bps = 512,
        .extensions = ".xdf"
    },
    
    /* D88 formats */
    {
        .id = UFT_D88_2D, .name = "D88 2D",
        .description = "D88 320K (PC-88)",
        .signature = NULL, .sig_offset = 0,
        .cyls = 40, .heads = 2, .secs = 16, .bps = 256,
        .extensions = ".d88;.88d;.d68;.d98"
    },
    {
        .id = UFT_D88_2DD, .name = "D88 2DD",
        .description = "D88 640K/720K",
        .signature = NULL, .sig_offset = 0,
        .cyls = 80, .heads = 2, .secs = 16, .bps = 256,
        .extensions = ".d88;.88d;.d68;.d98"
    },
    {
        .id = UFT_D88_2HD, .name = "D88 2HD",
        .description = "D88 1.2M (PC-98)",
        .signature = NULL, .sig_offset = 0,
        .cyls = 77, .heads = 2, .secs = 8, .bps = 1024,
        .extensions = ".d88;.88d;.d68;.d98"
    },
};

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline const uft_japanese_preset_t *uft_japanese_get_preset(uft_japanese_format_id_t id) {
    if (id >= UFT_JAPANESE_FORMAT_COUNT) return NULL;
    return &UFT_JAPANESE_PRESETS[id];
}

/**
 * Detect DIM media type from header byte
 */
static inline uft_japanese_format_id_t uft_dim_detect_type(uint8_t media_byte) {
    switch (media_byte) {
        case 0: return UFT_DIM_2HD;
        case 1: return UFT_DIM_2HS;
        case 2: return UFT_DIM_2HC;
        case 9: return UFT_DIM_2HQ;
        case 3: return UFT_DIM_2DD;
        case 17: return UFT_DIM_2D;
        default: return UFT_JAPANESE_FORMAT_COUNT;
    }
}

/**
 * Get DIM geometry from media type
 */
static inline void uft_dim_get_geometry(uint8_t media_byte,
                                         uint8_t *secs, uint8_t *size_code, uint8_t *gap3) {
    switch (media_byte) {
        case 0:  *secs = 8;  *size_code = 3; *gap3 = 0x74; break;
        case 1:  *secs = 9;  *size_code = 3; *gap3 = 0x39; break;
        case 2:  *secs = 15; *size_code = 2; *gap3 = 0x54; break;
        case 3:  *secs = 9;  *size_code = 3; *gap3 = 0x39; break;
        case 9:  *secs = 18; *size_code = 2; *gap3 = 0x54; break;
        case 17: *secs = 26; *size_code = 1; *gap3 = 0x33; break;
        default: *secs = 0;  *size_code = 0; *gap3 = 0;    break;
    }
}

/**
 * Probe for DIM format
 */
static inline int uft_dim_probe(const uint8_t *data, size_t len) {
    if (!data || len < UFT_DIM_DATA_OFFSET) return 0;
    return (memcmp(data + UFT_DIM_SIG_OFFSET, UFT_DIM_SIGNATURE, 11) == 0);
}

/**
 * Probe for NFD format
 */
static inline int uft_nfd_probe(const uint8_t *data, size_t len) {
    if (!data || len < UFT_NFD_HEADER_MIN) return 0;
    return (memcmp(data, UFT_NFD_SIGNATURE, 13) == 0);
}

/**
 * Get NFD revision
 */
static inline int uft_nfd_revision(const uint8_t *data, size_t len) {
    if (!uft_nfd_probe(data, len)) return -1;
    return data[13] - '0';  /* 'R0' or 'R1' */
}

#ifdef __cplusplus
}
#endif
#endif /* UFT_PRESET_JAPANESE_H */

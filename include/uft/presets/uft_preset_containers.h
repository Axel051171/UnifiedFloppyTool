/*
 * uft_preset_containers.h - Disk Image Container Format Presets
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on MAME cqm_dsk.cpp, imd_dsk.cpp, td0_dsk.cpp (BSD-3-Clause)
 *
 * Container formats for disk preservation and archival.
 * These formats store disk geometry and sector data, often compressed.
 */

#ifndef UFT_PRESET_CONTAINERS_H
#define UFT_PRESET_CONTAINERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Container Format IDs
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_container_format_id {
    /* CopyQM (CQM) */
    UFT_CONTAINER_CQM = 0,
    
    /* ImageDisk (IMD) */
    UFT_CONTAINER_IMD,
    
    /* Teledisk (TD0) */
    UFT_CONTAINER_TD0,
    UFT_CONTAINER_TD0_ADV,      /* TD0 Advanced Compression */
    
    /* QCOW (QEMU) */
    UFT_CONTAINER_QCOW,
    UFT_CONTAINER_QCOW2,
    
    /* VirtualBox/VMware */
    UFT_CONTAINER_VDI,
    UFT_CONTAINER_VMDK,
    UFT_CONTAINER_VHD,
    
    /* Raw sector image */
    UFT_CONTAINER_RAW,
    UFT_CONTAINER_IMG,
    UFT_CONTAINER_IMA,
    
    UFT_CONTAINER_FORMAT_COUNT
} uft_container_format_id_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * CopyQM (CQM) Format
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_CQM_SIGNATURE       "CQ"
#define UFT_CQM_HEADER_SIZE     133

typedef struct uft_cqm_header {
    uint8_t  signature[2];      /* "CQ" */
    uint8_t  version;           /* Format version */
    uint16_t sector_size;       /* Bytes per sector */
    uint8_t  reserved1[13];
    uint8_t  sectors_per_track; /* Sectors per track */
    uint8_t  reserved2;
    uint8_t  heads;             /* Number of sides */
    uint8_t  reserved3[72];
    uint8_t  tracks;            /* Number of tracks */
    uint8_t  reserved4[21];
    uint16_t comment_length;    /* Length of comment */
    uint8_t  sector_base;       /* First sector number - 1 */
    uint8_t  reserved5[2];
    uint8_t  interleave;        /* Sector interleave */
    uint8_t  skew;              /* Track skew */
} uft_cqm_header_t;

/* CQM RLE compression markers */
#define UFT_CQM_RLE_REPEAT      0x8000  /* Negative length = repeat byte */

/* ═══════════════════════════════════════════════════════════════════════════
 * ImageDisk (IMD) Format
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_IMD_SIGNATURE       "IMD"
#define UFT_IMD_HEADER_END      0x1A

typedef struct uft_imd_track {
    uint8_t  mode;              /* Recording mode */
    uint8_t  cylinder;          /* Cylinder number */
    uint8_t  head;              /* Head number + flags */
    uint8_t  sector_count;      /* Number of sectors */
    uint8_t  sector_size;       /* Sector size code (128 << n) */
    /* Followed by sector numbering map */
    /* Optional: cylinder map (if head & 0x80) */
    /* Optional: head map (if head & 0x40) */
    /* Sector data follows */
} uft_imd_track_t;

/* IMD recording modes */
typedef enum uft_imd_mode {
    UFT_IMD_MODE_500_FM = 0,    /* 500 kbps FM */
    UFT_IMD_MODE_300_FM,        /* 300 kbps FM */
    UFT_IMD_MODE_250_FM,        /* 250 kbps FM */
    UFT_IMD_MODE_500_MFM,       /* 500 kbps MFM */
    UFT_IMD_MODE_300_MFM,       /* 300 kbps MFM */
    UFT_IMD_MODE_250_MFM,       /* 250 kbps MFM */
} uft_imd_mode_t;

/* IMD sector data types */
typedef enum uft_imd_sector_type {
    UFT_IMD_UNAVAILABLE = 0,    /* Sector not available */
    UFT_IMD_NORMAL,             /* Normal sector data */
    UFT_IMD_COMPRESSED,         /* Compressed (fill byte) */
    UFT_IMD_DELETED,            /* Deleted data */
    UFT_IMD_DELETED_COMP,       /* Deleted + compressed */
    UFT_IMD_ERROR,              /* Data error */
    UFT_IMD_ERROR_COMP,         /* Error + compressed */
    UFT_IMD_DEL_ERROR,          /* Deleted + error */
    UFT_IMD_DEL_ERROR_COMP,     /* Deleted + error + compressed */
} uft_imd_sector_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Teledisk (TD0) Format
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_TD0_SIGNATURE       "TD"
#define UFT_TD0_ADV_SIGNATURE   "td"    /* Advanced compression */
#define UFT_TD0_HEADER_SIZE     12

typedef struct uft_td0_header {
    uint8_t  signature[2];      /* "TD" or "td" */
    uint8_t  volume_seq;        /* Volume sequence (0 for first) */
    uint8_t  check_sig;         /* Check signature */
    uint8_t  version;           /* TD0 version */
    uint8_t  data_rate;         /* Data rate */
    uint8_t  drive_type;        /* Drive type */
    uint8_t  stepping;          /* Stepping */
    uint8_t  dos_alloc;         /* DOS allocation flag */
    uint8_t  heads;             /* Number of sides */
    uint16_t crc;               /* Header CRC */
} uft_td0_header_t;

/* TD0 data rates */
typedef enum uft_td0_rate {
    UFT_TD0_RATE_250K = 0,      /* 250 kbps */
    UFT_TD0_RATE_300K,          /* 300 kbps */
    UFT_TD0_RATE_500K,          /* 500 kbps */
} uft_td0_rate_t;

/* TD0 drive types */
typedef enum uft_td0_drive {
    UFT_TD0_DRIVE_525_360K = 0, /* 5.25" 360K */
    UFT_TD0_DRIVE_525_1200K,    /* 5.25" 1.2M */
    UFT_TD0_DRIVE_35_720K,      /* 3.5" 720K */
    UFT_TD0_DRIVE_35_1440K,     /* 3.5" 1.44M */
} uft_td0_drive_t;

/* TD0 uses LZSS + Adaptive Huffman compression */
#define UFT_TD0_LZSS_N          4096    /* Ring buffer size */
#define UFT_TD0_LZSS_F          60      /* Lookahead buffer size */
#define UFT_TD0_LZSS_THRESHOLD  2       /* Minimum match length */

/* ═══════════════════════════════════════════════════════════════════════════
 * Container Preset Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_container_preset {
    uft_container_format_id_t id;
    const char *name;
    const char *description;
    const char *signature;
    uint8_t  signature_len;
    uint16_t header_size;
    uint8_t  compressed;        /* 1 = uses compression */
    uint8_t  stores_geometry;   /* 1 = geometry in file */
    const char *extensions;
} uft_container_preset_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_container_preset_t UFT_CONTAINER_PRESETS[] = {
    {
        .id = UFT_CONTAINER_CQM,
        .name = "CopyQM",
        .description = "CopyQM compressed disk image",
        .signature = "CQ", .signature_len = 2,
        .header_size = 133, .compressed = 1, .stores_geometry = 1,
        .extensions = ".cqm"
    },
    {
        .id = UFT_CONTAINER_IMD,
        .name = "ImageDisk",
        .description = "Dave Dunfield ImageDisk format",
        .signature = "IMD", .signature_len = 3,
        .header_size = 0, .compressed = 1, .stores_geometry = 1,
        .extensions = ".imd"
    },
    {
        .id = UFT_CONTAINER_TD0,
        .name = "Teledisk",
        .description = "Sydex Teledisk format",
        .signature = "TD", .signature_len = 2,
        .header_size = 12, .compressed = 0, .stores_geometry = 1,
        .extensions = ".td0"
    },
    {
        .id = UFT_CONTAINER_TD0_ADV,
        .name = "Teledisk (ADV)",
        .description = "Teledisk with advanced compression",
        .signature = "td", .signature_len = 2,
        .header_size = 12, .compressed = 1, .stores_geometry = 1,
        .extensions = ".td0"
    },
    {
        .id = UFT_CONTAINER_QCOW,
        .name = "QCOW",
        .description = "QEMU Copy-On-Write v1",
        .signature = "QFI\xfb", .signature_len = 4,
        .header_size = 48, .compressed = 1, .stores_geometry = 0,
        .extensions = ".qcow"
    },
    {
        .id = UFT_CONTAINER_QCOW2,
        .name = "QCOW2",
        .description = "QEMU Copy-On-Write v2/v3",
        .signature = "QFI\xfb", .signature_len = 4,
        .header_size = 104, .compressed = 1, .stores_geometry = 0,
        .extensions = ".qcow2"
    },
    {
        .id = UFT_CONTAINER_VDI,
        .name = "VDI",
        .description = "VirtualBox Disk Image",
        .signature = "<<< ", .signature_len = 4,
        .header_size = 400, .compressed = 0, .stores_geometry = 0,
        .extensions = ".vdi"
    },
    {
        .id = UFT_CONTAINER_VMDK,
        .name = "VMDK",
        .description = "VMware Virtual Disk",
        .signature = "KDMV", .signature_len = 4,
        .header_size = 512, .compressed = 0, .stores_geometry = 0,
        .extensions = ".vmdk"
    },
    {
        .id = UFT_CONTAINER_VHD,
        .name = "VHD",
        .description = "Microsoft Virtual Hard Disk",
        .signature = "conectix", .signature_len = 8,
        .header_size = 512, .compressed = 0, .stores_geometry = 0,
        .extensions = ".vhd"
    },
    {
        .id = UFT_CONTAINER_RAW,
        .name = "RAW",
        .description = "Raw sector image",
        .signature = NULL, .signature_len = 0,
        .header_size = 0, .compressed = 0, .stores_geometry = 0,
        .extensions = ".raw"
    },
    {
        .id = UFT_CONTAINER_IMG,
        .name = "IMG",
        .description = "Raw disk image",
        .signature = NULL, .signature_len = 0,
        .header_size = 0, .compressed = 0, .stores_geometry = 0,
        .extensions = ".img"
    },
    {
        .id = UFT_CONTAINER_IMA,
        .name = "IMA",
        .description = "Raw floppy image",
        .signature = NULL, .signature_len = 0,
        .header_size = 0, .compressed = 0, .stores_geometry = 0,
        .extensions = ".ima"
    },
};

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline const uft_container_preset_t *uft_container_get_preset(uft_container_format_id_t id) {
    if (id >= UFT_CONTAINER_FORMAT_COUNT) return NULL;
    return &UFT_CONTAINER_PRESETS[id];
}

/**
 * Detect container format from signature
 */
static inline uft_container_format_id_t uft_container_detect(const uint8_t *data, size_t len) {
    if (!data || len < 8) return UFT_CONTAINER_FORMAT_COUNT;
    
    /* Check each format's signature */
    for (int i = 0; i < UFT_CONTAINER_FORMAT_COUNT; i++) {
        const uft_container_preset_t *p = &UFT_CONTAINER_PRESETS[i];
        if (p->signature && p->signature_len > 0 && len >= p->signature_len) {
            int match = 1;
            for (size_t j = 0; j < p->signature_len; j++) {
                if (data[j] != (uint8_t)p->signature[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) return (uft_container_format_id_t)i;
        }
    }
    
    return UFT_CONTAINER_FORMAT_COUNT;
}

/**
 * Calculate IMD sector size from code
 */
static inline uint32_t uft_imd_sector_size(uint8_t code) {
    return 128U << code;  /* 0=128, 1=256, 2=512, 3=1024, etc. */
}

#ifdef __cplusplus
}
#endif
#endif /* UFT_PRESET_CONTAINERS_H */

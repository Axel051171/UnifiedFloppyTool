/**
 * @file uft_libflux_formats.h
 * @brief HxC Format Structures and Loaders for UFT
 * 
 * This file contains format structures extracted from UFT HFE Format
 * for supporting 100+ disk image formats.
 * 
 * Original UFT HFE Format: http://hxc2001.free.fr
 * 
 * Integrated for UnifiedFloppyTool (UFT) - December 2025
 */

#ifndef UFT_LIBFLUX_FORMATS_H
#define UFT_LIBFLUX_FORMATS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/*                           IPF FORMAT (SPS/CAPS)                            */
/*===========================================================================*/

/**
 * IPF (Interchangeable Preservation Format) - Copy protection support
 * Used by Software Preservation Society (SPS) / CAPS
 */

#define UFT_IPF_ID 0x843265bb

/* Encoder types */
#define UFT_IPF_ENCOD_CAPS 1
#define UFT_IPF_ENCOD_SPS  2

/* Density types for copy protection */
typedef enum {
    UFT_IPF_DEN_NOISE     = 1,   /* Random noise area */
    UFT_IPF_DEN_UNIFORM   = 2,   /* Uniform density */
    UFT_IPF_DEN_COPYLOCK  = 3,   /* Copylock protection */
    UFT_IPF_DEN_SPEEDLOCK = 6    /* Speedlock protection */
} uft_ipf_density_type_t;

/* Chunk codes */
typedef enum {
    UFT_IPF_CHUNK_END   = 0,
    UFT_IPF_CHUNK_SYNC  = 1,
    UFT_IPF_CHUNK_DATA  = 2,
    UFT_IPF_CHUNK_GAP   = 3,
    UFT_IPF_CHUNK_RAW   = 4,
    UFT_IPF_CHUNK_FLAKY = 5
} uft_ipf_chunk_code_t;

#pragma pack(push, 1)

typedef struct {
    uint8_t  id[4];             /* Magic ID */
    uint32_t len;               /* Chunk length */
    uint32_t crc;               /* CRC32 */
} uft_ipf_header_t;

typedef struct {
    uint32_t type;
    uint32_t encoder;           /* CAPS or SPS */
    uint32_t enc_rev;
    uint32_t release;
    uint32_t revision;
    uint32_t origin;
    uint32_t min_cyl;
    uint32_t max_cyl;
    uint32_t min_head;
    uint32_t max_head;
    uint32_t date;
    uint32_t time;
    uint32_t platform[4];
    uint32_t disk_num;
    uint32_t user_id;
    uint32_t reserved[3];
} uft_ipf_info_t;

typedef struct {
    uint32_t cyl;
    uint32_t head;
    uint32_t den_type;          /* Density type for protection */
    uint32_t sig_type;
    uint32_t trk_size;
    uint32_t start_pos;
    uint32_t start_bit;
    uint32_t data_bits;
    uint32_t gap_bits;
    uint32_t trk_bits;
    uint32_t blk_cnt;
    uint32_t process;
    uint32_t flags;
    uint32_t dat_chunk;
    uint32_t reserved[3];
} uft_ipf_img_t;

typedef struct {
    uint32_t size;
    uint32_t bsize;
    uint32_t dcrc;
    uint32_t dat_chunk;
} uft_ipf_data_t;

typedef struct {
    uint32_t blockbits;
    uint32_t gapbits;
    union {
        struct {
            uint32_t block_size;
            uint32_t gap_size;
        } caps;
        struct {
            uint32_t gap_offset;
            uint32_t cell_type;
        } sps;
    } u;
    uint32_t enc_type;
    uint32_t flag;
    uint32_t gap_value;
    uint32_t data_offset;
} uft_ipf_block_t;

#pragma pack(pop)

/*===========================================================================*/
/*                           DMK FORMAT (TRS-80)                              */
/*===========================================================================*/

/**
 * DMK (David Keil's TRS-80 format)
 * Supports FM and MFM with IDAM pointers
 */

#define UFT_DMK_FLAG_SINGLE_SIDE    0x10
#define UFT_DMK_FLAG_SINGLE_DENSITY 0x40
#define UFT_DMK_FLAG_IGNORE_DENSITY 0x80

#pragma pack(push, 1)

typedef struct {
    uint8_t  write_protected;
    uint8_t  track_number;
    uint16_t track_len;         /* Track length in bytes */
    uint8_t  flags;             /* See UFT_DMK_FLAG_* */
    uint8_t  reserved1[7];      /* 0x05-0x0B */
    uint8_t  reserved2[4];      /* 0x0C-0x0F */
} uft_dmk_header_t;

#pragma pack(pop)

/* DMK track structure:
 * - 64 bytes: IDAM pointer table (up to 64 sector headers)
 * - track_len - 64 bytes: raw track data (FM or MFM)
 * 
 * IDAM pointers:
 * - Bit 15: 0=MFM, 1=FM (double density flag)
 * - Bits 0-13: Offset to IDAM within track data
 */

#define UFT_DMK_IDAM_TABLE_SIZE 64
#define UFT_DMK_IDAM_FM_FLAG    0x8000
#define UFT_DMK_IDAM_OFFSET_MASK 0x3FFF

/*===========================================================================*/
/*                           A2R FORMAT (Applesauce)                          */
/*===========================================================================*/

/**
 * A2R (Applesauce) - Apple II flux capture format
 */

#pragma pack(push, 1)

typedef struct {
    uint8_t  sign[4];           /* "A2R2" */
    uint8_t  ff_byte;           /* 0xFF */
    uint8_t  lfcrlf[3];         /* 0x0A 0x0D 0x0A */
} uft_a2r_header_t;

typedef struct {
    uint8_t  sign[4];           /* "INFO", "STRM", "META" */
    uint32_t chunk_size;
} uft_a2r_chunk_header_t;

typedef struct {
    uint8_t  version;           /* 1 or 2 */
    char     creator[32];
    uint8_t  disk_type;         /* 1=5.25", 2=3.5" */
    uint8_t  write_protected;
    uint8_t  synchronized;      /* Cross-track sync used */
} uft_a2r_info_t;

typedef struct {
    uint8_t  location;          /* Track * 4 + quarter track offset */
    uint8_t  capture_type;      /* 1=timing, 2=bits, 3=xtiming */
    uint32_t data_length;
    uint32_t estimated_loop_point;
} uft_a2r_capture_t;

#pragma pack(pop)

/* A2R capture types */
#define UFT_A2R_CAPTURE_TIMING  1   /* Flux timing data */
#define UFT_A2R_CAPTURE_BITS    2   /* Bitstream data */
#define UFT_A2R_CAPTURE_XTIMING 3   /* Extended timing */

/*===========================================================================*/
/*                           WOZ FORMAT (Apple II)                            */
/*===========================================================================*/

/**
 * WOZ - Apple II preservation format (v1, v2, v3)
 */

/* Chunk IDs */
#define UFT_WOZ_CHUNK_INFO 0x4F464E49  /* "INFO" */
#define UFT_WOZ_CHUNK_TMAP 0x50414D54  /* "TMAP" */
#define UFT_WOZ_CHUNK_TRKS 0x534B5254  /* "TRKS" */
#define UFT_WOZ_CHUNK_META 0x4154454D  /* "META" */
#define UFT_WOZ_CHUNK_WRIT 0x54495257  /* "WRIT" (v2+) */

/* Disk types */
#define UFT_WOZ_DISK_525   1   /* 5.25" floppy */
#define UFT_WOZ_DISK_35    2   /* 3.5" floppy */

/* Compatible hardware flags */
#define UFT_WOZ_HW_APPLE2       0x0001
#define UFT_WOZ_HW_APPLE2PLUS   0x0002
#define UFT_WOZ_HW_APPLE2E      0x0004
#define UFT_WOZ_HW_APPLE2C      0x0008
#define UFT_WOZ_HW_APPLE2E_ENH  0x0010
#define UFT_WOZ_HW_APPLE2GS     0x0020
#define UFT_WOZ_HW_APPLE2C_PLUS 0x0040
#define UFT_WOZ_HW_APPLE3       0x0080
#define UFT_WOZ_HW_APPLE3_PLUS  0x0100

#pragma pack(push, 1)

typedef struct {
    uint8_t  headertag[3];      /* "WOZ" */
    uint8_t  version;           /* '1', '2', or '3' */
    uint8_t  pad;               /* 0xFF */
    uint8_t  lfcrlf[3];         /* 0x0A 0x0D 0x0A */
    uint32_t crc32;             /* CRC32 of remaining file */
} uft_woz_fileheader_t;

typedef struct {
    uint32_t id;
    uint32_t size;
    /* uint8_t data[]; */
} uft_woz_chunk_t;

typedef struct {
    /* v1, v2, v3 */
    uint8_t  version;
    uint8_t  disk_type;         /* 1=5.25", 2=3.5" */
    uint8_t  write_protected;
    uint8_t  sync;              /* Cross-track synchronized */
    uint8_t  cleaned;           /* MC3470 fake bits removed */
    uint8_t  creator[32];
    /* v2, v3 */
    uint8_t  sides_count;
    uint8_t  boot_sector_format; /* 1=16-sector, 2=13-sector, 3=both */
    uint8_t  bit_timing;        /* 125ns increments (8 = 1µs) */
    uint16_t compatible_hw;     /* Hardware flags */
    uint16_t required_ram;      /* In KB */
    uint16_t largest_track;     /* In 512-byte blocks */
    /* v3 */
    uint16_t flux_block;
    uint16_t largest_flux_track;
} uft_woz_info_t;

/* v2+ track entry */
typedef struct {
    uint16_t starting_block;    /* From file start, in 512-byte blocks */
    uint16_t block_count;
    uint32_t bit_count;
} uft_woz_trk_t;

/* v1 track structure */
typedef struct {
    uint8_t  bitstream[6646];
    uint16_t bytes_count;
    uint16_t bit_count;
    uint16_t bit_splice_point;  /* 0xFFFF if no splice info */
    uint8_t  splice_nibble;
    uint8_t  splice_bit_count;
    uint16_t reserved;
} uft_woz_trk_v1_t;

#pragma pack(pop)

/*===========================================================================*/
/*                           IMD FORMAT (ImageDisk)                           */
/*===========================================================================*/

/**
 * IMD (ImageDisk by various authors)
 * Supports FM/MFM with per-sector metadata
 */

/* Mode byte encoding */
#define UFT_IMD_MODE_500_FM   0   /* 500 kbps FM */
#define UFT_IMD_MODE_300_FM   1   /* 300 kbps FM */
#define UFT_IMD_MODE_250_FM   2   /* 250 kbps FM */
#define UFT_IMD_MODE_500_MFM  3   /* 500 kbps MFM */
#define UFT_IMD_MODE_300_MFM  4   /* 300 kbps MFM */
#define UFT_IMD_MODE_250_MFM  5   /* 250 kbps MFM */

/* Sector data types */
#define UFT_IMD_DATA_UNAVAIL  0   /* Sector data unavailable */
#define UFT_IMD_DATA_NORMAL   1   /* Normal data */
#define UFT_IMD_DATA_COMPRESS 2   /* Compressed (all same byte) */
#define UFT_IMD_DATA_DEL      3   /* Deleted data */
#define UFT_IMD_DATA_DEL_COMP 4   /* Deleted + compressed */
#define UFT_IMD_DATA_ERROR    5   /* Data with error */
#define UFT_IMD_DATA_ERR_COMP 6   /* Error + compressed */
#define UFT_IMD_DATA_DEL_ERR  7   /* Deleted + error */
#define UFT_IMD_DATA_DEL_ERR_C 8  /* Deleted + error + compressed */

#pragma pack(push, 1)

typedef struct {
    uint8_t  mode;              /* Recording mode */
    uint8_t  cylinder;
    uint8_t  head;              /* Bit 7: sector cylinder map, Bit 6: sector head map */
    uint8_t  sectors;           /* Number of sectors */
    uint8_t  sector_size;       /* 0=128, 1=256, 2=512, 3=1024, 4=2048, 5=4096, 6=8192 */
    /* Followed by:
     * - Sector numbering map (sectors bytes)
     * - Optional: Cylinder map (if head bit 7 set)
     * - Optional: Head map (if head bit 6 set)
     * - Sector data records
     */
} uft_imd_track_header_t;

#pragma pack(pop)

/*===========================================================================*/
/*                           D64/D81 FORMATS (Commodore)                      */
/*===========================================================================*/

/**
 * D64 - Commodore 1541 disk image
 * D81 - Commodore 1581 disk image
 */

/* D64 track/sector counts by zone */
static const uint8_t UFT_D64_SECTORS_PER_TRACK[] = {
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 1-17 */
    19, 19, 19, 19, 19, 19, 19,                                          /* 18-24 */
    18, 18, 18, 18, 18, 18,                                              /* 25-30 */
    17, 17, 17, 17, 17,                                                  /* 31-35 */
    17, 17, 17, 17, 17                                                   /* 36-40 (extended) */
};

/* D64 speed zones */
static const uint8_t UFT_D64_SPEED_ZONE[] = {
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  /* Zone 3: tracks 1-17 */
    2, 2, 2, 2, 2, 2, 2,                                /* Zone 2: tracks 18-24 */
    1, 1, 1, 1, 1, 1,                                   /* Zone 1: tracks 25-30 */
    0, 0, 0, 0, 0,                                      /* Zone 0: tracks 31-35 */
    0, 0, 0, 0, 0                                       /* Zone 0: tracks 36-40 */
};

/* D64 standard sizes */
#define UFT_D64_SIZE_35_TRACKS      174848   /* 35 tracks, no errors */
#define UFT_D64_SIZE_35_TRACKS_ERR  175531   /* 35 tracks with error info */
#define UFT_D64_SIZE_40_TRACKS      196608   /* 40 tracks, no errors */
#define UFT_D64_SIZE_40_TRACKS_ERR  197376   /* 40 tracks with error info */

/* D81 constants */
#define UFT_D81_TRACKS              80
#define UFT_D81_SECTORS_PER_TRACK   40
#define UFT_D81_SECTOR_SIZE         256
#define UFT_D81_SIZE                819200

/*===========================================================================*/
/*                           ADF FORMAT (Amiga)                               */
/*===========================================================================*/

/**
 * ADF - Amiga Disk File
 */

#define UFT_ADF_SECTOR_SIZE     512
#define UFT_ADF_SECTORS_TRACK   11
#define UFT_ADF_TRACKS          80
#define UFT_ADF_SIDES           2

#define UFT_ADF_DD_SIZE         (UFT_ADF_SECTOR_SIZE * UFT_ADF_SECTORS_TRACK * UFT_ADF_TRACKS * UFT_ADF_SIDES)  /* 901120 bytes */
#define UFT_ADF_HD_SIZE         (UFT_ADF_DD_SIZE * 2)  /* 1802240 bytes */

/* Amiga MFM sync word */
#define UFT_AMIGA_MFM_SYNC      0x4489

/* Amiga boot block types */
#define UFT_AMIGA_BOOTBLOCK_OFS 0x444F5300  /* "DOS\0" - OFS */
#define UFT_AMIGA_BOOTBLOCK_FFS 0x444F5301  /* "DOS\1" - FFS */
#define UFT_AMIGA_BOOTBLOCK_OFS_INTL 0x444F5302  /* International OFS */
#define UFT_AMIGA_BOOTBLOCK_FFS_INTL 0x444F5303  /* International FFS */
#define UFT_AMIGA_BOOTBLOCK_OFS_DC   0x444F5304  /* Dir cache OFS */
#define UFT_AMIGA_BOOTBLOCK_FFS_DC   0x444F5305  /* Dir cache FFS */

/*===========================================================================*/
/*                           MSA FORMAT (Atari ST)                            */
/*===========================================================================*/

/**
 * MSA - Magic Shadow Archiver (Atari ST compressed disk image)
 */

#define UFT_MSA_SIGNATURE   0x0E0F

#pragma pack(push, 1)

typedef struct {
    uint16_t signature;         /* 0x0E0F */
    uint16_t sectors_per_track; /* 9, 10, or 11 */
    uint16_t sides;             /* 0 = single, 1 = double */
    uint16_t start_track;       /* Usually 0 */
    uint16_t end_track;         /* Usually 79 or 80 */
} uft_msa_header_t;

#pragma pack(pop)

/*===========================================================================*/
/*                           D88 FORMAT (PC-88/98)                            */
/*===========================================================================*/

/**
 * D88 - Japanese PC-88/PC-98 disk image format
 */

#define UFT_D88_MAX_TRACK_OFFSET 164

/* Disk types */
#define UFT_D88_TYPE_2D     0x00   /* 2D */
#define UFT_D88_TYPE_2DD    0x10   /* 2DD */
#define UFT_D88_TYPE_2HD    0x20   /* 2HD */
#define UFT_D88_TYPE_1D     0x30   /* 1D */
#define UFT_D88_TYPE_1DD    0x40   /* 1DD */

/* Density */
#define UFT_D88_DENSITY_D   0x00   /* Double density (MFM) */
#define UFT_D88_DENSITY_S   0x40   /* Single density (FM) */
#define UFT_D88_DENSITY_H   0x01   /* High density */

#include <stddef.h>

#pragma pack(push, 1)

typedef struct {
    char     name[17];          /* Disk name (null-terminated) */
    uint8_t  reserved1[9];
    uint8_t  write_protect;     /* 0x00 = no, 0x10 = yes */
    uint8_t  disk_type;         /* See UFT_D88_TYPE_* */
    uint32_t disk_size;         /* Total file size */
    uint32_t track_offset[UFT_D88_MAX_TRACK_OFFSET];  /* Offsets to track data */
} uft_d88_header_t;

typedef struct {
    uint8_t  c;                 /* Cylinder */
    uint8_t  h;                 /* Head */
    uint8_t  r;                 /* Record (sector number) */
    uint8_t  n;                 /* Sector size code (0=128, 1=256, 2=512, 3=1024) */
    uint16_t sectors;           /* Number of sectors */
    uint8_t  density;           /* See UFT_D88_DENSITY_* */
    uint8_t  deleted;           /* Deleted data mark */
    uint8_t  status;            /* FDC status */
    uint8_t  reserved[5];
    uint16_t data_size;         /* Actual data size */
    /* Followed by sector data */
} uft_d88_sector_header_t;

#pragma pack(pop)

/*===========================================================================*/
/*                           STX FORMAT (Atari Pasti)                         */
/*===========================================================================*/

/**
 * STX - Pasti format for Atari ST with copy protection support
 */

#define UFT_STX_SIGNATURE 0x01525350  /* "RSP\1" */

#pragma pack(push, 1)

typedef struct {
    uint32_t signature;         /* "RSP\1" */
    uint16_t version;           /* Format version */
    uint16_t tool_revision;
    uint16_t reserved1;
    uint8_t  track_count;
    uint8_t  revision;
    uint32_t reserved2;
} uft_stx_header_t;

typedef struct {
    uint32_t record_size;       /* Total record size */
    uint32_t fuzzy_count;       /* Number of fuzzy mask bytes */
    uint16_t sector_count;
    uint16_t track_flags;
    uint16_t track_length;      /* MFM track length in bytes */
    uint8_t  track_number;
    uint8_t  track_type;        /* 0=FM, 1=MFM */
} uft_stx_track_header_t;

#pragma pack(pop)

/*===========================================================================*/
/*                           TELEDISK FORMAT                                  */
/*===========================================================================*/

/**
 * TeleDisk (.TD0) - Sydex disk archiver format
 */

#define UFT_TD0_SIGNATURE_NORMAL    0x4454  /* "TD" */
#define UFT_TD0_SIGNATURE_ADVANCED  0x6474  /* "td" (advanced compression) */

#pragma pack(push, 1)

typedef struct {
    uint16_t signature;         /* "TD" or "td" */
    uint8_t  sequence;          /* Volume sequence (0 for single) */
    uint8_t  check_sequence;    /* Check byte for sequence */
    uint8_t  version;           /* TeleDisk version */
    uint8_t  data_rate;         /* 0=250K, 1=300K, 2=500K */
    uint8_t  drive_type;        /* 1=360K, 2=1.2M, 3=720K, 4=1.44M */
    uint8_t  stepping;          /* 0=single, 1=double stepping */
    uint8_t  dos_alloc;         /* DOS allocation flag */
    uint8_t  sides;             /* Number of sides */
    uint16_t crc;               /* Header CRC */
} uft_td0_header_t;

#pragma pack(pop)

/*===========================================================================*/
/*                           FORMAT DETECTION MAGIC                           */
/*===========================================================================*/

/**
 * Magic byte signatures for format auto-detection
 */

typedef struct {
    const char *name;
    const uint8_t *magic;
    size_t magic_len;
    size_t magic_offset;
} uft_format_magic_t;

/* Common magic signatures */
static const uint8_t UFT_MAGIC_HFE[]  = { 'H', 'X', 'C', 'P', 'I', 'C', 'F', 'E' };
static const uint8_t UFT_MAGIC_HFEv3[] = { 'H', 'X', 'C', 'H', 'F', 'E', 'V', '3' };
static const uint8_t UFT_MAGIC_SCP[]  = { 'S', 'C', 'P' };
static const uint8_t UFT_MAGIC_A2R[]  = { 'A', '2', 'R', '2' };
static const uint8_t UFT_MAGIC_WOZ1[] = { 'W', 'O', 'Z', '1' };
static const uint8_t UFT_MAGIC_WOZ2[] = { 'W', 'O', 'Z', '2' };
static const uint8_t UFT_MAGIC_STX[]  = { 'R', 'S', 'P', 0x01 };
static const uint8_t UFT_MAGIC_TD0[]  = { 'T', 'D' };
static const uint8_t UFT_MAGIC_TD0A[] = { 't', 'd' };
static const uint8_t UFT_MAGIC_IMD[]  = { 'I', 'M', 'D', ' ' };

/*===========================================================================*/
/*                           LOADER PLUGIN INTERFACE                          */
/*===========================================================================*/

/**
 * HxC-style loader plugin interface for UFT
 */

struct uft_floppy;  /* Forward declaration */

typedef struct {
    const char *name;           /* Format name */
    const char *description;    /* Format description */
    const char **extensions;    /* File extensions (NULL-terminated) */
    
    /* Probe function: returns >0 if valid, 0 if maybe, <0 if invalid */
    int (*probe)(const char *filename, const uint8_t *header, size_t header_len);
    
    /* Load function: returns 0 on success */
    int (*load)(const char *filename, struct uft_floppy **floppy);
    
    /* Save function: returns 0 on success, NULL if format is read-only */
    int (*save)(const char *filename, struct uft_floppy *floppy);
    
    /* Get format capabilities */
    uint32_t (*get_caps)(void);
} uft_loader_plugin_t;

/* Loader capabilities flags */
#define UFT_LOADER_CAP_READ         0x0001
#define UFT_LOADER_CAP_WRITE        0x0002
#define UFT_LOADER_CAP_FLUX         0x0004
#define UFT_LOADER_CAP_SECTOR       0x0008
#define UFT_LOADER_CAP_PROTECTION   0x0010
#define UFT_LOADER_CAP_WEAK_BITS    0x0020
#define UFT_LOADER_CAP_VARIABLE_BR  0x0040
#define UFT_LOADER_CAP_MULTIREV     0x0080

/*===========================================================================*/
/*                           FILESYSTEM INTERFACE                             */
/*===========================================================================*/

/**
 * Filesystem manager interface (from HxC fs_manager)
 */

typedef struct {
    const char *name;           /* Filesystem name */
    int fs_id;                  /* Filesystem ID */
    
    int track_per_disk;
    int side_per_track;
    int sector_per_track;
    int sector_size;
    
    /* Operations */
    int (*mount)(void *ctx, struct uft_floppy *floppy);
    int (*unmount)(void *ctx);
    int (*read_sector)(void *ctx, int lba, uint8_t *buffer);
    int (*write_sector)(void *ctx, int lba, const uint8_t *buffer);
    int (*get_dir)(void *ctx, const char *path, void *callback, void *user);
    int (*read_file)(void *ctx, const char *path, uint8_t **data, size_t *len);
    int (*write_file)(void *ctx, const char *path, const uint8_t *data, size_t len);
} uft_filesystem_t;

/* Supported filesystem IDs */
#define UFT_FS_FAT12        1
#define UFT_FS_AMIGADOS_OFS 2
#define UFT_FS_AMIGADOS_FFS 3
#define UFT_FS_CPM          4
#define UFT_FS_FLEX         5
#define UFT_FS_PRODOS       6
#define UFT_FS_TRSDOS       7

#ifdef __cplusplus
}
#endif

#endif /* UFT_LIBFLUX_FORMATS_H */

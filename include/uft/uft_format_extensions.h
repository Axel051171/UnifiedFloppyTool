/**
 * @file uft_format_extensions.h
 * @brief Extended Format Support
 * 
 * P3-007: Format Extensions
 * 
 * Additional format support beyond core formats:
 * - Atari ST (STX, MSA)
 * - Amstrad CPC (DSK, EDSK)
 * - BBC Micro (SSD, DSD)
 * - MSX (DSK)
 * - Sam Coupe (SAD, MGT)
 * - Spectrum +3 (DSK)
 * - PC Engine (HuCard dumps)
 */

#ifndef UFT_FORMAT_EXTENSIONS_H
#define UFT_FORMAT_EXTENSIONS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format IDs
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    /* Atari ST */
    UFT_FMT_ST_RAW = 0x100,     /* Raw ST sector image */
    UFT_FMT_ST_STX,             /* Pasti STX format */
    UFT_FMT_ST_MSA,             /* MSA compressed image */
    
    /* Amstrad CPC */
    UFT_FMT_CPC_DSK = 0x200,    /* Standard DSK */
    UFT_FMT_CPC_EDSK,           /* Extended DSK */
    
    /* BBC Micro */
    UFT_FMT_BBC_SSD = 0x300,    /* Single-sided DFS */
    UFT_FMT_BBC_DSD,            /* Double-sided DFS */
    UFT_FMT_BBC_ADF,            /* ADFS */
    
    /* MSX */
    UFT_FMT_MSX_DSK = 0x400,    /* MSX-DOS DSK */
    UFT_FMT_MSX_DMK,            /* DMK format */
    
    /* Sam Coupe */
    UFT_FMT_SAM_SAD = 0x500,    /* SAD format */
    UFT_FMT_SAM_MGT,            /* MGT format */
    
    /* Spectrum */
    UFT_FMT_SPEC_DSK = 0x600,   /* +3 DSK */
    UFT_FMT_SPEC_TRD,           /* TR-DOS TRD */
    UFT_FMT_SPEC_SCL,           /* SCL archive */
    
    /* Other 8-bit */
    UFT_FMT_ORIC_DSK = 0x700,   /* Oric MFM */
    UFT_FMT_EINSTEIN,           /* Tatung Einstein */
    UFT_FMT_SHARP_MZ,           /* Sharp MZ series */
} uft_format_ext_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Atari ST Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief MSA file header
 */
typedef struct {
    uint16_t magic;             /* 0x0E0F */
    uint16_t sectors_per_track;
    uint16_t sides;             /* 0 or 1 */
    uint16_t start_track;
    uint16_t end_track;
} uft_msa_header_t;

/**
 * @brief Decompress MSA file
 */
int uft_msa_decompress(
    const uint8_t *msa_data, size_t msa_size,
    uint8_t *raw_output, size_t *raw_size);

/**
 * @brief Compress to MSA format
 */
int uft_msa_compress(
    const uint8_t *raw_data, size_t raw_size,
    int tracks, int sides, int sectors,
    uint8_t *msa_output, size_t *msa_size);

/**
 * @brief STX track header
 */
typedef struct {
    uint32_t record_size;
    uint32_t fuzzy_size;
    uint16_t sector_count;
    uint16_t flags;
    uint16_t mfm_size;
    uint8_t  track_number;
    uint8_t  track_type;
} uft_stx_track_t;

/**
 * @brief Parse STX file
 */
int uft_stx_parse(
    const uint8_t *stx_data, size_t stx_size,
    void (*track_callback)(int track, int side, 
                          const uft_stx_track_t *info,
                          const uint8_t *data, void *ctx),
    void *ctx);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Amstrad CPC / Spectrum +3 DSK
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define CPC_DSK_MAGIC "MV - CPCEMU"
#define CPC_EDSK_MAGIC "EXTENDED CPC DSK"

/**
 * @brief CPC DSK header
 */
typedef struct {
    char     magic[34];
    char     creator[14];
    uint8_t  tracks;
    uint8_t  sides;
    uint16_t track_size;        /* Standard DSK only */
    uint8_t  track_sizes[204];  /* EDSK: size/256 per track */
} uft_cpc_dsk_header_t;

/**
 * @brief CPC track info
 */
typedef struct {
    char     magic[13];         /* "Track-Info\r\n" */
    uint8_t  unused[3];
    uint8_t  track;
    uint8_t  side;
    uint8_t  unused2[2];
    uint8_t  sector_size;       /* 0-6: 128<<N */
    uint8_t  sector_count;
    uint8_t  gap3;
    uint8_t  filler;
} uft_cpc_track_info_t;

/**
 * @brief CPC sector info
 */
typedef struct {
    uint8_t  track;             /* C */
    uint8_t  side;              /* H */
    uint8_t  sector;            /* R */
    uint8_t  size;              /* N */
    uint8_t  status1;           /* FDC ST1 */
    uint8_t  status2;           /* FDC ST2 */
    uint16_t data_size;         /* EDSK only */
} uft_cpc_sector_info_t;

/**
 * @brief Load CPC DSK file
 */
int uft_cpc_dsk_load(
    const uint8_t *dsk_data, size_t dsk_size,
    void (*sector_callback)(int track, int side, int sector,
                           const uint8_t *data, size_t size,
                           uint8_t st1, uint8_t st2, void *ctx),
    void *ctx);

/**
 * @brief Create CPC DSK file
 */
int uft_cpc_dsk_create(
    uint8_t *output, size_t *output_size,
    int tracks, int sides, int sectors, int sector_size,
    const uint8_t *sector_data);

/* ═══════════════════════════════════════════════════════════════════════════════
 * BBC Micro Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief BBC DFS catalog entry
 */
typedef struct {
    char     filename[7];
    char     directory;
    uint16_t load_addr;
    uint16_t exec_addr;
    uint16_t length;
    uint8_t  start_sector;
} uft_bbc_dfs_entry_t;

/**
 * @brief Parse BBC DFS image
 */
int uft_bbc_dfs_parse(
    const uint8_t *ssd_data, size_t ssd_size,
    uft_bbc_dfs_entry_t *entries, int *entry_count,
    char *disk_title);

/**
 * @brief Extract file from BBC DFS
 */
int uft_bbc_dfs_extract(
    const uint8_t *ssd_data, size_t ssd_size,
    const char *filename,
    uint8_t *output, size_t *output_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * TR-DOS (Spectrum)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TR-DOS catalog entry
 */
typedef struct {
    char     filename[8];
    char     extension;
    uint16_t start;
    uint16_t length;
    uint8_t  sectors;
    uint8_t  first_sector;
    uint8_t  first_track;
} uft_trdos_entry_t;

/**
 * @brief Parse TR-DOS image
 */
int uft_trdos_parse(
    const uint8_t *trd_data, size_t trd_size,
    uft_trdos_entry_t *entries, int *entry_count,
    char *disk_label);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect extended format
 */
uft_format_ext_t uft_detect_format_ext(
    const uint8_t *data, size_t size,
    int *confidence);

/**
 * @brief Get format name
 */
const char* uft_format_ext_name(uft_format_ext_t format);

/**
 * @brief Get format description
 */
const char* uft_format_ext_description(uft_format_ext_t format);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_EXTENSIONS_H */

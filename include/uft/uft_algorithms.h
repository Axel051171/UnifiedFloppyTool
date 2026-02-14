/**
 * @file uft_algorithms.h
 * @brief Specialized Algorithms for Disk Analysis
 * 
 * @version 4.2.0
 * @date 2026-01-03
 * 
 * ALGORITHMS:
 * - Rabin-Karp rolling hash for pattern matching in flux data
 * - Human68K FAT variant for Sharp X68000
 * - Tarbell CP/M format support
 * - Nintendo GameCube disk format
 * 
 * USAGE:
 * ```c
 * // Find sync patterns in flux stream
 * uft_rk_context_t ctx;
 * uft_rk_init(&ctx, pattern, pattern_len);
 * size_t matches[100];
 * int count = uft_rk_search(&ctx, flux_data, flux_len, matches, 100);
 * ```
 */

#ifndef UFT_ALGORITHMS_H
#define UFT_ALGORITHMS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * RABIN-KARP PATTERN MATCHING
 * 
 * Efficient rolling hash algorithm for finding patterns in flux streams.
 * Optimal for finding sync marks, address marks, and data patterns.
 * 
 * Time: O(n + m) average, O(nm) worst case
 * Space: O(1)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_RK_PRIME        0x01000193  /* FNV prime */
#define UFT_RK_BASE         256

typedef struct {
    uint64_t pattern_hash;      /* Hash of pattern */
    uint64_t high_pow;          /* base^(m-1) mod prime */
    const uint8_t *pattern;     /* Pattern to find */
    size_t pattern_len;         /* Pattern length */
    uint64_t prime;             /* Prime modulus */
} uft_rk_context_t;

/**
 * @brief Initialize Rabin-Karp context
 * @param ctx Context to initialize
 * @param pattern Pattern to search for
 * @param pattern_len Pattern length
 */
void uft_rk_init(uft_rk_context_t *ctx,
                 const uint8_t *pattern,
                 size_t pattern_len);

/**
 * @brief Initialize with custom prime
 */
void uft_rk_init_custom(uft_rk_context_t *ctx,
                        const uint8_t *pattern,
                        size_t pattern_len,
                        uint64_t prime);

/**
 * @brief Search for pattern in data
 * @param ctx Initialized context
 * @param data Data to search
 * @param data_len Data length
 * @param matches Output array for match positions
 * @param max_matches Maximum matches to find
 * @return Number of matches found
 */
int uft_rk_search(const uft_rk_context_t *ctx,
                  const uint8_t *data,
                  size_t data_len,
                  size_t *matches,
                  int max_matches);

/**
 * @brief Search for multiple patterns simultaneously
 * @param patterns Array of patterns
 * @param pattern_lens Array of pattern lengths
 * @param pattern_count Number of patterns
 * @param data Data to search
 * @param data_len Data length
 * @param matches Output: matches[i] = position, pattern_ids[i] = which pattern
 * @param pattern_ids Output: which pattern matched
 * @param max_matches Maximum matches
 * @return Total matches found
 */
int uft_rk_search_multi(const uint8_t **patterns,
                        const size_t *pattern_lens,
                        int pattern_count,
                        const uint8_t *data,
                        size_t data_len,
                        size_t *matches,
                        int *pattern_ids,
                        int max_matches);

/**
 * @brief Compute rolling hash for single byte update
 * @param ctx Context
 * @param old_hash Current hash
 * @param old_byte Byte leaving window
 * @param new_byte Byte entering window
 * @return New hash value
 */
uint64_t uft_rk_roll(const uft_rk_context_t *ctx,
                     uint64_t old_hash,
                     uint8_t old_byte,
                     uint8_t new_byte);

/* ═══════════════════════════════════════════════════════════════════════════════
 * HUMAN68K FAT - Sharp X68000
 * 
 * Modified FAT filesystem used by Sharp X68000 (Human68K OS).
 * Based on FAT12/FAT16 with Japanese filename support and different
 * sector sizes.
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_HUMAN68K_SECTOR_SIZE    1024    /* Default sector size */
#define UFT_HUMAN68K_MAX_FILENAME   18      /* 8.3 + attributes */

/* Human68K Boot Sector */
#pragma pack(push, 1)
typedef struct {
    uint8_t  jump[2];           /* Jump instruction (0x60 0x??) */
    char     oem_name[16];      /* OEM name ("Hudson soft 2.00") */
    uint16_t bytes_per_sector;  /* 256, 512, or 1024 */
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_type;        /* 0xFE = 2HD, 0xF9 = 2DD */
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint16_t hidden_sectors;
    /* Extended for large disks */
    uint32_t total_sectors_32;
    /* Boot code follows */
} uft_human68k_boot_t;
#pragma pack(pop)

/* Human68K Directory Entry (32 bytes) */
#pragma pack(push, 1)
typedef struct {
    char     filename[8];       /* Filename (Shift-JIS) */
    char     extension[3];      /* Extension */
    uint8_t  attributes;        /* File attributes */
    uint8_t  reserved[10];      /* Reserved (may contain extended info) */
    uint16_t time;              /* Modified time */
    uint16_t date;              /* Modified date */
    uint16_t first_cluster;     /* First cluster */
    uint32_t file_size;         /* File size */
} uft_human68k_dirent_t;
#pragma pack(pop)

/* Human68K Volume */
typedef struct {
    uft_human68k_boot_t boot;
    
    uint8_t  *fat;              /* File allocation table */
    size_t   fat_size;
    
    uft_human68k_dirent_t *root; /* Root directory */
    int      root_entries;
    
    uint8_t  *data;             /* Raw image data */
    size_t   data_size;
    
    /* Calculated */
    int      fat_type;          /* 12 or 16 */
    uint32_t cluster_size;
    uint32_t data_start_sector;
} uft_human68k_volume_t;

/**
 * @brief Detect Human68K filesystem
 */
int uft_human68k_detect(const uint8_t *data, size_t size);

/**
 * @brief Mount Human68K volume
 */
int uft_human68k_mount(const uint8_t *data, size_t size,
                       uft_human68k_volume_t *volume);

/**
 * @brief List root directory
 */
int uft_human68k_list_root(const uft_human68k_volume_t *volume,
                           uft_human68k_dirent_t *entries,
                           int max_entries);

/**
 * @brief Extract file
 */
int uft_human68k_extract_file(const uft_human68k_volume_t *volume,
                              const char *filename,
                              uint8_t **data, size_t *size);

/**
 * @brief Free volume resources
 */
void uft_human68k_free(uft_human68k_volume_t *volume);

/* ═══════════════════════════════════════════════════════════════════════════════
 * TARBELL CP/M FORMAT
 * 
 * Early CP/M disk format used by Tarbell Electronics floppy controllers.
 * Single-sided, single-density (SSSD) 8-inch format.
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_TARBELL_SECTOR_SIZE     128
#define UFT_TARBELL_SECTORS_TRACK   26
#define UFT_TARBELL_TRACKS          77
#define UFT_TARBELL_TOTAL_SIZE      (77 * 26 * 128)  /* 256,256 bytes */

/* CP/M Directory Entry (32 bytes) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  user_number;       /* User number (0-15, 0xE5=deleted) */
    char     filename[8];       /* Filename (space-padded) */
    char     extension[3];      /* Extension (high bits = flags) */
    uint8_t  extent_low;        /* Extent counter low */
    uint8_t  reserved1;
    uint8_t  extent_high;       /* Extent counter high */
    uint8_t  record_count;      /* Records in this extent (0-128) */
    uint8_t  allocation[16];    /* Allocation blocks */
} uft_cpm_dirent_t;
#pragma pack(pop)

/* Tarbell Disk Geometry */
typedef struct {
    int      tracks;
    int      sectors_per_track;
    int      sector_size;
    int      block_size;        /* Allocation block size */
    int      dir_blocks;        /* Directory size in blocks */
    int      reserved_tracks;   /* System tracks */
    bool     single_sided;
    int      skew;              /* Sector skew factor */
} uft_tarbell_geometry_t;

/* Standard Tarbell geometries */
extern const uft_tarbell_geometry_t UFT_TARBELL_SSSD_8;  /* 8" SSSD */
extern const uft_tarbell_geometry_t UFT_TARBELL_DSDD_8;  /* 8" DSDD */
extern const uft_tarbell_geometry_t UFT_TARBELL_SSDD_5;  /* 5.25" SSDD */

typedef struct {
    uft_tarbell_geometry_t geometry;
    
    uint8_t  *data;
    size_t   data_size;
    
    uft_cpm_dirent_t *directory;
    int      dir_entries;
    
    uint8_t  *allocation_map;   /* Block allocation bitmap */
    int      total_blocks;
    int      used_blocks;
} uft_tarbell_disk_t;

/**
 * @brief Detect Tarbell CP/M format
 */
int uft_tarbell_detect(const uint8_t *data, size_t size);

/**
 * @brief Open Tarbell disk image
 */
int uft_tarbell_open(const uint8_t *data, size_t size,
                     const uft_tarbell_geometry_t *geometry,
                     uft_tarbell_disk_t *disk);

/**
 * @brief List files in directory
 */
int uft_tarbell_list_files(const uft_tarbell_disk_t *disk,
                           uft_cpm_dirent_t *entries,
                           int max_entries);

/**
 * @brief Extract file to buffer
 */
int uft_tarbell_extract(const uft_tarbell_disk_t *disk,
                        const char *filename,
                        uint8_t **data, size_t *size);

/**
 * @brief Read logical sector (with de-skewing)
 */
int uft_tarbell_read_sector(const uft_tarbell_disk_t *disk,
                            int track, int sector,
                            uint8_t *buffer);

/**
 * @brief Free disk resources
 */
void uft_tarbell_free(uft_tarbell_disk_t *disk);

/* ═══════════════════════════════════════════════════════════════════════════════
 * NINTENDO GAMECUBE DISK FORMAT
 * 
 * Nintendo GameCube optical disc format (GCM/ISO).
 * Uses custom CLV encoding with Nintendo's proprietary filesystem.
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_GCM_MAGIC               0xC2339F3D  /* GameCube magic */
#define UFT_GCM_SECTOR_SIZE         2048
#define UFT_GCM_DISK_SIZE           1459978240  /* 1.4 GB */

/* GameCube Disk Header (at offset 0) */
#pragma pack(push, 1)
typedef struct {
    uint32_t console_id;        /* 'G' for GameCube */
    char     game_code[2];      /* Game code */
    char     region_code;       /* Region: 'E'=US, 'P'=EU, 'J'=JP */
    char     maker_code[2];     /* Maker/publisher code */
    uint8_t  disc_id;           /* Disc number */
    uint8_t  version;           /* Version */
    uint8_t  audio_streaming;   /* Audio streaming flag */
    uint8_t  stream_buffer_size;
    uint8_t  unused1[14];
    uint32_t wii_magic;         /* 0x5D1C9EA3 for Wii */
    uint32_t gc_magic;          /* 0xC2339F3D for GC */
    char     game_name[992];    /* Game title (null-terminated) */
    /* Debug info follows... */
} uft_gcm_header_t;
#pragma pack(pop)

/* Disc Header Info (at offset 0x420) */
#pragma pack(push, 1)
typedef struct {
    uint32_t debug_monitor_offset;
    uint32_t debug_monitor_load_addr;
    uint8_t  unused[24];
    uint32_t dol_offset;        /* Main executable offset */
    uint32_t fst_offset;        /* File system table offset */
    uint32_t fst_size;          /* FST size */
    uint32_t fst_max_size;      /* Maximum FST size */
    uint32_t user_position;
    uint32_t user_size;
    uint8_t  unused2[4];
} uft_gcm_disc_info_t;
#pragma pack(pop)

/* FST Entry (12 bytes) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  flags;             /* Bit 0: 0=file, 1=directory */
    uint8_t  name_offset[3];    /* 24-bit offset into string table */
    uint32_t offset_or_parent;  /* File: offset, Dir: parent index */
    uint32_t size_or_next;      /* File: size, Dir: next entry index */
} uft_gcm_fst_entry_t;
#pragma pack(pop)

typedef struct {
    char     name[256];
    bool     is_directory;
    uint32_t offset;
    uint32_t size;
    uint32_t parent;
} uft_gcm_file_t;

typedef struct {
    uft_gcm_header_t header;
    uft_gcm_disc_info_t disc_info;
    
    /* File System Table */
    uft_gcm_fst_entry_t *fst;
    int      fst_entries;
    char     *string_table;
    
    /* File cache */
    uft_gcm_file_t *files;
    int      file_count;
    
    /* Raw data */
    uint8_t  *data;
    size_t   data_size;
} uft_gcm_disc_t;

/**
 * @brief Detect GameCube disc image
 */
int uft_gcm_detect(const uint8_t *data, size_t size);

/**
 * @brief Open GameCube disc image
 */
int uft_gcm_open(const uint8_t *data, size_t size, uft_gcm_disc_t *disc);

/**
 * @brief Get disc information
 */
void uft_gcm_info(const uft_gcm_disc_t *disc);

/**
 * @brief List all files
 */
int uft_gcm_list_files(const uft_gcm_disc_t *disc,
                       uft_gcm_file_t *files, int max_files);

/**
 * @brief Find file by path
 */
int uft_gcm_find_file(const uft_gcm_disc_t *disc,
                      const char *path,
                      uft_gcm_file_t *file);

/**
 * @brief Extract file
 */
int uft_gcm_extract_file(const uft_gcm_disc_t *disc,
                         const char *path,
                         uint8_t **data, size_t *size);

/**
 * @brief Extract DOL (main executable)
 */
int uft_gcm_extract_dol(const uft_gcm_disc_t *disc,
                        uint8_t **data, size_t *size);

/**
 * @brief Extract boot.bin (apploader)
 */
int uft_gcm_extract_apploader(const uft_gcm_disc_t *disc,
                              uint8_t **data, size_t *size);

/**
 * @brief Free disc resources
 */
void uft_gcm_free(uft_gcm_disc_t *disc);

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADDITIONAL ALGORITHMS
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Boyer-Moore pattern search (faster for long patterns)
 */
int uft_bm_search(const uint8_t *pattern, size_t pattern_len,
                  const uint8_t *data, size_t data_len,
                  size_t *matches, int max_matches);

/**
 * @brief Knuth-Morris-Pratt pattern search (no hash collisions)
 */
int uft_kmp_search(const uint8_t *pattern, size_t pattern_len,
                   const uint8_t *data, size_t data_len,
                   size_t *matches, int max_matches);

/**
 * @brief Find repeated sequences (for compression analysis)
 */
typedef struct {
    size_t offset;
    size_t length;
    int    count;
} uft_repeat_t;

int uft_find_repeats(const uint8_t *data, size_t data_len,
                     size_t min_length,
                     uft_repeat_t *repeats, int max_repeats);

/**
 * @brief Calculate data entropy
 */
double uft_entropy(const uint8_t *data, size_t length);

/**
 * @brief Detect data compression type
 */
typedef enum {
    UFT_COMPRESS_NONE,
    UFT_COMPRESS_RLE,
    UFT_COMPRESS_LZ,
    UFT_COMPRESS_HUFFMAN,
    UFT_COMPRESS_DEFLATE,
    UFT_COMPRESS_UNKNOWN
} uft_compress_type_t;

uft_compress_type_t uft_detect_compression(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ALGORITHMS_H */

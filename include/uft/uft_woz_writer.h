/**
 * @file uft_woz_writer.h
 * @brief WOZ 2.0 Disk Image Writer for Apple II
 * 
 * P2-007: WOZ Writer
 * 
 * Creates WOZ 2.0 disk images supporting:
 * - 5.25" floppy (35 tracks, quarter-track support)
 * - 3.5" floppy (80 tracks)
 * - Flux-level timing data
 * - Copy protection preservation
 * - META chunk for disk info
 */

#ifndef UFT_WOZ_WRITER_H
#define UFT_WOZ_WRITER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * WOZ Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define WOZ_MAGIC           "WOZ2"
#define WOZ_VERSION         2
#define WOZ_HEADER_SIZE     12

/* Chunk IDs */
#define WOZ_CHUNK_INFO      0x4F464E49  /* "INFO" */
#define WOZ_CHUNK_TMAP      0x50414D54  /* "TMAP" */
#define WOZ_CHUNK_TRKS      0x534B5254  /* "TRKS" */
#define WOZ_CHUNK_FLUX      0x58554C46  /* "FLUX" */
#define WOZ_CHUNK_WRIT      0x54495257  /* "WRIT" */
#define WOZ_CHUNK_META      0x4154454D  /* "META" */

/* Disk types */
#define WOZ_DISK_525        1   /* 5.25" floppy */
#define WOZ_DISK_35         2   /* 3.5" floppy */

/* Track limits */
#define WOZ_MAX_TRACKS_525  160  /* 40 tracks * 4 quarter tracks */
#define WOZ_MAX_TRACKS_35   160  /* 80 tracks * 2 sides */
#define WOZ_TMAP_SIZE       160

/* Timing */
#define WOZ_BIT_TIME_NS     125  /* 125ns per bit cell (8MHz) */
#define WOZ_TRACK_BITS_525  51200  /* ~6400 bytes typical */
#define WOZ_TRACK_BITS_35   100000 /* ~12500 bytes */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief WOZ INFO chunk
 */
typedef struct {
    uint8_t  version;           /* WOZ version (2) */
    uint8_t  disk_type;         /* 1=5.25", 2=3.5" */
    uint8_t  write_protected;   /* 1=protected */
    uint8_t  synchronized;      /* 1=cross-track synchronized */
    uint8_t  cleaned;           /* 1=MC3470 cleaned */
    char     creator[32];       /* Creator string */
    uint8_t  disk_sides;        /* 1 or 2 */
    uint8_t  boot_sector_format;/* 0=unknown, 1=16-sector, 2=13-sector, 3=both */
    uint8_t  optimal_bit_timing;/* In 125ns units (default 32 = 4us) */
    uint16_t compatible_hardware;/* Hardware compatibility flags */
    uint16_t required_ram;      /* Required RAM in KB */
    uint16_t largest_track;     /* Largest track block count */
    uint16_t flux_block;        /* FLUX chunk block start */
    uint16_t largest_flux_track;/* Largest flux track */
} woz_info_t;

/**
 * @brief WOZ track entry (TRKS chunk)
 */
typedef struct {
    uint16_t start_block;       /* Starting 512-byte block */
    uint16_t block_count;       /* Number of blocks */
    uint32_t bit_count;         /* Number of bits in track */
} woz_trk_entry_t;

/**
 * @brief Writer configuration
 */
typedef struct {
    uint8_t disk_type;          /* WOZ_DISK_525 or WOZ_DISK_35 */
    uint8_t disk_sides;         /* 1 or 2 */
    uint8_t boot_format;        /* 0=unknown, 1=16-sector, 2=13-sector */
    uint8_t bit_timing;         /* Bit timing (default 32) */
    bool write_protected;
    bool synchronized;
    char creator[32];
    
    /* Track options */
    bool include_quarter_tracks;
    bool include_flux;
    int track_count;            /* Tracks to write (35 or 80) */
    
    /* META info */
    char *title;
    char *subtitle;
    char *publisher;
    char *developer;
    char *copyright;
    char *version;
    char *language;
    char *requires_machine;
    char *notes;
} woz_writer_config_t;

#define WOZ_WRITER_CONFIG_DEFAULT { \
    .disk_type = WOZ_DISK_525, \
    .disk_sides = 1, \
    .boot_format = 1, \
    .bit_timing = 32, \
    .write_protected = false, \
    .synchronized = false, \
    .creator = "UFT 3.8.6", \
    .include_quarter_tracks = false, \
    .include_flux = false, \
    .track_count = 35, \
    .title = NULL, \
    .subtitle = NULL, \
    .publisher = NULL, \
    .developer = NULL, \
    .copyright = NULL, \
    .version = NULL, \
    .language = NULL, \
    .requires_machine = NULL, \
    .notes = NULL \
}

/**
 * @brief Track data for writing
 */
typedef struct {
    int track_number;           /* 0-based track number */
    int quarter_track;          /* 0-3 for quarter tracks */
    uint8_t *bit_data;          /* Raw bit stream */
    size_t bit_count;           /* Number of bits */
    uint32_t *flux_data;        /* Optional flux timing (ns) */
    size_t flux_count;          /* Number of flux transitions */
} woz_track_data_t;

/**
 * @brief Writer context
 */
typedef struct woz_writer woz_writer_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create WOZ writer
 */
woz_writer_t* woz_writer_create(const woz_writer_config_t *config);

/**
 * @brief Destroy writer
 */
void woz_writer_destroy(woz_writer_t *writer);

/**
 * @brief Add track data
 */
int woz_writer_add_track(woz_writer_t *writer, const woz_track_data_t *track);

/**
 * @brief Write WOZ file
 */
int woz_writer_write(woz_writer_t *writer, const char *path);

/**
 * @brief Write WOZ to buffer
 */
int woz_writer_write_buffer(woz_writer_t *writer, uint8_t *buffer, size_t *size);

/**
 * @brief Calculate CRC32 for WOZ
 */
uint32_t woz_crc32(const uint8_t *data, size_t size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Conversion Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert NIB track to WOZ bit stream
 */
int woz_from_nib_track(
    const uint8_t *nib_data,
    size_t nib_size,
    uint8_t *bit_data,
    size_t *bit_count);

/**
 * @brief Convert DSK/DO sector data to WOZ
 */
int woz_from_dsk_track(
    const uint8_t *sector_data,
    int track_number,
    bool dos_order,
    uint8_t *bit_data,
    size_t *bit_count);

/**
 * @brief Encode 6-and-2 GCR for Apple II
 */
void woz_gcr_encode_6and2(const uint8_t *data, uint8_t *gcr);

/**
 * @brief Write Apple II address field
 */
int woz_write_address_field(
    uint8_t *output,
    int volume,
    int track,
    int sector);

/**
 * @brief Write Apple II data field
 */
int woz_write_data_field(
    uint8_t *output,
    const uint8_t *sector_data);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WOZ_WRITER_H */

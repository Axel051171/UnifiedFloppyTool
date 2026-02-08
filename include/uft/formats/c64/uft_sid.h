/**
 * @file uft_sid.h
 * @brief SID Music File Format Support
 * 
 * Complete SID/PSID/RSID format handling:
 * - Parse SID header and metadata
 * - Extract player and music data
 * - Create SID files from components
 * - Support for PSID v1-4 and RSID
 * 
 * SID Format:
 * - 124/126-byte header (PSID/RSID magic, version, load/init/play addresses)
 * - Optional load address (2 bytes if header load = 0)
 * - C64 program data
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_SID_H
#define UFT_SID_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** SID magic signatures */
#define SID_MAGIC_PSID          "PSID"
#define SID_MAGIC_RSID          "RSID"
#define SID_MAGIC_LEN           4

/** Header sizes */
#define SID_HEADER_V1           0x0076      /**< 118 bytes */
#define SID_HEADER_V2           0x007C      /**< 124 bytes */

/** SID versions */
#define SID_VERSION_1           1
#define SID_VERSION_2           2
#define SID_VERSION_3           3
#define SID_VERSION_4           4

/** Maximum string lengths */
#define SID_STRING_LEN          32

/** Clock flags (v2+) */
#define SID_CLOCK_UNKNOWN       0x00
#define SID_CLOCK_PAL           0x01
#define SID_CLOCK_NTSC          0x02
#define SID_CLOCK_ANY           0x03

/** SID model flags (v2+) */
#define SID_MODEL_UNKNOWN       0x00
#define SID_MODEL_6581          0x01
#define SID_MODEL_8580          0x02
#define SID_MODEL_ANY           0x03

/** Flags (v2+) */
#define SID_FLAG_MUSPLAYER      0x01        /**< Built-in music player */
#define SID_FLAG_PSID_SPECIFIC  0x02        /**< PSID-specific (v2) */
#define SID_FLAG_BASIC          0x02        /**< BASIC flag (RSID) */

/** Second/Third SID address constants */
#define SID_ADDR_NONE           0x00
#define SID_ADDR_D420           0x01
#define SID_ADDR_D440           0x02
#define SID_ADDR_D460           0x03
#define SID_ADDR_D480           0x04
#define SID_ADDR_D4A0           0x05
#define SID_ADDR_D4C0           0x06
#define SID_ADDR_D4E0           0x07
#define SID_ADDR_DE00           0x08
#define SID_ADDR_DE20           0x09
#define SID_ADDR_DE40           0x0A
#define SID_ADDR_DE60           0x0B
#define SID_ADDR_DE80           0x0C
#define SID_ADDR_DEA0           0x0D
#define SID_ADDR_DEC0           0x0E
#define SID_ADDR_DEE0           0x0F
#define SID_ADDR_DF00           0x10

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief SID file type
 */
typedef enum {
    SID_TYPE_PSID = 0,          /**< PlaySID format */
    SID_TYPE_RSID = 1           /**< RealSID format */
} sid_type_t;

/**
 * @brief SID header structure
 */
typedef struct {
    char        magic[4];           /**< "PSID" or "RSID" */
    uint16_t    version;            /**< Version (1-4) */
    uint16_t    data_offset;        /**< Offset to C64 data */
    uint16_t    load_address;       /**< Load address (0 = use first 2 bytes) */
    uint16_t    init_address;       /**< Init routine address */
    uint16_t    play_address;       /**< Play routine address (0 = IRQ) */
    uint16_t    songs;              /**< Number of songs */
    uint16_t    start_song;         /**< Default song (1-based) */
    uint32_t    speed;              /**< Speed flags (bit per song) */
    char        name[32];           /**< Song name */
    char        author[32];         /**< Author name */
    char        released[32];       /**< Release info (copyright) */
    /* v2+ fields */
    uint16_t    flags;              /**< Flags */
    uint8_t     start_page;         /**< Relocation start page */
    uint8_t     page_length;        /**< Relocation page length */
    uint8_t     second_sid;         /**< Second SID address */
    uint8_t     third_sid;          /**< Third SID address */
} sid_header_t;

/**
 * @brief SID file info
 */
typedef struct {
    sid_type_t  type;               /**< PSID or RSID */
    uint16_t    version;            /**< Version number */
    char        name[33];           /**< Song name (null terminated) */
    char        author[33];         /**< Author name */
    char        released[33];       /**< Release info */
    uint16_t    load_address;       /**< Actual load address */
    uint16_t    init_address;       /**< Init address */
    uint16_t    play_address;       /**< Play address */
    uint16_t    songs;              /**< Number of songs */
    uint16_t    start_song;         /**< Default song */
    uint8_t     clock;              /**< Clock (PAL/NTSC) */
    uint8_t     sid_model;          /**< SID model (6581/8580) */
    size_t      data_size;          /**< C64 data size */
    uint16_t    end_address;        /**< End address */
} sid_info_t;

/**
 * @brief SID image context
 */
typedef struct {
    uint8_t     *data;              /**< SID file data */
    size_t      size;               /**< Total file size */
    sid_header_t header;            /**< Parsed header */
    uint8_t     *c64_data;          /**< Pointer to C64 data */
    size_t      c64_data_size;      /**< C64 data size */
    uint16_t    actual_load_addr;   /**< Actual load address */
    bool        owns_data;          /**< true if we allocated */
} sid_image_t;

/* ============================================================================
 * API Functions - Image Management
 * ============================================================================ */

/**
 * @brief Open SID from data
 * @param data SID file data
 * @param size Data size
 * @param image Output image context
 * @return 0 on success
 */
int sid_open(const uint8_t *data, size_t size, sid_image_t *image);

/**
 * @brief Load SID from file
 * @param filename File path
 * @param image Output image context
 * @return 0 on success
 */
int sid_load(const char *filename, sid_image_t *image);

/**
 * @brief Save SID to file
 * @param image SID image
 * @param filename Output path
 * @return 0 on success
 */
int sid_save(const sid_image_t *image, const char *filename);

/**
 * @brief Close SID image
 * @param image Image to close
 */
void sid_close(sid_image_t *image);

/**
 * @brief Validate SID format
 * @param data SID data
 * @param size Data size
 * @return true if valid
 */
bool sid_validate(const uint8_t *data, size_t size);

/**
 * @brief Detect if data is SID format
 * @param data Data to check
 * @param size Data size
 * @return true if SID
 */
bool sid_detect(const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - SID Info
 * ============================================================================ */

/**
 * @brief Get SID info
 * @param image SID image
 * @param info Output info
 * @return 0 on success
 */
int sid_get_info(const sid_image_t *image, sid_info_t *info);

/**
 * @brief Get song name
 * @param image SID image
 * @param name Output buffer (33 bytes min)
 */
void sid_get_name(const sid_image_t *image, char *name);

/**
 * @brief Get author name
 * @param image SID image
 * @param author Output buffer (33 bytes min)
 */
void sid_get_author(const sid_image_t *image, char *author);

/**
 * @brief Get release info
 * @param image SID image
 * @param released Output buffer (33 bytes min)
 */
void sid_get_released(const sid_image_t *image, char *released);

/**
 * @brief Check if song uses CIA timer
 * @param image SID image
 * @param song Song number (0-based)
 * @return true if CIA timer, false if VBI
 */
bool sid_song_uses_cia(const sid_image_t *image, int song);

/* ============================================================================
 * API Functions - Data Extraction
 * ============================================================================ */

/**
 * @brief Get C64 program data
 * @param image SID image
 * @param data Output: pointer to data
 * @param size Output: data size
 * @return 0 on success
 */
int sid_get_c64_data(const sid_image_t *image, const uint8_t **data, size_t *size);

/**
 * @brief Extract as PRG file
 * @param image SID image
 * @param prg_data Output buffer
 * @param max_size Maximum size
 * @param extracted Output: bytes extracted
 * @return 0 on success
 */
int sid_extract_prg(const sid_image_t *image, uint8_t *prg_data,
                    size_t max_size, size_t *extracted);

/**
 * @brief Save as PRG file
 * @param image SID image
 * @param filename Output filename
 * @return 0 on success
 */
int sid_save_prg(const sid_image_t *image, const char *filename);

/* ============================================================================
 * API Functions - SID Creation
 * ============================================================================ */

/**
 * @brief Create new SID file
 * @param type SID type (PSID/RSID)
 * @param version Version (1-4)
 * @param image Output image
 * @return 0 on success
 */
int sid_create(sid_type_t type, uint16_t version, sid_image_t *image);

/**
 * @brief Set SID metadata
 * @param image SID image
 * @param name Song name
 * @param author Author name
 * @param released Release info
 */
void sid_set_metadata(sid_image_t *image, const char *name,
                      const char *author, const char *released);

/**
 * @brief Set SID addresses
 * @param image SID image
 * @param load Load address
 * @param init Init address
 * @param play Play address
 */
void sid_set_addresses(sid_image_t *image, uint16_t load,
                       uint16_t init, uint16_t play);

/**
 * @brief Set song count
 * @param image SID image
 * @param songs Number of songs
 * @param start_song Default song (1-based)
 */
void sid_set_songs(sid_image_t *image, uint16_t songs, uint16_t start_song);

/**
 * @brief Add C64 data
 * @param image SID image
 * @param data C64 program data
 * @param size Data size
 * @return 0 on success
 */
int sid_set_data(sid_image_t *image, const uint8_t *data, size_t size);

/**
 * @brief Create SID from PRG
 * @param prg_data PRG data (with load address)
 * @param prg_size PRG size
 * @param name Song name
 * @param author Author
 * @param init Init address (0 = use load address)
 * @param play Play address
 * @param image Output image
 * @return 0 on success
 */
int sid_from_prg(const uint8_t *prg_data, size_t prg_size,
                 const char *name, const char *author,
                 uint16_t init, uint16_t play, sid_image_t *image);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get clock name
 * @param clock Clock flags
 * @return Clock name string
 */
const char *sid_clock_name(uint8_t clock);

/**
 * @brief Get SID model name
 * @param model Model flags
 * @return Model name string
 */
const char *sid_model_name(uint8_t model);

/**
 * @brief Get second/third SID address
 * @param addr Address code
 * @return Actual address (e.g., 0xD420)
 */
uint16_t sid_decode_address(uint8_t addr);

/**
 * @brief Print SID info
 * @param image SID image
 * @param fp Output file
 */
void sid_print_info(const sid_image_t *image, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SID_H */

/**
 * @file uft_a2r.h
 * @brief UnifiedFloppyTool - Applesauce A2R Format Support
 * @version 3.1.4.007
 *
 * Complete A2R (Applesauce 2.0 Raw) format implementation.
 * Supports reading, writing, editing, and validation.
 *
 * Sources analyzed:
 * - a2rchery.py by 4am (2018)
 *
 * A2R Format Specification:
 * - Chunk-based format similar to WOZ
 * - Stores raw flux timing data
 * - Support for 5.25" and 3.5" disks
 * - Metadata and capture information
 */

#ifndef UFT_A2R_H
#define UFT_A2R_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * A2R Format Constants
 *============================================================================*/

/** A2R magic signature */
#define UFT_A2R_SIGNATURE           "A2R2"
#define UFT_A2R_SIGNATURE_LEN       4

/** A2R header markers */
#define UFT_A2R_MARKER_FF           0xFF
#define UFT_A2R_MARKER_LF_CR_LF     "\x0A\x0D\x0A"

/** A2R chunk IDs */
#define UFT_A2R_CHUNK_INFO          "INFO"
#define UFT_A2R_CHUNK_STRM          "STRM"
#define UFT_A2R_CHUNK_META          "META"

/** A2R disk types */
typedef enum {
    UFT_A2R_DISK_525 = 1,         /**< 5.25" floppy */
    UFT_A2R_DISK_35  = 2          /**< 3.5" floppy */
} uft_a2r_disk_type_t;

/** A2R capture types */
typedef enum {
    UFT_A2R_CAPTURE_TIMING  = 1,  /**< Timing capture */
    UFT_A2R_CAPTURE_BITS    = 2,  /**< Bits capture */
    UFT_A2R_CAPTURE_XTIMING = 3   /**< Extended timing capture */
} uft_a2r_capture_type_t;

/** A2R language codes */
static const char *uft_a2r_languages[] = {
    "English", "Spanish", "French", "German", "Chinese",
    "Japanese", "Italian", "Dutch", "Portuguese", "Danish",
    "Finnish", "Norwegian", "Swedish", "Russian", "Polish",
    "Turkish", "Arabic", "Thai", "Czech", "Hungarian",
    "Catalan", "Croatian", "Greek", "Hebrew", "Romanian",
    "Slovak", "Ukrainian", "Indonesian", "Malay", "Vietnamese",
    "Other"
};
#define UFT_A2R_LANGUAGE_COUNT 31

/** A2R RAM requirements */
static const char *uft_a2r_requires_ram[] = {
    "16K", "24K", "32K", "48K", "64K", "128K", "256K",
    "512K", "768K", "1M", "1.25M", "1.5M+", "Unknown"
};
#define UFT_A2R_RAM_COUNT 13

/** A2R machine requirements */
static const char *uft_a2r_requires_machine[] = {
    "2", "2+", "2e", "2c", "2e+", "2gs", "2c+", "3", "3+"
};
#define UFT_A2R_MACHINE_COUNT 9

/*============================================================================
 * A2R File Structures
 *============================================================================*/

/**
 * @brief A2R file header (8 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t  signature[4];        /**< "A2R2" */
    uint8_t  marker_ff;           /**< 0xFF */
    uint8_t  marker_lf_cr_lf[3];  /**< 0x0A 0x0D 0x0A */
} uft_a2r_header_t;

/**
 * @brief A2R chunk header (8 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t  id[4];               /**< Chunk ID (INFO, STRM, META) */
    uint32_t size;                /**< Chunk size (little-endian) */
} uft_a2r_chunk_header_t;

/**
 * @brief A2R INFO chunk (36 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t  version;             /**< Format version (should be 1) */
    char     creator[32];         /**< Creator string (UTF-8, space-padded) */
    uint8_t  disk_type;           /**< Disk type (1=5.25", 2=3.5") */
    uint8_t  write_protected;     /**< Write protected flag */
    uint8_t  synchronized;        /**< Track synchronized flag */
} uft_a2r_info_t;

/**
 * @brief A2R STRM flux record header
 */
typedef struct __attribute__((packed)) {
    uint8_t  location;            /**< Track location (0-159 in 0.25 steps) */
    uint8_t  capture_type;        /**< Capture type (1=timing, 2=bits, 3=xtiming) */
    uint32_t data_length;         /**< Length of flux data */
    uint32_t tick_count;          /**< Number of ticks in this capture */
} uft_a2r_strm_record_t;

/*============================================================================
 * A2R Runtime Structures
 *============================================================================*/

/**
 * @brief A2R flux record (parsed)
 */
typedef struct {
    uint8_t  location;            /**< Track location (0.25 track units) */
    uint8_t  capture_type;        /**< Capture type */
    uint32_t tick_count;          /**< Tick count */
    uint8_t  *data;               /**< Flux data */
    size_t   data_size;           /**< Data size */
} uft_a2r_flux_record_t;

/**
 * @brief A2R track (can have multiple flux records)
 */
typedef struct {
    uint8_t  location;            /**< Track location */
    uft_a2r_flux_record_t *records; /**< Flux records */
    size_t   record_count;        /**< Number of records */
} uft_a2r_track_t;

/**
 * @brief A2R metadata entry
 */
typedef struct {
    char     *key;                /**< Metadata key */
    char     **values;            /**< Array of values (can be multiple) */
    size_t   value_count;         /**< Number of values */
} uft_a2r_meta_entry_t;

/**
 * @brief A2R file context
 */
typedef struct {
    /* File header */
    uint8_t  version;
    
    /* INFO chunk */
    char     creator[33];         /**< Creator string (null-terminated) */
    uint8_t  disk_type;           /**< Disk type */
    bool     write_protected;
    bool     synchronized;
    
    /* STRM chunk */
    uft_a2r_track_t *tracks;      /**< Track array */
    size_t   track_count;         /**< Number of tracks */
    
    /* META chunk */
    uft_a2r_meta_entry_t *meta;   /**< Metadata entries */
    size_t   meta_count;          /**< Number of entries */
    
    /* Raw data reference */
    uint8_t  *raw_data;           /**< Original file data */
    size_t   raw_size;            /**< Original file size */
} uft_a2r_file_t;

/*============================================================================
 * A2R Validation Functions
 *============================================================================*/

/**
 * @brief Validate A2R header
 * @param data File data
 * @param size Data size
 * @return true if valid A2R header
 */
static inline bool uft_a2r_validate_header(const uint8_t *data, size_t size)
{
    if (size < 8) return false;
    if (memcmp(data, UFT_A2R_SIGNATURE, 4) != 0) return false;
    if (data[4] != 0xFF) return false;
    if (data[5] != 0x0A || data[6] != 0x0D || data[7] != 0x0A) return false;
    return true;
}

/**
 * @brief Validate INFO version
 * @param version Version byte
 * @return true if valid
 */
static inline bool uft_a2r_validate_version(uint8_t version)
{
    return version == 1;
}

/**
 * @brief Validate disk type
 * @param disk_type Disk type byte
 * @return true if valid
 */
static inline bool uft_a2r_validate_disk_type(uint8_t disk_type)
{
    return disk_type == UFT_A2R_DISK_525 || disk_type == UFT_A2R_DISK_35;
}

/**
 * @brief Validate capture type
 * @param capture_type Capture type byte
 * @return true if valid
 */
static inline bool uft_a2r_validate_capture_type(uint8_t capture_type)
{
    return capture_type >= UFT_A2R_CAPTURE_TIMING && 
           capture_type <= UFT_A2R_CAPTURE_XTIMING;
}

/**
 * @brief Validate language string
 * @param language Language string
 * @return true if valid
 */
static inline bool uft_a2r_validate_language(const char *language)
{
    if (!language || !*language) return true; /* Empty is OK */
    for (int i = 0; i < UFT_A2R_LANGUAGE_COUNT; i++) {
        if (strcmp(language, uft_a2r_languages[i]) == 0) return true;
    }
    return false;
}

/**
 * @brief Validate requires_ram string
 * @param ram RAM requirement string
 * @return true if valid
 */
static inline bool uft_a2r_validate_requires_ram(const char *ram)
{
    if (!ram || !*ram) return true;
    for (int i = 0; i < UFT_A2R_RAM_COUNT; i++) {
        if (strcmp(ram, uft_a2r_requires_ram[i]) == 0) return true;
    }
    return false;
}

/**
 * @brief Validate requires_machine string
 * @param machine Machine requirement string
 * @return true if valid
 */
static inline bool uft_a2r_validate_requires_machine(const char *machine)
{
    if (!machine || !*machine) return true;
    for (int i = 0; i < UFT_A2R_MACHINE_COUNT; i++) {
        if (strcmp(machine, uft_a2r_requires_machine[i]) == 0) return true;
    }
    return false;
}

/*============================================================================
 * A2R File Operations
 *============================================================================*/

/**
 * @brief Read A2R file
 * @param data File data
 * @param size Data size
 * @param a2r Output A2R context
 * @return 0 on success, negative on error
 */
int uft_a2r_read(const uint8_t *data, size_t size, uft_a2r_file_t *a2r);

/**
 * @brief Write A2R file
 * @param a2r A2R context
 * @param out_data Output buffer (caller allocates)
 * @param out_size Buffer size
 * @return Bytes written, or negative on error
 */
int uft_a2r_write(const uft_a2r_file_t *a2r, uint8_t *out_data, size_t out_size);

/**
 * @brief Free A2R file context
 * @param a2r A2R context to free
 */
void uft_a2r_free(uft_a2r_file_t *a2r);

/*============================================================================
 * A2R Track Operations
 *============================================================================*/

/**
 * @brief Get track by location
 * @param a2r A2R file context
 * @param location Track location (0-159 in 0.25 steps, e.g., 0=0.00, 1=0.25, 4=1.00)
 * @return Pointer to track, or NULL if not found
 */
uft_a2r_track_t *uft_a2r_get_track(uft_a2r_file_t *a2r, uint8_t location);

/**
 * @brief Convert track location to human-readable string
 * @param location Track location (0-159)
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Pointer to buffer
 */
static inline char *uft_a2r_track_location_str(uint8_t location, char *buffer, size_t size)
{
    static const char *quarters[] = { ".00", ".25", ".50", ".75" };
    int track = location / 4;
    int quarter = location % 4;
    snprintf(buffer, size, "%d%s", track, quarters[quarter]);
    return buffer;
}

/*============================================================================
 * A2R Metadata Operations
 *============================================================================*/

/**
 * @brief Get metadata value by key
 * @param a2r A2R file context
 * @param key Metadata key
 * @return First value for key, or NULL if not found
 */
const char *uft_a2r_get_meta(const uft_a2r_file_t *a2r, const char *key);

/**
 * @brief Get all metadata values for key
 * @param a2r A2R file context
 * @param key Metadata key
 * @param count Output value count
 * @return Array of values, or NULL if not found
 */
const char **uft_a2r_get_meta_values(const uft_a2r_file_t *a2r, const char *key, size_t *count);

/**
 * @brief Set metadata value
 * @param a2r A2R file context
 * @param key Metadata key
 * @param value Value to set
 * @return 0 on success, negative on error
 */
int uft_a2r_set_meta(uft_a2r_file_t *a2r, const char *key, const char *value);

/**
 * @brief Delete metadata key
 * @param a2r A2R file context
 * @param key Key to delete
 * @return 0 on success, negative if not found
 */
int uft_a2r_delete_meta(uft_a2r_file_t *a2r, const char *key);

/*============================================================================
 * A2R Conversion
 *============================================================================*/

/**
 * @brief Convert A2R to WOZ2
 * @param a2r A2R file context
 * @param out_data Output buffer
 * @param out_size Buffer size
 * @return Bytes written, or negative on error
 */
int uft_a2r_to_woz2(const uft_a2r_file_t *a2r, uint8_t *out_data, size_t out_size);

/**
 * @brief Export A2R to JSON
 * @param a2r A2R file context
 * @param out_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written, or negative on error
 */
int uft_a2r_to_json(const uft_a2r_file_t *a2r, char *out_buffer, size_t buffer_size);

/**
 * @brief Import JSON to A2R metadata
 * @param a2r A2R file context
 * @param json_data JSON string
 * @return 0 on success, negative on error
 */
int uft_a2r_from_json(uft_a2r_file_t *a2r, const char *json_data);

#ifdef __cplusplus
}
#endif

#endif /* UFT_A2R_H */

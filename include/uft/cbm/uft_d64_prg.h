/**
 * @file uft_d64_prg.h
 * @brief D64 PRG File Manipulation API
 * @version 1.0.0
 * 
 * Tools for manipulating PRG files within D64 images:
 * - Load address modification
 * - File chain traversal
 * - Pattern searching within files
 * - PRG extraction/insertion
 * 
 * Based on "The Little Black Book" forensic techniques.
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_D64_PRG_H
#define UFT_D64_PRG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Forward Declarations
 * ═══════════════════════════════════════════════════════════════════════════ */

struct uft_d64_image;

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Common C64 load addresses */
#define UFT_C64_BASIC_START     0x0801  /**< BASIC program start */
#define UFT_C64_SCREEN_START    0x0400  /**< Screen memory */
#define UFT_C64_BITMAP_START    0x2000  /**< Bitmap graphics */
#define UFT_C64_SPRITE_START    0x3000  /**< Common sprite location */
#define UFT_C64_CHARSET_START   0x3800  /**< Common charset location */
#define UFT_C64_HIMEM           0xC000  /**< High memory */

/** File types */
#define UFT_D64_FTYPE_DEL       0x00    /**< Deleted/Scratched */
#define UFT_D64_FTYPE_SEQ       0x01    /**< Sequential */
#define UFT_D64_FTYPE_PRG       0x02    /**< Program */
#define UFT_D64_FTYPE_USR       0x03    /**< User */
#define UFT_D64_FTYPE_REL       0x04    /**< Relative */

/** File type flags */
#define UFT_D64_FTYPE_CLOSED    0x80    /**< File is closed (valid) */
#define UFT_D64_FTYPE_LOCKED    0x40    /**< File is locked (< in listing) */
#define UFT_D64_FTYPE_SPLAT     0x20    /**< Splat file (unclosed) */

/* ═══════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief PRG file information
 */
typedef struct uft_d64_prg_info {
    char        filename[17];       /**< Filename (ASCII, null-terminated) */
    uint8_t     file_type;          /**< File type byte (raw) */
    uint8_t     start_track;        /**< First sector track */
    uint8_t     start_sector;       /**< First sector number */
    uint16_t    load_address;       /**< PRG load address */
    uint16_t    size_blocks;        /**< Size in blocks */
    size_t      size_bytes;         /**< Size in bytes (computed) */
    bool        is_closed;          /**< File is properly closed */
    bool        is_locked;          /**< File is locked */
    bool        is_basic;           /**< Appears to be BASIC program */
} uft_d64_prg_info_t;

/**
 * @brief Track/Sector position with offset
 */
typedef struct uft_d64_ts_position {
    uint8_t     track;              /**< Track number */
    uint8_t     sector;             /**< Sector number */
    uint16_t    offset;             /**< Byte offset within sector */
} uft_d64_ts_position_t;

/**
 * @brief File chain callback function type
 * 
 * @param track     Current track
 * @param sector    Current sector  
 * @param data      Sector data (254 bytes payload)
 * @param data_len  Actual data length (may be < 254 for last sector)
 * @param user_data User context pointer
 * @return          0 to continue, non-zero to stop iteration
 */
typedef int (*uft_d64_chain_callback_t)(uint8_t track, uint8_t sector,
                                         const uint8_t* data, size_t data_len,
                                         void* user_data);

/* ═══════════════════════════════════════════════════════════════════════════
 * PRG Information Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get PRG file information by filename
 * 
 * @param img       D64 image
 * @param filename  Filename to search for (ASCII)
 * @param info      Output info structure
 * @return          0 on success, -1 if not found or error
 */
int uft_d64_prg_get_info(const struct uft_d64_image* img,
                          const char* filename,
                          uft_d64_prg_info_t* info);

/**
 * @brief Get load address of a PRG file
 * 
 * @param img       D64 image
 * @param filename  Filename (ASCII)
 * @param addr      Output load address
 * @return          0 on success, negative on error
 */
int uft_d64_prg_get_load_address(const struct uft_d64_image* img,
                                  const char* filename,
                                  uint16_t* addr);

/**
 * @brief Check if a file appears to be a BASIC program
 * 
 * Checks load address (0x0801) and validates BASIC token structure.
 * 
 * @param img       D64 image
 * @param filename  Filename (ASCII)
 * @return          true if likely BASIC, false otherwise
 */
bool uft_d64_prg_is_basic(const struct uft_d64_image* img,
                           const char* filename);

/* ═══════════════════════════════════════════════════════════════════════════
 * PRG Modification Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Set load address of a PRG file
 * 
 * Modifies the first two bytes of the PRG file to set a new load address.
 * 
 * @param img       D64 image
 * @param filename  Filename (ASCII)
 * @param addr      New load address
 * @return          0 on success, negative on error
 */
int uft_d64_prg_set_load_address(struct uft_d64_image* img,
                                  const char* filename,
                                  uint16_t addr);

/**
 * @brief Patch bytes within a PRG file
 * 
 * @param img       D64 image
 * @param filename  Filename (ASCII)
 * @param offset    Offset within file (after load address)
 * @param data      Data to write
 * @param len       Length of data
 * @return          0 on success, negative on error
 */
int uft_d64_prg_patch(struct uft_d64_image* img,
                       const char* filename,
                       size_t offset,
                       const uint8_t* data,
                       size_t len);

/* ═══════════════════════════════════════════════════════════════════════════
 * File Chain Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Iterate over file chain sectors
 * 
 * Calls callback for each sector in the file chain.
 * 
 * @param img           D64 image
 * @param start_track   First sector track
 * @param start_sector  First sector number
 * @param callback      Callback function
 * @param user_data     User context for callback
 * @return              0 on success, negative on error
 */
int uft_d64_iterate_chain(const struct uft_d64_image* img,
                           uint8_t start_track, uint8_t start_sector,
                           uft_d64_chain_callback_t callback,
                           void* user_data);

/**
 * @brief Get file size in bytes by traversing chain
 * 
 * @param img           D64 image
 * @param start_track   First sector track
 * @param start_sector  First sector number
 * @return              Size in bytes, or 0 on error
 */
size_t uft_d64_get_chain_size(const struct uft_d64_image* img,
                               uint8_t start_track, uint8_t start_sector);

/**
 * @brief Read entire file into buffer
 * 
 * @param img           D64 image
 * @param filename      Filename (ASCII)
 * @param buffer        Output buffer (caller-allocated)
 * @param buffer_size   Buffer size
 * @param bytes_read    Actual bytes read (output)
 * @return              0 on success, negative on error
 */
int uft_d64_prg_read(const struct uft_d64_image* img,
                      const char* filename,
                      uint8_t* buffer,
                      size_t buffer_size,
                      size_t* bytes_read);

/* ═══════════════════════════════════════════════════════════════════════════
 * Pattern Search Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Search for byte pattern within a file
 * 
 * @param img       D64 image
 * @param filename  Filename (ASCII)
 * @param pattern   Pattern to search for
 * @param pat_len   Pattern length
 * @param position  Output: position of first match
 * @return          0 if found, 1 if not found, negative on error
 */
int uft_d64_prg_find_pattern(const struct uft_d64_image* img,
                              const char* filename,
                              const uint8_t* pattern,
                              size_t pat_len,
                              uft_d64_ts_position_t* position);

/**
 * @brief Search and replace pattern within a file
 * 
 * @param img           D64 image
 * @param filename      Filename (ASCII)
 * @param pattern       Pattern to search for
 * @param pat_len       Pattern length
 * @param replacement   Replacement bytes
 * @param rep_len       Replacement length (must equal pat_len)
 * @param count         Output: number of replacements made
 * @return              0 on success, negative on error
 */
int uft_d64_prg_replace_pattern(struct uft_d64_image* img,
                                 const char* filename,
                                 const uint8_t* pattern,
                                 size_t pat_len,
                                 const uint8_t* replacement,
                                 size_t rep_len,
                                 size_t* count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D64_PRG_H */

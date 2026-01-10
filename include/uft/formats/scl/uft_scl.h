/**
 * @file uft_scl.h
 * @brief SCL Format Parser (Sinclair/TR-DOS Container)
 * @version 2.0.0 - Integrated from uft_drive_trx_scl_pack_v2
 * 
 * SCL ist ein einfaches Container-Format für TR-DOS Dateien:
 * - "SINCLAIR" Magic (8 Bytes)
 * - File Count (1 Byte)
 * - Directory (14 Bytes pro Datei)
 * - Concatenierte Daten
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SCL header magic: "SINCLAIR" */
#define UFT_SCL_MAGIC           "SINCLAIR"
#define UFT_SCL_MAGIC_LEN       8
#define UFT_SCL_MAX_FILES       255

/**
 * @brief SCL Directory Entry
 */
typedef struct {
    char name[9];               /**< 8 chars, space-padded; NUL-terminated here */
    uint8_t type;               /**< TR-DOS type byte */
    uint8_t param[3];           /**< Type-dependent params */
    uint8_t length_sectors;     /**< Data length in 256-byte sectors */
} uft_scl_entry_t;

/**
 * @brief Parsed SCL Container
 */
typedef struct {
    uint8_t file_count;         /**< Number of files */
    uft_scl_entry_t *entries;   /**< Directory entries (owned) */
    const uint8_t *data;        /**< Points into original buffer */
    size_t data_len;            /**< Total data length */
} uft_scl_t;

/**
 * @brief Validate SCL structure
 * @param buf SCL data
 * @param len Data length
 * @return UFT_OK or error code
 */
int uft_scl_validate(const uint8_t *buf, size_t len);

/**
 * @brief Parse SCL container
 * @param buf SCL data (must remain valid)
 * @param len Data length
 * @param out Output structure
 * @return UFT_OK or error code
 */
int uft_scl_parse(const uint8_t *buf, size_t len, uft_scl_t *out);

/**
 * @brief Free SCL resources
 * @param scl SCL structure
 */
void uft_scl_free(uft_scl_t *scl);

/**
 * @brief Get single file from SCL
 * @param buf Original SCL buffer
 * @param len Buffer length
 * @param index File index
 * @param meta_out Optional: file metadata
 * @param data_out Output: pointer to file data
 * @param data_len_out Output: file data length
 * @return UFT_OK or error code
 */
int uft_scl_get_file(const uint8_t *buf, size_t len, size_t index,
                     uft_scl_entry_t *meta_out,
                     const uint8_t **data_out, size_t *data_len_out);

/**
 * @brief Build SCL from entries and file data
 * @param entries Directory entries
 * @param file_data Array of file data pointers
 * @param file_sizes Array of file sizes (must be multiple of 256)
 * @param file_count Number of files
 * @param out_buf Output: allocated buffer (caller must free)
 * @param out_len Output: buffer length
 * @return UFT_OK or error code
 */
int uft_scl_build(const uft_scl_entry_t *entries,
                  const uint8_t *const *file_data,
                  const size_t *file_sizes,
                  size_t file_count,
                  uint8_t **out_buf, size_t *out_len);

/**
 * @brief Probe if buffer is SCL format
 * @param buf Data to probe
 * @param len Data length
 * @return true if SCL signature found
 */
static inline bool uft_scl_probe(const uint8_t *buf, size_t len) {
    if (!buf || len < 9) return false;
    return (buf[0] == 'S' && buf[1] == 'I' && buf[2] == 'N' && buf[3] == 'C' &&
            buf[4] == 'L' && buf[5] == 'A' && buf[6] == 'I' && buf[7] == 'R');
}

#ifdef __cplusplus
}
#endif

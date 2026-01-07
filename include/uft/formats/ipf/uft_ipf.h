/**
 * @file uft_ipf.h
 * @brief IPF Container: Read / Write / Validate (C11)
 * 
 * Implements robust chunk-container parsing for IPF/CAPS-style files.
 * 
 * Features:
 * - Strict validation (bounds/overlap + optional CRC32)
 * - Big/Little endian support with auto-detection
 * - Minimal writer (container-only)
 * - Integration hooks for CapsImg provider
 * 
 * Note: Full track decoding requires a provider (CapsImg or similar).
 * This module handles container structure only.
 * 
 * @version 1.0.0 - Initial integration into UFT
 * @date 2026-01-07
 */

#ifndef UFT_IPF_H
#define UFT_IPF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Codes (UFT-compatible)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum uft_ipf_err {
    UFT_IPF_OK          =  0,   /**< Success */
    UFT_IPF_EINVAL      = -1,   /**< Invalid argument */
    UFT_IPF_EIO         = -2,   /**< I/O error */
    UFT_IPF_EFORMAT     = -3,   /**< Invalid format */
    UFT_IPF_ESHORT      = -4,   /**< File too short */
    UFT_IPF_EBOUNDS     = -5,   /**< Chunk out of bounds */
    UFT_IPF_EOVERLAP    = -6,   /**< Chunks overlap */
    UFT_IPF_ECRC        = -7,   /**< CRC mismatch */
    UFT_IPF_ENOMEM      = -8,   /**< Out of memory */
    UFT_IPF_ENOTSUP     = -9    /**< Not supported */
} uft_ipf_err_t;

/**
 * @brief Get error string
 */
const char *uft_ipf_strerror(uft_ipf_err_t err);

/* ═══════════════════════════════════════════════════════════════════════════════
 * FourCC Type
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_fourcc {
    uint8_t b[4];
} uft_fourcc_t;

/** Create FourCC from characters */
static inline uft_fourcc_t uft_fourcc_make(char a, char b, char c, char d) {
    uft_fourcc_t x = {{(uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d}};
    return x;
}

/** Compare two FourCCs */
static inline bool uft_fourcc_eq(uft_fourcc_t a, uft_fourcc_t b) {
    return a.b[0]==b.b[0] && a.b[1]==b.b[1] && a.b[2]==b.b[2] && a.b[3]==b.b[3];
}

/** Format FourCC as string (static buffer) */
static inline const char *uft_fourcc_str(uft_fourcc_t f) {
    static char buf[8];
    buf[0] = (char)f.b[0];
    buf[1] = (char)f.b[1];
    buf[2] = (char)f.b[2];
    buf[3] = (char)f.b[3];
    buf[4] = '\0';
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Known FourCCs
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_IPF_MAGIC_CAPS  uft_fourcc_make('C','A','P','S')
#define UFT_IPF_MAGIC_IPFF  uft_fourcc_make('I','P','F','F')
#define UFT_IPF_CHUNK_INFO  uft_fourcc_make('I','N','F','O')
#define UFT_IPF_CHUNK_IMGE  uft_fourcc_make('I','M','G','E')
#define UFT_IPF_CHUNK_DATA  uft_fourcc_make('D','A','T','A')
#define UFT_IPF_CHUNK_TRCK  uft_fourcc_make('T','R','C','K')

/* ═══════════════════════════════════════════════════════════════════════════════
 * Chunk Model
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parsed chunk information
 */
typedef struct uft_ipf_chunk {
    uft_fourcc_t id;              /**< Chunk identifier (FourCC) */
    uint64_t     header_offset;   /**< Offset of chunk header */
    uint64_t     data_offset;     /**< Offset of payload data */
    uint32_t     data_size;       /**< Payload size in bytes */
    uint32_t     header_size;     /**< Header size (8 or 12) */
    uint32_t     crc32;           /**< CRC32 if present, else 0 */
    uint32_t     flags;           /**< Internal flags */
} uft_ipf_chunk_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Container Handle
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IPF container context
 */
typedef struct uft_ipf {
    FILE            *fp;          /**< File handle */
    uint64_t         file_size;   /**< Total file size */
    bool             big_endian;  /**< True if big-endian */
    uft_fourcc_t     magic;       /**< Detected magic */
    uft_ipf_chunk_t *chunks;      /**< Parsed chunks */
    size_t           chunk_count; /**< Number of chunks */
    char             path[260];   /**< File path (for error messages) */
} uft_ipf_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Reader API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open IPF file
 * @param ctx Context to initialize
 * @param path File path
 * @return UFT_IPF_OK on success
 */
uft_ipf_err_t uft_ipf_open(uft_ipf_t *ctx, const char *path);

/**
 * @brief Close IPF file and free resources
 */
void uft_ipf_close(uft_ipf_t *ctx);

/**
 * @brief Parse chunk structure
 * @param ctx Opened context
 * @return UFT_IPF_OK on success
 */
uft_ipf_err_t uft_ipf_parse(uft_ipf_t *ctx);

/**
 * @brief Validate container integrity
 * @param ctx Parsed context
 * @param strict If true, check overlaps and CRCs
 * @return UFT_IPF_OK if valid
 */
uft_ipf_err_t uft_ipf_validate(const uft_ipf_t *ctx, bool strict);

/**
 * @brief Get number of chunks
 */
size_t uft_ipf_chunk_count(const uft_ipf_t *ctx);

/**
 * @brief Get chunk at index
 * @return NULL if out of bounds
 */
const uft_ipf_chunk_t *uft_ipf_chunk_at(const uft_ipf_t *ctx, size_t idx);

/**
 * @brief Find chunk by FourCC
 * @return Index or SIZE_MAX if not found
 */
size_t uft_ipf_find_chunk(const uft_ipf_t *ctx, uft_fourcc_t id);

/**
 * @brief Read chunk payload data
 * @param ctx Context
 * @param idx Chunk index
 * @param buf Output buffer
 * @param buf_sz Buffer size
 * @param out_read Bytes actually read (optional)
 * @return UFT_IPF_OK on success
 */
uft_ipf_err_t uft_ipf_read_chunk_data(const uft_ipf_t *ctx, size_t idx,
                                       void *buf, size_t buf_sz, size_t *out_read);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Writer API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief IPF writer context
 */
typedef struct uft_ipf_writer {
    FILE        *fp;              /**< Output file */
    bool         big_endian;      /**< Use big-endian */
    uft_fourcc_t magic;           /**< Container magic */
    uint64_t     bytes_written;   /**< Total bytes written */
    uint32_t     header_mode;     /**< Header size (8 or 12) */
    uint32_t     chunk_count;     /**< Chunks written */
} uft_ipf_writer_t;

/**
 * @brief Open writer
 * @param w Writer context
 * @param path Output file path
 * @param magic Container magic FourCC
 * @param big_endian Use big-endian encoding
 * @param header_mode 8 (no CRC) or 12 (with CRC)
 * @return UFT_IPF_OK on success
 */
uft_ipf_err_t uft_ipf_writer_open(uft_ipf_writer_t *w, const char *path,
                                   uft_fourcc_t magic, bool big_endian,
                                   uint32_t header_mode);

/**
 * @brief Write file header (magic + optional info)
 */
uft_ipf_err_t uft_ipf_writer_write_header(uft_ipf_writer_t *w);

/**
 * @brief Add chunk to container
 * @param w Writer context
 * @param id Chunk FourCC
 * @param data Chunk payload
 * @param data_size Payload size
 * @param add_crc32 Include CRC32 in header
 * @return UFT_IPF_OK on success
 */
uft_ipf_err_t uft_ipf_writer_add_chunk(uft_ipf_writer_t *w, uft_fourcc_t id,
                                        const void *data, uint32_t data_size,
                                        bool add_crc32);

/**
 * @brief Finalize and close writer
 */
uft_ipf_err_t uft_ipf_writer_close(uft_ipf_writer_t *w);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate CRC32 (IEEE polynomial)
 */
uint32_t uft_ipf_crc32(const void *data, size_t len);

/**
 * @brief Dump container info to file
 */
void uft_ipf_dump_info(const uft_ipf_t *ctx, FILE *out);

/* ═══════════════════════════════════════════════════════════════════════════════
 * CapsImg Provider Adapter (Optional)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_IPF_ENABLE_CAPSIMG

/**
 * @brief CapsImg adapter for full track access
 * 
 * Requires linking against CapsImg library.
 */
typedef struct uft_ipf_caps_adapter {
    void *caps_handle;    /**< CapsImg library handle */
    int   image_id;       /**< Loaded image ID */
} uft_ipf_caps_adapter_t;

int uft_ipf_caps_open(uft_ipf_caps_adapter_t *a, const char *path);
int uft_ipf_caps_close(uft_ipf_caps_adapter_t *a);
int uft_ipf_caps_get_track_count(uft_ipf_caps_adapter_t *a);
int uft_ipf_caps_read_track(uft_ipf_caps_adapter_t *a, int track, int side,
                             void *out_buf, size_t out_sz, size_t *out_len);

#endif /* UFT_IPF_ENABLE_CAPSIMG */

#ifdef __cplusplus
}
#endif

#endif /* UFT_IPF_H */

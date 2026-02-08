/**
 * @file uft_json_serialize.h
 * @brief UFT JSON Serialization API
 * 
 * P0-IR-005: Comprehensive JSON export/import for UFT Intermediate Representation
 * 
 * Features:
 * - Track/Sector/Flux data serialization
 * - Metadata and analysis results export
 * - Streaming JSON generation for large files
 * - Pretty-print and compact modes
 * - Schema versioning for compatibility
 * 
 * @copyright UFT Project 2026
 */

#ifndef UFT_JSON_SERIALIZE_H
#define UFT_JSON_SERIALIZE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Version & Schema
 *===========================================================================*/

#define UFT_JSON_SCHEMA_VERSION     "1.0.0"
#define UFT_JSON_SCHEMA_URL         "https://uft.io/schema/v1"

/*===========================================================================
 * Configuration
 *===========================================================================*/

/** JSON output options */
typedef enum {
    UFT_JSON_COMPACT     = 0,       /**< Minimal whitespace */
    UFT_JSON_PRETTY      = 1,       /**< Indented, readable */
    UFT_JSON_PRETTY_2    = 2,       /**< 2-space indent */
    UFT_JSON_PRETTY_4    = 4,       /**< 4-space indent */
    UFT_JSON_PRETTY_TAB  = 8        /**< Tab indent */
} uft_json_format_t;

/** Data encoding for binary content */
typedef enum {
    UFT_JSON_ENC_BASE64  = 0,       /**< Base64 encoding (default) */
    UFT_JSON_ENC_HEX     = 1,       /**< Hexadecimal string */
    UFT_JSON_ENC_ARRAY   = 2        /**< JSON number array [0,1,255,...] */
} uft_json_encoding_t;

/** Serialization options */
typedef struct {
    uft_json_format_t format;       /**< Output format */
    uft_json_encoding_t encoding;   /**< Binary data encoding */
    bool include_raw_data;          /**< Include raw sector/flux data */
    bool include_analysis;          /**< Include analysis results */
    bool include_metadata;          /**< Include file metadata */
    bool include_checksums;         /**< Include CRC/hash values */
    bool include_timing;            /**< Include timing information */
    bool include_weak_bits;         /**< Include weak bit maps */
    uint32_t max_data_size;         /**< Max bytes per data block (0=unlimited) */
} uft_json_options_t;

/** Default options */
#define UFT_JSON_OPTIONS_DEFAULT { \
    .format = UFT_JSON_PRETTY_2, \
    .encoding = UFT_JSON_ENC_BASE64, \
    .include_raw_data = true, \
    .include_analysis = true, \
    .include_metadata = true, \
    .include_checksums = true, \
    .include_timing = false, \
    .include_weak_bits = true, \
    .max_data_size = 0 \
}

/*===========================================================================
 * Status Codes
 *===========================================================================*/

typedef enum {
    UFT_JSON_OK           = 0,
    UFT_JSON_E_INVALID    = -1,     /**< Invalid parameter */
    UFT_JSON_E_MEMORY     = -2,     /**< Memory allocation failed */
    UFT_JSON_E_BUFFER     = -3,     /**< Buffer too small */
    UFT_JSON_E_IO         = -4,     /**< I/O error */
    UFT_JSON_E_FORMAT     = -5,     /**< Invalid JSON format */
    UFT_JSON_E_SCHEMA     = -6,     /**< Schema mismatch */
    UFT_JSON_E_OVERFLOW   = -7      /**< Data overflow */
} uft_json_status_t;

/*===========================================================================
 * Data Structures for Serialization
 *===========================================================================*/

/** * TODO: Data structures for serialization are defined in implementation.
 * This header will be completed in future revisions.
 */

#ifdef __cplusplus
}
#endif

#endif /* UFT_JSON_SERIALIZE_H */

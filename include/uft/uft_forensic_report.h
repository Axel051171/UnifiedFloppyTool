/**
 * @file uft_forensic_report.h
 * @brief Forensic Report Generator API
 * 
 * TICKET-006: Forensic Report Generator
 * Generate professional PDF/JSON/HTML reports with hash chain and audit trail
 * 
 * @version 5.1.0
 * @date 2026-01-03
 */

#ifndef UFT_FORENSIC_REPORT_H
#define UFT_FORENSIC_REPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "uft_types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Report Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Report output format */
typedef enum {
    UFT_REPORT_FORMAT_JSON,     /**< JSON format */
    UFT_REPORT_FORMAT_HTML,     /**< HTML format */
    UFT_REPORT_FORMAT_PDF,      /**< PDF format */
    UFT_REPORT_FORMAT_MARKDOWN, /**< Markdown format */
    UFT_REPORT_FORMAT_TEXT,     /**< Plain text */
    UFT_REPORT_FORMAT_XML,      /**< XML format */
} uft_report_format_t;

/** Report type/purpose */
typedef enum {
    UFT_REPORT_TYPE_READ,       /**< Disk read operation */
    UFT_REPORT_TYPE_WRITE,      /**< Disk write operation */
    UFT_REPORT_TYPE_VERIFY,     /**< Verification report */
    UFT_REPORT_TYPE_RECOVERY,   /**< Recovery operation */
    UFT_REPORT_TYPE_ANALYSIS,   /**< Disk analysis */
    UFT_REPORT_TYPE_COMPARISON, /**< Disk comparison */
    UFT_REPORT_TYPE_CONVERSION, /**< Format conversion */
    UFT_REPORT_TYPE_INVENTORY,  /**< Collection inventory */
} uft_report_type_t;

/** Report sections to include */
typedef enum {
    UFT_REPORT_SEC_SUMMARY      = (1 << 0),  /**< Executive summary */
    UFT_REPORT_SEC_METADATA     = (1 << 1),  /**< Disk metadata */
    UFT_REPORT_SEC_HASHES       = (1 << 2),  /**< Hash values */
    UFT_REPORT_SEC_HASH_CHAIN   = (1 << 3),  /**< Full hash chain */
    UFT_REPORT_SEC_TRACK_MAP    = (1 << 4),  /**< Track status map */
    UFT_REPORT_SEC_TRACK_DETAIL = (1 << 5),  /**< Detailed track info */
    UFT_REPORT_SEC_ERRORS       = (1 << 6),  /**< Error list */
    UFT_REPORT_SEC_TIMELINE     = (1 << 7),  /**< Operation timeline */
    UFT_REPORT_SEC_PROTECTION   = (1 << 8),  /**< Copy protection */
    UFT_REPORT_SEC_FILESYSTEM   = (1 << 9),  /**< Filesystem info */
    UFT_REPORT_SEC_FLUX         = (1 << 10), /**< Flux statistics */
    UFT_REPORT_SEC_HEATMAP      = (1 << 11), /**< Visual heatmap */
    UFT_REPORT_SEC_AUDIT        = (1 << 12), /**< Audit trail */
    UFT_REPORT_SEC_SIGNATURE    = (1 << 13), /**< Digital signature */
    UFT_REPORT_SEC_ALL          = 0xFFFF,    /**< All sections */
} uft_report_sections_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Hash Chain
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Hash algorithm */
typedef enum {
    UFT_HASH_MD5,
    UFT_HASH_SHA1,
    UFT_HASH_SHA256,
    UFT_HASH_SHA512,
    UFT_HASH_CRC32,
    UFT_HASH_XXH64,
} uft_hash_algo_t;

/** Single hash entry */
typedef struct {
    char            *data_id;       /**< Identifier (e.g., "track_00_0") */
    uft_hash_algo_t algorithm;      /**< Hash algorithm */
    char            hash[129];      /**< Hash value (hex string) */
    uint64_t        timestamp;      /**< When computed */
    size_t          data_size;      /**< Size of hashed data */
    uint32_t        sequence;       /**< Sequence number in chain */
    char            prev_hash[129]; /**< Previous hash (for chain) */
} uft_hash_entry_t;

/** Hash chain */
typedef struct {
    uft_hash_entry_t *entries;      /**< Array of hash entries */
    int             count;          /**< Number of entries */
    int             capacity;       /**< Allocated capacity */
    uft_hash_algo_t algorithm;      /**< Default algorithm */
    char            root_hash[129]; /**< Root/final hash */
    bool            verified;       /**< Chain verified */
} uft_hash_chain_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Audit Trail
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Audit event type */
typedef enum {
    UFT_AUDIT_START,        /**< Operation started */
    UFT_AUDIT_END,          /**< Operation completed */
    UFT_AUDIT_READ,         /**< Data read */
    UFT_AUDIT_WRITE,        /**< Data written */
    UFT_AUDIT_ERROR,        /**< Error occurred */
    UFT_AUDIT_RETRY,        /**< Retry attempted */
    UFT_AUDIT_SKIP,         /**< Track/sector skipped */
    UFT_AUDIT_RECOVER,      /**< Recovery attempted */
    UFT_AUDIT_VERIFY,       /**< Verification performed */
    UFT_AUDIT_CONFIG,       /**< Configuration change */
    UFT_AUDIT_USER,         /**< User action */
} uft_audit_event_t;

/** Single audit entry */
typedef struct {
    uint64_t        timestamp;      /**< Event timestamp (ms) */
    uft_audit_event_t event;        /**< Event type */
    char            *description;   /**< Event description */
    char            *detail;        /**< Additional detail */
    int             cylinder;       /**< Related cylinder (-1 if N/A) */
    int             head;           /**< Related head (-1 if N/A) */
    int             sector;         /**< Related sector (-1 if N/A) */
    uft_error_t     error_code;     /**< Error code if applicable */
} uft_audit_entry_t;

/** Audit trail */
typedef struct {
    uft_audit_entry_t *entries;     /**< Array of entries */
    int             count;          /**< Number of entries */
    int             capacity;       /**< Allocated capacity */
    uint64_t        start_time;     /**< Operation start time */
    uint64_t        end_time;       /**< Operation end time */
} uft_audit_trail_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Report Data Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Track status for report */
typedef struct {
    int             cylinder;
    int             head;
    int             sectors_total;
    int             sectors_good;
    int             sectors_bad;
    int             retry_count;
    float           read_time_ms;
    uint32_t        crc;
    char            hash[65];       /**< SHA-256 of track data */
    bool            has_errors;
    bool            has_weak_bits;
    bool            has_protection;
} uft_report_track_t;

/** Disk metadata for report */
typedef struct {
    char            *source_path;
    char            *target_path;
    uft_format_t    source_format;
    uft_format_t    target_format;
    
    /* Geometry */
    int             cylinders;
    int             heads;
    int             sectors_per_track;
    int             bytes_per_sector;
    size_t          total_size;
    
    /* Detection */
    char            *detected_format;
    char            *detected_encoding;
    char            *detected_filesystem;
    char            *volume_label;
    
    /* Hardware */
    char            *hardware_name;
    char            *hardware_serial;
    char            *drive_type;
    
    /* Media info */
    char            *media_type;
    char            *write_protect;
} uft_report_metadata_t;

/** Protection info for report */
typedef struct {
    char            *scheme_name;
    char            *scheme_version;
    int             confidence;
    char            *details;
    int             track_count;
    int             *affected_tracks;
} uft_report_protection_t;

/** Report options */
typedef struct {
    uft_report_format_t format;
    uft_report_type_t   type;
    uint32_t            sections;       /**< OR'd UFT_REPORT_SEC_* */
    
    /* Branding */
    char                *title;
    char                *organization;
    char                *operator_name;
    char                *case_number;
    char                *evidence_id;
    char                *logo_path;
    
    /* Options */
    bool                include_raw_data;
    bool                include_heatmap;
    bool                include_timeline;
    bool                sign_report;
    char                *signature_key;
    
    /* Hash options */
    uft_hash_algo_t     hash_algorithm;
    bool                compute_track_hashes;
    bool                compute_sector_hashes;
    
    /* Output */
    char                *output_path;
    bool                overwrite;
} uft_report_options_t;

/** Default report options */
#define UFT_REPORT_OPTIONS_DEFAULT { \
    .format = UFT_REPORT_FORMAT_JSON, \
    .type = UFT_REPORT_TYPE_READ, \
    .sections = UFT_REPORT_SEC_SUMMARY | UFT_REPORT_SEC_METADATA | \
                UFT_REPORT_SEC_HASHES | UFT_REPORT_SEC_ERRORS, \
    .title = NULL, \
    .organization = NULL, \
    .operator_name = NULL, \
    .case_number = NULL, \
    .evidence_id = NULL, \
    .logo_path = NULL, \
    .include_raw_data = false, \
    .include_heatmap = true, \
    .include_timeline = true, \
    .sign_report = false, \
    .signature_key = NULL, \
    .hash_algorithm = UFT_HASH_SHA256, \
    .compute_track_hashes = true, \
    .compute_sector_hashes = false, \
    .output_path = NULL, \
    .overwrite = false \
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Report Builder
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Opaque report handle */
typedef struct uft_report uft_report_t;

/**
 * @brief Create new report
 * @param options Report options
 * @return New report handle
 */
uft_report_t *uft_report_create(const uft_report_options_t *options);

/**
 * @brief Destroy report
 * @param report Report to destroy
 */
void uft_report_destroy(uft_report_t *report);

/**
 * @brief Set report metadata
 * @param report Report handle
 * @param metadata Metadata structure
 */
void uft_report_set_metadata(uft_report_t *report, const uft_report_metadata_t *metadata);

/**
 * @brief Add track result to report
 * @param report Report handle
 * @param track Track result
 */
void uft_report_add_track(uft_report_t *report, const uft_report_track_t *track);

/**
 * @brief Add error to report
 * @param report Report handle
 * @param cylinder Cylinder number (-1 for general)
 * @param head Head number
 * @param sector Sector number
 * @param error_code Error code
 * @param message Error message
 */
void uft_report_add_error(uft_report_t *report, int cylinder, int head, int sector,
                           uft_error_t error_code, const char *message);

/**
 * @brief Add protection detection result
 * @param report Report handle
 * @param protection Protection info
 */
void uft_report_add_protection(uft_report_t *report, const uft_report_protection_t *protection);

/**
 * @brief Add audit event
 * @param report Report handle
 * @param event Event type
 * @param description Event description
 * @param cylinder Related cylinder (-1 if N/A)
 * @param head Related head (-1 if N/A)
 */
void uft_report_add_audit(uft_report_t *report, uft_audit_event_t event,
                           const char *description, int cylinder, int head);

/**
 * @brief Add hash to chain
 * @param report Report handle
 * @param data_id Data identifier
 * @param data Data to hash
 * @param size Data size
 */
void uft_report_add_hash(uft_report_t *report, const char *data_id,
                          const uint8_t *data, size_t size);

/**
 * @brief Set overall result
 * @param report Report handle
 * @param success Whether operation succeeded
 * @param message Result message
 */
void uft_report_set_result(uft_report_t *report, bool success, const char *message);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Report Generation
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generate report to file
 * @param report Report handle
 * @param path Output file path
 * @return UFT_OK on success
 */
uft_error_t uft_report_generate(uft_report_t *report, const char *path);

/**
 * @brief Generate report to string
 * @param report Report handle
 * @return Allocated string (caller must free)
 */
char *uft_report_generate_string(uft_report_t *report);

/**
 * @brief Generate specific format
 * @param report Report handle
 * @param format Output format
 * @return Allocated string (caller must free)
 */
char *uft_report_generate_format(uft_report_t *report, uft_report_format_t format);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Hash Chain API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create hash chain
 * @param algorithm Hash algorithm to use
 * @return New hash chain
 */
uft_hash_chain_t *uft_hash_chain_create(uft_hash_algo_t algorithm);

/**
 * @brief Destroy hash chain
 * @param chain Chain to destroy
 */
void uft_hash_chain_destroy(uft_hash_chain_t *chain);

/**
 * @brief Add data to hash chain
 * @param chain Hash chain
 * @param data_id Data identifier
 * @param data Data to hash
 * @param size Data size
 * @return Hash entry (do not free)
 */
uft_hash_entry_t *uft_hash_chain_add(uft_hash_chain_t *chain, const char *data_id,
                                      const uint8_t *data, size_t size);

/**
 * @brief Finalize hash chain
 * @param chain Hash chain
 * @return Root hash string (valid until chain destroyed)
 */
const char *uft_hash_chain_finalize(uft_hash_chain_t *chain);

/**
 * @brief Verify hash chain integrity
 * @param chain Hash chain
 * @return true if chain is valid
 */
bool uft_hash_chain_verify(uft_hash_chain_t *chain);

/**
 * @brief Export hash chain to JSON
 * @param chain Hash chain
 * @return JSON string (caller must free)
 */
char *uft_hash_chain_to_json(const uft_hash_chain_t *chain);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Audit Trail API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create audit trail
 * @return New audit trail
 */
uft_audit_trail_t *uft_audit_trail_create(void);

/**
 * @brief Destroy audit trail
 * @param trail Trail to destroy
 */
void uft_audit_trail_destroy(uft_audit_trail_t *trail);

/**
 * @brief Log audit event
 * @param trail Audit trail
 * @param event Event type
 * @param description Event description
 * @param cylinder Related cylinder
 * @param head Related head
 * @param sector Related sector
 */
void uft_audit_log(uft_audit_trail_t *trail, uft_audit_event_t event,
                    const char *description, int cylinder, int head, int sector);

/**
 * @brief Export audit trail to JSON
 * @param trail Audit trail
 * @return JSON string (caller must free)
 */
char *uft_audit_trail_to_json(const uft_audit_trail_t *trail);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Get report format name */
const char *uft_report_format_name(uft_report_format_t format);

/** Get report type name */
const char *uft_report_type_name(uft_report_type_t type);

/** Get hash algorithm name */
const char *uft_hash_algo_name(uft_hash_algo_t algo);

/** Get audit event name */
const char *uft_audit_event_name(uft_audit_event_t event);

/** Compute hash of data */
void uft_compute_hash(uft_hash_algo_t algo, const uint8_t *data, size_t size,
                       char *hash_out, size_t hash_size);

/** Get file extension for format */
const char *uft_report_format_extension(uft_report_format_t format);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORENSIC_REPORT_H */

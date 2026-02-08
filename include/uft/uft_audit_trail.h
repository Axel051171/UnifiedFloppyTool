/**
 * @file uft_audit_trail.h
 * @brief UFT Audit Trail System - Complete Operation Logging
 * 
 * @version 3.2.0
 * @date 2026-01-04
 * 
 * Provides forensic-grade logging of all operations for reproducibility
 * and verification. Supports "Kein Bit verloren" philosophy with
 * complete provenance tracking.
 * 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 UnifiedFloppyTool Contributors
 */

#ifndef UFT_AUDIT_TRAIL_H
#define UFT_AUDIT_TRAIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Maximum entries in memory */
#define UFT_AUDIT_MAX_ENTRIES       (65536)

/** Maximum event data size */
#define UFT_AUDIT_MAX_DATA_SIZE     (4096)

/** Maximum path length */
#define UFT_AUDIT_MAX_PATH          (512)

/** Magic for audit file */
#define UFT_AUDIT_MAGIC             (0x55465441)  /* "UFTA" */

/** Current audit format version */
#define UFT_AUDIT_VERSION           (0x0100)

/* ═══════════════════════════════════════════════════════════════════════════
 * Event Types
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    /* Session events */
    UFT_AE_SESSION_START        = 0x0001,
    UFT_AE_SESSION_END          = 0x0002,
    UFT_AE_CONFIG_CHANGE        = 0x0003,
    
    /* File operations */
    UFT_AE_FILE_OPEN            = 0x0100,
    UFT_AE_FILE_CLOSE           = 0x0101,
    UFT_AE_FILE_READ            = 0x0102,
    UFT_AE_FILE_WRITE           = 0x0103,
    UFT_AE_FILE_CREATE          = 0x0104,
    UFT_AE_FILE_DELETE          = 0x0105,
    
    /* Format detection */
    UFT_AE_FORMAT_DETECT        = 0x0200,
    UFT_AE_FORMAT_VERIFY        = 0x0201,
    UFT_AE_FORMAT_CONVERT       = 0x0202,
    
    /* Track operations */
    UFT_AE_TRACK_READ           = 0x0300,
    UFT_AE_TRACK_WRITE          = 0x0301,
    UFT_AE_TRACK_DECODE         = 0x0302,
    UFT_AE_TRACK_ENCODE         = 0x0303,
    UFT_AE_TRACK_REPAIR         = 0x0304,
    
    /* Sector operations */
    UFT_AE_SECTOR_READ          = 0x0400,
    UFT_AE_SECTOR_WRITE         = 0x0401,
    UFT_AE_SECTOR_VERIFY        = 0x0402,
    UFT_AE_SECTOR_REPAIR        = 0x0403,
    
    /* Hardware operations */
    UFT_AE_HW_CONNECT           = 0x0500,
    UFT_AE_HW_DISCONNECT        = 0x0501,
    UFT_AE_HW_CALIBRATE         = 0x0502,
    UFT_AE_HW_READ_FLUX         = 0x0503,
    UFT_AE_HW_WRITE_FLUX        = 0x0504,
    
    /* Recovery operations */
    UFT_AE_RECOVERY_START       = 0x0600,
    UFT_AE_RECOVERY_SUCCESS     = 0x0601,
    UFT_AE_RECOVERY_FAIL        = 0x0602,
    UFT_AE_RECOVERY_PARTIAL     = 0x0603,
    
    /* Errors and warnings */
    UFT_AE_ERROR                = 0x0F00,
    UFT_AE_WARNING              = 0x0F01,
    UFT_AE_CRC_MISMATCH         = 0x0F02,
    UFT_AE_DATA_LOSS            = 0x0F03,
    
    /* Checksum/hash events */
    UFT_AE_CHECKSUM_INPUT       = 0x1000,
    UFT_AE_CHECKSUM_OUTPUT      = 0x1001,
    UFT_AE_HASH_COMPUTED        = 0x1002,
} uft_audit_event_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Severity Levels
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_AUDIT_DEBUG         = 0,
    UFT_AUDIT_INFO          = 1,
    UFT_AUDIT_WARNING       = 2,
    UFT_AUDIT_ERROR         = 3,
    UFT_AUDIT_CRITICAL      = 4,
} uft_audit_severity_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Audit Entry Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Identification */
    uint64_t            sequence;       /**< Monotonic sequence number */
    uint64_t            timestamp_us;   /**< Microseconds since session start */
    time_t              wall_time;      /**< Wall clock time */
    
    /* Event info */
    uft_audit_event_t   event;
    uft_audit_severity_t severity;
    uint16_t            flags;
    
    /* Location context */
    uint8_t             cylinder;
    uint8_t             head;
    uint8_t             sector;
    uint8_t             revolution;
    
    /* Operation details */
    int32_t             result_code;    /**< Operation result */
    uint32_t            bytes_affected; /**< Bytes read/written */
    uint32_t            bits_affected;  /**< Bits processed */
    
    /* Checksums */
    uint32_t            crc_before;     /**< CRC before operation */
    uint32_t            crc_after;      /**< CRC after operation */
    
    /* Associated data */
    char                description[128];
    char                file_path[UFT_AUDIT_MAX_PATH];
    
    /* Extended data (heap allocated if needed) */
    size_t              ext_data_size;
    uint8_t*            ext_data;
} uft_audit_entry_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Audit Session Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Session identification */
    uint8_t             session_id[16]; /**< UUID */
    time_t              start_time;
    time_t              end_time;
    
    /* Software info */
    char                uft_version[16];
    char                os_info[64];
    char                hostname[64];
    
    /* Entry storage */
    uft_audit_entry_t*  entries;
    size_t              entry_count;
    size_t              entry_capacity;
    uint64_t            next_sequence;
    
    /* Timing */
    uint64_t            session_start_us;
    
    /* Output files */
    FILE*               log_file;
    char                log_path[UFT_AUDIT_MAX_PATH];
    
    /* Configuration */
    uint32_t            flags;
    uft_audit_severity_t min_severity;
    bool                auto_flush;
    bool                include_data;
} uft_audit_session_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Session Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_AUDIT_FLAG_TIMESTAMPS   (1 << 0)   /**< Include timestamps */
#define UFT_AUDIT_FLAG_CHECKSUMS    (1 << 1)   /**< Compute checksums */
#define UFT_AUDIT_FLAG_BINARY_LOG   (1 << 2)   /**< Write binary log */
#define UFT_AUDIT_FLAG_TEXT_LOG     (1 << 3)   /**< Write text log */
#define UFT_AUDIT_FLAG_JSON         (1 << 4)   /**< Write JSON format */
#define UFT_AUDIT_FLAG_VERBOSE      (1 << 5)   /**< Include debug events */
#define UFT_AUDIT_FLAG_COMPACT      (1 << 6)   /**< Minimize storage */

#define UFT_AUDIT_DEFAULT_FLAGS     (UFT_AUDIT_FLAG_TIMESTAMPS | \
                                     UFT_AUDIT_FLAG_CHECKSUMS | \
                                     UFT_AUDIT_FLAG_TEXT_LOG)

/* ═══════════════════════════════════════════════════════════════════════════
 * Session Management
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create new audit session
 * @param log_path Path to log file (NULL for memory-only)
 * @param flags Session flags
 * @return Session or NULL on failure
 */
uft_audit_session_t* uft_audit_session_create(
    const char* log_path,
    uint32_t flags
);

/**
 * @brief End and save audit session
 * @param session Session to end
 * @return 0 on success
 */
int uft_audit_session_end(uft_audit_session_t* session);

/**
 * @brief Free session resources
 * @param session Session to free
 */
void uft_audit_session_free(uft_audit_session_t* session);

/**
 * @brief Set minimum severity to log
 * @param session Session
 * @param severity Minimum severity
 */
void uft_audit_set_min_severity(
    uft_audit_session_t* session,
    uft_audit_severity_t severity
);

/* ═══════════════════════════════════════════════════════════════════════════
 * Event Logging
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Log an audit event
 * @param session Session
 * @param event Event type
 * @param severity Severity level
 * @param description Event description
 * @return Entry sequence number or 0 on failure
 */
uint64_t uft_audit_log(
    uft_audit_session_t* session,
    uft_audit_event_t event,
    uft_audit_severity_t severity,
    const char* description
);

/**
 * @brief Log event with track context
 * @param session Session
 * @param event Event type
 * @param severity Severity
 * @param cylinder Cylinder number
 * @param head Head number
 * @param description Description
 * @return Sequence number
 */
uint64_t uft_audit_log_track(
    uft_audit_session_t* session,
    uft_audit_event_t event,
    uft_audit_severity_t severity,
    uint8_t cylinder,
    uint8_t head,
    const char* description
);

/**
 * @brief Log event with sector context
 * @param session Session
 * @param event Event type
 * @param severity Severity
 * @param cylinder Cylinder
 * @param head Head
 * @param sector Sector
 * @param description Description
 * @return Sequence number
 */
uint64_t uft_audit_log_sector(
    uft_audit_session_t* session,
    uft_audit_event_t event,
    uft_audit_severity_t severity,
    uint8_t cylinder,
    uint8_t head,
    uint8_t sector,
    const char* description
);

/**
 * @brief Log file operation
 * @param session Session
 * @param event Event type (FILE_*)
 * @param file_path File path
 * @param bytes Bytes affected
 * @param result Operation result
 * @return Sequence number
 */
uint64_t uft_audit_log_file(
    uft_audit_session_t* session,
    uft_audit_event_t event,
    const char* file_path,
    size_t bytes,
    int result
);

/**
 * @brief Log checksum/hash
 * @param session Session
 * @param event Event type (CHECKSUM_*)
 * @param file_path Related file
 * @param hash_type Hash algorithm ("CRC32", "SHA256", etc.)
 * @param hash_value Hash value (hex string)
 * @return Sequence number
 */
uint64_t uft_audit_log_checksum(
    uft_audit_session_t* session,
    uft_audit_event_t event,
    const char* file_path,
    const char* hash_type,
    const char* hash_value
);

/**
 * @brief Log with extended data
 * @param session Session
 * @param event Event
 * @param severity Severity
 * @param description Description
 * @param data Extended data
 * @param data_size Data size
 * @return Sequence number
 */
uint64_t uft_audit_log_data(
    uft_audit_session_t* session,
    uft_audit_event_t event,
    uft_audit_severity_t severity,
    const char* description,
    const void* data,
    size_t data_size
);

/* ═══════════════════════════════════════════════════════════════════════════
 * Query & Export
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get entry by sequence number
 * @param session Session
 * @param sequence Sequence number
 * @return Entry or NULL if not found
 */
const uft_audit_entry_t* uft_audit_get_entry(
    const uft_audit_session_t* session,
    uint64_t sequence
);

/**
 * @brief Count entries matching criteria
 * @param session Session
 * @param event_mask Event types to count (0 = all)
 * @param min_severity Minimum severity
 * @return Entry count
 */
size_t uft_audit_count_entries(
    const uft_audit_session_t* session,
    uint32_t event_mask,
    uft_audit_severity_t min_severity
);

/**
 * @brief Export session to JSON file
 * @param session Session
 * @param path Output path
 * @return 0 on success
 */
int uft_audit_export_json(
    const uft_audit_session_t* session,
    const char* path
);

/**
 * @brief Export session to Markdown report
 * @param session Session
 * @param path Output path
 * @return 0 on success
 */
int uft_audit_export_markdown(
    const uft_audit_session_t* session,
    const char* path
);

/**
 * @brief Print session summary
 * @param session Session
 * @param out Output stream (NULL = stdout)
 */
void uft_audit_print_summary(
    const uft_audit_session_t* session,
    FILE* out
);

/* ═══════════════════════════════════════════════════════════════════════════
 * Load/Replay
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Load session from binary log
 * @param path Log file path
 * @return Loaded session or NULL
 */
uft_audit_session_t* uft_audit_session_load(const char* path);

/**
 * @brief Verify session integrity
 * @param session Session to verify
 * @return 0 if valid, error code otherwise
 */
int uft_audit_verify_session(const uft_audit_session_t* session);

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get event type name
 * @param event Event type
 * @return Event name string
 */
const char* uft_audit_event_name(uft_audit_event_t event);

/**
 * @brief Get severity name
 * @param severity Severity level
 * @return Severity name string
 */
const char* uft_audit_severity_name(uft_audit_severity_t severity);

/**
 * @brief Format timestamp
 * @param timestamp Unix timestamp
 * @param buffer Output buffer
 * @param size Buffer size
 * @return buffer pointer
 */
char* uft_audit_format_time(time_t timestamp, char* buffer, size_t size);

/* ═══════════════════════════════════════════════════════════════════════════
 * Global Session (Convenience)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Set global audit session
 * @param session Session (NULL to disable)
 */
void uft_audit_set_global(uft_audit_session_t* session);

/**
 * @brief Get global audit session
 * @return Global session or NULL
 */
uft_audit_session_t* uft_audit_get_global(void);

/**
 * @brief Log to global session (convenience macro)
 */
#define UFT_AUDIT_LOG(event, sev, desc) \
    do { \
        uft_audit_session_t* _s = uft_audit_get_global(); \
        if (_s) uft_audit_log(_s, (event), (sev), (desc)); \
    } while(0)

#define UFT_AUDIT_LOG_TRACK(event, sev, cyl, head, desc) \
    do { \
        uft_audit_session_t* _s = uft_audit_get_global(); \
        if (_s) uft_audit_log_track(_s, (event), (sev), (cyl), (head), (desc)); \
    } while(0)

#define UFT_AUDIT_LOG_SECTOR(event, sev, cyl, head, sec, desc) \
    do { \
        uft_audit_session_t* _s = uft_audit_get_global(); \
        if (_s) uft_audit_log_sector(_s, (event), (sev), (cyl), (head), (sec), (desc)); \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* UFT_AUDIT_TRAIL_H */

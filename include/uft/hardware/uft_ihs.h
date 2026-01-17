/**
 * @file uft_ihs.h
 * @brief Index Hole Sensor (IHS) Support for C64 Disk Preservation
 * 
 * Provides precise track alignment using index hole detection.
 * Supports SuperCard+ (SC+) compatible IHS hardware for 1541/1571 drives.
 * 
 * Reference: nibtools by Pete Rittwage (c64preservation.com)
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_IHS_H
#define UFT_IHS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * IHS Drive Commands (SuperCard+ Protocol)
 * ============================================================================ */

/** Turn IHS on */
#define IHS_CMD_ON              0x10

/** Turn IHS off */
#define IHS_CMD_OFF             0x11

/** Check IHS presence (long timeout) */
#define IHS_CMD_PRESENT2        0x12

/** Check IHS presence (with parallel burst read) */
#define IHS_CMD_PRESENT         0x13

/** Deep bitrate analysis */
#define IHS_CMD_DBR_ANALYSIS    0x14

/** Read memory */
#define IHS_CMD_READ_MEM        0x15

/** Read track with IHS (SC+ mode) */
#define IHS_CMD_READ_SCP        0x16

/* ============================================================================
 * IHS Status Codes
 * ============================================================================ */

/** IHS detected and working */
#define IHS_STATUS_OK           0x00

/** Index hole not detected (or sensor not working) */
#define IHS_STATUS_NO_HOLE      0x08

/** IHS disabled (must enable first) */
#define IHS_STATUS_DISABLED     0x10

/** Unknown error */
#define IHS_STATUS_ERROR        0x63

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum track data size */
#define IHS_MAX_TRACK_SIZE      0x2000

/** Number of passes for deep bitrate analysis */
#define IHS_DBR_PASSES          16

/** Default timeout for IHS operations (ms) */
#define IHS_DEFAULT_TIMEOUT     5000

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief IHS detection result
 */
typedef enum {
    IHS_RESULT_DETECTED     = 0,    /**< IHS detected */
    IHS_RESULT_NO_HOLE      = 1,    /**< Hole not detected */
    IHS_RESULT_DISABLED     = 2,    /**< IHS disabled */
    IHS_RESULT_NOT_PRESENT  = 3,    /**< IHS hardware not present */
    IHS_RESULT_TIMEOUT      = 4,    /**< Operation timed out */
    IHS_RESULT_ERROR        = 5,    /**< Unknown error */
} ihs_result_t;

/**
 * @brief Track alignment analysis result
 */
typedef struct {
    int         track;              /**< Track number */
    int         halftrack;          /**< Halftrack number */
    uint8_t     bitrate;            /**< Detected bitrate/density */
    int         pre_sync_lo;        /**< Pre-sync bytes (low) */
    int         pre_sync_hi;        /**< Pre-sync bytes (high) */
    int         sync_count_lo;      /**< Sync count (low) */
    int         sync_count_hi;      /**< Sync count (high) */
    uint8_t     data_bytes[5];      /**< First 5 data bytes after sync */
    bool        is_killer;          /**< Killer track (all sync) */
    bool        no_sync;            /**< No sync found */
} ihs_track_analysis_t;

/**
 * @brief Full disk alignment report
 */
typedef struct {
    int                     num_tracks;             /**< Number of tracks analyzed */
    bool                    has_halftracks;         /**< Halftracks included */
    ihs_track_analysis_t    tracks[84];             /**< Per-track analysis */
    char                    description[512];       /**< Summary description */
} ihs_alignment_report_t;

/**
 * @brief Deep bitrate analysis result
 */
typedef struct {
    int         track;              /**< Track number */
    uint8_t     densities[IHS_DBR_PASSES];  /**< Detected densities per pass */
    int         density_counts[4];  /**< Count per density (0-3) */
    uint8_t     best_density;       /**< Most frequent density */
    float       confidence;         /**< Confidence percentage */
} ihs_bitrate_analysis_t;

/**
 * @brief IHS hardware interface (abstract)
 */
typedef struct ihs_interface {
    void        *context;           /**< Implementation-specific context */
    
    /** Send command to drive */
    int (*send_cmd)(struct ihs_interface *iface, uint8_t cmd, 
                    const uint8_t *data, size_t len);
    
    /** Read response byte */
    int (*read_byte)(struct ihs_interface *iface, uint8_t *byte);
    
    /** Read track data */
    int (*read_track)(struct ihs_interface *iface, uint8_t *buffer, 
                      size_t max_len, size_t *actual_len);
    
    /** Motor control */
    int (*motor_on)(struct ihs_interface *iface);
    int (*motor_off)(struct ihs_interface *iface);
    
    /** Step to track */
    int (*step_to)(struct ihs_interface *iface, int halftrack);
    
    /** Set density */
    int (*set_density)(struct ihs_interface *iface, uint8_t density);
    
    /** Close interface */
    void (*close)(struct ihs_interface *iface);
} ihs_interface_t;

/**
 * @brief IHS session
 */
typedef struct {
    ihs_interface_t *iface;         /**< Hardware interface */
    ihs_result_t    last_result;    /**< Last operation result */
    bool            ihs_enabled;    /**< IHS currently enabled */
    int             current_track;  /**< Current head position */
    uint8_t         current_density;/**< Current density setting */
    int             timeout_ms;     /**< Operation timeout */
} ihs_session_t;

/* ============================================================================
 * API Functions - Session Management
 * ============================================================================ */

/**
 * @brief Create IHS session with interface
 * @param iface Hardware interface (takes ownership)
 * @return New session, or NULL on error
 */
ihs_session_t *ihs_session_create(ihs_interface_t *iface);

/**
 * @brief Close IHS session
 * @param session Session to close
 */
void ihs_session_close(ihs_session_t *session);

/**
 * @brief Set operation timeout
 * @param session IHS session
 * @param timeout_ms Timeout in milliseconds
 */
void ihs_set_timeout(ihs_session_t *session, int timeout_ms);

/* ============================================================================
 * API Functions - IHS Control
 * ============================================================================ */

/**
 * @brief Check for IHS presence
 * @param session IHS session
 * @param keep_on Keep IHS enabled after check
 * @return Detection result
 */
ihs_result_t ihs_check_presence(ihs_session_t *session, bool keep_on);

/**
 * @brief Enable IHS
 * @param session IHS session
 * @return 0 on success
 */
int ihs_enable(ihs_session_t *session);

/**
 * @brief Disable IHS
 * @param session IHS session
 * @return 0 on success
 */
int ihs_disable(ihs_session_t *session);

/**
 * @brief Check if IHS is enabled
 * @param session IHS session
 * @return true if enabled
 */
bool ihs_is_enabled(const ihs_session_t *session);

/* ============================================================================
 * API Functions - Track Reading
 * ============================================================================ */

/**
 * @brief Read track with IHS alignment
 * @param session IHS session
 * @param halftrack Halftrack number (2-84)
 * @param buffer Output buffer
 * @param max_len Maximum buffer size
 * @param actual_len Output: actual length read
 * @param density Output: detected density
 * @return 0 on success
 */
int ihs_read_track(ihs_session_t *session, int halftrack,
                   uint8_t *buffer, size_t max_len,
                   size_t *actual_len, uint8_t *density);

/**
 * @brief Scan track density
 * @param session IHS session
 * @param halftrack Halftrack number
 * @param density Output: detected density
 * @return 0 on success
 */
int ihs_scan_density(ihs_session_t *session, int halftrack, uint8_t *density);

/* ============================================================================
 * API Functions - Track Analysis
 * ============================================================================ */

/**
 * @brief Analyze single track alignment
 * @param session IHS session
 * @param halftrack Halftrack number
 * @param result Output analysis result
 * @return 0 on success
 */
int ihs_analyze_track(ihs_session_t *session, int halftrack,
                      ihs_track_analysis_t *result);

/**
 * @brief Generate full disk alignment report
 * @param session IHS session
 * @param start_track Start halftrack (default 2)
 * @param end_track End halftrack (default 70)
 * @param include_halftracks Include halftracks
 * @param report Output report
 * @return 0 on success
 */
int ihs_alignment_report(ihs_session_t *session,
                         int start_track, int end_track,
                         bool include_halftracks,
                         ihs_alignment_report_t *report);

/**
 * @brief Deep bitrate analysis on track
 * @param session IHS session
 * @param halftrack Halftrack number
 * @param result Output analysis result
 * @return 0 on success
 */
int ihs_deep_bitrate_analysis(ihs_session_t *session, int halftrack,
                              ihs_bitrate_analysis_t *result);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get result name string
 * @param result Result code
 * @return Result name
 */
const char *ihs_result_name(ihs_result_t result);

/**
 * @brief Print alignment report to file
 * @param report Report to print
 * @param fp Output file (can be stdout)
 */
void ihs_print_report(const ihs_alignment_report_t *report, FILE *fp);

/**
 * @brief Get default bitrate for track
 * @param track Track number (1-42)
 * @return Default bitrate/density (0-3)
 */
uint8_t ihs_default_bitrate(int track);

/* ============================================================================
 * API Functions - Software IHS Emulation
 * ============================================================================ */

/**
 * @brief Analyze track buffer for sync alignment
 * Similar to IHS analysis but works on existing track data
 * @param track_data Track data buffer
 * @param track_len Track data length
 * @param result Output analysis result
 * @return 0 on success
 */
int ihs_analyze_buffer(const uint8_t *track_data, size_t track_len,
                       ihs_track_analysis_t *result);

/**
 * @brief Find index hole position in track data
 * Estimates where index hole would be based on data patterns
 * @param track_data Track data
 * @param track_len Track length
 * @param track Track number (for density estimation)
 * @return Estimated index position, or -1 if not determined
 */
int ihs_find_index_position(const uint8_t *track_data, size_t track_len, int track);

/**
 * @brief Rotate track data to align with index hole
 * @param track_data Track data (modified in place)
 * @param track_len Track length
 * @param index_pos Index position to rotate to start
 * @return 0 on success
 */
int ihs_rotate_to_index(uint8_t *track_data, size_t track_len, size_t index_pos);

/* ============================================================================
 * Factory Functions - Interface Backends
 * ============================================================================ */

/**
 * @brief Create null/dummy IHS interface (for testing)
 * @return Dummy interface
 */
ihs_interface_t *ihs_create_null_interface(void);

/**
 * @brief Create simulation interface from track data
 * @param track_data Array of track data pointers
 * @param num_tracks Number of tracks
 * @return Simulation interface
 */
ihs_interface_t *ihs_create_sim_interface(uint8_t **track_data, int num_tracks);

/* Note: Real hardware interfaces (OpenCBM, XUM1541, etc.) would be
 * implemented in separate modules that create ihs_interface_t instances */

#ifdef __cplusplus
}
#endif

#endif /* UFT_IHS_H */

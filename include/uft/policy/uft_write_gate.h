/**
 * @file uft_write_gate.h
 * @brief Write Safety Gate - Fail-Closed Policy Layer
 * @version 1.0.0
 * 
 * Enforces safety checks BEFORE any destructive operation:
 * 1. Format capability check (write allowed?)
 * 2. Drive diagnostics (hardware safe?)
 * 3. Recovery snapshot (backup created?)
 * 
 * "Bei uns geht kein Bit verloren"
 */

#ifndef UFT_WRITE_GATE_H
#define UFT_WRITE_GATE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_error.h"
#include "uft/core/uft_snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Gate Status Codes
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_GATE_OK = 0,                    /**< All checks passed */
    UFT_GATE_FORMAT_READONLY = -200,    /**< Format doesn't support write */
    UFT_GATE_DRIVE_UNSAFE = -201,       /**< Drive diagnostics failed */
    UFT_GATE_SNAPSHOT_FAILED = -202,    /**< Couldn't create backup */
    UFT_GATE_VERIFY_FAILED = -203,      /**< Snapshot verification failed */
    UFT_GATE_NEEDS_OVERRIDE = -204,     /**< Requires explicit user override */
    UFT_GATE_PRECHECK_FAILED = -205,    /**< General precheck failure */
} uft_gate_status_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Capabilities (for gate check)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_FMT_CAP_READ      = (1u << 0),  /**< Can read this format */
    UFT_FMT_CAP_WRITE     = (1u << 1),  /**< Can write this format */
    UFT_FMT_CAP_PHYSICAL  = (1u << 2),  /**< Physical disk format */
    UFT_FMT_CAP_LOGICAL   = (1u << 3),  /**< Logical container format */
    UFT_FMT_CAP_PROTECTED = (1u << 4),  /**< Copy-protected */
    UFT_FMT_CAP_VERIFY    = (1u << 5),  /**< Supports verification */
} uft_format_cap_t;

typedef struct {
    const char *format_name;     /**< Detected format name */
    uint32_t capabilities;       /**< Bitmask of uft_format_cap_t */
    uint32_t confidence;         /**< Detection confidence (0-1000) */
    char reason[128];            /**< Detection reason */
} uft_format_probe_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Drive Diagnostics (for gate check)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_DRIVE_DIAG_UNSTABLE_RPM   = (1u << 0),  /**< RPM out of spec */
    UFT_DRIVE_DIAG_BAD_INDEX      = (1u << 1),  /**< Index pulse issues */
    UFT_DRIVE_DIAG_BAD_SEEK       = (1u << 2),  /**< Seek errors */
    UFT_DRIVE_DIAG_WRITE_UNSAFE   = (1u << 3),  /**< Write not recommended */
    UFT_DRIVE_DIAG_NO_DISK        = (1u << 4),  /**< No disk in drive */
    UFT_DRIVE_DIAG_WRITE_PROTECT  = (1u << 5),  /**< Disk is write-protected */
} uft_drive_diag_flag_t;

typedef struct {
    double rpm_avg;              /**< Average RPM */
    double rpm_jitter;           /**< RPM variation (%) */
    double index_jitter_us;      /**< Index pulse jitter (µs) */
    double seek_error_tracks;    /**< Seek error in tracks */
    uint32_t flags;              /**< Bitmask of uft_drive_diag_flag_t */
    char controller[64];         /**< Controller name */
} uft_drive_diag_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Gate Policy
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    bool require_format_check;      /**< Check format capabilities */
    bool require_drive_diag;        /**< Run drive diagnostics */
    bool require_snapshot;          /**< Create recovery snapshot */
    bool allow_readonly_override;   /**< Allow override for RO formats */
    bool allow_unsafe_drive;        /**< Allow override for unsafe drives */
    bool strict_mode;               /**< Fail on any warning */
    uint32_t min_confidence;        /**< Minimum format confidence (0-1000) */
} uft_write_gate_policy_t;

/**
 * @brief Default strict policy
 */
#define UFT_GATE_POLICY_STRICT { \
    .require_format_check = true, \
    .require_drive_diag = true, \
    .require_snapshot = true, \
    .allow_readonly_override = false, \
    .allow_unsafe_drive = false, \
    .strict_mode = true, \
    .min_confidence = 800 \
}

/**
 * @brief Relaxed policy (for testing/development)
 */
#define UFT_GATE_POLICY_RELAXED { \
    .require_format_check = true, \
    .require_drive_diag = false, \
    .require_snapshot = true, \
    .allow_readonly_override = true, \
    .allow_unsafe_drive = false, \
    .strict_mode = false, \
    .min_confidence = 500 \
}

/**
 * @brief Image-only policy (no hardware)
 */
#define UFT_GATE_POLICY_IMAGE_ONLY { \
    .require_format_check = true, \
    .require_drive_diag = false, \
    .require_snapshot = true, \
    .allow_readonly_override = false, \
    .allow_unsafe_drive = false, \
    .strict_mode = false, \
    .min_confidence = 700 \
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Gate Result
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_gate_status_t status;        /**< Final gate decision */
    uft_format_probe_t format;       /**< Format detection result */
    uft_drive_diag_t drive;          /**< Drive diagnostics result */
    uft_snapshot_t snapshot;         /**< Recovery snapshot info */
    char decision_reason[256];       /**< Human-readable explanation */
    bool override_required;          /**< True if user override needed */
    uint32_t checks_passed;          /**< Bitmask of passed checks */
    uint32_t checks_failed;          /**< Bitmask of failed checks */
} uft_write_gate_result_t;

/* Check flags for result */
typedef enum {
    UFT_CHECK_FORMAT     = (1u << 0),
    UFT_CHECK_DRIVE      = (1u << 1),
    UFT_CHECK_SNAPSHOT   = (1u << 2),
    UFT_CHECK_VERIFY     = (1u << 3),
} uft_gate_check_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Run pre-write safety gate
 * 
 * This MUST be called before any destructive operation.
 * Returns UFT_GATE_OK only if ALL enabled checks pass.
 * 
 * @param policy Gate policy
 * @param image_data Image data to write (for format detection + snapshot)
 * @param image_len Image data length
 * @param snapshot_dir Directory for recovery snapshot
 * @param snapshot_prefix Snapshot filename prefix
 * @param result Output: detailed result
 * @return UFT_GATE_OK if write allowed, error code otherwise
 */
uft_gate_status_t uft_write_gate_precheck(
    const uft_write_gate_policy_t *policy,
    const uint8_t *image_data,
    size_t image_len,
    const char *snapshot_dir,
    const char *snapshot_prefix,
    uft_write_gate_result_t *result);

/**
 * @brief Run pre-write gate with drive diagnostics
 * 
 * @param policy Gate policy
 * @param image_data Image data
 * @param image_len Image length
 * @param drive_diag Pre-collected drive diagnostics (or NULL)
 * @param snapshot_dir Snapshot directory
 * @param snapshot_prefix Snapshot prefix
 * @param result Output result
 * @return Gate status
 */
uft_gate_status_t uft_write_gate_precheck_with_diag(
    const uft_write_gate_policy_t *policy,
    const uint8_t *image_data,
    size_t image_len,
    const uft_drive_diag_t *drive_diag,
    const char *snapshot_dir,
    const char *snapshot_prefix,
    uft_write_gate_result_t *result);

/**
 * @brief Probe format from bytes
 * @param data Image data
 * @param len Data length
 * @param out Output format probe
 * @return UFT_SUCCESS or error
 */
uft_error_t uft_write_gate_probe_format(
    const uint8_t *data,
    size_t len,
    uft_format_probe_t *out);

/**
 * @brief Get status string
 * @param status Gate status
 * @return Human-readable string
 */
const char *uft_gate_status_str(uft_gate_status_t status);

/**
 * @brief Check if result allows write with user override
 * @param result Gate result
 * @return true if override is possible
 */
bool uft_write_gate_can_override(const uft_write_gate_result_t *result);

/**
 * @brief Confirm user override
 * @param result Gate result to modify
 * @param reason User's override reason
 * @return UFT_GATE_OK if override accepted
 */
uft_gate_status_t uft_write_gate_apply_override(
    uft_write_gate_result_t *result,
    const char *reason);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WRITE_GATE_H */

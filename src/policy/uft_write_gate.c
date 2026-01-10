/**
 * @file uft_write_gate.c
 * @brief Write Safety Gate Implementation
 * @version 1.0.0
 * 
 * Fail-closed policy layer for all write operations.
 */

#include "uft/policy/uft_write_gate.h"
#include "uft/core/uft_sha256.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Detection (simplified - integrates with existing UFT format system)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Format signatures */
typedef struct {
    const char *name;
    size_t min_size;
    size_t max_size;
    const uint8_t *magic;
    size_t magic_len;
    size_t magic_offset;
    uint32_t caps;
    uint32_t confidence;
} format_sig_t;

static const uint8_t MAGIC_ADF_BOOT[] = {0x44, 0x4F, 0x53};  /* "DOS" */
static const uint8_t MAGIC_D64_BAM[] = {0x12, 0x01, 0x41};   /* Track 18, sector 0 marker */
static const uint8_t MAGIC_SCL[] = {'S','I','N','C','L','A','I','R'};
static const uint8_t MAGIC_TRD[] = {0x00};  /* Placeholder - size-based */
static const uint8_t MAGIC_SCP[] = {'S','C','P'};
static const uint8_t MAGIC_G64[] = {'G','C','R','-','1','5','4','1'};
static const uint8_t MAGIC_WOZ[] = {'W','O','Z','1'};
static const uint8_t MAGIC_WOZ2[] = {'W','O','Z','2'};
static const uint8_t MAGIC_NIB[] = {0};  /* Size-based */

static const format_sig_t FORMAT_SIGS[] = {
    /* Amiga */
    {"ADF (Amiga DD)", 901120, 901120, NULL, 0, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL, 900},
    {"ADF (Amiga HD)", 1802240, 1802240, NULL, 0, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL, 900},
    
    /* Commodore */
    {"D64 (C64 1541)", 174848, 175531, NULL, 0, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL, 850},
    {"D71 (C128 1571)", 349696, 351062, NULL, 0, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL, 850},
    {"D81 (C128 1581)", 819200, 819200, NULL, 0, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL, 850},
    {"G64 (GCR Flux)", 0, 0, MAGIC_G64, 8, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL | UFT_FMT_CAP_PROTECTED, 950},
    
    /* ZX Spectrum */
    {"SCL (Sinclair)", 0, 0, MAGIC_SCL, 8, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_LOGICAL, 950},
    {"TRD (TR-DOS)", 655360, 655360, NULL, 0, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL, 800},
    
    /* Apple */
    {"WOZ (Apple II)", 0, 0, MAGIC_WOZ, 4, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL | UFT_FMT_CAP_PROTECTED, 950},
    {"WOZ2 (Apple II)", 0, 0, MAGIC_WOZ2, 4, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL | UFT_FMT_CAP_PROTECTED, 950},
    {"NIB (Apple II)", 232960, 232960, NULL, 0, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_PHYSICAL, 800},  /* NIB is read-only in most tools */
    
    /* Flux formats */
    {"SCP (SuperCard Pro)", 0, 0, MAGIC_SCP, 3, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL | UFT_FMT_CAP_PROTECTED, 950},
    
    /* IBM PC */
    {"IMG (PC 720K)", 737280, 737280, NULL, 0, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL, 700},
    {"IMG (PC 1.44M)", 1474560, 1474560, NULL, 0, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL, 700},
    {"XDF (IBM)", 1915904, 1915904, NULL, 0, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL | UFT_FMT_CAP_PROTECTED, 850},
    {"DMF (MS)", 1720320, 1720320, NULL, 0, 0,
     UFT_FMT_CAP_READ | UFT_FMT_CAP_WRITE | UFT_FMT_CAP_PHYSICAL, 850},
    
    {NULL, 0, 0, NULL, 0, 0, 0, 0}
};

static void set_reason(uft_write_gate_result_t *r, const char *fmt, ...) {
    if (!r) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(r->decision_reason, sizeof(r->decision_reason), fmt, args);
    va_end(args);
}

uft_error_t uft_write_gate_probe_format(const uint8_t *data, size_t len,
                                        uft_format_probe_t *out) {
    if (!data || !out) return UFT_ERR_INVALID_ARG;
    
    memset(out, 0, sizeof(*out));
    out->format_name = "Unknown";
    out->confidence = 0;
    out->capabilities = UFT_FMT_CAP_READ;  /* Default: read-only */
    snprintf(out->reason, sizeof(out->reason), "No matching format signature");
    
    const format_sig_t *best = NULL;
    uint32_t best_conf = 0;
    
    for (const format_sig_t *sig = FORMAT_SIGS; sig->name; sig++) {
        bool match = false;
        
        /* Check magic bytes first */
        if (sig->magic && sig->magic_len > 0) {
            if (len >= sig->magic_offset + sig->magic_len) {
                if (memcmp(data + sig->magic_offset, sig->magic, sig->magic_len) == 0) {
                    match = true;
                }
            }
        }
        /* Then check size */
        else if (sig->min_size > 0 && sig->max_size > 0) {
            if (len >= sig->min_size && len <= sig->max_size) {
                match = true;
            }
        }
        
        if (match && sig->confidence > best_conf) {
            best = sig;
            best_conf = sig->confidence;
        }
    }
    
    if (best) {
        out->format_name = best->name;
        out->confidence = best->confidence;
        out->capabilities = best->caps;
        snprintf(out->reason, sizeof(out->reason), "Matched: %s", best->name);
        return UFT_SUCCESS;
    }
    
    return UFT_ERR_FORMAT_DETECT;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Gate Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_gate_status_str(uft_gate_status_t status) {
    switch (status) {
        case UFT_GATE_OK: return "OK - Write allowed";
        case UFT_GATE_FORMAT_READONLY: return "Format is read-only";
        case UFT_GATE_DRIVE_UNSAFE: return "Drive diagnostics failed";
        case UFT_GATE_SNAPSHOT_FAILED: return "Snapshot creation failed";
        case UFT_GATE_VERIFY_FAILED: return "Snapshot verification failed";
        case UFT_GATE_NEEDS_OVERRIDE: return "Requires user override";
        case UFT_GATE_PRECHECK_FAILED: return "Precheck failed";
        default: return "Unknown gate status";
    }
}

uft_gate_status_t uft_write_gate_precheck(
    const uft_write_gate_policy_t *policy,
    const uint8_t *image_data,
    size_t image_len,
    const char *snapshot_dir,
    const char *snapshot_prefix,
    uft_write_gate_result_t *result) {
    
    return uft_write_gate_precheck_with_diag(
        policy, image_data, image_len, NULL,
        snapshot_dir, snapshot_prefix, result);
}

uft_gate_status_t uft_write_gate_precheck_with_diag(
    const uft_write_gate_policy_t *policy,
    const uint8_t *image_data,
    size_t image_len,
    const uft_drive_diag_t *drive_diag,
    const char *snapshot_dir,
    const char *snapshot_prefix,
    uft_write_gate_result_t *result) {
    
    if (!policy || !result) {
        return UFT_GATE_PRECHECK_FAILED;
    }
    
    memset(result, 0, sizeof(*result));
    result->status = UFT_GATE_OK;
    
    /* ─────────────────────────────────────────────────────────────────────────
     * Step 1: Format Detection & Capability Check
     * ───────────────────────────────────────────────────────────────────────── */
    if (policy->require_format_check) {
        if (!image_data || image_len == 0) {
            set_reason(result, "No image data provided for format check");
            result->status = UFT_GATE_PRECHECK_FAILED;
            result->checks_failed |= UFT_CHECK_FORMAT;
            return result->status;
        }
        
        uft_error_t err = uft_write_gate_probe_format(image_data, image_len, &result->format);
        if (err != UFT_SUCCESS) {
            set_reason(result, "Format detection failed: unknown format");
            result->status = UFT_GATE_PRECHECK_FAILED;
            result->checks_failed |= UFT_CHECK_FORMAT;
            return result->status;
        }
        
        /* Check confidence threshold */
        if (result->format.confidence < policy->min_confidence) {
            set_reason(result, "Format confidence too low: %u < %u",
                      result->format.confidence, policy->min_confidence);
            result->status = UFT_GATE_PRECHECK_FAILED;
            result->checks_failed |= UFT_CHECK_FORMAT;
            return result->status;
        }
        
        /* Check write capability */
        if (!(result->format.capabilities & UFT_FMT_CAP_WRITE)) {
            if (policy->allow_readonly_override) {
                set_reason(result, "Format '%s' is read-only - override required",
                          result->format.format_name);
                result->override_required = true;
                result->status = UFT_GATE_FORMAT_READONLY;
                result->checks_failed |= UFT_CHECK_FORMAT;
                /* Continue to create snapshot anyway */
            } else {
                set_reason(result, "Format '%s' is read-only - write blocked",
                          result->format.format_name);
                result->status = UFT_GATE_FORMAT_READONLY;
                result->checks_failed |= UFT_CHECK_FORMAT;
                return result->status;
            }
        } else {
            result->checks_passed |= UFT_CHECK_FORMAT;
        }
    }
    
    /* ─────────────────────────────────────────────────────────────────────────
     * Step 2: Drive Diagnostics
     * ───────────────────────────────────────────────────────────────────────── */
    if (policy->require_drive_diag) {
        if (!drive_diag) {
            if (policy->strict_mode) {
                set_reason(result, "Drive diagnostics required but not provided");
                result->status = UFT_GATE_NEEDS_OVERRIDE;
                result->override_required = true;
                result->checks_failed |= UFT_CHECK_DRIVE;
                return result->status;
            }
            /* Non-strict: skip drive check */
        } else {
            /* Copy diagnostics to result */
            result->drive = *drive_diag;
            
            /* Check for unsafe conditions */
            if (drive_diag->flags & UFT_DRIVE_DIAG_WRITE_UNSAFE) {
                if (policy->allow_unsafe_drive) {
                    set_reason(result, "Drive marked UNSAFE - override required");
                    result->override_required = true;
                    result->status = UFT_GATE_DRIVE_UNSAFE;
                    result->checks_failed |= UFT_CHECK_DRIVE;
                } else {
                    set_reason(result, "Drive diagnostics: WRITE_UNSAFE flag set");
                    result->status = UFT_GATE_DRIVE_UNSAFE;
                    result->checks_failed |= UFT_CHECK_DRIVE;
                    return result->status;
                }
            } else if (drive_diag->flags & UFT_DRIVE_DIAG_WRITE_PROTECT) {
                set_reason(result, "Disk is write-protected");
                result->status = UFT_GATE_DRIVE_UNSAFE;
                result->checks_failed |= UFT_CHECK_DRIVE;
                return result->status;
            } else if (drive_diag->flags & UFT_DRIVE_DIAG_NO_DISK) {
                set_reason(result, "No disk in drive");
                result->status = UFT_GATE_DRIVE_UNSAFE;
                result->checks_failed |= UFT_CHECK_DRIVE;
                return result->status;
            } else {
                result->checks_passed |= UFT_CHECK_DRIVE;
            }
        }
    }
    
    /* ─────────────────────────────────────────────────────────────────────────
     * Step 3: Recovery Snapshot
     * ───────────────────────────────────────────────────────────────────────── */
    if (policy->require_snapshot) {
        if (!snapshot_dir || !snapshot_prefix) {
            set_reason(result, "Snapshot required but directory/prefix not provided");
            result->status = UFT_GATE_SNAPSHOT_FAILED;
            result->checks_failed |= UFT_CHECK_SNAPSHOT;
            return result->status;
        }
        
        if (!image_data || image_len == 0) {
            set_reason(result, "Snapshot required but no image data");
            result->status = UFT_GATE_SNAPSHOT_FAILED;
            result->checks_failed |= UFT_CHECK_SNAPSHOT;
            return result->status;
        }
        
        uft_snapshot_opts_t opts = UFT_SNAPSHOT_OPTS_DEFAULT;
        uft_error_t err = uft_snapshot_create(
            snapshot_dir, snapshot_prefix,
            image_data, image_len,
            &opts, &result->snapshot);
        
        if (err != UFT_SUCCESS) {
            set_reason(result, "Snapshot creation failed: %d", err);
            result->status = UFT_GATE_SNAPSHOT_FAILED;
            result->checks_failed |= UFT_CHECK_SNAPSHOT;
            return result->status;
        }
        
        /* Verify snapshot */
        err = uft_snapshot_verify(&result->snapshot);
        if (err != UFT_SUCCESS) {
            set_reason(result, "Snapshot verification failed: %d", err);
            result->status = UFT_GATE_VERIFY_FAILED;
            result->checks_failed |= UFT_CHECK_VERIFY;
            return result->status;
        }
        
        result->checks_passed |= UFT_CHECK_SNAPSHOT | UFT_CHECK_VERIFY;
    }
    
    /* ─────────────────────────────────────────────────────────────────────────
     * Final Decision
     * ───────────────────────────────────────────────────────────────────────── */
    if (result->override_required) {
        /* Some check failed but can be overridden */
        return result->status;
    }
    
    if (result->checks_failed == 0) {
        set_reason(result, "All checks passed - write allowed");
        result->status = UFT_GATE_OK;
    }
    
    return result->status;
}

bool uft_write_gate_can_override(const uft_write_gate_result_t *result) {
    if (!result) return false;
    return result->override_required;
}

uft_gate_status_t uft_write_gate_apply_override(
    uft_write_gate_result_t *result,
    const char *reason) {
    
    if (!result || !reason) {
        return UFT_GATE_PRECHECK_FAILED;
    }
    
    if (!result->override_required) {
        return result->status;  /* No override needed */
    }
    
    /* Log override reason */
    char new_reason[256];
    snprintf(new_reason, sizeof(new_reason),
             "OVERRIDE APPLIED: %s (original: %s)",
             reason, result->decision_reason);
    strncpy(result->decision_reason, new_reason, sizeof(result->decision_reason) - 1);
    
    result->override_required = false;
    result->status = UFT_GATE_OK;
    
    return UFT_GATE_OK;
}

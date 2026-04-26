/* =====================================================================
 * GENERATED FILE — DO NOT EDIT BY HAND.
 * Source of truth: data/errors.tsv
 * Regenerate with: make generate  (or scripts/verify_errors_ssot.sh)
 * Any manual edits will be overwritten on the next generator run.
 * ===================================================================== */
/**
 * @file uft_error_strings.c
 * @brief uft_strerror() implementation (GENERATED).
 *
 * Generated from data/errors.tsv by
 * scripts/generators/gen_errors_strings.py.  Do not edit by hand.
 */

#include "uft/uft_error.h"

#include <stddef.h>

struct uft_err_entry {
    int         code;
    const char* name;
    const char* description;
    const char* category;
};

static const struct uft_err_entry UFT_ERR_TABLE[] = {
    {     0, "UFT_SUCCESS", "Operation completed successfully", "MISC" },
    {    -1, "UFT_ERR_INVALID_ARG", "Invalid argument (NULL pointer, out of range, etc.)", "ARG" },
    {    -2, "UFT_ERR_BUFFER_TOO_SMALL", "Required buffer too small", "ARG" },
    {    -3, "UFT_ERR_INVALID_PATH", "Invalid path or filename", "ARG" },
    {    -5, "UFT_ERR_NULL_POINTER", "Null pointer passed where non-null required (distinct diagnostic from generic INVALID_ARG; 86 call sites)", "ARG" },
    {   -10, "UFT_ERR_IO", "General I/O error", "IO" },
    {   -11, "UFT_ERR_FILE_NOT_FOUND", "File not found", "IO" },
    {   -12, "UFT_ERR_PERMISSION", "Permission denied", "IO" },
    {   -13, "UFT_ERR_FILE_EXISTS", "File already exists", "IO" },
    {   -14, "UFT_ERR_EOF", "End of file reached", "IO" },
    {   -20, "UFT_ERR_FORMAT", "Unknown or invalid format", "FORMAT" },
    {   -21, "UFT_ERR_FORMAT_DETECT", "Format detection failed", "FORMAT" },
    {   -22, "UFT_ERR_FORMAT_VARIANT", "Unsupported format variant", "FORMAT" },
    {   -23, "UFT_ERR_CORRUPTED", "Corrupted or invalid data", "FORMAT" },
    {   -24, "UFT_ERR_CRC", "CRC/checksum mismatch", "FORMAT" },
    {   -30, "UFT_ERR_MEMORY", "Memory allocation failed", "RESOURCE" },
    {   -31, "UFT_ERR_RESOURCE", "Resource limit exceeded", "RESOURCE" },
    {   -32, "UFT_ERR_BUSY", "Resource busy", "RESOURCE" },
    {   -40, "UFT_ERR_NOT_SUPPORTED", "Feature not supported", "FEATURE" },
    {   -41, "UFT_ERR_NOT_IMPLEMENTED", "Feature not implemented", "FEATURE" },
    {   -42, "UFT_ERR_NOT_PERMITTED", "Operation not permitted in current state", "FEATURE" },
    {   -50, "UFT_ERR_HARDWARE", "Hardware communication error", "HARDWARE" },
    {   -51, "UFT_ERR_USB", "USB error", "HARDWARE" },
    {   -52, "UFT_ERR_DEVICE_NOT_FOUND", "Device not found", "HARDWARE" },
    {   -53, "UFT_ERR_TIMEOUT", "Operation timed out", "HARDWARE" },
    {   -90, "UFT_ERR_INTERNAL", "Internal error (should not happen)", "INTERNAL" },
    {   -91, "UFT_ERR_ASSERTION", "Assertion failed", "INTERNAL" },
    {  -100, "UFT_ERR_UNKNOWN", "Unknown error", "MISC" },
    {   -25, "UFT_ERR_FORMAT_INVALID", "Format structurally invalid (magic/header mismatch)", "FORMAT" },
    {   -26, "UFT_ERR_UNKNOWN_FORMAT", "Format could not be identified from content", "FORMAT" },
    {   -27, "UFT_ERR_ENCODING", "Encoding error (MFM/FM/GCR bit-level decode failed)", "FORMAT" },
    {   -28, "UFT_ERR_LONG_TRACK", "Track exceeds standard length (possible protection)", "FORMAT" },
    {   -29, "UFT_ERR_NON_STANDARD", "Non-standard format variant", "FORMAT" },
    {   -60, "UFT_ERR_SYNC_LOST", "PLL/sync lost during decode", "FORMAT" },
    {   -61, "UFT_ERR_WEAK_BITS", "Weak bits detected in data field", "FORMAT" },
    {   -62, "UFT_ERR_TIMING", "Timing anomaly (splice, density-mix)", "FORMAT" },
    {   -63, "UFT_ERR_ID_MISMATCH", "Sector ID header mismatch", "FORMAT" },
    {   -64, "UFT_ERR_DELETED_DATA", "Deleted-data address mark (0xF8)", "FORMAT" },
    {   -65, "UFT_ERR_MISSING_SECTOR", "Expected sector not found on track", "FORMAT" },
    {   -66, "UFT_ERR_INCOMPLETE", "Incomplete data (truncated sector/track)", "FORMAT" },
    {   -67, "UFT_ERR_PLL_UNLOCK", "PLL never achieved lock", "FORMAT" },
    {   -80, "UFT_ERR_PROTECTION", "Copy-protection scheme triggered", "PROTECTION" },
    {   -81, "UFT_ERR_COPY_DENIED", "Copy refused due to protection policy", "PROTECTION" },
    {   -54, "UFT_ERR_WRITE_PROTECT", "Disk is write-protected", "HARDWARE" },
    {   -55, "UFT_ERR_WRITE_FAULT", "Drive reports write fault", "HARDWARE" },
    {   -56, "UFT_ERR_TRACK_OVERFLOW", "Track buffer overflow during write", "HARDWARE" },
    {    -4, "UFT_ERR_VERIFY_FAIL", "Verification compare failed", "ARG" },
    {   -15, "UFT_ERR_FILE_OPEN", "Cannot open file", "IO" },
    {   -16, "UFT_ERR_FILE_READ", "File read error", "IO" },
    {   -17, "UFT_ERR_FILE_CREATE", "Cannot create file", "IO" },
    {   -18, "UFT_ERR_FILE_SEEK", "File seek error (offset out of range or device failure)", "IO" },
    {   -19, "UFT_ERR_FILE_WRITE", "File write error", "IO" },
    {   -92, "UFT_ERR_CANCELLED", "Operation cancelled by user or policy", "MISC" },
    {   -93, "UFT_ERR_INVALID_STATE", "Object/context in invalid state for operation", "INTERNAL" },
    {   -94, "UFT_ERR_VERSION", "Version mismatch", "MISC" },
    {   -95, "UFT_ERR_PLUGIN_LOAD", "Plugin load failed (ABI / dlopen / init error)", "RESOURCE" },
    {   -96, "UFT_ERR_PLUGIN_NOT_FOUND", "Plugin not found by name or format id", "RESOURCE" },
    { 0, NULL, NULL, NULL }  /* sentinel */
};

const char* uft_strerror(uft_rc_t rc) {
    for (const struct uft_err_entry* e = UFT_ERR_TABLE; e->name != NULL; ++e) {
        if (e->code == (int)rc) {
            return e->description;
        }
    }
    return "Unknown UFT error code";
}

/* Extended lookup helpers (name + description) for diagnostic callers. */

const char* uft_error_name(uft_error_t err) {
    for (const struct uft_err_entry* e = UFT_ERR_TABLE; e->name != NULL; ++e) {
        if (e->code == (int)err) {
            return e->name;
        }
    }
    return "UFT_ERR_UNKNOWN";
}

const char* uft_error_desc(uft_error_t err) {
    return uft_strerror((uft_rc_t)err);
}

const char* uft_error_category(uft_error_t err) {
    for (const struct uft_err_entry* e = UFT_ERR_TABLE; e->name != NULL; ++e) {
        if (e->code == (int)err) {
            return e->category;
        }
    }
    return "UNKNOWN";
}

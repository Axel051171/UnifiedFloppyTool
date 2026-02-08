/**
 * @file uft.h
 * @brief UnifiedFloppyTool - Master Include Header
 * 
 * Inkludiert alle Core-APIs.
 * Für GUI-Entwickler: Nur uft_gui.h verwenden!
 * 
 * @note Version information is defined in uft_types.h
 */

#ifndef UFT_H
#define UFT_H

/* Core Types (includes version) */
#include "uft_types.h"

/* Error handling */
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get UFT library version string
 */
static inline const char* uft_version(void) {
    return UFT_VERSION_STRING;
}

/**
 * @brief Get UFT library version components
 */
static inline void uft_version_get(int *major, int *minor, int *patch) {
    if (major) *major = UFT_VERSION_MAJOR;
    if (minor) *minor = UFT_VERSION_MINOR;
    if (patch) *patch = UFT_VERSION_PATCH;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_H */

/**
 * @file uft_v3_bridge.h
 * @brief Public API for v3 parser bridge
 * 
 * The v3 parsers provide advanced features:
 * - Copy protection detection
 * - Weak bit handling
 * - Multi-revolution merging
 * - Detailed diagnostics
 * - Format conversion (G64→D64)
 */

#ifndef UFT_V3_BRIDGE_H
#define UFT_V3_BRIDGE_H

#include "uft/uft_formats_extended.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Handlers
 * Use these with the standard format handler API
 * ═══════════════════════════════════════════════════════════════════════════════ */

extern uft_format_handler_t uft_d64_v3_handler;
extern uft_format_handler_t uft_g64_v3_handler;
extern uft_format_handler_t uft_scp_v3_handler;

/* ═══════════════════════════════════════════════════════════════════════════════
 * D64 Extended Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect copy protection schemes
 * @param handle Handle from d64_v3_open
 * @param name Buffer for protection name
 * @param name_size Buffer size
 * @return true if protection detected
 */
bool uft_d64_v3_detect_protection(void* handle, char* name, size_t name_size);

/**
 * @brief Get diagnostic text
 * @param handle Handle from d64_v3_open
 * @return Allocated string (caller must free) or NULL
 */
char* uft_d64_v3_get_diagnosis(void* handle);

/**
 * @brief Write D64 image
 * @param handle Handle from d64_v3_open
 * @param out_size Output size
 * @return Allocated buffer (caller must free) or NULL
 */
uint8_t* uft_d64_v3_write(void* handle, size_t* out_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * G64 Extended Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_g64_v3_detect_protection(void* handle, char* name, size_t name_size);
uint8_t* uft_g64_v3_export_d64(void* handle, size_t* out_size);
uint8_t* uft_g64_v3_write(void* handle, size_t* out_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * SCP Extended Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_scp_v3_detect_protection(void* handle, char* name, size_t name_size);
uint8_t* uft_scp_v3_write(void* handle, size_t* out_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_V3_BRIDGE_H */

/**
 * @file uft_d64_adapter.h
 * @brief D64 Format Adapter for XDF-API
 * @version 1.0.0
 * 
 * Connects the D64 (C64) parser to the XDF Adapter system.
 */

#ifndef UFT_D64_ADAPTER_H
#define UFT_D64_ADAPTER_H

#include "uft/xdf/uft_xdf_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief D64 Format Adapter
 */
extern const uft_format_adapter_t uft_d64_adapter;

/**
 * @brief Initialize D64 adapter
 */
void uft_d64_adapter_init(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D64_ADAPTER_H */

/**
 * @file uft_adf_adapter.h
 * @brief ADF Format Adapter for XDF-API
 * @version 1.0.0
 * 
 * Connects the ADF parser to the XDF Adapter system.
 * 
 * Usage:
 *   uft_adapter_register(&uft_adf_adapter);
 *   // Then use via XDF-API
 */

#ifndef UFT_ADF_ADAPTER_H
#define UFT_ADF_ADAPTER_H

#include "uft/xdf/uft_xdf_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ADF Format Adapter
 * 
 * Register with: uft_adapter_register(&uft_adf_adapter);
 */
extern const uft_format_adapter_t uft_adf_adapter;

/**
 * @brief Initialize ADF adapter (call once at startup)
 */
void uft_adf_adapter_init(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ADF_ADAPTER_H */

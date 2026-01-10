/**
 * @file uft_trd_adapter.h
 * @brief TRD Format Adapter for XDF-API
 * @version 1.0.0
 * 
 * ZX Spectrum TR-DOS disk images.
 */

#ifndef UFT_TRD_ADAPTER_H
#define UFT_TRD_ADAPTER_H

#include "uft/xdf/uft_xdf_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const uft_format_adapter_t uft_trd_adapter;
void uft_trd_adapter_init(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRD_ADAPTER_H */

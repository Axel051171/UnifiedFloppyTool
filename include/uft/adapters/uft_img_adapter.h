/**
 * @file uft_img_adapter.h
 * @brief IMG/IMA Format Adapter for XDF-API
 * @version 1.0.0
 * 
 * PC floppy disk images (FAT12).
 */

#ifndef UFT_IMG_ADAPTER_H
#define UFT_IMG_ADAPTER_H

#include "uft/xdf/uft_xdf_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const uft_format_adapter_t uft_img_adapter;
void uft_img_adapter_init(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IMG_ADAPTER_H */

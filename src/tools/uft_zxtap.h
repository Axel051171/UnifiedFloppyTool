/**
 * @file uft_zxtap.h  
 * @brief ZX Spectrum TAP Format
 */
#ifndef UFT_ZXTAP_H
#define UFT_ZXTAP_H
#include <stdint.h>
#include <stddef.h>
typedef struct uft_tap_block uft_tap_block_t;
int uft_tap_read(const char *filename, uft_tap_block_t **blocks, size_t *count);
#endif

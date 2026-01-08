#ifndef UFT_FORMAT_DETECT_V2_H
#define UFT_FORMAT_DETECT_V2_H
#include <stdint.h>
#include <stddef.h>
typedef struct uft_format_info uft_format_info_t;
int uft_format_detect_v2(const uint8_t *data, size_t len, uft_format_info_t *info);
#endif

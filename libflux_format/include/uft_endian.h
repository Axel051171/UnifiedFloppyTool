// uft_endian.h - portable endian helpers (C11, no deps)
#ifndef UFT_ENDIAN_H
#define UFT_ENDIAN_H
#include <stdint.h>
static inline uint16_t uft_bswap16(uint16_t v){return (uint16_t)((v>>8)|(v<<8));}
static inline uint32_t uft_bswap32(uint32_t v){
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8)  | ((v & 0xFF000000u) >> 24);
}
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
  #define UFT_HOST_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#else
  #define UFT_HOST_LITTLE_ENDIAN 0
#endif
static inline uint16_t uft_le16(uint16_t v){ return UFT_HOST_LITTLE_ENDIAN ? v : uft_bswap16(v); }
static inline uint32_t uft_le32(uint32_t v){ return UFT_HOST_LITTLE_ENDIAN ? v : uft_bswap32(v); }
#endif

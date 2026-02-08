/**
 * @file uft_packed.h
 * @brief Cross-platform packed struct macros
 * 
 * USAGE PATTERN:
 *   #include "uft/uft_packed.h"
 *   
 *   UFT_PACK_BEGIN
 *   typedef struct {
 *       uint8_t a;
 *       uint32_t b;
 *   } my_struct_t;
 *   UFT_PACK_END
 */

#ifndef UFT_PACKED_H
#define UFT_PACKED_H

/* Undefine any existing definitions */
#undef UFT_PACK_BEGIN
#undef UFT_PACK_END
#undef UFT_PACKED_BEGIN
#undef UFT_PACKED_END
#undef UFT_PACKED_STRUCT
#undef UFT_PACKED_ATTR
#undef UFT_PACKED

#if defined(_MSC_VER)
    /* Microsoft Visual C++ */
    #define UFT_PACK_BEGIN      __pragma(pack(push, 1))
    #define UFT_PACK_END        __pragma(pack(pop))
    #define UFT_PACKED_BEGIN    __pragma(pack(push, 1))
    #define UFT_PACKED_END      __pragma(pack(pop))
    #define UFT_PACKED_STRUCT   
    #define UFT_PACKED_ATTR     
    #define UFT_PACKED          
#elif defined(__GNUC__) || defined(__clang__)
    /* GCC / Clang */
    #define UFT_PACK_BEGIN      _Pragma("pack(push, 1)")
    #define UFT_PACK_END        _Pragma("pack(pop)")
    #define UFT_PACKED_BEGIN    _Pragma("pack(push, 1)")
    #define UFT_PACKED_END      _Pragma("pack(pop)")
    #define UFT_PACKED_STRUCT   __attribute__((packed))
    #define UFT_PACKED_ATTR     __attribute__((packed))
    #define UFT_PACKED          __attribute__((packed))
#else
    /* Fallback */
    #define UFT_PACK_BEGIN
    #define UFT_PACK_END
    #define UFT_PACKED_BEGIN
    #define UFT_PACKED_END
    #define UFT_PACKED_STRUCT
    #define UFT_PACKED_ATTR
    #define UFT_PACKED
#endif

#endif /* UFT_PACKED_H */

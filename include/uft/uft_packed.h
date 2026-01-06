/**
 * @file uft_packed.h
 * @brief Centralized packed struct definitions - INCLUDE THIS INSTEAD OF LOCAL DEFINITIONS
 * @version 1.0.0
 * @date 2026-01-06
 * 
 * This header provides the SINGLE SOURCE OF TRUTH for packed struct macros.
 * All other headers should #include "uft/uft_packed.h" instead of defining their own.
 * 
 * Usage:
 *   UFT_PACKED_BEGIN
 *   typedef struct {
 *       uint8_t a;
 *       uint32_t b;
 *   } UFT_PACKED_STRUCT my_packed_struct;
 *   UFT_PACKED_END
 */

#ifndef UFT_PACKED_H
#define UFT_PACKED_H

/* Undefine any existing definitions to prevent conflicts */
#ifdef UFT_PACKED_BEGIN
#undef UFT_PACKED_BEGIN
#endif
#ifdef UFT_PACKED_END
#undef UFT_PACKED_END
#endif
#ifdef UFT_PACKED_STRUCT
#undef UFT_PACKED_STRUCT
#endif
#ifdef UFT_PACKED_ATTR
#undef UFT_PACKED_ATTR
#endif
#ifdef UFT_PACKED
#undef UFT_PACKED
#endif

/* Define based on compiler */
#if defined(_MSC_VER)
    /* MSVC: use pragma pack */
    #define UFT_PACKED_BEGIN    __pragma(pack(push, 1))
    #define UFT_PACKED_END      __pragma(pack(pop))
    #define UFT_PACKED_STRUCT   /* empty */
    #define UFT_PACKED_ATTR     /* empty */
    #define UFT_PACKED          /* empty */
#elif defined(__GNUC__) || defined(__clang__)
    /* GCC/Clang: use pragma pack for consistency with MSVC */
    #define UFT_PACKED_BEGIN    _Pragma("pack(push, 1)")
    #define UFT_PACKED_END      _Pragma("pack(pop)")
    #define UFT_PACKED_STRUCT   /* empty - using pragma instead */
    #define UFT_PACKED_ATTR     __attribute__((packed))
    #define UFT_PACKED          __attribute__((packed))
#else
    /* Unknown compiler: no-op */
    #define UFT_PACKED_BEGIN
    #define UFT_PACKED_END
    #define UFT_PACKED_STRUCT
    #define UFT_PACKED_ATTR
    #define UFT_PACKED
#endif

#endif /* UFT_PACKED_H */

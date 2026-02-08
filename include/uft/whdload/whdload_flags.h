#pragma once
/* whdload_flags.h — extracted WHDL bit definitions from WHDLoad include (whdload.i) */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_whd_flag_bit {
    UFT_WHDL_BIT_Disk = 0,
    UFT_WHDL_BIT_NoError = 1,
    UFT_WHDL_BIT_EmulTrap = 2,
    UFT_WHDL_BIT_NoDivZero = 3,
    UFT_WHDL_BIT_Req68020 = 4,
    UFT_WHDL_BIT_ReqAGA = 5,
    UFT_WHDL_BIT_NoKbd = 6,
    UFT_WHDL_BIT_EmulLineA = 7,
    UFT_WHDL_BIT_EmulTrapV = 8,
    UFT_WHDL_BIT_EmulChk = 9,
    UFT_WHDL_BIT_EmulPriv = 10,
    UFT_WHDL_BIT_EmulLineF = 11,
    UFT_WHDL_BIT_ClearMem = 12,
    UFT_WHDL_BIT_Examine = 13,
    UFT_WHDL_BIT_EmulDivZero = 14,
    UFT_WHDL_BIT_EmulIllegal = 15,
} uft_whd_flag_bit_t;

static inline uint32_t uft_whd_flag_mask(uft_whd_flag_bit_t bit){ return (bit>=0 && bit<32) ? (1u<<bit) : 0u; }

#ifdef __cplusplus
}
#endif

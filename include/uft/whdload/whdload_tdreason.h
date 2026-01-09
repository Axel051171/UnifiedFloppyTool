#pragma once
/* whdload_tdreason.h — extracted TDREASON_* codes from WHDLoad include (whdload.i) */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_whd_tdreason {
    UFT_TDREASON_OK = -1,
    UFT_TDREASON_DOSREAD = 1,
    UFT_TDREASON_DOSWRITE = 2,
    UFT_TDREASON_DEBUG = 5,
    UFT_TDREASON_DOSLIST = 6,
    UFT_TDREASON_DISKLOAD = 7,
    UFT_TDREASON_DISKLOADDEV = 8,
    UFT_TDREASON_WRONGVER = 9,
    UFT_TDREASON_OSEMUFAIL = 10,
    UFT_TDREASON_REQ68020 = 11,
    UFT_TDREASON_REQAGA = 12,
    UFT_TDREASON_MUSTNTSC = 13,
    UFT_TDREASON_MUSTPAL = 14,
    UFT_TDREASON_MUSTREG = 15,
    UFT_TDREASON_DELETEFILE = 27,
    UFT_TDREASON_FAILMSG = 43,
} uft_whd_tdreason_t;

#ifdef __cplusplus
}
#endif

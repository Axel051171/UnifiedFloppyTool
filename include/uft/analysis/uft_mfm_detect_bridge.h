/**
 * @file uft_mfm_detect_bridge.h
 * @brief Bridge between OTDR analysis and MFM detection
 */
#ifndef UFT_MFM_DETECT_BRIDGE_H
#define UFT_MFM_DETECT_BRIDGE_H

#include "mfm_detect.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert OTDR encoding enum to mfm_detect result
 */
static inline mfm_detect_result_t otdr_to_mfm_detect(int otdr_encoding)
{
    switch (otdr_encoding) {
        case 1: return MFM_DETECT_MFM_DD;
        case 2: return MFM_DETECT_MFM_HD;
        case 3: return MFM_DETECT_FM_SD;
        case 4: return MFM_DETECT_GCR_C64;
        case 5: return MFM_DETECT_GCR_APPLE;
        case 6: return MFM_DETECT_AMIGA_MFM;
        default: return MFM_DETECT_UNKNOWN;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_DETECT_BRIDGE_H */

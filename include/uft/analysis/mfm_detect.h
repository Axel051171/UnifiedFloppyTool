/**
 * @file mfm_detect.h
 * @brief MFM Encoding Auto-Detection
 */
#ifndef UFT_MFM_DETECT_H
#define UFT_MFM_DETECT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MFM_DETECT_UNKNOWN = 0,
    MFM_DETECT_MFM_DD,
    MFM_DETECT_MFM_HD,
    MFM_DETECT_FM_SD,
    MFM_DETECT_GCR_C64,
    MFM_DETECT_GCR_APPLE,
    MFM_DETECT_AMIGA_MFM
} mfm_detect_result_t;

typedef struct {
    mfm_detect_result_t encoding;
    int                 confidence;     /* 0-100 */
    uint32_t            bitcell_ns;     /* detected nominal bitcell */
    uint32_t            data_rate;      /* bits/sec */
} mfm_detect_info_t;

/**
 * @brief Detect encoding from flux timing histogram
 */
int mfm_detect_encoding(const uint32_t *flux_ns, size_t count,
                         mfm_detect_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_DETECT_H */

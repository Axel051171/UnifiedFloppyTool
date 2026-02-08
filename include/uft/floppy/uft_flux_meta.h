/**
 * @file uft_flux_meta.h
 * @brief Shared flux/copy-protection metadata types for format modules
 */

#ifndef UFT_FLUX_META_H
#define UFT_FLUX_META_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Weak/fuzzy region descriptor */
typedef struct {
    uint32_t offset;
    uint32_t length;
} WeakRegion;

/* Flux timing parameters */
typedef struct {
    uint32_t nominal_cell_ns;
    uint32_t jitter_ns;
    uint32_t encoding_hint;   /* 0=unknown, 1=FM, 2=MFM, 3=GCR */
} FluxTiming;

/* Combined flux metadata */
typedef struct {
    FluxTiming timing;
    WeakRegion *weak_regions;
    uint32_t weak_count;
} FluxMeta;

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_META_H */

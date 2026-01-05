// fluxdec.h - Flux Decoder Core (FM/MFM/GCR skeleton) (C11)
// UFT - Unified Floppy Tooling
#ifndef UFT_FLUXDEC_H
#define UFT_FLUXDEC_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FLUX_ENCODING_UNKNOWN = 0,
    FLUX_ENCODING_FM,
    FLUX_ENCODING_MFM,
    FLUX_ENCODING_GCR
} FluxEncoding;

typedef struct {
    FluxEncoding encoding;
    uint32_t nominal_cell_ns;
    uint32_t tolerance_ns;
} FluxDecodeProfile;

typedef struct {
    uint8_t *bitstream;   /* decoded bits */
    uint32_t bit_count;
    bool weak_detected;
} FluxDecodedTrack;

/* Decode flux intervals (ns) into a raw bitstream */
int flux_decode_track(const uint32_t *intervals_ns,
                      uint32_t count,
                      const FluxDecodeProfile *profile,
                      FluxDecodedTrack *out);

/* Free decoded track */
void flux_free_decoded(FluxDecodedTrack *t);

#endif

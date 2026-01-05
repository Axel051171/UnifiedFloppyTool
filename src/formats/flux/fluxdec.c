// fluxdec.c - Flux Decoder Core (C11)
#include "uft/formats/fluxdec.h"
#include <stdlib.h>
#include <string.h>

int flux_decode_track(const uint32_t *intervals_ns,
                      uint32_t count,
                      const FluxDecodeProfile *profile,
                      FluxDecodedTrack *out)
{
    if(!intervals_ns || !profile || !out) return -1;
    memset(out,0,sizeof(*out));

    /* Skeleton decoder:
     * Real implementation would use PLL + adaptive windowing.
     * Here we simply classify intervals as 0/1 cells.
     */
    out->bitstream = (uint8_t*)malloc(count);
    if(!out->bitstream) return -1;

    uint32_t cell = profile->nominal_cell_ns;
    uint32_t tol  = profile->tolerance_ns;

    for(uint32_t i=0;i<count;i++){
        uint32_t iv = intervals_ns[i];
        if(iv > cell + tol) {
            out->bitstream[out->bit_count++] = 0;
            out->weak_detected = true;
        } else {
            out->bitstream[out->bit_count++] = 1;
        }
    }
    return 0;
}

void flux_free_decoded(FluxDecodedTrack *t){
    if(!t) return;
    free(t->bitstream);
    memset(t,0,sizeof(*t));
}

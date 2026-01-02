/*
 * ufm_c64_trait_longtrack.c
 *
 * Heuristic:
 * Flags bitlen_max outliers compared to median bitlen across tracks.
 */
#include "uft/uft_error.h"
#include "uft_c64_trait_longtrack.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

static int clamp100(int v){ return (v<0)?0:((v>100)?100:v); }
static void ev(char* buf,size_t cap,const char* fmt, ...) {
    if(!buf||cap==0) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
}


static int cmp_u32(const void* a,const void* b){
    uint32_t A=*(const uint32_t*)a,B=*(const uint32_t*)b;
    return (A>B)-(A<B);
}
int ufm_c64_detect_trait_longtrack(const ufm_c64_track_sig_t* tracks, size_t n,
                                  char* evidence, size_t cap){
    if(evidence && cap) evidence[0]='\0';
    if(!tracks || n==0) return 0;

    /* collect bitlen_max values that look valid */
    uint32_t vals[128]; size_t m=0;
    for(size_t i=0;i<n && m<128;i++){
        if(tracks[i].bitlen_max > 1000) vals[m++] = tracks[i].bitlen_max;
    }
    if(m < 6) return 0;

    qsort(vals, m, sizeof(vals[0]), cmp_u32);
    uint32_t med = vals[m/2];

    int outliers=0;
    uint32_t worst=0;
    int worst_track=-1;
    for(size_t i=0;i<n;i++){
        uint32_t bl = tracks[i].bitlen_max;
        if(bl <= 1000) continue;
        if(bl > (uint32_t)((double)med * 1.08)){
            outliers++;
            if(bl > worst){ worst=bl; worst_track=tracks[i].track_x2; }
        }
    }

    int conf=0;
    if(outliers>0){
        conf = 45;
        if(outliers>=2) conf += 10;
        if(worst > (uint32_t)((double)med*1.15)) conf += 15;
    }
    conf=clamp100(conf);
    if(conf>0) ev(evidence, cap, "Trait long-track outlier: outliers=%d median_bitlen=%u worst_bitlen=%u @track_x2=%d",
                 outliers, (unsigned)med, (unsigned)worst, worst_track);
    return conf;
}


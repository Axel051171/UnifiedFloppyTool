/*
 * ufm_c64_trait_bitlen_jitter.c
 *
 * Heuristic:
 * Uses bitlen_min/max spread to detect unstable timing/weak regions across revolutions.
 */
#include "uft/uft_error.h"
#include "uft_c64_trait_bitlen_jitter.h"
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


int ufm_c64_detect_trait_bitlen_jitter(const ufm_c64_track_sig_t* tracks, size_t n,
                                      char* evidence, size_t cap){
    if(evidence && cap) evidence[0]='\0';
    if(!tracks || n==0) return 0;

    int affected=0;
    double worst_ratio=0.0;
    int worst_track=-1;
    for(size_t i=0;i<n;i++){
        uint32_t mn = tracks[i].bitlen_min;
        uint32_t mx = tracks[i].bitlen_max;
        if(mx < 1000 || mn < 1000 || mx < mn) continue;
        double r = (double)(mx - mn) / (double)mx;
        if(r > 0.05) affected++;
        if(r > worst_ratio){ worst_ratio=r; worst_track=tracks[i].track_x2; }
    }

    int conf=0;
    if(affected>0){
        conf = 40;
        if(affected>=2) conf += 10;
        if(worst_ratio >= 0.10) conf += 15;
        if(worst_ratio >= 0.20) conf += 15;
        conf = clamp100(conf);
        ev(evidence, cap, "Trait bitlen jitter across revs: affected_tracks=%d worst_ratio=%.2f @track_x2=%d",
           affected, worst_ratio, worst_track);
    }
    return conf;
}


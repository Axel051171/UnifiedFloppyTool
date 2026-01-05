/*
 * ufm_c64_trait_speed_anomaly.c
 *
 * Heuristic:
 * Checks for large adjacent-track bit-length ratio spikes that suggest nonstandard speed/format regions.
 */
#include "uft/uft_error.h"
#include "uft_c64_trait_speed_anomaly.h"
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


/* crude adjacent-track anomaly detector */
int ufm_c64_detect_trait_speed_anomaly(const ufm_c64_track_sig_t* tracks, size_t n,
                                      char* evidence, size_t cap){
    if(evidence && cap) evidence[0]='\0';
    if(!tracks || n<6) return 0;

    /* copy pointers and sort by track_x2 */
    const ufm_c64_track_sig_t* order[128]; size_t m=0;
    for(size_t i=0;i<n && m<128;i++) order[m++] = &tracks[i];
    for(size_t i=0;i<m;i++){
        for(size_t j=i+1;j<m;j++){
            if(order[j]->track_x2 < order[i]->track_x2){
                const ufm_c64_track_sig_t* tmp=order[i]; order[i]=order[j]; order[j]=tmp;
            }
        }
    }

    int spikes=0;
    double worst=0.0;
    int worst_pair_a=-1, worst_pair_b=-1;
    for(size_t i=1;i<m;i++){
        uint32_t a=order[i-1]->bitlen_max;
        uint32_t b=order[i]->bitlen_max;
        if(a<1000 || b<1000) continue;
        double r = (a>b)?((double)a/(double)b):((double)b/(double)a);
        if(r > 1.15){ spikes++; }
        if(r > worst){ worst=r; worst_pair_a=order[i-1]->track_x2; worst_pair_b=order[i]->track_x2; }
    }

    int conf=0;
    if(spikes>0){
        conf = 35;
        if(spikes>=2) conf += 10;
        if(worst>=1.25) conf += 15;
        if(worst>=1.40) conf += 15;
        conf = clamp100(conf);
        ev(evidence, cap, "Trait speed/bitlen anomaly: spikes=%d worst_adjacent_ratio=%.2f (track_x2 %d->%d)",
           spikes, worst, worst_pair_a, worst_pair_b);
    }
    return conf;
}


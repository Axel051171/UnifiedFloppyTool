/*
 * ufm_c64_trait_halftracks.c
 *
 * Heuristic:
 * Checks for odd track_x2 values, implying half-tracks in the capture.
 */
#include "uft/uft_error.h"
#include "uft_c64_trait_halftracks.h"
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


int ufm_c64_detect_trait_halftracks(const ufm_c64_track_sig_t* tracks, size_t n,
                                   char* evidence, size_t cap){
    if(evidence && cap) evidence[0]='\0';
    if(!tracks || n==0) return 0;

    int odd=0;
    for(size_t i=0;i<n;i++){
        if((tracks[i].track_x2 & 1) != 0) odd++;
    }
    int conf = 0;
    if(odd>0){
        conf = 55;
        if(odd>=4) conf += 10;
        conf = clamp100(conf);
        ev(evidence, cap, "Trait half-tracks present: odd_track_x2_count=%d (capture includes half-tracks)", odd);
    }
    return conf;
}


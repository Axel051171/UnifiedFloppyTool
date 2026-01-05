/*
 * ufm_c64_trait_weakbits.c
 *
 * Heuristic:
 * Looks for significant weak_bits_total/max_run across tracks.
 */
#include "uft/uft_error.h"
#include "uft_c64_trait_weakbits.h"
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


int ufm_c64_detect_trait_weakbits(const ufm_c64_track_sig_t* tracks, size_t n,
                                 char* evidence, size_t cap){
    if(evidence && cap) evidence[0]='\0';
    if(!tracks || n==0) return 0;

    uint32_t total=0, maxrun=0;
    int affected=0;
    for(size_t i=0;i<n;i++){
        total += tracks[i].weak_bits_total;
        if(tracks[i].weak_bits_max_run > maxrun) maxrun = tracks[i].weak_bits_max_run;
        if(tracks[i].weak_bits_total >= 256 || tracks[i].weak_bits_max_run >= 64) affected++;
    }

    int conf = 0;
    if(affected==0 && total==0 && maxrun==0) conf = 0;
    else{
        conf = 40;
        if(affected >= 2) conf += 15;
        if(total >= 2048) conf += 15;
        if(maxrun >= 256) conf += 15;
        if(maxrun >= 512) conf += 10;
    }

    conf = clamp100(conf);
    if(conf>0) ev(evidence, cap, "Trait weak-bits likely: affected_tracks=%d total=%u max_run=%u",
                 affected, (unsigned)total, (unsigned)maxrun);
    return conf;
}


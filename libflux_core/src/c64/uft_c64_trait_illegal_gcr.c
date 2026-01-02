/*
 * ufm_c64_trait_illegal_gcr.c
 *
 * Heuristic:
 * Sums illegal_gcr_events across tracks and flags if non-zero/significant.
 */
#include "uft/uft_error.h"
#include "uft_c64_trait_illegal_gcr.h"
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


int ufm_c64_detect_trait_illegal_gcr(const ufm_c64_track_sig_t* tracks, size_t n,
                                    char* evidence, size_t cap){
    if(evidence && cap) evidence[0]='\0';
    if(!tracks || n==0) return 0;

    uint32_t total=0; int affected=0;
    for(size_t i=0;i<n;i++){
        total += tracks[i].illegal_gcr_events;
        if(tracks[i].illegal_gcr_events) affected++;
    }
    int conf=0;
    if(total>0){
        conf = 40;
        if(affected>=2) conf += 10;
        if(total>=50) conf += 15;
        if(total>=200) conf += 15;
        conf = clamp100(conf);
        ev(evidence, cap, "Trait illegal GCR events: affected_tracks=%d total_events=%u", affected, (unsigned)total);
    }
    return conf;
}


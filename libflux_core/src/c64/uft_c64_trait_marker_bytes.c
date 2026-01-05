/*
 * ufm_c64_trait_marker_bytes.c
 *
 * Heuristic:
 * Looks for decoder-emitted marker byte counters (0x52/0x75/0x6B/0x7B).
 */
#include "uft/uft_error.h"
#include "uft_c64_trait_marker_bytes.h"
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


int ufm_c64_detect_trait_marker_bytes(const ufm_c64_track_sig_t* tracks, size_t n,
                                     char* evidence, size_t cap){
    if(evidence && cap) evidence[0]='\0';
    if(!tracks || n==0) return 0;

    uint32_t total = 0;
    int affected = 0;
    for(size_t i=0;i<n;i++){
        uint32_t c = tracks[i].count_52 + tracks[i].count_75 + tracks[i].count_6b + tracks[i].count_7b;
        total += c;
        if(c >= 8) affected++;
    }
    int conf = 0;
    if(total > 0){
        conf = 35;
        if(affected >= 1) conf += 15;
        if(affected >= 3) conf += 10;
        if(total >= 200) conf += 10;
        conf = clamp100(conf);
        ev(evidence, cap, "Trait marker bytes seen: affected_tracks=%d total_marker_hits=%u (0x52/0x75/0x6B/0x7B)", affected, (unsigned)total);
    }
    return conf;
}


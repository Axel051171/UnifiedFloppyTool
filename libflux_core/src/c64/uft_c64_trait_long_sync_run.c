/*
 * ufm_c64_trait_long_sync_run.c
 *
 * Heuristic:
 * Flags very long max_sync_run_bits, common in sync-based protection/loader schemes.
 */
#include "uft/uft_error.h"
#include "uft_c64_trait_long_sync_run.h"
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


int ufm_c64_detect_trait_long_sync_run(const ufm_c64_track_sig_t* tracks, size_t n,
                                      char* evidence, size_t cap){
    if(evidence && cap) evidence[0]='\0';
    if(!tracks || n==0) return 0;

    uint32_t worst=0; int worst_track=-1;
    for(size_t i=0;i<n;i++){
        if(tracks[i].max_sync_run_bits > worst){
            worst = tracks[i].max_sync_run_bits;
            worst_track = tracks[i].track_x2;
        }
    }
    int conf=0;
    if(worst >= 256){
        conf = 45;
        if(worst >= 512) conf += 20;
        if(worst >= 1024) conf += 15;
        conf = clamp100(conf);
        ev(evidence, cap, "Trait long sync-run: max_sync_run_bits=%u @track_x2=%d", (unsigned)worst, worst_track);
    }
    return conf;
}


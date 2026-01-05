/*
 * ufm_c64_trait_dirtrack_anomaly.c
 *
 * Heuristic:
 * Inspects track 18 for unusually high weakbits/illegal GCR/sync runs.
 */
#include "uft/uft_error.h"
#include "uft_c64_trait_dirtrack_anomaly.h"
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


static const ufm_c64_track_sig_t* find_track18(const ufm_c64_track_sig_t* t,size_t n){
    for(size_t i=0;i<n;i++){
        if(t[i].track_x2 == 36) return &t[i]; /* track 18 *2 */
    }
    return NULL;
}
int ufm_c64_detect_trait_dirtrack_anomaly(const ufm_c64_track_sig_t* tracks, size_t n,
                                         char* evidence, size_t cap){
    if(evidence && cap) evidence[0]='\0';
    if(!tracks || n==0) return 0;

    const ufm_c64_track_sig_t* t18 = find_track18(tracks, n);
    if(!t18) return 0;

    int conf=0;
    if(t18->weak_bits_total >= 256 || t18->weak_bits_max_run >= 64) conf = 50;
    if(t18->illegal_gcr_events >= 20) conf += 10;
    if(t18->max_sync_run_bits >= 512) conf += 10;

    conf = clamp100(conf);
    if(conf>0){
        ev(evidence, cap, "Trait directory-track anomaly (track 18): weak_total=%u weak_run=%u illegal_gcr=%u max_sync=%u",
           (unsigned)t18->weak_bits_total, (unsigned)t18->weak_bits_max_run,
           (unsigned)t18->illegal_gcr_events, (unsigned)t18->max_sync_run_bits);
    }
    return conf;
}


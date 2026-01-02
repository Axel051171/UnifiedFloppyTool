/*
 * ufm_c64_trait_multirev_recommended.c
 *
 * Heuristic:
 * If only 1 revolution captured yet weak/jitter hints appear, recommends multi-rev capture.
 */
#include "uft/uft_error.h"
#include "uft_c64_trait_multirev_recommended.h"
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


int ufm_c64_detect_trait_multirev_recommended(const ufm_c64_track_sig_t* tracks, size_t n,
                                             char* evidence, size_t cap){
    if(evidence && cap) evidence[0]='\0';
    if(!tracks || n==0) return 0;

    int any_single_rev=0;
    int any_multi_rev=0;
    int hint=0;
    for(size_t i=0;i<n;i++){
        if(tracks[i].revolutions <= 1) any_single_rev=1;
        if(tracks[i].revolutions >= 2) any_multi_rev=1;

        /* if there's weakbits/jitter but only 1 rev, recommend multi-rev */
        if(tracks[i].revolutions <= 1){
            if(tracks[i].weak_bits_total >= 128 || tracks[i].weak_bits_max_run >= 64) hint++;
            if(tracks[i].bitlen_max > 0 && tracks[i].bitlen_min > 0){
                if(tracks[i].bitlen_max > tracks[i].bitlen_min){
                    double r=(double)(tracks[i].bitlen_max - tracks[i].bitlen_min)/(double)tracks[i].bitlen_max;
                    if(r>0.05) hint++;
                }
            }
        }
    }

    int conf=0;
    if(any_multi_rev) return 0; /* already captured multi-rev: no need to recommend */
    if(any_single_rev && hint>0){
        conf = 45;
        if(hint>=2) conf += 10;
        if(hint>=5) conf += 15;
        conf = clamp100(conf);
        ev(evidence, cap, "Trait multi-rev recommended: capture shows instability but revolutions<=1 on affected tracks (hints=%d)", hint);
    }
    return conf;
}


/*
 * uft_c64_protection_taxonomy.c
 */

#include "uft/uft_error.h"
#include "uft_c64_protection_taxonomy.h"
#include <string.h>
#include <stdio.h>

static void append(char* dst, size_t cap, const char* s){
    if(!dst || cap==0) return;
    size_t cur = strlen(dst);
    if(cur >= cap-1) return;
    size_t rem = cap - cur;
    snprintf(dst + cur, rem, "%s", s);
}

const char* uft_c64_prot_type_name(uft_c64_prot_type_t t){
    switch(t){
        case UFT_C64_PROT_NONE: return "none";
        case UFT_C64_PROT_WEAK_BITS: return "weak_bits";
        case UFT_C64_PROT_LONG_TRACK: return "long_track";
        case UFT_C64_PROT_SHORT_TRACK: return "short_track";
        case UFT_C64_PROT_HALF_TRACK_DATA: return "half_track_data";
        case UFT_C64_PROT_ILLEGAL_GCR: return "illegal_gcr";
        case UFT_C64_PROT_LONG_SYNC: return "long_sync";
        case UFT_C64_PROT_SECTOR_ANOMALY: return "sector_anomaly";
        default: return "unknown";
    }
}

static void push_hit(uft_c64_prot_hit_t* hits, size_t cap, size_t* io_n,
                     uft_c64_prot_type_t type, int track_x2, int severity){
    if(severity < 0) severity = 0;
    if(severity > 100) severity = 100;
    if(!hits || cap==0) return;
    if(*io_n >= cap) return;
    hits[*io_n].type = type;
    hits[*io_n].track_x2 = track_x2;
    hits[*io_n].severity_0_100 = severity;
    (*io_n)++;
}

/* Heuristic thresholds:
 * Note: C64 1541 nominal bit length depends on zone (track group) and RPM variations.
 * We keep thresholds wide and "relative" (min/max span per track, plus half-track indicators).
 */
static int severity_weak(const uft_c64_track_metrics_t* t){
    if(t->revolutions < 2) return 0;
    if(t->weak_region_bits == 0) return 0;

    /* scale by max run (contiguous weak area is more protection-like) */
    uint32_t run = t->weak_region_max_run;
    uint32_t bits = t->weak_region_bits;
    int sev = 0;
    if(run >= 256) sev += 40;
    else if(run >= 128) sev += 25;
    else if(run >= 64) sev += 15;

    if(bits >= 1024) sev += 40;
    else if(bits >= 512) sev += 25;
    else if(bits >= 256) sev += 15;

    if(sev > 100) sev = 100;
    return sev;
}

static int severity_illegal_gcr(const uft_c64_track_metrics_t* t){
    if(t->illegal_gcr_events == 0) return 0;
    if(t->illegal_gcr_events >= 200) return 90;
    if(t->illegal_gcr_events >= 50) return 70;
    if(t->illegal_gcr_events >= 10) return 50;
    return 25;
}

static int severity_long_sync(const uft_c64_track_metrics_t* t){
    /* long sync runs matter when a scheme is sync-length sensitive */
    uint32_t s = t->max_sync_run_bits;
    if(s == 0) return 0;
    if(s >= 1200) return 90;
    if(s >= 900)  return 70;
    if(s >= 700)  return 50;
    if(s >= 500)  return 30;
    return 10;
}

static int severity_tracklen(const uft_c64_track_metrics_t* t, bool want_long){
    if(t->bitlen_min == 0 || t->bitlen_max == 0) return 0;
    uint32_t span = t->bitlen_max - t->bitlen_min;

    /* A stable long/short track often shows consistent deviation, not just noisy span.
       Without nominal per-zone target, we use span as a "how unstable/strange" signal and
       absolute max/min as "very extreme". */
    if(span < 1000 && t->revolutions >= 2){
        /* stable across revs; now use absolute len */
        uint32_t L = want_long ? t->bitlen_max : t->bitlen_min;
        /* very broad thresholds to avoid false positives */
        if(want_long){
            if(L >= 230000) return 85;
            if(L >= 210000) return 60;
            if(L >= 200000) return 40;
        } else {
            if(L <= 160000) return 85;
            if(L <= 170000) return 60;
            if(L <= 180000) return 40;
        }
        return 0;
    }

    /* If span is huge, it's more "unstable" (could be weak bits / splice) */
    if(span >= 8000) return 50;
    if(span >= 4000) return 30;
    return 0;
}

bool uft_c64_prot_analyze(const uft_c64_track_metrics_t* tracks, size_t track_count,
                          uft_c64_prot_hit_t* hits, size_t hits_cap,
                          uft_c64_prot_report_t* out){
    if(!out) return false;
    memset(out, 0, sizeof(*out));
    out->summary[0] = '\0';

    if(!tracks || track_count == 0){
        append(out->summary, sizeof(out->summary),
               "No track metrics provided. Provide per-track bit lengths, weak-bit stats, illegal GCR events, sync run metrics.\n");
        return false;
    }

    size_t n_hits = 0;
    int score = 0;
    int tracks_with_any = 0;

    for(size_t i=0;i<track_count;i++){
        const uft_c64_track_metrics_t* t = &tracks[i];

        int sev_weak = severity_weak(t);
        int sev_illegal = severity_illegal_gcr(t);
        int sev_sync = severity_long_sync(t);
        int sev_long = severity_tracklen(t, true);
        int sev_short = severity_tracklen(t, false);

        int sev_half = 0;
        if(t->is_half_track && t->has_meaningful_data) sev_half = 70;
        else if(t->is_half_track) sev_half = 30;

        bool any = false;
        if(sev_weak){ push_hit(hits, hits_cap, &n_hits, UFT_C64_PROT_WEAK_BITS, t->track_x2, sev_weak); any=true; score += 10 + sev_weak/10; }
        if(sev_long){ push_hit(hits, hits_cap, &n_hits, UFT_C64_PROT_LONG_TRACK, t->track_x2, sev_long); any=true; score += 8 + sev_long/12; }
        if(sev_short){ push_hit(hits, hits_cap, &n_hits, UFT_C64_PROT_SHORT_TRACK, t->track_x2, sev_short); any=true; score += 8 + sev_short/12; }
        if(sev_half){ push_hit(hits, hits_cap, &n_hits, UFT_C64_PROT_HALF_TRACK_DATA, t->track_x2, sev_half); any=true; score += 10 + sev_half/14; }
        if(sev_illegal){ push_hit(hits, hits_cap, &n_hits, UFT_C64_PROT_ILLEGAL_GCR, t->track_x2, sev_illegal); any=true; score += 8 + sev_illegal/14; }
        if(sev_sync){ push_hit(hits, hits_cap, &n_hits, UFT_C64_PROT_LONG_SYNC, t->track_x2, sev_sync); any=true; score += 6 + sev_sync/16; }

        if(any) tracks_with_any++;
    }

    /* normalize */
    if(score > 100) score = 100;
    if(tracks_with_any == 0) score = 0;

    out->confidence_0_100 = score;
    out->hits_written = n_hits;

    /* Summary */
    append(out->summary, sizeof(out->summary), "C64 protection trait analysis (preservation-oriented):\n");
    if(score == 0){
        append(out->summary, sizeof(out->summary), "- No strong protection traits detected from supplied metrics.\n");
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "- Overall confidence: %d/100\n", score);
        append(out->summary, sizeof(out->summary), buf);
        snprintf(buf, sizeof(buf), "- Tracks with notable traits: %d/%d\n", tracks_with_any, (int)track_count);
        append(out->summary, sizeof(out->summary), buf);

        append(out->summary, sizeof(out->summary),
               "- Professional capture advice: capture >=3 revolutions on suspect tracks; keep raw flux; do not 'repair' weak areas.\n");
        append(out->summary, sizeof(out->summary),
               "- If half-tracks show data (e.g., 34.5/35.5/36), record them explicitly; many schemes rely on head positioning.\n");
    }

    append(out->summary, sizeof(out->summary),
           "Safety note: this module does not implement bypass/patching. It only classifies preservation-relevant traits.\n");

    return true;
}

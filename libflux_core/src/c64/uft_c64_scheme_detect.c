/*
 * uft_c64_scheme_detect.c
 */

#include "uft/uft_error.h"
#include "uft_c64_scheme_detect.h"
#include <string.h>
#include <stdio.h>

static void append(char* dst, size_t cap, const char* s){
    if(!dst || cap==0) return;
    size_t cur = strlen(dst);
    if(cur >= cap-1) return;
    size_t rem = cap - cur;
    snprintf(dst + cur, rem, "%s", s);
}

const char* uft_c64_scheme_name(uft_c64_scheme_t s){
    switch(s){
        case UFT_C64_SCHEME_UNKNOWN: return "unknown";
        case UFT_C64_SCHEME_GEOS_GAPDATA: return "geos_gapdata";
        case UFT_C64_SCHEME_RAPIDLOK: return "rapidlok";
        case UFT_C64_SCHEME_RAPIDLOK2: return "rapidlok2";
        case UFT_C64_SCHEME_RAPIDLOK6: return "rapidlok6";
        case UFT_C64_SCHEME_EA_FATTRACK: return "ea_fattrack";
        case UFT_C64_SCHEME_VORPAL: return "vorpal";
        case UFT_C64_SCHEME_VMAX: return "vmax";
        default: return "unknown";
    }
}

static const uft_c64_track_sig_t* find_track(const uft_c64_track_sig_t* t, size_t n, int track_x2){
    for(size_t i=0;i<n;i++) if(t[i].track_x2 == track_x2) return &t[i];
    return NULL;
}

static int clamp100(int v){ return v<0?0:(v>100?100:v); }

/* Generic trait scores */
static int score_weak(const uft_c64_track_sig_t* t){
    if(!t || t->revolutions < 2) return 0;
    if(t->weak_bits_total == 0) return 0;
    int s = 0;
    if(t->weak_bits_max_run >= 256) s += 40;
    else if(t->weak_bits_max_run >= 128) s += 25;
    else if(t->weak_bits_max_run >= 64) s += 15;

    if(t->weak_bits_total >= 2048) s += 45;
    else if(t->weak_bits_total >= 1024) s += 30;
    else if(t->weak_bits_total >= 512) s += 20;

    return clamp100(s);
}

static int score_longtrack(const uft_c64_track_sig_t* t){
    if(!t || t->bitlen_max == 0) return 0;
    /* very broad: long-track protections tend to push length high */
    if(t->bitlen_max >= 230000) return 85;
    if(t->bitlen_max >= 215000) return 65;
    if(t->bitlen_max >= 205000) return 45;
    return 0;
}

static int score_halft(const uft_c64_track_sig_t* t){
    if(!t) return 0;
    /* Half-track represented by odd x2 value */
    if((t->track_x2 % 2) != 0){
        /* If we see any marker/errors on half-track, treat as meaningful data hint */
        if(t->illegal_gcr_events || t->count_75 || t->count_6b || t->count_7b || t->count_00) return 75;
        return 35;
    }
    return 0;
}

static int score_illegal(const uft_c64_track_sig_t* t){
    if(!t) return 0;
    if(t->illegal_gcr_events >= 200) return 90;
    if(t->illegal_gcr_events >= 50) return 70;
    if(t->illegal_gcr_events >= 10) return 45;
    return 0;
}

static int score_longsync(const uft_c64_track_sig_t* t){
    if(!t || t->max_sync_run_bits == 0) return 0;
    if(t->max_sync_run_bits >= 1200) return 90;
    if(t->max_sync_run_bits >= 900) return 70;
    if(t->max_sync_run_bits >= 700) return 50;
    if(t->max_sync_run_bits >= 500) return 30;
    return 10;
}

static void push_hit(uft_c64_scheme_report_t* out, uft_c64_scheme_t s, int conf){
    if(!out) return;
    if(out->hits_n >= sizeof(out->hits)/sizeof(out->hits[0])) return;
    out->hits[out->hits_n].scheme = s;
    out->hits[out->hits_n].confidence_0_100 = clamp100(conf);
    out->hits_n++;
}

/* Scheme heuristics (conservative):
 * - These are "likely" flags. When in doubt, return unknown.
 */
static int detect_geos_gapdata(const uft_c64_track_sig_t* tracks, size_t n){
    /* GEOS check is on track 21. We'll just flag if track 21 has very low illegal events
       AND is otherwise "normal" but caller reported they ran a gap validator module.
       If you don't run that module, this detector stays conservative and returns 0. */
    (void)tracks; (void)n;
    return 0; /* use dedicated uft_c64_geos_gap_protection for real validation */
}

static int detect_rapidlok_family(const uft_c64_track_sig_t* tracks, size_t n, int* out_variant){
    /* Rapidlok traits:
       - "bad GCR bytes" in gaps (0x00) and sync-length sensitivity.
       - Marker bytes 0x75/0x6B/0x7B are decoder-dependent but strong signals when present.
       We treat:
         RL6 likely if 0x7B is strong (extra sector marker) and track 36 has RL markers.
         RL2 likely if 0x7B present AND track 18 shows multiple markers.
         RL1 (rapidlok) if 0x00 in gaps + sync lengths present + key track 36 traits (if provided elsewhere).
    */
    int conf = 0;
    int has75=0, has6b=0, has7b=0, has00=0;
    int longsync=0;

    for(size_t i=0;i<n;i++){
        const uft_c64_track_sig_t* t=&tracks[i];
        if(t->count_75) has75++;
        if(t->count_6b) has6b++;
        if(t->count_7b) has7b++;
        if(t->count_00) has00++;
        if(score_longsync(t) >= 50) longsync++;
    }

    if((has75+has6b) >= 2) conf += 35;
    if(has7b >= 1) conf += 25;
    if(has00 >= 2) conf += 15;
    if(longsync >= 1) conf += 10;

    const uft_c64_track_sig_t* t36 = find_track(tracks, n, 72);
    const uft_c64_track_sig_t* t18 = find_track(tracks, n, 36);

    if(t36 && (t36->count_7b || t36->count_75 || t36->count_6b)) conf += 10;
    if(t18 && (t18->count_7b + t18->count_75 + t18->count_6b + t18->count_52) >= 2) conf += 10;

    conf = clamp100(conf);

    /* Pick variant */
    if(out_variant) *out_variant = 0;
    if(conf >= 40){
        if(has7b >= 1 && t36 && (t36->count_7b >= 1) && out_variant) *out_variant = 6;
        else if(has7b >= 1 && t18 && out_variant) *out_variant = 2;
        else if(out_variant) *out_variant = 1;
    }
    return conf;
}

static int detect_vorpal(const uft_c64_track_sig_t* tracks, size_t n){
    /* Vorpal is commonly associated with track length anomalies and unusual sync behavior.
       With just metrics, we detect a "likely vorpal-like" pattern:
       - several long tracks + some illegal/weak traits, but not strongly RapidLok markers.
     */
    int conf = 0;
    int longt=0, weak=0, illegal=0, longsync=0;

    for(size_t i=0;i<n;i++){
        const uft_c64_track_sig_t* t=&tracks[i];
        if(score_longtrack(t) >= 45) longt++;
        if(score_weak(t) >= 40) weak++;
        if(score_illegal(t) >= 45) illegal++;
        if(score_longsync(t) >= 50) longsync++;
    }

    if(longt >= 2) conf += 35;
    if(weak >= 1) conf += 15;
    if(illegal >= 1) conf += 15;
    if(longsync >= 1) conf += 10;

    /* Penalize if RapidLok markers are strong (then it's probably not Vorpal). */
    int has_rl_markers = 0;
    for(size_t i=0;i<n;i++) if(tracks[i].count_7b || tracks[i].count_75 || tracks[i].count_6b) has_rl_markers++;
    if(has_rl_markers >= 2) conf -= 25;

    return clamp100(conf);
}

static int detect_vmax(const uft_c64_track_sig_t* tracks, size_t n){
    /* V-MAX is widely discussed as involving long tracks and nibble tricks.
       With metrics only, we flag "likely V-MAX-like" if:
       - many long tracks OR very extreme lengths
       - plus weak bits or illegal events on some tracks
       - and half-tracks may contain data
     */
    int conf = 0;
    int longt=0, extreme=0, weak=0, illegal=0, halft=0;

    for(size_t i=0;i<n;i++){
        const uft_c64_track_sig_t* t=&tracks[i];
        if(score_longtrack(t) >= 45) longt++;
        if(t->bitlen_max >= 240000) extreme++;
        if(score_weak(t) >= 30) weak++;
        if(score_illegal(t) >= 45) illegal++;
        if(score_halft(t) >= 35) halft++;
    }

    if(longt >= 4) conf += 45;
    else if(longt >= 2) conf += 30;

    if(extreme >= 1) conf += 15;
    if(weak >= 1) conf += 15;
    if(illegal >= 1) conf += 15;
    if(halft >= 1) conf += 10;

    return clamp100(conf);
}

static int detect_ea_fattrack(const uft_c64_track_sig_t* tracks, size_t n){
    /* EA "fat track" concept:
       - preservation requires half-track stepping stability and multi-rev reads on tracks near 34..35.
       - With metrics, we consider it likely if track 34/34.5/35 show:
         - long track or weird sync or illegal events, AND half-track has meaningful traits.
     */
    int conf = 0;
    const uft_c64_track_sig_t* t34 = find_track(tracks, n, 68);
    const uft_c64_track_sig_t* t34h = find_track(tracks, n, 69);
    const uft_c64_track_sig_t* t35 = find_track(tracks, n, 70);

    if(t34 && score_longtrack(t34) >= 45) conf += 20;
    if(t35 && score_longtrack(t35) >= 45) conf += 20;
    if(t34 && score_longsync(t34) >= 50) conf += 10;
    if(t35 && score_longsync(t35) >= 50) conf += 10;

    if(t34h){
        conf += 15; /* half-track exists */
        if(score_weak(t34h) || score_illegal(t34h) || score_longsync(t34h) >= 30) conf += 10;
    }

    /* Multi-rev requirement */
    if(t34 && t34->revolutions >= 3) conf += 5;
    if(t35 && t35->revolutions >= 3) conf += 5;

    return clamp100(conf);
}

bool uft_c64_detect_schemes(const uft_c64_track_sig_t* tracks, size_t track_count,
                           uft_c64_scheme_report_t* out){
    if(!out) return false;
    memset(out, 0, sizeof(*out));
    out->summary[0] = '\0';

    if(!tracks || track_count == 0){
        append(out->summary, sizeof(out->summary), "No track signatures provided.\n");
        return false;
    }

    /* Rapidlok family */
    int rl_var = 0;
    int rl_conf = detect_rapidlok_family(tracks, track_count, &rl_var);
    if(rl_conf >= 45){
        if(rl_var == 6) push_hit(out, UFT_C64_SCHEME_RAPIDLOK6, rl_conf);
        else if(rl_var == 2) push_hit(out, UFT_C64_SCHEME_RAPIDLOK2, rl_conf);
        else push_hit(out, UFT_C64_SCHEME_RAPIDLOK, rl_conf);
    }

    /* Other scheme heuristics */
    int ea = detect_ea_fattrack(tracks, track_count);
    if(ea >= 45) push_hit(out, UFT_C64_SCHEME_EA_FATTRACK, ea);

    int vor = detect_vorpal(tracks, track_count);
    if(vor >= 55) push_hit(out, UFT_C64_SCHEME_VORPAL, vor);

    int vmax = detect_vmax(tracks, track_count);
    if(vmax >= 55) push_hit(out, UFT_C64_SCHEME_VMAX, vmax);

    /* GEOS gap-data is best detected by the dedicated validator; keep placeholder. */
    int geos = detect_geos_gapdata(tracks, track_count);
    if(geos >= 70) push_hit(out, UFT_C64_SCHEME_GEOS_GAPDATA, geos);

    /* If nothing hit, unknown */
    if(out->hits_n == 0){
        push_hit(out, UFT_C64_SCHEME_UNKNOWN, 0);
        append(out->summary, sizeof(out->summary),
               "No strong scheme signature detected from coarse metrics. Provide richer inputs:\n"
               "- per-track decoded bytes (for marker scans)\n"
               "- per-track sync-run metrics\n"
               "- multi-rev weak-bit statistics\n");
        return true;
    }

    append(out->summary, sizeof(out->summary), "Detected likely C64 protection schemes (preservation-oriented, heuristic):\n");
    for(size_t i=0;i<out->hits_n;i++){
        char buf[160];
        snprintf(buf, sizeof(buf), "- %s (confidence %d/100)\n",
                 uft_c64_scheme_name(out->hits[i].scheme),
                 out->hits[i].confidence_0_100);
        append(out->summary, sizeof(out->summary), buf);
    }

    append(out->summary, sizeof(out->summary),
           "Capture advice:\n"
           "- Always keep raw flux. Do NOT 'repair' weak areas.\n"
           "- Capture >=3 revolutions on suspect tracks (weak bits/jitter).\n"
           "- Include half-tracks if your tool supports it (e.g., 34.5/35.5/36).\n"
           "- Preserve gaps/sync lengths (GEOS & sync-sensitive schemes can fail otherwise).\n");

    return true;
}

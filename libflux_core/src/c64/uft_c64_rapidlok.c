/*
 * uft_c64_rapidlok.c
 */

#include "uft/uft_error.h"
#include "uft_c64_rapidlok.h"
#include <string.h>
#include <stdio.h>

static void append(char* dst, size_t cap, const char* s){
    if(!dst || cap==0) return;
    size_t cur = strlen(dst);
    if(cur >= cap-1) return;
    size_t rem = cap - cur;
    snprintf(dst + cur, rem, "%s", s);
}

static const uft_rl_track_view_t* find_track(const uft_rl_track_view_t* t, size_t n, int track_x2){
    for(size_t i=0;i<n;i++){
        if(t[i].track_x2 == track_x2) return &t[i];
    }
    return NULL;
}

static bool track_has_bad_gcr00(const uft_rl_track_view_t* tv, size_t min_hits){
    if(!tv || !tv->gcr || tv->gcr_len == 0) return false;

    /* The Rittwage notes: "Each gap has bad GCR ($00) instead of inert bytes." */
    size_t hits = 0;
    for(size_t i=0;i<tv->gcr_len;i++){
        if(tv->gcr[i] == 0x00){
            hits++;
            if(hits >= min_hits) return true;
        }
    }
    return false;
}

static bool key_track36_looks_present(const uft_rl_track_view_t* tv){
    if(!tv) return false;

    /* From the notes:
     * - key on track 36: long sync, series of encoded bytes, then some bad GCR.
     *
     * Without knowing exact encoding bytes, we do a conservative check:
     * - has a "sector0_sync_bits" or "start_sync_bits" significantly above normal
     * - contains some bad GCR 0x00 (often in gaps/after key)
     */
    bool has_long_sync = false;
    if(tv->start_sync_bits >= 600) has_long_sync = true;      /* long */
    if(tv->sector0_sync_bits >= 600) has_long_sync = true;

    bool has_bad = track_has_bad_gcr00(tv, 32);

    return has_long_sync && has_bad;
}

static void score_sync_sensitivity(const uft_rl_track_view_t* tv, bool* out_start320, bool* out_sec0480){
    if(out_start320) *out_start320 = false;
    if(out_sec0480)  *out_sec0480  = false;
    if(!tv) return;

    /* Rittwage notes:
     * - all tracks start with sync about 320 bits
     * - sector 0 has sync about 480 bits before header
     * Tolerances: drives vary in RPM; we accept +/- 80 bits.
     */
    const uint32_t tol = 80;

    if(tv->start_sync_bits)
        if(uft_rl_within(tv->start_sync_bits, 320, tol)) if(out_start320) *out_start320 = true;

    if(tv->sector0_sync_bits)
        if(uft_rl_within(tv->sector0_sync_bits, 480, tol)) if(out_sec0480) *out_sec0480 = true;
}

static bool has_multi_rev(const uft_rl_track_view_t* tracks, size_t n){
    /* "Professional standard": >=3 revs on key/sensitive tracks helps detect weak bits / jitter. */
    for(size_t i=0;i<n;i++){
        if(tracks[i].revolutions >= 3) return true;
    }
    return false;
}

bool uft_rl_analyze(const uft_rl_track_view_t* tracks, size_t track_count,
                    uft_rl_observation_t* out){
    if(!out) return false;
    memset(out, 0, sizeof(*out));
    if(!tracks || track_count == 0){
        snprintf(out->summary, sizeof(out->summary),
                 "No track data provided. Need decoded track bytes + sync metrics.");
        return false;
    }

    out->has_multi_rev_capture = has_multi_rev(tracks, track_count);

    /* Check generic indicators across all supplied tracks. */
    size_t bad_gcr_tracks = 0;
    size_t start320_hits = 0;
    size_t sec0480_hits  = 0;

    for(size_t i=0;i<track_count;i++){
        if(track_has_bad_gcr00(&tracks[i], 64)) bad_gcr_tracks++;

        bool s320=false, s480=false;
        score_sync_sensitivity(&tracks[i], &s320, &s480);
        if(s320) start320_hits++;
        if(s480) sec0480_hits++;
    }

    out->gap_has_bad_gcr00 = (bad_gcr_tracks >= 2); /* heuristic: seen on multiple tracks */
    out->start_sync_near_320 = (start320_hits >= 2);
    out->sector0_sync_near_480 = (sec0480_hits >= 1);

    /* Key track 36 (track_x2=72). */
    const uft_rl_track_view_t* t36 = find_track(tracks, track_count, 72);
    if(t36){
        out->key_track36_present = key_track36_looks_present(t36);
    } else {
        /* If track 36 not present in the provided subset, we can't decide. */
        out->key_track36_present = false;
    }

    /* Variant note:
     * Rittwage mentions some titles check sync between track 34/35.
     * Without per-title code signatures, we set this if BOTH tracks show sync metrics
     * and are near expected values (or are missing metrics).
     */
    const uft_rl_track_view_t* t34 = find_track(tracks, track_count, 68);
    const uft_rl_track_view_t* t35 = find_track(tracks, track_count, 70);
    if(t34 && t35){
        bool a320=false,a480=false,b320=false,b480=false;
        score_sync_sensitivity(t34, &a320, &a480);
        score_sync_sensitivity(t35, &b320, &b480);
        out->trk34_35_sync_sensitive = (a320 || a480 || b320 || b480);
    }

    /* Confidence score */
    int score = 0;
    if(out->gap_has_bad_gcr00) score += 20;
    if(out->start_sync_near_320) score += 20;
    if(out->sector0_sync_near_480) score += 15;
    if(t36){ /* only score if we actually looked at it */
        if(out->key_track36_present) score += 30;
        else score -= 10;
    }
    if(out->has_multi_rev_capture) score += 10;

    if(score < 0) score = 0;
    if(score > 100) score = 100;
    out->confidence_0_100 = score;

    /* Summary */
    append(out->summary, sizeof(out->summary), "Rapidlok analysis summary:\n");
    if(out->gap_has_bad_gcr00)
        append(out->summary, sizeof(out->summary), "- Found many $00 bytes in track byte streams (matches 'bad GCR in gaps' trait).\n");
    else
        append(out->summary, sizeof(out->summary), "- Did not confidently observe bad-GCR ($00) in gaps.\n");

    if(out->start_sync_near_320)
        append(out->summary, sizeof(out->summary), "- Start-of-track sync lengths near ~320 bits observed.\n");
    else
        append(out->summary, sizeof(out->summary), "- Start-of-track sync ~320 bits not confirmed (missing metrics or mismatch).\n");

    if(out->sector0_sync_near_480)
        append(out->summary, sizeof(out->summary), "- Sector-0 pre-header sync near ~480 bits observed.\n");
    else
        append(out->summary, sizeof(out->summary), "- Sector-0 pre-header sync ~480 bits not confirmed.\n");

    if(t36){
        if(out->key_track36_present)
            append(out->summary, sizeof(out->summary), "- Track 36 shows 'key track' characteristics (long sync + bad GCR).\n");
        else
            append(out->summary, sizeof(out->summary), "- Track 36 present but key-track signature not detected (could be variant or decoding loss).\n");
    } else {
        append(out->summary, sizeof(out->summary), "- Track 36 not provided; cannot evaluate key-track presence.\n");
    }

    if(out->has_multi_rev_capture)
        append(out->summary, sizeof(out->summary), "- Multi-revolution capture detected (good for preservation/weak-bit analysis).\n");
    else
        append(out->summary, sizeof(out->summary), "- Suggest capturing >=3 revolutions on sensitive tracks (track 36, 34/35) for preservation grade.\n");

    {
        char buf[128];
        snprintf(buf, sizeof(buf), "- Overall confidence: %d/100\n", out->confidence_0_100);
        append(out->summary, sizeof(out->summary), buf);
    }

    return true;
}

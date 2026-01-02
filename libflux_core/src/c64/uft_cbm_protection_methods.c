/*
 * uft_cbm_protection_methods.c
 */

#include "uft/uft_error.h"
#include "uft_cbm_protection_methods.h"
#include <string.h>
#include <stdio.h>

static void append(char* dst, size_t cap, const char* s){
    if(!dst || cap==0) return;
    size_t cur = strlen(dst);
    if(cur >= cap-1) return;
    size_t rem = cap - cur;
    snprintf(dst + cur, rem, "%s", s);
}

static int clamp100(int v){ return v<0?0:(v>100?100:v); }

const char* uft_cbm_method_name(uft_cbm_prot_method_t m){
    switch(m){
        case UFT_CBM_PROT_UNKNOWN: return "unknown";
        case UFT_CBM_PROT_INTENTIONAL_ERRORS: return "intentional_disk_errors";
        case UFT_CBM_PROT_TRACK_SKEW: return "track_skew";
        case UFT_CBM_PROT_FAT_TRACKS: return "fat_tracks";
        case UFT_CBM_PROT_HALF_TRACKS: return "half_tracks";
        case UFT_CBM_PROT_EXTRA_TRACKS: return "extra_tracks";
        case UFT_CBM_PROT_CHANGED_BITRATES: return "changed_bitrates";
        case UFT_CBM_PROT_GAP_SIGNATURES: return "gap_signatures";
        case UFT_CBM_PROT_LONG_SECTORS: return "long_sectors";
        case UFT_CBM_PROT_CUSTOM_FORMATS: return "custom_formats";
        case UFT_CBM_PROT_LONG_TRACKS: return "long_tracks";
        case UFT_CBM_PROT_SYNC_COUNTING: return "sync_counting";
        case UFT_CBM_PROT_TRACK_SYNCHRONIZATION: return "track_synchronization";
        case UFT_CBM_PROT_WEAK_BITS_UNFORMATTED: return "weak_bits_unformatted";
        case UFT_CBM_PROT_SIGNATURE_KEY_TRACKS: return "signature_key_tracks";
        case UFT_CBM_PROT_NO_SYNC: return "no_sync";
        case UFT_CBM_PROT_SPIRADISC_LIKE: return "spiradisc_like";
        default: return "unknown";
    }
}

static void push_hit(uft_cbm_method_hit_t* hits, size_t cap, size_t* io_n,
                     uft_cbm_prot_method_t m, int track_x2, int conf){
    if(!hits || cap==0) return;
    if(*io_n >= cap) return;
    hits[*io_n].method = m;
    hits[*io_n].track_x2 = track_x2;
    hits[*io_n].confidence_0_100 = clamp100(conf);
    (*io_n)++;
}

/* Trait heuristics:
 * Keep them conservative. The idea is to flag "needs careful preservation" not to overclaim.
 */

static int score_errors(const uft_cbm_track_metrics_t* t){
    if(!t) return 0;
    /* Intentional errors can appear as persistent CRC failures/missing sectors. */
    uint32_t e = t->sector_crc_failures + t->sector_missing;
    if(e >= 40) return 85;
    if(e >= 10) return 60;
    if(e >= 3)  return 35;
    return 0;
}

static int score_longtrack(const uft_cbm_track_metrics_t* t){
    if(!t || t->bitlen_max == 0) return 0;
    /* Broad thresholds; 1541 typical max bits varies by zone. Flag extremes. */
    if(t->bitlen_max >= 240000) return 90;
    if(t->bitlen_max >= 225000) return 70;
    if(t->bitlen_max >= 210000) return 50;
    return 0;
}

static int score_sync_sensitive(const uft_cbm_track_metrics_t* t){
    if(!t || t->max_sync_bits == 0) return 0;
    /* Counting sync lengths or relying on "very long" sync. */
    if(t->max_sync_bits >= 1400) return 85;
    if(t->max_sync_bits >= 1000) return 65;
    if(t->max_sync_bits >= 700)  return 40;
    return 0;
}

static int score_weakbits(const uft_cbm_track_metrics_t* t){
    if(!t) return 0;
    /* We approximate weak bits by illegal events + variance + multi-rev.
       True weak-bit detection should be done in your decoder; this is a placeholder. */
    int s = 0;
    if(t->revolutions >= 2 && t->illegal_gcr_events >= 50) s += 40;
    if(t->illegal_gcr_events >= 200) s += 40;
    if(t->no_sync_detected) s += 20;
    return clamp100(s);
}

static int score_halft_or_extra(const uft_cbm_track_metrics_t* t, bool want_half){
    if(!t) return 0;
    bool is_half = (t->track_x2 % 2) != 0;
    bool is_extra = (t->track_x2/2) > 35;
    if(want_half && !is_half) return 0;
    if(!want_half && !is_extra) return 0;

    if(t->has_meaningful_data) return 80;
    return 35;
}

static int score_bitrate(const uft_cbm_track_metrics_t* t){
    if(!t) return 0;
    return t->nonstandard_bitrate ? 70 : 0;
}

static int score_gap_sigs(const uft_cbm_track_metrics_t* t){
    if(!t) return 0;
    int s = 0;
    if(t->gap_non55_bytes >= 32) s += 55;
    else if(t->gap_non55_bytes >= 8) s += 35;
    if(t->gap_length_weird) s += 20;
    return clamp100(s);
}

static int score_key_track_like(const uft_cbm_track_metrics_t* t){
    if(!t) return 0;
    /* Key tracks can be all sync, all zeros/unformatted, special fill patterns.
       Approximate by "no sync" (or huge sync) + lots of illegal events + low sector structure. */
    int s = 0;
    if(t->no_sync_detected) s += 40;
    if(score_sync_sensitive(t) >= 65) s += 25;
    if(t->illegal_gcr_events >= 200) s += 25;
    if(t->sector_count_observed == 0) s += 10;
    return clamp100(s);
}

static int score_track_sync(const uft_cbm_track_metrics_t* t){
    if(!t) return 0;
    /* Track synchronization needs index awareness to confirm.
       If capture indicates alignment and index is present, flag. */
    if(t->has_index_reference && t->track_alignment_locked) return 80;
    return 0;
}

bool uft_cbm_prot_analyze(const uft_cbm_track_metrics_t* tracks, size_t track_count,
                          uft_cbm_method_hit_t* hits, size_t hits_cap,
                          uft_cbm_report_t* out){
    if(!out) return false;
    memset(out, 0, sizeof(*out));
    out->summary[0] = '\0';

    if(!tracks || track_count == 0){
        append(out->summary, sizeof(out->summary),
               "No track metrics provided.\n");
        return false;
    }

    size_t n_hits = 0;
    int overall = 0;
    int flagged = 0;

    /* Disk-wide aggregations */
    int any_bitrate = 0, any_gap = 0, any_longsync = 0, any_longtrk = 0, any_weak = 0, any_nosync = 0;
    int any_half = 0, any_extra = 0, any_errors = 0, any_key = 0, any_tracksync = 0;

    for(size_t i=0;i<track_count;i++){
        const uft_cbm_track_metrics_t* t = &tracks[i];

        int e = score_errors(t);
        int lt = score_longtrack(t);
        int ss = score_sync_sensitive(t);
        int wk = score_weakbits(t);
        int ht = score_halft_or_extra(t, true);
        int ex = score_halft_or_extra(t, false);
        int br = score_bitrate(t);
        int gs = score_gap_sigs(t);
        int kt = score_key_track_like(t);
        int ts = score_track_sync(t);

        if(e){ any_errors=1; push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_INTENTIONAL_ERRORS, t->track_x2, e); overall += 10 + e/12; flagged++; }
        if(lt){ any_longtrk=1; push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_LONG_TRACKS, t->track_x2, lt); overall += 8 + lt/14; flagged++; }
        if(ss){ any_longsync=1; push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_SYNC_COUNTING, t->track_x2, ss); overall += 6 + ss/18; flagged++; }
        if(wk){ any_weak=1; push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_WEAK_BITS_UNFORMATTED, t->track_x2, wk); overall += 8 + wk/14; flagged++; }
        if(ht){ any_half=1; push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_HALF_TRACKS, t->track_x2, ht); overall += 7 + ht/20; flagged++; }
        if(ex){ any_extra=1; push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_EXTRA_TRACKS, t->track_x2, ex); overall += 7 + ex/20; flagged++; }
        if(br){ any_bitrate=1; push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_CHANGED_BITRATES, t->track_x2, br); overall += 6 + br/20; flagged++; }
        if(gs){ any_gap=1; push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_GAP_SIGNATURES, t->track_x2, gs); overall += 6 + gs/20; flagged++; }
        if(kt){ any_key=1; push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_SIGNATURE_KEY_TRACKS, t->track_x2, kt); overall += 8 + kt/18; flagged++; }
        if(t->no_sync_detected){ any_nosync=1; push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_NO_SYNC, t->track_x2, 80); overall += 10; flagged++; }
        if(ts){ any_tracksync=1; push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_TRACK_SYNCHRONIZATION, t->track_x2, ts); overall += 10; flagged++; }
    }

    /* Infer higher-level methods that require specialized hardware or cross-track behavior:
       - Track skew / fat tracks: needs track-to-track timing/alignment checks, best with index data.
       - Long sectors / custom formats: requires sector parser; approximate via "sector count weird + long tracks + illegal events".
       - SpiraDisc-like: half-track stepping with alignment, needs index reference.
    */
    if(any_tracksync){
        push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_TRACK_SKEW, 0, 60);
        push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_FAT_TRACKS, 0, 55);
        overall += 10;
    }

    if(any_longtrk && any_weak && any_gap == 0){
        push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_CUSTOM_FORMATS, 0, 45);
    }

    if(any_half && any_tracksync){
        push_hit(hits, hits_cap, &n_hits, UFT_CBM_PROT_SPIRADISC_LIKE, 0, 55);
        overall += 8;
    }

    /* Normalize overall */
    overall = clamp100(overall);
    if(flagged == 0) overall = 0;

    out->overall_0_100 = overall;
    out->hits_written = n_hits;

    /* Summary */
    append(out->summary, sizeof(out->summary),
           "CBM/1541 protection methods analysis (preservation-oriented):\n");

    if(overall == 0){
        append(out->summary, sizeof(out->summary),
               "- No strong protection traits flagged from supplied metrics.\n");
    } else {
        char buf[256];
        snprintf(buf, sizeof(buf), "- Overall: %d/100 (traits flagged: %d)\n", overall, flagged);
        append(out->summary, sizeof(out->summary), buf);

        append(out->summary, sizeof(out->summary),
               "Professional capture advice:\n"
               "- Keep raw flux.\n"
               "- Capture >=3 revolutions on suspect tracks.\n"
               "- Include half-tracks and tracks >35 if your hardware allows.\n"
               "- Preserve gaps and sync lengths; do not 'normalize' them.\n"
               "- If you have an index sensor, record index timing for track-sync/Skew/Fat-track cases.\n");
    }

    append(out->summary, sizeof(out->summary),
           "Safety note: This module does not implement bypass/patching; it only classifies preservation-relevant traits.\n");

    return true;
}

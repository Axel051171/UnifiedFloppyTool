/*
 * uft_c64_rapidlok2_detect.c
 */

#include "uft/uft_error.h"
#include "uft_c64_rapidlok2_detect.h"
#include <string.h>
#include <stdio.h>

static void append(char* dst, size_t cap, const char* s){
    if(!dst || cap==0) return;
    size_t cur = strlen(dst);
    if(cur >= cap-1) return;
    size_t rem = cap - cur;
    snprintf(dst + cur, rem, "%s", s);
}

size_t uft_rl2_count_byte(const uint8_t* buf, size_t len, uint8_t v, size_t cap){
    if(!buf) return 0;
    size_t c=0;
    for(size_t i=0;i<len;i++){
        if(buf[i]==v){
            c++;
            if(cap && c>=cap) break;
        }
    }
    return c;
}

static bool any_multi_rev(const uft_rl2_track_view_t* t, size_t n){
    for(size_t i=0;i<n;i++) if(t[i].revolutions >= 3) return true;
    return false;
}

static void scan_track_markers(const uft_rl2_track_view_t* tv,
                               bool* has52, bool* has75, bool* has6b, bool* has7b,
                               bool* unusual_sync){
    if(!tv || !tv->bytes || tv->bytes_len == 0) return;

    /* We purposely avoid "patch signatures". We look only for structural marker bytes that
       the text describes as required/present on RL disks. fileciteturn9file0 */
    size_t c52 = uft_rl2_count_byte(tv->bytes, tv->bytes_len, 0x52, 0);
    size_t c75 = uft_rl2_count_byte(tv->bytes, tv->bytes_len, 0x75, 0);
    size_t c6b = uft_rl2_count_byte(tv->bytes, tv->bytes_len, 0x6B, 0);
    size_t c7b = uft_rl2_count_byte(tv->bytes, tv->bytes_len, 0x7B, 0);

    /* Thresholds are conservative; different decoders may emit different byte views. */
    if(c52 >= 1 && has52) *has52 = true;
    if(c75 >= 1 && has75) *has75 = true;
    if(c6b >= 1 && has6b) *has6b = true;
    if(c7b >= 1 && has7b) *has7b = true;

    if(unusual_sync && tv->max_sync_bits >= 800) *unusual_sync = true; /* "very long sync" */
}

static const uft_rl2_track_view_t* find_track(const uft_rl2_track_view_t* t, size_t n, int track_x2){
    for(size_t i=0;i<n;i++) if(t[i].track_x2 == track_x2) return &t[i];
    return NULL;
}

bool uft_rl2_analyze(const uft_rl2_track_view_t* tracks, size_t track_count,
                     uft_rl2_report_t* out){
    if(!out) return false;
    memset(out, 0, sizeof(*out));
    if(out->summary[0] != '\0') out->summary[0] = '\0';

    if(!tracks || track_count == 0){
        snprintf(out->summary, sizeof(out->summary),
                 "No track data supplied. Provide decoded track bytes + optional sync metrics.");
        return false;
    }

    out->has_multi_rev = any_multi_rev(tracks, track_count);

    /* Scan all provided tracks */
    for(size_t i=0;i<track_count;i++){
        scan_track_markers(&tracks[i],
                           &out->has_52_dos_headers,
                           &out->has_75_headers,
                           &out->has_6b_data,
                           &out->has_7b_extras,
                           &out->has_unusual_sync);
    }

    /* Track 18 anchor heuristic:
       RL2 loader routines are often pulled from track 18 (as described in text). fileciteturn9file0
       We don't search for opcodes/addresses, just check if track 18 has multiple markers. */
    const uft_rl2_track_view_t* t18 = find_track(tracks, track_count, 36);
    if(t18){
        size_t m = 0;
        if(uft_rl2_count_byte(t18->bytes, t18->bytes_len, 0x52, 0) >= 1) m++;
        if(uft_rl2_count_byte(t18->bytes, t18->bytes_len, 0x75, 0) >= 1) m++;
        if(uft_rl2_count_byte(t18->bytes, t18->bytes_len, 0x6B, 0) >= 1) m++;
        if(uft_rl2_count_byte(t18->bytes, t18->bytes_len, 0x7B, 0) >= 1) m++;
        out->suggests_track18_anchor = (m >= 2);
    }

    /* Confidence scoring (heuristic) */
    int score = 0;
    if(out->has_52_dos_headers) score += 15;
    if(out->has_75_headers)     score += 25;
    if(out->has_6b_data)        score += 15;
    if(out->has_7b_extras)      score += 20;
    if(out->has_unusual_sync)   score += 10;
    if(out->suggests_track18_anchor) score += 10;
    if(out->has_multi_rev)      score += 5;

    if(score > 100) score = 100;
    out->confidence_0_100 = score;

    /* Summary / preservation guidance */
    append(out->summary, sizeof(out->summary), "RapidLok2 detection (preservation-oriented):\n");

    if(out->has_75_headers)
        append(out->summary, sizeof(out->summary), "- Observed marker 0x75 (RapidLok data header).\n");
    else
        append(out->summary, sizeof(out->summary), "- Marker 0x75 not observed (could be decode/view mismatch).\n");

    if(out->has_6b_data)
        append(out->summary, sizeof(out->summary), "- Observed marker 0x6B (RapidLok data sector).\n");
    if(out->has_7b_extras)
        append(out->summary, sizeof(out->summary), "- Observed marker 0x7B (RapidLok extra sectors).\n");
    if(out->has_52_dos_headers)
        append(out->summary, sizeof(out->summary), "- Observed marker 0x52 (DOS reference headers on RL-formatted tracks).\n");

    if(out->has_unusual_sync)
        append(out->summary, sizeof(out->summary), "- Very long sync runs observed (sync-length sensitivity is important).\n");
    else
        append(out->summary, sizeof(out->summary), "- No very-long sync runs observed (or sync metrics missing).\n");

    if(out->suggests_track18_anchor)
        append(out->summary, sizeof(out->summary), "- Track 18 shows multiple RL markers; treat it as a critical anchor track.\n");

    if(out->has_multi_rev)
        append(out->summary, sizeof(out->summary), "- Multi-revolution capture detected (good).\n");
    else
        append(out->summary, sizeof(out->summary), "- Recommendation: capture >=3 revolutions on sensitive tracks (18..29, 36) for preservation-grade stability checks.\n");

    {
        char buf[128];
        snprintf(buf, sizeof(buf), "- Confidence: %d/100\n", out->confidence_0_100);
        append(out->summary, sizeof(out->summary), buf);
    }

    append(out->summary, sizeof(out->summary),
           "Note: This module does not implement patching/bypass. It only helps identify likely RL2 structure and capture needs.\n");

    return true;
}

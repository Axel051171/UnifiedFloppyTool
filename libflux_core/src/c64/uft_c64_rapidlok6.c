/*
 * uft_c64_rapidlok6.c
 */

#include "uft/uft_error.h"
#include "uft_c64_rapidlok6.h"
#include <string.h>
#include <stdio.h>

static void append(char* dst, size_t cap, const char* s){
    if(!dst || cap==0) return;
    size_t cur = strlen(dst);
    if(cur >= cap-1) return;
    size_t rem = cap - cur;
    snprintf(dst + cur, rem, "%s", s);
}

uft_rl6_track_group_t uft_rl6_group_for_track(int track_num){
    if(track_num == 18) return UFT_RL6_TRK_18_SPECIAL;
    if(track_num == 36) return UFT_RL6_TRK_36_KEY;
    if(track_num >= 1 && track_num <= 17) return UFT_RL6_TRK_1_17;
    if(track_num >= 19 && track_num <= 35) return UFT_RL6_TRK_19_35;
    return UFT_RL6_TRK_UNKNOWN;
}

static size_t count_byte(const uint8_t* b, size_t n, uint8_t v, size_t cap){
    if(!b) return 0;
    size_t c=0;
    for(size_t i=0;i<n;i++){
        if(b[i]==v){
            c++;
            if(cap && c>=cap) break;
        }
    }
    return c;
}

static bool within_u16(uint16_t v, uint16_t target, uint16_t tol){
    if(v == 0) return false;
    return (v >= (uint16_t)(target - tol)) && (v <= (uint16_t)(target + tol));
}

bool uft_rl6_analyze_track(const uft_rl6_track_view_t* tv, uft_rl6_track_report_t* out){
    if(!tv || !out) return false;
    memset(out, 0, sizeof(*out));
    out->group = uft_rl6_group_for_track(tv->track_num);

    if(!tv->bytes || tv->bytes_len == 0){
        snprintf(out->summary, sizeof(out->summary),
                 "Track %d: no bytes supplied.", tv->track_num);
        return false;
    }

    /* Structural markers */
    size_t c7b = count_byte(tv->bytes, tv->bytes_len, UFT_RL6_MARK_EXTRA, 0);
    size_t c52 = count_byte(tv->bytes, tv->bytes_len, UFT_RL6_MARK_DOSREF, 0);
    size_t c75 = count_byte(tv->bytes, tv->bytes_len, UFT_RL6_MARK_HDR, 0);
    size_t c6b = count_byte(tv->bytes, tv->bytes_len, UFT_RL6_MARK_DATA, 0);

    out->has_extra_sector_7b = (c7b >= 16); /* extra sector tends to contain many 0x7B bytes */
    out->has_dosref_52 = (c52 >= 1);
    out->has_hdr_75 = (c75 >= 1);
    out->has_data_6b = (c6b >= 1);

    out->suggests_parity_present = (c6b >= 3); /* many data blocks -> parity likely relevant */

    /* Sync heuristics with tolerances (in bytes of 0xFF) */
    const uint16_t tol = 6;
    out->start_sync_reasonable = within_u16(tv->start_sync_ff_run, 0x14, tol);
    out->dosref_sync_reasonable = within_u16(tv->dosref_sync_ff_run, 0x29, tol);
    /* first data sync after DOS ref is expected long (~0x3E) */
    out->first_data_sync_long = within_u16(tv->first_data_hdr_sync_ff_run, 0x3E, 12);

    /* Confidence scoring */
    int score = 0;

    /* Track 18 is special; don't over-score it. */
    if(out->group == UFT_RL6_TRK_18_SPECIAL){
        score += out->has_dosref_52 ? 10 : 0;
        score += out->has_hdr_75 ? 10 : 0;
        score += out->has_data_6b ? 10 : 0;
    } else {
        if(out->has_extra_sector_7b) score += 20;
        if(out->has_dosref_52) score += 15;
        if(out->has_hdr_75) score += 20;
        if(out->has_data_6b) score += 15;

        if(out->start_sync_reasonable) score += 10;
        if(out->dosref_sync_reasonable) score += 10;
        if(out->first_data_sync_long) score += 10;

        if(tv->revolutions >= 3) score += 5;
        if(out->group == UFT_RL6_TRK_36_KEY) score += 5; /* key track exists & was analyzed */
    }

    if(score > 100) score = 100;
    out->confidence_0_100 = score;

    /* Summary */
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "RapidLok6 track analysis: track %d\n", tv->track_num);
        append(out->summary, sizeof(out->summary), buf);
    }

    switch(out->group){
        case UFT_RL6_TRK_1_17: append(out->summary, sizeof(out->summary), "- Group: tracks 1-17 (12 data sectors, ~307692 bit/s)\n"); break;
        case UFT_RL6_TRK_19_35: append(out->summary, sizeof(out->summary), "- Group: tracks 19-35 (11 data sectors, ~285714 bit/s)\n"); break;
        case UFT_RL6_TRK_18_SPECIAL: append(out->summary, sizeof(out->summary), "- Group: track 18 (special/loader/DOS)\n"); break;
        case UFT_RL6_TRK_36_KEY: append(out->summary, sizeof(out->summary), "- Group: track 36 (key track)\n"); break;
        default: append(out->summary, sizeof(out->summary), "- Group: unknown\n"); break;
    }

    append(out->summary, sizeof(out->summary),
           out->has_extra_sector_7b ? "- Found $7B-heavy region (extra sector)\n" :
                                      "- Extra sector ($7B) not clearly found\n");
    append(out->summary, sizeof(out->summary),
           out->has_dosref_52 ? "- Found $52 DOS reference header\n" :
                                "- DOS reference header ($52) not found\n");
    append(out->summary, sizeof(out->summary),
           out->has_hdr_75 ? "- Found $75 RL data headers\n" :
                             "- RL data header marker ($75) not found\n");
    append(out->summary, sizeof(out->summary),
           out->has_data_6b ? "- Found $6B RL data blocks\n" :
                              "- RL data block marker ($6B) not found\n");

    if(tv->start_sync_ff_run){
        char buf[96];
        snprintf(buf, sizeof(buf), "- Start sync FF run: %u bytes (%s)\n",
                 (unsigned)tv->start_sync_ff_run,
                 out->start_sync_reasonable ? "ok" : "off");
        append(out->summary, sizeof(out->summary), buf);
    }
    if(tv->dosref_sync_ff_run){
        char buf[96];
        snprintf(buf, sizeof(buf), "- DOSref sync FF run: %u bytes (%s)\n",
                 (unsigned)tv->dosref_sync_ff_run,
                 out->dosref_sync_reasonable ? "ok" : "off");
        append(out->summary, sizeof(out->summary), buf);
    }
    if(tv->first_data_hdr_sync_ff_run){
        char buf[96];
        snprintf(buf, sizeof(buf), "- First data header sync FF run: %u bytes (%s)\n",
                 (unsigned)tv->first_data_hdr_sync_ff_run,
                 out->first_data_sync_long ? "long-ok" : "short/unknown");
        append(out->summary, sizeof(out->summary), buf);
    }

    {
        char buf[96];
        snprintf(buf, sizeof(buf), "- Confidence: %d/100\n", out->confidence_0_100);
        append(out->summary, sizeof(out->summary), buf);
    }

    if(out->group != UFT_RL6_TRK_18_SPECIAL){
        if(tv->revolutions < 3){
            append(out->summary, sizeof(out->summary),
                   "- Suggestion: capture >=3 revolutions on sensitive tracks for preservation-grade stability checks.\n");
        }
        if(out->group == UFT_RL6_TRK_36_KEY){
            append(out->summary, sizeof(out->summary),
                   "- Note: track 36 is critical; missing/unstable dumps here break extra-sector verification.\n");
        }
    }

    return true;
}

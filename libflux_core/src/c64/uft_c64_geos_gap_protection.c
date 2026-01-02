/*
 * uft_c64_geos_gap_protection.c
 */

#include "uft/uft_error.h"
#include "uft_c64_geos_gap_protection.h"
#include <string.h>
#include <stdio.h>

static void append(char* dst, size_t cap, const char* s){
    if(!dst || cap==0) return;
    size_t cur = strlen(dst);
    if(cur >= cap-1) return;
    size_t rem = cap - cur;
    snprintf(dst + cur, rem, "%s", s);
}

static uft_geos_gap_rule_cfg_t defaults(void){
    uft_geos_gap_rule_cfg_t c;
    c.sync_run_min = 10;          /* typical 1541 SYNC definition */
    c.allowed_a = 0x55;
    c.allowed_b = 0x67;
    c.require_trailing_67 = true;
    c.track_number = 21;
    return c;
}

static inline bool is_sync_byte(uint8_t b){
    return b == 0xFF;
}

static bool is_allowed_gap_byte(uint8_t b, const uft_geos_gap_rule_cfg_t* cfg){
    return (b == cfg->allowed_a) || (b == cfg->allowed_b);
}

/* Find SYNC runs and treat everything between them as "gap+block".
 * We conservatively define "gap region" as the bytes immediately before a SYNC run,
 * up to the previous non-gap boundary. Without a full GCR sector parser, we cannot
 * distinguish header/data blocks precisely; instead, we validate "bytes that are not
 * part of SYNC runs" that appear right before SYNC (the "cap").
 *
 * This matches the article's concept of checking "Vorspannlücken" and "Datenlücken"
 * just before SYNC boundaries. citeturn2view3
 */
bool uft_geos_gap_validate_track(const uint8_t* track_bytes, size_t track_len,
                                 const uft_geos_gap_rule_cfg_t* cfg_in,
                                 uft_geos_gap_findings_t* out){
    if(!track_bytes || track_len == 0 || !out) return false;
    memset(out, 0, sizeof(*out));

    uft_geos_gap_rule_cfg_t cfg = cfg_in ? *cfg_in : defaults();
    out->track_number = cfg.track_number;

    if(cfg.sync_run_min < 2) cfg.sync_run_min = 2;

    /* Strategy:
     * - Walk the stream, detect SYNC runs of >= sync_run_min bytes of 0xFF.
     * - For each SYNC run start position, look backwards to find the "cap region":
     *   the contiguous run of bytes immediately preceding SYNC that are NOT 0xFF.
     * - Validate that cap region bytes are only 0x55 or 0x67, and that the last byte is 0x67.
     *
     * This is intentionally simple and works even if you don't have a full sector parser.
     */
    for(size_t i=0;i<track_len;){
        /* detect SYNC run */
        if(is_sync_byte(track_bytes[i])){
            size_t j = i;
            while(j < track_len && is_sync_byte(track_bytes[j])) j++;
            size_t run = j - i;

            if(run >= cfg.sync_run_min){
                /* SYNC run begins at i. Validate preceding cap region. */
                size_t cap_end = i; /* exclusive */
                size_t cap_start = cap_end;
                while(cap_start > 0 && !is_sync_byte(track_bytes[cap_start-1])){
                    cap_start--;
                    /* stop runaway: cap regions are usually not gigantic; but we avoid hard limits. */
                }

                /* If cap_start==cap_end => no preceding non-FF bytes; skip. */
                if(cap_start < cap_end){
                    out->gaps_found++;

                    uint32_t bad=0;
                    for(size_t k=cap_start;k<cap_end;k++){
                        if(!is_allowed_gap_byte(track_bytes[k], &cfg)){
                            bad++;
                        }
                    }
                    if(bad){
                        out->gaps_bad_bytes++;
                        out->bad_byte_count += bad;
                    }

                    if(cfg.require_trailing_67){
                        uint8_t last = track_bytes[cap_end-1];
                        if(last != 0x67){
                            out->gaps_bad_trailing++;
                        }
                    }
                }
            }

            i = j;
        } else {
            i++;
        }
    }

    /* Score:
     * - If no gaps found, we can't decide.
     * - Otherwise penalize violations.
     */
    int score = 0;
    if(out->gaps_found == 0){
        score = 0;
        append(out->summary, sizeof(out->summary),
               "No SYNC runs found with configured threshold; cannot validate GEOS gap rules.\n");
    } else {
        score = 100;
        /* penalize each bad gap strongly */
        score -= (int)(out->gaps_bad_bytes * 8);
        score -= (int)(out->gaps_bad_trailing * 10);
        /* penalize total bad bytes */
        if(out->bad_byte_count > 0){
            int p = (int)(out->bad_byte_count / 8);
            if(p > 25) p = 25;
            score -= p;
        }
        if(score < 0) score = 0;

        {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "GEOS gap validation (track %d): gaps_found=%u bad_gaps=%u bad_trailing=%u bad_bytes=%u score=%d/100\n",
                     out->track_number,
                     (unsigned)out->gaps_found,
                     (unsigned)out->gaps_bad_bytes,
                     (unsigned)out->gaps_bad_trailing,
                     (unsigned)out->bad_byte_count,
                     score);
            append(out->summary, sizeof(out->summary), buf);
        }

        append(out->summary, sizeof(out->summary),
               "Expected (per GEOS protection check): gap bytes only 0x55/0x67 and each checked gap ends with 0x67 before SYNC.\n");
    }

    out->confidence_0_100 = score;
    return true;
}

uint32_t uft_geos_gap_rewrite_inplace(uint8_t* track_bytes, size_t track_len,
                                      const uft_geos_gap_rule_cfg_t* cfg_in){
    if(!track_bytes || track_len == 0) return 0;
    uft_geos_gap_rule_cfg_t cfg = cfg_in ? *cfg_in : defaults();
    if(cfg.sync_run_min < 2) cfg.sync_run_min = 2;

    uint32_t modified = 0;

    for(size_t i=0;i<track_len;){
        if(is_sync_byte(track_bytes[i])){
            size_t j=i;
            while(j<track_len && is_sync_byte(track_bytes[j])) j++;
            size_t run=j-i;

            if(run >= cfg.sync_run_min){
                size_t cap_end = i;
                size_t cap_start = cap_end;
                while(cap_start > 0 && !is_sync_byte(track_bytes[cap_start-1])){
                    cap_start--;
                }

                if(cap_start < cap_end){
                    bool any_change = false;

                    /* rewrite all cap bytes to 0x55 unless already allowed */
                    for(size_t k=cap_start;k<cap_end;k++){
                        if(!is_allowed_gap_byte(track_bytes[k], &cfg) || track_bytes[k] != 0x55){
                            track_bytes[k] = 0x55;
                            any_change = true;
                        }
                    }

                    if(cfg.require_trailing_67){
                        if(track_bytes[cap_end-1] != 0x67){
                            track_bytes[cap_end-1] = 0x67;
                            any_change = true;
                        }
                    }

                    if(any_change) modified++;
                }
            }

            i=j;
        } else {
            i++;
        }
    }

    return modified;
}

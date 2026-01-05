#include "uft/uft_error.h"
#include "cpc_pipeline.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

/* Track+sector confidence scoring.
 * We do NOT 'fix' data. We only score how trustworthy it is.
 */
static uint8_t clamp_u8(int v) { if (v < 0) return 0; if (v > 100) return 100; return (uint8_t)v; }

static void cpc_apply_sector_confidence(ufm_logical_image_t *li, const flux_consensus_stats_t *cs)
{
    if (!li) return;
    int track_base = 100;
    if (cs && cs->total_bits) {
        /* conflicts ratio => penalize */
        const double r = (cs->total_bits != 0)
            ? (double)cs->weak_bits / (double)cs->total_bits
            : 0.0;
        if (r > 0.10) track_base -= 40;
        else if (r > 0.05) track_base -= 25;
        else if (r > 0.01) track_base -= 10;
    }
    for (uint32_t i = 0; i < li->count; ++i) {
        ufm_sector_t *s = &li->sectors[i];
        int c = track_base;
        /* We currently track CRC failures as a single flag. */
        if (s->flags & UFM_SEC_BAD_CRC) c -= 40;
        if (s->flags & UFM_SEC_DELETED_DAM) c -= 5;
        if (s->meta_type == UFM_SECTOR_META_MFM_IBM && s->meta) {
            const ufm_sector_meta_mfm_ibm_t *m = (const ufm_sector_meta_mfm_ibm_t*)s->meta;
            if (m->weak_score) c -= (int)m->weak_score * 8;
        }
        s->confidence = clamp_u8(c);
    }
}


static uint64_t sum_dt_ns(const uint32_t *dt, uint32_t n)
{
    uint64_t s = 0;
    for (uint32_t i = 0; i < n; i++) s += dt[i];
    return s;
}

static int score_stats(const cpc_mfm_decode_stats_t *st)
{
    /* Higher is better: sectors weigh heavy, CRC hurts. */
    int score = (int)st->sectors_emitted * 100;
    score -= (int)st->bad_crc_dam * 10;
    score -= (int)st->bad_crc_idam * 3;
    score -= (int)st->truncated_fields * 25;
    return score;
}

int cpc_decode_track_from_ufm(
    const ufm_revolution_t *cap,
    uint16_t cyl, uint16_t head,
    const flux_pll_params_t *pll_in,
    ufm_logical_image_t *li_out,
    cpc_flux_decode_result_t *res_out)
{
    if (!cap || !cap->dt_ns || cap->count == 0 || !pll_in || !pll_in->nominal_bitcell_ns || !li_out)
        return -EINVAL;

    cpc_flux_decode_result_t res;
    memset(&res, 0, sizeof(res));

    ufm_revolution_t *slices = NULL;
    uint32_t nslices = 0;
    int rc = ufm_slice_by_index(cap, &slices, &nslices);
    if (rc < 0) return rc;
    res.slices_tried = nslices;

    /* ---------------- Sync-anchored multi-rev consensus path ----------------
     *
     * We decode PLL raw bits for each rev-slice, find the first MFM sync word
     * (0x4489) and rotate the slice so that this sync becomes bit 0.
     * Then we majority-vote across revolutions to build a "consensus" raw track.
     * Weak/disputed bits are tracked in consensus_stats. We do NOT attempt to
     * smooth timing or repair flux here.
     */

    /* temp arrays for consensus */
    uint8_t **rev_raw = NULL;
    size_t  *rev_bits = NULL;
    uint32_t rev_used = 0;

    if (nslices >= 2)
    {
        rev_raw = (uint8_t**)calloc(nslices, sizeof(uint8_t*));
        rev_bits = (size_t*)calloc(nslices, sizeof(size_t));
        if (!rev_raw || !rev_bits) {
            rc = -ENOMEM;
            goto cleanup_slices;
        }

        for (uint32_t i = 0; i < nslices; i++)
        {
            const ufm_revolution_t *sl = &slices[i];
            if (sl->count < 16) continue;

            flux_pll_params_t pll = *pll_in;
            if (pll.max_bitcells == 0) {
                const uint64_t dur = sum_dt_ns(sl->dt_ns, sl->count);
                uint32_t approx = (uint32_t)(dur / pll.nominal_bitcell_ns);
                pll.max_bitcells = approx + 8192u;
            }

            const size_t raw_bytes_cap = ((size_t)pll.max_bitcells + 7u) >> 3;
            uint8_t *raw = (uint8_t*)calloc(1, raw_bytes_cap);
            if (!raw) { rc = -ENOMEM; break; }

            size_t bits_written = 0;
            flux_pll_stats_t pll_st;
            rc = flux_mfm_decode_pll_raw(sl->dt_ns, sl->count, &pll, raw, raw_bytes_cap,
                                         &bits_written, &pll_st);
            if (rc < 0 || bits_written < 256) {
                free(raw);
                continue;
            }

            size_t anchor = 0;
            if (flux_find_mfm_sync_anchor_4489(raw, bits_written, &anchor) < 0) {
                /* no stable anchor -> skip for consensus */
                free(raw);
                continue;
            }
            res.consensus_stats.anchor_hits++;

            /* rotate to anchor */
            uint8_t *rot = (uint8_t*)calloc(1, (bits_written + 7u) >> 3);
            if (!rot) { free(raw); rc = -ENOMEM; break; }
            rc = flux_rotate_bits(raw, bits_written, anchor, rot, (bits_written + 7u) >> 3);
            free(raw);
            if (rc < 0) { free(rot); continue; }

            rev_raw[rev_used] = rot;
            rev_bits[rev_used] = bits_written;
            rev_used++;

            /* keep best pll stats for telemetry (first good one is fine for now) */
            if (res.pll_stats.total_ns == 0) res.pll_stats = pll_st;
        }

        res.consensus_stats.revs_in = nslices;
        res.consensus_stats.revs_used = rev_used;

        if (rc >= 0 && rev_used >= 2)
        {
            /* Build consensus track */
            size_t out_len_bits = 0;
            size_t min_len = (size_t)-1;
            for (uint32_t r = 0; r < rev_used; r++)
                if (rev_bits[r] < min_len) min_len = rev_bits[r];

            const size_t out_bytes = (min_len + 7u) >> 3;
            uint8_t *cons = (uint8_t*)calloc(1, out_bytes);
            uint8_t *weak = (uint8_t*)calloc(1, out_bytes);
            if (!cons || !weak) {
                free(cons); free(weak);
                rc = -ENOMEM;
                goto cleanup_consensus;
            }

            rc = flux_build_consensus_bits((const uint8_t* const*)rev_raw, rev_bits, rev_used,
                                           cons, out_bytes, &out_len_bits,
                                           weak, out_bytes, &res.consensus_stats);
            free(weak);
            if (rc == 0)
            {
                ufm_logical_image_t tmp;
                ufm_logical_init(&tmp);
                cpc_mfm_decode_stats_t mfm_st;
                rc = cpc_mfm_decode_track_bits(cons, out_len_bits, cyl, head, true, &tmp, &mfm_st);
                if (rc == 0) {
                    ufm_logical_free(li_out);
                    *li_out = tmp;
                    res.slices_used = rev_used;
                    res.mfm_stats = mfm_st;
                    /* Assign per-sector confidence (track consensus + CRC flags). */
                    cpc_apply_sector_confidence(&tmp, &res.consensus_stats);
                    free(cons);
                    goto cleanup_consensus;
                }
                ufm_logical_free(&tmp);
            }
            free(cons);
        }
    }

    /* If consensus path fails, fall back to best-slice scoring (old behaviour). */

    /* We'll decode each slice into a temporary logical image and keep the best.
       This avoids committing to the wrong index window when captures are noisy.
     */
    int best_score = INT32_MIN;
    ufm_logical_image_t best_li;
    ufm_logical_init(&best_li);
    cpc_mfm_decode_stats_t best_mfm = {0};
    flux_pll_stats_t best_pll = {0};
    uint32_t best_idx = 0;

    for (uint32_t i = 0; i < nslices; i++)
    {
        const ufm_revolution_t *sl = &slices[i];
        if (sl->count < 16) continue;

        /* Build per-slice PLL params with a sane max_bitcells derived from duration.
           Max cells ~ duration/nom + margin.
         */
        flux_pll_params_t pll = *pll_in;
        if (pll.max_bitcells == 0) {
            const uint64_t dur = sum_dt_ns(sl->dt_ns, sl->count);
            uint32_t approx = (uint32_t)(dur / pll.nominal_bitcell_ns);
            pll.max_bitcells = approx + 8192u;
        }

        const size_t raw_bytes_cap = ((size_t)pll.max_bitcells + 7u) >> 3;
        uint8_t *raw_bits = (uint8_t*)calloc(1, raw_bytes_cap);
        if (!raw_bits) { rc = -ENOMEM; break; }

        size_t raw_bits_written = 0;
        flux_pll_stats_t pll_st;
        rc = flux_mfm_decode_pll_raw(sl->dt_ns, sl->count, &pll, raw_bits, raw_bytes_cap,
                                     &raw_bits_written, &pll_st);
        if (rc < 0) {
            free(raw_bits);
            continue;
        }

        ufm_logical_image_t tmp;
        ufm_logical_init(&tmp);
        cpc_mfm_decode_stats_t mfm_st;
        rc = cpc_mfm_decode_track_bits(raw_bits, raw_bits_written, cyl, head, true, &tmp, &mfm_st);
        free(raw_bits);
        if (rc < 0) {
            ufm_logical_free(&tmp);
            continue;
        }

        const int sc = score_stats(&mfm_st);
        if (sc > best_score) {
            best_score = sc;
            best_idx = i;
            best_mfm = mfm_st;
            best_pll = pll_st;
            ufm_logical_free(&best_li);
            best_li = tmp; /* struct copy (owns heap); tmp must not be freed now */
        } else {
            ufm_logical_free(&tmp);
        }
    }

cleanup_consensus:
    if (rev_raw) {
        for (uint32_t r = 0; r < rev_used; r++) free(rev_raw[r]);
        free(rev_raw);
    }
    free(rev_bits);

cleanup_slices:
    /* cleanup slices */
    for (uint32_t i = 0; i < nslices; i++)
        ufm_revolution_free_contents(&slices[i]);
    free(slices);

    if (best_score == INT32_MIN) {
        ufm_logical_free(&best_li);
        return -EIO;
    }

    /* move best logical image into caller */
    ufm_logical_free(li_out);
    *li_out = best_li;

    res.slices_used = 1;
    res.mfm_stats = best_mfm;
    res.pll_stats = best_pll;

    if (res_out) *res_out = res;
    (void)best_idx; /* reserved for future debug/telemetry */
    return 0;
}
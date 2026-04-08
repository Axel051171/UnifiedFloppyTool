/**
 * @file ufm_c64_scheme_detect.c
 * @brief C64 Copy Protection Scheme Detection Implementation
 */

#include "uft/protection/ufm_c64_scheme_detect.h"
#include "uft/protection/ufm_cbm_protection_methods.h"
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Protection Type Names
 * ═══════════════════════════════════════════════════════════════════════════ */

const char *ufm_c64_prot_type_name(ufm_c64_prot_type_t type)
{
    static const char *names[] = {
        [UFM_PROT_NONE]             = "None",
        [UFM_PROT_VMAX]             = "V-MAX!",
        [UFM_PROT_RAPIDLOK]        = "RapidLok",
        [UFM_PROT_COPYLOCK]        = "CopyLock",
        [UFM_PROT_SPEEDLOCK]       = "Speedlock",
        [UFM_PROT_FATBITS]         = "FatBits",
        [UFM_PROT_PIRATE_SLAYER]   = "Pirate Slayer",
        [UFM_PROT_VORPAL]          = "Vorpal",
        [UFM_PROT_GEOS]            = "GEOS",
        [UFM_PROT_ECA]             = "EA/ECA",
        [UFM_PROT_CUSTOM_SYNC]     = "Custom Sync",
        [UFM_PROT_HALF_TRACK]      = "Half Track",
        [UFM_PROT_LONG_TRACK]      = "Long Track",
        [UFM_PROT_DENSITY_MISMATCH]= "Density Mismatch",
        [UFM_PROT_DUPLICATE_ID]    = "Duplicate ID",
        [UFM_PROT_BAD_GCR]         = "Bad GCR",
        [UFM_PROT_TIMING]          = "Timing-Based",
        [UFM_PROT_UNKNOWN]         = "Unknown",
    };
    if (type >= 0 && type < UFM_PROT_COUNT) return names[type];
    return "Unknown";
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Method Checks
 * ═══════════════════════════════════════════════════════════════════════════ */

bool ufm_cbm_check_vmax(const ufm_c64_track_metrics_t *m)
{
    /* V-MAX! uses custom sync patterns and non-standard sector counts */
    return m->has_custom_sync && m->sector_count != 21 &&
           m->sector_count != 19 && m->sector_count != 18 &&
           m->sector_count != 17;
}

bool ufm_cbm_check_rapidlok(const ufm_c64_track_metrics_t *m)
{
    /* RapidLok uses half-tracks and custom sync on track 36+ */
    return m->has_half_track && m->track >= 36;
}

bool ufm_cbm_check_custom_sync(const ufm_c64_track_metrics_t *m)
{
    return m->has_custom_sync;
}

bool ufm_cbm_check_half_track(const ufm_c64_track_metrics_t *m)
{
    return m->has_half_track;
}

bool ufm_cbm_check_long_track(const ufm_c64_track_metrics_t *m,
                               float threshold)
{
    return m->track_length_ratio > threshold;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main Analysis
 * ═══════════════════════════════════════════════════════════════════════════ */

static int add_hit(ufm_c64_prot_hit_t *hits, int max_hits, int *count,
                   ufm_c64_prot_type_t type, uint8_t track, uint8_t confidence,
                   const char *desc)
{
    if (*count >= max_hits) return 0;
    ufm_c64_prot_hit_t *h = &hits[*count];
    memset(h, 0, sizeof(*h));
    h->type = type;
    h->track = track;
    h->confidence = confidence;
    snprintf(h->description, sizeof(h->description), "%s", desc);
    (*count)++;
    return 1;
}

bool ufm_c64_prot_analyze(const ufm_c64_track_metrics_t *metrics,
                           int n_tracks,
                           ufm_c64_prot_hit_t *hits_out,
                           int max_hits,
                           ufm_c64_prot_report_t *report)
{
    if (!metrics || !hits_out || !report || n_tracks <= 0) return false;

    memset(report, 0, sizeof(*report));
    int hit_count = 0;
    int long_tracks = 0, half_tracks = 0, custom_syncs = 0;
    int bad_gcr_tracks = 0, dup_id_tracks = 0;

    for (int t = 0; t < n_tracks; t++) {
        const ufm_c64_track_metrics_t *m = &metrics[t];

        /* Long track detection */
        if (ufm_cbm_check_long_track(m, 1.02f)) {
            long_tracks++;
            add_hit(hits_out, max_hits, &hit_count, UFM_PROT_LONG_TRACK,
                    m->track, 80, "Track exceeds standard length");
        }

        /* Half-track detection */
        if (ufm_cbm_check_half_track(m)) {
            half_tracks++;
            add_hit(hits_out, max_hits, &hit_count, UFM_PROT_HALF_TRACK,
                    m->track, 85, "Data on half-track position");
        }

        /* Custom sync */
        if (ufm_cbm_check_custom_sync(m)) {
            custom_syncs++;
            add_hit(hits_out, max_hits, &hit_count, UFM_PROT_CUSTOM_SYNC,
                    m->track, 75, "Non-standard sync pattern");
        }

        /* Bad GCR */
        if (m->bad_gcr_count > 10) {
            bad_gcr_tracks++;
            add_hit(hits_out, max_hits, &hit_count, UFM_PROT_BAD_GCR,
                    m->track, 70, "Invalid GCR byte sequences");
        }

        /* Duplicate sector IDs — confidence scaled by number of affected tracks.
         * A single track with duplicate IDs could be media damage, so we start
         * at 50% and scale up as more tracks exhibit the pattern. */
        if (m->duplicate_ids > 0) {
            dup_id_tracks++;
            uint8_t dup_conf;
            if (dup_id_tracks >= 4)
                dup_conf = 85;
            else if (dup_id_tracks >= 2)
                dup_conf = 70;
            else
                dup_conf = 50;
            add_hit(hits_out, max_hits, &hit_count, UFM_PROT_DUPLICATE_ID,
                    m->track, dup_conf, "Duplicate sector header IDs");
        }

        /* V-MAX! */
        if (ufm_cbm_check_vmax(m)) {
            add_hit(hits_out, max_hits, &hit_count, UFM_PROT_VMAX,
                    m->track, 85, "V-MAX! signature: custom sync + non-standard sectors");
        }

        /* RapidLok */
        if (ufm_cbm_check_rapidlok(m)) {
            add_hit(hits_out, max_hits, &hit_count, UFM_PROT_RAPIDLOK,
                    m->track, 80, "RapidLok signature: half-track >= 36");
        }
    }

    report->hits_written = (uint32_t)hit_count;

    /* Classify primary scheme */
    if (custom_syncs > 3 && half_tracks > 0)
        report->primary_scheme = UFM_PROT_RAPIDLOK;
    else if (custom_syncs > 5)
        report->primary_scheme = UFM_PROT_VMAX;
    else if (long_tracks > 3)
        report->primary_scheme = UFM_PROT_LONG_TRACK;
    else if (dup_id_tracks > 0)
        report->primary_scheme = UFM_PROT_DUPLICATE_ID;
    else if (bad_gcr_tracks > 3)
        report->primary_scheme = UFM_PROT_BAD_GCR;
    else if (hit_count > 0)
        report->primary_scheme = UFM_PROT_UNKNOWN;
    else
        report->primary_scheme = UFM_PROT_NONE;

    /* Confidence */
    if (hit_count == 0) {
        report->confidence_0_100 = 0;
        snprintf(report->summary, sizeof(report->summary), "No protection detected");
    } else {
        int max_conf = 0;
        for (int i = 0; i < hit_count; i++)
            if (hits_out[i].confidence > max_conf) max_conf = hits_out[i].confidence;
        report->confidence_0_100 = max_conf;
        snprintf(report->summary, sizeof(report->summary),
                 "%s detected (%d hits, %d%% confidence)",
                 ufm_c64_prot_type_name(report->primary_scheme),
                 hit_count, max_conf);
    }

    snprintf(report->scheme_name, sizeof(report->scheme_name), "%s",
             ufm_c64_prot_type_name(report->primary_scheme));

    return true;
}

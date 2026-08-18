/**
 * @file test_c64_protection_real_corpus.c
 * @brief POSITIVE control: C64 metrics vs really protected disks (MF-383).
 *
 * MF-381/382 verified the metric extractor against a freshly formatted disk:
 * the numbers were right and nothing false-fired. What that could NOT show is
 * whether the metrics actually discriminate — a detector that always reports
 * "clean" also passes a clean-disk test. This file supplies the missing half.
 *
 * Corpus (LOCAL-ONLY, tests/corpus/ is gitignored; game code is copyrighted):
 * two dumps from the C64 Preservation Project 10th Anniversary Collection,
 * which Pete Rittwage released for "no strings attached" download
 * (archive.org item C64_Preservation_Project_10th_Anniversary_Collection).
 * sha256 + exact path inside the collection are in the corpus manifest.
 *
 *   c64pp_bountybob.g64      Bounty Bob Strikes Back [Big Five, 1985]
 *                            PROTECTED: 71 populated half-track slots, of
 *                            which 35 are true half-tracks; 15 whole tracks
 *                            carry sync marks but no standard 1541 header.
 *   c64pp_aliensyndrome.g64  Alien Syndrome [Sega, 1987]
 *                            A real commercial disk with NO anomaly at all —
 *                            a stronger negative control than a self-formatted
 *                            image, because it is a genuine production disk.
 *
 * Ground truth pinned by independent python inspection BEFORE any UFT run.
 *
 * SKIPS (exit 77) when the local images are absent (e.g. CI).
 */

#include "uft/formats/c64/uft_d64_g64.h"
#include "uft/protection/ufm_c64_metrics.h"
#include "uft/protection/ufm_c64_scheme_detect.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_g64;

#ifndef UFT_CORPUS_RESTRICTED_DIR
#error "UFT_CORPUS_RESTRICTED_DIR must be defined by the build"
#endif

#define SKIP_EXIT 77

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static const char *free_img(const char *name) {
    static char p[512];
    snprintf(p, sizeof(p), "%s/%s", UFT_CORPUS_DIR, name);
    return p;
}

/** Count populated G64 slots via read_half_track(), split by parity (MF-404).
 *  Returns -1 if the image cannot be opened. */
static int count_slots(const char *path, int *whole_out, int *half_out) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    if (uft_format_plugin_g64.open(&disk, path, true) != UFT_OK) return -1;
    if (!uft_format_plugin_g64.read_half_track) {
        uft_format_plugin_g64.close(&disk);
        return -1;
    }

    int whole = 0, half = 0;
    for (int slot = 0; slot < 84; slot++) {
        uft_track_t t; memset(&t, 0, sizeof(t));
        if (uft_format_plugin_g64.read_half_track(&disk, slot, 0, &t) != UFT_OK)
            continue;
        if (t.raw_data && t.raw_size > 0) {
            if (slot % 2) half++; else whole++;
        }
        free(t.raw_data);
        for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
        free(t.sectors);
    }
    uft_format_plugin_g64.close(&disk);
    *whole_out = whole; *half_out = half;
    return whole + half;
}

static const char *img(const char *name) {
    static char p[512];
    snprintf(p, sizeof(p), "%s/%s", UFT_CORPUS_RESTRICTED_DIR, name);
    return p;
}

/** Collect whole-track metrics through the real plugin path. */
static int collect(const char *file, ufm_c64_track_metrics_t *out, int max_tracks) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    if (uft_format_plugin_g64.open(&disk, img(file), true) != UFT_OK) return -1;

    int n = 0;
    for (int cyl = 0; cyl < max_tracks; cyl++) {
        uft_track_t t; memset(&t, 0, sizeof(t));
        if (uft_format_plugin_g64.read_track(&disk, cyl, 0, &t) != UFT_OK) break;
        if (t.raw_data && t.raw_size > 0) {
            if (ufm_c64_metrics_from_gcr(t.raw_data, t.raw_size, cyl * 2,
                                         UFM_C64_SPEED_ZONE_AUTO, &out[n])) {
                n++;
            }
        }
        free(t.raw_data);
        for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
        free(t.sectors);
    }
    uft_format_plugin_g64.close(&disk);
    return n;
}


/* ── The GUI's data path (MF-405, PROT-8) ────────────────────────────────
 * ProtectionAnalysisWidget does not use the format plugin. It reads G64 via
 * g64_load()/g64_get_track() and then classifies. This helper is that path,
 * minus Qt, so it can be tested: src/gui/ has no test coverage at all.
 *
 * Note the index conversion. g64_get_track() numbers slots as track x 2
 * (index 2 = track 1), ufm_c64_metrics_from_gcr() as a 0-based slot
 * (index 0 = track 1). Passing the index through unconverted shifts every
 * track by one and lands on the speed-zone boundaries, which is measurable:
 * it invents three "long track" hits on a disk that has none. */
static int gui_path_analyze(const char *path, ufm_c64_prot_report_t *report,
                            ufm_c64_track_metrics_t *m_out, int max_tracks)
{
    g64_image_t *g = NULL;
    if (g64_load(path, &g) != 0 || !g) return -1;

    int n = 0;
    for (int ht = 2; ht <= g->num_tracks * 2 && ht < G64_MAX_TRACKS; ht++) {
        if (n >= max_tracks) break;
        const uint8_t *d = NULL; size_t len = 0; uint8_t speed = 0;
        if (g64_get_track(g, ht, &d, &len, &speed) != 0 || !d || len == 0)
            continue;
        if (ufm_c64_metrics_from_gcr(d, len, ht - 2, UFM_C64_SPEED_ZONE_AUTO,
                                     &m_out[n]))
            n++;
    }
    g64_free(g);
    if (n <= 0) return -1;

    static ufm_c64_prot_hit_t hits[256];
    if (!ufm_c64_prot_analyze(m_out, n, hits, 256, report)) return -1;
    return n;
}

TEST(protected_disk_flags_exactly_the_pinned_tracks) {
    /* Pinned: whole tracks 1..10, 13..16 and 37 carry sync but no standard
     * header; every other populated whole track is ordinary. */
    static const int expect[] = {1,2,3,4,5,6,7,8,9,10,13,14,15,16,37};
    ufm_c64_track_metrics_t m[42];
    memset(m, 0, sizeof(m));
    int n = collect("c64pp_bountybob.g64", m, 42);
    ASSERT(n > 30);

    int found = 0;
    for (int i = 0; i < n; i++) {
        if (!m[i].has_custom_sync) continue;
        found++;
        bool expected = false;
        for (size_t k = 0; k < sizeof(expect) / sizeof(expect[0]); k++)
            if (m[i].track == expect[k]) { expected = true; break; }
        ASSERT(expected);                  /* no flag outside the pinned set */
    }
    ASSERT(found == (int)(sizeof(expect) / sizeof(expect[0])));
}

TEST(protected_disk_track1_is_short_and_headerless) {
    ufm_c64_track_metrics_t m[42];
    memset(m, 0, sizeof(m));
    ASSERT(collect("c64pp_bountybob.g64", m, 42) > 0);
    ASSERT(m[0].track == 1);
    ASSERT(m[0].sector_count == 0);         /* own format, no 1541 headers */
    ASSERT(m[0].sync_count == 4);           /* pinned */
    ASSERT(m[0].max_sync_run_bits == 30);   /* pinned */
    ASSERT(m[0].has_custom_sync);
    /* 6250 bytes where zone 3 nominally holds 7692 => clearly short */
    ASSERT(m[0].track_length_ratio > 0.81f && m[0].track_length_ratio < 0.82f);
}

TEST(protected_disk_produces_protection_hits) {
    ufm_c64_track_metrics_t m[42];
    memset(m, 0, sizeof(m));
    int n = collect("c64pp_bountybob.g64", m, 42);
    ASSERT(n > 30);

    ufm_c64_prot_hit_t hits[128];
    ufm_c64_prot_report_t report;
    ASSERT(ufm_c64_prot_analyze(m, n, hits, 128, &report));
    ASSERT(report.hits_written > 0);        /* the whole point of a positive control */
    ASSERT(report.confidence_0_100 > 0);

    int custom = 0;
    for (uint32_t i = 0; i < report.hits_written; i++)
        if (hits[i].type == UFM_PROT_CUSTOM_SYNC) custom++;
    ASSERT(custom == 15);                   /* one per pinned track */
}

TEST(real_protected_disk_is_described_not_named_PROT5) {
    /* Regression guard on real data for the PROT-5 fix (MF-402). With 15
     * custom-sync tracks the classifier used to take the `custom_syncs > 5`
     * branch and name the scheme "V-MAX!", derived purely from generic
     * structure with no V-MAX-specific evidence in the code path.
     *
     * It must now report the structure it measured. This says nothing about
     * which protection this disk actually uses — establishing that needs a
     * V-MAX-specific detector run against a disk of documented provenance. */
    ufm_c64_track_metrics_t m[42];
    memset(m, 0, sizeof(m));
    int n = collect("c64pp_bountybob.g64", m, 42);
    ASSERT(n > 30);

    ufm_c64_prot_hit_t hits[128];
    ufm_c64_prot_report_t report;
    ASSERT(ufm_c64_prot_analyze(m, n, hits, 128, &report));

    ASSERT(report.primary_scheme == UFM_PROT_CUSTOM_SYNC);
    ASSERT(report.hits_written > 0);
    for (uint32_t i = 0; i < report.hits_written; i++) {
        ASSERT(hits[i].type != UFM_PROT_VMAX);
        ASSERT(hits[i].type != UFM_PROT_RAPIDLOK);
    }
}

TEST(real_commercial_disk_without_protection_stays_silent) {
    /* Alien Syndrome: 40 tracks, all with standard 1541 headers. A production
     * disk that must not produce a single hit — the negative control that a
     * self-formatted image cannot provide. */
    ufm_c64_track_metrics_t m[42];
    memset(m, 0, sizeof(m));
    int n = collect("c64pp_aliensyndrome.g64", m, 42);
    ASSERT(n == 40);

    for (int i = 0; i < n; i++) {
        ASSERT(!m[i].has_custom_sync);
        ASSERT(m[i].sector_count > 0);
        ASSERT(m[i].duplicate_ids == 0);
    }

    ufm_c64_prot_hit_t hits[128];
    ufm_c64_prot_report_t report;
    ASSERT(ufm_c64_prot_analyze(m, n, hits, 128, &report));
    ASSERT(report.hits_written == 0);
    ASSERT(report.primary_scheme == UFM_PROT_NONE);
}

TEST(half_tracks_are_reachable_through_the_plugin_PROT6) {
    /* PROT-6: read_track() addresses whole cylinders only, so the odd G64
     * slots were unreachable through the plugin API — on this disk 35 tracks
     * that are present in the file were never seen by the pipeline. The
     * corpus manifest pins the expected shape: 71 populated slots, 35 of them
     * true half-tracks. */
    int whole = -1, half = -1;
    int total = count_slots(img("c64pp_bountybob.g64"), &whole, &half);
    ASSERT(total > 0);
    ASSERT(half == 35);
    ASSERT(total == 71);
    ASSERT(whole == 36);
}

TEST(a_disk_without_half_tracks_reports_none) {
    /* Negative control, rights-free: a plain VICE-formatted 35-track disk.
     * If read_half_track() invented data, this would not stay at zero. */
    int whole = -1, half = -1;
    int total = count_slots(free_img("vice_c1541_35trk.g64"), &whole, &half);
    ASSERT(total > 0);
    ASSERT(half == 0);
    ASSERT(whole == 35);
}

TEST(even_slots_agree_with_read_track) {
    /* The two entry points must not drift apart: an even slot is the same
     * data as the corresponding whole track, and is not flagged half. */
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_g64.open(&disk, free_img("vice_c1541_35trk.g64"),
                                      true) == UFT_OK);
    for (int cyl = 0; cyl < 35; cyl++) {
        uft_track_t a, b;
        memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
        ASSERT(uft_format_plugin_g64.read_track(&disk, cyl, 0, &a) == UFT_OK);
        ASSERT(uft_format_plugin_g64.read_half_track(&disk, cyl * 2, 0, &b) == UFT_OK);
        ASSERT(a.raw_size == b.raw_size);
        if (a.raw_size > 0)
            ASSERT(memcmp(a.raw_data, b.raw_data, a.raw_size) == 0);
        ASSERT(!a.is_half_track);
        ASSERT(!b.is_half_track);
        free(a.raw_data); free(b.raw_data);
        for (size_t i = 0; i < a.sector_count; i++) free(a.sectors[i].data);
        for (size_t i = 0; i < b.sector_count; i++) free(b.sectors[i].data);
        free(a.sectors); free(b.sectors);
    }
    uft_format_plugin_g64.close(&disk);
}

TEST(gui_path_populates_the_fields_the_classifier_reads_PROT8) {
    /* The defect PROT-8 named: the widget filled track_x2, revolutions,
     * bitlen_*, weak_region_*, illegal_gcr_events and max_sync_run_bits, while
     * ufm_c64_prot_analyze() reads track, has_half_track, track_length_ratio,
     * has_custom_sync, sector_count, bad_gcr_count and duplicate_ids. The two
     * sets do not overlap at all, so the classifier saw zeros. Measured on
     * this disk before the fix: 71 tracks in, 0 hits out. */
    static ufm_c64_track_metrics_t m[128];
    memset(m, 0, sizeof(m));
    ufm_c64_prot_report_t report;
    int n = gui_path_analyze(img("c64pp_bountybob.g64"), &report, m, 128);
    ASSERT(n == 71);

    int with_track = 0, with_ratio = 0, with_half = 0;
    int sync_whole = 0, sync_half = 0;
    for (int i = 0; i < n; i++) {
        if (m[i].track > 0)                  with_track++;
        if (m[i].track_length_ratio > 0.0f)  with_ratio++;
        if (m[i].has_half_track)             with_half++;
        if (m[i].has_custom_sync) {
            if (m[i].is_half_track) sync_half++; else sync_whole++;
        }
    }
    ASSERT(with_track == n);        /* every entry identifies its track */
    ASSERT(with_ratio == n);        /* every entry has a measured length */
    ASSERT(with_half == 35);        /* the 35 half-tracks from the manifest */

    /* The 15 whole tracks are the same set the plugin path pins, so the two
     * readers agree where they overlap. The 13 half-tracks carrying custom
     * sync are data that NEITHER path could see before PROT-6 and PROT-8: the
     * plugin could not address half-tracks, and the widget's own metrics never
     * set has_custom_sync at all. */
    ASSERT(sync_whole == 15);
    ASSERT(sync_half == 13);
}

TEST(gui_path_finds_the_protection_PROT8) {
    static ufm_c64_track_metrics_t m[128];
    memset(m, 0, sizeof(m));
    ufm_c64_prot_report_t report;
    ASSERT(gui_path_analyze(img("c64pp_bountybob.g64"), &report, m, 128) > 0);

    ASSERT(report.hits_written > 0);
    ASSERT(report.primary_scheme == UFM_PROT_HALF_TRACK);
    ASSERT(report.confidence_0_100 == 85);
    /* MF-402 still holds through this path: the primary scheme names the
     * dominant structure (half-tracks), not a product. */
    ASSERT(report.primary_scheme != UFM_PROT_VMAX);
    ASSERT(report.primary_scheme != UFM_PROT_RAPIDLOK);
}

TEST(gui_path_and_plugin_path_agree_on_clean_disks_PROT8) {
    /* Both G64 readers, same disks, same answer: silence. This is what caught
     * the index-convention mismatch — unconverted, this reader reported three
     * "long track" hits on each of these two clean disks. */
    static ufm_c64_track_metrics_t m[128];
    ufm_c64_prot_report_t report;

    memset(m, 0, sizeof(m));
    ASSERT(gui_path_analyze(img("c64pp_aliensyndrome.g64"), &report, m, 128) == 40);
    ASSERT(report.hits_written == 0);

    memset(m, 0, sizeof(m));
    ASSERT(gui_path_analyze(free_img("vice_c1541_35trk.g64"), &report, m, 128) == 35);
    ASSERT(report.hits_written == 0);
}

int main(void) {
    FILE *f = fopen(img("c64pp_bountybob.g64"), "rb");
    if (!f) {
        printf("SKIP: local C64 protection corpus absent (%s/c64pp_*.g64)\n"
               "      re-fetch per tests/corpus_manifest/manifest.json\n",
               UFT_CORPUS_RESTRICTED_DIR);
        return SKIP_EXIT;
    }
    fclose(f);
    printf("=== Positive control: C64 protection vs real disks (MF-383) ===\n");
    RUN(protected_disk_flags_exactly_the_pinned_tracks);
    RUN(protected_disk_track1_is_short_and_headerless);
    RUN(protected_disk_produces_protection_hits);
    RUN(real_protected_disk_is_described_not_named_PROT5);
    RUN(real_commercial_disk_without_protection_stays_silent);
    RUN(half_tracks_are_reachable_through_the_plugin_PROT6);
    RUN(a_disk_without_half_tracks_reports_none);
    RUN(even_slots_agree_with_read_track);
    RUN(gui_path_populates_the_fields_the_classifier_reads_PROT8);
    RUN(gui_path_finds_the_protection_PROT8);
    RUN(gui_path_and_plugin_path_agree_on_clean_disks_PROT8);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

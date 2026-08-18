/**
 * @file test_c64_metrics_corpus.c
 * @brief C64 GCR metric extraction against a REAL G64 (MF-381, closes PROT-2).
 *
 * Until now `ufm_c64_prot_analyze()` had no data supplier at all: it consumes
 * `ufm_c64_track_metrics_t` per track, and nothing in the tree produced one.
 * Every C64 scheme was therefore unreachable regardless of the classifier.
 *
 * This test drives the real chain end to end:
 *   G64 plugin -> uft_track_t::raw_data (GCR) -> ufm_c64_metrics_from_gcr()
 *   -> ufm_c64_prot_analyze()
 *
 * Corpus: tests/corpus_free/vice_c1541_35trk.g64, produced by VICE 3.10 c1541
 * (`c1541 -format "uftg64,42" g64 ...` plus one marker file of original UFT
 * text) — rights-free, therefore tracked in-repo.
 *
 * Ground truth pinned by independent python inspection BEFORE any UFT run:
 *   header "GCR-1541", 84 half-track slots, 35 tracks present, 0 half-tracks,
 *   speed zones 3/2/1/0, track lengths 7692/7143/6667/6250 bytes, and per
 *   track: 21/19/18/17 sector headers, sync_count = 2x sectors, longest sync
 *   run 41 bits, 0 illegal GCR codes, 0 duplicate IDs.
 *
 * The analyze() call is a NEGATIVE control: a freshly formatted disk must
 * produce no protection hits. A positive control needs a real protected
 * original, which is still outstanding — see docs/KNOWN_ISSUES.md PROT-2.
 */

#include "uft/protection/ufm_c64_metrics.h"
#include "uft/protection/ufm_c64_scheme_detect.h"
#include "uft/protection/ufm_cbm_protection_methods.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_g64;

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static const char *img_path(void) {
    static char p[512];
    snprintf(p, sizeof(p), "%s/vice_c1541_35trk.g64", UFT_CORPUS_DIR);
    return p;
}

/** Expected sector count per 1541 speed zone (standard geometry). */
static uint32_t expected_sectors(int track) {
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;
}

TEST(zone_tables_match_1541_geometry) {
    ASSERT(ufm_c64_zone_for_track(1)  == 3);
    ASSERT(ufm_c64_zone_for_track(17) == 3);
    ASSERT(ufm_c64_zone_for_track(18) == 2);
    ASSERT(ufm_c64_zone_for_track(24) == 2);
    ASSERT(ufm_c64_zone_for_track(25) == 1);
    ASSERT(ufm_c64_zone_for_track(30) == 1);
    ASSERT(ufm_c64_zone_for_track(31) == 0);
    ASSERT(ufm_c64_zone_for_track(35) == 0);
    ASSERT(ufm_c64_zone_for_track(0)  == -1);

    /* Pinned against the lengths VICE actually wrote into the G64 */
    ASSERT(ufm_c64_zone_nominal_bytes(3) == 7692);
    ASSERT(ufm_c64_zone_nominal_bytes(2) == 7143);
    ASSERT(ufm_c64_zone_nominal_bytes(1) == 6667);
    ASSERT(ufm_c64_zone_nominal_bytes(0) == 6250);
    ASSERT(ufm_c64_zone_nominal_bytes(4) == 0);
}

TEST(track1_metrics_match_pinned_values) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_g64.open(&disk, img_path(), true) == UFT_OK);

    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_g64.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.raw_data != NULL);
    ASSERT(t.raw_size == 7692);                 /* pinned zone-3 length */

    ufm_c64_track_metrics_t m;
    ASSERT(ufm_c64_metrics_from_gcr(t.raw_data, t.raw_size, 0,
                                    UFM_C64_SPEED_ZONE_AUTO, &m));
    ASSERT(m.track == 1);
    ASSERT(m.bitcell_count == 7692u * 8u);      /* pinned 61536 bits */
    ASSERT(m.sector_count == 21);               /* pinned header count */
    ASSERT(m.sync_count == 42);                 /* header + data per sector */
    ASSERT(m.max_sync_run_bits == 41);          /* pinned longest sync run */
    ASSERT(m.bad_gcr_count == 0);
    ASSERT(m.duplicate_ids == 0);
    ASSERT(!m.is_half_track && !m.has_half_track);
    ASSERT(m.has_meaningful_data);
    ASSERT(m.track_length_ratio > 0.999f && m.track_length_ratio < 1.001f);

    free(t.raw_data);
    for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
    free(t.sectors);
    uft_format_plugin_g64.close(&disk);
}

TEST(all_35_tracks_match_standard_geometry) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_g64.open(&disk, img_path(), true) == UFT_OK);

    for (int cyl = 0; cyl < 35; cyl++) {
        uft_track_t t; memset(&t, 0, sizeof(t));
        ASSERT(uft_format_plugin_g64.read_track(&disk, cyl, 0, &t) == UFT_OK);
        ASSERT(t.raw_data != NULL && t.raw_size > 0);

        ufm_c64_track_metrics_t m;
        ASSERT(ufm_c64_metrics_from_gcr(t.raw_data, t.raw_size, cyl * 2,
                                        UFM_C64_SPEED_ZONE_AUTO, &m));
        ASSERT(m.sector_count == expected_sectors(cyl + 1));
        ASSERT(m.sync_count == m.sector_count * 2);
        ASSERT(m.bad_gcr_count == 0);
        ASSERT(m.duplicate_ids == 0);
        /* a freshly formatted disk sits within 1% of the nominal length */
        ASSERT(m.track_length_ratio > 0.99f && m.track_length_ratio < 1.01f);

        free(t.raw_data);
        for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
        free(t.sectors);
    }
    uft_format_plugin_g64.close(&disk);
}

TEST(unprotected_disk_yields_no_protection_hits) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_g64.open(&disk, img_path(), true) == UFT_OK);

    ufm_c64_track_metrics_t metrics[35];
    memset(metrics, 0, sizeof(metrics));

    for (int cyl = 0; cyl < 35; cyl++) {
        uft_track_t t; memset(&t, 0, sizeof(t));
        ASSERT(uft_format_plugin_g64.read_track(&disk, cyl, 0, &t) == UFT_OK);
        ASSERT(ufm_c64_metrics_from_gcr(t.raw_data, t.raw_size, cyl * 2,
                                        UFM_C64_SPEED_ZONE_AUTO, &metrics[cyl]));
        free(t.raw_data);
        for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
        free(t.sectors);
    }
    uft_format_plugin_g64.close(&disk);

    /* NEGATIVE control: the classifier now receives real data for the first
     * time, and must report nothing on an unprotected disk. */
    ufm_c64_prot_hit_t hits[64];
    ufm_c64_prot_report_t report;
    ASSERT(ufm_c64_prot_analyze(metrics, 35, hits, 64, &report));
    ASSERT(report.hits_written == 0);
    ASSERT(report.primary_scheme == UFM_PROT_NONE);
    ASSERT(report.confidence_0_100 == 0);
}

TEST(rejects_invalid_arguments) {
    ufm_c64_track_metrics_t m;
    uint8_t buf[16];
    memset(buf, 0xFF, sizeof(buf));
    ASSERT(!ufm_c64_metrics_from_gcr(NULL, 16, 0, 3, &m));
    ASSERT(!ufm_c64_metrics_from_gcr(buf, 0, 0, 3, &m));
    ASSERT(!ufm_c64_metrics_from_gcr(buf, 16, 0, 3, NULL));
    /* all-ones input: one long sync, no decodable block, no crash */
    ASSERT(ufm_c64_metrics_from_gcr(buf, sizeof(buf), 0, 3, &m));
    ASSERT(m.sync_count == 1);
    ASSERT(m.max_sync_run_bits == 128);
    ASSERT(m.sector_count == 0);
}

/* The anomaly counters are what protection detection actually rests on, and on
 * a clean disk they are all zero — verified only at zero, they would be
 * untested code. These two cases drive them off zero deterministically. */
TEST(illegal_gcr_is_counted) {
    /* 16 sync bits, then zero bytes: 0b00000 is not in the 16-code GCR
     * alphabet, so the block decode must stop and report one illegal code. */
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    buf[0] = 0xFF; buf[1] = 0xFF;

    ufm_c64_track_metrics_t m;
    ASSERT(ufm_c64_metrics_from_gcr(buf, sizeof(buf), 0, 3, &m));
    ASSERT(m.sync_count == 1);
    ASSERT(m.max_sync_run_bits == 16);
    ASSERT(m.bad_gcr_count == 1);
    ASSERT(m.illegal_gcr_events == 1);
    ASSERT(m.sector_count == 0);
}

TEST(duplicate_header_ids_are_counted) {
    /* Two identical headers built from real GCR: take track 1 of the corpus
     * image, keep only its first two sync+header blocks, and make the second
     * header a byte-identical copy of the first. Everything except the
     * duplication stays genuine 1541 GCR. */
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_g64.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_g64.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.raw_data != NULL && t.raw_size > 512);

    /* A sector's sync+header occupies well under 256 bytes; duplicating the
     * first 256 bytes therefore duplicates the first header verbatim. */
    const size_t chunk = 256;
    uint8_t *dup = malloc(chunk * 2);
    ASSERT(dup != NULL);
    memcpy(dup, t.raw_data, chunk);
    memcpy(dup + chunk, t.raw_data, chunk);

    ufm_c64_track_metrics_t m;
    ASSERT(ufm_c64_metrics_from_gcr(dup, chunk * 2, 0, 3, &m));
    ASSERT(m.sector_count == 2);          /* same header seen twice */
    ASSERT(m.duplicate_ids == 1);         /* and reported as duplicate */

    free(dup);
    free(t.raw_data);
    for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
    free(t.sectors);
    uft_format_plugin_g64.close(&disk);
}

/* has_custom_sync (MF-382). Definition grounded in nibtools
 * (rittwage/nibtools @0abdc11): sync present, but no standard 1541 header
 * block behind any of them — nibtools' "track w/non-standard headers". Killer
 * tracks (BM_FF_TRACK) are a separate category there and here. */
TEST(standard_tracks_never_report_custom_sync) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_g64.open(&disk, img_path(), true) == UFT_OK);

    for (int cyl = 0; cyl < 35; cyl++) {
        uft_track_t t; memset(&t, 0, sizeof(t));
        ASSERT(uft_format_plugin_g64.read_track(&disk, cyl, 0, &t) == UFT_OK);
        ufm_c64_track_metrics_t m;
        ASSERT(ufm_c64_metrics_from_gcr(t.raw_data, t.raw_size, cyl * 2,
                                        UFM_C64_SPEED_ZONE_AUTO, &m));
        ASSERT(!m.has_custom_sync);        /* 21..17 headers => standard */
        free(t.raw_data);
        for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
        free(t.sectors);
    }
    uft_format_plugin_g64.close(&disk);
}

TEST(headerless_real_gcr_reports_custom_sync) {
    /* Real GCR, not synthetic: bytes [26,326) of corpus track 1 hold one sync
     * plus the following DATA block (id 0x07) and no header block (id 0x08).
     * Pinned by python before the C path was written: sync_count 1,
     * max_sync_run 24 bits, 0 headers, 0 illegal codes. */
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_g64.open(&disk, img_path(), true) == UFT_OK);
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_g64.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.raw_data != NULL && t.raw_size > 326);

    ufm_c64_track_metrics_t m;
    ASSERT(ufm_c64_metrics_from_gcr(t.raw_data + 26, 300, 0, 3, &m));
    ASSERT(m.sync_count == 1);
    ASSERT(m.max_sync_run_bits == 24);
    ASSERT(m.sector_count == 0);           /* no standard header */
    ASSERT(m.bad_gcr_count == 0);
    ASSERT(m.has_custom_sync);             /* <- the condition under test */

    free(t.raw_data);
    for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
    free(t.sectors);
    uft_format_plugin_g64.close(&disk);
}

TEST(killer_track_is_not_reported_as_custom_sync) {
    /* All-$FF track = nibtools BM_FF_TRACK. It has sync and no headers, but is
     * its own category; folding it into has_custom_sync would make every
     * killer track look like V-MAX! to the classifier. */
    uint8_t buf[512];
    memset(buf, 0xFF, sizeof(buf));
    ufm_c64_track_metrics_t m;
    ASSERT(ufm_c64_metrics_from_gcr(buf, sizeof(buf), 0, 3, &m));
    ASSERT(m.sync_count == 1);
    ASSERT(m.max_sync_run_bits == sizeof(buf) * 8);
    ASSERT(m.sector_count == 0);
    ASSERT(!m.has_custom_sync);
}

/* Regression guard for the FIX of PROT-5 (MF-402).
 *
 * This test used to pin the defect: a header-less track made the classifier
 * emit "V-MAX!" at 85 % confidence, because ufm_cbm_check_vmax() reduced to
 * ufm_cbm_check_custom_sync() — has_custom_sync implies sector_count == 0, so
 * its "sector count is non-standard" half was tautologically true.
 *
 * The check is gone. What must hold now is the principle behind the fix: this
 * module reports the structure it measured, and never a product name. */
TEST(structural_analysis_emits_no_product_names) {
    ufm_c64_track_metrics_t m;
    memset(&m, 0, sizeof(m));
    m.track = 20;
    m.has_custom_sync = true;
    m.sector_count = 0;
    m.track_length_ratio = 1.0f;

    ASSERT(ufm_cbm_check_custom_sync(&m));

    ufm_c64_prot_hit_t hits[16];
    ufm_c64_prot_report_t report;
    ASSERT(ufm_c64_prot_analyze(&m, 1, hits, 16, &report));

    bool saw_custom = false;
    for (uint32_t i = 0; i < report.hits_written; i++) {
        if (hits[i].type == UFM_PROT_CUSTOM_SYNC) saw_custom = true;
        /* No named scheme may be derived from generic structure. */
        ASSERT(hits[i].type != UFM_PROT_VMAX);
        ASSERT(hits[i].type != UFM_PROT_RAPIDLOK);
        ASSERT(hits[i].type != UFM_PROT_COPYLOCK);
        ASSERT(hits[i].type != UFM_PROT_SPEEDLOCK);
        ASSERT(hits[i].type != UFM_PROT_VORPAL);
    }
    ASSERT(saw_custom);
    /* A single custom-sync track is not enough to call the dominant structure
     * (the chain needs > 5), so the honest answer is UNKNOWN — a hit was found
     * but nothing characterises the disk yet. It used to be "V-MAX!". */
    ASSERT(report.primary_scheme == UFM_PROT_UNKNOWN);
}

/* A half-track beyond track 35 is a sharper structural fact, but still not an
 * identification. It used to produce a second hit labelled "RapidLok". */
TEST(half_track_beyond_35_is_reported_structurally) {
    ufm_c64_track_metrics_t m;
    memset(&m, 0, sizeof(m));
    m.track = 38;
    m.has_half_track = true;
    m.track_length_ratio = 1.0f;

    ASSERT(ufm_cbm_has_half_track_beyond_35(&m));

    ufm_c64_prot_hit_t hits[16];
    ufm_c64_prot_report_t report;
    ASSERT(ufm_c64_prot_analyze(&m, 1, hits, 16, &report));

    int half_hits = 0;
    for (uint32_t i = 0; i < report.hits_written; i++) {
        if (hits[i].type == UFM_PROT_HALF_TRACK) half_hits++;
        ASSERT(hits[i].type != UFM_PROT_RAPIDLOK);
    }
    ASSERT(half_hits == 1);   /* one observation, one hit */
}

int main(void) {
    printf("=== C64 GCR metrics vs real VICE G64 (MF-381, PROT-2) ===\n");
    RUN(zone_tables_match_1541_geometry);
    RUN(track1_metrics_match_pinned_values);
    RUN(all_35_tracks_match_standard_geometry);
    RUN(unprotected_disk_yields_no_protection_hits);
    RUN(rejects_invalid_arguments);
    RUN(illegal_gcr_is_counted);
    RUN(duplicate_header_ids_are_counted);
    RUN(standard_tracks_never_report_custom_sync);
    RUN(headerless_real_gcr_reports_custom_sync);
    RUN(killer_track_is_not_reported_as_custom_sync);
    RUN(structural_analysis_emits_no_product_names);
    RUN(half_track_beyond_35_is_reported_structurally);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

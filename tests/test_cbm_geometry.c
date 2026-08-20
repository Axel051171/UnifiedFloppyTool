/**
 * @file test_cbm_geometry.c
 * @brief The one CBM geometry must reproduce all 24 tables it replaces (MF-434).
 *
 * Consolidating a duplicated fact is only safe if the replacement is provably
 * the same fact. So this does not test the new accessors against a fresh copy
 * of the numbers — that would just be a 25th table. It holds them against the
 * three shapes the tree actually used, and against the reference images.
 *
 * The three indexing conventions found in 23 files:
 *   len 43: 1-based, leading 0, tracks 1..42   (8 occurrences)
 *   len 41: 1-based, leading 0, tracks 1..40   (6 occurrences)
 *   len 40: 0-based, tracks 1..40              (3 occurrences)
 * plus D71 at 70/71 entries and D67 at 35. The values agreed everywhere; the
 * conventions did not, which is the actual hazard being removed.
 *
 * The corpus assertions matter more than the table ones: they tie the numbers
 * to disks that VICE c1541 produced, not to what UFT already believed.
 */

#include "uft/formats/cbm/uft_cbm_geometry.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-48s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* Verbatim copies of the tables this replaces, so the comparison is against
 * what the tree really contained and not against a paraphrase. */

/* uft_d64_g64.c::sector_map, d64_plugin.c::d64_spt, gcr_ops.c::sector_map,
 * nib_format.c, d64_file.c, g64_parser_v3.c, track_align.c, bam_editor.h */
static const int legacy_43[43] = {
    0,
    21,21,21,21,21,21,21,21,21,21, 21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17,17,17,17,17,17,17,17
};

/* commodore/d64.c::spt, d64_parser_v2/v3, uft_d64_writer.c,
 * uft_c64_protection.h, uft_cbm_gcr.h */
static const int legacy_41[41] = {
    0,
    21,21,21,21,21,21,21,21,21,21, 21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17,17,17,17,17,17
};

/* flux_decoder.c, cbm_formats.c, xdf_api_impl.c — 0-based, track N at [N-1] */
static const int legacy_40[40] = {
    21,21,21,21,21,21,21,21,21,21, 21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17,17,17,17,17,17
};

/* commodore/d67.c::spt — 0-based, 20 sectors in zone 2 */
static const int legacy_d67_35[35] = {
    21,21,21,21,21,21,21,21,21,21, 21,21,21,21,21,21,21,
    20,20,20,20,20,20,20,
    18,18,18,18,18,18,
    17,17,17,17,17
};

/* uft_d64_g64.c::speed_map (1-based, leading 0) */
static const int legacy_speed_43[43] = {
    0,
    3,3,3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,
    2,2,2,2,2,2,2,
    1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0
};

/* uft_d64_g64.c::gap_map (1-based, leading 0) */
static const int legacy_gap_43[43] = {
    0,
    9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,
    19,19,19,19,19,19,19,
    13,13,13,13,13,13,
    10,10,10,10,10,10,10,10,10,10,10,10
};

TEST(reproduces_the_1_based_43_entry_table) {
    for (int t = 1; t <= 42; t++)
        ASSERT(uft_cbm_sectors_per_track(UFT_CBM_1541, t) == legacy_43[t]);
    /* and refuses what the table padded with zero */
    ASSERT(uft_cbm_sectors_per_track(UFT_CBM_1541, 0) == 0);
    ASSERT(uft_cbm_sectors_per_track(UFT_CBM_1541, 43) == 0);
}

TEST(reproduces_the_1_based_41_entry_table) {
    for (int t = 1; t <= 40; t++)
        ASSERT(uft_cbm_sectors_per_track(UFT_CBM_1541, t) == legacy_41[t]);
}

TEST(reproduces_the_0_based_40_entry_table) {
    /* The convention that made this worth removing: same numbers, shifted. */
    for (int t = 1; t <= 40; t++)
        ASSERT(uft_cbm_sectors_per_track(UFT_CBM_1541, t) == legacy_40[t - 1]);
}

TEST(reproduces_the_2040_table_with_its_20_sector_zone) {
    for (int t = 1; t <= 35; t++)
        ASSERT(uft_cbm_sectors_per_track(UFT_CBM_2040, t) == legacy_d67_35[t - 1]);
    /* the difference that defines the family */
    ASSERT(uft_cbm_sectors_per_track(UFT_CBM_2040, 18) == 20);
    ASSERT(uft_cbm_sectors_per_track(UFT_CBM_1541, 18) == 19);
    /* and a 2040 has no track 36 */
    ASSERT(uft_cbm_sectors_per_track(UFT_CBM_2040, 36) == 0);
}

TEST(reproduces_the_speed_and_gap_tables) {
    for (int t = 1; t <= 42; t++) {
        ASSERT(uft_cbm_speed_zone(UFT_CBM_1541, t) == legacy_speed_43[t]);
        ASSERT(uft_cbm_gap_length(UFT_CBM_1541, t) == legacy_gap_43[t]);
    }
    /* capacity_map[4] = { 6250, 6666, 7142, 7692 }, indexed by speed */
    ASSERT(uft_cbm_track_capacity(UFT_CBM_1541, 1)  == 7692);  /* zone 3 */
    ASSERT(uft_cbm_track_capacity(UFT_CBM_1541, 18) == 7142);  /* zone 2 */
    ASSERT(uft_cbm_track_capacity(UFT_CBM_1541, 25) == 6666);  /* zone 1 */
    ASSERT(uft_cbm_track_capacity(UFT_CBM_1541, 31) == 6250);  /* zone 0 */
}

TEST(the_2040_gap_is_reported_as_unknown_not_guessed) {
    /* A 20-sector zone-2 track has less room per sector than a 19-sector one,
     * so the 1541 gap of 19 cannot simply carry over, and no authoritative
     * source was found. Reporting 0 is the honest answer; a plausible number
     * here would be exactly the kind of invention the project forbids. */
    ASSERT(uft_cbm_gap_length(UFT_CBM_2040, 18) == 0);
    /* the zones that DO match the 1541 keep their value */
    ASSERT(uft_cbm_gap_length(UFT_CBM_2040, 1) == 9);
    ASSERT(uft_cbm_gap_length(UFT_CBM_2040, 25) == 13);
}

TEST(block_offsets_match_the_cumulative_table) {
    /* uft_d64_g64.c::block_offset — 0, 0, 21, 42, ... */
    ASSERT(uft_cbm_block_offset(UFT_CBM_1541, 1)  == 0);
    ASSERT(uft_cbm_block_offset(UFT_CBM_1541, 2)  == 21);
    ASSERT(uft_cbm_block_offset(UFT_CBM_1541, 18) == 357);
    ASSERT(uft_cbm_block_offset(UFT_CBM_1541, 19) == 376);
    ASSERT(uft_cbm_block_offset(UFT_CBM_1541, 0)  == -1);
}

TEST(totals_match_the_disk_sizes_the_world_uses) {
    ASSERT(uft_cbm_total_blocks(UFT_CBM_1541, 35) == 683);   /* 174848 bytes */
    ASSERT(uft_cbm_total_blocks(UFT_CBM_1541, 40) == 768);   /* 196608 bytes */
    ASSERT(uft_cbm_total_blocks(UFT_CBM_2040, 35) == 690);   /* 176640 bytes */
    ASSERT(uft_cbm_total_blocks(UFT_CBM_1581, 80) == 3200);  /* 819200 bytes */
    /* 1571 = 1541 on two heads */
    ASSERT(uft_cbm_total_blocks(UFT_CBM_1571, 35) * uft_cbm_heads(UFT_CBM_1571)
           == 1366);                                          /* 349696 bytes */
}

TEST(the_1581_reports_no_gcr_zones_rather_than_zero) {
    /* A 1581 is MFM. Speed zones and GCR gaps do not exist for it, and -1
     * says that, where 0 would read as "zone 0". */
    ASSERT(uft_cbm_sectors_per_track(UFT_CBM_1581, 1)  == 40);
    ASSERT(uft_cbm_sectors_per_track(UFT_CBM_1581, 80) == 40);
    ASSERT(uft_cbm_speed_zone(UFT_CBM_1581, 1) == -1);
    ASSERT(uft_cbm_track_capacity(UFT_CBM_1581, 1) == 0);
}

/* --- against the reference images, not against what UFT already believed --- */

static long file_size(const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", UFT_CORPUS_DIR, name);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

TEST(the_geometry_predicts_the_c1541_image_sizes) {
    /* Each of these was produced by VICE c1541, outside UFT. If the geometry
     * were wrong the arithmetic would not land on the byte count. */
    struct { const char *file; uft_cbm_family_t fam; int tracks; int heads; }
    cases[] = {
        { "vice_c1541_35trk.d64", UFT_CBM_1541, 35, 1 },
        { "vice_c1541_2040.d67",  UFT_CBM_2040, 35, 1 },
        { "vice_c1541_70trk.d71", UFT_CBM_1571, 35, 2 },
        { "vice_c1541_80trk.d81", UFT_CBM_1581, 80, 1 },  /* d81 counts 80 tr */
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        long sz = file_size(cases[i].file);
        ASSERT(sz > 0);
        long expect = (long)uft_cbm_total_blocks(cases[i].fam, cases[i].tracks)
                    * cases[i].heads * 256;
        if (expect != sz) {
            printf("\n        %s: geometry says %ld, file is %ld\n",
                   cases[i].file, expect, sz);
        }
        ASSERT(expect == sz);
    }
}

int main(void)
{
    printf("=== One CBM geometry replacing 24 tables (MF-434) ===\n");
    RUN(reproduces_the_1_based_43_entry_table);
    RUN(reproduces_the_1_based_41_entry_table);
    RUN(reproduces_the_0_based_40_entry_table);
    RUN(reproduces_the_2040_table_with_its_20_sector_zone);
    RUN(reproduces_the_speed_and_gap_tables);
    RUN(the_2040_gap_is_reported_as_unknown_not_guessed);
    RUN(block_offsets_match_the_cumulative_table);
    RUN(totals_match_the_disk_sizes_the_world_uses);
    RUN(the_1581_reports_no_gcr_zones_rather_than_zero);
    RUN(the_geometry_predicts_the_c1541_image_sizes);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

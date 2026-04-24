/**
 * @file test_img_plugin.c
 * @brief IMG (PC Raw Floppy) Plugin — probe tests.
 *
 * Self-contained per template from test_d64_plugin.c.
 * Mirrors src/formats/img/uft_img.c size-based geometry lookup.
 *
 * IMG is a raw, no-header sector-by-sector floppy image. Probe is
 * size-based against the known_geometries table. Accepted shapes:
 *
 *   320KB  5.25" DS/DD   327680
 *   360KB  5.25" DS/DD   368640
 *   720KB  3.5"  DS/DD   737280
 *   1.2MB  5.25" DS/HD  1228800
 *   1.44MB 3.5"  DS/HD  1474560
 *   1.68MB 3.5"  DMF    1720320
 *   2.88MB 3.5"  DS/ED  2949120
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-28s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

typedef struct {
    uint32_t size;
    uint8_t  cyls;
    uint8_t  heads;
    uint8_t  sectors_per_track;
} img_geom_t;

static const img_geom_t IMG_GEOMETRIES[] = {
    {  327680u, 40, 2,  8 },
    {  368640u, 40, 2,  9 },
    {  737280u, 80, 2,  9 },
    { 1228800u, 80, 2, 15 },
    { 1474560u, 80, 2, 18 },
    { 1720320u, 80, 2, 21 },
    { 2949120u, 80, 2, 36 },
    { 0, 0, 0, 0 }
};

static const img_geom_t *img_probe_replica(size_t file_size) {
    for (int i = 0; IMG_GEOMETRIES[i].size != 0; i++) {
        if ((uint32_t)file_size == IMG_GEOMETRIES[i].size) {
            return &IMG_GEOMETRIES[i];
        }
    }
    return NULL;
}

TEST(probe_360k) {
    const img_geom_t *g = img_probe_replica(368640u);
    ASSERT(g != NULL);
    ASSERT(g->cyls == 40);
    ASSERT(g->sectors_per_track == 9);
}

TEST(probe_720k) {
    const img_geom_t *g = img_probe_replica(737280u);
    ASSERT(g != NULL);
    ASSERT(g->cyls == 80);
    ASSERT(g->sectors_per_track == 9);
}

TEST(probe_1440k) {
    const img_geom_t *g = img_probe_replica(1474560u);
    ASSERT(g != NULL);
    ASSERT(g->cyls == 80);
    ASSERT(g->sectors_per_track == 18);
}

TEST(probe_2880k) {
    const img_geom_t *g = img_probe_replica(2949120u);
    ASSERT(g != NULL);
    ASSERT(g->sectors_per_track == 36);
}

TEST(probe_dmf_168k) {
    const img_geom_t *g = img_probe_replica(1720320u);
    ASSERT(g != NULL);
    ASSERT(g->sectors_per_track == 21);
}

TEST(probe_all_geometries_consistent) {
    /* cyls × heads × spt × 512 must equal size */
    for (int i = 0; IMG_GEOMETRIES[i].size != 0; i++) {
        uint32_t computed = (uint32_t)IMG_GEOMETRIES[i].cyls *
                             IMG_GEOMETRIES[i].heads *
                             IMG_GEOMETRIES[i].sectors_per_track * 512u;
        ASSERT(computed == IMG_GEOMETRIES[i].size);
    }
}

TEST(probe_rejects_zero) {
    ASSERT(img_probe_replica(0) == NULL);
}

TEST(probe_rejects_d64_size) {
    /* D64 (174848) must not collide with IMG table */
    ASSERT(img_probe_replica(174848u) == NULL);
}

TEST(probe_rejects_adf_size) {
    ASSERT(img_probe_replica(901120u) == NULL);
}

TEST(probe_rejects_off_by_one) {
    ASSERT(img_probe_replica(1474560u - 1) == NULL);
    ASSERT(img_probe_replica(1474560u + 1) == NULL);
}

int main(void) {
    printf("=== IMG plugin probe tests ===\n");
    RUN(probe_360k);
    RUN(probe_720k);
    RUN(probe_1440k);
    RUN(probe_2880k);
    RUN(probe_dmf_168k);
    RUN(probe_all_geometries_consistent);
    RUN(probe_rejects_zero);
    RUN(probe_rejects_d64_size);
    RUN(probe_rejects_adf_size);
    RUN(probe_rejects_off_by_one);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}

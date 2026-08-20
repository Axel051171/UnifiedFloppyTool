/**
 * @file uft_cbm_geometry.c
 * @brief The Commodore drive geometries, in one place (MF-434).
 *
 * See uft_cbm_geometry.h for why this exists. Everything below is expressed
 * as zone boundaries rather than per-track arrays, because the zones are the
 * actual fact — the arrays were always just that fact written out 35, 40, 42
 * or 70 times, which is how the tree ended up with three incompatible
 * indexing conventions for it.
 */

#include "uft/formats/cbm/uft_cbm_geometry.h"

/* One zone: tracks [first..last] carry `sectors` sectors at `speed`. */
typedef struct {
    uint8_t first;
    uint8_t last;
    uint8_t sectors;
    uint8_t speed;
    uint16_t gap;
} cbm_zone_t;

/* 1541 / 1540 / 2031 — CBM DOS 2.6.
 *
 * Tracks 36-42 are the "extended" range: no stock DOS uses them, plenty of
 * copy-protected and 40-track-formatted disks do. They continue zone 0.
 * Gap lengths from the same source as the rest of the encoder they replace
 * (uft_d64_g64.c), which produces byte-identical G64 output against the
 * c1541 reference image. */
static const cbm_zone_t zones_1541[] = {
    {  1, 17, 21, 3,  9 },
    { 18, 24, 19, 2, 19 },
    { 25, 30, 18, 1, 13 },
    { 31, 42, 17, 0, 10 },
};

/* 2040 / 4040 — CBM DOS 1.
 *
 * Same mechanics, same speed zones, but DOS 1 packs 20 sectors into zone 2
 * where 2.6 uses 19 — 690 blocks against 683 over 35 tracks. Verified against
 * tests/corpus_free/vice_c1541_2040.d67 (test_corpus_cbm_vice).
 *
 * Gap: NOT established for this family. A 20-sector zone-2 track has less
 * room per sector than a 19-sector one, so the 1541 value of 19 cannot simply
 * be reused, and no authoritative source for the 2040 value was found. The
 * accessor returns 0 there rather than a plausible-looking guess — see the
 * project's first principle. */
static const cbm_zone_t zones_2040[] = {
    {  1, 17, 21, 3,  9 },
    { 18, 24, 20, 2,  0 },
    { 25, 30, 18, 1, 13 },
    { 31, 35, 17, 0, 10 },
};

/* 1581 — CBM DOS 3.0. MFM, not GCR, and not zoned: 40 sectors on every one of
 * the 80 tracks, both sides, 3200 blocks. Speed zones and GCR gaps do not
 * apply; the accessors report that rather than inventing values. */
static const cbm_zone_t zones_1581[] = {
    {  1, 80, 40, 0, 0 },
};

/* GCR bytes per track by speed zone. Physical property of the drive: the
 * spindle turns at a constant 300 rpm while the bit clock changes per zone. */
static const size_t capacity_by_speed[4] = { 6250, 6666, 7142, 7692 };

typedef struct {
    const cbm_zone_t *zones;
    size_t zone_count;
    uint8_t max_track;
    uint8_t heads;
    uint8_t has_speed_zones;   /* 0 for the 1581: MFM, no GCR zones */
} cbm_family_t;

static const cbm_family_t families[UFT_CBM_FAMILY_COUNT] = {
    [UFT_CBM_1541] = { zones_1541, sizeof(zones_1541) / sizeof(zones_1541[0]),
                       42, 1, 1 },
    [UFT_CBM_2040] = { zones_2040, sizeof(zones_2040) / sizeof(zones_2040[0]),
                       35, 1, 1 },
    /* A 1571 is a 1541 with a second head: identical per-side layout. */
    [UFT_CBM_1571] = { zones_1541, sizeof(zones_1541) / sizeof(zones_1541[0]),
                       42, 2, 1 },
    [UFT_CBM_1581] = { zones_1581, sizeof(zones_1581) / sizeof(zones_1581[0]),
                       80, 2, 0 },
};

static const cbm_family_t *fam(uft_cbm_family_t family)
{
    if (family < 0 || family >= UFT_CBM_FAMILY_COUNT) return NULL;
    return &families[family];
}

static const cbm_zone_t *zone_of(const cbm_family_t *f, int track)
{
    if (!f || track < 1 || track > f->max_track) return NULL;
    for (size_t i = 0; i < f->zone_count; i++) {
        if (track >= f->zones[i].first && track <= f->zones[i].last)
            return &f->zones[i];
    }
    return NULL;
}

int uft_cbm_max_track(uft_cbm_family_t family)
{
    const cbm_family_t *f = fam(family);
    return f ? f->max_track : 0;
}

int uft_cbm_heads(uft_cbm_family_t family)
{
    const cbm_family_t *f = fam(family);
    return f ? f->heads : 0;
}

int uft_cbm_sectors_per_track(uft_cbm_family_t family, int track)
{
    const cbm_zone_t *z = zone_of(fam(family), track);
    return z ? z->sectors : 0;
}

int uft_cbm_speed_zone(uft_cbm_family_t family, int track)
{
    const cbm_family_t *f = fam(family);
    if (!f || !f->has_speed_zones) return -1;
    const cbm_zone_t *z = zone_of(f, track);
    return z ? z->speed : -1;
}

size_t uft_cbm_track_capacity(uft_cbm_family_t family, int track)
{
    int speed = uft_cbm_speed_zone(family, track);
    if (speed < 0 || speed > 3) return 0;
    return capacity_by_speed[speed];
}

int uft_cbm_gap_length(uft_cbm_family_t family, int track)
{
    const cbm_zone_t *z = zone_of(fam(family), track);
    return z ? z->gap : 0;
}

int uft_cbm_block_offset(uft_cbm_family_t family, int track)
{
    const cbm_family_t *f = fam(family);
    if (!f || track < 1 || track > f->max_track) return -1;
    int blocks = 0;
    for (int t = 1; t < track; t++) {
        const cbm_zone_t *z = zone_of(f, t);
        if (z) blocks += z->sectors;
    }
    return blocks;
}

int uft_cbm_total_blocks(uft_cbm_family_t family, int track_count)
{
    const cbm_family_t *f = fam(family);
    if (!f || track_count < 1 || track_count > f->max_track) return 0;
    int blocks = 0;
    for (int t = 1; t <= track_count; t++) {
        const cbm_zone_t *z = zone_of(f, t);
        if (z) blocks += z->sectors;
    }
    return blocks;
}

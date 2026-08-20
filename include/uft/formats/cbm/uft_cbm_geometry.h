/**
 * @file uft_cbm_geometry.h
 * @brief The Commodore drive geometries, in one place (MF-434).
 *
 * Sectors per track, speed zone, gap length and track capacity are properties
 * of a DRIVE, not of a file format. A 1541 puts 21 sectors on track 1 whether
 * you store the result as D64, G64, NIB or nothing at all.
 *
 * Before this header the tree carried **24 copies** of that zone table across
 * 23 files, in three different indexing conventions: 1-based with a leading
 * zero and 43 entries (8×), the same padded to 41 (6×), and 0-based with 40
 * (3×), plus D71 variants at 70, 71, 35 and 36 entries. Measured, and worth
 * stating plainly: the VALUES agreed everywhere. This is not a bug being
 * fixed, it is a fact being given one home before the next off-by-one comes
 * from someone reading a 0-based table as 1-based.
 *
 * Two tables in the tree legitimately differ and are represented as their own
 * families rather than folded in:
 *   - src/formats/commodore/d67.c — 20 sectors in zone 2, not 19. That is what
 *     makes a 2040/4040 disk a 2040 disk (690 blocks against 683), verified
 *     against tests/corpus_free/vice_c1541_2040.d67.
 *   - src/formats/lisa/uft_lisa_twiggy.c — an Apple drive, unrelated.
 *
 * Track numbers are 1-BASED throughout, the way the domain and every drive
 * manual counts them. There is deliberately no array to index: an accessor
 * cannot be read with the wrong convention.
 *
 * Verified by tests/test_cbm_geometry.c against every table it replaces and
 * against the c1541 reference images (D64/D67/D71/D81, T1b).
 */

#ifndef UFT_CBM_GEOMETRY_H
#define UFT_CBM_GEOMETRY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Which Commodore drive laid the disk out. */
typedef enum uft_cbm_family {
    UFT_CBM_1541 = 0,   /**< 1541/1540/2031 — D64, G64, NIB. 683 blocks/35 tr */
    UFT_CBM_2040,       /**< 2040/4040 DOS 1 — D67. 690 blocks/35 tr */
    UFT_CBM_1571,       /**< 1571 — D71, G71. 1541 layout on both sides */
    UFT_CBM_1581,       /**< 1581 — D81. Not zoned: 40 sectors everywhere */
    UFT_CBM_FAMILY_COUNT
} uft_cbm_family_t;

/** Highest track number this family can address (extended tracks included). */
int uft_cbm_max_track(uft_cbm_family_t family);

/** Number of heads the drive has (1571 is the double-sided one). */
int uft_cbm_heads(uft_cbm_family_t family);

/**
 * @brief Sectors on a track.
 * @param track 1-based track number
 * @return sector count, or 0 if the track is out of range for this family
 */
int uft_cbm_sectors_per_track(uft_cbm_family_t family, int track);

/**
 * @brief Speed zone of a track (0-3, 3 = fastest/outermost).
 *
 * The zone is a property of the drive mechanics, so a 2040 shares the 1541
 * zones even though it packs an extra sector into zone 2.
 * @return zone, or -1 if the track is out of range
 */
int uft_cbm_speed_zone(uft_cbm_family_t family, int track);

/**
 * @brief Raw GCR bytes a track holds at its speed zone.
 * @return capacity in bytes, or 0 if the track is out of range
 */
size_t uft_cbm_track_capacity(uft_cbm_family_t family, int track);

/**
 * @brief Default inter-sector gap length in GCR bytes.
 * @return gap length, or 0 if the track is out of range or the value is not
 *         established for this family (see the note on 2040 in the .c)
 */
int uft_cbm_gap_length(uft_cbm_family_t family, int track);

/**
 * @brief Blocks on all tracks BEFORE this one — the start block of the track.
 * @param track 1-based track number
 * @return block index, or -1 if the track is out of range
 */
int uft_cbm_block_offset(uft_cbm_family_t family, int track);

/**
 * @brief Total blocks across tracks 1..track_count.
 * @return block count, or 0 if track_count is out of range
 */
int uft_cbm_total_blocks(uft_cbm_family_t family, int track_count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CBM_GEOMETRY_H */

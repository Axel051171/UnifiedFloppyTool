/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_dos33.h
 * @brief Apple DOS 3.3 catalog reader (VTOC, catalog chain) — P3-384
 *
 * ── Why ──────────────────────────────────────────────────────────────────
 *
 * UFT reads six Apple containers (`do` on T2, `po`, `d13`, `nib`, `woz`,
 * `2img`) and until P3-384 could not tell a person which FILES are on a
 * DOS 3.3 disk. `src/fs/` had no Apple file system at all
 * (`docs/VERIFICATION_TIERS_FS.md`: zero hits for dos33/apple).
 *
 * This module reads the catalog, not the files — like `uft_cbmdos.h`,
 * "what is on it" comes first and can be answered without following a
 * single track/sector list.
 *
 * ── Reference ────────────────────────────────────────────────────────────
 *
 * Layout as described in Worth/Lechner, *Beneath Apple DOS* (Quality
 * Software 1981), the book the tree already cites for the 6-and-2
 * encoding. The book text was NOT at hand when this was written; every
 * offset below is instead checked against a2tools' code at the lines
 * named further down, and the catalog is checked against its output:
 *
 *   VTOC, track 17 sector 0
 *     +0x01/0x02  first catalog track/sector
 *     +0x03       DOS release (3)
 *     +0x06       volume number
 *     +0x27       track/sector pairs per T/S list (0x7A = 122)
 *     +0x34/0x35  tracks per disk (35), sectors per track (16)
 *     +0x36/0x37  bytes per sector, little endian (256)
 *     +0x38+4*t   free map of track t: byte 0 = sectors 15..8,
 *                 byte 1 = sectors 7..0, bit set = free
 *   catalog sector
 *     +0x01/0x02  next catalog track/sector (0/0 = end)
 *     +0x0B       7 entries of 35 bytes:
 *                 +0x00 T/S list track (0x00 never used, 0xFF deleted)
 *                 +0x01 T/S list sector
 *                 +0x02 file type, bit 7 = locked
 *                 +0x03 name, 30 bytes, high-bit ASCII, space padded
 *                 +0x21 length in sectors, little endian
 *
 * Checked against the independent program `a2tools` (catseye/a2tools
 * @ 52ad81cc, GPL-2.0-or-later, Terry Kyriacopoulos), used as PRODUCER and
 * reader, not ported: its VTOC plausibility check (a2tools.c:199-203), its
 * free count (a2tools.c:505-514) and its catalog walk (a2tools.c:239-294)
 * agree with the table above. The expectations in
 * `tests/test_dos33_katalog_gegen_a2tools.c` are a2tools' own `dir`
 * output, not read off the image.
 *
 * ── Where this reader and a2tools differ, on purpose ─────────────────────
 *
 * - a2tools treats an entry as present when name byte 0 is non-zero
 *   (a2tools.c:273). This reader treats it as present when the track byte
 *   is neither 0x00 nor 0xFF — the table above. A never-used entry is
 *   skipped and ends nothing: every sector of the chain is visited. What
 *   DOS 3.3's own CATALOG does with a never-used entry in mid-sector is
 *   NOT measured here.
 * - A deleted entry (track byte 0xFF) is KEPT and marked `deleted`, like
 *   `uft_cbmdos_entry_t::deleted` (MF-909). DOS 3.3 moves the original
 *   T/S-list track into name byte 30; a2tools' `del` does not. The reader
 *   therefore reports that track only when it is a valid track number.
 *
 * ── What this module does NOT do ─────────────────────────────────────────
 *
 * It reads no file contents, follows no T/S list, writes nothing, and
 * rejects DOS 3.2 (13 sectors) and every geometry other than 35 x 16 x 256.
 */

#ifndef UFT_FS_DOS33_H
#define UFT_FS_DOS33_H

#include "uft/uft_error.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Size of a DOS-ordered 35-track, 16-sector image. */
#define UFT_DOS33_IMAGE_SIZE 143360u

/** One catalog entry. */
typedef struct {
    char     name[31];       /**< high bit stripped, trailing spaces cut */
    uint8_t  type;           /**< type byte without the lock bit */
    char     type_char;      /**< 'T','I','A','B','S','R','X','Y' or '?' —
                                  a2tools' letters (a2tools.c:277-287);
                                  X/Y are the "new A"/"new B" types */
    bool     locked;         /**< bit 7 of the type byte */
    bool     deleted;        /**< track byte 0xFF */
    uint16_t sectors;        /**< length in sectors as the entry says */
    uint8_t  ts_track;       /**< first T/S list; for a deleted entry the
                                  track DOS saved in name byte 30, or 0xFF
                                  when that byte is no valid track */
    uint8_t  ts_sector;
} uft_dos33_entry_t;

/** A read catalog. `entries` belongs to the caller until uft_dos33_free. */
typedef struct {
    uint8_t            volume;
    uint8_t            dos_release;
    unsigned           free_sectors;   /**< set bits of the VTOC free map */
    unsigned           catalog_sectors;/**< sectors visited in the chain */
    uft_dos33_entry_t *entries;
    int                entry_count;    /**< including deleted ones */
    int                deleted_count;
} uft_dos33_catalog_t;

/**
 * Reads the catalog of a DOS-ordered image held in memory.
 *
 * @return UFT_OK; UFT_ERR_INVALID_ARG; UFT_ERR_FORMAT when the
 *         buffer is not 143360 bytes, the VTOC fails the plausibility
 *         check, or the catalog chain leaves the disk or loops;
 *         UFT_ERR_MEMORY.
 */
uft_error_t uft_dos33_read_catalog_mem(const uint8_t *img, size_t len,
                                       uft_dos33_catalog_t *out);

/** Same, from a `.do`/`.dsk` file in DOS order. Opens read-only. */
uft_error_t uft_dos33_read_catalog(const char *path,
                                   uft_dos33_catalog_t *out);

/** Frees the entry list and zeroes the structure. */
void uft_dos33_free(uft_dos33_catalog_t *cat);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FS_DOS33_H */

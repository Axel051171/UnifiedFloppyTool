/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_dos33.c
 * @brief Apple DOS 3.3 catalog reader — P3-384. Layout and reference in
 *        include/uft/fs/uft_dos33.h.
 */
#include "uft/fs/uft_dos33.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DOS33_TRACKS        35u
#define DOS33_SPT           16u
#define DOS33_SECTOR        256u
#define DOS33_VTOC_TRACK    17u
#define DOS33_ENTRY_FIRST   0x0Bu
#define DOS33_ENTRY_SIZE    35u
#define DOS33_ENTRIES       7u
#define DOS33_NAME_LEN      30u
#define DOS33_TRACK_UNUSED  0x00u
#define DOS33_TRACK_DELETED 0xFFu

static const uint8_t *dos33_sector(const uint8_t *img, unsigned t, unsigned s)
{
    return img + ((size_t)t * DOS33_SPT + s) * DOS33_SECTOR;
}

/* The geometry a DOS 3.3 VTOC must state (a2tools.c:199-203 checks the
 * same six bytes). */
static bool dos33_vtoc_plausible(const uint8_t *v)
{
    return v[0x03] == 3u && v[0x27] == 0x7Au &&
           v[0x34] == DOS33_TRACKS && v[0x35] == DOS33_SPT &&
           v[0x36] == 0x00u && v[0x37] == 0x01u;
}

static unsigned dos33_free_count(const uint8_t *v)
{
    unsigned n = 0;
    for (unsigned t = 0; t < DOS33_TRACKS; t++) {
        unsigned map = ((unsigned)v[0x38 + 4u * t] << 8) | v[0x39 + 4u * t];
        for (unsigned b = 0; b < 16u; b++) n += (map >> b) & 1u;
    }
    return n;
}

static char dos33_type_char(uint8_t type)
{
    switch (type) {
    case 0x00: return 'T';
    case 0x01: return 'I';
    case 0x02: return 'A';
    case 0x04: return 'B';
    case 0x08: return 'S';
    case 0x10: return 'R';
    case 0x20: return 'X';
    case 0x40: return 'Y';
    default:   return '?';
    }
}

static void dos33_entry(const uint8_t *e, bool deleted, uft_dos33_entry_t *out)
{
    /* A deleted entry has only 29 name bytes: DOS keeps the original
     * T/S-list track in byte 30 (header). */
    unsigned len = deleted ? DOS33_NAME_LEN - 1u : DOS33_NAME_LEN;

    memset(out, 0, sizeof *out);
    for (unsigned i = 0; i < len; i++) out->name[i] = (char)(e[3 + i] & 0x7Fu);
    while (len > 0 && out->name[len - 1] == ' ') out->name[--len] = '\0';

    out->locked    = (e[2] & 0x80u) != 0;
    out->type      = (uint8_t)(e[2] & 0x7Fu);
    out->type_char = dos33_type_char(out->type);
    out->deleted   = deleted;
    out->sectors   = (uint16_t)(e[0x21] | (e[0x22] << 8));
    out->ts_sector = e[1];
    if (!deleted)
        out->ts_track = e[0];
    else
        out->ts_track = (e[3 + DOS33_NAME_LEN - 1u] < DOS33_TRACKS)
                            ? e[3 + DOS33_NAME_LEN - 1u] : 0xFFu;
}

uft_error_t uft_dos33_read_catalog_mem(const uint8_t *img, size_t len,
                                       uft_dos33_catalog_t *out)
{
    if (!img || !out) return UFT_ERR_INVALID_ARG;
    memset(out, 0, sizeof *out);
    if (len != UFT_DOS33_IMAGE_SIZE) return UFT_ERR_FORMAT;

    const uint8_t *v = dos33_sector(img, DOS33_VTOC_TRACK, 0);
    if (!dos33_vtoc_plausible(v)) return UFT_ERR_FORMAT;

    out->volume       = v[0x06];
    out->dos_release  = v[0x03];
    out->free_sectors = dos33_free_count(v);

    /* The chain brake: a sector visited twice is a loop, a pointer off the
     * disk is a broken chain. Both are findings, not an empty catalog. */
    uint8_t visited[DOS33_TRACKS * DOS33_SPT];
    memset(visited, 0, sizeof visited);
    int cap = 0;
    unsigned t = v[0x01], s = v[0x02];

    while (t != 0 || s != 0) {
        if (t >= DOS33_TRACKS || s >= DOS33_SPT || visited[t * DOS33_SPT + s]) {
            uft_dos33_free(out);
            return UFT_ERR_FORMAT;
        }
        visited[t * DOS33_SPT + s] = 1;
        out->catalog_sectors++;

        const uint8_t *c = dos33_sector(img, t, s);
        for (unsigned i = 0; i < DOS33_ENTRIES; i++) {
            const uint8_t *e = c + DOS33_ENTRY_FIRST + i * DOS33_ENTRY_SIZE;
            if (e[0] == DOS33_TRACK_UNUSED) continue;
            if (out->entry_count == cap) {
                int ncap = cap ? cap * 2 : 16;
                void *n = realloc(out->entries, (size_t)ncap * sizeof *out->entries);
                if (!n) { uft_dos33_free(out); return UFT_ERR_MEMORY; }
                out->entries = n;
                cap = ncap;
            }
            bool del = (e[0] == DOS33_TRACK_DELETED);
            dos33_entry(e, del, &out->entries[out->entry_count++]);
            if (del) out->deleted_count++;
        }
        t = c[0x01];
        s = c[0x02];
    }
    return UFT_OK;
}

uft_error_t uft_dos33_read_catalog(const char *path, uft_dos33_catalog_t *out)
{
    if (!path || !out) return UFT_ERR_INVALID_ARG;
    memset(out, 0, sizeof *out);

    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;
    uint8_t *img = malloc(UFT_DOS33_IMAGE_SIZE + 1u);
    if (!img) { fclose(f); return UFT_ERR_MEMORY; }
    size_t n = fread(img, 1, UFT_DOS33_IMAGE_SIZE + 1u, f);
    int err = ferror(f);
    fclose(f);
    if (err) { free(img); return UFT_ERR_IO; }

    uft_error_t rc = uft_dos33_read_catalog_mem(img, n, out);
    free(img);
    return rc;
}

void uft_dos33_free(uft_dos33_catalog_t *cat)
{
    if (!cat) return;
    free(cat->entries);
    memset(cat, 0, sizeof *cat);
}

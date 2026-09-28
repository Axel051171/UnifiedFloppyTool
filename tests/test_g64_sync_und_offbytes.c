/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_g64_sync_und_offbytes.c
 * @brief G64: a sync is what the 1541 sees as one, and the two off bytes
 *        after the checksum do not decide whether a sector exists
 *        (P3-663, MF-1500).
 *
 * REFERENCE: nibtools (rittwage/nibtools @ 0abdc11, gcr.c) — behaviour,
 * not code:
 *   - find_sync(): a sync is a byte whose lowest bit is set, followed by
 *     0xFF — at least 9 one bits ("sync flag goes up after the 10th bit
 *     ... but sometimes they are short a bit"). Valid GCR never carries
 *     more than 8 ones in a row, so 9 cannot fire inside data.
 *   - convert_GCR_sector(): all 65 GCR groups are decoded; invalid GCR is
 *     looked for in the first 320 GCR bytes only (block id, 255 data
 *     bytes). The last group carries data byte 256, the checksum and the
 *     two OFF bytes, which a 1541 does not check (D64.TXT, "off bytes").
 *   Measured by executing nibconv on the real disk (t4 review): 768 of 768
 *   sectors of c64pp_aliensyndrome.g64 are good.
 *
 * What the tree does since MF-1500: it CALLS the sync rule the tree already
 * had — gcr_find_sync() in src/formats/c64/uft_gcr_ops.c, definition A
 * (0xFF followed by a byte with its MSB set, 9 one bits from a byte
 * boundary) — instead of writing a fifth one (MF-1177). nibtools' rule
 * above counts the same 9 bits placed one byte earlier; on this disk both
 * give 768 of 768 (measured with throwaway builds of both). They differ
 * only for a sync of one single 0xFF byte, which no file here carries.
 *
 * What the tree did (src/formats/g64/uft_g64.c before MF-1500):
 *   - find_sync() wanted 5 whole 0xFF bytes. The real disk has mostly 3-4
 *     byte syncs; the tree delivered 33 of its 768 sectors.
 *   - one invalid GCR code ANYWHERE in the 65 groups — the off bytes
 *     included — dropped the sector without an entry.
 *   - the checksum was recomputed, but no sector said so
 *     (UFT_SECTOR_CRC_CHECKED missing), so a good sector reached the
 *     disk2 bridge as "unchecked" — the counterpart of P3-659.
 *
 * The CI part derives its inputs in memory from
 * tests/corpus_free/vice_c1541_35trk.g64 (VICE c1541, cross-tool): no byte
 * moves, only the bytes named in each case change.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must point at tests/corpus_free"
#endif
#ifndef UFT_CORPUS_RESTRICTED_DIR
#error "UFT_CORPUS_RESTRICTED_DIR must point at tests/corpus"
#endif

extern const uft_format_plugin_t uft_format_plugin_g64;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

static uint8_t *lies(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long l = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = (l > 0) ? malloc((size_t)l) : NULL;
    if (b && fread(b, 1, (size_t)l, f) != (size_t)l) { free(b); b = NULL; }
    fclose(f);
    *n = b ? (size_t)l : 0;
    return b;
}

static const char *tmpdir(void)
{
    const char *t = getenv("TMPDIR");
    if (!t || !t[0]) t = getenv("TMP");
    if (!t || !t[0]) t = getenv("TEMP");
    if (!t || !t[0]) t = ".";
    return t;
}

/* Track 1 of a G64: offset table at byte 12, u32 per half track; the track
 * starts with its u16 length. */
static uint8_t *spur1(uint8_t *g, size_t n, size_t *len)
{
    uint32_t off = (uint32_t)g[12] | (uint32_t)g[13] << 8 |
                   (uint32_t)g[14] << 16 | (uint32_t)g[15] << 24;
    if (off + 2u > n) return NULL;
    *len = (size_t)(g[off] | g[off + 1] << 8);
    if (off + 2u + *len > n) return NULL;
    return g + off + 2u;
}

typedef struct {
    size_t   n;
    unsigned ok, crc_err, checked;
    uint8_t  data[21][256];
    int      nr[21];
} ergebnis_t;

static int lese_spur1(const char *pfad, ergebnis_t *e)
{
    uft_disk_t disk;
    uft_track_t tr;
    memset(e, 0, sizeof *e);
    memset(&disk, 0, sizeof disk);
    memset(&tr, 0, sizeof tr);
    if (uft_format_plugin_g64.open(&disk, pfad, true) != UFT_OK) return -1;
    if (uft_format_plugin_g64.read_track(&disk, 0, 0, &tr) != UFT_OK) {
        uft_format_plugin_g64.close(&disk);
        return -1;
    }
    for (size_t i = 0; i < tr.sector_count && e->n < 21; i++) {
        const uft_sector_t *s = &tr.sectors[i];
        if (s->status & UFT_SECTOR_CRC_ERROR) e->crc_err++; else e->ok++;
        if (s->status & UFT_SECTOR_CRC_CHECKED) e->checked++;
        e->nr[e->n] = s->id.sector;
        if (s->data && s->data_len == 256) memcpy(e->data[e->n], s->data, 256);
        e->n++;
    }
    uft_track_cleanup(&tr);
    uft_format_plugin_g64.close(&disk);
    return 0;
}

static int schreibe(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return -1;
    size_t w = fwrite(b, 1, n, f);
    fclose(f);
    return w == n ? 0 : -1;
}

/* Same content per sector number? */
static int gleich(const ergebnis_t *a, const ergebnis_t *b)
{
    int treffer = 0;
    for (size_t i = 0; i < a->n; i++)
        for (size_t j = 0; j < b->n; j++)
            if (a->nr[i] == b->nr[j] && memcmp(a->data[i], b->data[j], 256) == 0) {
                treffer++;
                break;
            }
    return treffer;
}

int main(void)
{
    char pfad[600], h[200];
    size_t n = 0, tl = 0;
    printf("G64: Sync nach dem 1541-Mass, Off-Bytes entscheiden nichts (MF-1500)\n");

    snprintf(pfad, sizeof pfad, "%s/vice_c1541_35trk.g64", UFT_CORPUS_DIR);
    uint8_t *orig = lies(pfad, &n);
    if (!orig) { pruefe("Korpusdatei lesbar", 0, pfad); return 1; }

    ergebnis_t ref, e;
    if (lese_spur1(pfad, &ref) != 0) { pruefe("Original oeffnet", 0, NULL); return 1; }
    snprintf(h, sizeof h, "%zu Sektoren, %u gut, %u mit CRC_CHECKED", ref.n, ref.ok, ref.checked);
    pruefe("Original, Spur 1: 21 gute Sektoren", ref.n == 21 && ref.ok == 21, h);
    pruefe("Original, Spur 1: jeder gute Sektor sagt, dass er geprueft ist (CRC_CHECKED)",
           ref.checked == 21, h);

    uint8_t *kopie = malloc(n);
    snprintf(pfad, sizeof pfad, "%s/uft_mf1500.g64", tmpdir());

    /* 1. Short syncs: every run of >= 5 x 0xFF on track 1 loses its first
     *    byte to 0x55 (01010101, bit 0 set) — 32 + 1 one bits remain, the
     *    1541 needs 10. */
    memcpy(kopie, orig, n);
    uint8_t *t = spur1(kopie, n, &tl);
    unsigned kurz = 0;
    for (size_t k = 0; t && k < tl; ) {
        if (t[k] == 0xFF) {
            size_t j = k;
            while (j < tl && t[j] == 0xFF) j++;
            if (j - k >= 5) { t[k] = 0x55; kurz++; }
            k = j;
        } else {
            k++;
        }
    }
    schreibe(pfad, kopie, n);
    lese_spur1(pfad, &e);
    snprintf(h, sizeof h, "%u Syncs gekuerzt; %zu Sektoren, %d inhaltsgleich", kurz, e.n, gleich(&e, &ref));
    pruefe("Syncs von 4 Byte: 21 von 21 Sektoren, inhaltsgleich",
           kurz > 0 && e.n == 21 && e.ok == 21 && gleich(&e, &ref) == 21, h);

    /* The data block starts after a sync with 0x55 (GCR of 0x07's first
     * bits); the header with 0x52. Find the data blocks on track 1. */
    size_t blk[32];
    unsigned nb = 0;
    t = spur1(orig, n, &tl);
    for (size_t k = 1; t && k + 325 <= tl && nb < 32; k++)
        if (t[k - 1] == 0xFF && t[k] == 0x55) blk[nb++] = k;

    /* 2. Off bytes: GCR bytes 323 and 324 of every data block set to 0x00
     *    — invalid GCR (00000), but only in the two off bytes. */
    memcpy(kopie, orig, n);
    t = spur1(kopie, n, &tl);
    for (unsigned b = 0; b < nb; b++) { t[blk[b] + 323] = 0x00; t[blk[b] + 324] = 0x00; }
    schreibe(pfad, kopie, n);
    lese_spur1(pfad, &e);
    snprintf(h, sizeof h, "%u Datenbloecke; %zu Sektoren, %u gut, %d inhaltsgleich",
             nb, e.n, e.ok, gleich(&e, &ref));
    pruefe("ungueltige Off-Bytes: 21 von 21 Sektoren, gut und inhaltsgleich",
           nb == 21 && e.n == 21 && e.ok == 21 && gleich(&e, &ref) == 21, h);

    /* 3. One invalid GCR byte inside the checked 320 bytes of the FIRST
     *    data block: the sector stays, marked bad — it does not vanish. */
    memcpy(kopie, orig, n);
    t = spur1(kopie, n, &tl);
    if (nb) t[blk[0] + 100] = 0x00;
    schreibe(pfad, kopie, n);
    lese_spur1(pfad, &e);
    snprintf(h, sizeof h, "%zu Sektoren, %u gut, %u mit CRC-Fehler", e.n, e.ok, e.crc_err);
    pruefe("ungueltiges GCR in den Daten: Sektor bleibt, als fehlerhaft markiert",
           e.n == 21 && e.ok == 20 && e.crc_err == 1, h);

    remove(pfad);
    free(kopie);
    free(orig);

    /* 4. LOCAL-ONLY: the real disk. nibconv (executed): 768 of 768 good. */
    snprintf(pfad, sizeof pfad, "%s/c64pp_aliensyndrome.g64", UFT_CORPUS_RESTRICTED_DIR);
    FILE *f = fopen(pfad, "rb");
    if (!f) {
        printf("  [---] Alien Syndrome (lokal) nicht vorhanden - Teil 4 entfaellt\n");
    } else {
        fclose(f);
        uft_disk_t disk;
        memset(&disk, 0, sizeof disk);
        unsigned gut = 0, alle = 0;
        if (uft_format_plugin_g64.open(&disk, pfad, true) == UFT_OK) {
            /* all tracks the file carries: this disk has 40, not 35 */
            for (int c = 0; c < (int)disk.geometry.cylinders; c++) {
                uft_track_t tr;
                memset(&tr, 0, sizeof tr);
                if (uft_format_plugin_g64.read_track(&disk, c, 0, &tr) != UFT_OK) continue;
                for (size_t i = 0; i < tr.sector_count; i++) {
                    alle++;
                    if (!(tr.sectors[i].status & (UFT_SECTOR_CRC_ERROR | UFT_SECTOR_ID_CRC_ERROR)))
                        gut++;
                }
                uft_track_cleanup(&tr);
            }
            uft_format_plugin_g64.close(&disk);
        }
        snprintf(h, sizeof h, "%u Sektoren, %u gut", alle, gut);
        pruefe("Alien Syndrome (echt, lokal): 768 von 768 gut wie nibconv", alle == 768 && gut == 768, h);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_korpus_container.c
 * @brief Formate MIT Container-Kopf, gegen zwei unabhängige Hände (MF-796).
 *
 * ── Warum ein eigener Test ───────────────────────────────────────────────
 *
 * `test_corpus_gw_geometrie.c` prüft zehn Formate gegen `gw` 1.23. Das
 * Rezept endet dort, wo ein Format einen **Kopf** trägt: `gw` schreibt
 * nur kopflose Sektordumps. Container beherrschen **SAMdisk 4.0** und
 * **hxcfe 2.16.13**.
 *
 * ── Die Zwei-Hände-Prüfung sitzt im Generator ────────────────────────────
 *
 * Ein von SAMdisk erzeugter Container belegt allein nichts — das wäre
 * eine Hand. `tests/corpus_manifest/gen_container_corpus.py` schreibt
 * eine Datei nur, wenn eine **zweite, unabhängige** Hand sie zurücklesen
 * kann und dabei genau das Rohabbild wieder herausgibt:
 *
 *     Rohabbild --SAMdisk--> Container --hxcfe--> Rohabbild
 *
 * Für `edsk` gemessen: 737 280 B Rohabbild → 778 496 B EDSK → 737 280 B
 * zurück, **byteidentisch**. Und ohne SAMdisks Warnung „input format
 * guessed from file size", weil die Geometrie explizit übergeben wird
 * (`-c80 -s9 -z2 -b1`).
 *
 * ── Was MF-785 falsch geschlossen hatte ──────────────────────────────────
 *
 * Dort fielen fünf Container-Kandidaten durch, und die Ursache wurde in
 * der geratenen Eingangsgeometrie gesehen. Gemessen (MF-794) trägt das
 * nicht: mit expliziter Geometrie kommt für `sad` eine **byteidentische**
 * Datei heraus. Falsch war die **Erwartung** — ein Container ordnet nach
 * seinem eigenen Format, und der Test verglich gegen eine lineare
 * Anordnung. Der echte Fehler lag bei UFT.
 *
 * ── Geprüft werden vier Spuren, nicht eine (MF-795) ──────────────────────
 *
 * Spur (0,0) ist die einzige, bei der zylinder-dur und kopf-dur denselben
 * Versatz ergeben — also genau die Stelle, an der eine vertauschte
 * Anordnung unsichtbar ist. Bei `sad` lag dort ein Fehler, der 158 von
 * 160 Spuren betraf, während der Test grün blieb.
 *
 * ── Was das NICHT belegt ─────────────────────────────────────────────────
 *
 * Ein **Dateisystem**. Der Inhalt ist eigen; belegt ist die
 * Sektoraufteilung und ihre Reihenfolge.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_edsk;

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

static int _pass = 0, _fail = 0;

typedef struct {
    const char                *name;
    const uft_format_plugin_t *plugin;
    const char                *datei;
    long                       bytes;
    uint16_t                   cyl, heads, spt, ss;
    unsigned                   erste_id;   /* IBM: 1, Apple/CBM: 0 */
} fall_t;

/* Die Sollwerte sind die, auf die sich SAMdisk und hxcfe GEMEINSAM
 * festgelegt haben — nicht die, die UFT allein behauptet. Wer eine
 * ändert, muss sagen, welche der beiden Hände sich geirrt hat. */
static const fall_t FAELLE[] = {
    { "edsk", &uft_format_plugin_edsk, "samdisk_edsk.dsk",
      778496L, 80, 2, 9, 512, 1 },
};

static void free_ts(uft_track_t *t)
{
    for (size_t i = 0; i < t->sector_count; i++) free(t->sectors[i].data);
    free(t->sectors); t->sectors = NULL; t->sector_count = 0;
}

static void pruefe(const fall_t *f)
{
    char pfad[512];
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, f->datei);
    printf("  [TEST] %-8s ... ", f->name);

    FILE *fp = fopen(pfad, "rb");
    if (!fp) { printf("FAIL: %s nicht lesbar\n", f->datei); _fail++; return; }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fclose(fp);
    if (sz != f->bytes) {
        printf("FAIL: %ld B, erwartet %ld\n", sz, f->bytes); _fail++; return;
    }

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    if (f->plugin->open(&disk, pfad, true) != UFT_OK) {
        printf("FAIL: open\n"); _fail++; return;
    }

    if (disk.geometry.cylinders != f->cyl || disk.geometry.heads != f->heads ||
        disk.geometry.sectors != f->spt || disk.geometry.sector_size != f->ss) {
        printf("FAIL: Geometrie %u/%u/%u/%u, erwartet %u/%u/%u/%u\n",
               disk.geometry.cylinders, disk.geometry.heads,
               disk.geometry.sectors, disk.geometry.sector_size,
               f->cyl, f->heads, f->spt, f->ss);
        _fail++; f->plugin->close(&disk); return;
    }

    static const struct { int c, h; } SPUREN[] = { {0,0}, {0,1}, {1,0}, {-1,-1} };
    for (size_t i = 0; i < sizeof(SPUREN) / sizeof(SPUREN[0]); i++) {
        int c = SPUREN[i].c, h = SPUREN[i].h;
        if (c < 0) { c = f->cyl - 1; h = f->heads - 1; }
        if (c >= f->cyl || h >= f->heads) continue;

        uft_track_t t;
        memset(&t, 0, sizeof(t));
        if (f->plugin->read_track(&disk, c, h, &t) != UFT_OK) {
            printf("FAIL: read_track(%d,%d)\n", c, h);
            _fail++; f->plugin->close(&disk); return;
        }
        if (t.sector_count != f->spt) {
            printf("FAIL: Spur (%d,%d): %zu Sektoren, erwartet %u\n",
                   c, h, t.sector_count, f->spt);
            _fail++; free_ts(&t); f->plugin->close(&disk); return;
        }
        /* Das Quellabbild war linear zylinder-dur durchnummeriert.
         *
         * GESCHLÜSSELT WIRD NACH SEKTOR-ID, NICHT NACH LAGE. Gemessen an
         * der Datei: SAMdisk legt mit seinem Spurversatz (`--skew`,
         * Vorgabe 1) die Spur (1,0) als `9 1 2 3 4 5 6 7 8` ab. Die Datei
         * ist damit RICHTIG — der Sektor mit der ID 9 trägt die Marke 26,
         * also 18 + 8. Eine Prüfung, die physische und logische Folge
         * gleichsetzt, meldete hier einen Fehler, wo keiner ist, und
         * verlöre zugleich die Fähigkeit, echten Versatz zu erkennen.
         *
         * Geprüft wird deshalb: jeder Sektor trägt die Marke, die zu
         * SEINER ID gehört — und jede ID kommt genau einmal vor. */
        unsigned basis = ((unsigned)c * f->heads + (unsigned)h) * f->spt;
        unsigned gesehen = 0;
        for (size_t s = 0; s < t.sector_count; s++) {
            const uint8_t *b = t.sectors[s].data;
            if (!b || t.sectors[s].data_len != f->ss) {
                printf("FAIL: Spur (%d,%d) Lage %zu ohne Daten\n", c, h, s);
                _fail++; free_ts(&t); f->plugin->close(&disk); return;
            }
            if (memcmp(b, "UFT\x00", 4) != 0) {
                printf("FAIL: Spur (%d,%d) Lage %zu ohne Kennung\n", c, h, s);
                _fail++; free_ts(&t); f->plugin->close(&disk); return;
            }
            unsigned id = t.sectors[s].id.sector;
            if (id < f->erste_id || id >= f->erste_id + f->spt) {
                printf("FAIL: Spur (%d,%d) Lage %zu hat ID %u, "
                       "erwartet %u..%u\n", c, h, s, id,
                       f->erste_id, f->erste_id + f->spt - 1);
                _fail++; free_ts(&t); f->plugin->close(&disk); return;
            }
            unsigned lfd = id - f->erste_id;
            if (gesehen & (1u << lfd)) {
                printf("FAIL: Spur (%d,%d) ID %u zweimal\n", c, h, id);
                _fail++; free_ts(&t); f->plugin->close(&disk); return;
            }
            gesehen |= 1u << lfd;

            unsigned nr = (unsigned)b[4] | ((unsigned)b[5] << 8);
            if (nr != basis + lfd) {
                printf("FAIL: Spur (%d,%d) Sektor-ID %u traegt Marke %u, "
                       "erwartet %u — umsortiert\n", c, h, id, nr, basis + lfd);
                _fail++; free_ts(&t); f->plugin->close(&disk); return;
            }
        }
        if (gesehen != (1u << f->spt) - 1u) {
            printf("FAIL: Spur (%d,%d) — nicht jede ID genau einmal "
                   "(Maske 0x%x)\n", c, h, gesehen);
            _fail++; free_ts(&t); f->plugin->close(&disk); return;
        }
        free_ts(&t);
    }

    f->plugin->close(&disk);
    printf("OK  %u/%u/%u/%u, 4 Spuren in Reihenfolge\n",
           f->cyl, f->heads, f->spt, f->ss);
    _pass++;
}

int main(void)
{
    printf("test_korpus_container (MF-796) — SAMdisk 4.0 + hxcfe 2.16.13\n");
    for (size_t i = 0; i < sizeof(FAELLE) / sizeof(FAELLE[0]); i++)
        pruefe(&FAELLE[i]);
    printf("%d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail ? 1 : 0;
}

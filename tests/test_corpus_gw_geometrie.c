/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_corpus_gw_geometrie.c
 * @brief Fünf T3-Formate gegen `gw` 1.23 — die Geometrie, gemessen (MF-783).
 *
 * `micropolis`, `northstar`, `ssd`, `po` und `trd` standen auf **T3**:
 * kein Test, keine Spec-Quelle. Jedes behauptet eine Geometrie, und jede
 * Behauptung stand für sich allein.
 *
 * `gw` 1.23 führt für alle fünf ein **eigenes, unabhängiges** Modell
 * derselben Geometrie. Gemessen, je Format Rohabbild → SCP → Rohabbild:
 *
 *     micropolis  micropolis.100tpi.ss   1232 Sektoren (77×16)   315 392 B
 *     northstar   northstar.fm.ss         350 Sektoren (35×10)    89 600 B
 *     ssd         acorn.dfs.ss            400 Sektoren (40×10)   102 400 B
 *     po          apple2.prodos.140       560 Sektoren (35×16)   143 360 B
 *     trd         zx.trdos.ds80          2560 Sektoren (80×2×16) 655 360 B
 *
 * Alle fünf Rundläufe **byteidentisch**. Zwei getrennte Hände sagen
 * dasselbe über dieselbe Diskette.
 *
 * ── Warum der Inhalt strukturiert ist ────────────────────────────────────
 *
 * Ein Abbild aus Zufallsbytes belegt die Geometrie nur halb: es zeigt,
 * dass die richtige **Anzahl** Bytes zurückkommt, nicht dass sie in der
 * richtigen **Reihenfolge** stehen. Würde `gw` die Sektoren anders
 * nummerieren als UFT, fiele das bei Zufallsdaten trotzdem als
 * byteidentisch auf — der Strom ginge nur durch dieselbe Permutation hin
 * und zurück.
 *
 * Deshalb trägt jeder Sektor seine **laufende Nummer** in den Bytes 4–5,
 * hinter der Kennung `UFT\0`. Der Test liest sie zurück und prüft die
 * Reihenfolge, nicht nur die Länge.
 *
 * ── Was das NICHT belegt ─────────────────────────────────────────────────
 *
 * Ein **Dateisystem**. Der Inhalt ist eigen; belegt ist allein, dass zwei
 * unabhängige Hände dieselbe Sektoraufteilung annehmen. Dieselbe Grenze
 * wie bei MF-782, und sie steht hier, damit niemand mehr hineinliest.
 *
 * Erzeugt von `tests/corpus_manifest/gen_gw_geometry_corpus.py`.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_micropolis;
extern const uft_format_plugin_t uft_format_plugin_northstar;
extern const uft_format_plugin_t uft_format_plugin_ssd;
extern const uft_format_plugin_t uft_format_plugin_po;
extern const uft_format_plugin_t uft_format_plugin_trd;
extern const uft_format_plugin_t uft_format_plugin_pdp;
extern const uft_format_plugin_t uft_format_plugin_img;
extern const uft_format_plugin_t uft_format_plugin_t1k;
extern const uft_format_plugin_t uft_format_plugin_sam;
extern const uft_format_plugin_t uft_format_plugin_jvc;

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

static int _pass = 0, _fail = 0;

typedef struct {
    const char                 *name;
    const uft_format_plugin_t  *plugin;
    const char                 *datei;
    long                        bytes;
    uint16_t                    cyl, heads, spt, ss;
} fall_t;

/* Die Sollwerte sind die, auf die sich UFT und `gw` GEMEINSAM festgelegt
 * haben — nicht die, die UFT allein behauptet. Wer eine ändert, muss
 * sagen, welche der beiden Hände sich geirrt hat. */
static const fall_t FAELLE[] = {
    { "micropolis", &uft_format_plugin_micropolis, "gw_micropolis.img",
      315392L, 77, 1, 16, 256 },
    { "northstar",  &uft_format_plugin_northstar,  "gw_northstar.img",
       89600L, 35, 1, 10, 256 },
    { "ssd",        &uft_format_plugin_ssd,        "gw_ssd.img",
      102400L, 40, 1, 10, 256 },
    { "po",         &uft_format_plugin_po,         "gw_po.img",
      143360L, 35, 1, 16, 256 },
    { "trd",        &uft_format_plugin_trd,        "gw_trd.img",
      655360L, 80, 2, 16, 256 },

    /* Zweite Runde (MF-784). Alle fuenf sind KOPFLOSE Sektordumps —
     * Formate mit Container-Kopf (sap, 2img, vdk, fdi_pc98) oder
     * Archivstruktur (scl) kann gw grundsaetzlich nicht liefern. */
    { "pdp",        &uft_format_plugin_pdp,        "gw_pdp.img",
      256256L, 77, 1, 26, 128 },
    { "img",        &uft_format_plugin_img,        "gw_img.img",
      737280L, 80, 2,  9, 512 },
    { "t1k",        &uft_format_plugin_t1k,        "gw_t1k.img",
     1474560L, 80, 2, 18, 512 },
    { "sam",        &uft_format_plugin_sam,        "gw_sam.img",
      819200L, 80, 2, 10, 512 },
    { "jvc",        &uft_format_plugin_jvc,        "gw_jvc.img",
      161280L, 35, 1, 18, 256 },
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
    printf("  [TEST] %-12s ... ", f->name);

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

    int schlecht = 0;
    if (disk.geometry.cylinders  != f->cyl)   schlecht = 1;
    if (disk.geometry.heads      != f->heads) schlecht = 2;
    if (disk.geometry.sectors    != f->spt)   schlecht = 3;
    if (disk.geometry.sector_size != f->ss)   schlecht = 4;
    if (schlecht) {
        printf("FAIL: Geometrie %u/%u/%u/%u, erwartet %u/%u/%u/%u (Feld %d)\n",
               disk.geometry.cylinders, disk.geometry.heads,
               disk.geometry.sectors, disk.geometry.sector_size,
               f->cyl, f->heads, f->spt, f->ss, schlecht);
        _fail++; f->plugin->close(&disk); return;
    }

    /* Die REIHENFOLGE, nicht nur die Laenge: Spur 0 muss die Sektoren 0..n-1
     * in genau dieser Folge tragen. Jeder Sektor nennt seine Nummer selbst. */
    uft_track_t t;
    memset(&t, 0, sizeof(t));
    if (f->plugin->read_track(&disk, 0, 0, &t) != UFT_OK) {
        printf("FAIL: read_track(0,0)\n"); _fail++; f->plugin->close(&disk);
        return;
    }
    if (t.sector_count != f->spt) {
        printf("FAIL: %zu Sektoren auf Spur 0, erwartet %u\n",
               t.sector_count, f->spt);
        _fail++; free_ts(&t); f->plugin->close(&disk); return;
    }
    for (size_t s = 0; s < t.sector_count; s++) {
        const uint8_t *b = t.sectors[s].data;
        if (!b || t.sectors[s].data_len != f->ss) {
            printf("FAIL: Sektor %zu ohne Daten\n", s);
            _fail++; free_ts(&t); f->plugin->close(&disk); return;
        }
        if (memcmp(b, "UFT\x00", 4) != 0) {
            printf("FAIL: Sektor %zu ohne Kennung\n", s);
            _fail++; free_ts(&t); f->plugin->close(&disk); return;
        }
        unsigned nr = (unsigned)b[4] | ((unsigned)b[5] << 8);
        if (nr != (unsigned)s) {
            printf("FAIL: Sektor an Lage %zu nennt sich %u — umsortiert\n",
                   s, nr);
            _fail++; free_ts(&t); f->plugin->close(&disk); return;
        }
    }

    free_ts(&t);
    f->plugin->close(&disk);
    printf("OK  %u/%u/%u/%u, Spur 0 in Reihenfolge\n",
           f->cyl, f->heads, f->spt, f->ss);
    _pass++;
}

int main(void)
{
    printf("test_corpus_gw_geometrie (MF-783) — Abbilder von gw 1.23\n");
    for (size_t i = 0; i < sizeof(FAELLE) / sizeof(FAELLE[0]); i++)
        pruefe(&FAELLE[i]);
    printf("%d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail ? 1 : 0;
}

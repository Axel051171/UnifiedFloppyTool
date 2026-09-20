/**
 * @file test_td0_error_marks.c
 * @brief TD0 disk-error marking (read + represent) and the open-scan off-by-one.
 *
 * Links the real Teledisk TD0 plugin (src/formats/td0/uft_td0.c). TD0 encodes
 * per-sector status in the sector-header flag byte.
 *
 * BERICHTIGT MF-1286: hier stand „bit0 (0x01) = data CRC error". Falsch —
 * 0x01 ist DUP, die doppelte Sektor-ID. Der CRC-Fehler ist **bit1 (0x02)**,
 * und 0x04 ist die geloeschte Datenmarke. Vier Quellen sagen 0x02, zwei
 * davon ausserhalb dieses Baums und eine davon ein SCHREIBER:
 * `uft_td0.h:101`, `uft_format_converters.c:46`, `samdisk/td0.cpp:257`,
 * libdsk `drvtele.c:138` (lesend) und `:732` (schreibend).
 *
 * **Und das ist in DIESER Datei das zweite Mal.** Ein paar Zeilen
 * weiter unten steht ueber die Kennung: „the test was green because both
 * sides shared the same mistake" (MF-389). Genau das ist wieder
 * passiert: die Pruefdatei schrieb 0x01 fuer „CRC err", weil das Plugin
 * 0x01 las. Ein Test, der sich seine Pruefdatei selbst baut, prueft die
 * Erfindung gegen sich selbst (Klasse MF-1009/MF-1028) — er kann diesen
 * Fehler nicht finden, er kann ihn nur festhalten.
 *
 * TD0 is read-only (writing needs re-compression), so this covers the
 * read + represent half of the disk-error work package.
 *
 * The existing test_td0_plugin.c only exercises the probe (magic bytes) and a
 * data-less header — it never walks a data-bearing track. This test builds a
 * NORMAL (uncompressed) TD0 with one track of three 256-byte data sectors
 * (normal / deleted / CRC-error) and asserts:
 *   - open() reports the correct geometry (cylinders == 1). This is the
 *     regression guard for MF-334: open()'s geometry scan used to skip
 *     `len - 1` bytes of each data record while read_track consumes `len`,
 *     drifting one byte per data sector and mis-scanning multi-sector tracks.
 *     A misaligned scan would not land on the 0xFF end marker with one track.
 *   - read_track() surfaces the deleted / CRC-error flags on the right sectors.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include "uft/formats/uft_td0.h"   /* MF-1296: uft_td0_crc() */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_td0;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SS 256u

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_td0_err_%d.td0", dir, rand() % 100000);
}

static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

/* One raw 256-byte data sector: 6-byte header + [len LE16] + method(0) + data.
   data record length = 1 (method) + 256 = 257. First data byte is a tag. */
/* Der ZWEITE Anker fuer `uft_td0_crc()` (MF-1296).
 *
 * Der erste liegt in `test_td0_pruefsummen.c` Gruppe 1 und misst gegen
 * eine von libdsk geschriebene Datei. Dieser hier nagelt die vier Werte
 * fest, die DIESE Pruefdatei braucht — gerechnet mit einer unabhaengigen
 * Python-Umsetzung, nicht mit der C-Fassung, die hier geprueft wird.
 *
 * Ohne ihn waere `put_sector()` zirkulaer: es schriebe, was der Leser
 * ohnehin erwartet, und beide koennten gemeinsam falsch liegen — die
 * Gestalt von MF-1009 (`apridisk`), wo Packer und Entpacker
 * Spiegelbilder derselben Erfindung waren. */
static int crc_anker_haelt(void) {
    static const struct { uint8_t tag, erwartet; } anker[] = {
        { 0xA1, 0x2D }, { 0xB2, 0x27 }, { 0xC3, 0x8E }, { 0xD4, 0x00 },
    };
    int gut = 1;
    for (unsigned k = 0; k < sizeof(anker)/sizeof(anker[0]); k++) {
        uint8_t daten[SS];
        daten[0] = anker[k].tag;
        for (unsigned i = 1; i < SS; i++) daten[i] = 0x10;
        const uint8_t ist = (uint8_t)(uft_td0_crc(daten, SS, 0u) & 0xFFu);
        if (ist != anker[k].erwartet) {
            printf("    CRC-Anker verfehlt: tag 0x%02X -> 0x%02X, "
                   "erwartet 0x%02X\n",
                   anker[k].tag, ist, anker[k].erwartet);
            gut = 0;
        }
    }
    return gut;
}

static void put_sector(FILE *f, uint8_t sec_num, uint8_t flags, uint8_t tag) {
    /* MF-1296: das sechste Byte des Sektorkopfs ist die Pruefsumme UEBER
     * DIE DATEN, und sie stand hier auf 0 — ein Wert, den keine der vier
     * Nutzlasten hat. Solange niemand nachrechnete, fiel das nicht auf;
     * seit MF-1296 rechnet der Leser nach, und die Pruefdatei muss sagen
     * koennen, was sie behauptet.
     *
     * Die Rechnung ist NICHT zirkulaer, obwohl Pruefdatei und Pruefling
     * dieselbe Funktion rufen: `uft_td0_crc()` ist in
     * `test_td0_pruefsummen.c` Gruppe 1 an einer von LIBDSK geschriebenen
     * Datei geeicht (Dateikopf 0x6EDD, Kommentar 0xFEF2, 1440 Sektoren).
     * Die Verankerung liegt also ausserhalb dieses Baums — anders als bei
     * `apridisk` (MF-1009), wo Packer und Entpacker Spiegelbilder
     * derselben Erfindung waren.
     *
     * Zweiter Anker unten: die vier erwarteten Bytes sind mit einer
     * UNABHAENGIGEN Python-Umsetzung gerechnet und stehen als Zahlen da. */
    uint8_t daten[SS];
    daten[0] = tag;
    for (unsigned i = 1; i < SS; i++) daten[i] = 0x10;
    const uint8_t crc = (uint8_t)(uft_td0_crc(daten, SS, 0u) & 0xFFu);

    uint8_t hdr[6] = { 0, 0, sec_num, 1 /*size code 1 = 256*/, flags, crc };
    fwrite(hdr, 1, 6, f);
    uint16_t len = 1 + SS;
    uint8_t lb[2] = { (uint8_t)(len & 0xFF), (uint8_t)(len >> 8) };
    fwrite(lb, 1, 2, f);
    fputc(0, f);                                   /* encoding method 0 = raw */
    fwrite(daten, 1, SS, f);
}

static int build_td0(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    /* Signature 'T','D' = normal/RLE Teledisk. This fixture used to write
     * {0x44,0x54} ("DT"), matching the byte-swapped magic constant the plugin
     * carried until MF-389 — the test was green because both sides shared the
     * same mistake. See docs/KNOWN_ISSUES.md FMT-13. */
    uint8_t header[12] = { 0x54, 0x44, 0, 0, 0x00 /*version<0x10 => no comment*/,
                           0, 0, 0, 0, 1 /*sides*/, 0, 0 };
    fwrite(header, 1, 12, f);
    uint8_t trk_hdr[4] = { 4 /*num_sec*/, 0 /*cyl*/, 0 /*head*/, 0 /*crc*/ };
    fwrite(trk_hdr, 1, 4, f);
    put_sector(f, 1, 0x00, 0xA1);                  /* normal        */
    put_sector(f, 2, 0x04, 0xB2);                  /* deleted DAM   */
    put_sector(f, 3, 0x02, 0xC3);                  /* CRC err (war 0x01) */
    /* MF-1286: der vierte ist neu und sichert die Richtung, die vorher
     * still falsch war — eine doppelte Sektor-ID ist KEIN CRC-Fehler. */
    put_sector(f, 4, 0x01, 0xD4);                  /* doppelte ID   */
    uint8_t end[4] = { 0xFF, 0, 0, 0 };            /* end-of-tracks marker */
    fwrite(end, 1, 4, f);
    fclose(f);
    return 1;
}

static const uft_sector_t *find_by_tag(const uft_track_t *t, uint8_t tag) {
    for (size_t i = 0; i < t->sector_count; i++)
        if (t->sectors[i].data && t->sectors[i].data[0] == tag)
            return &t->sectors[i];
    return NULL;
}

TEST(open_geometry_scan_aligned) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_td0(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_td0.open(&disk, path, true) == UFT_OK);
    /* One track (cyl 0). A drifting scan (old len-1) would not land on the
       0xFF end marker and would mis-count the geometry. */
    ASSERT(disk.geometry.cylinders == 1);

    if (uft_format_plugin_td0.close) uft_format_plugin_td0.close(&disk);
    remove(path);
}

TEST(read_surfaces_error_marks) {
    /* MF-1296: erst der Anker, dann die Pruefdatei. Haelt er nicht,
     * ist alles Weitere eine Rechnung, die sich selbst bestaetigt. */
    ASSERT(crc_anker_haelt());
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_td0(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_td0.open(&disk, path, true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_td0.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 4);

    const uft_sector_t *n = find_by_tag(&t, 0xA1);
    const uft_sector_t *d = find_by_tag(&t, 0xB2);
    const uft_sector_t *e = find_by_tag(&t, 0xC3);
    const uft_sector_t *u = find_by_tag(&t, 0xD4);
    ASSERT(n && d && e && u);
    ASSERT(n->crc_ok == true);
    ASSERT(n->deleted == false);
    ASSERT(d->deleted == true);
    ASSERT(e->crc_ok == false);
    ASSERT(e->crc_valid == false);
    ASSERT(e->data_crc_ok == false);
    /* MF-1286: 0x01 ist DUP. Der Sektor ist unauffaellig, seine Daten
     * sind gut — wer ihn als CRC-kaputt meldet, erfindet einen Fehler. */
    ASSERT(u->crc_ok == true);
    ASSERT(u->deleted == false);

    free_track_sectors(&t);
    if (uft_format_plugin_td0.close) uft_format_plugin_td0.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== TD0 disk-error marking (read/represent) + scan alignment ===\n");
    RUN(open_geometry_scan_aligned);
    RUN(read_surfaces_error_marks);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

/**
 * @file test_d88_header_variants.c
 * @brief D88/D77: der 672-Byte-Kopf ist gueltig und wurde abgewiesen (MF-625).
 *
 * Benannte Referenz (dieselbe, die schon MF-336 entschieden hat):
 *   - <https://www.pc98.org/project/doc/d88.html> — "Total Size: 688 or 672
 *     bytes"; die Spurtabelle bei 0x20 hat **164 Eintraege (modern)** oder
 *     **160 (aeltere Werkzeuge, ergibt einen 672-Byte-Kopf)".
 *   - MAME `src/lib/formats/d88_dsk.cpp` (BSD-3-Clause) fuehrt denselben
 *     Fall: der erste Spur-Versatz muss 0x02A0 **oder** 0x02B0 sein.
 *
 * Was der Baum vorher tat, gemessen an `src/formats/d88/uft_d88.c:53` und
 * `src/formats/d77/uft_d77.c:84`: beide Erkenner pruefen jeden Spur-Versatz
 * gegen die feste Untergrenze 0x2B0 und geben bei Unterschreitung `false`
 * zurueck. Ein Abbild mit 160-Eintraege-Kopf hat seinen ersten Versatz bei
 * 0x2A0 — also **acht Byte unterhalb** der Schranke. Folge: die Datei wird
 * nicht etwa teilweise gelesen, sondern vom Erkenner rundheraus abgewiesen
 * und damit von `uft_disk_open()` gar nicht erst geoeffnet.
 *
 * Das ist kein Randfall der Darstellung, sondern Datenverlust am Tor: ein
 * vollstaendig gueltiges Abbild wird als "unbekanntes Format" behandelt.
 *
 * Diese Datei baut je ein Abbild in beiden Kopf-Fassungen, streng nach der
 * Spec oben, und verlangt, dass beide gelesen werden. Der 688er-Fall steht
 * dabei nicht aus Bequemlichkeit da: er ist die Gegenprobe, dass die
 * Korrektur die bisher funktionierende Fassung nicht beschaedigt.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_d88;
extern const uft_format_plugin_t uft_format_plugin_d77;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* Die beiden Kopf-Fassungen aus der Spec. */
#define HDR_164  0x2B0u          /* 688 Byte, 164 Spureintraege */
#define HDR_160  0x2A0u          /* 672 Byte, 160 Spureintraege */

#define SS    256u
#define NSEC  2

static void put_le16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v & 0xFF);
                                               p[1] = (uint8_t)(v >> 8); }
static void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);        p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF); p[3] = (uint8_t)(v >> 24);
}

static void temp_path(char *path, size_t size, const char *marke) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_d88_hdr_%s_%d.img", dir, marke, rand() % 100000);
}

static void free_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

/**
 * Baut ein einspuriges Abbild mit der gewuenschten Kopfgroesse.
 *
 * @param hdr  HDR_164 oder HDR_160 — bestimmt zugleich, wie viele
 *             Spureintraege die Tabelle hat und wo die Daten beginnen.
 */
static int build(const char *path, uint32_t hdr)
{
    const uint32_t track_len = NSEC * (16u + SS);
    const uint32_t total     = hdr + track_len;
    uint8_t *buf = (uint8_t *)calloc(1, total);
    if (!buf) return 0;

    memcpy(buf, "UFT-HDRTEST", 11);   /* Datentraegername, ASCII + NUL-Rest */
    buf[0x1A] = 0x00;                 /* nicht schreibgeschuetzt */
    buf[0x1B] = 0x00;                 /* Medium 2D */
    put_le32(buf + 0x1C, total);      /* Abbildgroesse */
    put_le32(buf + 0x20, hdr);        /* Spur 0 beginnt hinter dem Kopf */
    /* Alle weiteren Eintraege bleiben 0 = unformatiert. Bei HDR_160 liegen
     * die letzten vier Eintraege der 164er-Lesart bereits IM Sektorkopf der
     * ersten Spur — genau das muss ein Leser vermeiden. */

    uint8_t *t = buf + hdr;
    for (int s = 0; s < NSEC; s++) {
        uint8_t *h = t + s * (16 + SS);
        h[0] = 0;                     /* C */
        h[1] = 0;                     /* H */
        h[2] = (uint8_t)(s + 1);      /* R, 1-basiert */
        h[3] = 1;                     /* N -> 128 << 1 = 256 */
        put_le16(h + 4, NSEC);        /* Sektoren auf der Spur */
        h[6] = 0x00;                  /* Dichte: doppelt */
        h[7] = 0x00;                  /* kein Deleted-Mark */
        h[8] = 0x00;                  /* FDC-Status: in Ordnung */
        put_le16(h + 14, (uint16_t)SS);
        memset(h + 16, 0x00, SS);
        h[16] = (uint8_t)(0xE0 + s);  /* Wiedererkennungsmarke */
    }

    FILE *f = fopen(path, "wb");
    if (!f) { free(buf); return 0; }
    int ok = fwrite(buf, 1, total, f) == total;
    fclose(f);
    free(buf);
    return ok;
}

/**
 * Fuehrt den Erkenner ueber das Abbild — das Tor, an dem `uft_disk_open()`
 * entscheidet, ob das Plugin ueberhaupt zustaendig ist.
 *
 * Der erste Anlauf dieses Tests rief nur `plug->open()` und war deshalb
 * gruen, obwohl der Fehler dastand: `open()` liest den Kopf selbst und
 * kommt an der Schranke gar nicht vorbei. Ein Beweis, der die fragliche
 * Stelle nicht durchlaeuft, beweist nichts.
 */
static void erkennt_abbild(const uft_format_plugin_t *plug, uint32_t hdr,
                           const char *marke)
{
    char path[300];
    temp_path(path, sizeof(path), marke);
    ASSERT(build(path, hdr));

    FILE *f = fopen(path, "rb");
    ASSERT(f != NULL);
    static uint8_t roh[HDR_164 + 4096];
    size_t gelesen = fread(roh, 1, sizeof(roh), f);
    fseek(f, 0, SEEK_END);
    long dateigroesse = ftell(f);
    fclose(f);
    ASSERT(gelesen >= HDR_160);

    ASSERT(plug->probe != NULL);
    int konfidenz = 0;
    ASSERT(plug->probe(roh, gelesen, (size_t)dateigroesse, &konfidenz) == true);
    ASSERT(konfidenz > 0);
    remove(path);
}

/** Liest Spur 0 ueber das echte Plugin und prueft beide Sektoren. */
static void liest_spur_null(const uft_format_plugin_t *plug, uint32_t hdr,
                            const char *marke)
{
    char path[300];
    temp_path(path, sizeof(path), marke);
    ASSERT(build(path, hdr));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(plug->open(&disk, path, true) == UFT_OK);

    uft_track_t tr;
    memset(&tr, 0, sizeof(tr));
    ASSERT(plug->read_track(&disk, 0, 0, &tr) == UFT_OK);
    ASSERT(tr.sector_count == NSEC);
    ASSERT(tr.sectors[0].data && tr.sectors[0].data[0] == 0xE0);
    ASSERT(tr.sectors[1].data && tr.sectors[1].data[0] == 0xE1);

    free_sectors(&tr);
    if (plug->close) plug->close(&disk);
    remove(path);
}

TEST(d88_erkennt_kopf_688) { erkennt_abbild(&uft_format_plugin_d88, HDR_164, "d88p688"); }
TEST(d88_erkennt_kopf_672) { erkennt_abbild(&uft_format_plugin_d88, HDR_160, "d88p672"); }
TEST(d77_erkennt_kopf_688) { erkennt_abbild(&uft_format_plugin_d77, HDR_164, "d77p688"); }
TEST(d77_erkennt_kopf_672) { erkennt_abbild(&uft_format_plugin_d77, HDR_160, "d77p672"); }

TEST(d88_kopf_688_bleibt_lesbar)  { liest_spur_null(&uft_format_plugin_d88, HDR_164, "d88_688"); }
TEST(d88_kopf_672_wird_gelesen)   { liest_spur_null(&uft_format_plugin_d88, HDR_160, "d88_672"); }
TEST(d77_kopf_688_bleibt_lesbar)  { liest_spur_null(&uft_format_plugin_d77, HDR_164, "d77_688"); }
TEST(d77_kopf_672_wird_gelesen)   { liest_spur_null(&uft_format_plugin_d77, HDR_160, "d77_672"); }

int main(void)
{
    printf("=== D88/D77 Kopf-Fassungen: 688 und 672 Byte (MF-625) ===\n");
    RUN(d88_erkennt_kopf_688);
    RUN(d88_erkennt_kopf_672);
    RUN(d77_erkennt_kopf_688);
    RUN(d77_erkennt_kopf_672);
    RUN(d88_kopf_688_bleibt_lesbar);
    RUN(d88_kopf_672_wird_gelesen);
    RUN(d77_kopf_688_bleibt_lesbar);
    RUN(d77_kopf_672_wird_gelesen);
    printf("\nErgebnis: %d bestanden, %d gescheitert\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

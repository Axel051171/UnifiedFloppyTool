/* IMD: the sector IDs the plugin reports are the IDs in the file (P0-18,
 * MF-1348).
 *
 * REFERENCE: MAME src/lib/formats/imd_dsk.cpp (BSD-3-Clause, read only,
 * neue-ideen/formats1.zip), imd_format::load():
 *     sects[i].track  = tnum.size() ? tnum[i] : track;   cylinder map, 0x80
 *     sects[i].head   = hnum.size() ? hnum[i] : head;    head map, 0x40
 *     sects[i].sector = snum[i];                         numbering map
 *     stype 0 -> sects[i].data = nullptr                 no data
 * (the same source docs/spec_verification.json names for `imd`).
 *
 * What was wrong (measured, P0-18): the plugin passed snum[i] to
 * uft_format_add_sector(), which expects a 0-based INDEX and adds 1 — so
 * every ID of every IMD file came out one too high (hxcfe_pc160.imd: map
 * 1..8, reported 2..9). A dtype-0 sector ("data unavailable") came out as a
 * GOOD sector full of 0xE5. And the cylinder/head maps were skipped: the ID
 * named the physical position instead of what the sector header says.
 *
 * Why no test saw it: the IMD tests find sectors by a tag byte in their
 * CONTENT, and MF-1068 compared sector COUNTS against real recordings —
 * never the IDs.
 *
 * The foreign file is parsed here by an independent walker (not the
 * plugin), so the test does not ask the same source as the unit under test.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_FREE_DIR
#define UFT_CORPUS_FREE_DIR "tests/corpus_free"
#endif

extern const uft_format_plugin_t uft_format_plugin_imd;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) {
        printf("  [ok ] %s\n", was);
        gruen++;
    } else {
        printf("  [ROT] %s%s%s\n", was, hinweis ? " -- " : "", hinweis ? hinweis : "");
        rot++;
    }
}

static uint8_t *datei_lesen(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long l = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = (l > 0) ? (uint8_t *)malloc((size_t)l) : NULL;
    if (d && fread(d, 1, (size_t)l, f) != (size_t)l) { free(d); d = NULL; }
    fclose(f);
    *n = d ? (size_t)l : 0;
    return d;
}

static void spur_frei(uft_track_t *t)
{
    for (size_t i = 0; i < t->sector_count; i++) free(t->sectors[i].data);
    free(t->sectors);
    t->sectors = NULL;
    t->sector_count = 0;
}

/* --- 1. the foreign file, every track, every ID ------------------------ */

static void t_fremde_datei(void)
{
    puts("1. hxcfe_pc160.imd (fremd erzeugt): jede ID wie in der Nummernkarte");
    const char *pfad = UFT_CORPUS_FREE_DIR "/hxcfe_pc160.imd";
    size_t n = 0;
    uint8_t *d = datei_lesen(pfad, &n);
    if (!d) { pruefe("Korpusdatei lesbar", 0, pfad); return; }

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_imd.open(&disk, pfad, true) != UFT_OK) {
        pruefe("open", 0, pfad);
        free(d);
        return;
    }

    /* independent walk over the track records */
    size_t pos = 0;
    while (pos < n && d[pos] != 0x1A) pos++;
    pos++;
    unsigned spuren = 0, ids = 0, gleich = 0, anzahl_gleich = 0;
    char erste_abweichung[128] = "";
    while (pos + 5 <= n) {
        const uint8_t cyl = d[pos + 1], kopf = d[pos + 2], nsec = d[pos + 3];
        const uint8_t code = d[pos + 4];
        const size_t groesse = (size_t)128 << (code < 7 ? code : 6);
        const uint8_t *karte = d + pos + 5;
        pos += 5 + nsec;
        if (kopf & 0x80) pos += nsec;
        if (kopf & 0x40) pos += nsec;
        for (unsigned s = 0; s < nsec && pos < n; s++) {
            const uint8_t typ = d[pos++];
            if (typ == 0) continue;
            pos += (typ % 2 == 0) ? 1 : groesse;
        }

        uft_track_t t;
        memset(&t, 0, sizeof t);
        uft_format_plugin_imd.read_track(&disk, cyl, kopf & 0x0F, &t);
        spuren++;
        if (t.sector_count == nsec) anzahl_gleich++;
        for (unsigned s = 0; s < nsec && s < t.sector_count; s++) {
            ids++;
            if (t.sectors[s].id.sector == karte[s]) gleich++;
            else if (!erste_abweichung[0])
                snprintf(erste_abweichung, sizeof erste_abweichung,
                         "C%u H%u Platz %u: Datei %u, Plugin %u", cyl,
                         kopf & 0x0F, s, karte[s], t.sectors[s].id.sector);
        }
        spur_frei(&t);
    }
    uft_format_plugin_imd.close(&disk);
    free(d);

    char h[160];
    snprintf(h, sizeof h, "%u Spuren, %u mit richtiger Sektorzahl", spuren, anzahl_gleich);
    pruefe("jede Spur traegt so viele Sektoren wie die Datei", spuren > 0 && anzahl_gleich == spuren, h);
    snprintf(h, sizeof h, "%u von %u IDs gleich; %s", gleich, ids,
             erste_abweichung[0] ? erste_abweichung : "keine Abweichung");
    pruefe("jede ID ist die ID der Nummernkarte", ids > 0 && gleich == ids, h);
}

/* --- synthetic files ---------------------------------------------------- */

static int imd_schreiben(const char *pfad, uint8_t cyl, uint8_t kopfbyte,
                         uint8_t nsec, const uint8_t *karte,
                         const uint8_t *zkarte, const uint8_t *kkarte,
                         const uint8_t *typen)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    const char *kommentar = "IMD 1.18: uft sector id test\r\n";
    fwrite(kommentar, 1, strlen(kommentar), f);
    fputc(0x1A, f);
    fputc(5, f); fputc(cyl, f); fputc(kopfbyte, f); fputc(nsec, f); fputc(0, f);
    fwrite(karte, 1, nsec, f);
    if (kopfbyte & 0x80) fwrite(zkarte, 1, nsec, f);
    if (kopfbyte & 0x40) fwrite(kkarte, 1, nsec, f);
    for (unsigned s = 0; s < nsec; s++) {
        fputc(typen[s], f);
        if (typen[s] == 0) continue;
        if (typen[s] % 2 == 0) { fputc(0x40 + (int)s, f); continue; }
        fputc(0xA0 + (int)s, f);
        for (int i = 1; i < 128; i++) fputc((int)s, f);
    }
    fclose(f);
    return 1;
}

static void wegwerf_pfad(char *buf, size_t n, const char *name)
{
    static unsigned lauf = 0;
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(buf, n, "%s/uft_%s_%u.imd", d, name, lauf++);
}

static int lesen(const char *pfad, int cyl, int head, uft_track_t *t)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    memset(t, 0, sizeof *t);
    if (uft_format_plugin_imd.open(&disk, pfad, true) != UFT_OK) return 0;
    const int rc = uft_format_plugin_imd.read_track(&disk, cyl, head, t);
    uft_format_plugin_imd.close(&disk);
    return rc == UFT_OK;
}

static void t_nicht_verfuegbar_und_fremde_nummern(void)
{
    puts("2. Nummernkarte 0, 0xC1, 7, 3; Platz 4 'data unavailable' (dtype 0)");
    char pfad[1024];
    wegwerf_pfad(pfad, sizeof pfad, "imd_ids");
    const uint8_t karte[4] = { 0x00, 0xC1, 0x07, 0x03 };
    const uint8_t typen[4] = { 1, 2, 3, 0 };   /* normal, compressed, deleted, unavailable */
    if (!imd_schreiben(pfad, 0, 0, 4, karte, NULL, NULL, typen)) {
        pruefe("Wegwerf-Datei anlegbar", 0, pfad);
        return;
    }
    uft_track_t t;
    const int ok = lesen(pfad, 0, 0, &t);
    remove(pfad);
    pruefe("gelesen, vier Sektoren", ok && t.sector_count == 4, NULL);
    if (!ok || t.sector_count != 4) { spur_frei(&t); return; }

    char h[96];
    snprintf(h, sizeof h, "%u %u %u %u", t.sectors[0].id.sector, t.sectors[1].id.sector,
             t.sectors[2].id.sector, t.sectors[3].id.sector);
    pruefe("IDs 0, 0xC1, 7, 3 wie in der Datei",
           t.sectors[0].id.sector == 0x00 && t.sectors[1].id.sector == 0xC1 &&
           t.sectors[2].id.sector == 0x07 && t.sectors[3].id.sector == 0x03, h);

    const uft_sector_t *weg = &t.sectors[3];
    snprintf(h, sizeof h, "status=0x%x crc_ok=%d", (unsigned)weg->status, (int)weg->crc_ok);
    pruefe("'data unavailable' ist als fehlend gekennzeichnet",
           (weg->status & UFT_SECTOR_MISSING) != 0, h);
    pruefe("'data unavailable' traegt keine gute Pruefsumme", !weg->crc_ok, h);
    pruefe("die gelesenen Sektoren bleiben gut",
           t.sectors[0].crc_ok && (t.sectors[0].status & UFT_SECTOR_MISSING) == 0, NULL);
    pruefe("der geloeschte Sektor bleibt geloescht", t.sectors[2].deleted, NULL);
    spur_frei(&t);
}

static void t_zylinder_und_kopfkarte(void)
{
    puts("3. Zylinder- und Kopfkarte: die ID nennt, was im Sektorkopf steht");
    char pfad[1024];
    wegwerf_pfad(pfad, sizeof pfad, "imd_karten");
    const uint8_t karte[2] = { 1, 2 };
    const uint8_t zkarte[2] = { 5, 6 };
    const uint8_t kkarte[2] = { 1, 1 };
    const uint8_t typen[2] = { 1, 1 };
    /* physical cylinder 2, head 0, both maps present */
    if (!imd_schreiben(pfad, 2, 0x00 | 0x80 | 0x40, 2, karte, zkarte, kkarte, typen)) {
        pruefe("Wegwerf-Datei anlegbar", 0, pfad);
        return;
    }
    uft_track_t t;
    const int ok = lesen(pfad, 2, 0, &t);
    remove(pfad);
    pruefe("gelesen, zwei Sektoren", ok && t.sector_count == 2, NULL);
    if (!ok || t.sector_count != 2) { spur_frei(&t); return; }
    char h[96];
    snprintf(h, sizeof h, "C%u H%u / C%u H%u", t.sectors[0].id.cylinder, t.sectors[0].id.head,
             t.sectors[1].id.cylinder, t.sectors[1].id.head);
    pruefe("Zylinder aus der Zylinderkarte (5, 6), nicht die Lage 2",
           t.sectors[0].id.cylinder == 5 && t.sectors[1].id.cylinder == 6, h);
    pruefe("Kopf aus der Kopfkarte (1), nicht die Lage 0",
           t.sectors[0].id.head == 1 && t.sectors[1].id.head == 1, h);
    spur_frei(&t);

    /* ONE map at a time: with both present, reading the head-map flag for
     * the cylinder map went unnoticed (mutation run, MF-1348). */
    wegwerf_pfad(pfad, sizeof pfad, "imd_nur_zylinderkarte");
    if (!imd_schreiben(pfad, 2, 0x00 | 0x80, 2, karte, zkarte, NULL, typen)) {
        pruefe("Wegwerf-Datei anlegbar", 0, pfad);
        return;
    }
    int ok1 = lesen(pfad, 2, 0, &t);
    remove(pfad);
    snprintf(h, sizeof h, "C%u H%u", ok1 && t.sector_count ? t.sectors[1].id.cylinder : 99,
             ok1 && t.sector_count ? t.sectors[1].id.head : 99);
    pruefe("nur Zylinderkarte: C6 aus der Karte, H0 aus der Lage",
           ok1 && t.sector_count == 2 && t.sectors[1].id.cylinder == 6 &&
           t.sectors[1].id.head == 0 && t.sectors[1].data &&
           t.sectors[1].data[0] == 0xA1, h);
    spur_frei(&t);

    wegwerf_pfad(pfad, sizeof pfad, "imd_nur_kopfkarte");
    if (!imd_schreiben(pfad, 2, 0x00 | 0x40, 2, karte, NULL, kkarte, typen)) {
        pruefe("Wegwerf-Datei anlegbar", 0, pfad);
        return;
    }
    ok1 = lesen(pfad, 2, 0, &t);
    remove(pfad);
    snprintf(h, sizeof h, "C%u H%u", ok1 && t.sector_count ? t.sectors[1].id.cylinder : 99,
             ok1 && t.sector_count ? t.sectors[1].id.head : 99);
    pruefe("nur Kopfkarte: C2 aus der Lage, H1 aus der Karte, Daten an ihrer Stelle",
           ok1 && t.sector_count == 2 && t.sectors[1].id.cylinder == 2 &&
           t.sectors[1].id.head == 1 && t.sectors[1].data &&
           t.sectors[1].data[0] == 0xA1, h);
    spur_frei(&t);

    /* without maps the ID keeps the physical position */
    wegwerf_pfad(pfad, sizeof pfad, "imd_ohne_karten");
    if (!imd_schreiben(pfad, 2, 0x01, 2, karte, NULL, NULL, typen)) {
        pruefe("Wegwerf-Datei anlegbar", 0, pfad);
        return;
    }
    const int ok2 = lesen(pfad, 2, 1, &t);
    remove(pfad);
    snprintf(h, sizeof h, "C%u H%u", ok2 && t.sector_count ? t.sectors[0].id.cylinder : 99,
             ok2 && t.sector_count ? t.sectors[0].id.head : 99);
    pruefe("ohne Karten: ID = physische Lage (C2 H1)",
           ok2 && t.sector_count == 2 && t.sectors[0].id.cylinder == 2 &&
           t.sectors[0].id.head == 1, h);
    spur_frei(&t);
}

int main(void)
{
    puts("=== IMD: die gemeldete ID ist die ID der Datei (P0-18, Referenz MAME imd_dsk.cpp) ===");
    t_fremde_datei();
    t_nicht_verfuegbar_und_fremde_nummern();
    t_zylinder_und_kopfkarte();
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

/**
 * @file test_cas_gegen_castools.c
 * @brief `cas` gegen ein MSX-Kassettenabbild von FREMDER Hand (MF-1223)
 *
 * ERZEUGER (Kanal *Oracle* nach MF-695 — ausgefuehrt, nicht portiert):
 *   `wav2cas` aus `joyrex2001/castools`, gebaut mit `mingw32-make`
 *   (gcc 13.1.0, erster Versuch rc 0, vier Binaries).
 *   Lizenz: **GPL-2** — `COPYING` IM PAKET gemessen (18 009 Byte), nicht
 *   nur am Ursprung nachgelesen. Dieselbe Klasse wie `hxcfe`: ausfuehren
 *   ja, uebernehmen nein.
 *
 * WARUM `cas` ueberhaupt auf T2 STAND, und was gefehlt hat:
 *   MF-1040 hat den Leser gegen MAMEs `fmsx_cas.cpp` abgenommen und drei
 *   stille Verluste behoben (256-Block-Grenze, 65 535-Kuerzung, ein
 *   leerer Block mit Status OK — dahinter eine Heap-Korruption). Die
 *   Stufe blieb T2 mit einer benannten Begruendung: „MAMEs Lader wandelt
 *   CAS in WAV-Abtastwerte und gibt keine Bloecke zurueck" — es gab
 *   keinen fremden ERZEUGER. Genau der fehlt hier nicht mehr.
 *
 * ABSTAMMUNG — UND WARUM MAMEs `imgtool` HIER NICHT GENUEGT:
 *   MAMEs `imgtool` kennt ein `fmsx_cas`-Modul und koennte schreiben —
 *   aber sein Code IST die Referenz, gegen die UFTs Leser abgenommen
 *   wurde. Das waere die Falle aus MF-1135 (`dms`): dort stammten UFTs
 *   Leser UND hxcfes Leser aus xDMS, jeder Abgleich war derselbe Kreis
 *   mit zwei Namen. `castools` ist unabhaengig: `cas2wav.c:69`
 *   deklariert die Kennung `1F A6 DE BA CC 13 7D 74` SELBST, und
 *   `wav2cas.c` nennt die MSX-Blocktypen (0xD0/0xD3/0xEA) **0 Mal** —
 *   es ist ein reiner Signaldekoder und deutet keinen Inhalt.
 *
 * DER UMWEG IST DER BELEG (Klasse MF-1084):
 *   Ein Werkzeug, das `.cas` nach `.cas` schreibt, koennte die Bytes
 *   durchreichen. Der Weg hier fuehrt durch eine AUDIO-WELLENFORM:
 *     Eingabe (1584 B)  --cas2wav-->  1 853 036 B WAV
 *     WAV               --wav2cas-->  dieses Abbild (1584 B)
 *   Wer das kann, modelliert FSK (1200/2400 Hz), die 11-Bit-Rahmung und
 *   die Blockkoepfe wirklich. Gemessen kommen die Bytes **0 abweichend**
 *   zurueck, und `wav2cas` protokolliert dabei, was es gesehen hat
 *   (`header detected`, `data block`).
 *
 * DREI HAENDE, KEIN SELBSTGESPRAECH (MF-1028):
 *   Die Eingabe ist UFT-eigen und benennt sich selbst — je Datenblock
 *   512 Byte mit der Marke `UFT-K CAS Bnn ` alle 14 Byte. Dass sie
 *   WOHLGEFORMT ist, sagt nicht UFT, sondern `casdir` (drittes Werkzeug
 *   des Pakets, eigener Leser): es listet `UFTKOR  binary` und die
 *   beiden Datenbloecke.
 *
 * REPRODUZIERBAR: `tests/corpus_manifest/gen_cas_corpus.py`
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_cas;

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

#define BILD        "castools_wav2cas_uftk.cas"

/* Alle Zahlen an der Datei gemessen, nicht angenommen. */
#define GROESSE     1584u
#define BLOECKE     4u      /* 1 Kopfblock + 3 Datenbloecke */
#define KOPF_LEN    16u     /* 10 x 0xD0 + "UFTKOR" */
#define DATEN_LEN   512u
#define MARKE_LEN   14u     /* "UFT-K CAS B00 " */
#define MARKEN      36u     /* 512 = 36 x 14 + 8 */

static const uint8_t CAS_SYNC[8] = {
    0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74
};

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  [OK]   %s\n", was); }
    else    { rot++;   printf("  [ROT]  %s\n         -> %s\n", was, detail); }
}

static uint8_t *lies(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long g = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (g <= 0) { fclose(f); return NULL; }
    uint8_t *b = malloc((size_t)g);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)g, f) != (size_t)g) { free(b); fclose(f); return NULL; }
    fclose(f);
    *n = (size_t)g;
    return b;
}

static void temp_pfad(char *aus, size_t n, const char *marke)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(aus, n, "%s/uft_cas_%s_%d.cas", dir, marke, rand() % 100000);
}

static void spur_freigeben(uft_track_t *tr)
{
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors); tr->sectors = NULL; tr->sector_count = 0;
}

/* Zaehlt vollstaendige Vorkommen von `marke` in `daten`. 512 ist KEIN
 * Vielfaches von 14, die Phase wandert also — deshalb wird gesucht und
 * nicht ab Versatz 0 durchverglichen (Lehre aus MF-1149, wo eine
 * 30-Byte-Marke gegen 512 genau die Haelfte traf). */
static unsigned marken_zaehlen(const uint8_t *daten, size_t len,
                               const char *marke)
{
    size_t ml = strlen(marke);
    unsigned n = 0;
    if (!daten || len < ml) return 0;
    for (size_t i = 0; i + ml <= len; i++)
        if (memcmp(daten + i, marke, ml) == 0) n++;
    return n;
}

int main(void)
{
    char pfad[512];
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, BILD);
    printf("== cas gegen castools wav2cas (joyrex2001, GPL-2)\n");
    printf("   Abbild: %s\n", pfad);

    size_t n = 0;
    uint8_t *roh = lies(pfad, &n);
    pruefe("Korpusdatei lesbar", roh != NULL, pfad);
    if (!roh) { printf("\n  %d gruen, %d rot\n", gruen, rot); return 1; }
    pruefe("Groesse 1584 Byte", n == GROESSE, "andere Groesse");

    char d[200];

    /* 1. Die Kennung — und zwar die, die castools SELBST deklariert. */
    pruefe("Datei beginnt mit der MSX-CAS-Kennung",
           n >= 8 && memcmp(roh, CAS_SYNC, 8) == 0, "Kennung fehlt");
    unsigned syncs = 0;
    for (size_t i = 0; i + 8 <= n; i++)
        if (memcmp(roh + i, CAS_SYNC, 8) == 0) syncs++;
    snprintf(d, sizeof(d), "%u Marken gefunden, erwartet %u", syncs, BLOECKE);
    pruefe("4 Sync-Marken = 4 Bloecke", syncs == BLOECKE, d);

    int konf = 0;
    pruefe("Sonde nimmt das FREMDE Abbild an",
           uft_format_plugin_cas.probe(roh, n, n, &konf) == true,
           "probe() sagte nein");
    snprintf(d, sizeof(d), "Konfidenz %d", konf);
    pruefe("Konfidenz im Merkmalsband (>= 50, MF-729)", konf >= 50, d);

    /* 2. Geometrie: ein Band ist ein Zylinder, ein Block ein Sektor. */
    uft_disk_t disk; memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    pruefe("open() nimmt das fremde Abbild an",
           uft_format_plugin_cas.open(&disk, pfad, true) == UFT_OK,
           "open() hat abgesagt");
    if (!disk.plugin_data) {
        free(roh);
        printf("\n  %d gruen, %d rot\n", gruen, rot);
        return 1;
    }
    pruefe("1 Zylinder, 1 Kopf (Band, keine Diskette)",
           disk.geometry.cylinders == 1 && disk.geometry.heads == 1,
           "Geometrie weicht ab");
    snprintf(d, sizeof(d), "sectors=%d total=%u", disk.geometry.sectors,
             (unsigned)disk.geometry.total_sectors);
    pruefe("4 Sektoren = 4 Bloecke",
           disk.geometry.sectors == (int)BLOECKE
           && disk.geometry.total_sectors == BLOECKE, d);

    /* 3. Der Beleg: jeder Block traegt seine eigene Marke. */
    uft_track_t t; memset(&t, 0, sizeof(t));
    pruefe("read_track(0,0) liefert die Bloecke",
           uft_format_plugin_cas.read_track(&disk, 0, 0, &t) == UFT_OK,
           "read_track hat abgesagt");
    snprintf(d, sizeof(d), "%zu Sektoren", t.sector_count);
    pruefe("4 Sektoren im Spurobjekt", t.sector_count == BLOECKE, d);

    if (t.sector_count == BLOECKE) {
        /* Kopfblock: 10 x 0xD0 + "UFTKOR" — die MSX-Konvention fuer einen
         * Binaerkopf, an der `casdir` den Namen erkennt. */
        const uft_sector_t *s0 = &t.sectors[0];
        int kopf_ok = (s0->data_len == KOPF_LEN && s0->data != NULL);
        for (unsigned i = 0; kopf_ok && i < 10; i++)
            if (s0->data[i] != 0xD0) kopf_ok = 0;
        if (kopf_ok) kopf_ok = (memcmp(s0->data + 10, "UFTKOR", 6) == 0);
        snprintf(d, sizeof(d), "data_len=%zu", s0->data_len);
        pruefe("Block 0 ist der Binaerkopf (10 x 0xD0 + UFTKOR)", kopf_ok, d);

        unsigned getroffen = 0, marken_gesamt = 0, falsche_laenge = 0;
        for (unsigned b = 1; b < BLOECKE; b++) {
            const uft_sector_t *s = &t.sectors[b];
            char marke[MARKE_LEN + 1];
            snprintf(marke, sizeof(marke), "UFT-K CAS B%02u ", b - 1);
            if (s->data_len != DATEN_LEN) { falsche_laenge++; continue; }
            if (s->data && memcmp(s->data, marke, MARKE_LEN) == 0) getroffen++;
            marken_gesamt += marken_zaehlen(s->data, s->data_len, marke);
        }
        snprintf(d, sizeof(d), "%u von 3 Bloecken an ihrer Marke, %u mit "
                 "falscher Laenge", getroffen, falsche_laenge);
        pruefe("3 von 3 Datenbloecken an ihrer eigenen Marke",
               getroffen == BLOECKE - 1 && falsche_laenge == 0, d);
        snprintf(d, sizeof(d), "%u Marken, erwartet %u", marken_gesamt,
                 MARKEN * (BLOECKE - 1));
        pruefe("108 Marken insgesamt (36 je Block, 512 = 36 x 14 + 8)",
               marken_gesamt == MARKEN * (BLOECKE - 1), d);

        /* Die `+1`-Konvention von `uft_format_add_sector()` (MF-1016)
         * gehoert gemessen, nicht angenommen: fuer ein BAND gibt es keine
         * Sektornummern, die Zahl ist UFTs eigene Buchhaltung. */
        snprintf(d, sizeof(d), "IDs %u..%u",
                 (unsigned)t.sectors[0].id.sector,
                 (unsigned)t.sectors[BLOECKE - 1].id.sector);
        pruefe("Sektornummern 1..4 — UFTs eigene Zaehlung, nicht die des Bandes",
               t.sectors[0].id.sector == 1
               && t.sectors[BLOECKE - 1].id.sector == BLOECKE, d);
    }
    spur_freigeben(&t);
    if (uft_format_plugin_cas.close) uft_format_plugin_cas.close(&disk);

    /* 4. Anti-Tautologie: eine gekippte Sync-Marke MUSS die Blockzahl
     *    aendern. Ohne diese Probe koennte 3. gruen sein, weil der
     *    Blocklauf gar nicht unterscheidet (Klasse MF-1014/MF-1026). */
    {
        char tmp[512];
        temp_pfad(tmp, sizeof(tmp), "kaputt");
        uint8_t *kopie = malloc(n);
        int gebaut = 0;
        if (kopie) {
            memcpy(kopie, roh, n);
            size_t treffer = 0, stelle = 0;
            for (size_t i = 0; i + 8 <= n; i++)
                if (memcmp(kopie + i, CAS_SYNC, 8) == 0) {
                    if (++treffer == 3) { stelle = i; break; }
                }
            if (treffer == 3) {
                kopie[stelle + 3] ^= 0xFF;
                FILE *f = fopen(tmp, "wb");
                if (f) {
                    gebaut = (fwrite(kopie, 1, n, f) == n);
                    fclose(f);
                }
            }
            free(kopie);
        }
        if (gebaut) {
            uft_disk_t d2; memset(&d2, 0, sizeof(d2)); d2.read_only = true;
            uft_error_t rc = uft_format_plugin_cas.open(&d2, tmp, true);
            snprintf(d, sizeof(d), "rc=%d sectors=%d", (int)rc,
                     d2.geometry.sectors);
            pruefe("eine gekippte Sync-Marke senkt die Blockzahl auf 3",
                   rc == UFT_OK && d2.geometry.sectors == (int)BLOECKE - 1, d);
            if (d2.plugin_data && uft_format_plugin_cas.close)
                uft_format_plugin_cas.close(&d2);
            remove(tmp);
        } else {
            pruefe("Gegenprobe baubar", 0, "Kopie liess sich nicht schreiben");
        }
    }

    free(roh);
    printf("\n  %d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}

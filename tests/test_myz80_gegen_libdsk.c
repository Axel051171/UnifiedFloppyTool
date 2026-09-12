/**
 * @file test_myz80_gegen_libdsk.c
 * @brief MYZ80: die Kennung, die UFT suchte, steht in keiner Datei (MF-1029)
 *
 * ── Die Quelle ──────────────────────────────────────────────────────
 *
 * `tools/uft-scout/work/libdsk/lib/drvmyz80.c` (John Elliott,
 * **LGPL-2+**; **nur gelesen**, Kanal *Spec* nach MF-695). Vier
 * Aussagen tragen diesen Test:
 *
 *   Z.  82-91   `myz80_open()` liest 256 Byte und verlangt, dass
 *               **jedes einzelne `0xE5`** ist. Es gibt **keine**
 *               Kennung.
 *   Z. 288-297  `myz80_getgeom()`: **64** Zylinder, **1** Kopf,
 *               **128** Sektoren, `dg_secbase = 0` (**0-basiert**),
 *               **1024** Byte je Sektor.
 *   Z. 178      `offset = (131072L * cylinder) + (1024L * sector) + 256`
 *   Z. 182-190  **Kurze Dateien sind gueltig:** „MYZ80 disc files can
 *               be shorter than the full 8Mb. If so, the missing
 *               sectors are all assumed to be full of 0xE5s. Unlike in
 *               'raw' files, it is not an error to try to read a
 *               missing sector."
 *
 * ── Was der Vorzustand war ──────────────────────────────────────────
 *
 * **UFT konnte keine einzige MYZ80-Datei lesen, und der Grund war
 * zwingend.** `include/uft/formats/uft_myz80.h` beschrieb eine
 * `myz80_header_t` mit `magic[6] = "MYZ80 "`, `version`, `flags`,
 * `cylinders`, `heads`, `sectors`, `sector_size`, `first_sector`,
 * `label[32]`, `comment[64]` und 142 Byte Polsterung. **Nichts davon
 * existiert.** Die ersten 256 Byte einer echten Datei sind durchgehend
 * `0xE5`, also scheiterte `memcmp(header->magic, "MYZ80 ", 6)` immer —
 * und der Groessenrueckfall der Sonde kannte nur 256256 und 1025024
 * Byte (77 x 2 x 26 x 128, eine 8-Zoll-CP/M-Geometrie ohne Bezug zu
 * MYZ80). Eine volle MYZ80-Datei ist **8388864** Byte gross.
 *
 * Dazu die Geometrie: angenommen 77 x 2 x 26 x 128, wirklich
 * **64 x 1 x 128 x 1024**.
 *
 * Das ist die Klasse von **MF-961** (`86f` probte auf `"86BX"`, ein
 * Magic, das in keiner echten Datei steht) und **MF-1022** (`sap`
 * suchte `"SAP"` bei Versatz 0, wo das Formatbyte steht) — zum
 * **dritten** Mal. Und wie bei `qrst` (MF-1028) nannte der alte
 * Dateikopf `libdsk drvmyz80.c ... Fassung 1.5.12 geprueft`.
 *
 * ── Warum dieses Fixture kein Selbstgespraech ist ───────────────────
 *
 * `tests/corpus_free/myz80_spec_1zyl.myz80` ist von UFT nach der
 * Vorlage gebaut, aber ihr Bytelayout ist von **fremder Hand
 * nachgewiesen**:
 *
 *     dskid -type myz80    ->  64 Zylinder, 1 Kopf, 128 Sektoren,
 *                              1024 Byte, „First sector: 0"
 *     dsktrans -itype myz80 -otype raw
 *                          ->  8388608 Byte; Zylinder 0 **byteidentisch**
 *                              (131072 von 131072), Zylinder 1..63
 *                              **zu 100 % 0xE5**
 *
 * Damit sind die Versatzformel UND die Kurzdatei-Regel von einer
 * unabhaengigen Umsetzung bestaetigt. **T2 und nicht T1b**, weil T1b
 * einen fremden *Erzeuger* verlangt.
 *
 * Die Datei traegt absichtlich nur **einen** Zylinder (131328 statt
 * 8388864 Byte) — damit prueft sie die Kurzdatei-Regel gleich mit.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

extern const uft_format_plugin_t uft_format_plugin_myz80;

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

#define FIXTURE "myz80_spec_1zyl.myz80"
#define KOPF   256
#define ZYL    64
#define SPT    128
#define SS     1024
#define ZYL_IN_DATEI 1

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  [OK]   %s\n", was); }
    else { rot++; printf("  [ROT]  %s\n         -> %s\n", was, detail); }
}

static uint8_t *lies(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    uint8_t *b;
    long len;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return NULL; }
    b = (uint8_t *)malloc((size_t)len);
    if (!b || fread(b, 1, (size_t)len, f) != (size_t)len) {
        free(b); fclose(f); return NULL;
    }
    fclose(f);
    *n = (size_t)len;
    return b;
}

static int schreibe(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    if (fwrite(b, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_myz80;
    const char *tmp = getenv("TEMP");
    char pfad[600];
    uint8_t *datei;
    size_t n;

    printf("MYZ80 gegen libdsk drvmyz80.c (LGPL-2+, nur gelesen)\n");
    printf("=====================================================\n");

    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, FIXTURE);
    datei = lies(pfad, &n);
    if (!datei) {
        printf("  [SKIP] %s fehlt im Korpus\n", pfad);
        printf("\n%d gruen, %d rot, 1 uebersprungen\n", gruen, rot);
        return 0;
    }
    printf("  Fixture: %s (%zu Byte)\n\n", FIXTURE, n);

    /* ── 1. Der reservierte Bereich ist durchgehend 0xE5 ───────────── */
    {
        size_t i, e5 = 0;
        char d[160];
        for (i = 0; i < KOPF && i < n; i++)
            if (datei[i] == 0xE5) e5++;
        snprintf(d, sizeof(d), "%zu von %d Byte sind 0xE5; Groesse %zu "
                 "(volle Datei waere %d)", e5, KOPF, n,
                 KOPF + ZYL * SPT * SS);
        pruefe("Fixture: alle 256 Kopfbytes sind 0xE5, und es ist eine "
               "KURZE Datei (ein Zylinder)",
               e5 == KOPF && n == (size_t)(KOPF + ZYL_IN_DATEI * SPT * SS),
               d);
    }

    /* ── 2. Die Sonde ──────────────────────────────────────────────── */
    {
        int conf = -1;
        bool ja = p->probe(datei, n < 4096 ? n : 4096, n, &conf);
        char d[140];
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        /* MF-729: 256 gleiche Bytes sind eine Konvention, keine
         * Signatur — also Band „Struktur gelesen" (50..79), nicht
         * „Merkmal getroffen". */
        pruefe("Sonde nimmt an, Konfidenz im Band 50..79 (eine "
               "Konvention, keine Kennung)",
               ja && conf >= 50 && conf < 80, d);
    }

    /* ── 3. Gegenprobe: EIN Byte im Kopf ungleich 0xE5 ─────────────── */
    {
        uint8_t *kaputt = (uint8_t *)malloc(n);
        int conf = -1;
        bool ja;
        char d[160];
        memcpy(kaputt, datei, n);
        kaputt[200] = 0xE4;      /* ein einziges Byte */
        ja = p->probe(kaputt, 4096, n, &conf);
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("Gegenprobe: EIN Byte ungleich 0xE5 im Kopf -> ABGEWIESEN "
               "(libdsk prueft alle 256)", !ja, d);
        free(kaputt);
    }

    /* ── 4. Gegenprobe: die erfundene Kennung wird NICHT angenommen ── */
    {
        uint8_t *kaputt = (uint8_t *)malloc(n);
        int conf = -1;
        bool ja;
        char d[190];
        memcpy(kaputt, datei, n);
        memcpy(kaputt, "MYZ80 ", 6);   /* was UFT bis MF-1029 suchte */
        ja = p->probe(kaputt, 4096, n, &conf);
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("Gegenprobe: eine Datei, die mit \"MYZ80 \" beginnt, wird "
               "ABGEWIESEN — genau diese Kennung suchte UFT, und sie "
               "steht in keiner echten Datei", !ja, d);
        free(kaputt);
    }

    /* ── 5. Oeffnen: die feste Geometrie ──────────────────────────── */
    {
        uft_disk_t disk;
        uft_error_t rc;
        memset(&disk, 0, sizeof(disk));
        rc = p->open(&disk, pfad, true);
        {
            char d[200];
            snprintf(d, sizeof(d), "rc=%d, %u Zyl, %u Koepfe, %u spt, "
                     "%u Byte", (int)rc, disk.geometry.cylinders,
                     disk.geometry.heads, disk.geometry.sectors,
                     disk.geometry.sector_size);
            pruefe("open: die FESTE Geometrie 64 / 1 / 128 / 1024 — auch "
                   "bei einer kurzen Datei",
                   rc == UFT_OK && disk.geometry.cylinders == ZYL
                   && disk.geometry.heads == 1
                   && disk.geometry.sectors == SPT
                   && disk.geometry.sector_size == SS, d);
        }
        if (rc != UFT_OK) {
            printf("\n%d gruen, %d rot\n", gruen, rot);
            free(datei);
            return 1;
        }

        /* ── 6. Zylinder 0: Versatzformel und 0-basierte IDs ──────── */
        {
            uft_track_t tr;
            int s, falsche_id = 0, fremd = 0;
            char erstes[220] = "";
            memset(&tr, 0, sizeof(tr));
            if (p->read_track(&disk, 0, 0, &tr) != UFT_OK
                || (int)tr.sector_count != SPT) {
                char d[140];
                snprintf(d, sizeof(d), "%u Sektoren statt %d",
                         (unsigned)tr.sector_count, SPT);
                pruefe("Zylinder 0 liefert 128 Sektoren", 0, d);
            } else {
                for (s = 0; s < SPT; s++) {
                    const uint8_t *dd = tr.sectors[s].data;
                    if (tr.sectors[s].id.sector != (uint8_t)s) {
                        falsche_id++;
                        if (!erstes[0])
                            snprintf(erstes, sizeof(erstes),
                                     "Sektor %d hat ID %u (libdsk: "
                                     "dg_secbase = 0)", s,
                                     (unsigned)tr.sectors[s].id.sector);
                    }
                    if (!dd || dd[0] != 0 || dd[1] != 0
                        || dd[2] != (uint8_t)(s & 0xFF)
                        || dd[3] != (uint8_t)((s >> 8) & 0xFF)
                        || memcmp(dd + 4, "UFT-M", 5) != 0) {
                        fremd++;
                        if (!erstes[0] && dd)
                            snprintf(erstes, sizeof(erstes),
                                     "Sektor %d liefert Zyl %u Kopf %u "
                                     "Sektor %u", s, (unsigned)dd[0],
                                     (unsigned)dd[1],
                                     (unsigned)(dd[2] | (dd[3] << 8)));
                    }
                }
                {
                    char d[300];
                    snprintf(d, sizeof(d), "%d falsche IDs, %d fremde "
                             "Sektoren; erster: %s", falsche_id, fremd,
                             erstes[0] ? erstes : "-");
                    pruefe("Zylinder 0: 128 Sektoren, IDs 0..127, jeder "
                           "liefert seine EIGENEN Bytes (Versatz "
                           "131072*Zyl + 1024*Sektor + 256)",
                           falsche_id == 0 && fremd == 0, d);
                }
            }
            free(tr.sectors);
            free(tr.raw_data);
        }

        /* ── 7. Die Kurzdatei-Regel ────────────────────────────────── */
        {
            uft_track_t tr;
            char d[240];
            int alle_e5 = 1, alle_gekennzeichnet = 1, s, k;
            memset(&tr, 0, sizeof(tr));
            if (p->read_track(&disk, 1, 0, &tr) != UFT_OK
                || (int)tr.sector_count != SPT) {
                snprintf(d, sizeof(d), "rc nicht OK oder %u Sektoren",
                         (unsigned)tr.sector_count);
                pruefe("Zylinder 1 (nicht in der Datei) ist lesbar", 0, d);
            } else {
                for (s = 0; s < SPT; s++) {
                    const uint8_t *dd = tr.sectors[s].data;
                    if (!dd) { alle_e5 = 0; break; }
                    for (k = 0; k < SS; k++)
                        if (dd[k] != 0xE5) { alle_e5 = 0; break; }
                    if (tr.sectors[s].status == UFT_SECTOR_OK)
                        alle_gekennzeichnet = 0;
                }
                snprintf(d, sizeof(d), "alle 0xE5: %d, alle als fehlend "
                         "gekennzeichnet: %d", alle_e5,
                         alle_gekennzeichnet);
                /* libdsk: ein fehlender Sektor ist KEIN Fehler und gilt
                 * als 0xE5. UFT liefert die Bytes so — kennzeichnet sie
                 * aber, weil „das Format sagt 0xE5" und „hier wurde
                 * 0xE5 gelesen" zwei Aussagen sind (MF-980). */
                pruefe("Zylinder 1 fehlt in der Datei: er ist lesbar, "
                       "liefert 0xE5 (libdsks Regel) UND ist als fehlend "
                       "gekennzeichnet (MF-980)",
                       alle_e5 && alle_gekennzeichnet, d);
            }
            free(tr.sectors);
            free(tr.raw_data);
        }

        /* ── 8. Grenzen ────────────────────────────────────────────── */
        {
            uft_track_t tr;
            uft_error_t a, c2;
            char d[160];
            memset(&tr, 0, sizeof(tr));
            a = p->read_track(&disk, ZYL, 0, &tr);
            free(tr.sectors); free(tr.raw_data);
            memset(&tr, 0, sizeof(tr));
            c2 = p->read_track(&disk, 0, 1, &tr);
            free(tr.sectors); free(tr.raw_data);
            snprintf(d, sizeof(d), "Zylinder 64 -> rc=%d, Kopf 1 -> rc=%d",
                     (int)a, (int)c2);
            pruefe("Zylinder 64 und Kopf 1 werden ABGEWIESEN (64 Zyl, "
                   "EIN Kopf)", a != UFT_OK && c2 != UFT_OK, d);
        }

        p->close(&disk);
    }

    /* ── 9. Gegenprobe: die alten Groessen sind keine MYZ80 ──────────
     *
     * **Und diese Gegenprobe musste isoliert werden, weil die
     * Mutationsmatrix einen DRITTEN Grund offengelegt hat, warum UFT
     * MYZ80 nie lesen konnte.**
     *
     * Der erste Anlauf rief `p->probe(puffer, 4096, 256256u, ...)` —
     * die Dateigroesse also als drittes Argument. Die Mutation „die
     * alten Groessen wieder als kopflos annehmen" rutschte damit
     * **durch**, und der Grund ist lehrreich: `myz80_probe_plugin()`
     * verwirft `file_size` (`(void)file_size`) und gibt nur die
     * **Puffergroesse** weiter. Der Groessenrueckfall des alten Codes
     * verglich also `size == 256256` gegen die Groesse des
     * SONDENPUFFERS, und der ist 4096 Byte. **Der Zweig war toter
     * Code** — ein dritter, unabhaengiger Grund neben der erfundenen
     * Kennung und der falschen Geometrie.
     *
     * Isoliert wird die Zusage, indem der Puffer WIRKLICH 256256 Byte
     * gross ist. Dann trifft die Bedingung, und die Mutation faellt. */
    {
        const size_t alt_groesse = 256256u;
        uint8_t *fremd = (uint8_t *)calloc(1, alt_groesse);
        int conf = -1;
        bool ja;
        char d[200];
        assert(fremd != NULL);
        ja = p->probe(fremd, alt_groesse, alt_groesse, &conf);
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d (Puffer %zu Byte)",
                 ja, conf, alt_groesse);
        pruefe("Gegenprobe: 256256 Byte Nullen werden ABGEWIESEN — die "
               "Groesse war eine der zwei, die UFT als \"headerless "
               "MYZ80\" annahm (und der Zweig war ohnehin unerreichbar)",
               !ja, d);
        free(fremd);
    }

    /* ── 10. Eine Datei, die NUR aus dem Kopf besteht ──────────────── */
    {
        uint8_t kopf[KOPF];
        char kpfad[600];
        uft_disk_t disk;
        uft_error_t rc;
        char d[180];
        memset(kopf, 0xE5, sizeof(kopf));
        snprintf(kpfad, sizeof(kpfad), "%s/uft_myz80_leer.myz80",
                 tmp ? tmp : ".");
        if (schreibe(kpfad, kopf, sizeof(kopf))) {
            memset(&disk, 0, sizeof(disk));
            rc = p->open(&disk, kpfad, true);
            snprintf(d, sizeof(d), "rc=%d, %u Zyl", (int)rc,
                     disk.geometry.cylinders);
            /* Auch das ist eine gueltige MYZ80 — die kuerzeste. Alle
             * 8192 Sektoren gelten als 0xE5. */
            pruefe("eine Datei aus NUR 256 Byte 0xE5 ist die kuerzeste "
                   "gueltige MYZ80 und meldet 64 Zylinder",
                   rc == UFT_OK && disk.geometry.cylinders == ZYL, d);
            if (rc == UFT_OK) p->close(&disk);
            remove(kpfad);
        } else {
            pruefe("eine Datei aus NUR 256 Byte 0xE5 ist gueltig", 0,
                   "Pruefdatei liess sich nicht schreiben");
        }
    }

    /* ── MF-1033: libdsk hat die VOLLE MYZ80 GESCHRIEBEN ──────────
     *
     * 8 388 864 Byte = 256 Byte reservierter Bereich + 64 x 128 x 1024 —
     * genau die Groesse, die MF-1029 als Rueckfall gemessen hat und die
     * das vorhandene Fixture bewusst NICHT hat (es traegt einen
     * Zylinder). libdsk fuellt die fehlenden Sektoren nach seiner
     * eigenen Regel mit 0xE5.
     *
     * Bis MF-1032 galt P3-333 („libdsk erzeugt keine Fixtures"); der
     * Kanal war `-format <name>`, und fuer Formate mit eigener
     * `getgeom` braucht es ihn nicht einmal. Damit steht `myz80` auf
     * **T1b** statt T2, und die VOLLE Geometrie ist erstmals an einem
     * Fremderzeugnis geprueft. */
    {
        char fpfad[600], d[260];
        uft_disk_t da, db;
        int gleich = 0, ungleich = 0;
        size_t s;
        uft_track_t ta, tb;

        snprintf(fpfad, sizeof(fpfad), "%s/%s", UFT_CORPUS_DIR,
                 "libdsk_myz80_voll.myz80");
        memset(&da, 0, sizeof(da));
        memset(&db, 0, sizeof(db));
        snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, FIXTURE);
        if (p->open(&da, pfad, true) == UFT_OK
            && p->open(&db, fpfad, true) == UFT_OK) {
            memset(&ta, 0, sizeof(ta));
            memset(&tb, 0, sizeof(tb));
            if (p->read_track(&da, 0, 0, &ta) == UFT_OK
                && p->read_track(&db, 0, 0, &tb) == UFT_OK
                && ta.sector_count == tb.sector_count) {
                for (s = 0; s < ta.sector_count; s++) {
                    if (ta.sectors[s].data && tb.sectors[s].data
                        && ta.sectors[s].data_len == tb.sectors[s].data_len
                        && ta.sectors[s].id.sector == tb.sectors[s].id.sector
                        && memcmp(ta.sectors[s].data, tb.sectors[s].data,
                                  ta.sectors[s].data_len) == 0) gleich++;
                    else ungleich++;
                }
            }
            snprintf(d, sizeof(d), "fremde Datei %ux%ux%ux%u, Zylinder 0: "
                     "%d Sektoren gleich, %d ungleich",
                     db.geometry.cylinders, db.geometry.heads,
                     db.geometry.sectors, db.geometry.sector_size,
                     gleich, ungleich);
            pruefe("MF-1033: das von libdsk GESCHRIEBENE volle Abbild "
                   "meldet 64 x 1 x 128 x 1024, und Zylinder 0 liefert "
                   "alle 128 Sektoren byteidentisch (T1b)",
                   gleich == 128 && ungleich == 0
                   && db.geometry.cylinders == 64 && db.geometry.heads == 1
                   && db.geometry.sectors == 128
                   && db.geometry.sector_size == 1024, d);
            uft_track_release(&ta);
            uft_track_release(&tb);
            p->close(&da);
            p->close(&db);
        } else {
            pruefe("MF-1033: das von libdsk geschriebene volle Abbild "
                   "laesst sich oeffnen", 0,
                   "eine der beiden Dateien fehlt");
        }
    }

    free(datei);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

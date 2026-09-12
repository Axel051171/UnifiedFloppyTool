/**
 * @file test_qrst_gegen_libdsk.c
 * @brief QRST: der ganze Aufbau war erfunden (MF-1028)
 *
 * ── Die Quelle ──────────────────────────────────────────────────────
 *
 * `tools/uft-scout/work/libdsk/doc/qrst.html` — John Elliotts
 * Formatbeschreibung des Compaq-Formats „Quick Release Sector
 * Transfer", und dieselbe Beschreibung, die libdsks `lib/drvqrst.c`
 * umsetzt (**LGPL-2+**, John Elliott; **nur gelesen**, Kanal *Spec*
 * nach MF-695 — keine Zeile Quelltext uebernommen).
 *
 * Der Kopf ist **796 Byte** lang:
 *
 *     0000: 'QRST',0        Kennung, FUENF Byte (mit Nullbyte)
 *     0005: 0, 80h, 3Fh     unbenutzt; QRST.EXE liest sie nicht
 *     0008: Pruefsumme      LE32, Summe byte*(1+Versatz)
 *     000C: Kapazitaetskode 1=360k 2=1.2M 3=720k 4=1.4M
 *                           5=160k 6=180k 7=320k
 *     000D: Bandnummer      1-basiert
 *     000E: Bandzahl
 *     000F: Beschreibung    ASCII, 0-terminiert, bis 004A
 *     004B: Etikett         ASCII, 0-terminiert, bis 031B
 *
 * Danach Spursaetze in **drei** Arten:
 *
 *     roh     : cyl, head, 0, dann tracklen Byte
 *     leer    : cyl, head, 1, dann EIN Fuellbyte
 *     gepackt : cyl, head, 2, LE16 Laenge, dann die gepackten Bytes —
 *               **abwechselnd** Literal-Lauf `<len><bytes>` und
 *               Wiederhol-Lauf `<len><byte>`
 *
 * ── Was der Vorzustand war ──────────────────────────────────────────
 *
 * **UFT konnte KEINE echte QRST-Datei lesen, und sein Schreiber
 * erzeugte ein Format, das es nicht gibt.** Gemessen gegen die
 * Beschreibung:
 *
 *   | | UFT | wirklich |
 *   |---|---|---|
 *   | Kopfgroesse | `QRST_HEADER_SIZE 22` | **796** |
 *   | Kennung | `"QRST"`, 4 Byte | `"QRST",0`, 5 Byte |
 *   | Kopfaufbau | `version`/`cylinders`/`heads`/`sectors`/
 *                  `sector_size` als u16 ab Versatz 4 | 0x08 Pruefsumme,
 *                  **0x0C Kapazitaetskode** |
 *   | Geometrie | aus dem Kopf gelesen | aus dem **Kapazitaetskode** |
 *   | Spursatz | `cyl`(u16) `head`(u8) `compressed`(u8)
 *                `data_size`(**u32**) = 8 Byte | **3** Byte, und das
 *                dritte ist der **Typ** |
 *   | Spurarten | 0/1 (keine/RLE) | **0/1/2** |
 *   | Packung | `0x00 count value`-Strom | abwechselnde Laeufe |
 *   | Pruefsumme | fehlt ganz | LE32 bei 0x08 |
 *
 * Das ist die Klasse der fuenf fabrizierten Parser (FMT-2/3/10/11/12)
 * und, was die Packung betrifft, woertlich MF-1009: dort war es
 * `apridisk` mit „einem Byte-Strom statt drei Byte Laenge plus
 * Fuellbyte, dessen Rundlauftest gruen war, weil Packer und Entpacker
 * Spiegelbilder waren". **Hier ist es dieselbe Klasse ein zweites Mal**
 * — und der Dateikopf nannte `libdsk drvqrst.c ... Fassung 1.5.12
 * geprueft`, eine benannte Referenz, deren Verhalten der Code nicht
 * umsetzte. Gestalt von MF-1026 (`victor9k` nannte MAME und wich in
 * fuenf Punkten ab).
 *
 * ── Warum dieses Fixture kein Selbstgespraech ist ───────────────────
 *
 * `tests/corpus_free/qrst_spec_160k.qrst` ist von UFT erzeugt — aber
 * ihr Bytelayout ist **von fremder Hand nachgewiesen**, nicht
 * behauptet. libdsks aus dem Klon gebaute Werkzeuge lesen sie:
 *
 *     dskid  -type qrst  ->  40 Zylinder, 1 Kopf, 8 Sektoren, 512 Byte,
 *                            Beschreibung "UFT MF-1028 Pruefdatei",
 *                            Etikett "UFT-QRST"
 *     dsktrans -itype qrst -otype raw
 *                        ->  163840 von 163840 Byte IDENTISCH
 *
 * Damit gehen **alle drei** Spursatz-Arten durch einen fremden
 * Entpacker. Das ist der Unterschied zu MF-1009, wo Packer und
 * Entpacker aus derselben Hand kamen und der Rundlauf deshalb nichts
 * bewies.
 *
 * **T2 und nicht T1b**, weil T1b einen fremden *Erzeuger* verlangt.
 * Hier hat eine fremde Umsetzung nur *gelesen*.
 *
 * Jeder Sektor ist selbstbeschreibend: Byte 0 Zylinder, 1 Kopf,
 * 2 Sektor, 3..7 `"UFT-Q"`. Spur 1 ist die leere Spur (durchgehend
 * 0xE5), Spur 2 die gepackte.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

extern const uft_format_plugin_t uft_format_plugin_qrst;

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

#define FIXTURE "qrst_spec_160k.qrst"
#define ZYL  40
#define KOPF 1
#define SPT  8
#define SS   512
#define LEERSPUR 1      /* Typ 1, Fuellbyte 0xE5 */
#define PACKSPUR 2      /* Typ 2, abwechselnde Laeufe */

static int gruen = 0, rot = 0, uebersprungen = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  [OK]   %s\n", was); }
    else { rot++; printf("  [ROT]  %s\n         -> %s\n", was, detail); }
}

static uint8_t *lies_datei(const char *pfad, size_t *n)
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
        free(b);
        fclose(f);
        return NULL;
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
    const uft_format_plugin_t *p = &uft_format_plugin_qrst;
    const char *tmp = getenv("TEMP");
    char pfad[600];
    uint8_t *datei;
    size_t n;

    printf("QRST gegen libdsk doc/qrst.html (LGPL-2+, nur gelesen)\n");
    printf("=======================================================\n");

    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, FIXTURE);
    datei = lies_datei(pfad, &n);
    if (!datei) {
        printf("  [SKIP] %s fehlt im Korpus\n", pfad);
        printf("\n%d gruen, %d rot, 1 uebersprungen\n", gruen, rot);
        return 0;
    }
    printf("  Fixture: %s (%zu Byte)\n\n", FIXTURE, n);

    /* ── 1. Der Kopf ist 796 Byte und die Kennung fuenf ────────────── */
    {
        char d[200];
        const int magic_ok = (n > 796 && memcmp(datei, "QRST\0", 5) == 0);
        const int unbenutzt_ok = (datei[5] == 0x00 && datei[6] == 0x80
                                  && datei[7] == 0x3F);
        snprintf(d, sizeof(d), "Kennung %02X %02X %02X %02X %02X, "
                 "unbenutzt %02X %02X %02X, Kapazitaetskode %u",
                 datei[0], datei[1], datei[2], datei[3], datei[4],
                 datei[5], datei[6], datei[7], datei[0x0C]);
        pruefe("Fixture: \"QRST\",0 bei 0x00, 00 80 3F bei 0x05, "
               "Kapazitaetskode 5 bei 0x0C",
               magic_ok && unbenutzt_ok && datei[0x0C] == 5, d);
    }

    /* ── 2. Die Sonde ──────────────────────────────────────────────── */
    {
        int conf = -1;
        bool ja = p->probe(datei, n < 4096 ? n : 4096, n, &conf);
        char d[140];
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        /* MF-729: eine 5-Byte-Kennung ist ein Merkmal -> 80..100 */
        pruefe("Sonde nimmt an, Konfidenz >= 80 (die 5-Byte-Kennung ist "
               "ein Merkmal)", ja && conf >= 80, d);
    }

    /* ── 3. Gegenprobe: die Kennung OHNE Nullbyte ──────────────────── */
    {
        uint8_t *kaputt = (uint8_t *)malloc(n);
        int conf = -1;
        bool ja;
        char d[140];
        memcpy(kaputt, datei, n);
        kaputt[4] = 'X';               /* "QRSTX" statt "QRST\0" */
        ja = p->probe(kaputt, 4096, n, &conf);
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("Gegenprobe: \"QRSTX\" wird ABGEWIESEN — das Nullbyte "
               "gehoert zur Kennung", !ja, d);
        free(kaputt);
    }

    /* ── 4. Gegenprobe: ein Kapazitaetskode, den es nicht gibt ─────── */
    {
        uint8_t *kaputt = (uint8_t *)malloc(n);
        int conf = -1;
        bool ja;
        char d[140];
        memcpy(kaputt, datei, n);
        kaputt[0x0C] = 8;              /* die Tafel kennt 1..7 */
        ja = p->probe(kaputt, 4096, n, &conf);
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("Gegenprobe: Kapazitaetskode 8 wird ABGEWIESEN "
               "(die Tafel kennt 1..7)", !ja, d);
        free(kaputt);
    }

    /* ── 5. Oeffnen: die Geometrie kommt aus dem Kode ──────────────── */
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
            pruefe("open: Kapazitaetskode 5 -> 40 / 1 / 8 / 512",
                   rc == UFT_OK && disk.geometry.cylinders == ZYL
                   && disk.geometry.heads == KOPF
                   && disk.geometry.sectors == SPT
                   && disk.geometry.sector_size == SS, d);
        }
        if (rc != UFT_OK) {
            printf("\n%d gruen, %d rot, %d uebersprungen\n", gruen, rot,
                   uebersprungen);
            free(datei);
            return 1;
        }

        /* ── 6. Alle 40 Spuren, alle drei Satzarten ────────────────── */
        {
            int c, s;
            int falsche_zahl = 0, fremd = 0, leer_falsch = 0;
            char erstes[240] = "";
            for (c = 0; c < ZYL; c++) {
                uft_track_t tr;
                memset(&tr, 0, sizeof(tr));
                if (p->read_track(&disk, c, 0, &tr) != UFT_OK
                    || (int)tr.sector_count != SPT) {
                    falsche_zahl++;
                    if (!erstes[0])
                        snprintf(erstes, sizeof(erstes),
                                 "Spur %d: %u Sektoren statt %d", c,
                                 (unsigned)tr.sector_count, SPT);
                    free(tr.sectors);
                    free(tr.raw_data);
                    continue;
                }
                for (s = 0; s < SPT; s++) {
                    const uint8_t *dd = tr.sectors[s].data;
                    if (!dd) { fremd++; continue; }
                    if (c == LEERSPUR) {
                        /* Typ 1: die ganze Spur ist 0xE5 */
                        int k, alle = 1;
                        for (k = 0; k < SS; k++)
                            if (dd[k] != 0xE5) { alle = 0; break; }
                        if (!alle) {
                            leer_falsch++;
                            if (!erstes[0])
                                snprintf(erstes, sizeof(erstes),
                                         "die leere Spur %d Sektor %d ist "
                                         "nicht 0xE5, sondern %02X",
                                         c, s, dd[0]);
                        }
                        continue;
                    }
                    if (dd[0] != (uint8_t)c || dd[1] != 0
                        || dd[2] != (uint8_t)s
                        || memcmp(dd + 3, "UFT-Q", 5) != 0) {
                        fremd++;
                        if (!erstes[0])
                            snprintf(erstes, sizeof(erstes),
                                     "Spur %d Sektor %d liefert Zyl %u "
                                     "Kopf %u Sektor %u", c, s,
                                     (unsigned)dd[0], (unsigned)dd[1],
                                     (unsigned)dd[2]);
                    }
                }
                free(tr.sectors);
                free(tr.raw_data);
            }
            {
                char d[320];
                snprintf(d, sizeof(d), "%d Spuren falsche Sektorzahl, "
                         "%d fremde Sektoren, %d Fehler in der leeren "
                         "Spur; erster: %s", falsche_zahl, fremd,
                         leer_falsch, erstes[0] ? erstes : "-");
                pruefe("alle 40 Spuren: 38 rohe, die leere (Typ 1, 0xE5) "
                       "und die GEPACKTE (Typ 2, abwechselnde Laeufe) "
                       "liefern jeder seine eigenen Bytes",
                       falsche_zahl == 0 && fremd == 0
                       && leer_falsch == 0, d);
            }
        }

        /* ── 7. Die gepackte Spur einzeln ──────────────────────────── */
        {
            uft_track_t tr;
            char d[220];
            int ok = 0;
            memset(&tr, 0, sizeof(tr));
            if (p->read_track(&disk, PACKSPUR, 0, &tr) == UFT_OK
                && (int)tr.sector_count == SPT && tr.sectors[0].data
                && tr.sectors[SPT - 1].data) {
                const uint8_t *a = tr.sectors[0].data;
                const uint8_t *z = tr.sectors[SPT - 1].data;
                ok = (a[0] == PACKSPUR && a[2] == 0
                      && z[0] == PACKSPUR && z[2] == SPT - 1);
            }
            snprintf(d, sizeof(d), "%u Sektoren; erster (%u,%u,%u), "
                     "letzter (%u,%u,%u)", (unsigned)tr.sector_count,
                     tr.sectors && tr.sectors[0].data
                        ? (unsigned)tr.sectors[0].data[0] : 255u,
                     tr.sectors && tr.sectors[0].data
                        ? (unsigned)tr.sectors[0].data[1] : 255u,
                     tr.sectors && tr.sectors[0].data
                        ? (unsigned)tr.sectors[0].data[2] : 255u,
                     tr.sectors && tr.sectors[SPT - 1].data
                        ? (unsigned)tr.sectors[SPT - 1].data[0] : 255u,
                     tr.sectors && tr.sectors[SPT - 1].data
                        ? (unsigned)tr.sectors[SPT - 1].data[1] : 255u,
                     tr.sectors && tr.sectors[SPT - 1].data
                        ? (unsigned)tr.sectors[SPT - 1].data[2] : 255u);
            pruefe("die GEPACKTE Spur 2 wird richtig entpackt — erster "
                   "und letzter Sektor nennen sich selbst", ok, d);
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
            snprintf(d, sizeof(d), "Spur 40 -> rc=%d, Kopf 1 -> rc=%d",
                     (int)a, (int)c2);
            pruefe("Spur 40 und Kopf 1 werden ABGEWIESEN (40 Zyl, "
                   "1 Kopf)", a != UFT_OK && c2 != UFT_OK, d);
        }

        p->close(&disk);
    }

    /* ── 9. Die Pruefsumme steht in der Datei selbst ───────────────── */
    {
        /* doc/qrst.html: „The checksum is the sum of all bytes on the
         * disc, each byte multiplied by (1 + its offset on the disc)."
         * Das ist ein Beleg AM OBJEKT, nicht an einem Werkzeug — wie
         * MF-869 (die CRCs der Diskette von 1979) und MF-1013. */
        uint32_t soll = (uint32_t)datei[8] | ((uint32_t)datei[9] << 8)
                        | ((uint32_t)datei[10] << 16)
                        | ((uint32_t)datei[11] << 24);
        uint32_t ist = 0;
        uft_disk_t disk;
        char d[200];
        int vollstaendig = 1;
        size_t versatz = 0;
        memset(&disk, 0, sizeof(disk));
        if (p->open(&disk, pfad, true) == UFT_OK) {
            int c, s, k;
            for (c = 0; c < ZYL && vollstaendig; c++) {
                uft_track_t tr;
                memset(&tr, 0, sizeof(tr));
                if (p->read_track(&disk, c, 0, &tr) != UFT_OK
                    || (int)tr.sector_count != SPT) {
                    vollstaendig = 0;
                    free(tr.sectors);
                    free(tr.raw_data);
                    break;
                }
                for (s = 0; s < SPT; s++) {
                    const uint8_t *dd = tr.sectors[s].data;
                    if (!dd) { vollstaendig = 0; break; }
                    for (k = 0; k < SS; k++) {
                        versatz++;
                        ist += (uint32_t)dd[k] * (uint32_t)versatz;
                    }
                }
                free(tr.sectors);
                free(tr.raw_data);
            }
            p->close(&disk);
        } else {
            vollstaendig = 0;
        }
        snprintf(d, sizeof(d), "im Kopf 0x%08X, nachgerechnet 0x%08X "
                 "ueber %zu Byte%s", soll, ist, versatz,
                 vollstaendig ? "" : " (unvollstaendig gelesen)");
        pruefe("die Pruefsumme im Kopf (Summe byte*(1+Versatz)) geht auf "
               "— ein Beleg AM OBJEKT, nicht an einem Werkzeug",
               vollstaendig && ist == soll, d);
    }

    /* ── 10. Gegenprobe: ein Spurtyp, den es nicht gibt ──────────────
     *
     * **Und diese Gegenprobe muss am LETZTEN Satz ansetzen, nicht am
     * ersten.** Am ersten waere sie gruen aus dem falschen Grund: eine
     * unbekannte Satzart macht den Satzstrom unlesbar, also wird die
     * Datei auch dann abgewiesen, wenn der Leser den Satz bloss
     * stillschweigend UEBERSPRINGT — der naechste Satz beginnt an der
     * falschen Stelle und faellt durch die Geometriepruefung. Die
     * Mutationsmatrix hat das gezeigt: die Mutation „unbekannte
     * Satzarten stillschweigend annehmen" rutschte durch.
     *
     * Am letzten Satz ist es isoliert: wird er uebersprungen, fehlt
     * nur die letzte Spur, und die Datei geht auf. Also faellt die
     * Zusage genau dann, wenn sie fallen soll. Dieselbe Lehre wie in
     * MF-1014 und MF-1026 — ein Gegenbeweis, dessen Ablehnungsgrund
     * nicht gemessen ist, beweist nichts. */
    {
        uint8_t *kaputt = (uint8_t *)malloc(n);
        uft_disk_t disk;
        uft_error_t rc;
        char d[200];
        char kpfad[600];
        size_t pos = 796, letzter = 0;
        const size_t tracklen = (size_t)SPT * SS;
        memcpy(kaputt, datei, n);

        /* die Saetze abgehen, um den letzten zu finden */
        while (pos + 3 <= n) {
            const uint8_t typ = kaputt[pos + 2];
            letzter = pos;
            if (typ == 0)      pos += 3 + tracklen;
            else if (typ == 1) pos += 4;
            else if (typ == 2) {
                const uint16_t clen = (uint16_t)(kaputt[pos + 3]
                                      | (kaputt[pos + 4] << 8));
                pos += 5 + clen;
            } else break;
        }
        kaputt[letzter + 2] = 3;      /* Typ des LETZTEN Spursatzes */

        snprintf(kpfad, sizeof(kpfad), "%s/uft_qrst_typ3.qrst",
                 tmp ? tmp : ".");
        if (letzter && schreibe(kpfad, kaputt, n)) {
            memset(&disk, 0, sizeof(disk));
            rc = p->open(&disk, kpfad, true);
            snprintf(d, sizeof(d), "letzter Satz bei %zu (Spur %u), "
                     "open -> rc=%d", letzter,
                     (unsigned)kaputt[letzter], (int)rc);
            pruefe("Gegenprobe: Spurtyp 3 im LETZTEN Satz wird ABGEWIESEN "
                   "(es gibt 0, 1 und 2) — nicht stillschweigend "
                   "uebersprungen", rc != UFT_OK, d);
            if (rc == UFT_OK) p->close(&disk);
            remove(kpfad);
        } else {
            pruefe("Gegenprobe: Spurtyp 3 wird ABGEWIESEN", 0,
                   letzter ? "Pruefdatei liess sich nicht schreiben"
                           : "kein Spursatz gefunden");
        }
        free(kaputt);
    }

    /* ── MF-1033: libdsk hat eine QRST GESCHRIEBEN ─────────────────
     *
     * Bis MF-1032 galt P3-333 („libdsk erzeugt keine Fixtures"), und das
     * traf nicht. `dsktrans -itype qrst -otype qrst` laeuft durch
     * libdsks eigenen Leser UND seinen eigenen **Packer**; die Ausgabe
     * ist deshalb NICHT byteidentisch mit der Eingabe (5363 statt
     * 156679 Byte), sondern eine eigene Fassung derselben Diskette.
     *
     * Genau das macht sie zum Fremderzeugnis: alle drei Spursatz-Arten
     * und ein fremder Packer gehen hier durch UFTs Entpacker. Damit
     * steht `qrst` auf **T1b** statt T2. */
    {
        char fpfad[600], d[260];
        uft_disk_t da, db;
        int c, gleich = 0, ungleich = 0, spuren = 0;

        snprintf(fpfad, sizeof(fpfad), "%s/%s", UFT_CORPUS_DIR,
                 "libdsk_qrst_160k.qrst");
        memset(&da, 0, sizeof(da));
        memset(&db, 0, sizeof(db));
        snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, FIXTURE);
        if (p->open(&da, pfad, true) == UFT_OK
            && p->open(&db, fpfad, true) == UFT_OK) {
            for (c = 0; c < 40; c++) {
                uft_track_t ta, tb;
                memset(&ta, 0, sizeof(ta));
                memset(&tb, 0, sizeof(tb));
                if (p->read_track(&da, c, 0, &ta) != UFT_OK
                    || p->read_track(&db, c, 0, &tb) != UFT_OK) continue;
                spuren++;
                if (ta.sector_count != tb.sector_count) {
                    ungleich += (int)ta.sector_count;
                } else {
                    size_t s;
                    for (s = 0; s < ta.sector_count; s++) {
                        if (ta.sectors[s].data && tb.sectors[s].data
                            && ta.sectors[s].data_len == tb.sectors[s].data_len
                            && ta.sectors[s].id.sector == tb.sectors[s].id.sector
                            && memcmp(ta.sectors[s].data, tb.sectors[s].data,
                                      ta.sectors[s].data_len) == 0) gleich++;
                        else ungleich++;
                    }
                }
                uft_track_release(&ta);
                uft_track_release(&tb);
            }
            snprintf(d, sizeof(d), "%d Spuren verglichen, %d Sektoren "
                     "gleich, %d ungleich; fremde Datei %ux%ux%ux%u",
                     spuren, gleich, ungleich, db.geometry.cylinders,
                     db.geometry.heads, db.geometry.sectors,
                     db.geometry.sector_size);
            pruefe("MF-1033: das von libdsk GESCHRIEBENE Abbild liefert "
                   "alle 320 Sektoren byteidentisch — fremder Packer, "
                   "UFTs Entpacker (T1b)",
                   gleich == 320 && ungleich == 0
                   && db.geometry.cylinders == 40 && db.geometry.heads == 1
                   && db.geometry.sectors == 8
                   && db.geometry.sector_size == 512, d);
            p->close(&da);
            p->close(&db);
        } else {
            pruefe("MF-1033: das von libdsk geschriebene Abbild laesst "
                   "sich oeffnen", 0, "eine der beiden Dateien fehlt");
        }
    }

    free(datei);
    printf("\n%d gruen, %d rot, %d uebersprungen\n", gruen, rot,
           uebersprungen);
    return rot ? 1 : 0;
}

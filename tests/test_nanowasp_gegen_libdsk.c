/**
 * @file test_nanowasp_gegen_libdsk.c
 * @brief NanoWasp: kein Kopf, kopf-dur, und ein Sektor-Skew (MF-1030)
 *
 * ── Die Quelle ──────────────────────────────────────────────────────
 *
 * `tools/uft-scout/work/libdsk/lib/drvnwasp.c` (John Elliott,
 * **LGPL-2+**; **nur gelesen**, Kanal *Spec* nach MF-695). Fuenf
 * Stellen tragen diesen Test:
 *
 *   Z.  23-30  Der Kopfkommentar: die Datei speichert die Sektoren in
 *              **SIDES_OUTOUT** (kopf-dur) statt SIDES_ALT — „though
 *              the MicroBee actually writes them in SIDES_ALT order"
 *              —, und der Treiber **kehrt den Sektor-Skew um**, damit
 *              sie in logischer Reihenfolge herauskommen. libdsk nennt
 *              das selbst „an abuse of libdsk ... cpmtools doesn't
 *              support the type of skewing done by the microbee
 *              (**sector 1 doesn't map to sector 1**)".
 *   Z.  71-94  `nwasp_open()` prueft **NICHTS** — keine Kennung, kein
 *              Kopf; die Datei beginnt mit Sektordaten.
 *   Z. 127     `static const int skew[10] = { 1,4,7,0,3,6,9,2,5,8 };`
 *   Z. 147     `offset = 204800L * head + 5120L * cylinder
 *                        + 512 * skew[sector-1];`
 *   Z. 286-301 `nwasp_getgeom()`: **40** Zylinder, **2** Koepfe,
 *              **10** Sektoren, `dg_secbase = 1`, **512** Byte.
 *
 * ── Was der Vorzustand war ──────────────────────────────────────────
 *
 * **UFT konnte keine NanoWasp-Datei lesen.** Es verlangte eine
 * **24 Byte lange Kennung** `"nanowasp floppy image\r\n\032"` und
 * einen **80-Byte-Kopf** mit Geometriefeldern, dazu **80** Zylinder
 * als Vorgabe. Nichts davon existiert.
 *
 * Und haette die Kennung gestimmt, waere es doppelt falsch gewesen: 80
 * Byte Sektordaten waeren als Kopf verworfen worden, UND die Anordnung
 * ist kopf-dur mit Skew statt linear.
 *
 * Das ist die Klasse von **MF-961** (`86f`/`"86BX"`), **MF-1022**
 * (`sap`/`"SAP"`) und **MF-1029** (`myz80`/`"MYZ80 "`) — zum
 * **vierten** Mal, und dreimal davon in dieser Runde.
 *
 * ── Warum dieses Fixture kein Selbstgespraech ist ───────────────────
 *
 * `tests/corpus_free/nwasp_spec_400k.nanowasp` ist von UFT nach der
 * Vorlage gebaut, aber ihr Bytelayout ist von **fremder Hand
 * nachgewiesen**:
 *
 *     dskid -type nanowasp   ->  40 Zylinder, 2 Koepfe, 10 Sektoren,
 *                                512 Byte, „First sector: 1"
 *     dsktrans -itype nanowasp -otype raw
 *                            ->  409600 Byte, in denen **alle 800
 *                                Sektoren in logischer Reihenfolge**
 *                                stehen
 *
 * Der zweite Lauf ist der entscheidende: er prueft **Skew und
 * kopf-dure Anordnung zugleich**, denn nur wenn beide stimmen, kommt
 * die Diskette sortiert heraus.
 *
 * **T2 und nicht T1b**, weil T1b einen fremden *Erzeuger* verlangt.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_nanowasp;

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

#define FIXTURE "nwasp_spec_400k.nanowasp"
#define ZYL   40
#define KOPF  2
#define SPT   10
#define SS    512
#define GESAMT (ZYL * KOPF * SPT * SS)

/* libdsk `drvnwasp.c:127` */
static const int SKEW[SPT] = { 1, 4, 7, 0, 3, 6, 9, 2, 5, 8 };

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
    const uft_format_plugin_t *p = &uft_format_plugin_nanowasp;
    const char *tmp = getenv("TEMP");
    char pfad[600];
    uint8_t *datei;
    size_t n;

    printf("NanoWasp gegen libdsk drvnwasp.c (LGPL-2+, nur gelesen)\n");
    printf("========================================================\n");

    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, FIXTURE);
    datei = lies(pfad, &n);
    if (!datei) {
        printf("  [SKIP] %s fehlt im Korpus\n", pfad);
        printf("\n%d gruen, %d rot, 1 uebersprungen\n", gruen, rot);
        return 0;
    }
    printf("  Fixture: %s (%zu Byte)\n\n", FIXTURE, n);

    /* ── 1. Es gibt keinen Kopf ────────────────────────────────────── */
    {
        char d[200];
        /* Physischer Platz 0 traegt den LOGISCHEN Sektor 4, weil
         * skew[3] == 0. Steht dort ein Kopf, ist es keine NanoWasp. */
        snprintf(d, sizeof(d), "%zu Byte; erste drei Bytes %02X %02X %02X "
                 "(Kopf %u, Zylinder %u, log. Sektor %u)", n,
                 datei[0], datei[1], datei[2],
                 (unsigned)datei[0], (unsigned)datei[1],
                 (unsigned)datei[2]);
        pruefe("Fixture: genau 409600 Byte, und bei Versatz 0 steht "
               "SOFORT ein Sektor — der logische Sektor 4 (skew[3] == 0)",
               n == (size_t)GESAMT && datei[0] == 0 && datei[1] == 0
               && datei[2] == 4, d);
    }

    /* ── 2. Die Sonde prueft die DATEIGROESSE ─────────────────────── */
    {
        int conf = -1;
        bool ja = p->probe(datei, 4096, n, &conf);
        char d[140];
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        /* MF-729: 30..49 = „nur die Groesse", und mehr gibt es hier
         * nicht zu pruefen. */
        pruefe("Sonde nimmt an, Konfidenz im Band 30..49 (die Groesse ist "
               "die EINZIGE pruefbare Eigenschaft)",
               ja && conf >= 30 && conf < 50, d);
    }

    /* ── 3. Gegenprobe: ein Byte zu wenig ist keine NanoWasp ───────── */
    {
        int conf = -1;
        bool ja = p->probe(datei, 4096, n - 1, &conf);
        char d[140];
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("Gegenprobe: 409599 Byte werden ABGEWIESEN", !ja, d);
    }

    /* ── 4. Und die Sonde darf NICHT die Puffergroesse nehmen ─────────
     *
     * Das ist die Lehre aus MF-1029: dort verwarf
     * `myz80_probe_plugin()` die Dateigroesse (`(void)file_size`) und
     * verglich die gesuchten Groessen gegen die PUFFERgroesse — 4096
     * Byte. Der ganze Zweig war damit toter Code, und niemand hat es
     * gemerkt, weil nichts ihn geprueft hat.
     *
     * Bei NanoWasp waere derselbe Fehler das ganze Format: die Groesse
     * ist die einzige Pruefung. Diese Zusage haelt ihn fest. */
    {
        int conf = -1;
        bool ja = p->probe(datei, (size_t)GESAMT, 4096u, &conf);
        char d[200];
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("Gegenprobe: ein 409600 Byte grosser PUFFER mit "
               "Dateigroesse 4096 wird ABGEWIESEN — die Sonde nimmt die "
               "DATEIgroesse, nicht die Puffergroesse (MF-1029)",
               !ja, d);
    }

    /* ── 5. Oeffnen ────────────────────────────────────────────────── */
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
            pruefe("open: die FESTE Geometrie 40 / 2 / 10 / 512",
                   rc == UFT_OK && disk.geometry.cylinders == ZYL
                   && disk.geometry.heads == KOPF
                   && disk.geometry.sectors == SPT
                   && disk.geometry.sector_size == SS, d);
        }
        if (rc != UFT_OK) {
            printf("\n%d gruen, %d rot\n", gruen, rot);
            free(datei);
            return 1;
        }

        /* ── 6. Alle 800 Sektoren: Skew UND kopf-dure Anordnung ───── */
        {
            int c, h, s;
            int falsche_id = 0, fremd = 0, unlesbar = 0;
            char erstes[240] = "";
            for (c = 0; c < ZYL; c++) {
                for (h = 0; h < KOPF; h++) {
                    uft_track_t tr;
                    memset(&tr, 0, sizeof(tr));
                    if (p->read_track(&disk, c, h, &tr) != UFT_OK
                        || (int)tr.sector_count != SPT) {
                        unlesbar++;
                        free(tr.sectors);
                        free(tr.raw_data);
                        continue;
                    }
                    for (s = 0; s < SPT; s++) {
                        const uint8_t *dd = tr.sectors[s].data;
                        /* `dg_secbase = 1` -> IDs 1..10 */
                        if (tr.sectors[s].id.sector != (uint8_t)(s + 1)) {
                            falsche_id++;
                            if (!erstes[0])
                                snprintf(erstes, sizeof(erstes),
                                         "K%d Z%d Sektor %d hat ID %u "
                                         "(libdsk: dg_secbase = 1)", h, c,
                                         s + 1,
                                         (unsigned)tr.sectors[s].id.sector);
                        }
                        if (!dd || dd[0] != (uint8_t)h
                            || dd[1] != (uint8_t)c
                            || dd[2] != (uint8_t)(s + 1)
                            || memcmp(dd + 3, "UFT-N", 5) != 0) {
                            fremd++;
                            if (!erstes[0] && dd)
                                snprintf(erstes, sizeof(erstes),
                                         "K%d Z%d log. Sektor %d liefert "
                                         "K%u Z%u Sektor %u", h, c, s + 1,
                                         (unsigned)dd[0], (unsigned)dd[1],
                                         (unsigned)dd[2]);
                        }
                    }
                    free(tr.sectors);
                    free(tr.raw_data);
                }
            }
            {
                char d[320];
                snprintf(d, sizeof(d), "%d Spuren unlesbar, %d falsche "
                         "IDs, %d fremde Sektoren; erster: %s", unlesbar,
                         falsche_id, fremd, erstes[0] ? erstes : "-");
                pruefe("alle 800 Sektoren: IDs 1..10 und jeder liefert "
                       "seine EIGENEN Bytes — damit sind Skew "
                       "{1,4,7,0,3,6,9,2,5,8} UND die kopf-dure "
                       "Anordnung geprueft",
                       unlesbar == 0 && falsche_id == 0 && fremd == 0, d);
            }
        }

        /* ── 7. Der Skew einzeln, an der auffaelligsten Stelle ─────── */
        {
            uft_track_t tr;
            char d[240];
            int ok = 0;
            memset(&tr, 0, sizeof(tr));
            if (p->read_track(&disk, 0, 0, &tr) == UFT_OK
                && (int)tr.sector_count == SPT) {
                /* Der logische Sektor 1 liegt auf physischem Platz 1
                 * (skew[0] == 1), also bei Versatz 512 — NICHT bei 0. */
                const uint8_t *log1 = tr.sectors[0].data;
                ok = (log1 && log1[2] == 1
                      && memcmp(log1, datei + SS, 8) == 0);
            }
            snprintf(d, sizeof(d), "log. Sektor 1 soll bei Versatz %d "
                     "liegen (skew[0] == %d), nicht bei 0", SS, SKEW[0]);
            pruefe("der logische Sektor 1 kommt von Versatz 512 — "
                   "\"sector 1 doesn't map to sector 1\"", ok, d);
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
            c2 = p->read_track(&disk, 0, KOPF, &tr);
            free(tr.sectors); free(tr.raw_data);
            snprintf(d, sizeof(d), "Zylinder 40 -> rc=%d, Kopf 2 -> rc=%d",
                     (int)a, (int)c2);
            pruefe("Zylinder 40 und Kopf 2 werden ABGEWIESEN",
                   a != UFT_OK && c2 != UFT_OK, d);
        }

        p->close(&disk);
    }

    /* ── 9. Die erfundene Kennung wird NICHT als Kopf gelesen ────────
     *
     * Wenn `"nanowasp floppy image\r\n\032"` in den ersten Bytes
     * steht, darf der Leser nicht 80 Byte ueberspringen — die Datei
     * hat keinen Kopf, und die Bytes gehoeren zum logischen Sektor 4
     * (physischer Platz 0). Genau das prueft diese Zusage: der Sektor
     * muss die Kennungsbytes LIEFERN, nicht verschwinden. */
    {
        uint8_t *kaputt = (uint8_t *)malloc(n);
        char kpfad[600];
        uft_disk_t disk;
        uft_track_t tr;
        char d[240];
        int ok = 0;
        memcpy(kaputt, datei, n);
        memcpy(kaputt, "nanowasp floppy image\r\n\032", 24);
        snprintf(kpfad, sizeof(kpfad), "%s/uft_nwasp_magic.nanowasp",
                 tmp ? tmp : ".");
        if (schreibe(kpfad, kaputt, n)) {
            memset(&disk, 0, sizeof(disk));
            if (p->open(&disk, kpfad, true) == UFT_OK) {
                memset(&tr, 0, sizeof(tr));
                if (p->read_track(&disk, 0, 0, &tr) == UFT_OK
                    && (int)tr.sector_count == SPT
                    && tr.sectors[3].data) {
                    /* log. Sektor 4 == physischer Platz 0 == Versatz 0 */
                    ok = (memcmp(tr.sectors[3].data,
                                 "nanowasp floppy image\r\n\032", 24) == 0);
                }
                snprintf(d, sizeof(d), "log. Sektor 4 beginnt mit "
                         "%02X %02X %02X %02X",
                         tr.sectors && tr.sectors[3].data
                            ? tr.sectors[3].data[0] : 0,
                         tr.sectors && tr.sectors[3].data
                            ? tr.sectors[3].data[1] : 0,
                         tr.sectors && tr.sectors[3].data
                            ? tr.sectors[3].data[2] : 0,
                         tr.sectors && tr.sectors[3].data
                            ? tr.sectors[3].data[3] : 0);
                free(tr.sectors);
                free(tr.raw_data);
                p->close(&disk);
            } else {
                snprintf(d, sizeof(d), "open schlug fehl");
            }
            pruefe("die erfundene Kennung wird als SEKTORDATEN gelesen, "
                   "nicht als Kopf uebersprungen — der logische Sektor 4 "
                   "liefert sie", ok, d);
            remove(kpfad);
        } else {
            pruefe("die erfundene Kennung wird als Sektordaten gelesen", 0,
                   "Pruefdatei liess sich nicht schreiben");
        }
        free(kaputt);
    }

    free(datei);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

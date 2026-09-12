/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_nib_gegen_a2nibblize.c
 * @brief NIB gegen ein Abbild von fremder Hand (MF-1050)
 *
 * ── Die fremde Hand ──────────────────────────────────────────────────
 *
 * `a2nibblize` aus **Apple-II-Disk-Tools** (Christopher A. Mosher,
 * GPL-3.0) — dasselbe Paket, dessen Schwesterwerkzeug `to_woz2` in
 * `docs/ORACLES.md` seit MF-712 als Oracle gefuehrt wird. Seine eigene
 * Hilfe sagt:
 *
 *     Converts an Apple ][ floppy disk image from .do format
 *     to .nib "nibble" format.
 *
 * Kanal *Oracle* nach MF-695: das Werkzeug wird **ausgefuehrt**, nichts
 * uebernommen. Die Eingabe ist UFT-eigen (siehe unten), die Ausgabe
 * also eine mechanische Umformung eigener Daten durch fremden Code.
 *
 * ── Warum das den Unterschied macht ──────────────────────────────────
 *
 * `nib` stand auf **T3**: Tests gab es, aber kein Abbild, das jemand
 * anders erzeugt hat.
 *
 * **Und der Baum hatte recht damit, soweit er hingesehen hat.**
 * `docs/ORACLES.md` haelt zu nibtools fest: „*Und es ist nicht das
 * Oracle fuer UFTs `nib`: das ist Apple-II-NIB, nibtools' NIB ist
 * Commodore. Zwei Formate, dieselbe Endung. `nibconv` schreibt
 * ausserdem nur D64/G64, kann also kein NIB erzeugen — kein Weg zu
 * T1b.*" Das ist praezise und stimmt: ueber nibtools fuehrt kein Weg.
 *
 * Was fehlte, war keine Berichtigung, sondern eine **nicht gestellte
 * Frage** — ob das APPLE-Paket einen Erzeuger enthaelt. Es enthaelt
 * einen, und er lag neben dem seit MF-712 registrierten `to_woz2`, im
 * selben Klon, seit derselben Sichtung. Die Lehre ist nicht „eine
 * Aussage war falsch", sondern: **ein geklontes Paket ist mehr als das
 * eine Werkzeug, wegen dessen man es geklont hat.**
 *
 * ── Die Pruefdatei benennt sich selbst ───────────────────────────────
 *
 * `tests/corpus_free/uftk_dos33_35trk.do` ist UFT-eigen: 35 x 16 x 256
 * Byte in DOS-3.3-Reihenfolge, und **jeder Sektor traegt seine eigene
 * Anschrift** — `"UFT-K Tnn Snn "`, gefolgt von einer aus Spur und
 * Sektor abgeleiteten Fuellung, damit keine zwei Sektoren denselben
 * Inhalt haben (Methode aus MF-1020). Ein Leseergebnis sagt damit nicht
 * nur, DASS etwas kam, sondern WELCHE Stelle geliefert wurde.
 *
 * `tests/corpus_free/a2nibblize_uftk_35trk.nib` ist die davon erzeugte
 * NIB: 35 Spuren x 6656 = **232 960** Byte.
 *
 * ── Und 6656 ist nicht angenommen, sondern hergeleitet ───────────────
 *
 * `a2nibblize.c:78` rechnet die Spurlaenge aus der Feldanordnung aus,
 * statt sie zu behaupten:
 *
 *     NIB16_SIZE = TRACKS * (0x30 + 16 * (6+3+(4*2)+3+3+343+3+0x1B) + 0x110)
 *                = 35    * (0x30 + 16 * 396                        + 0x110)
 *                = 35    * 6656
 *                = 232960
 *
 * Also: 6 Byte Vorspann, 3 Byte Adress-Prolog, 8 Byte Spur/Sektor/Band
 * in 4-and-4, 3 Byte Adress-Epilog, 3 Byte Daten-Prolog, 343 Byte
 * 6-and-2-Nutzlast, 3 Byte Daten-Epilog, 27 Byte Luecke. Das sind genau
 * die Konstanten, die `uft_nib.c` fuehrt (`NIB_TRACK_SIZE 6656`,
 * `NIB_FILE_SIZE 232960`) — bis MF-1050 ohne genannte Quelle.
 *
 * ── Der Befund, der beim Messen entstand ─────────────────────────────
 *
 * Der erste Lauf verglich Sektor-ID gegen Quellsektor und bekam **140
 * von 560** Treffer; „Sektor-ID 1" lieferte den Inhalt von Quellsektor
 * 7. Das sah nach einem Lesefehler aus und war **keiner**: eine
 * `.do`-Datei speichert Sektoren in LOGISCHER Reihenfolge, `a2nibblize`
 * legt sie in PHYSISCHER auf die Spur, und das Adressfeld einer NIB
 * traegt die **physische** Nummer. Die DOS-3.3-Verschraenkung lautet
 *
 *     logisch  0 1 2 3 4 5 6 7 8 9 A B C D E F
 *     physisch 0 D B 9 7 5 3 1 E C A 8 6 4 2 F
 *
 * und ihre Umkehrung hat vier Fixpunkte: 0, 5, A, F. Vier mal 35 Spuren
 * sind **genau die 140**, die „zufaellig" stimmten — die Tafel sagt die
 * Messung also exakt voraus, statt sie wegzuerklaeren. Deshalb steht
 * sie hier und wird angewandt, statt dass der Test sie umgeht.
 *
 * **Das ist der eigentliche Wert dieser Zusicherung:** sie prueft nicht
 * nur, dass 560 Sektoren herauskommen, sondern dass jeder den Inhalt
 * traegt, den DOS 3.3 an seiner physischen Stelle ablegen wuerde.
 *
 * ── Abnahme ──────────────────────────────────────────────────────────
 *
 * 35 von 35 Spuren, 560 von 560 Sektoren, **byteidentisch**.
 *
 * ── Was hier NICHT geprueft wird ─────────────────────────────────────
 *
 * Die Schreibseite. `uft_nib.c` hat keinen `write_track`, und dieser
 * Test behauptet keinen. Ebensowenig 13-Sektor-Abbilder (5-and-3):
 * `a2nibblize` kann sie erzeugen, aber die Pruefdatei ist 16-sektorig,
 * und eine Zusage ueber Ungemessenes waere genau das, was dieser Baum
 * nicht tut.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"

extern const uft_format_plugin_t uft_format_plugin_nib;

#define SPUREN     35
#define SEKTOREN   16
#define SEKTORGR  256

/* DOS 3.3: physische Sektornummer -> logische. Umkehrung der ueblichen
 * Tafel logisch->physisch {0,D,B,9,7,5,3,1,E,C,A,8,6,4,2,F}. */
static const int PHYS2LOG[16] = {
    0x0, 0x7, 0xE, 0x6, 0xD, 0x5, 0xC, 0x4,
    0xB, 0x3, 0xA, 0x2, 0x9, 0x1, 0x8, 0xF
};

static int _pass = 0, _fail = 0;
#define PRUEFE(bed, txt)                                                \
    do {                                                                \
        if (bed) { _pass++; }                                           \
        else { printf("  FAIL @ %d: %s\n", __LINE__, (txt)); _fail++; } \
    } while (0)

/* Der Korpuspfad kommt als Uebersetzungsdefinition, nicht aus der
 * Umgebung — so setzt ihn `tests/CMakeLists.txt` fuer alle
 * Korpus-Tests. Fehlt er, ueberspringt sich dieser Test benannt. */
#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR ""
#endif

static char *pfad(const char *datei)
{
    const char *d = UFT_CORPUS_DIR;
    if (!d || !*d) return NULL;
    size_t n = strlen(d) + strlen(datei) + 2;
    char *p = (char *)malloc(n);
    if (!p) return NULL;
    snprintf(p, n, "%s/%s", d, datei);
    return p;
}

static unsigned char *lies(const char *p, size_t erwartet)
{
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    unsigned char *b = (unsigned char *)malloc(erwartet);
    if (!b) { fclose(f); return NULL; }
    size_t n = fread(b, 1, erwartet, f);
    int noch = (fgetc(f) != EOF);
    fclose(f);
    if (n != erwartet || noch) { free(b); return NULL; }
    return b;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== NIB gegen a2nibblize (MF-1050) ===\n");

    char *p_do  = pfad("uftk_dos33_35trk.do");
    char *p_nib = pfad("a2nibblize_uftk_35trk.nib");
    if (!p_do || !p_nib) {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt — ohne Korpus prueft "
               "dieser Test nichts\n");
        free(p_do); free(p_nib);
        return 77;
    }

    unsigned char *quelle = lies(p_do, (size_t)SPUREN * SEKTOREN * SEKTORGR);
    if (!quelle) {
        printf("SKIP: %s fehlt oder hat die falsche Groesse\n", p_do);
        free(p_do); free(p_nib);
        return 77;
    }

    const uft_format_plugin_t *pl = &uft_format_plugin_nib;

    /* Die Sonde: 232 960 Byte werden angenommen, und die Konfidenz
     * bleibt im gueltigen Band (MF-729) — eine NIB traegt keine
     * Kennung, und das Plugin behauptet auch keine. */
    unsigned char kopf[4096];
    FILE *f = fopen(p_nib, "rb");
    if (!f) {
        printf("SKIP: %s fehlt\n", p_nib);
        free(quelle); free(p_do); free(p_nib);
        return 77;
    }
    fseek(f, 0, SEEK_END);
    long fs = ftell(f);
    fseek(f, 0, SEEK_SET);
    size_t kn = fread(kopf, 1, sizeof kopf, f);
    fclose(f);

    PRUEFE(fs == 232960, "die Pruefdatei ist 35 x 6656 = 232960 Byte gross");

    int konf = -1;
    PRUEFE(pl->probe(kopf, kn, (size_t)fs, &konf),
           "probe nimmt das a2nibblize-Erzeugnis an");
    PRUEFE(konf >= 30 && konf <= 100, "die Konfidenz liegt im gueltigen Band");

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    PRUEFE(pl->open(&disk, p_nib, true) == UFT_OK, "open gelingt");
    PRUEFE(disk.geometry.cylinders == SPUREN, "35 Spuren angesagt");
    PRUEFE(disk.geometry.heads == 1, "einseitig");

    int spuren = 0, sektoren = 0, gleich = 0;
    char erster[220];
    erster[0] = 0;

    for (int c = 0; c < SPUREN; c++) {
        uft_track_t t;
        memset(&t, 0, sizeof t);
        if (pl->read_track(&disk, c, 0, &t) != UFT_OK) {
            if (!erster[0])
                snprintf(erster, sizeof erster, "Spur %d nicht lesbar", c);
            continue;
        }
        spuren++;
        sektoren += (int)t.sector_count;
        for (size_t s = 0; s < t.sector_count; s++) {
            const uft_sector_t *sec = &t.sectors[s];
            int phys = (int)sec->id.sector;
            if (!sec->data || sec->data_len != SEKTORGR
                || phys < 0 || phys > 15) {
                if (!erster[0])
                    snprintf(erster, sizeof erster,
                             "Spur %d: Sektor-ID %d mit %zu Byte",
                             c, phys, sec->data_len);
                continue;
            }
            const unsigned char *soll =
                quelle + ((size_t)c * SEKTOREN + PHYS2LOG[phys]) * SEKTORGR;
            if (memcmp(sec->data, soll, SEKTORGR) == 0) {
                gleich++;
            } else if (!erster[0]) {
                char kam[15], erw[15];
                memcpy(kam, sec->data, 14); kam[14] = 0;
                memcpy(erw, soll, 14);      erw[14] = 0;
                snprintf(erster, sizeof erster,
                         "Spur %d phys %X (log %X): erwartet \"%s\", kam \"%s\"",
                         c, phys, PHYS2LOG[phys], erw, kam);
            }
        }
    }

    if (erster[0]) printf("  erster Fehler: %s\n", erster);

    PRUEFE(spuren == SPUREN, "alle 35 Spuren lesbar");
    PRUEFE(sektoren == SPUREN * SEKTOREN, "560 Sektoren geliefert");
    PRUEFE(gleich == SPUREN * SEKTOREN,
           "alle 560 Sektoren byteidentisch mit der Quelle, unter "
           "Anwendung der DOS-3.3-Verschraenkung");

    if (pl->close) pl->close(&disk);
    free(quelle); free(p_do); free(p_nib);

    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    printf("\nNICHT geprueft: die Schreibseite (uft_nib.c hat keinen\n"
           "write_track) und 13-Sektor-Abbilder (5-and-3). Die\n"
           "Pruefdatei ist 16-sektorig; eine Zusage darueber hinaus\n"
           "waere ungemessen.\n");
    return _fail == 0 ? 0 : 1;
}

/**
 * @file test_schranken_die_nie_greifen.c
 * @brief Zwei Pruefungen, die der Typ schon entschieden hat (MF-988).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * Der Linux-Bau meldet 22-mal `-Wtype-limits`: „comparison is always
 * true/false due to limited range of data type". Der Uebersetzer sagt
 * damit, dass **der Typ die Pruefung entscheidet, nicht der Wert**.
 *
 * Das ist harmlos, solange der Typ ENGER ist als die Schranke — dann ist
 * die Pruefung nur ueberfluessig. Gefaehrlich ist die Gegenrichtung: eine
 * Schranke, die der Typ nicht erreichen kann, ist **keine Schranke**. Sie
 * sieht im Quelltext aus wie eine Sicherung und ist keine.
 *
 * Von den 22 Stellen sind gemessen:
 *
 *   6  echte Verhaltensfehler  (PETSCII, hier Fall 3+4)
 *   1  entwerteter Waechter    (JVC,     hier Fall 1+2)
 *   1  entwerteter Waechter    (SCL,     siehe unten — nicht pruefbar)
 *   5  32-Bit-Ueberlaufwaechter, auf 64 Bit inaktiv und RICHTIG
 *   9  Schranke weiter als der Typ, also folgenlos ueberfluessig
 *
 * Dieser Test deckt die beiden Fundstellen, die ein Verhalten
 * aendern, in fuenf Faellen — zwei davon Gegenproben.
 *
 * ── Was hier NICHT geprueft wird, und warum ──────────────────────────
 *
 * `src/formats/scl/uft_scl_parser_v2.c:305` traegt denselben Fehler:
 *
 *     uint8_t file_count;                       // 0..255
 *     #define SCL_MAX_FILES 256
 *     if (builder->file_count >= SCL_MAX_FILES) return false;
 *
 * `>= 256` kann fuer einen `uint8_t` nie zutreffen. Beim 257. Eintrag
 * laeuft der Zaehler auf 0 zurueck und ueberschreibt still den ersten —
 * kein Speicherfehler (das Feld hat genau 256 Plaetze), aber ein
 * stiller Datenverlust mit Erfolgsmeldung.
 *
 * Dafuer gibt es hier keinen Fall, und das ist eine Aussage, keine
 * Nachlaessigkeit: `scl_builder_add_file` ist `static` und hat im
 * **ganzen Baum keinen Aufrufer**. Ohne Aufrufer gibt es kein Verhalten,
 * das rot werden koennte. Der Fehler ist trotzdem behoben — belegt
 * durch Rechnung statt durch Ausfuehrung, und genau so berichtet.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include "uft/uft_file_ops.h"

extern const uft_format_plugin_t uft_format_plugin_jvc;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-48s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

static const char *temp_pfad(char *buf, size_t n, const char *name)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(buf, n, "%s/uft_%s.tmp", d, name);
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Faelle 1+2 — JVC: der Cast schneidet ab, BEVOR die Schranke prueft
 *
 *      uint8_t cyls = (uint8_t)(total_tracks / sides);
 *      if (cyls == 0 || cyls > UFT_MAX_CYLINDERS) { abweisen }
 *
 *  `UFT_MAX_CYLINDERS` ist 256, `cyls` ist ein `uint8_t` — die zweite
 *  Bedingung kann nie zutreffen. Und sie muesste es auch gar nicht: der
 *  Cast in der Zeile davor hat den zu grossen Wert bereits in einen
 *  kleinen, plausiblen verwandelt. Die Schranke prueft das Ergebnis der
 *  Verstuemmelung, nicht den gelesenen Wert.
 *
 *  JVC ist KEIN toter Code: `uft_format_plugin_jvc` steht in
 *  `src/formats/format_registry/uft_format_registry.c:282`.
 * ═══════════════════════════════════════════════════════════════════ */

#define JVC_SPT       1u     /* Sektoren je Spur  */
#define JVC_SS      128u     /* Groessencode 1    */
#define JVC_SEITEN    1u

/* Baut eine JVC-Datei mit `spuren` Spuren. Der Kopf ist 3 Byte lang;
 * JVC leitet seine Kopflaenge aus `Dateigroesse % 256` ab, und
 * spuren*128 ist immer durch 256 teilbar, sobald spuren gerade ist —
 * die 3 Kopfbytes bleiben also als Rest stehen. */
static int baue_jvc(const char *pfad, unsigned spuren)
{
    const size_t hdr  = 3;
    const size_t body = (size_t)spuren * JVC_SPT * JVC_SS;

    uint8_t *f = calloc(1, hdr + body);
    if (!f) return 0;
    f[0] = JVC_SPT;
    f[1] = JVC_SEITEN;
    f[2] = 1;                 /* Groessencode 1 -> 128 Byte */

    FILE *fp = fopen(pfad, "wb");
    if (!fp) { free(f); return 0; }
    int ok = (fwrite(f, 1, hdr + body, fp) == hdr + body);
    fclose(fp);
    free(f);

    /* Die Annahme ueber die Kopferkennung nachrechnen statt glauben. */
    return ok && ((hdr + body) % 256 == hdr);
}

/* Der Gegenzweig: eine gewoehnliche JVC-Datei muss weiterhin genau ihre
 * Zylinderzahl melden. Ohne ihn koennte der Fix „lehne alles ab" lauten
 * und der Test waere gruen. */
static void test_jvc_gewoehnliche_datei_meldet_ihre_zylinder(void)
{
    char p[400];
    temp_pfad(p, sizeof(p), "jvc40");
    ASSERT(baue_jvc(p, 40));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_jvc.open(&disk, p, true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 40);
    ASSERT(disk.geometry.heads == JVC_SEITEN);
    ASSERT(disk.geometry.sectors == JVC_SPT);
    uft_format_plugin_jvc.close(&disk);
    remove(p);
}

static void test_jvc_meldet_nie_eine_ungelesene_zylinderzahl(void)
{
    /* 300 Spuren, einseitig. `(uint8_t)300` ist 44 — eine voellig
     * plausible Zylinderzahl fuer eine TRS-80-Diskette. Genau das macht
     * den Fehler gefaehrlich: er faellt niemandem auf. */
    char p[400];
    temp_pfad(p, sizeof(p), "jvc300");
    ASSERT(baue_jvc(p, 300));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    const uft_error_t rc = uft_format_plugin_jvc.open(&disk, p, true);

    /* Die ZUSAGE, nicht die Umsetzung: absagen ist richtig, 300 melden
     * waere auch richtig — was nicht sein darf, ist UFT_OK mit einer
     * Zahl, die so nicht in der Datei steht. */
    if (rc == UFT_OK) {
        if (disk.geometry.cylinders != 300)
            printf("\n        meldet %u Zylinder fuer eine Datei mit 300"
                   " (256 Spuren still verloren)\n        ",
                   disk.geometry.cylinders);
        ASSERT(disk.geometry.cylinders == 300);
        uft_format_plugin_jvc.close(&disk);
    }
    remove(p);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Faelle 3+4 — PETSCII: `char` ist auf x86-64 VORZEICHENBEHAFTET
 *
 *      char name[17];
 *      if (name[i] == 0xA0) ...                      // nie wahr
 *      else if (name[i] >= 0xC1 && name[i] <= 0xDA)  // nie wahr / immer wahr
 *
 *  Ein `char` deckt -128..127 ab. 0xA0 ist 160, 0xC1 ist 193 — beide
 *  ausserhalb. **Die PETSCII-Wandlung tut auf dieser Plattform nichts**,
 *  und auf einer mit vorzeichenlosem `char` (Linux/ARM) tut sie es. Ein
 *  Diskettenname wird also je nach Bauplattform anders angezeigt.
 *
 *  Ehrlich zur Reichweite: `uft_list_files_extended()` hat heute keinen
 *  Aufrufer. `d81_list_files()` ist aber in `include/uft/uft_file_ops.h`
 *  oeffentlich erklaert — der Test ruft die veroeffentlichte API, nicht
 *  einen Umweg.
 * ═══════════════════════════════════════════════════════════════════ */

#define D81_BYTES        819200u
#define D81_SEKTORGROESSE   256u
#define D81_SEKTOREN         40u
#define D81_KOPF_SPUR        40u   /* Kopfsektor: Spur 40, Sektor 0 */

static void test_d81_petscii_wird_in_ascii_gewandelt(void)
{
    uint8_t *bild = calloc(1, D81_BYTES);
    ASSERT(bild != NULL);

    const size_t kopf = ((D81_KOPF_SPUR - 1) * D81_SEKTOREN + 0)
                        * D81_SEKTORGROESSE;

    /* „UFT" in PETSCII-Grossbuchstaben (0xC1..0xDA entspricht A..Z),
     * aufgefuellt mit 0xA0 — genau so steht es auf einer echten
     * CBM-Diskette. */
    bild[kopf + 4] = 0xD5;                       /* U */
    bild[kopf + 5] = 0xC6;                       /* F */
    bild[kopf + 6] = 0xD4;                       /* T */
    for (int i = 7; i < 20; i++) bild[kopf + i] = 0xA0;

    uft_directory_t dir;
    memset(&dir, 0, sizeof(dir));
    const int rc = d81_list_files(bild, D81_BYTES, &dir);
    ASSERT(rc >= 0);

    if (strcmp(dir.disk_name, "UFT") != 0) {
        printf("\n        Name kam als");
        for (int i = 0; i < 6 && dir.disk_name[i]; i++)
            printf(" %02X", (unsigned char)dir.disk_name[i]);
        printf(" zurueck, erwartet \"UFT\"\n        ");
    }
    ASSERT(strcmp(dir.disk_name, "UFT") == 0);

    free(bild);
}

/* Der Gegenzweig: ein Name, der schon ASCII ist, darf NICHT angefasst
 * werden. Ohne ihn koennte der Fix „ziehe von allem 0x80 ab" lauten.
 *
 * Gefuellt wird hier mit 0x20, NICHT mit 0xA0 — sonst haenge dieser Fall
 * an derselben Ursache wie der Hauptfall und traenne nichts mehr. So ist
 * er heute schon gruen und muss es bleiben. */
static void test_d81_ascii_name_bleibt_unveraendert(void)
{
    uint8_t *bild = calloc(1, D81_BYTES);
    ASSERT(bild != NULL);

    const size_t kopf = ((D81_KOPF_SPUR - 1) * D81_SEKTOREN + 0)
                        * D81_SEKTORGROESSE;
    memcpy(bild + kopf + 4, "uft", 3);
    for (int i = 7; i < 20; i++) bild[kopf + i] = 0x20;

    uft_directory_t dir;
    memset(&dir, 0, sizeof(dir));
    ASSERT(d81_list_files(bild, D81_BYTES, &dir) >= 0);
    ASSERT(strcmp(dir.disk_name, "uft") == 0);

    free(bild);
}

/* Der ZWEITE PETSCII-Block: Dateinamen im Verzeichnis. Eigener Fall, weil
 * es eine eigene Codestelle ist — ohne ihn waere der Fix an
 * `uft_file_ops_extended.c:250` ohne Rotbeweis geblieben. Genau das hat
 * die Mutationsmatrix gezeigt, bevor dieser Fall hier stand. */
static void test_d81_dateiname_wird_aus_petscii_gewandelt(void)
{
    uint8_t *bild = calloc(1, D81_BYTES);
    ASSERT(bild != NULL);

    /* Verzeichnis: Spur 40, Sektor 3 — erster Eintrag, 32 Byte breit. */
    const size_t verz = ((D81_KOPF_SPUR - 1) * D81_SEKTOREN + 3)
                        * D81_SEKTORGROESSE;
    uint8_t *e = bild + verz;

    e[2] = 0x82;              /* PRG, nicht geloescht */
    e[3] = 1;                 /* Startspur  */
    e[4] = 0;                 /* Startsektor */
    e[5] = 0xD4;              /* T */
    e[6] = 0xC5;              /* E */
    e[7] = 0xD3;              /* S */
    e[8] = 0xD4;              /* T */
    for (int i = 9; i < 21; i++) e[i] = 0xA0;

    uft_directory_t dir;
    memset(&dir, 0, sizeof(dir));
    ASSERT(d81_list_files(bild, D81_BYTES, &dir) >= 0);
    ASSERT(dir.count >= 1);

    if (strcmp(dir.files[0].name, "TEST") != 0) {
        printf("\n        Dateiname kam als");
        for (int i = 0; i < 6 && dir.files[0].name[i]; i++)
            printf(" %02X", (unsigned char)dir.files[0].name[i]);
        printf(" zurueck, erwartet \"TEST\"\n        ");
    }
    ASSERT(strcmp(dir.files[0].name, "TEST") == 0);

    free(bild);
}

int main(void)
{
    printf("=== Schranken, die der Typ schon entschieden hat (MF-988) ===\n");
    RUN(jvc_gewoehnliche_datei_meldet_ihre_zylinder);
    RUN(jvc_meldet_nie_eine_ungelesene_zylinderzahl);
    RUN(d81_ascii_name_bleibt_unveraendert);
    RUN(d81_petscii_wird_in_ascii_gewandelt);
    RUN(d81_dateiname_wird_aus_petscii_gewandelt);
    printf("\n%d Pruefungen gruen, %d rot\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

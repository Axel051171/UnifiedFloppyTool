/**
 * @file test_d64_erzeugen_gegen_vice.c
 * @brief Eine erzeugte D64 muss aussehen wie eine von VICE (MF-994).
 *
 * ── Was hier belegt wird ─────────────────────────────────────────────
 *
 * MF-991 hat die Tuer zu `plugin->create` gebaut; MF-994 haengt das
 * D64-Plugin daran. Der Inhalt kommt dabei **nicht** aus neuer
 * Formatlogik, sondern aus `bam_create_d64()` — seit jeher im Baum, in
 * `tests/test_bam_editor.c` an zehn Stellen geprueft, und bis MF-994
 * ohne einen einzigen Aufrufer ausserhalb der Tests.
 *
 * Der springende Punkt: eine erzeugte Diskette wird nicht von UFT
 * gelesen, sondern von **VICE oder einem echten 1541-DOS**. Es genuegt
 * also nicht, dass UFT sie selbst wieder aufmachen kann. Sie muss
 * aussehen wie eine, die von dort kommt.
 *
 * ── Das Orakel ───────────────────────────────────────────────────────
 *
 * `tests/corpus_free/vice_c1541_35trk.d64`, erzeugt von VICEs `c1541` —
 * genau die Referenz, die `uft_d64_plugin.c` in seinem `spec_status`
 * selbst nennt („de-facto via VICE"). Die Pruefwerte hier sind **daraus
 * gelesen**, nicht aus einer Beschreibung uebernommen:
 *
 *     Verweis auf das Verzeichnis   18 / 1
 *     DOS-Kennung                   'A'
 *     Spur 18:  frei = 17,  Bitmap = FC FF 07
 *
 * ── Eine widerlegte Vermutung, die hierher gehoert ───────────────────
 *
 * Vor der Messung lautete der Verdacht: `bam_format_disk()` reserviere
 * die Verzeichnisspur nicht vollstaendig, weil sie alle Spuren frei
 * setzt und danach nur zwei Bloecke belegt — 17 Verzeichnissektoren
 * waeren dann faelschlich als frei markiert.
 *
 * **Das Orakel widerlegt es.** VICE belegt auf Spur 18 ebenfalls nur
 * 18/0 und 18/1; die uebrigen 17 stehen frei, weil das Verzeichnis bei
 * Bedarf hineinwaechst. Die bekannten „664 BLOCKS FREE" entstehen
 * dadurch, dass der Zaehler die Spur 18 ueberspringt — nicht dadurch,
 * dass sie belegt waere.
 *
 * Der Testfall unten prueft deshalb `FC FF 07` als **Sollwert**, nicht
 * als Fehler. Eine Vermutung, die eine Messung nicht traegt, wird
 * zurueckgenommen und aufgeschrieben; sonst stellt sie der Naechste
 * noch einmal an.
 *
 * ── Und was dieselbe Messung dann WIRKLICH fand ──────────────────────
 *
 * Beim ersten Lauf dieses Tests fiel vier von fuenf Faellen gruen aus
 * und einer rot — und der rote war nicht der erwartete. Die BAM stand
 * **gar nicht an ihrem Platz**: `bam_t.tracks` fuehrte 43 Eintraege, wo
 * eine D64 35 hat, jeder Spureintrag lag vier Byte zu spaet und der
 * Diskettenname zweiunddreissig.
 *
 * Das wurde **MF-992**, mit eigenem Rotbeweis
 * (`tests/test_bam_gegen_vice.c`). Dieser Test hier setzt darauf auf.
 *
 * Die Lehre steht hier, weil sie sonst verlorengeht: **eine widerlegte
 * Vermutung entlastet nur das, wonach sie gefragt hat.** Der Verdacht
 * gegen die Spur-18-Belegung trug nicht — die Vorlage war trotzdem
 * falsch, an einer Stelle, nach der niemand gesucht hatte. Wer nach dem
 * Freispruch aufgehoert haette, haette die Verschiebung verdrahtet.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_error.h"
#include "uft/core/uft_disk_create_image.h"

extern uft_disk_t *uft_disk_open(const char *path, bool read_only);
extern void        uft_disk_close(void *disk);
extern uft_error_t uft_register_all_formats(void);

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build"
#endif

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-48s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define D64_35_BYTES  174848L
#define D64_40_BYTES  196608L

/* Versatz der BAM: Spur 18, Sektor 0. Spuren 1..17 haben je 21 Sektoren. */
#define BAM_OFFSET    (17 * 21 * 256)          /* 91392 */
/* Der Eintrag einer Spur: 4 Byte, beginnend bei BAM+4. */
#define BAM_EINTRAG(t) (BAM_OFFSET + 4 + ((t) - 1) * 4)

static const char *temp_pfad(char *buf, size_t n, const char *name)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(buf, n, "%s/uft_neu_%s.d64", d, name);
    return buf;
}

static uint8_t *datei_lesen(const char *p, long *groesse)
{
    FILE *f = fopen(p, "rb");
    if (!f) { *groesse = -1; return NULL; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)n);
    if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) { free(d); d = NULL; n = -1; }
    fclose(f);
    *groesse = n;
    return d;
}

static uft_geometry_t geo(uint16_t zylinder, uint16_t koepfe, uint16_t ss)
{
    uft_geometry_t g;
    memset(&g, 0, sizeof(g));
    g.cylinders   = zylinder;
    g.heads       = koepfe;
    g.sectors     = 21;
    g.sector_size = ss;
    return g;
}

/* ═══ 1. Die erzeugte BAM steht Byte fuer Byte wie VICEs ════════════ */
static void test_erzeugte_bam_gleicht_der_von_vice(void)
{
    /* Erst das Orakel lesen — die Sollwerte kommen von dort, nicht aus
     * dem Kopf. Faellt der Korpus weg, faellt der Test auf, statt still
     * gegen fest verdrahtete Zahlen weiterzulaufen. */
    long vice_n = 0;
    uint8_t *vice = datei_lesen(UFT_CORPUS_DIR "/vice_c1541_35trk.d64", &vice_n);
    ASSERT(vice != NULL);
    ASSERT(vice_n == D64_35_BYTES);

    char p[400];
    temp_pfad(p, sizeof(p), "bam");
    remove(p);

    const uft_geometry_t g = geo(35, 1, 256);
    uft_disk_t *disk = NULL;
    ASSERT(uft_disk_create_image(p, "D64", &g, false, &disk) == UFT_OK);
    ASSERT(disk != NULL);
    uft_disk_close(disk);

    long neu_n = 0;
    uint8_t *neu = datei_lesen(p, &neu_n);
    ASSERT(neu != NULL);
    ASSERT(neu_n == D64_35_BYTES);

    /* Verweis auf das Verzeichnis und DOS-Kennung */
    ASSERT(neu[BAM_OFFSET + 0] == vice[BAM_OFFSET + 0]);   /* 18 */
    ASSERT(neu[BAM_OFFSET + 1] == vice[BAM_OFFSET + 1]);   /*  1 */
    ASSERT(neu[BAM_OFFSET + 2] == vice[BAM_OFFSET + 2]);   /* 'A' */

    /* Der Spur-18-Eintrag: vier Byte, byteweise gegen das Orakel. */
    const int e = BAM_EINTRAG(18);
    if (memcmp(neu + e, vice + e, 4) != 0) {
        printf("\n        Spur 18: erzeugt %02X %02X %02X %02X,"
               " VICE %02X %02X %02X %02X\n        ",
               neu[e], neu[e+1], neu[e+2], neu[e+3],
               vice[e], vice[e+1], vice[e+2], vice[e+3]);
    }
    ASSERT(memcmp(neu + e, vice + e, 4) == 0);

    /* Eine frische Diskette hat auf Spur 1 nichts belegt; VICEs Abbild
     * traegt dort eine Datei. Der Vergleich gilt also NUR fuer Spur 18
     * und die drei Kopfbytes — das steht hier, damit niemand ihn
     * spaeter auf die ganze BAM ausdehnt und sich wundert. */
    ASSERT(neu[BAM_EINTRAG(1)] == 21);

    free(neu);
    free(vice);
    remove(p);
}

/* ═══ 2. Das Ergebnis ist wieder zu oeffnen ═════════════════════════ */
static void test_erzeugte_d64_ist_wieder_lesbar(void)
{
    char p[400];
    temp_pfad(p, sizeof(p), "lesbar");
    remove(p);

    const uft_geometry_t g = geo(35, 1, 256);
    uft_disk_t *disk = NULL;
    ASSERT(uft_disk_create_image(p, "D64", &g, false, &disk) == UFT_OK);
    uft_disk_close(disk);

    uft_disk_t *wieder = uft_disk_open(p, true);
    ASSERT(wieder != NULL);
    ASSERT(wieder->geometry.cylinders == 35);
    ASSERT(wieder->geometry.heads == 1);
    ASSERT(wieder->geometry.sector_size == 256);
    ASSERT(wieder->geometry.total_sectors == 683);
    uft_disk_close(wieder);

    remove(p);
}

/* ═══ 3. Gegenzweig: 40 Spuren ═════════════════════════════════════ */
static void test_vierzig_spuren_gehen_auch(void)
{
    char p[400];
    temp_pfad(p, sizeof(p), "40trk");
    remove(p);

    const uft_geometry_t g = geo(40, 1, 256);
    uft_disk_t *disk = NULL;
    ASSERT(uft_disk_create_image(p, "D64", &g, false, &disk) == UFT_OK);
    uft_disk_close(disk);

    long n = 0;
    uint8_t *d = datei_lesen(p, &n);
    ASSERT(d != NULL);
    ASSERT(n == D64_40_BYTES);
    free(d);
    remove(p);
}

/* ═══ 4. Absagen: was das Format nicht kann, sagt es ═══════════════
 *
 * Ohne diese beiden waere „nimm alles an und liefere 35 Spuren" ein
 * gruener Fix — und der Anwender bekaeme still etwas anderes, als er
 * verlangt hat.                                                        */
static void test_zweiseitig_wird_abgelehnt(void)
{
    char p[400];
    temp_pfad(p, sizeof(p), "zweiseitig");
    remove(p);

    const uft_geometry_t g = geo(35, 2, 256);
    uft_disk_t *disk = NULL;
    ASSERT(uft_disk_create_image(p, "D64", &g, false, &disk)
           == UFT_ERROR_INVALID_ARG);
    ASSERT(disk == NULL);

    long n = 0;
    free(datei_lesen(p, &n));
    ASSERT(n == -1);            /* keine Datei entstanden */
}

static void test_ungepruefte_spurzahl_wird_abgelehnt(void)
{
    char p[400];
    temp_pfad(p, sizeof(p), "41trk");
    remove(p);

    /* 41 und 42 Spuren LIEST das Plugin (die VICE/Schepers-Varianten),
     * aber es gibt keine gepruefte Formatierung dafuer. Lesen zu koennen
     * ist keine Erlaubnis, zu schreiben. */
    const uft_geometry_t g = geo(41, 1, 256);
    uft_disk_t *disk = NULL;
    ASSERT(uft_disk_create_image(p, "D64", &g, false, &disk)
           == UFT_ERROR_INVALID_ARG);
    ASSERT(disk == NULL);

    long n = 0;
    free(datei_lesen(p, &n));
    ASSERT(n == -1);
}

int main(void)
{
    printf("=== D64 erzeugen, gemessen gegen VICE (MF-994) ===\n");

    if (uft_register_all_formats() != UFT_OK) {
        printf("FAIL: Registry liess sich nicht fuellen\n");
        return 1;
    }

    RUN(erzeugte_bam_gleicht_der_von_vice);
    RUN(erzeugte_d64_ist_wieder_lesbar);
    RUN(vierzig_spuren_gehen_auch);
    RUN(zweiseitig_wird_abgelehnt);
    RUN(ungepruefte_spurzahl_wird_abgelehnt);

    printf("\n%d Pruefungen gruen, %d rot\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

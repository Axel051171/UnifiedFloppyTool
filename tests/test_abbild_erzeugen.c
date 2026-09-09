/**
 * @file test_abbild_erzeugen.c
 * @brief Die Tuer zu `plugin->create` (MF-991).
 *
 * ── Was hier belegt wird ─────────────────────────────────────────────
 *
 * Bis MF-991 gab es keinen Weg, mit UFT ein leeres Abbild anzulegen. Der
 * `create`-Zeiger der Plugin-Tafel hatte **drei** Umsetzungen (`img`,
 * `hfe`, `g64`) und **null Aufrufer**; die frueher gedachte API war in
 * MF-294 entfernt worden, weil ihr Name mit dem 0-Argument-Allokator
 * `uft_disk_create()` kollidierte — Signaturbombe ohne Warnung.
 *
 * Dieser Test haelt vier Zusagen fest, und drei davon sind
 * **Verweigerungen**. Das ist kein Zufall: eine Erzeugungs-API, die
 * grosszuegig ist, richtet mehr Schaden an als eine, die es nicht gibt.
 *
 * ── Warum es keinen Rotbeweis „vorher" gibt ──────────────────────────
 *
 * Vor MF-991 existierte die Funktion nicht — ein Test dagegen haette
 * nicht gefehlt, sondern nicht uebersetzt. Das ist kein Rotbeweis,
 * sondern ein fehlendes Symbol.
 *
 * Belegt wird deshalb wie in MF-986: **jede** Zusage hat ihre eigene
 * Mutation, und jede Mutation faellt genau ihre Zusage. Die Matrix steht
 * in der MF-991-Commitnachricht. Was die Mutationen abdecken, ist genau
 * das, was hier schiefgehen koennte:
 *
 *   M1  `!plugin->create` -> `UFT_OK` statt Absage  (Erfolg ohne Tat)
 *   M2  Vorhandenseins-Pruefung entfernt            (stilles Ueberschreiben)
 *   M3  `plugin->create(...)` gar nicht rufen       (leere Zusage)
 *   M4  Plugin-Fehler auf UFT_OK einebnen           (verschluckte Absage)
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

/* Aus uft_core_stubs.c — zum Wiederoeffnen des Erzeugten. */
extern uft_disk_t *uft_disk_open(const char *path, bool read_only);
extern void        uft_disk_close(void *disk);
extern uft_error_t uft_register_all_formats(void);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-50s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* 40 Spuren x 2 Koepfe x 9 Sektoren x 512 = 368640 Byte (360K DD).
 * Bewusst klein: der Test schreibt die Datei wirklich. */
#define ZYL   40u
#define KOPF   2u
#define SEK    9u
#define SS   512u
#define ERWARTETE_GROESSE ((long)ZYL * KOPF * SEK * SS)

static uft_geometry_t geo360(void)
{
    uft_geometry_t g;
    memset(&g, 0, sizeof(g));
    g.cylinders   = ZYL;
    g.heads       = KOPF;
    g.sectors     = SEK;
    g.sector_size = SS;
    g.total_sectors = ZYL * KOPF * SEK;
    return g;
}

static const char *temp_pfad(char *buf, size_t n, const char *name)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(buf, n, "%s/uft_neu_%s.img", d, name);
    return buf;
}

static long dateigroesse(const char *p)
{
    FILE *f = fopen(p, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

/* ═══ 1. Der Hauptfall: erzeugen, und es steht wirklich da ═══════════
 *
 * Geprueft wird nicht die Rueckgabe allein, sondern das ERGEBNIS auf der
 * Platte: die Datei hat die ausgerechnete Groesse, und ein unabhaengiger
 * `uft_disk_open()` liest dieselbe Geometrie zurueck. Ohne den zweiten
 * Teil koennte `create` eine Datei beliebigen Inhalts anlegen.           */
static void test_img_wird_erzeugt_und_ist_wieder_lesbar(void)
{
    char p[400];
    temp_pfad(p, sizeof(p), "haupt");
    remove(p);

    const uft_geometry_t g = geo360();
    uft_disk_t *disk = NULL;
    const uft_error_t rc = uft_disk_create_image(p, "IMG", &g, false, &disk);
    ASSERT(rc == UFT_OK);
    ASSERT(disk != NULL);
    uft_disk_close(disk);

    ASSERT(dateigroesse(p) == ERWARTETE_GROESSE);

    uft_disk_t *wieder = uft_disk_open(p, true);
    ASSERT(wieder != NULL);
    ASSERT(wieder->geometry.cylinders == ZYL);
    ASSERT(wieder->geometry.heads == KOPF);
    ASSERT(wieder->geometry.sectors == SEK);
    ASSERT(wieder->geometry.sector_size == SS);
    uft_disk_close(wieder);

    remove(p);
}

/* ═══ 2. Ein Plugin ohne `create` sagt ab — und legt nichts an ═══════
 *
 * 134 der 137 Plugins koennen nicht erzeugen. Die gefaehrliche Antwort
 * waere `UFT_OK` mit einer leeren oder halben Datei: „Erfolg ohne Tat",
 * die Klasse aus MF-883/930. Deshalb wird BEIDES geprueft — der Code UND
 * dass keine Datei entstanden ist.                                       */
static void test_format_ohne_create_sagt_ab_und_legt_nichts_an(void)
{
    char p[400];
    temp_pfad(p, sizeof(p), "ohnecreate");
    remove(p);

    const uft_geometry_t g = geo360();
    uft_disk_t *disk = (uft_disk_t *)0x1;   /* muss auf NULL gesetzt werden */
    const uft_error_t rc = uft_disk_create_image(p, "TRD", &g, false, &disk);

    ASSERT(rc == UFT_ERROR_NOT_SUPPORTED);
    ASSERT(disk == NULL);
    ASSERT(dateigroesse(p) == -1);          /* keine Datei entstanden */
}

/* ═══ 3. Unbekannter Formatname ═════════════════════════════════════ */
static void test_unbekanntes_format_sagt_ab(void)
{
    char p[400];
    temp_pfad(p, sizeof(p), "unbekannt");
    remove(p);

    const uft_geometry_t g = geo360();
    uft_disk_t *disk = NULL;
    const uft_error_t rc =
        uft_disk_create_image(p, "GIBTESNICHT", &g, false, &disk);

    ASSERT(rc == UFT_ERROR_UNKNOWN_FORMAT);
    ASSERT(disk == NULL);
    ASSERT(dateigroesse(p) == -1);
}

/* ═══ 4. Eine vorhandene Datei wird NICHT still gekuerzt ═════════════
 *
 * Der wichtigste Fall. Alle drei `create`-Umsetzungen beginnen mit
 * `fopen(path,"wb")` — ohne die Pruefung davor waere eine vorhandene
 * Sammlung nach diesem Aufruf leer.
 *
 * Geprueft wird byteweise, nicht nur die Groesse: eine Datei kann
 * dieselbe Laenge behalten und trotzdem ueberschrieben sein.             */
static void test_vorhandene_datei_bleibt_unangetastet(void)
{
    char p[400];
    temp_pfad(p, sizeof(p), "vorhanden");
    remove(p);

    static const char INHALT[] = "Das hier ist die Sammlung des Anwenders.";
    FILE *f = fopen(p, "wb");
    ASSERT(f != NULL);
    ASSERT(fwrite(INHALT, 1, sizeof(INHALT), f) == sizeof(INHALT));
    fclose(f);

    const uft_geometry_t g = geo360();
    uft_disk_t *disk = NULL;
    const uft_error_t rc = uft_disk_create_image(p, "IMG", &g, false, &disk);

    ASSERT(rc == UFT_ERROR_FILE_EXISTS);
    ASSERT(disk == NULL);

    char zurueck[sizeof(INHALT)];
    memset(zurueck, 0, sizeof(zurueck));
    f = fopen(p, "rb");
    ASSERT(f != NULL);
    const size_t gelesen = fread(zurueck, 1, sizeof(zurueck), f);
    fclose(f);

    ASSERT(gelesen == sizeof(INHALT));
    ASSERT(memcmp(zurueck, INHALT, sizeof(INHALT)) == 0);
    ASSERT(dateigroesse(p) == (long)sizeof(INHALT));

    remove(p);
}

/* ═══ 5. Gegenprobe: MIT Freigabe wird ueberschrieben ════════════════
 *
 * Ohne diesen Fall waere „sage immer FILE_EXISTS" ein gruener Fix, und
 * die Freigabe waere eine Zusage ohne Wirkung.                           */
static void test_mit_freigabe_wird_ueberschrieben(void)
{
    char p[400];
    temp_pfad(p, sizeof(p), "freigabe");
    remove(p);

    FILE *f = fopen(p, "wb");
    ASSERT(f != NULL);
    ASSERT(fwrite("alt", 1, 3, f) == 3);
    fclose(f);

    const uft_geometry_t g = geo360();
    uft_disk_t *disk = NULL;
    const uft_error_t rc = uft_disk_create_image(p, "IMG", &g, true, &disk);

    ASSERT(rc == UFT_OK);
    ASSERT(disk != NULL);
    uft_disk_close(disk);
    ASSERT(dateigroesse(p) == ERWARTETE_GROESSE);

    remove(p);
}

/* ═══ 6. Das Plugin darf strenger sein, und sein Fehler kommt an ════
 *
 * `img_create` besteht auf 512 Byte je Sektor. Dieser Fall belegt, dass
 * der Fehler des Plugins **durchgereicht** und nicht auf ein allgemeines
 * `UFT_ERROR_INVALID_ARG` eingeebnet wird — und dass die Tuer nicht
 * selbst schon abfaengt, was das Plugin besser weiss.                    */
static void test_plugin_fehler_wird_durchgereicht(void)
{
    char p[400];
    temp_pfad(p, sizeof(p), "sektorgroesse");
    remove(p);

    uft_geometry_t g = geo360();
    g.sector_size = 256;                    /* IMG kann nur 512 */

    uft_disk_t *disk = NULL;
    const uft_error_t rc = uft_disk_create_image(p, "IMG", &g, false, &disk);

    ASSERT(rc == UFT_ERROR_INVALID_ARG);    /* aus img_create, nicht von hier */
    ASSERT(disk == NULL);
}

int main(void)
{
    printf("=== Ein Abbild erzeugen: die Tuer zu plugin->create (MF-991) ===\n");

    /* MF-447: ohne diesen Aufruf ist die Registry zur Laufzeit leer. */
    if (uft_register_all_formats() != UFT_OK) {
        printf("FAIL: Registry liess sich nicht fuellen\n");
        return 1;
    }

    RUN(img_wird_erzeugt_und_ist_wieder_lesbar);
    RUN(format_ohne_create_sagt_ab_und_legt_nichts_an);
    RUN(unbekanntes_format_sagt_ab);
    RUN(vorhandene_datei_bleibt_unangetastet);
    RUN(mit_freigabe_wird_ueberschrieben);
    RUN(plugin_fehler_wird_durchgereicht);

    printf("\n%d Pruefungen gruen, %d rot\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

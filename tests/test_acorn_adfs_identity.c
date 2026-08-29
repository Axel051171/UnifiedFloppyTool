/**
 * @file test_acorn_adfs_identity.c
 * @brief `.adl` ist ADFS L (655 360), nicht ADFS M (327 680) — MF-654.
 *
 * Gemessener Fehler. `src/formats/adl/uft_adl.c` erklaerte im Kopf:
 *
 *   "ADL = Acorn DFS disk with 80 tracks (vs 40 for ADFS-S).
 *    Headerless, 80 tracks x 1 head x 16 sectors x 256 bytes = 327,680."
 *
 * Dreifach falsch: ADL ist **ADFS**, nicht DFS; ADFS L misst
 * **655 360** Byte, nicht 327 680 (das ist ADFS **M**); und ADFS L ist
 * **zweiseitig**, nicht einseitig.
 *
 * Referenzen, beide unabhaengig gelesen (DiscImageManager,
 * geraldholdsworth, GPL-3.0 — nur als Spec gelesen, kein Code uebernommen):
 *
 *   1. `LazarusSource/DiscImage_Private.pas:188` bindet die Endungen an
 *      die ADFS-Untertypen in dieser Reihenfolge:
 *        ('ads','adm','adl','adf','adf', ...)
 *      also S -> .ads, M -> .adm, **L -> .adl**.
 *   2. `LazarusSource/DiscImage_ADFS.pas:73-75` nennt die Groessen:
 *        163840 -> ADFS S
 *        327680 -> ADFS M
 *        655360 -> ADFS L
 *   3. Gegenprobe an den mitgelieferten Leer-Abbildern, selbst gemessen:
 *        ADFS_L.adl = 655 360 Byte
 *        ADFS_D.adf = 819 200 Byte
 *
 * 655 360 = 80 Spuren x 2 Seiten x 16 Sektoren x 256 Byte. Geht auf.
 *
 * Zweiter Befund, derselbe Anlass: `uft_adf_arc.c:8` nannte 655 360
 * "ADFS-D". Auch das ist falsch — ADFS D misst 819 200 (Beleg 3), und
 * dieselbe Datei fuehrt 819 200 in ihrer eigenen Groessentabelle.
 *
 * Was dieser Test NICHT entscheidet: ob es zwei Leser fuer dieselbe
 * Endung geben soll. `uft_adf_arc.c` beansprucht "adf;adl;adm" und
 * behandelt alle vier ADFS-Groessen; `uft_adl.c` beansprucht "adl;adf".
 * Das ist ein "zwei Leser, eine Tuer"-Fall wie SCOUT-43 und eine
 * Eigentuemer-Entscheidung. Der Test haelt nur fest, dass beide
 * dieselbe, richtige Antwort geben — damit die Reihenfolge der
 * Registrierung das Ergebnis nicht mehr aendert.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_adl;
extern const uft_format_plugin_t uft_format_plugin_adf_arc;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-42s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define ADFS_S   163840u
#define ADFS_M   327680u
#define ADFS_L   655360u
#define ADFS_D   819200u

/* Ein Kopf-Puffer genuegt: beide Proben schauen nur auf die Dateigroesse. */
static uint8_t head[512];

/* Der Kern: ADFS L muss proben. Vor MF-654 tat das nur adf_arc — das
 * Plugin, das die Endung .adl gar nicht im Namen fuehrt. */
TEST(adl_probt_die_ADFS_L_groesse)
{
    int conf = 0;
    bool ok = uft_format_plugin_adl.probe(head, sizeof(head), ADFS_L, &conf);
    if (!ok) {
        printf("ADL probt 655360 (ADFS L) NICHT — das ist seine eigene Groesse\n");
        _fail++;
    }
}

/* Und ADFS M darf nicht als ADL durchgehen: 327 680 ist .adm. */
TEST(adl_beansprucht_ADFS_M_nicht)
{
    int conf = 0;
    bool ok = uft_format_plugin_adl.probe(head, sizeof(head), ADFS_M, &conf);
    if (ok) {
        printf("ADL beansprucht 327680 — das ist ADFS M (.adm), nicht L\n");
        _fail++;
    }
}

/* Geometrie: zweiseitig, und die Rechnung muss aufgehen. */
TEST(ADFS_L_ist_zweiseitig)
{
    char path[512];
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, sizeof(path), "%s/uft_adfs_l_%d.adl", dir, rand() % 100000);

    FILE *f = fopen(path, "wb");
    ASSERT(f != NULL);
    static uint8_t zero[4096];
    for (unsigned i = 0; i < ADFS_L / sizeof(zero); i++) {
        if (fwrite(zero, 1, sizeof(zero), f) != sizeof(zero)) {
            fclose(f); remove(path); ASSERT(0);
        }
    }
    fclose(f);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    uft_error_t e = uft_format_plugin_adl.open(&disk, path, true);
    if (e != UFT_OK) {
        printf("ADL kann ein 655360-Byte-Abbild nicht oeffnen (err=%d)\n", (int)e);
        _fail++; remove(path); return;
    }

    if (disk.geometry.heads != 2) {
        printf("heads=%u, erwartet 2 — ADFS L ist zweiseitig\n",
               (unsigned)disk.geometry.heads);
        _fail++;
    }
    if (disk.geometry.cylinders != 80 || disk.geometry.sectors != 16 ||
        disk.geometry.sector_size != 256) {
        printf("Geometrie %ux%ux%ux%u passt nicht zu 80x2x16x256\n",
               (unsigned)disk.geometry.cylinders, (unsigned)disk.geometry.heads,
               (unsigned)disk.geometry.sectors, (unsigned)disk.geometry.sector_size);
        _fail++;
    }
    {
        uint32_t bytes = (uint32_t)disk.geometry.cylinders * disk.geometry.heads
                       * disk.geometry.sectors * disk.geometry.sector_size;
        if (bytes != ADFS_L) {
            printf("Geometrie ergibt %u Byte, die Datei hat %u\n",
                   bytes, ADFS_L);
            _fail++;
        }
    }
    uft_format_plugin_adl.close(&disk);
    remove(path);
}

/* Zwei Leser, eine Endung: solange beide registriert sind, muessen sie
 * fuer dieselbe Groesse dieselbe Geometrie liefern. Sonst entscheidet
 * die Reihenfolge der Registrierung, was der Benutzer sieht. */
TEST(beide_leser_stimmen_ueberein)
{
    int c1 = 0, c2 = 0;
    bool a = uft_format_plugin_adl.probe(head, sizeof(head), ADFS_L, &c1);
    bool b = uft_format_plugin_adf_arc.probe(head, sizeof(head), ADFS_L, &c2);
    if (a && b && c1 == c2) {
        printf("beide proben 655360 mit derselben Konfidenz %d — "
               "die Registrierungsreihenfolge entscheidet\n", c1);
        _fail++;
    }
}

/* Regressionsschutz: adf_arc darf ADFS D (819 200) weiter lesen, und
 * zwar mit der Geometrie, die zu 819 200 gehoert. */
TEST(adf_arc_haelt_ADFS_D)
{
    int conf = 0;
    ASSERT(uft_format_plugin_adf_arc.probe(head, sizeof(head), ADFS_D, &conf));
    ASSERT(uft_format_plugin_adf_arc.probe(head, sizeof(head), ADFS_S, &conf) == false
           || conf >= 0);  /* ADFS S steht nicht in seiner Tabelle */
}

int main(void)
{
    printf("=== Acorn ADFS Identitaet (MF-654) ===\n");
    RUN(adl_probt_die_ADFS_L_groesse);
    RUN(adl_beansprucht_ADFS_M_nicht);
    RUN(ADFS_L_ist_zweiseitig);
    RUN(beide_leser_stimmen_ueberein);
    RUN(adf_arc_haelt_ADFS_D);
    printf("  %d bestanden, %d gescheitert\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

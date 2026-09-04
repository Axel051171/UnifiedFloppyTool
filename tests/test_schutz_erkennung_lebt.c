/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_schutz_erkennung_lebt.c
 * @brief Die Selbsttests der Schutz-Erkennung, erstmals ausgefuehrt (MF-851).
 *
 * ── Woher diese Faelle kommen ────────────────────────────────────────
 *
 * Sie standen als `#ifdef UFT_UNIT_TESTS`-Block am Ende von
 * `src/protection/uft_protection_detect.c`. Gemessen ueber `git ls-files`:
 * **`UFT_UNIT_TESTS` wird im ganzen Baum nirgends definiert** — weder im
 * qmake-`.pro`, noch in einer `CMakeLists.txt`, noch als
 * Compilerschalter. Der Block wurde nie uebersetzt und nie ausgefuehrt.
 *
 * Das ist dieselbe Klasse wie MF-830 (21 Testdateien, deren `assert()`
 * unter `-DNDEBUG` ein leerer Ausdruck war) und MF-596 (32 Testdateien,
 * die ihren Erfolg bedingungslos zaehlten): ein Prueflauf, der nicht
 * laufen kann. Eingetragen als P3-89.
 *
 * ── Warum das hier besonders zaehlt ──────────────────────────────────
 *
 * `docs/OPEN_ITEMS.md` P0-2 haelt fest, dass der Katalog der 55+
 * benannten Schutzverfahren **keinen Aufrufer** hat — Bestand, nicht
 * Faehigkeit. Was davon erreichbar IST, ist die Signal-Erkennung und
 * drei heuristisch benannte Schemata; genau die pruefen diese Faelle.
 * Es ist also der erreichbare Rest des Subsystems, und er war unbewacht.
 *
 * ── Was uebernommen wurde und was nicht ──────────────────────────────
 *
 * Uebernommen: alle fuenf Faelle, inhaltlich unveraendert. Die Zusagen
 * sind NICHT abgeschwaecht worden, um sie gruen zu bekommen — wo eine
 * nicht traegt, steht das als Befund da, nicht als angepasste Erwartung.
 *
 * Ergaenzt: je eine Gegenprobe zu den beiden Erkennern. Ein Erkenner,
 * der auf seinem eigenen Muster anschlaegt, ist noch kein Erkenner —
 * er muss auf einem Puffer ohne das Muster SCHWEIGEN. Diese Haelfte
 * fehlte in allen fuenf urspruenglichen Faellen.
 */
#include "uft/uft_protection_detect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-44s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

TEST(vmax_erkennung)
{
    uint8_t track[100];
    memset(track, 0x00, sizeof track);
    memcpy(track + 20, UFT_VMAX_MARKERS, sizeof(UFT_VMAX_MARKERS));

    uft_protection_result_t result;
    memset(&result, 0, sizeof result);
    const uint8_t *pos = uft_prot_detect_vmax(track, sizeof track, &result);

    ASSERT(pos != NULL);
    ASSERT(pos == track + 20);
    ASSERT(result.type == UFT_PROT_VMAX);
    ASSERT(result.confidence >= 90);
}

TEST(vmax_schweigt_ohne_marke)
{
    /* GEGENPROBE, im Original nicht vorhanden.
     *
     * Ein Erkenner, der nur auf seinem eigenen Muster geprueft wird,
     * kann auch einer sein, der IMMER anschlaegt. Ein Puffer aus lauter
     * Nullen traegt keine V-MAX-Marke. */
    uint8_t track[100];
    memset(track, 0x00, sizeof track);

    uft_protection_result_t result;
    memset(&result, 0, sizeof result);
    ASSERT(uft_prot_detect_vmax(track, sizeof track, &result) == NULL);
}

TEST(rapidlok_erkennung)
{
    uint8_t track[300];
    memset(track, 0x00, sizeof track);
    memset(track + 10, 0xFF, 25);      /* 25 Sync-Bytes */
    track[35] = 0x55;                  /* Kennbyte      */
    memset(track + 36, 0x7B, 170);     /* 170 Kopfbytes */

    uft_protection_result_t result;
    memset(&result, 0, sizeof result);
    const uint8_t *pos = uft_prot_detect_rapidlok(track, sizeof track, &result);

    ASSERT(pos != NULL);
    ASSERT(result.type == UFT_PROT_RAPIDLOK);
    ASSERT(result.confidence >= 90);
}

TEST(rapidlok_schweigt_ohne_kopf)
{
    /* GEGENPROBE: 25 Sync-Bytes ALLEIN sind kein RapidLok — die kommen
     * auf jeder Spur vor. Ohne Kennbyte und Kopf darf nichts gemeldet
     * werden. */
    uint8_t track[300];
    memset(track, 0x00, sizeof track);
    memset(track + 10, 0xFF, 25);

    uft_protection_result_t result;
    memset(&result, 0, sizeof result);
    ASSERT(uft_prot_detect_rapidlok(track, sizeof track, &result) == NULL);
}

TEST(schwache_bits)
{
    uint8_t read1[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    uint8_t read2[] = { 0x01, 0x02, 0xFF, 0x04, 0x05 };
    uint8_t read3[] = { 0x01, 0x02, 0xAA, 0x04, 0x05 };

    const uint8_t *reads[] = { read1, read2, read3 };
    uint8_t weak_map[5];
    size_t weak_count = 0;

    ASSERT(uft_prot_detect_weak_bits(reads, 3, 5, weak_map, &weak_count));
    ASSERT(weak_count == 1);
    ASSERT(weak_map[2] == 1);
}

TEST(schwache_bits_schweigen_bei_einigkeit)
{
    /* GEGENPROBE: drei identische Lesungen tragen KEIN schwaches Bit.
     * Ohne diesen Fall waere eine Fassung gruen, die jede Stelle als
     * schwach meldet. */
    uint8_t r[] = { 0x11, 0x22, 0x33 };
    const uint8_t *reads[] = { r, r, r };
    uint8_t weak_map[3];
    size_t weak_count = 99;

    bool found = uft_prot_detect_weak_bits(reads, 3, 3, weak_map, &weak_count);
    ASSERT(!found);
    ASSERT(weak_count == 0);
}

TEST(kontextverwaltung)
{
    uft_protection_ctx_t ctx;
    ASSERT(uft_protection_ctx_init(&ctx) == 0);

    uft_protection_result_t result;
    memset(&result, 0, sizeof result);
    result.type = UFT_PROT_VMAX;
    result.name = "Test";
    result.confidence = 90;

    ASSERT(uft_protection_ctx_add_result(&ctx, &result) == 0);
    ASSERT(ctx.result_count == 1);

    uft_protection_ctx_free(&ctx);
}

TEST(namensfunktionen)
{
    /* `uft_protection_family_name()` traegt die Familienzuordnung, die
     * ein Bericht ausgibt. */
    ASSERT(strcmp(uft_protection_family_name(UFT_PROT_COPYLOCK),
                  "Rob Northen") == 0);
}

/*
 * GRENZE DES HEBENS, ausdruecklich (MF-851).
 *
 * Der urspruengliche Block prueft eine sechste Zusage:
 *
 *     assert(strcmp(uft_protection_type_name_local(UFT_PROT_VMAX),
 *                   "V-MAX") == 0);
 *
 * Sie ist hier NICHT enthalten, und zwar aus einem Grund, der zum
 * Befund gehoert: `uft_protection_type_name_local()` ist `static`. Ein
 * Test ausserhalb der Uebersetzungseinheit kommt nicht heran — gemessen
 * am Binder: „undefined reference".
 *
 * Das ist die Kehrseite des `#ifdef`-Blocks: er kann pruefen, was
 * niemand sonst sehen kann. Genau deshalb darf er nicht die EINZIGE
 * Pruefung sein — er lief hier nie, und die Funktion war vier Monate
 * unbewacht, waehrend ein gleichnamiger Zwilling in
 * `uft_protection_params.c` mit ZWEI Fehlern danebenlag (MF-842).
 *
 * Nicht getan: die Funktion oeffentlich machen, um sie pruefbar zu
 * bekommen. MF-842 hat den Zwilling geloescht, WEIL zwei gleiche Namen
 * die Verwechslung ermoeglichen; den verbliebenen Namen jetzt weiter
 * zu tragen, liefe der Reparatur zuwider. Der Zugriff ueber
 * `uft_protection_family_name()` deckt denselben Datensatz ab.
 */

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Schutz-Erkennung: Selbsttests erstmals ausgefuehrt (MF-851) ===\n");
    RUN(vmax_erkennung);
    RUN(vmax_schweigt_ohne_marke);
    RUN(rapidlok_erkennung);
    RUN(rapidlok_schweigt_ohne_kopf);
    RUN(schwache_bits);
    RUN(schwache_bits_schweigen_bei_einigkeit);
    RUN(kontextverwaltung);
    RUN(namensfunktionen);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

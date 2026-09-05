/**
 * @file test_g64_speedzonen.c
 * @brief Die Speed-Zonen einer von UFT erzeugten G64 (MF-877).
 *
 * `src/formats/g64/uft_g64.c` fuehrte eine EIGENE Zonentabelle, und sie war
 * gegenueber allem, was daneben liegt, GESPIEGELT: Spur 1 bekam Zone 0,
 * Spur 42 bekam Zone 3. Der Wert wird in jede ueber das registrierte Plugin
 * angelegte Datei geschrieben (`.create = g64_create`), ist also kein
 * Schoenheitsfehler in einer Konstante, sondern steht in der Ausgabe.
 *
 * Drei unabhaengige Belege sagen dasselbe, und alle drei sagen das Gegenteil
 * der alten Tabelle:
 *
 *   1. docs/format_specs/commodore/G64.TXT:285-291 (Peter Schepers, Rev 1.9)
 *          Track Range   Storage in Bytes   Speed Zone
 *             1-17            7820             3  (slowest writing speed)
 *            18-24            7170             2
 *            25-30            6300             1
 *            31-4x            6020             0  (fastest writing speed)
 *
 *   2. tests/corpus_free/vice_c1541_35trk.g64 — eine von VICE c1541 erzeugte
 *      Aufnahme. Ihre Speed-Tabelle, ausgelesen: Spur 1 und 17 -> 3,
 *      18 und 24 -> 2, 25 und 30 -> 1, 31 und 35 -> 0.
 *
 *   3. Die SSOT im Baum, `uft_cbm_speed_zone(UFT_CBM_1541, t)`
 *      (src/formats/cbm/uft_cbm_geometry.c:30-35), von der der ZWEITE
 *      G64-Schreiber `src/formats/c64/uft_d64_g64.c:58` seinen Wert bereits
 *      bezieht.
 *
 * Der Test prueft deshalb nicht gegen eine vierte Abschrift der Zahlen,
 * sondern gegen die SSOT und gegen die echte Aufnahme — und er enthaelt eine
 * Gegenprobe, damit er nicht gruen sein kann, wenn er nichts prueft.
 */

#include "uft/uft_format_plugin.h"
#include "uft/formats/cbm/uft_cbm_geometry.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

extern const uft_format_plugin_t uft_format_plugin_g64;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-52s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* ------------------------------------------------------------------ */
/* G64-Kopf, so wie er auf der Platte liegt                            */
/*   "GCR-1541" (8) | version (1) | num_entries (1) | max_size (2 LE)  */
/*   dann num_entries x 4 Byte Spur-Offsets (LE)                       */
/*   dann num_entries x 4 Byte Speed-Eintraege (LE)                    */
/* Halbspur-Index i entspricht Spur (i/2)+1.                           */
/* ------------------------------------------------------------------ */
#define G64_HDR_LEN 12

static uint32_t rd_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Liest die Speed-Tabelle einer G64-Datei.
 * @param out       Ziel, mindestens max_out Eintraege
 * @param max_out   Platz in out
 * @return Anzahl gelesener Eintraege, oder -1 bei Fehler
 */
static int read_speed_table(const char* path, uint32_t* out, int max_out)
{
    FILE* f = fopen(path, "rb");
    if (!f) return -1;

    uint8_t hdr[G64_HDR_LEN];
    if (fread(hdr, 1, G64_HDR_LEN, f) != G64_HDR_LEN) { fclose(f); return -1; }
    if (memcmp(hdr, "GCR-1541", 8) != 0) { fclose(f); return -1; }

    int entries = hdr[9];
    if (entries <= 0 || entries > max_out) { fclose(f); return -1; }

    /* Offset-Tabelle ueberspringen, Speed-Tabelle folgt direkt danach. */
    if (fseek(f, G64_HDR_LEN + entries * 4, SEEK_SET) != 0) { fclose(f); return -1; }

    for (int i = 0; i < entries; i++) {
        uint8_t buf[4];
        if (fread(buf, 1, 4, f) != 4) { fclose(f); return -1; }
        out[i] = rd_le32(buf);
    }
    fclose(f);
    return entries;
}

/* ------------------------------------------------------------------ */
/* 1. Die SSOT selbst — die vier Zonengrenzen aus G64.TXT:285-291      */
/* ------------------------------------------------------------------ */
TEST(die_ssot_zaehlt_von_innen_langsam_nach_aussen_schnell)
{
    ASSERT(uft_cbm_speed_zone(UFT_CBM_1541,  1) == 3);
    ASSERT(uft_cbm_speed_zone(UFT_CBM_1541, 17) == 3);
    ASSERT(uft_cbm_speed_zone(UFT_CBM_1541, 18) == 2);
    ASSERT(uft_cbm_speed_zone(UFT_CBM_1541, 24) == 2);
    ASSERT(uft_cbm_speed_zone(UFT_CBM_1541, 25) == 1);
    ASSERT(uft_cbm_speed_zone(UFT_CBM_1541, 30) == 1);
    ASSERT(uft_cbm_speed_zone(UFT_CBM_1541, 31) == 0);
    ASSERT(uft_cbm_speed_zone(UFT_CBM_1541, 42) == 0);
}

/* ------------------------------------------------------------------ */
/* 2. Gegenprobe: der Test darf nicht gruen sein, wenn er nichts sieht */
/* ------------------------------------------------------------------ */
TEST(gegenprobe_die_zonen_sind_nicht_alle_gleich)
{
    int z1  = uft_cbm_speed_zone(UFT_CBM_1541,  1);
    int z18 = uft_cbm_speed_zone(UFT_CBM_1541, 18);
    int z25 = uft_cbm_speed_zone(UFT_CBM_1541, 25);
    int z31 = uft_cbm_speed_zone(UFT_CBM_1541, 31);

    /* Vier verschiedene Werte, sonst prueft der Vergleich oben nichts. */
    ASSERT(z1 != z18 && z18 != z25 && z25 != z31);
    ASSERT(z1 != z31);
    /* Und die Richtung: innen langsam (3), aussen schnell (0). */
    ASSERT(z1 > z31);
}

/* ------------------------------------------------------------------ */
/* 3. Der Produktionspfad: was das registrierte Plugin WIRKLICH        */
/*    in die Datei schreibt                                            */
/* ------------------------------------------------------------------ */
static char g_created_path[512];
static uint32_t g_created_speed[256];
static int g_created_entries = 0;

static int erzeuge_g64_ueber_das_plugin(void)
{
    const char* path = "test_g64_speedzonen_out.g64";
    uft_disk_t disk;
    uft_geometry_t geom;

    memset(&disk, 0, sizeof(disk));
    memset(&geom, 0, sizeof(geom));
    geom.cylinders   = 35;
    geom.heads       = 1;
    geom.sectors     = 21;
    geom.sector_size = 256;

    if (!uft_format_plugin_g64.create) return -1;
    if (uft_format_plugin_g64.create(&disk, path, &geom) != UFT_OK) return -1;
    if (uft_format_plugin_g64.close) uft_format_plugin_g64.close(&disk);

    snprintf(g_created_path, sizeof(g_created_path), "%s", path);
    g_created_entries = read_speed_table(path, g_created_speed,
                                         (int)(sizeof(g_created_speed) / sizeof(g_created_speed[0])));
    return g_created_entries;
}

TEST(die_erzeugte_datei_traegt_die_zonen_der_ssot)
{
    ASSERT(g_created_entries > 0);
    /* 35 Zylinder x 2 Halbspur-Slots */
    ASSERT(g_created_entries == 70);

    for (int i = 0; i < g_created_entries; i++) {
        int track = (i / 2) + 1;
        int soll  = uft_cbm_speed_zone(UFT_CBM_1541, track);
        if ((int)g_created_speed[i] != soll) {
            printf("\n        Halbspur %d (Spur %d): Datei sagt %u, SSOT sagt %d\n",
                   i, track, g_created_speed[i], soll);
        }
        ASSERT((int)g_created_speed[i] == soll);
    }
}

TEST(gegenprobe_die_erzeugte_datei_ist_nicht_uniform)
{
    ASSERT(g_created_entries >= 70);
    /* Spur 1 -> Index 0, Spur 31 -> Index 60 */
    ASSERT(g_created_speed[0] != g_created_speed[60]);
    ASSERT(g_created_speed[0] == 3);
    ASSERT(g_created_speed[60] == 0);
}

/* ------------------------------------------------------------------ */
/* 4. Die echte Aufnahme — VICE c1541, nicht UFT                       */
/* ------------------------------------------------------------------ */
/**
 * Verglichen werden die GANZEN Spuren, also die geraden Slots. Fuer die
 * ungeraden (Halbspur-)Slots weichen die beiden Erzeuger ab, und zwar
 * gemessen an derselben Datei:
 *
 *   VICE c1541 setzt dort 0, und die zugehoerigen Offsets sind ebenfalls 0
 *   — die 42 ungeraden Slots dieser Aufnahme tragen keine Daten (gemessen:
 *   35 der 42 geraden Slots haben einen Offset, keiner der 42 ungeraden).
 *   Ein Speed-Eintrag ohne Spur-Daten sagt nichts aus.
 *
 *   UFT traegt dort die Zone der Spur ein, auf der der Halbspur-Slot
 *   radial liegt.
 *
 * Beides ist mit G64.TXT vereinbar; die Angabe ist nur dort aussagekraeftig,
 * wo ein Offset steht. Der Test stellt die Abweichung fest, statt sie zu
 * verschweigen, und prueft die geraden Slots hart.
 */
TEST(die_erzeugte_datei_stimmt_mit_der_vice_aufnahme_ueberein)
{
    const char* korpus = UFT_CORPUS_DIR "/vice_c1541_35trk.g64";
    uint32_t vice[256];
    int n = read_speed_table(korpus, vice, (int)(sizeof(vice) / sizeof(vice[0])));
    if (n < 0) {
        printf("UEBERSPRUNGEN (Korpus-Aufnahme fehlt: %s)\n", korpus);
        _pass++;              /* nur DIESER Fall entfaellt, benannt */
        _last_fail = _fail;
        return;
    }

    ASSERT(g_created_entries > 0);
    int gemeinsam = (n < g_created_entries) ? n : g_created_entries;
    ASSERT(gemeinsam >= 70);

    int verglichen = 0;
    for (int i = 0; i < gemeinsam; i += 2) {     /* nur ganze Spuren */
        if (vice[i] != g_created_speed[i]) {
            printf("\n        Spur %d: VICE sagt %u, UFT schreibt %u\n",
                   (i / 2) + 1, vice[i], g_created_speed[i]);
        }
        ASSERT(vice[i] == g_created_speed[i]);
        verglichen++;
    }
    /* Gegenprobe: die Schleife hat wirklich alle 35 Spuren angefasst. */
    ASSERT(verglichen == 35);
}

int main(void)
{
    printf("=== G64-Speed-Zonen gegen SSOT und VICE-Aufnahme (MF-877) ===\n");

    int n = erzeuge_g64_ueber_das_plugin();
    if (n < 0) {
        printf("  [SETUP] uft_format_plugin_g64.create() lieferte keine lesbare Datei\n");
    }

    RUN(die_ssot_zaehlt_von_innen_langsam_nach_aussen_schnell);
    RUN(gegenprobe_die_zonen_sind_nicht_alle_gleich);
    RUN(die_erzeugte_datei_traegt_die_zonen_der_ssot);
    RUN(gegenprobe_die_erzeugte_datei_ist_nicht_uniform);
    RUN(die_erzeugte_datei_stimmt_mit_der_vice_aufnahme_ueberein);

    if (g_created_path[0]) remove(g_created_path);

    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

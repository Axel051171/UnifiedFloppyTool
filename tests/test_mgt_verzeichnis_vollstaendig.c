/* SPDX-License-Identifier: MIT */
/**
 * @file test_mgt_verzeichnis_vollstaendig.c
 * @brief `uft_mgt_read_directory()` las 8 der 80 Eintraege — still (MF-933)
 *
 * ── DER BEFUND ───────────────────────────────────────────────────────
 *
 * `include/uft/formats/uft_mgt.h` fuehrt zwei Konstanten, die einander
 * widersprechen:
 *
 *     #define MGT_DIR_ENTRIES         80    // Maximum directory entries
 *     #define MGT_SECTORS_PER_DIR     4     // 4 sectors for directory
 *
 * 4 Sektoren zu 512 Byte fassen bei 256 Byte je Eintrag genau **8**
 * Eintraege, nicht 80. Und `uft_mgt_read_directory()` folgte der
 * kleineren Zahl: es lief ueber `MGT_SECTORS_PER_DIR` Sektoren von
 * `track_data[0]` und kehrte mit `UFT_OK` zurueck. Eine Diskette mit
 * mehr als acht Dateien lieferte acht — ohne Hinweis.
 *
 * Das ist dieselbe Klasse wie MF-928 (G64 schrieb die halbe Diskette)
 * und MF-930 (Schreiberfolg ohne Tat): **Erfolg gemeldet fuer eine
 * Teilarbeit.**
 *
 * ── DIE REFERENZ ─────────────────────────────────────────────────────
 *
 * Zwei Quellen im Baum sagen unabhaengig dasselbe:
 *
 *  1. `src/samdisk/Util.cpp` (vendort, MIT) laeuft das Verzeichnis so:
 *
 *         for (uint8_t cyl = 0; cyl < di.dir_tracks; ++cyl)
 *             for (uint8_t sec = 1; sec <= MGT_SECTORS; ++sec)
 *                 for (entry = 0; entry < 2; ++entry)
 *
 *     also `dir_tracks * 10 Sektoren * 2 Eintraege`.
 *
 *  2. UFTs EIGENE Konstante `MGT_DIR_ENTRIES 80`.
 *
 * Beide treffen sich bei `dir_tracks = 4`: 4 * 10 * 2 = 80. Das
 * Verzeichnis liegt auf den Zylindern 0–3, Seite 0.
 *
 * **Was NICHT belegt ist, steht hier ausdruecklich:** `dir_tracks` ist
 * bei SAMdisk eine Variable, die `GetDiskInfo()` aus Sektor 0 liest.
 * Deren Definition steht in `SAMCoupe.h` — und diese Datei FEHLT im
 * vendorten Bestand (gemessen MF-933; `src/samdisk/mgt.cpp` liesse sich
 * damit nicht uebersetzen, was nicht auffaellt, weil `src/samdisk` nur
 * im INCLUDEPATH steht und nicht in SOURCES). Eine Diskette mit einem
 * abweichenden `dir_tracks` ist deshalb NICHT abgedeckt; das steht als
 * eigener offener Punkt.
 *
 * ── ROTBEWEIS ────────────────────────────────────────────────────────
 *
 * Vor MF-933 meldet dieser Test 8 statt der gepflanzten 12 Eintraege.
 */

#include "uft/formats/uft_mgt.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* Ein Abbild mit `wieviele` Verzeichniseintraegen, verteilt ueber die
 * Zylinder 0..3 Seite 0 — zwei Eintraege je Sektor, zehn Sektoren je
 * Spur. Genau die Anordnung, die src/samdisk/Util.cpp laeuft. */
static uft_disk_image_t *baue_mgt(int wieviele)
{
    uft_disk_image_t *d = uft_disk_alloc(MGT_CYLINDERS, MGT_HEADS);
    if (!d) return NULL;
    d->tracks = MGT_CYLINDERS;
    d->heads = MGT_HEADS;
    d->sectors_per_track = MGT_SECTORS;
    d->bytes_per_sector = MGT_SECTOR_SIZE;

    int gesetzt = 0;
    for (int c = 0; c < MGT_CYLINDERS; c++) {
        for (int h = 0; h < MGT_HEADS; h++) {
            size_t idx = (size_t)c * MGT_HEADS + h;
            uft_track_t *t = uft_track_alloc(MGT_SECTORS, 0);
            if (!t) return NULL;
            t->cylinder = (uint8_t)c;
            t->head = (uint8_t)h;
            t->sector_count = MGT_SECTORS;
            for (int s = 0; s < MGT_SECTORS; s++) {
                t->sectors[s].data = calloc(1, MGT_SECTOR_SIZE);
                t->sectors[s].data_size = MGT_SECTOR_SIZE;
                t->sectors[s].id.sector = (uint8_t)(s + MGT_FIRST_SECTOR);
                t->sectors[s].id.cylinder = (uint8_t)c;
                t->sectors[s].id.head = (uint8_t)h;

                /* Verzeichnis: Zylinder 0-3, Seite 0. */
                if (h == 0 && c < 4) {
                    for (int e = 0; e < 2 && gesetzt < wieviele; e++) {
                        uint8_t *p = t->sectors[s].data + e * MGT_DIR_ENTRY_SIZE;
                        p[0] = 1;                       /* type: BASIC */
                        snprintf((char *)(p + 1), 11, "DATEI%03d", gesetzt);
                        gesetzt++;
                    }
                }
            }
            d->track_data[idx] = t;
        }
    }
    return d;
}

/* ─────────────────────────────────────────────────────────────────────
 *  Zwoelf Eintraege sind zwoelf, nicht acht.
 *
 *  Zwoelf ist mit Absicht gewaehlt: es passt nicht mehr in die vier
 *  Sektoren, die die alte Fassung ablief (8), liegt aber noch auf
 *  Zylinder 0. Der Fehler ist damit die SEKTOR-Schranke, nicht die
 *  Spur-Schranke — beide werden getrennt geprueft.
 * ───────────────────────────────────────────────────────────────────── */
TEST(zwoelf_eintraege_auf_zylinder_0)
{
    uft_disk_image_t *d = baue_mgt(12);
    ASSERT(d != NULL);

    mgt_dir_entry_t eintraege[MGT_DIR_ENTRIES];
    size_t n = 0;
    ASSERT(uft_mgt_read_directory(d, eintraege, MGT_DIR_ENTRIES, &n) == UFT_OK);

    if (n != 12) {
        printf("  (gemeldet: %u von 12)\n", (unsigned)n);
    }
    ASSERT(n == 12);

    uft_disk_free(d);
}

/* Achtzig Eintraege — die Zahl, die der eigene Header nennt. Sie liegen
 * ueber alle vier Verzeichnis-Zylinder verteilt, also faellt hier auch
 * die Spur-Schranke auf. */
TEST(achtzig_eintraege_ueber_vier_zylinder)
{
    uft_disk_image_t *d = baue_mgt(MGT_DIR_ENTRIES);
    ASSERT(d != NULL);

    mgt_dir_entry_t eintraege[MGT_DIR_ENTRIES];
    size_t n = 0;
    ASSERT(uft_mgt_read_directory(d, eintraege, MGT_DIR_ENTRIES, &n) == UFT_OK);

    if (n != MGT_DIR_ENTRIES) {
        printf("  (gemeldet: %u von %d)\n", (unsigned)n, MGT_DIR_ENTRIES);
    }
    ASSERT(n == MGT_DIR_ENTRIES);

    /* Der letzte Eintrag liegt auf Zylinder 3 — er beweist, dass die
     * Spur-Schranke gefallen ist, nicht nur die Sektor-Schranke. */
    ASSERT(eintraege[MGT_DIR_ENTRIES - 1].type == 1);
    ASSERT(memcmp(eintraege[MGT_DIR_ENTRIES - 1].filename, "DATEI079", 8) == 0);

    uft_disk_free(d);
}

/* Die Schranke des AUFRUFERS wird geachtet: wer Platz fuer 5 anbietet,
 * bekommt hoechstens 5. Ohne diese Zusicherung waere der Fix ein
 * Pufferueberlauf. */
TEST(die_schranke_des_aufrufers_gilt)
{
    uft_disk_image_t *d = baue_mgt(MGT_DIR_ENTRIES);
    ASSERT(d != NULL);

    mgt_dir_entry_t eintraege[8];
    memset(eintraege, 0, sizeof(eintraege));
    size_t n = 0;
    ASSERT(uft_mgt_read_directory(d, eintraege, 5, &n) == UFT_OK);
    ASSERT(n == 5);

    /* Die drei Plaetze dahinter wurden NICHT beschrieben. */
    for (int i = 5; i < 8; i++) {
        ASSERT(eintraege[i].type == 0);
    }

    uft_disk_free(d);
}

/* Ein Abbild ohne Verzeichnis-Spuren darf nicht abstuerzen. */
TEST(fehlende_spuren_stuerzen_nicht_ab)
{
    uft_disk_image_t *d = uft_disk_alloc(MGT_CYLINDERS, MGT_HEADS);
    ASSERT(d != NULL);
    d->tracks = MGT_CYLINDERS;
    d->heads = MGT_HEADS;
    /* track_data bleibt durchgehend NULL. */

    mgt_dir_entry_t eintraege[MGT_DIR_ENTRIES];
    size_t n = 123;
    uft_error_t r = uft_mgt_read_directory(d, eintraege, MGT_DIR_ENTRIES, &n);
    ASSERT(r == UFT_OK || r == UFT_ERR_INVALID_PARAM);
    if (r == UFT_OK) ASSERT(n == 0);

    uft_disk_free(d);
}

int main(void)
{
    printf("=== MGT-Verzeichnis vollstaendig (MF-933) ===\n");
    RUN(zwoelf_eintraege_auf_zylinder_0);
    RUN(achtzig_eintraege_ueber_vier_zylinder);
    RUN(die_schranke_des_aufrufers_gilt);
    RUN(fehlende_spuren_stuerzen_nicht_ab);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}

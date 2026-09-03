/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_atari_dir_past_end.c
 * @brief Eintraege hinter der Verzeichnis-Endmarke (MF-835).
 *
 * ── Die Regel ────────────────────────────────────────────────────────────
 *
 * DOS 2.0s bricht die Verzeichnissuche beim ersten NIE BENUTZTEN Eintrag
 * ab. Alles dahinter ist fuer DOS unsichtbar — **die Daten liegen aber
 * noch auf der Diskette**. Genau die Klasse, die
 * `uft_amigados_extended.c` fuer AmigaDOS als `orphan_count` fuehrt und
 * dort sogar begruendet („data still on disk = forensically valuable"),
 * und die SEDs FAT-Graph als „Leiche" anzeigt.
 *
 * Quelle: Joe Allen, `atari-tools` `readme.md` §Filesystem format
 * (DOS 2.0s) — *„it stops searching when it encounters the first
 * directory entry which has never been used (flag byte bits 6 and 7
 * both 0)"*.
 *
 * ── Was gemessen wurde ───────────────────────────────────────────────────
 *
 * Der Prueflauf dafuer EXISTIERT: `atari_check.c:232` meldet
 * „Eintrag #%d nach Ende-Marker". Er kann aber **nicht feuern**, weil der
 * Parser die Beweise vorher wegwirft — `atari_dos2.c:391-400`:
 *
 *     if (entry->status == DIR_FLAG_NEVER_USED) {
 *         disk->dir_entry_count = idx;
 *         for (int rest = idx; rest < MAX_FILES; rest++)
 *             disk->directory[rest].status = DIR_FLAG_NEVER_USED;
 *         return ATARI_OK;
 *     }
 *
 * Beim ersten Nullstatus hoert er auf zu LESEN und ueberschreibt alle
 * restlichen Plaetze mit „nie benutzt". Danach ist die Bedingung
 * `found_end && status != NEVER_USED` in `check_directory()`
 * unerfuellbar — auf JEDER Diskette.
 *
 * Fuenfter belegter Fall dieser Klasse in diesem Baum: eine Pruefung, die
 * gruen ist, WEIL sie unmoeglich ist. Und der einzige bisher, bei dem
 * nicht die Pruefung fehlt, sondern ihr Gegenstand zerstoert wird.
 *
 * ── Zweitens: die Endmarke war zu streng definiert ───────────────────────
 *
 * `DIR_FLAG_NEVER_USED` ist `0x00`, und geprueft wurde auf GLEICHHEIT.
 * Die Quelle sagt aber „bits 6 and 7 both 0" — ein Eintrag mit z. B.
 * `0x02` (DOS2_CREATED gesetzt, b6/b7 frei) beendet die DOS-Suche
 * ebenfalls, wurde von UFT aber als gewoehnlicher Eintrag weitergezaehlt.
 * UFT sah damit Dateien, die DOS nicht sieht, und sagte es nicht.
 */
#include "uft/formats/atari_dos.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define SD_SECTORS   720u
#define SD_SIZE      128u
#define ST_IN_USE    (DIR_FLAG_IN_USE | DIR_FLAG_DOS2_CREATED)   /* 0x42 */

/* Legt einen Verzeichniseintrag in `raw` ab (16 Byte). */
static void eintrag(uint8_t *raw, uint8_t status, uint16_t count,
                    uint16_t first, const char *name8, const char *ext3)
{
    memset(raw, 0, ATARI_DOS_DIR_ENTRY_SIZE);
    raw[0] = status;
    raw[1] = (uint8_t)(count & 0xFF);
    raw[2] = (uint8_t)(count >> 8);
    raw[3] = (uint8_t)(first & 0xFF);
    raw[4] = (uint8_t)(first >> 8);
    memset(raw + 5, ' ', 11);
    for (int i = 0; i < 8 && name8[i]; i++) raw[5 + i]  = (uint8_t)name8[i];
    for (int i = 0; i < 3 && ext3[i];  i++) raw[13 + i] = (uint8_t)ext3[i];
}

/* Baut eine SD-Diskette im Speicher mit:
 *   Eintrag 0  belegt   ("ERSTE.TXT")
 *   Eintrag 1  NIE BENUTZT  <- Endmarke
 *   Eintrag 2  belegt   ("VERSTECK.TXT")  <- fuer DOS unsichtbar
 *   Eintrag 3  Status 0x02: b6/b7 frei, also nach der Quelle EBENFALLS
 *              eine Endmarke — von UFT bisher als Eintrag gezaehlt.  */
static atari_disk_t *baue_disk(void)
{
    atari_disk_t *d = (atari_disk_t *)calloc(1, sizeof(atari_disk_t));
    if (!d) return NULL;
    d->data_size = SD_SECTORS * SD_SIZE;
    d->data = (uint8_t *)calloc(1, d->data_size);
    if (!d->data) { free(d); return NULL; }

    d->density               = DENSITY_SINGLE;
    d->fs_type               = FS_DOS_20;
    d->sector_size           = SD_SIZE;
    d->total_sectors         = SD_SECTORS;
    d->data_bytes_per_sector = 125;

    uint8_t *dir = &d->data[(DIR_SECTOR_START - 1u) * SD_SIZE];
    eintrag(dir + 0 * 16, ST_IN_USE, 1, 400, "ERSTE",    "TXT");
    /* Eintrag 1 bleibt 0x00 — die Endmarke. */
    eintrag(dir + 2 * 16, ST_IN_USE, 1, 401, "VERSTECK", "TXT");
    eintrag(dir + 3 * 16, DIR_FLAG_DOS2_CREATED, 1, 402, "ZWEITMRK", "TXT");
    return d;
}

static void frei(atari_disk_t *d)
{
    if (!d) return;
    free(d->data);
    free(d);
}

TEST(der_parser_wirft_die_eintraege_hinter_der_endmarke_weg)
{
    /* Rotbeweis, Teil 1: die DATEN. Vor MF-835 ueberschrieb
     * `dos2_read_directory()` alles ab der Endmarke mit „nie benutzt" —
     * Eintrag 2 verlor Status, Name und Startsektor. */
    atari_disk_t *d = baue_disk();
    ASSERT(d != NULL);
    ASSERT(dos2_read_directory(d) == ATARI_OK);

    /* Eintrag 0 ist normal sichtbar. */
    ASSERT(d->directory[0].status == ST_IN_USE);
    ASSERT(d->directory[0].first_sector == 400);

    /* Eintrag 1 ist die Endmarke. */
    ASSERT(d->directory[1].status == DIR_FLAG_NEVER_USED);

    /* Eintrag 2 liegt DAHINTER und muss erhalten bleiben. */
    ASSERT(d->directory[2].status == ST_IN_USE);
    ASSERT(d->directory[2].first_sector == 401);
    ASSERT(d->directory[2].sector_count == 1);
    ASSERT(strncmp(d->directory[2].filename, "VERSTECK", 8) == 0);

    /* Und `dir_entry_count` muss weiter sagen, wo DOS aufhoert — die
     * Zahl ist die Sichtgrenze, nicht die Datenmenge. */
    ASSERT(d->dir_entry_count == 1);

    frei(d);
}

TEST(der_pruefer_meldet_die_eintraege_hinter_der_endmarke)
{
    /* Rotbeweis, Teil 2: der BEFUND. Der Pruefzweig gab es schon, er
     * konnte nur nie erreicht werden. */
    atari_disk_t *d = baue_disk();
    ASSERT(d != NULL);
    ASSERT(dos2_read_directory(d) == ATARI_OK);

    check_result_t r;
    memset(&r, 0, sizeof r);
    r.is_valid = true;
    ASSERT(check_directory(d, &r, false) == ATARI_OK);

    int gefunden = 0;
    for (uint32_t i = 0; i < r.issue_count; i++)
        if (strstr(r.issues[i].message, "Ende-Marke") ||
            strstr(r.issues[i].message, "Ende-Marker"))
            gefunden++;

    /* Zwei Eintraege liegen hinter der Marke: #2 (belegt) und #3
     * (Status 0x02 — nach der Quelle selbst eine Marke, aber mit
     * gesetzten unteren Bits, also gemeldet). */
    ASSERT(gefunden >= 1);

    /* Ein Fund, kein Mangel: die Diskette bleibt gueltig. */
    ASSERT(r.is_valid);

    free(r.issues);
    frei(d);
}

TEST(endmarke_ist_bit6_und_bit7_frei_nicht_status_null)
{
    /* Die Quelle sagt „flag byte bits 6 and 7 both 0", nicht
     * „status == 0". Eintrag 3 hat 0x02 — b6/b7 frei — und beendet die
     * DOS-Suche damit ebenfalls. */
    atari_disk_t *d = baue_disk();
    ASSERT(d != NULL);
    ASSERT(dos2_read_directory(d) == ATARI_OK);

    /* Der Rohstatus bleibt unveraendert erhalten … */
    ASSERT(d->directory[3].status == DIR_FLAG_DOS2_CREATED);
    /* … und weder belegt noch geloescht. */
    ASSERT(!d->directory[3].is_valid);
    ASSERT(!d->directory[3].is_deleted);

    frei(d);
}

int main(void)
{
    printf("=== Atari-Verzeichnis: Eintraege hinter der Endmarke (MF-835) ===\n");
    RUN(der_parser_wirft_die_eintraege_hinter_der_endmarke_weg);
    RUN(der_pruefer_meldet_die_eintraege_hinter_der_endmarke);
    RUN(endmarke_ist_bit6_und_bit7_frei_nicht_status_null);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_atari_verlorene_sektoren.c
 * @brief Der Reparaturlauf gab Sektoren frei, die eine Datei belegt (MF-850).
 *
 * ── Der Widerspruch im selben Werkzeug ───────────────────────────────
 *
 * MF-835 hat `check_directory()` beigebracht, Eintraege HINTER der
 * Verzeichnis-Endmarke zu melden — fuer DOS unsichtbar, die Daten aber
 * noch auf der Diskette. Der Text lautet woertlich:
 *
 *     „Daten vermutlich noch vorhanden"
 *
 * Drei weitere Durchgaenge desselben Pruefers wurden dabei NICHT
 * mitgezogen. Sie brechen weiter beim ersten nie benutzten Eintrag ab:
 *
 *     atari_check.c:338   check_sector_chains
 *     atari_check.c:501   check_cross_links
 *     atari_check.c:576   check_lost_sectors     <- der gefaehrliche
 *
 * ── Warum der dritte schwerer wiegt als die anderen zwei ─────────────
 *
 * `check_lost_sectors()` baut aus den Verzeichniseintraegen die Menge
 * `used_by_files[]` und nennt jeden Sektor „verloren", der in der VTOC
 * belegt ist, aber in dieser Menge fehlt. Bricht die Schleife an der
 * Endmarke ab, fehlen die Sektoren der versteckten Datei — und mit
 * `fix = true` folgt:
 *
 *     dos2_free_sector(disk, s);
 *     dos2_write_vtoc(disk);
 *
 * Das ist kein verpasster Befund, sondern eine SCHREIBENDE Aenderung.
 * Dasselbe Werkzeug sagt im einen Durchgang „wiederherstellbar" und
 * gibt die Sektoren im anderen frei — gegen den ersten Satz dieses
 * Projekts: „Kein Bit verloren. Keine stille Veraenderung."
 *
 * ── Warum es bis jetzt nicht auffiel ─────────────────────────────────
 *
 * Vor MF-835 hat der PARSER alle Eintraege ab der Marke mit „nie
 * benutzt" ueberschrieben. Es gab also keine versteckten Eintraege, die
 * jemand haette uebersehen koennen — der Fehler entstand erst in dem
 * Moment, als der Parser ehrlich wurde. Eine Reparatur, die eine zweite
 * Stelle noetig macht und sie nicht mitzieht, ist im Baum belegt:
 * MF-794 (`sad` rechnete an zwei Stellen unterschiedlich),
 * MF-847 (fuenf Feldfehler in dem Leser, den die Registry haelt).
 *
 * ── Quelle fuer die Regel selbst ─────────────────────────────────────
 *
 * DOS 2.0s bricht die Verzeichnissuche beim ersten Eintrag ab, dessen
 * Flagbits 6 und 7 beide frei sind (`DIR_IST_ENDMARKE`, atari_dos.h:149).
 * `atari-tools` prueft dahinter, UFT tat es bis MF-835 nicht
 * (OPEN_ITEMS P3-65 (3)).
 */
#include "uft/formats/atari_dos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

#define SD_SECTORS   720u
#define SD_SIZE      128u
#define ST_IN_USE    (DIR_FLAG_IN_USE | DIR_FLAG_DOS2_CREATED)   /* 0x42 */

#define SEK_SICHTBAR   400u   /* Datei vor der Endmarke   */
#define SEK_VERSTECKT  401u   /* Datei HINTER der Endmarke */

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

/** Setzt in der VTOC-Bitmap das Bit fuer @p sek (gesetzt = FREI). */
static void bitmap_setze(uint8_t *vtoc, uint16_t sek, bool frei)
{
    uint8_t *bm = vtoc + VTOC_BITMAP_OFFSET;
    uint16_t byte_idx = sek / 8u;
    uint8_t  bit      = (uint8_t)(7 - (sek % 8u));   /* MSB zuerst */
    if (frei) bm[byte_idx] |= (uint8_t)(1u << bit);
    else      bm[byte_idx] &= (uint8_t)~(1u << bit);
}

/*
 * Eine SD-Diskette mit genau der Lage, um die es geht:
 *
 *   Eintrag 0   belegt, "SICHTBAR.TXT", ein Sektor  (400)
 *   Eintrag 1   0x00 — die Endmarke
 *   Eintrag 2   belegt, "VERSTECK.TXT", ein Sektor  (401)  <- unsichtbar
 *
 * Die VTOC meldet ALLES frei ausser den Systemsektoren und den beiden
 * Dateisektoren. Damit ist 401 der EINZIGE Kandidat fuer „verloren" —
 * der Rotbeweis kann nicht aus Versehen anschlagen.
 */
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

    /* ---- VTOC (Sektor 360) ---- */
    uint8_t *vtoc = &d->data[(VTOC_SECTOR - 1u) * SD_SIZE];
    vtoc[0] = 2;                                   /* DOS-Code 2      */
    vtoc[1] = (uint8_t)(707u & 0xFF);              /* total_sectors   */
    vtoc[2] = (uint8_t)(707u >> 8);

    for (uint16_t s = 0; s < SD_SECTORS; s++)      /* erst alles frei */
        bitmap_setze(vtoc, s, true);

    /* Systemsektoren und die beiden Dateisektoren belegen. */
    for (uint16_t s = 0; s <= BOOT_SECTOR_COUNT; s++) bitmap_setze(vtoc, s, false);
    bitmap_setze(vtoc, VTOC_SECTOR, false);
    for (uint16_t s = DIR_SECTOR_START; s <= DIR_SECTOR_END; s++)
        bitmap_setze(vtoc, s, false);
    bitmap_setze(vtoc, SEK_SICHTBAR,  false);
    bitmap_setze(vtoc, SEK_VERSTECKT, false);

    /* freie Sektoren: 720 minus die eben belegten — die genaue Zahl ist
     * fuer diesen Rotbeweis unerheblich, `check_vtoc` laeuft hier nicht. */
    vtoc[3] = (uint8_t)(700u & 0xFF);
    vtoc[4] = (uint8_t)(700u >> 8);

    /* ---- Verzeichnis (ab Sektor 361) ---- */
    uint8_t *dir = &d->data[(DIR_SECTOR_START - 1u) * SD_SIZE];
    eintrag(dir + 0 * 16, ST_IN_USE, 1, SEK_SICHTBAR,  "SICHTBAR", "TXT");
    /* Eintrag 1 bleibt 0x00 — die Endmarke. */
    eintrag(dir + 2 * 16, ST_IN_USE, 1, SEK_VERSTECKT, "VERSTECK", "TXT");

    /* Die Dateisektoren selbst: Verkettungsbytes auf 0 (Kettenende) —
     * calloc hat das schon erledigt. */
    return d;
}

static void frei(atari_disk_t *d)
{
    if (!d) return;
    free(d->data);
    free(d);
}

/** Liest Verzeichnis und VTOC ein, wie es jeder Aufrufer tut. */
static bool einlesen(atari_disk_t *d)
{
    return dos2_read_vtoc(d) == ATARI_OK
        && dos2_read_directory(d) == ATARI_OK;
}

/* ------------------------------------------------------------------ */

TEST(reparatur_gibt_versteckte_datei_nicht_frei)
{
    /* DER ROTBEWEIS.
     *
     * Sektor 401 gehoert „VERSTECK.TXT", einem Eintrag hinter der
     * Endmarke. Nach einem Reparaturlauf muss er BELEGT bleiben. */
    atari_disk_t *d = baue_disk();
    ASSERT(d != NULL);
    ASSERT(einlesen(d));

    ASSERT(!dos2_is_sector_free(d, SEK_VERSTECKT));   /* vorher belegt */

    check_result_t r;
    memset(&r, 0, sizeof r);
    ASSERT(check_lost_sectors(d, &r, true /* fix */) == ATARI_OK);

    if (dos2_is_sector_free(d, SEK_VERSTECKT)) {
        printf("\n      Sektor %u ist nach dem Reparaturlauf FREI\n"
               "      -> die Daten von VERSTECK.TXT sind zur "
               "Ueberschreibung freigegeben,\n"
               "         waehrend check_directory sie als "
               "\"vermutlich noch vorhanden\" meldet\n      ",
               SEK_VERSTECKT);
        _fail++;
    }
    frei(d);
}

TEST(echte_waisen_werden_weiter_gefunden)
{
    /* Gegenprobe 1 — die wichtigste.
     *
     * Der Durchgang darf nicht einfach aufhoeren zu arbeiten. Ein
     * Sektor, der in der VTOC belegt ist und zu KEINER Datei gehoert,
     * ist eine echte Waise und muss weiterhin gemeldet und mit `fix`
     * freigegeben werden. Sonst waere die Reparatur „behoben", indem
     * sie nichts mehr tut. */
    atari_disk_t *d = baue_disk();
    ASSERT(d != NULL);

    uint8_t *vtoc = &d->data[(VTOC_SECTOR - 1u) * SD_SIZE];
    bitmap_setze(vtoc, 500u, false);          /* belegt, aber niemandes */
    ASSERT(einlesen(d));

    check_result_t r;
    memset(&r, 0, sizeof r);
    ASSERT(check_lost_sectors(d, &r, true) == ATARI_OK);

    if (!dos2_is_sector_free(d, 500u)) {
        printf("\n      echte Waise 500 wurde NICHT freigegeben\n      ");
        _fail++;
    }
    frei(d);
}

TEST(sichtbare_datei_bleibt_unangetastet)
{
    /* Gegenprobe 2: der gewoehnliche Fall darf sich nicht aendern. */
    atari_disk_t *d = baue_disk();
    ASSERT(d != NULL);
    ASSERT(einlesen(d));

    check_result_t r;
    memset(&r, 0, sizeof r);
    ASSERT(check_lost_sectors(d, &r, true) == ATARI_OK);

    ASSERT(!dos2_is_sector_free(d, SEK_SICHTBAR));
    frei(d);
}

TEST(ohne_fix_wird_nichts_geschrieben)
{
    /* Gegenprobe 3: `fix = false` ist ein reiner Lesevorgang. Das ist
     * die Zusage, auf der jede forensische Erstsichtung beruht. */
    atari_disk_t *d = baue_disk();
    ASSERT(d != NULL);
    ASSERT(einlesen(d));

    uint8_t *vtoc = &d->data[(VTOC_SECTOR - 1u) * SD_SIZE];
    uint8_t vorher[SD_SIZE];
    memcpy(vorher, vtoc, SD_SIZE);

    check_result_t r;
    memset(&r, 0, sizeof r);
    ASSERT(check_lost_sectors(d, &r, false) == ATARI_OK);

    ASSERT(memcmp(vorher, vtoc, SD_SIZE) == 0);
    frei(d);
}

TEST(versteckte_kette_zaehlt_auch_bei_den_querverweisen)
{
    /* Gegenprobe 4 — die zweite betroffene Stelle (atari_check.c:501).
     *
     * Belegen zwei Dateien denselben Sektor und liegt eine davon hinter
     * der Endmarke, ist das eine Querverkettung. Wer bei der Endmarke
     * abbricht, sieht nur einen Anspruch und meldet nichts.
     *
     * Hier wird bewusst NUR geprueft, dass der Durchgang durchlaeuft und
     * den Befund traegt — nicht, wie er ihn formuliert. */
    atari_disk_t *d = baue_disk();
    ASSERT(d != NULL);

    /* VERSTECK.TXT auf denselben Sektor zeigen lassen wie SICHTBAR.TXT */
    uint8_t *dir = &d->data[(DIR_SECTOR_START - 1u) * SD_SIZE];
    dir[2 * 16 + 3] = (uint8_t)(SEK_SICHTBAR & 0xFF);
    dir[2 * 16 + 4] = (uint8_t)(SEK_SICHTBAR >> 8);
    ASSERT(einlesen(d));

    check_result_t r;
    memset(&r, 0, sizeof r);
    ASSERT(check_cross_links(d, &r) == ATARI_OK);

    bool gefunden = false;
    for (uint32_t i = 0; i < r.issue_count; i++) {
        if (r.issues[i].sector == SEK_SICHTBAR) { gefunden = true; break; }
    }
    if (!gefunden) {
        printf("\n      Querverkettung auf Sektor %u nicht gemeldet\n"
               "      -> der Durchgang bricht an der Endmarke ab\n      ",
               SEK_SICHTBAR);
        _fail++;
    }
    frei(d);
}

TEST(versteckte_kette_wird_auch_geprueft)
{
    /* Gegenprobe 5 — die DRITTE betroffene Stelle
     * (`check_sector_chains`, atari_check.c:338).
     *
     * Sie kam erst durch die Mutationsprobe dazu: von drei
     * zurueckgesetzten `break` fingen die vier Faelle darueber nur ZWEI.
     * Ein Tor, das seine eigene Luecke nicht kennt, ist die Klasse, die
     * dieser Baum am haeufigsten gefangen hat — also hier geschlossen,
     * statt sie stehen zu lassen.
     *
     * Aufbau: die versteckte Datei behauptet 5 Sektoren, ihre Kette hat
     * einen. Das ist ein Widerspruch im Verzeichnis, und er muss auch
     * dann auffallen, wenn der Eintrag hinter der Endmarke liegt. */
    atari_disk_t *d = baue_disk();
    ASSERT(d != NULL);

    uint8_t *dir = &d->data[(DIR_SECTOR_START - 1u) * SD_SIZE];
    dir[2 * 16 + 1] = 5;                  /* sector_count = 5 */
    dir[2 * 16 + 2] = 0;
    ASSERT(einlesen(d));

    check_result_t r;
    memset(&r, 0, sizeof r);
    ASSERT(check_sector_chains(d, &r, false) == ATARI_OK);

    bool gefunden = false;
    for (uint32_t i = 0; i < r.issue_count; i++) {
        if (r.issues[i].file_index == 2) { gefunden = true; break; }
    }
    if (!gefunden) {
        printf("\n      Widerspruch in der versteckten Kette nicht gemeldet\n"
               "      -> der Durchgang bricht an der Endmarke ab\n      ");
        _fail++;
    }
    frei(d);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Atari: Reparatur gegen versteckte Eintraege (MF-850) ===\n");
    RUN(reparatur_gibt_versteckte_datei_nicht_frei);
    RUN(echte_waisen_werden_weiter_gefunden);
    RUN(sichtbare_datei_bleibt_unangetastet);
    RUN(ohne_fix_wird_nichts_geschrieben);
    RUN(versteckte_kette_zaehlt_auch_bei_den_querverweisen);
    RUN(versteckte_kette_wird_auch_geprueft);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

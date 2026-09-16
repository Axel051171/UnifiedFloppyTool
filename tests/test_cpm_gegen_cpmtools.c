/**
 * @file test_cpm_gegen_cpmtools.c
 * @brief CP/M auf T1b — das erste CP/M-Abbild von FREMDER Hand, und zwei
 *        Haende haben daran gearbeitet (MF-1149)
 *
 * ── Warum es das vorher nicht gab ────────────────────────────────────
 *
 * MF-1039 hat `cpm` auf T2 gehoben und T1b ausdruecklich verneint: libdsk
 * koenne ein kopfloses SIDES_ALT-Abbild nur byteidentisch zurueckgeben,
 * „eine Gleichheit ohne Aussage". Das war richtig fuer `dsktrans`. Es ist
 * falsch fuer **cpmtools**, denn `mkfs.cpm` legt kein Abbild um, es legt
 * ein DATEISYSTEM an, und `cpmcp` legt Dateien hinein — wo die Bytes
 * landen, entscheidet dabei die Definition (P3-383).
 *
 * ── Die Kette, und warum sie ZWEI Werkzeuge braucht ──────────────────
 *
 * Gemessen: `mkfs.cpm` allein erzeugt eine **kurze** Datei — bei
 * `ibm-3740` **9984** Byte statt 256 256, denn cpmtools schreibt nur
 * Systemspuren und Verzeichnis. Das ist die CP/M-Konvention (fehlende
 * Sektoren gelten als 0xE5, MF-1029), aber `uft_cpm_detect_diskdef()`
 * verlangt die EXAKTE Gesamtgroesse. Ohne einen vollen Behaelter waere
 * jedes cpmtools-Erzeugnis unlesbar.
 *
 * Den Behaelter legt deshalb **libdsk** an, und cpmtools nennt den
 * Bruecken-Namen selbst: seine `diskdefs`-Zeile fuer `cf2dd` traegt
 * `libdsk:format pcw720`.
 *
 *     dskform -type raw -format pcw720 pcw720.img      (libdsk 1.5.12)
 *       -> 737 280 Byte, 737 270 davon 0xE5
 *     mkfs.cpm -f cf2dd -L UFTKORPUS -t <abbild>       (cpmtools 2.21)
 *     cpmcp    -f cf2dd <abbild> UFTK00.DAT ... 0:
 *
 * `fsck.cpm -f cf2dd -n` meldet danach fehlerfrei: „105/256 files
 * (0.0% non-contigous), 244/357 blocks". Die Kette ist **deterministisch**
 * — zweimal aus neu erzeugten Nutzdateien gebaut, gleiche SHA-256.
 *
 * Lizenzen: cpmtools 2.21 (Michael Haardt, **GPL-3**, gemessen an
 * `COPYING` und `configure.in` im Klon) und libdsk 1.5.12 (John Elliott,
 * **LGPL-2+**). Beide werden **ausgefuehrt**, nicht uebernommen — Kanal
 * *Oracle* nach MF-695. Keine Zeile fremden Quellcodes.
 *
 * ── Die Zahlen stimmen bei DREI Haenden ueberein ─────────────────────
 *
 *                      Zyl Kopf Sek Groesse ersterSek Sysspuren Blockgr.
 *     UFT pcw-720       80    2   9   512       1         1       2048
 *     libdsk pcw720     80    2   9   512       1         -          -
 *     cpmtools cf2dd  (160 Spuren)  9   512     -         1       2048
 *
 * cpmtools zaehlt keine Koepfe: `tracks 160` sind alle Spuren beider
 * Seiten. `maxdir 256` trifft UFTs `drm = 255`, `boottrk 1` trifft
 * `dpb.off = 1`, `blocksize 2048` trifft `bsh = 4 / blm = 15`.
 *
 * ── Unterscheidungskraft: gemessen, nicht behauptet ──────────────────
 *
 * Eine Pruefdatei taugt nur, wenn eine FALSCHE Definition ein ANDERES
 * Abbild ergibt (`docs/erzeuger_kanaele.json`, Pruefung 3). Dieselben 20
 * Nutzdateien, dieselbe Geometrie, andere Definition:
 *
 *     cf2dd     (boottrk 1, Block 2048, skew 1)   <- diese Datei
 *     cpm86-720 (boottrk 2, Block 2048, skew 1)   474 521 Byte abweichend
 *     altdsdd   (boottrk 2, Block 4096, skew 0)   473 722 Byte abweichend
 *
 * Von 737 280 Byte also knapp zwei Drittel. Der Inhalt haengt an der
 * Definition; das ist der Unterschied zu einem flachen Abbild mit
 * gleichfoermiger Geometrie, wo keine Anordnung sich zeigen kann
 * (`akai_s900`/`korg_dss1`, MF-1149).
 *
 * ── Der Inhalt ist nachgewiesen, nicht nur vorhanden ─────────────────
 *
 * MF-1021: ein erzeugtes Abbild ist erst dann ein Beleg, wenn sein
 * INHALT nachgewiesen ist — hxcfe hat einmal 184 320 Byte zu 100 % 0xF6
 * geschrieben und Erfolg gemeldet. Jeder 1024-Byte-Block der 20
 * Nutzdateien traegt deshalb ueber seine ganze Laenge
 *
 *     UFT-CPM UFTKnn.DAT BLK nnnn |
 *
 * 1024 Byte ist cf2dds halbe Blockgroesse und damit die kleinste
 * Einheit, in der eine Verschiebung sichtbar wird. Die Buchhaltung des
 * Abbilds geht damit restlos auf:
 *
 *     9 Sektoren Systemspur (C00 H0)          alle 0xE5
 *    16 Sektoren Verzeichnis (ab C00 H1 R1)   256 Eintraege
 *   960 Sektoren Nutzdaten (ab C01 H0 R8)     20 x 24 Blocks x 2
 *   455 Sektoren freier Rest                  alle 0xE5
 *   ----
 *  1440 = 80 x 2 x 9
 *
 * **Und die Summe steht hier ABSICHTLICH am Ende.** MF-1026: eine Summe,
 * die aufgeht, sagt nichts ueber die Verteilung darin — bei `victor9k`
 * hoben sich zwei Zonenfehler auf und die Summe blieb 1224. Geprueft
 * wird deshalb jeder der vier Bereiche einzeln; die Summe ist nur die
 * Quittung.
 *
 * ── Was dieser Test NICHT behauptet ──────────────────────────────────
 *
 * * Nichts ueber das DATEISYSTEM. UFTs `cpm` ist ein Sektorabbild-Leser;
 *   die Verzeichniseintraege werden hier als BYTES an ihrer Stelle
 *   geprueft, nicht ausgewertet. Die CP/M-FS-Stufe ist eine eigene
 *   Achse (`docs/VERIFICATION_TIERS_FS.md`).
 * * Nichts ueber die anderen 16 Definitionen. Belegt ist `pcw-720`.
 * * Nichts ueber Skew. `cpm_open()` legt die Sektoren LINEAR ab und
 *   wertet `skew` nicht aus; cf2dds `skew 1` wirkt auf die Ablage der
 *   Dateien, nicht auf UFTs Versatzrechnung.
 * * Nichts ueber kurze Abbilder. `mkfs.cpm` allein erzeugt sie, und UFT
 *   weist sie ab — der 0xE5-Zweig in `cpm_open()` bleibt damit
 *   unerreichbar (P3-337: es fehlt der Kanal, eine Definition zu
 *   BENENNEN).
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"
#include "uft/uft_types.h"
#include "uft/formats/uft_cpm_diskdef.h"

extern const uft_format_plugin_t uft_format_plugin_cpm;

static int gruen = 0;
static int rot = 0;

static void pruefe(const char *name, int bedingung, const char *hinweis)
{
    if (bedingung) {
        printf("  [OK]   %s\n", name);
        gruen++;
    } else {
        printf("  [ROT]  %s%s%s\n", name, hinweis ? "  -- " : "",
               hinweis ? hinweis : "");
        rot++;
    }
}

#define DATEI   "cpmtools_cf2dd_720k.cpm"

#define ZYL     80
#define KOEPFE   2
#define SPT      9
#define SS     512
#define SEKTOREN (ZYL * KOEPFE * SPT)            /* 1440 */

#define SYS_SEK   9                              /* boottrk 1 = 9 Sektoren */
#define DIR_SEK  16                              /* maxdir 256 x 32 = 8192 */
#define DAT_ERST (SYS_SEK + DIR_SEK)             /* 25 */
#define DATEIEN  20
#define SEK_JE_DATEI 48                          /* 24 Blocks x 1024 / 512 */
#define DAT_SEK  (DATEIEN * SEK_JE_DATEI)        /* 960 */
#define REST_SEK (SEKTOREN - DAT_ERST - DAT_SEK) /* 455 */

/* Ein Sektor gilt als leer, wenn JEDES Byte 0xE5 ist — das Fuellbyte,
 * das libdsks `dskform` schreibt und das CP/M fuer unbenutzt haelt. */
static int ist_e5(const uint8_t *d, size_t n)
{
    size_t i;
    if (!d) return 0;
    for (i = 0; i < n; i++)
        if (d[i] != 0xE5) return 0;
    return 1;
}

/* Kommt `nadel` irgendwo in `heu` vor?
 *
 * Gesucht statt an Stelle 0 verglichen, und der Grund ist gemessen: die
 * Marke ist **30** Byte lang, ein Sektor 512 — 512 ist kein Vielfaches
 * von 30, also beginnt der ZWEITE Sektor eines 1024-Byte-Blocks mitten in
 * einer Marke („T-CPM UFTK00.DAT ..."). Die erste Fassung dieser Zusage
 * verglich ab Stelle 0 und traf deshalb genau die Haelfte: **480 von
 * 960**. Die Unterscheidungskraft bleibt, weil ein Sektor immer ganz
 * innerhalb EINES Blocks liegt — was darin steht, gehoert diesem Block,
 * und 512 Byte tragen mindestens 16 vollstaendige Marken. */
static int enthaelt(const uint8_t *heu, size_t nheu, const char *nadel)
{
    size_t nn = strlen(nadel), i;
    if (!heu || nn == 0 || nheu < nn) return 0;
    for (i = 0; i + nn <= nheu; i++)
        if (memcmp(heu + i, nadel, nn) == 0) return 1;
    return 0;
}

static uint8_t *lies(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    uint8_t *b;
    long gr;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    gr = ftell(f);
    if (gr <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    b = (uint8_t *)malloc((size_t)gr);
    if (!b) { fclose(f); return NULL; }
    *n = fread(b, 1, (size_t)gr, f);
    fclose(f);
    return b;
}

/* Alle 1440 Sektoren EINMAL durch `read_track` holen und flach ablegen.
 * Flach, weil jede Zusage unten ueber die LAUFNUMMER argumentiert — und
 * die Laufnummer ist genau das, was `cpm_open()` als Versatz benutzt. */
static uint8_t *sektoren_holen(const uft_format_plugin_t *p, uft_disk_t *disk,
                               int *ids_ok, int *gelesen)
{
    uint8_t *feld = (uint8_t *)calloc(SEKTOREN, SS);
    int c, h, s;
    *ids_ok = 1;
    *gelesen = 0;
    if (!feld) return NULL;
    for (c = 0; c < ZYL; c++) {
        for (h = 0; h < KOEPFE; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (p->read_track(disk, c, h, &t) != UFT_OK) {
                uft_track_release(&t);
                continue;
            }
            if (t.sector_count != (size_t)SPT) *ids_ok = 0;
            for (s = 0; s < SPT && (size_t)s < t.sector_count; s++) {
                size_t lauf = (size_t)(c * KOEPFE + h) * SPT + (size_t)s;
                if (t.sectors[s].id.sector != (uint8_t)(s + 1)
                    || t.sectors[s].id.cylinder != (uint8_t)c
                    || t.sectors[s].id.head != (uint8_t)h)
                    *ids_ok = 0;
                if (t.sectors[s].data && t.sectors[s].data_len == (size_t)SS) {
                    memcpy(feld + lauf * SS, t.sectors[s].data, SS);
                    (*gelesen)++;
                }
            }
            uft_track_release(&t);
        }
    }
    return feld;
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_cpm;
    char pfad[600], d1[400];
    uint8_t *roh = NULL;
    size_t nroh = 0;

    printf("=== CP/M gegen cpmtools + libdsk (MF-1149) ===\n");

    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, DATEI);
    roh = lies(pfad, &nroh);
    if (!roh) {
        printf("  [SKIP] Pruefdatei fehlt im Korpus (%s)\n", DATEI);
        return 77;
    }

    /* ── 1. Die Groesse ist die, die cf2dd und pcw720 beide nennen ── */
    snprintf(d1, sizeof(d1), "%zu Byte", nroh);
    pruefe("737 280 Byte = 80 x 2 x 9 x 512 — dieselbe Zahl bei UFTs "
           "`pcw-720`, libdsks `pcw720` und cpmtools `cf2dd`",
           nroh == (size_t)SEKTOREN * SS, d1);

    /* ── 2. Die Sonde nimmt die DATEIgroesse (MF-1039) ────────────── */
    {
        int conf = -1;
        bool ja = p->probe(roh, 4096, nroh, &conf);
        snprintf(d1, sizeof(d1), "probe(4096-Puffer, Dateigroesse %zu) = %d, "
                 "Konfidenz %d", nroh, (int)ja, conf);
        /* BERICHTIGT MF-1182: hier stand `conf == 40`, von Hand
         * vergeben. Die Leiter gibt **25** — Struktur (ein geprueftes
         * Verzeichnisbyte an berechneter Stelle) plus Geometrie, ohne
         * Kennung und ohne Selbstkonsistenz, weil CP/M-Abbilder kopflos
         * sind und die Doktrin den Beleg an eine Groessenangabe IN der
         * Datei bindet.
         *
         * Und diese Datei ist genau die, an der P3-406 gemessen wurde:
         * ihre ersten 256 Byte sind 0xE5, weshalb MYZ80 mit seinen
         * damaligen 70 das Rennen gewann. Beide stehen jetzt bei 25. */
        pruefe("die Sonde sagt ja mit Konfidenz 25 (Struktur + "
               "Geometrie, MF-1182)", ja && conf == 25, d1);
    }

    /* ── 3. Gegenprobe: falsche Dateigroesse faellt ───────────────── */
    {
        int conf = -1;
        bool ja = p->probe(roh, 4096, 4096, &conf);
        snprintf(d1, sizeof(d1), "probe(..., Dateigroesse 4096) = %d (%d)",
                 (int)ja, conf);
        pruefe("Gegenprobe: derselbe Puffer mit Dateigroesse 4096 wird "
               "abgewiesen — keine der 17 Definitionen ist so gross",
               !ja, d1);
    }

    {
        uft_disk_t disk;
        uft_error_t e;
        memset(&disk, 0, sizeof(disk));
        e = p->open(&disk, pfad, true);

        /* ── 4. Oeffnen mit der richtigen Geometrie ───────────────── */
        {
            int geo = (e == UFT_OK
                       && disk.geometry.cylinders == ZYL
                       && disk.geometry.heads == KOEPFE
                       && disk.geometry.sectors == SPT
                       && disk.geometry.sector_size == SS
                       && disk.geometry.total_sectors == SEKTOREN);
            snprintf(d1, sizeof(d1), "open=%d, %u x %u x %u x %u, gesamt %u",
                     (int)e, disk.geometry.cylinders, disk.geometry.heads,
                     disk.geometry.sectors, disk.geometry.sector_size,
                     disk.geometry.total_sectors);
            pruefe("open liefert 80 x 2 x 9 x 512 und 1440 Sektoren", geo, d1);
            if (!geo) {
                if (e == UFT_OK) p->close(&disk);
                free(roh);
                printf("\n=== %d gruen, %d rot ===\n", gruen, rot);
                return rot ? 1 : 0;
            }
        }

        {
            int ids_ok = 0, gelesen = 0;
            uint8_t *feld = sektoren_holen(p, &disk, &ids_ok, &gelesen);
            if (!feld) {
                pruefe("Sektoren einsammeln", 0, "kein Speicher");
            } else {
                size_t lauf;

                /* ── 5. Alle 1440 Sektoren, IDs 1..9 je Spur ──────── */
                snprintf(d1, sizeof(d1), "%d von %d gelesen, IDs %s",
                         gelesen, SEKTOREN, ids_ok ? "richtig" : "FALSCH");
                pruefe("alle 1440 Sektoren kommen mit voller Laenge, und "
                       "jede Nummer ist 1..9 mit ihrem eigenen Zylinder und "
                       "Kopf — `first_sector = 1` wie in libdsks `pcw720`",
                       gelesen == SEKTOREN && ids_ok, d1);

                /* ── 6. Die Systemspur: 9 Sektoren, alle 0xE5 ─────── */
                {
                    int e5 = 0;
                    for (lauf = 0; lauf < (size_t)SYS_SEK; lauf++)
                        if (ist_e5(feld + lauf * SS, SS)) e5++;
                    snprintf(d1, sizeof(d1), "%d von %d Sektoren 0xE5",
                             e5, SYS_SEK);
                    pruefe("die EINE Systemspur (C00 H0, 9 Sektoren) ist "
                           "unbeschrieben — cf2dds `boottrk 1` trifft UFTs "
                           "`dpb.off = 1`", e5 == SYS_SEK, d1);
                }

                /* ── 7. Das Verzeichnis: 256 Eintraege, gezaehlt ──── */
                {
                    const uint8_t *dir = feld + (size_t)SYS_SEK * SS;
                    int u0 = 0, label = 0, zeit = 0, frei = 0, i;
                    for (i = 0; i < 256; i++) {
                        switch (dir[i * 32]) {
                        case 0x00: u0++;    break;   /* Benutzer 0        */
                        case 0x20: label++; break;   /* Diskettenetikett  */
                        case 0x21: zeit++;  break;   /* Zeitstempelsatz   */
                        case 0xE5: frei++;  break;   /* unbenutzt         */
                        default:            break;
                        }
                    }
                    snprintf(d1, sizeof(d1), "user0 %d, Etikett %d, Zeit %d, "
                             "frei %d (Summe %d von 256)",
                             u0, label, zeit, frei, u0 + label + zeit + frei);
                    pruefe("das Verzeichnis liegt ab C00 H1 R1 und traegt "
                           "40 Eintraege mit Benutzer 0, EIN Etikett (0x20), "
                           "64 Zeitstempelsaetze (0x21) und 151 freie",
                           u0 == 40 && label == 1 && zeit == 64
                           && frei == 151, d1);
                }

                /* ── 8. Die 20 Namen stehen je ZWEIMAL im Verzeichnis
                 *
                 * Zweimal, weil `dsm = 357` ueber 255 liegt und die
                 * Blockzeiger damit 16 Bit breit sind: 8 Zeiger je
                 * Eintrag x 2048 Byte = 16 384 Byte, und 24 576 Byte
                 * brauchen zwei. Die Zahl ist gemessen, nicht gesetzt. */
                {
                    const uint8_t *dir = feld + (size_t)SYS_SEK * SS;
                    int i, k, gefunden = 0;
                    char fehlt[40];
                    fehlt[0] = 0;
                    for (k = 0; k < DATEIEN; k++) {
                        char soll[12];
                        int n = 0;
                        /* CP/M fuellt den Namen auf ACHT Zeichen mit
                         * Leerzeichen auf und haengt die drei Zeichen der
                         * Endung an: "UFTK00  DAT". Die erste Fassung
                         * suchte "UFTK00DAT" und fand gemessen **0 von
                         * 20** — die Polsterung ist kein Detail, sie ist
                         * das Format. */
                        snprintf(soll, sizeof(soll), "UFTK%02d  DAT", k);
                        for (i = 0; i < 256; i++) {
                            const uint8_t *e2 = dir + i * 32;
                            char ist[12];
                            int j;
                            if (e2[0] != 0x00) continue;
                            for (j = 0; j < 11; j++)
                                ist[j] = (char)(e2[1 + j] & 0x7F);
                            ist[11] = 0;
                            if (strncmp(ist, soll, 11) == 0) n++;
                        }
                        if (n == 2) gefunden++;
                        else if (!fehlt[0])
                            snprintf(fehlt, sizeof(fehlt), "UFTK%02d: %dx",
                                     k, n);
                    }
                    snprintf(d1, sizeof(d1), "%d von %d Namen genau zweimal"
                             "%s%s", gefunden, DATEIEN, fehlt[0] ? " — " : "",
                             fehlt[0] ? fehlt : "");
                    pruefe("alle 20 Dateinamen stehen je zweimal im "
                           "Verzeichnis (zwei Extents je 24 576 Byte)",
                           gefunden == DATEIEN, d1);
                }

                /* ── 9. Die 960 Nutzsektoren an IHRER Stelle ──────── */
                {
                    int treffer = 0, daneben = 0;
                    char erster[200];
                    erster[0] = 0;
                    for (lauf = DAT_ERST;
                         lauf < (size_t)(DAT_ERST + DAT_SEK); lauf++) {
                        size_t rel = lauf - DAT_ERST;
                        int datei = (int)(rel / SEK_JE_DATEI);
                        int block = (int)((rel % SEK_JE_DATEI) / 2);
                        char soll[40];
                        snprintf(soll, sizeof(soll),
                                 "UFT-CPM UFTK%02d.DAT BLK %04d | ",
                                 datei, block);
                        if (enthaelt(feld + lauf * SS, SS, soll)) {
                            treffer++;
                        } else {
                            char ist[33];
                            daneben++;
                            memcpy(ist, feld + lauf * SS, 32);
                            ist[32] = 0;
                            if (!erster[0])
                                snprintf(erster, sizeof(erster),
                                         "Lauf %zu erwartet \"%s\", "
                                         "gelesen \"%s\"", lauf, soll, ist);
                        }
                    }
                    snprintf(d1, sizeof(d1), "%d von %d getroffen, %d "
                             "daneben%s%s", treffer, DAT_SEK, daneben,
                             erster[0] ? " — " : "", erster[0] ? erster : "");
                    pruefe("alle 960 Nutzsektoren ab C01 H0 R8 nennen ihre "
                           "Datei UND ihren Block — eine Verschiebung um "
                           "einen halben Block waere sichtbar",
                           daneben == 0 && treffer == DAT_SEK, d1);
                }

                /* ── 10. Der freie Rest: 455 Sektoren, alle 0xE5 ──── */
                {
                    int e5 = 0;
                    for (lauf = (size_t)(DAT_ERST + DAT_SEK);
                         lauf < (size_t)SEKTOREN; lauf++)
                        if (ist_e5(feld + lauf * SS, SS)) e5++;
                    snprintf(d1, sizeof(d1), "%d von %d Sektoren 0xE5",
                             e5, REST_SEK);
                    pruefe("der Rest hinter der letzten Datei (455 Sektoren) "
                           "ist unberuehrtes 0xE5 — 244 von 357 Blocks "
                           "belegt, wie `fsck.cpm` sagt",
                           e5 == REST_SEK, d1);
                }

                /* ── 11. Die Quittung, ABSICHTLICH zuletzt (MF-1026) ─ */
                snprintf(d1, sizeof(d1), "%d + %d + %d + %d = %d",
                         SYS_SEK, DIR_SEK, DAT_SEK, REST_SEK,
                         SYS_SEK + DIR_SEK + DAT_SEK + REST_SEK);
                pruefe("die Buchhaltung geht restlos auf: 9 + 16 + 960 + "
                       "455 = 1440. Sie steht zuletzt, weil eine Summe "
                       "nichts ueber die Verteilung darin sagt (MF-1026)",
                       SYS_SEK + DIR_SEK + DAT_SEK + REST_SEK == SEKTOREN,
                       d1);
                free(feld);
            }
        }

        /* ── 12. Die Tafel selbst gegen cpmtools `cf2dd` ──────────── */
        {
            const cpm_diskdef_t *d[256];
            size_t n = uft_cpm_list_diskdefs(d, 256), i;
            const cpm_diskdef_t *pcw = NULL;
            for (i = 0; i < n; i++)
                if (strcmp(d[i]->name, "pcw-720") == 0) pcw = d[i];
            {
                int ok = (pcw != NULL
                          && pcw->cylinders == ZYL && pcw->heads == KOEPFE
                          && pcw->sectors == SPT && pcw->sector_size == SS
                          && pcw->first_sector == 1
                          && pcw->system_tracks == 1
                          && pcw->dpb.off == 1
                          && pcw->dpb.drm == 255
                          && pcw->dpb.bsh == 4 && pcw->dpb.blm == 15);
                snprintf(d1, sizeof(d1), "sec1 %u, Sysspuren %u, off %u, "
                         "drm %u, bsh %u, blm %u",
                         pcw ? pcw->first_sector : 0,
                         pcw ? pcw->system_tracks : 0,
                         pcw ? pcw->dpb.off : 0, pcw ? pcw->dpb.drm : 0,
                         pcw ? pcw->dpb.bsh : 0, pcw ? pcw->dpb.blm : 0);
                pruefe("UFTs `pcw-720` trifft cpmtools `cf2dd` in JEDEM "
                       "Feld: boottrk 1 -> off 1, maxdir 256 -> drm 255, "
                       "blocksize 2048 -> bsh 4 / blm 15", ok, d1);
            }
        }

        p->close(&disk);
    }

    free(roh);
    printf("\n=== %d gruen, %d rot ===\n", gruen, rot);
    return rot ? 1 : 0;
}

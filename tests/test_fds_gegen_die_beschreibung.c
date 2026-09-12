/**
 * @file test_fds_gegen_die_beschreibung.c
 * @brief FDS (Famicom Disk System): 36 erfundene Byte je Seite, und ein
 *        `open`, das jedes Vielfache von 65 500 annahm (MF-1038).
 *
 * ── Die Quelle ───────────────────────────────────────────────────────
 *
 * Zwei Seiten des nesdev-Wiki, Kanal *Spec* nach MF-695 (gelesen, keine
 * Zeile Code):
 *
 *   `FDS_file_format` — der fwNES-Kopf ist **16 Byte**, Bytes 0-3
 *     woertlich „Constant $46 $44 $53 $1A", Byte 4 „Number of disk
 *     sides", Bytes 5-15 „Zero filled padding"; eine Seite ist **genau
 *     65 500 Byte**; „Some .FDS images may omit the header."
 *   `FDS_disk_format` — je Seite die Bloecke 1, 2, 3, 4, 3, 4, ...;
 *     Block 1 traegt bei Versatz **$01** die Zeichenfolge
 *     „*NINTENDO-HVC*".
 *
 * Zweite Hand, nur gelesen: MAMEs `formats/nes_dsk.cpp` (BSD-3-Clause).
 * Es erkennt **allein ueber die Dateigroesse** — 65516/131016/262016 mit
 * Kennung, 65500/131000/262000 ohne — und vergleicht dabei nur **drei**
 * Byte der Kennung. UFT verlangt vier, wie die Beschreibung.
 *
 * ── Der Rotbeweis ────────────────────────────────────────────────────
 *
 * Gemessen am unveraenderten Produktionspfad. **Die Versaetze waren
 * richtig** — jede Seite wurde an der richtigen Stelle gelesen, die
 * Selbstbenennung traf. Vier andere Dinge stimmten nicht:
 *
 * **(1) 36 erfundene Byte je Seite, als GUTER Sektor gemeldet.** UFT legt
 * eine Seite als 128 virtuelle 512-Byte-Sektoren ab — das ist UFTs eigene
 * Konvention, sie steht in keiner Quelle. 128 x 512 ist aber 65 536, und
 * eine Seite hat 65 500. Der letzte Sektor traegt
 * 65 500 - 127 x 512 = **476** Byte; gemeldet wurden 512, die letzten 36
 * mit Null gefuellt, Status `UFT_SECTOR_OK`:
 *
 *     V127 : 512 Byte gemeldet, Byte 476..511 alle null, Status 0 = OK
 *
 * Gestalt von MF-1022 (`sap`s Fuellsektor galt als guter Sektor) und die
 * Klasse, die `uft_format_add_sector()` in ihrem eigenen Kopf benennt
 * (MF-980).
 *
 * **(2) Der Kopf durfte luegen.** „8 Seiten" im Kopf, **eine** Seite
 * Daten: `probe` = 1 (70), `open` = **0**, angesagt wurden 8 Zylinder mit
 * 1024 Sektoren — und Seite 7 war dann nicht lesbar. Dieselbe Gestalt wie
 * MF-1019 bei `dim`.
 *
 * **(3) Jede Datei, deren Groesse ein Vielfaches von 65 500 ist, ging
 * auf.** 131 000 **Nullbytes**: `open` = 0, zwei Seiten, 256 Sektoren
 * Nullen als gute Daten. Die Sonde war dabei ehrlich (30, Band „nur die
 * Groesse" nach MF-729) — das Oeffnen hat ihre Zurueckhaltung aufgehoben.
 *
 * **(4) Die Polsterung wurde nicht geprueft.** Byte 9 = 'A' statt 0:
 * Konfidenz **95**, genau wie die gueltige Datei.
 *
 * ── Was dieser Test NICHT behauptet ──────────────────────────────────
 *
 * **T2, nicht T1b.** Die Pruefdateien sind hauseigen; im Baum gibt es
 * kein Werkzeug, das FDS liest oder schreibt (hxcfes Modulliste kennt
 * kein FDS, libdsk auch nicht). Die Abnahme ist die **Beschreibung** plus
 * MAMEs Groessentabelle, nicht eine fremde Zerlegung.
 *
 * **`FDS_MAX_SIDES` = 8 bleibt eine Hausregel.** Die Beschreibung nennt
 * kein Maximum; MAME kennt ueber die Groesse nur 1, 2 und 4. Die Schranke
 * ist harmlos, weil die Datei die angesagten Seiten seit MF-1038 auch
 * tragen muss — und genau das nagelt eine Gegenprobe hier fest.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_fds;

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

#define KOPF        16u
#define SEITE       65500u
#define VSPT        128
#define VSS         512u
#define LETZTER     (SEITE - 127u * VSS)      /* = 476 */

#define F_ZWEI    "fds_spec_2seiten.fds"
#define F_VIER    "fds_spec_4seiten.fds"
#define F_KOPFLOS "fds_spec_kopflos.fds"

static uint8_t *lies(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    uint8_t *b;
    long gr;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    if (gr <= 0) { fclose(f); return NULL; }
    b = (uint8_t *)malloc((size_t)gr);
    if (!b) { fclose(f); return NULL; }
    *n = fread(b, 1, (size_t)gr, f);
    fclose(f);
    return b;
}

static int schreib(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    int ok;
    if (!f) return 0;
    ok = (fwrite(b, 1, n, f) == n);
    fclose(f);
    return ok;
}

/* Liest eine Seite und prueft jeden Sektor gegen seinen eigenen Namen.
 * Gibt die Zahl der Treffer zurueck, -1 bei Lesefehler. */
static int seite_pruefen(const uft_format_plugin_t *p, uft_disk_t *d,
                         int seite, char *fehler, size_t nf)
{
    uft_track_t t;
    int treffer = 0, s;
    memset(&t, 0, sizeof(t));
    if (p->read_track(d, seite, 0, &t) != UFT_OK
        || (int)t.sector_count != VSPT) {
        snprintf(fehler, nf, "Seite %d: %zu Sektoren statt %d", seite,
                 (size_t)t.sector_count, VSPT);
        uft_track_release(&t);
        return -1;
    }
    /* Sektor 0 traegt Block 1: Typ 0x01, dann die Kennung bei +1. */
    if (t.sectors[0].data
        && t.sectors[0].data[0] == 0x01
        && memcmp(t.sectors[0].data + 1, "*NINTENDO-HVC*", 14) == 0)
        treffer++;
    else
        snprintf(fehler, nf, "Seite %d Sektor 0 traegt keinen Block 1",
                 seite);
    for (s = 1; s < VSPT; s++) {
        char soll[24], ist[24];
        snprintf(soll, sizeof(soll), "UFT-K S%d V%03d ", seite, s);
        if (!t.sectors[s].data) continue;
        memcpy(ist, t.sectors[s].data, 14);
        ist[14] = 0;
        if (strcmp(ist, soll) == 0) treffer++;
        else if (fehler[0] == 0)
            snprintf(fehler, nf, "Seite %d Sektor %d: \"%s\" statt \"%s\"",
                     seite, s, ist, soll);
    }
    uft_track_release(&t);
    return treffer;
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_fds;
    char pfad[600], hilf[600], d1[300], fehler[200];
    const char *tmp = getenv("TEMP");
    uint8_t *zwei = NULL, *kopie = NULL;
    size_t nzwei = 0;

    if (!tmp) tmp = getenv("TMPDIR");
    if (!tmp) tmp = ".";

    printf("=== FDS gegen die Beschreibung (MF-1038) ===\n");

    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F_ZWEI);
    zwei = lies(pfad, &nzwei);
    if (!zwei) {
        printf("  [SKIP] Pruefdatei fehlt im Korpus (%s)\n", F_ZWEI);
        return 77;
    }
    kopie = (uint8_t *)malloc(nzwei);
    if (!kopie) { free(zwei); return 1; }

    /* ── 1. Die Rechnung selbst ──────────────────────────────────── */
    snprintf(d1, sizeof(d1), "127 x %u + %u = %u, eine Seite ist %u",
             VSS, LETZTER, 127u * VSS + LETZTER, SEITE);
    pruefe("die 128 virtuellen Sektoren decken eine Seite RESTLOS — "
           "128 x 512 waere 65 536, also traegt der letzte 476 Byte",
           127u * VSS + LETZTER == SEITE, d1);

    /* ── 2..4 Die 2-Seiten-Datei ─────────────────────────────────── */
    {
        uft_disk_t disk;
        int conf = -1;
        bool ja = p->probe(zwei, 256, nzwei, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d bei %zu Byte",
                 (int)ja, conf, nzwei);
        pruefe("der fwNES-Kopf ist ein getroffenes Merkmal: Kennung, "
               "Polsterung und gedeckte Seitenzahl ergeben 95 (MF-729)",
               ja && conf == 95, d1);

        memset(&disk, 0, sizeof(disk));
        if (p->open(&disk, pfad, true) == UFT_OK) {
            snprintf(d1, sizeof(d1), "%u x %u x %u x %u",
                     disk.geometry.cylinders, disk.geometry.heads,
                     disk.geometry.sectors, disk.geometry.sector_size);
            pruefe("zwei Seiten werden als 2 x 1 x 128 x 512 gemeldet",
                   disk.geometry.cylinders == 2 && disk.geometry.heads == 1
                   && disk.geometry.sectors == VSPT
                   && disk.geometry.sector_size == VSS, d1);

            /* Beide Seiten, jeder Sektor gegen seinen Namen. */
            {
                int ges = 0, i, schlecht = 0;
                fehler[0] = 0;
                for (i = 0; i < 2; i++) {
                    int tr = seite_pruefen(p, &disk, i, fehler,
                                           sizeof(fehler));
                    if (tr < 0) { schlecht = 1; break; }
                    ges += tr;
                }
                snprintf(d1, sizeof(d1), "%d von %d getroffen%s%s", ges,
                         2 * VSPT, fehler[0] ? " — " : "", fehler);
                pruefe("beide Seiten: Block 1 an Sektor 0, und alle "
                       "uebrigen Sektoren nennen sich selbst",
                       !schlecht && ges == 2 * VSPT, d1);
            }

            /* Der letzte Sektor traegt 476 Byte — DER Befund. */
            {
                uft_track_t t;
                int ok = 0;
                memset(&t, 0, sizeof(t));
                if (p->read_track(&disk, 0, 0, &t) == UFT_OK
                    && t.sector_count == VSPT) {
                    ok = (t.sectors[VSPT - 1].data_len == LETZTER
                          && t.sectors[VSPT - 2].data_len == VSS);
                    snprintf(d1, sizeof(d1), "V126 = %zu Byte, V127 = %zu "
                             "Byte (Soll %u und %u)",
                             t.sectors[VSPT - 2].data_len,
                             t.sectors[VSPT - 1].data_len, VSS, LETZTER);
                }
                uft_track_release(&t);
                pruefe("der LETZTE virtuelle Sektor traegt 476 Byte, nicht "
                       "512 — vorher waren 36 Byte erfunden und als "
                       "UFT_SECTOR_OK gemeldet", ok, d1);
            }
            p->close(&disk);
        } else {
            pruefe("zwei Seiten werden als 2 x 1 x 128 x 512 gemeldet", 0,
                   "open scheiterte");
        }
    }

    /* ── 5. Die 4-Seiten-Datei (MAMEs dritte Groesse) ─────────────── */
    {
        uft_disk_t disk;
        int ok = 0;
        snprintf(hilf, sizeof(hilf), "%s/%s", UFT_CORPUS_DIR, F_VIER);
        memset(&disk, 0, sizeof(disk));
        if (p->open(&disk, hilf, true) == UFT_OK) {
            fehler[0] = 0;
            ok = (disk.geometry.cylinders == 4
                  && seite_pruefen(p, &disk, 3, fehler,
                                   sizeof(fehler)) == VSPT);
            snprintf(d1, sizeof(d1), "%u Zylinder%s%s",
                     disk.geometry.cylinders, fehler[0] ? " — " : "",
                     fehler);
            p->close(&disk);
        } else snprintf(d1, sizeof(d1), "open scheiterte");
        pruefe("262 016 Byte = 4 Seiten (eine der drei Groessen aus MAMEs "
               "nes_dsk.cpp), und Seite 3 liegt richtig", ok, d1);
    }

    /* ── 6. Kopflos ──────────────────────────────────────────────── */
    {
        uft_disk_t disk;
        int ok = 0, conf = -1;
        uint8_t *b;
        size_t n = 0;
        snprintf(hilf, sizeof(hilf), "%s/%s", UFT_CORPUS_DIR, F_KOPFLOS);
        b = lies(hilf, &n);
        if (b) {
            bool ja = p->probe(b, 256, n, &conf);
            memset(&disk, 0, sizeof(disk));
            if (ja && conf == 90 && p->open(&disk, hilf, true) == UFT_OK) {
                fehler[0] = 0;
                ok = (disk.geometry.cylinders == 1
                      && seite_pruefen(p, &disk, 0, fehler,
                                       sizeof(fehler)) == VSPT);
                p->close(&disk);
            }
            snprintf(d1, sizeof(d1), "probe=%d (%d), %zu Byte", (int)ja,
                     conf, n);
            free(b);
        } else snprintf(d1, sizeof(d1), "Pruefdatei fehlt");
        pruefe("eine Datei OHNE Kopf wird an Block 1 erkannt (Konfidenz "
               "90) — die Beschreibung erlaubt das ausdruecklich", ok, d1);
    }

    /* ── 7. Gegenprobe: Seitenzahl 0 ─────────────────────────────── */
    {
        uft_disk_t disk;
        uft_error_t e;
        int conf = -1;
        bool ja;
        memcpy(kopie, zwei, nzwei);
        kopie[4] = 0;
        ja = p->probe(kopie, 256, nzwei, &conf);
        snprintf(hilf, sizeof(hilf), "%s/uft_mf1038_seiten0.fds", tmp);
        schreib(hilf, kopie, nzwei);
        memset(&disk, 0, sizeof(disk));
        e = p->open(&disk, hilf, true);
        snprintf(d1, sizeof(d1), "probe=%d (%d), open=%d", (int)ja, conf,
                 (int)e);
        pruefe("Gegenprobe: Seitenzahl 0 wird von BEIDEN abgewiesen — "
               "vorher sagte die Sonde ja (70) und `open` antwortete -25",
               !ja && e != UFT_OK, d1);
        if (e == UFT_OK) p->close(&disk);
        remove(hilf);
    }

    /* ── 8. Gegenprobe: die Polsterung ist nicht null ────────────── */
    {
        uft_disk_t disk;
        uft_error_t e;
        int conf = -1;
        bool ja;
        memcpy(kopie, zwei, nzwei);
        kopie[9] = 'A';
        ja = p->probe(kopie, 256, nzwei, &conf);
        snprintf(hilf, sizeof(hilf), "%s/uft_mf1038_polster.fds", tmp);
        schreib(hilf, kopie, nzwei);
        memset(&disk, 0, sizeof(disk));
        e = p->open(&disk, hilf, true);
        snprintf(d1, sizeof(d1), "probe=%d (%d), open=%d", (int)ja, conf,
                 (int)e);
        pruefe("Gegenprobe: 'A' in Byte 9 laesst die Datei fallen — die "
               "Beschreibung sagt \"Bytes 5-15: Zero filled padding\", und "
               "vorher gab es dafuer trotzdem 95",
               !ja && e != UFT_OK, d1);
        if (e == UFT_OK) p->close(&disk);
        remove(hilf);
    }

    /* ── 9. Gegenprobe: der Kopf verspricht mehr als die Datei hat ─ */
    {
        uft_disk_t disk;
        uft_error_t e;
        int conf = -1;
        bool ja;
        size_t n = KOPF + SEITE;
        memcpy(kopie, zwei, nzwei);
        kopie[4] = 8;                      /* acht Seiten angesagt */
        ja = p->probe(kopie, 256, n, &conf);
        snprintf(hilf, sizeof(hilf), "%s/uft_mf1038_zuwenig.fds", tmp);
        schreib(hilf, kopie, n);           /* nur EINE Seite geschrieben */
        memset(&disk, 0, sizeof(disk));
        e = p->open(&disk, hilf, true);
        snprintf(d1, sizeof(d1), "probe=%d (%d), open=%d, %u Zylinder",
                 (int)ja, conf, (int)e, disk.geometry.cylinders);
        pruefe("Gegenprobe: \"8 Seiten\" bei einer Seite Daten wird "
               "abgewiesen — vorher wurden 8 Zylinder angesagt und Seite 7 "
               "war nicht lesbar", !ja && e != UFT_OK, d1);
        if (e == UFT_OK) p->close(&disk);
        remove(hilf);
    }

    /* ── 10. Gegenprobe: richtige Groesse, keine Kennung ──────────── */
    {
        uft_disk_t disk;
        uft_error_t e;
        int conf = -1;
        bool ja;
        size_t n = 2u * SEITE;
        uint8_t *leer = (uint8_t *)calloc(1, n);
        if (leer) {
            ja = p->probe(leer, 256, n, &conf);
            snprintf(hilf, sizeof(hilf), "%s/uft_mf1038_leer.fds", tmp);
            schreib(hilf, leer, n);
            memset(&disk, 0, sizeof(disk));
            e = p->open(&disk, hilf, true);
            snprintf(d1, sizeof(d1), "probe=%d (%d), open=%d, %u Zylinder",
                     (int)ja, conf, (int)e, disk.geometry.cylinders);
            pruefe("Gegenprobe: 131 000 NULLBYTES werden abgewiesen — "
                   "vorher ging jedes Vielfache von 65 500 auf und "
                   "lieferte 256 Sektoren Nullen als gute Daten",
                   !ja && e != UFT_OK, d1);
            if (e == UFT_OK) p->close(&disk);
            remove(hilf);
            free(leer);
        }
    }

    /* ── 11. Gegenprobe: die Sonde nimmt die DATEIgroesse ─────────── */
    {
        int conf = -1;
        bool mit = p->probe(zwei, 256, nzwei, &conf);
        int c2 = -1;
        bool ohne = p->probe(zwei, 256, KOPF + SEITE, &c2);
        snprintf(d1, sizeof(d1), "mit %zu Byte: %d (%d); mit %u Byte: "
                 "%d (%d)", nzwei, (int)mit, conf, KOPF + SEITE,
                 (int)ohne, c2);
        pruefe("Gegenprobe: derselbe Puffer faellt, wenn die DATEIgroesse "
               "die angesagten Seiten nicht deckt (MF-1029)",
               mit && !ohne, d1);
    }

    /* ── 12. Durchschreibprobe, und sie prueft die 476-Grenze ─────── */
    {
        uft_disk_t disk;
        uft_track_t t;
        int ok = 0;
        snprintf(hilf, sizeof(hilf), "%s/uft_mf1038_schreib.fds", tmp);
        snprintf(d1, sizeof(d1), "Kopie liess sich nicht anlegen");
        if (schreib(hilf, zwei, nzwei)) {
            memset(&disk, 0, sizeof(disk));
            snprintf(d1, sizeof(d1), "open (schreibbar) scheiterte");
            if (p->open(&disk, hilf, false) == UFT_OK) {
                memset(&t, 0, sizeof(t));
                if (p->read_track(&disk, 0, 0, &t) == UFT_OK
                    && t.sector_count == VSPT) {
                    /* Den LETZTEN Sektor der Seite 0 ueberschreiben. Er
                     * traegt 476 Byte; die 36 Byte dahinter gehoeren
                     * bereits zur Seite 1 und muessen unberuehrt
                     * bleiben. */
                    memset(t.sectors[VSPT - 1].data, 0x5A, LETZTER);
                    memcpy(t.sectors[VSPT - 1].data, "UFT-SCHREIB", 11);
                    if (p->write_track(&disk, 0, 0, &t) == UFT_OK) {
                        uint8_t *nach;
                        size_t nn = 0;
                        uft_track_release(&t);
                        p->close(&disk);
                        nach = lies(hilf, &nn);
                        if (nach && nn == nzwei) {
                            size_t g = KOPF + 127u * VSS;  /* V127-Anfang */
                            int a = (memcmp(nach + g, "UFT-SCHREIB",
                                            11) == 0);
                            int b2 = (memcmp(nach + KOPF + SEITE,
                                             zwei + KOPF + SEITE,
                                             SEITE) == 0);
                            ok = a && b2;
                            snprintf(d1, sizeof(d1), "V127 geschrieben: %d, "
                                     "Seite 1 unveraendert: %d", a, b2);
                        } else {
                            snprintf(d1, sizeof(d1), "Datei nach dem "
                                     "Schreiben %zu statt %zu Byte", nn,
                                     nzwei);
                        }
                        free(nach);
                    } else {
                        snprintf(d1, sizeof(d1), "write_track scheiterte");
                        uft_track_release(&t);
                        p->close(&disk);
                    }
                } else {
                    snprintf(d1, sizeof(d1), "Seite 0 nicht lesbar");
                    uft_track_release(&t);
                    p->close(&disk);
                }
            }
        }
        pruefe("Durchschreibprobe: der letzte Sektor der Seite 0 wird "
               "geschrieben, und die 36 Byte dahinter — die schon zur "
               "Seite 1 gehoeren — bleiben unberuehrt", ok, d1);
        remove(hilf);
    }

    /* ── 13. Gegenprobe: kopflos, aber keine ganze Zahl von Seiten ──
     *
     * Ohne diese Zusage laesst sich nicht unterscheiden, ob der kopflose
     * Zweig die Groesse wirklich prueft. Die Datei traegt eine echte
     * Seite 0 und 100 Byte zu viel. */
    {
        uft_disk_t disk;
        uft_error_t e2;
        int conf = -1;
        bool ja;
        size_t n = SEITE + 100u;
        uint8_t *schief = (uint8_t *)calloc(1, n);
        if (schief) {
            memcpy(schief, zwei + KOPF, SEITE);
            ja = p->probe(schief, 256, n, &conf);
            snprintf(hilf, sizeof(hilf), "%s/uft_mf1038_schief.fds", tmp);
            schreib(hilf, schief, n);
            memset(&disk, 0, sizeof(disk));
            e2 = p->open(&disk, hilf, true);
            snprintf(d1, sizeof(d1), "probe=%d (%d), open=%d bei %zu Byte",
                     (int)ja, conf, (int)e2, n);
            pruefe("Gegenprobe: eine kopflose Datei mit 100 Byte zu viel "
                   "wird abgewiesen — die Groesse muss eine GANZE Zahl von "
                   "Seiten sein", !ja && e2 != UFT_OK, d1);
            if (e2 == UFT_OK) p->close(&disk);
            remove(hilf);
            free(schief);
        }
    }

    free(kopie);
    free(zwei);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

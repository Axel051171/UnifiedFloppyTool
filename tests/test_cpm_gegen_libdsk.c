/**
 * @file test_cpm_gegen_libdsk.c
 * @brief CP/M: eine Sonde, die NIE zustimmen konnte — und eine Groesse,
 *        die drei Sektornummerierungen zugleich trifft (MF-1039).
 *
 * ── Befund 1: die Sonde war unerreichbar ─────────────────────────────
 *
 * `uft_cpm_detect_diskdef()` vergleicht `size` mit der **Gesamtgroesse
 * des Abbilds** (Zylinder x Koepfe x Sektoren x Sektorgroesse).
 * `cpm_probe_plugin()` verwarf aber `file_size` (`(void)file_size`) und
 * gab die **Puffergroesse** weiter — 4096 Byte.
 *
 * Gemessen: von den 17 Definitionen hat **keine** die Gesamtgroesse
 * 4096, die Bedingung traf also nie zu. An einer gueltigen
 * 256 256-Byte-Datei (`ibm-8ss`):
 *
 *     probe(4096-Puffer, Dateigroesse 256256) : 0
 *     probe(VOLLER Puffer)                    : 1, Konfidenz 60
 *     open                                    : 0, liest richtig
 *
 * Das Plugin war **ueber die Erkennung unerreichbar**; nur eine
 * ausdrueckliche Formatwahl kam hin. Das ist die MF-1029-Falle in ihrer
 * reinsten Gestalt — dort war ein Groessenrueckfall toter Code, hier der
 * ganze Erkenner — und die Form von MF-635.
 *
 * ── Befund 2: die Reihenfolge der Tabelle entschied ──────────────────
 *
 * Gemessen ueber alle Paare:
 *
 *     184 320 Byte : amstrad-pcw (erster Sektor   1, 1 Systemspur)
 *                    amstrad-cpc (erster Sektor 193, 2 Systemspuren)
 *                    spectrum-p3 (erster Sektor   1, 1 Systemspur)
 *     143 360 Byte : nec-pc8001  (3 Systemspuren)
 *                    sharp-mz80  (2 Systemspuren)
 *
 * Die alte Auswahl nahm die **erste** passende Definition. Fuer eine
 * CPC-Datendiskette waere das `amstrad-pcw` — und damit haetten **alle
 * 360 Sektoren** die Nummern 1..9 statt 0xC1..0xC9. Gestalt von MF-1016
 * (`jv1`) und MF-1026 (`tan`).
 *
 * ── Die Referenz ─────────────────────────────────────────────────────
 *
 * libdsks Geometrietafel `lib/dsksgeom.c` (`stdg[]`, John Elliott,
 * **LGPL-2+**, im Baum unter `tools/uft-scout/work/libdsk`; **nur
 * gelesen**, Kanal *Spec* nach MF-695):
 *
 *     {"pcw180",  { SIDES_ALT, 40, 1, 9,    1, 512, ... }}
 *     {"cpcdata", { SIDES_ALT, 40, 1, 9, 0xC1, 512, ... }}
 *     {"pcw720",  { SIDES_ALT, 80, 2, 9,    1, 512, ... }}
 *
 * **Die Zahlen stimmen mit UFTs Tafel ueberein** — falsch war nicht die
 * Tafel, sondern die Auswahl. Dieser Test haelt beides fest.
 *
 * ── Was dieser Test NICHT behauptet ──────────────────────────────────
 *
 * **T2, nicht T1b.** Die Pruefdateien sind hauseigen. libdsk kann zwar
 * Rohabbilder dieser Geometrien schreiben, aber bei einem kopflosen
 * Format in SIDES_ALT waere das Ergebnis mit der Eingabe **identisch** —
 * eine Gleichheit ohne Aussage, anders als bei `nanowasp` (MF-1033), wo
 * Skew und Anordnung im Spiel waren. Bestaetigt ist die **Tafel**, nicht
 * eine fremd erzeugte Datei.
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

#define F_EIN  "cpm_ibm8ss_256k.cpm"
#define F_MEHR "cpm_mehrdeutig_180k.cpm"

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

static const cpm_diskdef_t *suche(const char *name)
{
    const cpm_diskdef_t *d[256];
    size_t n = uft_cpm_list_diskdefs(d, 256), i;
    for (i = 0; i < n; i++)
        if (strcmp(d[i]->name, name) == 0) return d[i];
    return NULL;
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_cpm;
    char pfad[600], hilf[600], d1[260];
    const char *tmp = getenv("TEMP");
    uint8_t *ein = NULL;
    size_t nein = 0;

    if (!tmp) tmp = getenv("TMPDIR");
    if (!tmp) tmp = ".";

    printf("=== CP/M gegen libdsks Geometrietafel (MF-1039) ===\n");

    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F_EIN);
    ein = lies(pfad, &nein);
    if (!ein) {
        printf("  [SKIP] Pruefdatei fehlt im Korpus (%s)\n", F_EIN);
        return 77;
    }

    /* ── 1. Keine Definition ist 4096 Byte gross ─────────────────── */
    {
        const cpm_diskdef_t *d[256];
        size_t n = uft_cpm_list_diskdefs(d, 256), i, vier = 0;
        for (i = 0; i < n; i++) {
            size_t ges = (size_t)d[i]->cylinders * d[i]->heads
                       * d[i]->sectors * d[i]->sector_size;
            if (ges == 4096) vier++;
        }
        snprintf(d1, sizeof(d1), "%zu Definitionen, davon %zu mit "
                 "Gesamtgroesse 4096", n, vier);
        pruefe("KEINE Definition ist 4096 Byte gross — deshalb konnte die "
               "Sonde nie zustimmen, solange sie die Puffergroesse "
               "weitergab", n > 0 && vier == 0, d1);
    }

    /* ── 2. Die Sonde feuert jetzt mit dem Sondenpuffer ──────────── */
    {
        int conf = -1;
        bool ja = p->probe(ein, 4096, nein, &conf);
        snprintf(d1, sizeof(d1), "probe(4096 Byte Puffer, Dateigroesse "
                 "%zu) = %d, Konfidenz %d", nein, (int)ja, conf);
        pruefe("die Sonde nimmt die DATEIgroesse: mit 4096 Byte Puffer "
               "sagt sie ja, Konfidenz 40 (Band \"nur die Groesse\", "
               "MF-729) — vorher sagte sie nein", ja && conf == 40, d1);
    }

    /* ── 3. Gegenprobe: mit falscher Dateigroesse faellt sie ─────── */
    {
        int conf = -1;
        bool ja = p->probe(ein, 4096, 4096, &conf);
        snprintf(d1, sizeof(d1), "probe(..., Dateigroesse 4096) = %d (%d)",
                 (int)ja, conf);
        pruefe("Gegenprobe: derselbe Puffer mit Dateigroesse 4096 wird "
               "abgewiesen — genau das war der Vorzustand fuer JEDE Datei",
               !ja, d1);
    }

    /* ── 4. Oeffnen und lesen ────────────────────────────────────── */
    {
        uft_disk_t disk;
        uft_error_t e;
        memset(&disk, 0, sizeof(disk));
        e = p->open(&disk, pfad, true);
        if (e != UFT_OK) {
            snprintf(d1, sizeof(d1), "open=%d", (int)e);
            pruefe("die Datei laesst sich oeffnen (77 x 1 x 26 x 128)", 0,
                   d1);
        } else {
            int geo = (disk.geometry.cylinders == 77
                       && disk.geometry.heads == 1
                       && disk.geometry.sectors == 26
                       && disk.geometry.sector_size == 128);
            snprintf(d1, sizeof(d1), "%u x %u x %u x %u",
                     disk.geometry.cylinders, disk.geometry.heads,
                     disk.geometry.sectors, disk.geometry.sector_size);
            pruefe("die Datei laesst sich oeffnen (77 x 1 x 26 x 128)",
                   geo, d1);

            if (geo) {
                int c, s, treffer = 0, fehl = 0, ids = 1;
                char erster[80];
                erster[0] = 0;
                for (c = 0; c < 77; c++) {
                    uft_track_t t;
                    memset(&t, 0, sizeof(t));
                    if (p->read_track(&disk, c, 0, &t) != UFT_OK
                        || t.sector_count != 26) {
                        fehl += 26;
                        uft_track_release(&t);
                        continue;
                    }
                    for (s = 0; s < 26; s++) {
                        char soll[24], ist[24];
                        snprintf(soll, sizeof(soll), "UFT-K C%02d H0 S%03d ",
                                 c, s + 1);
                        if (!t.sectors[s].data) { fehl++; continue; }
                        memcpy(ist, t.sectors[s].data, 18);
                        ist[18] = 0;
                        /* Der erste Sektor der Spur 2 traegt das 0xE5 des
                         * Verzeichnisses an Stelle seines Namens. */
                        if (c == 2 && s == 0) {
                            if (t.sectors[s].data[0] == 0xE5) treffer++;
                            else fehl++;
                            continue;
                        }
                        if (strcmp(ist, soll) == 0) treffer++;
                        else {
                            fehl++;
                            if (!erster[0])
                                snprintf(erster, sizeof(erster),
                                         "C%d S%d: \"%s\"", c, s + 1, ist);
                        }
                        if (t.sectors[s].id.sector != (uint8_t)(s + 1))
                            ids = 0;
                    }
                    uft_track_release(&t);
                }
                snprintf(d1, sizeof(d1), "%d von 2002 getroffen, %d "
                         "daneben%s%s", treffer, fehl,
                         erster[0] ? " — " : "", erster);
                pruefe("alle 2002 Sektoren nennen sich selbst",
                       fehl == 0 && treffer == 2002, d1);
                pruefe("die Sektornummern laufen 1..26 — libdsks `pcw180` "
                       "und UFTs `ibm-8ss` nennen beide den ersten Sektor 1",
                       ids, NULL);
            }
            p->close(&disk);
        }
    }

    /* ── 5. Die Mehrdeutigkeit: 184 320 Byte ─────────────────────── */
    {
        uft_disk_t disk;
        uft_error_t e;
        int conf = -1;
        bool ja = false;
        uint8_t *m;
        size_t nm = 0;
        snprintf(hilf, sizeof(hilf), "%s/%s", UFT_CORPUS_DIR, F_MEHR);
        m = lies(hilf, &nm);
        if (m) {
            ja = p->probe(m, 4096, nm, &conf);
            free(m);
        }
        memset(&disk, 0, sizeof(disk));
        e = p->open(&disk, hilf, true);
        snprintf(d1, sizeof(d1), "probe=%d (%d), open=%d bei %zu Byte",
                 (int)ja, conf, (int)e, nm);
        pruefe("184 320 Byte trifft drei Definitionen mit ZWEI "
               "Sektornummerierungen (1 und 193) — Sonde und `open` sagen "
               "beide ab, statt die erste zu nehmen",
               !ja && e != UFT_OK, d1);
        if (e == UFT_OK) p->close(&disk);
    }

    /* ── 6. Die Tafel selbst, gegen libdsks `stdg[]` ─────────────── */
    {
        const cpm_diskdef_t *pcw = suche("amstrad-pcw");
        const cpm_diskdef_t *cpc = suche("amstrad-cpc");
        const cpm_diskdef_t *p720 = suche("pcw-720");
        int ok = (pcw && pcw->cylinders == 40 && pcw->heads == 1
                  && pcw->sectors == 9 && pcw->sector_size == 512
                  && pcw->first_sector == 1)
              && (cpc && cpc->cylinders == 40 && cpc->heads == 1
                  && cpc->sectors == 9 && cpc->sector_size == 512
                  && cpc->first_sector == 0xC1)
              && (p720 && p720->cylinders == 80 && p720->heads == 2
                  && p720->sectors == 9 && p720->sector_size == 512
                  && p720->first_sector == 1);
        snprintf(d1, sizeof(d1), "pcw erster Sektor %u, cpc %u, 720er %u",
                 pcw ? pcw->first_sector : 0, cpc ? cpc->first_sector : 0,
                 p720 ? p720->first_sector : 0);
        pruefe("die Tafel stimmt mit libdsks `stdg[]` ueberein: pcw180 "
               "40/1/9/512 ab 1, cpcdata ab 0xC1, pcw720 80/2/9/512 ab 1",
               ok, d1);
    }

    /* ── 7. Das Verzeichnisbyte wird geprueft — aber nicht geraten ─
     *
     * Bei `ibm-8ss` liegt der Verzeichnisanfang bei 2 x 26 x 128 = 6656,
     * also AUSSERHALB des 4096-Byte-Sondenpuffers. Die Sonde darf ihn
     * deshalb nicht beurteilen; `open` sieht die ganze Datei und muss
     * ihn pruefen. */
    {
        uft_disk_t disk;
        uft_error_t e;
        int conf = -1;
        bool ja;
        uint8_t *k = (uint8_t *)malloc(nein);
        FILE *f;
        if (k) {
            memcpy(k, ein, nein);
            k[2u * 26u * 128u] = 0x7F;     /* weder 0xE5 noch <= 15 */
            ja = p->probe(k, 4096, nein, &conf);
            snprintf(hilf, sizeof(hilf), "%s/uft_mf1039_dir.cpm", tmp);
            f = fopen(hilf, "wb");
            if (f) { fwrite(k, 1, nein, f); fclose(f); }
            memset(&disk, 0, sizeof(disk));
            e = p->open(&disk, hilf, true);
            snprintf(d1, sizeof(d1), "probe=%d (%d), open=%d", (int)ja,
                     conf, (int)e);
            pruefe("ein Verzeichnisanfang von 0x7F laesst `open` fallen — "
                   "die Sonde sieht ihn nicht (er liegt hinter dem "
                   "4096-Byte-Puffer) und RAET auch nicht",
                   ja && e != UFT_OK, d1);
            if (e == UFT_OK) p->close(&disk);
            remove(hilf);
            free(k);
        }
    }

    free(ein);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

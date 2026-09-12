/**
 * @file test_dim_kennung_und_groesse.c
 * @brief DIM: die Kennung bei 0xAB, die Dateigroesse — und eine
 *        Medientabelle, die UNGEKLAERT bleibt (MF-1019).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * **Befund 1 — die Kennung wurde nie geprueft.** Der Dateikopf von
 * `uft_dim.c` sagte „0xAB 1 Overtrack flag", und weder `dim_probe()`
 * noch `dim_open()` sahen dort hin. Bei 0xAB steht aber die **Kennung
 * des Formats**. Gemessen am Vorzustand mit einem Puffer aus 512
 * Nullbytes und einer plausiblen Dateigroesse:
 *
 *     ohne Kennung : Sonde JA, Konfidenz 45
 *
 * Die Sonde nahm also **jede** hinreichend grosse Datei an, die mit
 * einer Null beginnt.
 *
 * **Befund 2 — `open` prueft die Dateigroesse nicht.** Die Sonde
 * verlangte `file_size >= 256 + cyl*heads*spt*ss`, das Oeffnen
 * verlangte nichts davon.
 *
 * ── Die Referenzen fuer die Kennung: drei Stellen, eine Aussage ──────
 *
 * **MAME `formats/dim_dsk.cpp`** (BSD-3-Clause, Olivier Galibert), in
 * `neue-ideen/formats.zip`:
 *
 *     read_at(io, 0xab, h, 16);
 *     if (strncmp((const char *)h, "DIFC HEADER", 11) == 0)
 *         return FIFID_SIGN;
 *
 * **`src/formats/pc98/dim.c:37`** im eigenen Baum:
 *
 *     // 0xAB..0xB7 = "DIFC HEADER  " (13 bytes)
 *     return memcmp(hdr + 0xAB, mark, 13) == 0;
 *
 * **`src/formats/misc/dcp_dcu.c:51`** nennt dieselbe Kennung.
 *
 * Die richtige Pruefung lag also in Dateien, die niemand ruft —
 * `pc98/dim.c` steht in `docs/orphan_baseline.txt`. In dieser Runde
 * der dritte Fall dieser Gestalt nach `udi` (MF-1015, Pruefsumme) und
 * `scl` (MF-1014, TR-DOS-Layout).
 *
 * ── Was hier festgenagelt war — und seit MF-1037 BEWIESEN ist ─────
 *
 * Hier stand: „Die Medientabelle bleibt ungeklaert, und `dim` bleibt
 * deshalb T3" — drei Umsetzungen, drei Tabellen, keine zwei gleich bei
 * fuenf von sieben Werten (P3-325). Die Tafel unten hielt den
 * damaligen Stand ausdruecklich **festgenagelt und nicht bewiesen**,
 * damit eine Aenderung absichtlich geschieht.
 *
 * **Genau das ist eingetreten.** MF-1037 hat den Grund fuer den
 * Widerspruch gemessen: **zwei der drei Tabellen sind Tabellen fuer
 * ZWEI VERSCHIEDENE FORMATE** — 0x09, 0x11 und 0x19 sind
 * **DCP**-Medienbytes, nicht DIM. Die Tafel unten fuehrt deshalb jetzt
 * **vier** Werte, und die drei DCP-Werte stehen als *abgewiesen*.
 *
 * Der Beleg liegt in `tests/test_dim_gegen_hxcfe.c`: vier Pruefdateien
 * im Korpus, von hxcfe selbst zerlegt (DIM → IMD), **7952 von 7952
 * Sektoren byteidentisch** an UFTs eigenen Versaetzen. `dim` steht
 * seither auf **T2**.
 *
 * Dieser Test bleibt, weil er etwas anderes prueft: die **Kennung** und
 * die **Groessenpruefung**, beides ohne Korpus.

 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_dim;

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

#define HDR 256

/* Baut eine DIM: 256-Byte-Kopf mit Medienbyte und (optional) Kennung,
 * danach `daten` Byte Nutzlast. Spur 0 Sektor 0 traegt 0x5A. */
static int baue_dim(const char *pfad, uint8_t media, int kennung,
                    size_t daten)
{
    uint8_t *b = (uint8_t *)calloc(1, HDR + daten + 1);
    if (!b) return 0;
    b[0] = media;
    if (kennung) memcpy(b + 0xAB, "DIFC HEADER  ", 13);
    if (daten) memset(b + HDR, 0x5A, daten);
    FILE *f = fopen(pfad, "wb");
    if (!f) { free(b); return 0; }
    int ok = (fwrite(b, 1, HDR + daten, f) == HDR + daten);
    fclose(f);
    free(b);
    return ok;
}

static void frei(uft_track_t *t)
{
    if (!t) return;
    free(t->sectors);
    memset(t, 0, sizeof(*t));
}

int main(void)
{
    printf("=== DIM: Kennung und Groesse (MF-1019) ===\n");

    char pfad[512];
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMPDIR");
    if (!tmp) tmp = ".";
    snprintf(pfad, sizeof(pfad), "%s/uft_mf1019.dim", tmp);

    char h[240];

    /* ── Befund 1: die Sonde ─────────────────────────────────────────── */
    {
        static uint8_t kopf[512];
        memset(kopf, 0, sizeof(kopf));
        /* 0x00 = 2HD: 77 * 2 * 8 * 1024 + 256 */
        size_t fs = HDR + 77u * 2 * 8 * 1024;

        int c = -1;
        bool p = uft_format_plugin_dim.probe(kopf, sizeof(kopf), fs, &c);
        snprintf(h, sizeof(h), "%s, Konfidenz %d", p ? "JA" : "nein", c);
        pruefe("512 Nullbytes werden abgewiesen — es fehlt die Kennung",
               !p, h);

        memcpy(kopf + 0xAB, "DIFC HEADER  ", 13);
        c = -1;
        p = uft_format_plugin_dim.probe(kopf, sizeof(kopf), fs, &c);
        snprintf(h, sizeof(h), "%s, Konfidenz %d", p ? "JA" : "nein", c);
        pruefe("mit der Kennung wird sie angenommen, als getroffenes "
               "Merkmal (MF-729: 80..100)", p && c >= 80, h);

        /* Ein Byte der Kennung verfaelscht — MAME vergleicht 11. */
        kopf[0xAB + 4] = 'X';
        c = -1;
        p = uft_format_plugin_dim.probe(kopf, sizeof(kopf), fs, &c);
        pruefe("ein verfaelschtes Byte der Kennung laesst sie fallen",
               !p, NULL);
        memcpy(kopf + 0xAB, "DIFC HEADER  ", 13);

        /* Die beiden Leerzeichen dahinter sind NICHT verlangt: MAME
         * vergleicht 11 Byte, `pc98/dim.c` 13. Genommen ist die
         * schwaechere Annahme. */
        kopf[0xAB + 11] = 0x00;
        kopf[0xAB + 12] = 0x00;
        c = -1;
        p = uft_format_plugin_dim.probe(kopf, sizeof(kopf), fs, &c);
        pruefe("die zwei Leerzeichen hinter \"DIFC HEADER\" sind nicht "
               "verlangt (MAME vergleicht 11 Byte)", p, NULL);
    }

    /* ── Befund 1+2 beim Oeffnen ─────────────────────────────────────── */
    {
        size_t daten = 77u * 2 * 8 * 1024;   /* 2HD nach der Tabelle hier */

        /* ohne Kennung */
        baue_dim(pfad, 0x00, 0, daten);
        uft_disk_t d;
        memset(&d, 0, sizeof(d));
        uft_error_t r = uft_format_plugin_dim.open(&d, pfad, true);
        snprintf(h, sizeof(h), "open=%d", (int)r);
        pruefe("open weist eine Datei ohne Kennung ab", r != UFT_OK, h);
        if (r == UFT_OK) uft_format_plugin_dim.close(&d);
        remove(pfad);

        /* mit Kennung, vollstaendig */
        baue_dim(pfad, 0x00, 1, daten);
        memset(&d, 0, sizeof(d));
        r = uft_format_plugin_dim.open(&d, pfad, true);
        snprintf(h, sizeof(h), "open=%d, %u Zyl, %u Koepfe, %u Sek, %u Byte",
                 (int)r, d.geometry.cylinders, d.geometry.heads,
                 d.geometry.sectors, d.geometry.sector_size);
        pruefe("mit Kennung und vollstaendigen Daten laesst sie sich "
               "oeffnen", r == UFT_OK, h);
        if (r == UFT_OK) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            uft_error_t r2 = uft_format_plugin_dim.read_track(&d, 0, 0, &t);
            snprintf(h, sizeof(h), "rc=%d, %zu Sektoren, Byte 0 = %02X",
                     (int)r2, (size_t)t.sector_count,
                     t.sector_count ? t.sectors[0].data[0] : 0);
            pruefe("und die Daten beginnen hinter dem 256-Byte-Kopf",
                   r2 == UFT_OK && t.sector_count > 0
                   && t.sectors[0].data[0] == 0x5A, h);
            frei(&t);
            uft_format_plugin_dim.close(&d);
        }
        remove(pfad);

        /* mit Kennung, aber ein Sektor fehlt */
        baue_dim(pfad, 0x00, 1, daten - 1024);
        memset(&d, 0, sizeof(d));
        r = uft_format_plugin_dim.open(&d, pfad, true);
        snprintf(h, sizeof(h), "open=%d", (int)r);
        pruefe("eine zu kurze Datei wird abgewiesen — vorher bekam sie "
               "die volle angesagte Geometrie", r != UFT_OK, h);
        if (r == UFT_OK) uft_format_plugin_dim.close(&d);
        remove(pfad);
    }

    /* ── Die Medientabelle: FESTGENAGELT, nicht bewiesen ─────────────── */
    {
        /* MF-1037: die aufgeloeste Tafel. Belegt an hxcfes
         * Formatbeschreibung (Kanal Spec) und an seinem AUSGEFUEHRTEN
         * `X68000_DIM`-Lader; der Sektor-fuer-Sektor-Abgleich steht in
         * `tests/test_dim_gegen_hxcfe.c`. Vorher standen hier alle vier
         * Werte auf 77x2x8x1024, dazu die drei DCP-Medienbytes. */
        struct { uint8_t media; uint8_t cyl, heads, spt; uint16_t ss; }
        tafel[] = {
            { 0x00, 77, 2,  8, 1024 },
            { 0x01, 80, 2,  9, 1024 },
            { 0x02, 80, 2, 15,  512 },
            { 0x03, 80, 2, 18,  512 },
        };
        int alle = 1;
        for (size_t i = 0; i < sizeof(tafel)/sizeof(tafel[0]); i++) {
            size_t daten = (size_t)tafel[i].cyl * tafel[i].heads
                         * tafel[i].spt * tafel[i].ss;
            baue_dim(pfad, tafel[i].media, 1, daten);
            uft_disk_t d;
            memset(&d, 0, sizeof(d));
            uft_error_t r = uft_format_plugin_dim.open(&d, pfad, true);
            int gut = (r == UFT_OK
                       && d.geometry.cylinders == tafel[i].cyl
                       && d.geometry.heads == tafel[i].heads
                       && d.geometry.sectors == tafel[i].spt
                       && d.geometry.sector_size == tafel[i].ss);
            if (!gut) {
                printf("       Medienbyte %02X: open=%d, %u/%u/%u/%u "
                       "(erwartet %u/%u/%u/%u)\n", tafel[i].media, (int)r,
                       d.geometry.cylinders, d.geometry.heads,
                       d.geometry.sectors, d.geometry.sector_size,
                       tafel[i].cyl, tafel[i].heads, tafel[i].spt,
                       tafel[i].ss);
                alle = 0;
            }
            if (r == UFT_OK) uft_format_plugin_dim.close(&d);
            remove(pfad);
        }
        pruefe("die VIER DIM-Medienwerte liefern die belegte Geometrie "
               "(MF-1037 — vorher standen alle vier auf 77x2x8x1024)",
               alle, NULL);

        /* Und die drei DCP-Werte, die hier standen, fallen jetzt. */
        {
            static const uint8_t dcp[] = { 0x09, 0x11, 0x19 };
            int weg = 1;
            size_t k;
            for (k = 0; k < sizeof(dcp); k++) {
                uft_disk_t dd;
                uft_error_t rr;
                baue_dim(pfad, dcp[k], 1, 2u * 1024 * 1024 - HDR);
                memset(&dd, 0, sizeof(dd));
                rr = uft_format_plugin_dim.open(&dd, pfad, true);
                if (rr == UFT_OK) {
                    printf("       0x%02X wird noch angenommen\n", dcp[k]);
                    uft_format_plugin_dim.close(&dd);
                    weg = 0;
                }
                remove(pfad);
            }
            pruefe("die drei DCP-Medienbytes 0x09/0x11/0x19 werden "
                   "abgewiesen — sie gehoeren einer anderen Nummerierung",
                   weg, NULL);
        }

        /* Ein unbekanntes Medienbyte wird abgewiesen. */
        baue_dim(pfad, 0x7F, 1, 4096);
        uft_disk_t d;
        memset(&d, 0, sizeof(d));
        uft_error_t r = uft_format_plugin_dim.open(&d, pfad, true);
        pruefe("ein unbekanntes Medienbyte wird abgewiesen", r != UFT_OK,
               NULL);
        if (r == UFT_OK) uft_format_plugin_dim.close(&d);
        remove(pfad);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

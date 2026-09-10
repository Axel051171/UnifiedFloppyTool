/**
 * @file test_scl_ist_eine_trdos_diskette.c
 * @brief SCL ist ein Archiv und muss zu einer TR-DOS-Diskette ausgebaut
 *        werden — gegen zwei unabhaengige Orakel (MF-1014).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * `scl` stand auf **T3** und hatte keinen einzigen Test. Gemessen am
 * Quelltext lag der Grund offen: `scl_open()` setzte
 * `data_start = 9 + n*14` und legte die **Dateidaten auf Zylinder 0,
 * Sektor 0** — dorthin, wo bei TR-DOS der Katalog steht. Dazu
 * `heads = 1` und eine Zylinderzahl aus der Datenlaenge.
 *
 * Jeder Sektor lag damit eine ganze Spur zu frueh, und einen Katalog
 * gab es nirgends. Klasse **MF-794** (`sad` rechnete zylinder-dur statt
 * kopf-dur und las 158 von 160 Spuren an der falschen Stelle) — hier
 * mit dem Zusatz, dass die Struktur, die TR-DOS ausmacht, komplett
 * fehlte.
 *
 * ── Die Orakel ───────────────────────────────────────────────────────
 *
 * **Erstes Orakel, im Baum:** `src/samdisk/scl.cpp` (SAMdisk, MIT).
 * **Zweites, unabhaengiges:**
 * `tools/uft-scout/work/HxCFloppyEmulator/libhxcfe/sources/loaders/`
 * `scl_loader/scl_loader.c` (HxC, GPL-2, **nur gelesen**).
 *
 * Beide bauen dieselbe Diskette aus voellig getrennten Codebasen, und
 * **neun Feldpositionen im Infosatz stimmen byteweise** (Tafel im Kopf
 * von `src/formats/scl/uft_scl_plugin.c`). Ein drittes Mal bestaetigt
 * sie der eigene Baum: `src/formats/trd/uft_trd.c` liest die Dateizahl
 * bei `0x8E4`.
 *
 * Damit ist das hier **T2** nach `docs/VERIFICATION_PLAN.md`:
 * synthetischer Rundlauf **plus** Spec gegen eine autoritative
 * Fremdimplementierung. Kein SCL von fremder Hand liegt im Korpus —
 * fuer T1b fehlt es, und das steht so da.
 *
 * ── Warum die Zahlen hier nicht aus unserem Code kommen ──────────────
 *
 * Jeder erwartete Wert unten ist ausgeschrieben und aus einem der
 * Orakel abgelesen (MF-913), nicht aus `uft_scl_plugin.c` geholt:
 *
 *   Daten beginnen bei LBA 16            SAMdisk `uDataLba = 16`
 *   Startsektor = LBA & 0x0F             SAMdisk `pb[14]`
 *   Startspur   = LBA >> 4               SAMdisk `pb[15]`
 *   Geometrie 80 x 2 x 16 x 256          HxC fest, SAMdisk SizeToCylsTRD
 *   Infosatz 0x8E1..0x8F4                beide, byteweise gleich
 *   Grenze 128 Dateien                   SAMdisk `> 128`, HxC `> 127`
 *
 * Dazu die **Summe in der Datei selbst**: 32 Bit ueber Kopf, Eintraege
 * und Daten. Sie ist der einzige Wert an einer SCL, der nicht aus
 * unserem Code stammen kann — derselbe Weg wie MF-869 (FM-CRCs auf der
 * Diskette) und MF-1013 (GCR-Pruefsummen auf der Diskette).
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/formats/scl/uft_scl.h"

extern const uft_format_plugin_t uft_format_plugin_scl;
int uft_scl_pruefsumme(const char *path, uint32_t *soll, uint32_t *ist);

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

/* ── Die Pruefdatei ──────────────────────────────────────────────────
 *
 * Zwei Dateien. Die Laengenangabe der ersten ist mit Absicht
 * **0x0201** — damit das obere Byte (`Eintrag[12]`) nicht null ist.
 * Genau dieses Byte hat `uft_scl.c` verschluckt.
 */
#define SEK_A   3
#define SEK_B   1
#define DATEN_A (SEK_A * 256)
#define DATEN_B (SEK_B * 256)

static const uint8_t eintrag_a[14] = {
    'T','E','S','T','A',' ',' ',' ',            /* Name        */
    0x42,                                       /* Typ 'B'     */
    0x00, 0x80,                                 /* Start 0x8000 */
    0x01, 0x02,                                 /* Laenge 0x0201 */
    SEK_A                                       /* Sektoren    */
};
static const uint8_t eintrag_b[14] = {
    'Z','W','E','I',' ',' ',' ',' ',
    0x43,                                       /* Typ 'C'     */
    0x34, 0x12,                                 /* Start 0x1234 */
    0x00, 0x01,                                 /* Laenge 0x0100 */
    SEK_B
};

static size_t baue_scl(uint8_t *b, size_t kap, int dateizahl_ueberschreiben,
                       int summe_verfaelschen, int daten_kuerzen)
{
    size_t n = 0;
    memcpy(b + n, "SINCLAIR", 8); n += 8;
    b[n++] = dateizahl_ueberschreiben ? (uint8_t)dateizahl_ueberschreiben : 2;
    memcpy(b + n, eintrag_a, 14); n += 14;
    memcpy(b + n, eintrag_b, 14); n += 14;

    size_t daten_anfang = n;
    for (size_t i = 0; i < DATEN_A; i++) b[n++] = (uint8_t)(0xA0 ^ (i & 0xFF));
    for (size_t i = 0; i < DATEN_B; i++) b[n++] = (uint8_t)(0x5B ^ (i & 0xFF));
    (void)daten_anfang;

    if (daten_kuerzen) n -= 256;        /* eine Sektorlaenge fehlt */

    if (summe_verfaelschen) b[daten_anfang] ^= 0xFF;

    /* Summe: byteweise ueber alles bis hier, 32 Bit, little-endian */
    uint32_t s = 0;
    for (size_t i = 0; i < n; i++) s += b[i];
    b[n++] = (uint8_t)(s & 0xFF);
    b[n++] = (uint8_t)((s >> 8) & 0xFF);
    b[n++] = (uint8_t)((s >> 16) & 0xFF);
    b[n++] = (uint8_t)((s >> 24) & 0xFF);

    if (summe_verfaelschen) {
        /* Die Summe stimmte, jetzt das Datenbyte NACHtraeglich kippen:
         * so steht in der Datei eine Summe, die nicht mehr aufgeht. */
        b[daten_anfang] ^= 0xFF;
    }

    if (n > kap) { printf("  [ROT]  Puffer zu klein\n"); rot++; }
    return n;
}

/* Eine SCL mit `anzahl` Eintraegen, alle ohne Daten (0 Sektoren).
 *
 * MF-1014, und das ist eine Lehre aus dem eigenen Lauf: die erste
 * Fassung dieser Gegenprobe nahm 200 Dateien und die **zwei**
 * Datensaetze von oben — die Datei war damit zu kurz, und abgewiesen
 * wurde sie von der LAENGEN-Schranke, nicht von der 128er-Grenze. Die
 * Mutation „Grenze entfernt" fiel deshalb nicht. Genau dieselbe Falle
 * wie bei der CFI-Ueberlaufprobe.
 *
 * Jetzt wird die Grenze da geprueft, wo sie liegt: 128 muss durch,
 * 129 nicht — und beide Dateien sind vollstaendig.
 */
static size_t baue_viele(uint8_t *b, size_t kap, int anzahl)
{
    size_t n = 0;
    if ((size_t)(9 + anzahl * 14 + 4) > kap) return 0;
    memcpy(b + n, "SINCLAIR", 8); n += 8;
    b[n++] = (uint8_t)anzahl;
    for (int i = 0; i < anzahl; i++) {
        memset(b + n, ' ', 8);
        b[n] = 'F';
        b[n + 1] = (uint8_t)('0' + (i / 100) % 10);
        b[n + 2] = (uint8_t)('0' + (i / 10) % 10);
        b[n + 3] = (uint8_t)('0' + i % 10);
        n += 8;
        b[n++] = 0x42;              /* Typ */
        b[n++] = 0x00; b[n++] = 0x00;   /* Start  */
        b[n++] = 0x00; b[n++] = 0x00;   /* Laenge */
        b[n++] = 0;                 /* 0 Sektoren -> keine Daten */
    }
    uint32_t s = 0;
    for (size_t i = 0; i < n; i++) s += b[i];
    b[n++] = (uint8_t)(s & 0xFF);
    b[n++] = (uint8_t)((s >> 8) & 0xFF);
    b[n++] = (uint8_t)((s >> 16) & 0xFF);
    b[n++] = (uint8_t)((s >> 24) & 0xFF);
    return n;
}

static int schreibe(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    int ok = (fwrite(b, 1, n, f) == n);
    fclose(f);
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
    printf("=== SCL ist eine TR-DOS-Diskette (MF-1014) ===\n");

    char tmpdir[400];
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMPDIR");
    if (!tmp) tmp = ".";
    snprintf(tmpdir, sizeof(tmpdir), "%s", tmp);

    char pfad[512], pfad_magie[512], pfad_viele[512], pfad_kurz[512],
         pfad_summe[512];
    snprintf(pfad,       sizeof(pfad),       "%s/uft_mf1014.scl", tmpdir);
    snprintf(pfad_magie, sizeof(pfad_magie), "%s/uft_mf1014_magie.scl", tmpdir);
    snprintf(pfad_viele, sizeof(pfad_viele), "%s/uft_mf1014_viele.scl", tmpdir);
    snprintf(pfad_kurz,  sizeof(pfad_kurz),  "%s/uft_mf1014_kurz.scl", tmpdir);
    snprintf(pfad_summe, sizeof(pfad_summe), "%s/uft_mf1014_summe.scl", tmpdir);

    static uint8_t puffer[4096];
    size_t n = baue_scl(puffer, sizeof(puffer), 0, 0, 0);
    if (!schreibe(pfad, puffer, n)) {
        printf("  [ROT]  Pruefdatei liess sich nicht schreiben\n");
        return 1;
    }

    /* Erwartete Dateigroesse, aus SAMdisks calc_size:
     *   9 + 2*14 + (SEK_A + SEK_B)*256 + 4 */
    size_t soll_n = 9 + 2 * 14 + (SEK_A + SEK_B) * 256 + 4;
    char h[240];
    snprintf(h, sizeof(h), "%zu Byte geschrieben, %zu erwartet", n, soll_n);
    pruefe("die Pruefdatei hat die Groesse, die SAMdisk errechnet",
           n == soll_n, h);

    /* ── Die Summe in der Datei ──────────────────────────────────────── */
    {
        uint32_t soll = 0, ist = 0;
        int rc = uft_scl_pruefsumme(pfad, &soll, &ist);
        snprintf(h, sizeof(h), "rc=%d, Datei %08X, nachgerechnet %08X",
                 rc, soll, ist);
        pruefe("die 32-Bit-Summe am Dateiende geht auf",
               rc == 0 && soll == ist, h);
    }

    /* ── Oeffnen ─────────────────────────────────────────────────────── */
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    uft_error_t rc = uft_format_plugin_scl.open(&disk, pfad, true);
    snprintf(h, sizeof(h), "open lieferte %d", (int)rc);
    pruefe("die SCL laesst sich oeffnen", rc == UFT_OK, h);
    if (rc != UFT_OK) { printf("\n%d gruen, %d rot\n", gruen, rot); return 1; }

    /* ── Geometrie: TR-DOS, nicht aus der Datenlaenge gerechnet ─────── */
    snprintf(h, sizeof(h), "%u Zyl, %u Koepfe, %u Sek/Spur, %u Byte, %u gesamt",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size,
             disk.geometry.total_sectors);
    pruefe("die Geometrie ist TR-DOS: 80 x 2 x 16 x 256 = 2560 Sektoren",
           disk.geometry.cylinders == 80 && disk.geometry.heads == 2
           && disk.geometry.sectors == 16
           && disk.geometry.sector_size == 256
           && disk.geometry.total_sectors == 2560, h);

    /* ── Spur 0, Kopf 0: der KATALOG, nicht Dateidaten ──────────────── */
    uft_track_t t0;
    memset(&t0, 0, sizeof(t0));
    rc = uft_format_plugin_scl.read_track(&disk, 0, 0, &t0);
    snprintf(h, sizeof(h), "rc=%d, %zu Sektoren", (int)rc,
             (size_t)t0.sector_count);
    pruefe("Zylinder 0 / Kopf 0 liefert 16 Sektoren",
           rc == UFT_OK && t0.sector_count == 16, h);

    if (t0.sector_count >= 9) {
        const uint8_t *s0 = t0.sectors[0].data;

        /* Eintrag A: 14 Byte wie in der Datei, dann Sektor 0 / Spur 1.
         * SAMdisk: pb[14] = uDataLba & 0x0f, pb[15] = uDataLba >> 4,
         * mit uDataLba = 16 -> 0 und 1. */
        int a_ok = (memcmp(s0, eintrag_a, 14) == 0)
                 && s0[14] == 0 && s0[15] == 1;
        snprintf(h, sizeof(h), "Sektor %u, Spur %u (erwartet 0 / 1)",
                 s0[14], s0[15]);
        pruefe("Katalogeintrag 1 steht bei Byte 0, Start bei LBA 16",
               a_ok, h);

        /* Eintrag B liegt 16 Byte weiter und startet bei LBA 16+3 = 19
         * -> Sektor 19 & 0x0F = 3, Spur 19 >> 4 = 1. */
        int b_ok = (memcmp(s0 + 16, eintrag_b, 14) == 0)
                 && s0[16 + 14] == 3 && s0[16 + 15] == 1;
        snprintf(h, sizeof(h), "Sektor %u, Spur %u (erwartet 3 / 1)",
                 s0[30], s0[31]);
        pruefe("Katalogeintrag 2 steht 16 Byte weiter, Start bei LBA 19",
               b_ok, h);

        /* Der Befund, der MF-1014 ausgeloest hat: hier standen
         * Dateidaten. Das erste Datenbyte ist 0xA0. */
        snprintf(h, sizeof(h), "Byte 0 ist %02X", s0[0]);
        pruefe("und es sind NICHT die Dateidaten (Byte 0 != 0xA0)",
               s0[0] == 'T' && s0[0] != 0xA0, h);

        /* ── Infosatz: Spur 0, Sektor 8 ─────────────────────────────── */
        const uint8_t *si = t0.sectors[8].data;
        /* Nach zwei Dateien: LBA = 16 + 3 + 1 = 20
         *   erster freier Sektor 20 & 0x0F = 4, Spur 20 >> 4 = 1
         *   freie Sektoren 2560 - 20 = 2540 = 0x09EC -> EC 09 */
        int leer_name = 1;
        for (int i = 0; i < 9; i++) if (si[245 + i] != 0) leer_name = 0;

        snprintf(h, sizeof(h),
                 "E1=%u E2=%u E3=%02X E4=%u E5/E6=%02X%02X E7=%02X F4=%u",
                 si[225], si[226], si[227], si[228], si[230], si[229],
                 si[231], si[244]);
        pruefe("der Infosatz traegt alle neun Felder der beiden Orakel",
               si[225] == 4 && si[226] == 1 && si[227] == 0x16
               && si[228] == 2 && si[229] == 0xEC && si[230] == 0x09
               && si[231] == 0x10 && si[244] == 0
               && memcmp(si + 234, "         ", 9) == 0, h);

        pruefe("und der Diskettenname bleibt LEER — die SCL traegt keinen",
               leer_name, leer_name ? NULL : "0x8F5 ist belegt");
    }
    frei(&t0);

    /* ── Spur 0, Kopf 1: hier beginnen die Dateidaten (LBA 16) ──────── */
    uft_track_t t1;
    memset(&t1, 0, sizeof(t1));
    rc = uft_format_plugin_scl.read_track(&disk, 0, 1, &t1);
    pruefe("Zylinder 0 / Kopf 1 liefert 16 Sektoren",
           rc == UFT_OK && t1.sector_count == 16, NULL);

    if (t1.sector_count >= 4) {
        /* Datei A: drei Sektoren, Muster 0xA0 ^ (i & 0xFF) */
        int a_ok = 1;
        for (int s = 0; s < SEK_A; s++)
            for (int i = 0; i < 256; i++) {
                size_t global = (size_t)s * 256 + (size_t)i;
                if (t1.sectors[s].data[i] != (uint8_t)(0xA0 ^ (global & 0xFF)))
                    a_ok = 0;
            }
        snprintf(h, sizeof(h), "Sektor 0 Byte 0 = %02X (erwartet A0)",
                 t1.sectors[0].data[0]);
        pruefe("Datei 1 liegt byteweise auf LBA 16..18", a_ok, h);

        /* Datei B: ein Sektor bei LBA 19 -> Sektor 3 */
        int b_ok = 1;
        for (int i = 0; i < 256; i++)
            if (t1.sectors[3].data[i] != (uint8_t)(0x5B ^ (i & 0xFF)))
                b_ok = 0;
        snprintf(h, sizeof(h), "Sektor 3 Byte 0 = %02X (erwartet 5B)",
                 t1.sectors[3].data[0]);
        pruefe("Datei 2 liegt byteweise auf LBA 19", b_ok, h);

        /* Hinter den Dateien ist die Diskette leer, nicht gefuellt. */
        int leer = 1;
        for (int i = 0; i < 256; i++) if (t1.sectors[4].data[i] != 0) leer = 0;
        pruefe("dahinter ist die Diskette leer (LBA 20)", leer, NULL);
    }
    frei(&t1);

    /* Kopf 2 gibt es nicht. */
    {
        uft_track_t tx;
        memset(&tx, 0, sizeof(tx));
        uft_error_t r2 = uft_format_plugin_scl.read_track(&disk, 0, 2, &tx);
        pruefe("Kopf 2 wird abgewiesen", r2 != UFT_OK, NULL);
        frei(&tx);
        memset(&tx, 0, sizeof(tx));
        r2 = uft_format_plugin_scl.read_track(&disk, -1, 0, &tx);
        pruefe("Zylinder -1 wird abgewiesen (MF-519)", r2 != UFT_OK, NULL);
        frei(&tx);
    }

    uft_format_plugin_scl.close(&disk);

    /* ── Gegenproben ─────────────────────────────────────────────────── */
    {
        /* 1. Kennung verfaelscht */
        static uint8_t b2[4096];
        size_t n2 = baue_scl(b2, sizeof(b2), 0, 0, 0);
        b2[0] = 'X';
        schreibe(pfad_magie, b2, n2);
        uft_disk_t d2;
        memset(&d2, 0, sizeof(d2));
        uft_error_t r = uft_format_plugin_scl.open(&d2, pfad_magie, true);
        pruefe("eine verfaelschte Kennung wird abgewiesen", r != UFT_OK, NULL);
        if (r == UFT_OK) uft_format_plugin_scl.close(&d2);
    }
    {
        /* 2. Die Grenze, wo sie liegt: 128 muss durch, 129 nicht.
         *    SAMdisk `sh.bFiles > 128`, HxC `*trd_files > 127`. Beide
         *    Dateien sind vollstaendig — sonst greift die Laengenschranke
         *    und die Probe waere gruen aus dem falschen Grund. */
        static uint8_t b3[8192];
        size_t n3 = baue_viele(b3, sizeof(b3), 128);
        schreibe(pfad_viele, b3, n3);
        uft_disk_t d3;
        memset(&d3, 0, sizeof(d3));
        uft_error_t r = uft_format_plugin_scl.open(&d3, pfad_viele, true);
        snprintf(h, sizeof(h), "%zu Byte, open=%d", n3, (int)r);
        pruefe("genau 128 Dateien gehen durch (der Katalog fuellt 8 Sektoren)",
               r == UFT_OK, h);
        if (r == UFT_OK) {
            uft_track_t tk;
            memset(&tk, 0, sizeof(tk));
            uft_error_t r3 = uft_format_plugin_scl.read_track(&d3, 0, 0, &tk);
            /* 128 * 16 = 2048 Byte Katalog = Sektoren 0..7; der letzte
             * Eintrag endet damit genau vor dem Infosatz. */
            int letzter = (r3 == UFT_OK && tk.sector_count == 16
                           && tk.sectors[7].data[240] == 'F'
                           && tk.sectors[8].data[228] == 128);
            snprintf(h, sizeof(h), "Sektor 7 Byte 240 = %02X, Dateizahl = %u",
                     tk.sectors[7].data[240], tk.sectors[8].data[228]);
            pruefe("der 128. Eintrag endet genau vor dem Infosatz",
                   letzter, h);
            frei(&tk);
            uft_format_plugin_scl.close(&d3);
        }

        n3 = baue_viele(b3, sizeof(b3), 129);
        schreibe(pfad_viele, b3, n3);
        memset(&d3, 0, sizeof(d3));
        r = uft_format_plugin_scl.open(&d3, pfad_viele, true);
        snprintf(h, sizeof(h), "%zu Byte, open=%d", n3, (int)r);
        pruefe("129 Dateien werden abgewiesen — der 129. traefe den Infosatz",
               r != UFT_OK, h);
        if (r == UFT_OK) uft_format_plugin_scl.close(&d3);

        int conf = 0;
        bool p = uft_format_plugin_scl.probe(b3, n3, n3, &conf);
        pruefe("und die Sonde sagt auch nein", !p, NULL);
    }
    {
        /* 3. Daten fehlen — Nullen als Dateiinhalt waeren erfundene Daten */
        static uint8_t b4[4096];
        size_t n4 = baue_scl(b4, sizeof(b4), 0, 0, 1);
        schreibe(pfad_kurz, b4, n4);
        uft_disk_t d4;
        memset(&d4, 0, sizeof(d4));
        uft_error_t r = uft_format_plugin_scl.open(&d4, pfad_kurz, true);
        pruefe("eine abgeschnittene Datei wird abgewiesen", r != UFT_OK, NULL);
        if (r == UFT_OK) uft_format_plugin_scl.close(&d4);
    }
    {
        /* 4. Ein Datenbyte gekippt, Summe unveraendert: die Datei laesst
         *    sich weiter lesen (Kein Bit verloren), aber die Summe faellt. */
        static uint8_t b5[4096];
        size_t n5 = baue_scl(b5, sizeof(b5), 0, 1, 0);
        schreibe(pfad_summe, b5, n5);
        uint32_t soll = 0, ist = 0;
        int prc = uft_scl_pruefsumme(pfad_summe, &soll, &ist);
        snprintf(h, sizeof(h), "rc=%d, Datei %08X, nachgerechnet %08X",
                 prc, soll, ist);
        pruefe("ein gekipptes Datenbyte laesst die Summe fallen",
               prc == 1 && soll != ist, h);

        uft_disk_t d5;
        memset(&d5, 0, sizeof(d5));
        uft_error_t r = uft_format_plugin_scl.open(&d5, pfad_summe, true);
        pruefe("und die Datei bleibt lesbar (Kein Bit verloren)",
               r == UFT_OK, NULL);
        if (r == UFT_OK) uft_format_plugin_scl.close(&d5);
    }

    /* ── Der zweite Befund: das verschluckte Laengenbyte ─────────────── */
    {
        /* `uft_scl_parse()` fuellte `param[0..2]` mit den Bytes 9,10,11
         * und las Byte **12** nie. Der Eintrag der ersten Datei traegt
         * mit Absicht die Laenge 0x0201 — das obere Byte ist 0x02. */
        uft_scl_t scl;
        memset(&scl, 0, sizeof(scl));
        int prc = uft_scl_parse(puffer, n, &scl);
        snprintf(h, sizeof(h), "rc=%d, %u Dateien", prc, scl.file_count);
        pruefe("uft_scl_parse liest beide Eintraege", prc == 0
               && scl.file_count == 2, h);

        if (prc == 0 && scl.file_count == 2) {
            snprintf(h, sizeof(h),
                     "Datei 1: Start %04X, Laenge %04X (erwartet 8000 / 0201)",
                     scl.entries[0].start_address, scl.entries[0].length_bytes);
            pruefe("Startadresse und Laenge stehen vollstaendig da — auch "
                   "das obere Byte",
                   scl.entries[0].start_address == 0x8000
                   && scl.entries[0].length_bytes == 0x0201, h);

            snprintf(h, sizeof(h),
                     "Datei 2: Start %04X, Laenge %04X (erwartet 1234 / 0100)",
                     scl.entries[1].start_address, scl.entries[1].length_bytes);
            pruefe("und bei der zweiten Datei ebenso",
                   scl.entries[1].start_address == 0x1234
                   && scl.entries[1].length_bytes == 0x0100, h);
        }
        uft_scl_free(&scl);
    }
    {
        /* Rundlauf: bauen und zurueckparsen. `uft_scl_build` schrieb
         * `h[12] = 0` — damit ueberlebte keine Laenge ueber 255 den Weg. */
        static const uint8_t daten_a[DATEN_A] = { 0 };
        const uint8_t *felder[1] = { daten_a };
        size_t groessen[1] = { DATEN_A };
        uft_scl_entry_t ein;
        memset(&ein, 0, sizeof(ein));
        memcpy(ein.name, "RUND    ", 8);
        ein.name[8] = '\0';
        ein.type = 0x42;
        ein.start_address = 0x8000;
        ein.length_bytes = 0x0201;
        ein.length_sectors = SEK_A;

        uint8_t *aus = NULL;
        size_t aus_n = 0;
        int brc = uft_scl_build(&ein, felder, groessen, 1, &aus, &aus_n);
        pruefe("uft_scl_build baut eine SCL", brc == 0 && aus != NULL, NULL);

        if (brc == 0 && aus) {
            uft_scl_t zurueck;
            memset(&zurueck, 0, sizeof(zurueck));
            int prc = uft_scl_parse(aus, aus_n, &zurueck);
            snprintf(h, sizeof(h), "zurueck: Start %04X, Laenge %04X",
                     prc == 0 && zurueck.file_count ? zurueck.entries[0].start_address : 0,
                     prc == 0 && zurueck.file_count ? zurueck.entries[0].length_bytes : 0);
            pruefe("und der Rundlauf traegt die Laenge 0x0201 zurueck",
                   prc == 0 && zurueck.file_count == 1
                   && zurueck.entries[0].start_address == 0x8000
                   && zurueck.entries[0].length_bytes == 0x0201, h);
            uft_scl_free(&zurueck);
            free(aus);
        }
    }

    remove(pfad); remove(pfad_magie); remove(pfad_viele);
    remove(pfad_kurz); remove(pfad_summe);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

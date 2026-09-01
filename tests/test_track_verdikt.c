/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_track_verdikt.c
 * @brief Der eine Verdikt-Bauer, gegen die Handbuch-Tabelle (MF-765).
 *
 * Jeder Fall hier ist eine Zeile der Abbildung
 * `docs/nachbau/XCOPY_TRIPEL_TABELLE.md` — X-Copy-Code auf
 * (Diagnose, Folge, Reparierbarkeit). Die Soll-Seite stammt aus den
 * Handbüchern von 1991 (3.4 Abschnitt „7.2) ERRORS", 5.21), nicht aus dem
 * Quelltext der Vorlage.
 *
 * ── Der wichtigste Fall ist die FM-Gegenprobe ────────────────────────────
 *
 * Das Histogrammodul ist auf MFM gebaut (Abstände 2T/3T/4T, drei Berge).
 * Ein FM-Strom trägt ZWEI Berge und käme mit `sicher == false` heraus —
 * er würde als **UNLESBAR** eingestuft, also eine gültige Spur als
 * defekt gemeldet.
 *
 * Deshalb entscheidet der Aufrufer über `histogramm_gueltig`, und dieser
 * Test friert das ein: derselbe Befund einmal mit und einmal ohne
 * gültiges Bandmodell MUSS verschiedene Diagnosen liefern. Wer das
 * Histogramm später nach Kodierung parametrisiert, findet den Rotbeweis
 * hier schon vor.
 */
#include <stdio.h>
#include <string.h>
#include "uft/flux/uft_track_verdikt.h"

static int fehler = 0;

static void pruefe(const char *titel,
                   uft_track_befunde_t b,
                   uft_track_diagnose_t d_soll,
                   uft_track_folge_t f_soll,
                   uft_track_reparierbarkeit_t r_soll)
{
    uft_track_verdikt_t v;
    uft_track_verdikt_bilden(&b, &v);

    int ok = (v.diagnose == d_soll) && (v.folge == f_soll)
          && (v.reparierbarkeit == r_soll);
    if (!ok) fehler++;

    printf("  %s %-38s %-12s %-21s %s\n",
           ok ? "ok  " : "FAIL", titel,
           uft_track_diagnose_name(v.diagnose),
           uft_track_folge_name(v.folge),
           uft_track_reparierbarkeit_name(v.reparierbarkeit));
    if (!ok)
        printf("       erwartet: %s / %s / %s\n",
               uft_track_diagnose_name(d_soll),
               uft_track_folge_name(f_soll),
               uft_track_reparierbarkeit_name(r_soll));
}

int main(void)
{
    uft_track_befunde_t b;
    printf("test_track_verdikt (MF-765)\n");

    /* Code 0 — grüne Null: elf Sektoren, nichts zu melden. */
    memset(&b, 0, sizeof(b));
    b.sector_count = 11; b.expected_sectors = 11;
    pruefe("0 alles gelesen", b, UFT_DIAG_KEINE,
           UFT_FOLGE_REPRODUZIERBAR, UFT_REP_NICHT_NOETIG);

    /* Code 1 — Sektorzahl weicht ab. BEIDE Richtungen, wie das Handbuch
     * sagt: „less or MORE than 11 sectors". */
    memset(&b, 0, sizeof(b));
    b.sector_count = 9; b.expected_sectors = 11;
    pruefe("1 zu wenige Sektoren", b, UFT_DIAG_FREMDFORMAT,
           UFT_FOLGE_REPRODUZIERBAR, UFT_REP_NICHT_NOETIG);

    memset(&b, 0, sizeof(b));
    b.sector_count = 12; b.expected_sectors = 11;
    pruefe("1 zu VIELE Sektoren", b, UFT_DIAG_FREMDFORMAT,
           UFT_FOLGE_REPRODUZIERBAR, UFT_REP_NICHT_NOETIG);

    /* Sollzahl unbekannt: dann darf die Prüfung NICHT greifen. Eine
     * Diagnose „Fremdformat" ohne bekannte Erwartung wäre erfunden. */
    memset(&b, 0, sizeof(b));
    b.sector_count = 9; b.expected_sectors = 0;
    pruefe("1 Sollzahl unbekannt -> kein Urteil", b, UFT_DIAG_KEINE,
           UFT_FOLGE_REPRODUZIERBAR, UFT_REP_NICHT_NOETIG);

    /* Code 2 — keine Marke, aber Struktur da (ein Berg). */
    memset(&b, 0, sizeof(b));
    b.histogramm_gueltig = true; b.histogramm_berge = 1;
    pruefe("2 Struktur ohne Marke", b, UFT_DIAG_SCHUTZ,
           UFT_FOLGE_REPRODUZIERBAR, UFT_REP_NICHT_NOETIG);

    /* leer — gar keine Struktur. */
    memset(&b, 0, sizeof(b));
    b.histogramm_gueltig = true; b.histogramm_berge = 0;
    pruefe("leer (keine Wechsel-Struktur)", b, UFT_DIAG_LEER,
           UFT_FOLGE_REPRODUZIERBAR, UFT_REP_NICHT_NOETIG);

    /* unlesbar — viele Berge, kein gemeinsamer Takt. */
    memset(&b, 0, sizeof(b));
    b.histogramm_gueltig = true; b.histogramm_berge = 8;
    b.histogramm_sicher = false;
    pruefe("verrauscht (kein Takt)", b, UFT_DIAG_UNLESBAR,
           UFT_FOLGE_REPRODUZIERBAR, UFT_REP_NICHT_NOETIG);

    /* ── DIE FM-GEGENPROBE ───────────────────────────────────────────────
     *
     * Derselbe Befund wie „verrauscht", aber das Bandmodell passt NICHT
     * (FM hat zwei Berge, das Modul rechnet mit drei). Der Bauer darf
     * dann NICHT auf unlesbar erkennen — sonst wird eine gültige
     * FM-Spur als defekt gemeldet.
     *
     * Wer das Histogramm später nach Kodierung parametrisiert, muss
     * diesen Fall bewusst umstellen. Genau dafür steht er hier. */
    memset(&b, 0, sizeof(b));
    b.histogramm_gueltig = false;      /* <- der ganze Unterschied */
    b.histogramm_berge = 8;
    b.histogramm_sicher = false;
    pruefe("FM-Gegenprobe: Bandmodell passt nicht", b, UFT_DIAG_SCHUTZ,
           UFT_FOLGE_REPRODUZIERBAR, UFT_REP_NICHT_NOETIG);

    /* Codes 4 und 6 — Prüfsummen, laut Handbuch korrigierbar. */
    memset(&b, 0, sizeof(b));
    b.sector_count = 11; b.expected_sectors = 11; b.bad_id_crc = 1;
    pruefe("4 Kopf-Pruefsumme", b, UFT_DIAG_SCHADEN,
           UFT_FOLGE_REPRODUZIERBAR, UFT_REP_KORRIGIERBAR);

    memset(&b, 0, sizeof(b));
    b.sector_count = 11; b.expected_sectors = 11; b.bad_data_crc = 1;
    pruefe("6 Daten-Pruefsumme", b, UFT_DIAG_SCHADEN,
           UFT_FOLGE_REPRODUZIERBAR, UFT_REP_KORRIGIERBAR);

    /* Code 5 — Kopfinhalt zerstört. Das Handbuch nennt ihn NICHT als
     * korrigierbar; er darf also nicht mit 4 und 6 zusammenfallen. */
    memset(&b, 0, sizeof(b));
    b.sector_count = 11; b.expected_sectors = 11; b.bad_header_format = 1;
    pruefe("5 Kopfinhalt zerstoert", b, UFT_DIAG_SCHADEN,
           UFT_FOLGE_REPRODUZIERBAR, UFT_REP_NICHT_KORRIGIERBAR);

    /* Code 7 — überlange Spur. Die FOLGE ist die eigentliche Aussage:
     * mit gewöhnlichen Laufwerken nicht reproduzierbar. */
    memset(&b, 0, sizeof(b));
    b.sector_count = 11; b.expected_sectors = 11; b.ueberlange_spur = true;
    pruefe("7 ueberlange Spur", b, UFT_DIAG_KEINE,
           UFT_FOLGE_NICHT_REPRODUZIERBAR, UFT_REP_NICHT_NOETIG);

    /* GERETTET — schlägt alles. Das ist die Regel aus dem
     * Änderungsprotokoll zu 5.21: eine Rettung darf nicht als Erfolg
     * erscheinen. */
    memset(&b, 0, sizeof(b));
    b.sector_count = 11; b.expected_sectors = 11;
    b.bad_data_crc = 1; b.corrections_applied = 1;
    pruefe("gerettet schlaegt korrigierbar", b, UFT_DIAG_SCHADEN,
           UFT_FOLGE_REPRODUZIERBAR, UFT_REP_GERETTET);

    printf("%s (%d Fehler)\n", fehler ? "FAIL" : "PASS", fehler);
    return fehler ? 1 : 0;
}

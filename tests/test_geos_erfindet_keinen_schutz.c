/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * test_geos_erfindet_keinen_schutz.c — der GEOS-Detektor muss messen,
 * bevor er meldet (MF-1333, Stufe 0).
 *
 * ── Die benannte Referenz ────────────────────────────────────────────
 * `docs/format_specs/commodore/GEOS.TXT` liegt im Baum und sagt in
 * Z. 247-249 WOERTLICH, wie GEOS selbst prueft:
 *
 *   "If you want to check to see if a disk is formatted for GEOS, check
 *    the string in the BAM sector starting at $AD (offset 173) for the
 *    string "GEOS format". If it does not match, the disk is not in GEOS
 *    format. This is the way that GEOS itself verifies if a disk is GEOS
 *    formatted."
 *
 * Dieselbe Datei, Z. 220 und Z. 232:
 *   "04-8F: BAM entries for each track, in groups of four bytes per
 *           track, starting on track 1"
 *   "AD-BC: GEOS ID string ("GEOS format V1.x", in ASCII)"
 *
 * Zweite Referenz fuer den SCHUTZ selbst ist die Beschreibung des
 * URHEBERS, `neue-ideen/geocopy.zip -> geocopy/READ.ME.cvt` (Christian
 * Meilinger). Sie nennt die Spur woertlich:
 *   "Dieser Track ist auf allen GEOS-Boot-Disketten die Nummer 21
 *    (dezimal)."
 * und den Inhalt der Luecken ("$55 $55 $67 ..."). Kanal nach MF-695 ist
 * *Spec*: die Lizenz im Paket gewaehrt die Weitergabe, verbietet aber
 * kommerziellen Vertrieb ohne Einwilligung — damit ist ein Port in den
 * GPL-2-Baum ausgeschlossen, das Lesen nicht.
 *
 * ── Was vor MF-1333 gemessen falsch war ──────────────────────────────
 * Der Detektor suchte 4 Byte "GEOS" an BELIEBIGEM Versatz in SPUR 1
 * SEKTOR 0. Dreifach daneben: falscher Sektor, falscher Versatz, zu
 * schwache Kennung. Klasse MF-961 (`86f`) und MF-1022 (`sap`) — eine
 * echte Diskette wird nicht erkannt, eine fremde angenommen.
 *
 * Und alle drei Schutzarten, die er je zuwies, entstanden OHNE eine
 * Messung am Datentraeger:
 *   - V1_KEY_DISK   <- `track_count > 35`, eine reine Daseinsabfrage
 *   - V2_ENHANCED   <- bedingungslos, sobald irgendwo "GEOS format" stand
 *   - BAM_SIGNATURE <- `data[4 + 18*4]`, und das ist der Eintrag von
 *                      SPUR 19; Spur 18 laege bei `4 + 17*4`.
 *
 * Dass Spur 18 gemeint war, beweist die Konstante daneben: `!= 0x11`
 * (17 frei) trifft genau Spur 18 mit belegtem BAM- und erstem
 * Verzeichnissektor. Gegengeprueft an sechs Stellen im eigenen Baum,
 * die alle `4 + (track - 1) * 4` rechnen (u. a.
 * `src/formats/c64/uft_d71_d81.c:310` und
 * `src/formats/d64/uft_d64_parser_v3.c:1211`).
 *
 * ── Was dieser Test NICHT zusichert ──────────────────────────────────
 * Dass der Gap-Schutz erkannt wird. Er kann aus Sektordaten gar nicht
 * erkannt werden — genau deshalb verlangt Zusage 6, dass der Detektor
 * das SAGT (`GEOS_UNMESSBAR_GAPS`) statt zu schweigen.
 */

#include <stdio.h>
#include <string.h>

#include "uft/protection/uft_geos_protection.h"
#include "uft/formats/cbm/uft_cbm_geometry.h"

static int fehler = 0;

static void zusage(int ok, const char *was)
{
    printf("  %-64s %s\n", was, ok ? "ok" : "[ROT]");
    if (!ok) fehler++;
}

/* ── Eine 1541-Diskette im Speicher ──────────────────────────────────
 * 35 Spuren, je EIN Sektor 0 mit 256 Byte. Mehr braucht der Detektor
 * nicht: er liest Spur 1 Sektor 0 und Spur 18 Sektor 0.
 */
#define SPUREN 35

typedef struct {
    uint8_t          daten[SPUREN][256];
    uft_sector_t     sektoren[SPUREN];
    uft_track_t      spuren[SPUREN];
    uft_track_t     *zeiger[SPUREN];
    uft_disk_image_t bild;
} diskette_t;

static void diskette_bauen(diskette_t *d)
{
    memset(d, 0, sizeof *d);
    for (int i = 0; i < SPUREN; i++) {
        d->sektoren[i].data      = d->daten[i];
        d->sektoren[i].data_len  = 256;
        d->sektoren[i].id.sector = 0;
        d->spuren[i].sectors      = &d->sektoren[i];
        d->spuren[i].sector_count = 1;
        d->zeiger[i] = &d->spuren[i];
    }
    d->bild.track_data  = d->zeiger;
    d->bild.track_count = SPUREN;
}

/* Der BAM-Sektor liegt auf Spur 18, also Index 17. */
static uint8_t *bam(diskette_t *d) { return d->daten[17]; }

/* BAM-Eintrag einer Spur: vier Byte ab `4 + (spur-1)*4`, erstes Byte
 * ist die Zahl der freien Sektoren (GEOS.TXT:220). */
static void bam_frei_setzen(diskette_t *d, int spur, uint8_t frei)
{
    bam(d)[4 + (spur - 1) * 4] = frei;
}

/* Eine gewoehnliche, leere 1541-BAM: jede Spur voll frei, Spur 18 mit
 * BAM- und erstem Verzeichnissektor belegt (17 von 19 frei). */
static void bam_leer_fuellen(diskette_t *d)
{
    uint8_t *b = bam(d);
    b[0] = 18; b[1] = 1;      /* erster Verzeichnissektor */
    b[2] = 0x41;              /* DOS-Typ 1541 */
    b[3] = 0x2A;
    for (int spur = 1; spur <= SPUREN; spur++) {
        int n = uft_cbm_sectors_per_track(UFT_CBM_1541, spur);
        bam_frei_setzen(d, spur, (uint8_t)n);
    }
    bam_frei_setzen(d, 18, 17);
}

/* Die Kennung, an der GEOS selbst eine GEOS-Diskette erkennt. */
static void geos_kennung_setzen(diskette_t *d, const char *fassung)
{
    memcpy(bam(d) + 0xAD, fassung, strlen(fassung));
}

int main(void)
{
    printf("Der GEOS-Detektor misst, bevor er meldet (MF-1333)\n");

    /* Vorbedingung: die Geometriequelle antwortet ueberhaupt. Faellt sie,
     * sind alle Zahlen darunter wertlos (MF-1125: Werkzeugfehler von
     * Pruefling trennen). */
    int spur21_n = uft_cbm_sectors_per_track(UFT_CBM_1541, 21);
    if (spur21_n != 19) {
        printf("  [ROT] Geometriequelle sagt %d Sektoren auf Spur 21, "
               "erwartet 19 — Vorbedingung traegt nicht\n", spur21_n);
        return 1;
    }

    /* ── 1. Eine ECHTE GEOS-Diskette wird erkannt ───────────────────
     * Kennung NUR im BAM-Sektor, wie die Beschreibung es sagt. Spur 1
     * Sektor 0 bleibt leer — vor MF-1333 suchte der Detektor genau
     * dort, also war eine echte Diskette unerreichbar. */
    {
        diskette_t d;
        diskette_bauen(&d);
        bam_leer_fuellen(&d);
        geos_kennung_setzen(&d, "GEOS format V1.3");

        geos_analysis_result_t r;
        memset(&r, 0xAA, sizeof r);
        int rc = uft_geos_analyze_disk(&d.bild, &r);

        zusage(rc == UFT_OK, "echte GEOS-Diskette: Aufruf gelingt");
        zusage(r.is_geos_disk,
               "echte GEOS-Diskette: wird als GEOS erkannt");
        zusage(r.geos_version == 1,
               "echte GEOS-Diskette: Fassung 1 aus der Kennung gelesen");
    }

    /* ── 2. Ein HOCHSTAPLER wird abgewiesen ─────────────────────────
     * "GEOS format" steht in Spur 1 Sektor 0, aber NICHT an $AD des
     * BAM-Sektors. Nach der Regel, nach der GEOS selbst prueft, ist das
     * keine GEOS-Diskette. */
    {
        diskette_t d;
        diskette_bauen(&d);
        bam_leer_fuellen(&d);
        memcpy(d.daten[0] + 50, "GEOS format V1.3", 16);

        geos_analysis_result_t r;
        memset(&r, 0xAA, sizeof r);
        uft_geos_analyze_disk(&d.bild, &r);

        zusage(!r.is_geos_disk,
               "Hochstapler in Spur 1: wird NICHT als GEOS erkannt");
        zusage(r.protection_count == 0,
               "Hochstapler in Spur 1: meldet keinen Schutz");
    }

    /* ── 3. Der BAM-Versatz trifft Spur 18, nicht Spur 19 ───────────
     * Spur 18 ist gewoehnlich (17 frei), Spur 19 ist voll belegt (0).
     * Vor MF-1333 las der Detektor `4 + 18*4` — den Eintrag von Spur 19
     * — und meldete daraus eine BAM-Signatur. Spur 21 bleibt hier frei,
     * es darf also GAR KEINE Signatur kommen. */
    {
        diskette_t d;
        diskette_bauen(&d);
        bam_leer_fuellen(&d);
        geos_kennung_setzen(&d, "GEOS format V1.3");
        bam_frei_setzen(&d, 19, 0);          /* Nachbarspur belegt */
        memcpy(d.daten[0], "GEOS", 4);       /* alter Pfad wuerde greifen */

        geos_analysis_result_t r;
        memset(&r, 0xAA, sizeof r);
        uft_geos_analyze_disk(&d.bild, &r);

        int signatur = 0;
        for (int i = 0; i < r.protection_count; i++)
            if (r.protections[i] == GEOS_PROT_BAM_SIGNATURE) signatur = 1;

        zusage(!signatur,
               "belegte Spur 19 loest KEINE BAM-Signatur aus");
        zusage(r.spur21_bam_gelesen && r.spur21_frei == 19,
               "Spur 21 wird gelesen und als frei gemeldet (19 von 19)");
    }

    /* ── 4. Der ehrliche Positivfall ────────────────────────────────
     * Spur 21 ist in der BAM vollstaendig belegt — genau das, was der
     * Urheber beschreibt ("Der Track $15 ist in der BAM geschuetzt").
     * Das ist ein INDIZ und wird als solches gemeldet, mit der Zahl. */
    {
        diskette_t d;
        diskette_bauen(&d);
        bam_leer_fuellen(&d);
        geos_kennung_setzen(&d, "GEOS format V1.3");
        bam_frei_setzen(&d, 21, 0);

        geos_analysis_result_t r;
        memset(&r, 0xAA, sizeof r);
        uft_geos_analyze_disk(&d.bild, &r);

        int signatur = 0;
        for (int i = 0; i < r.protection_count; i++)
            if (r.protections[i] == GEOS_PROT_BAM_SIGNATURE) signatur = 1;

        zusage(signatur,
               "vollstaendig belegte Spur 21 meldet eine BAM-Signatur");
        zusage(r.spur21_bam_gelesen && r.spur21_frei == 0 &&
               r.spur21_sektoren == 19,
               "die Zahlen stehen dabei: 0 von 19 frei");
        zusage(r.konfidenz > 0 && r.konfidenz <= 60,
               "Konfidenz ist vergeben und ohne Gaps gedeckelt (<= 60)");
    }

    /* ── 5. KEINE Erfindung ─────────────────────────────────────────
     * Eine GEOS-Diskette ohne jedes Schutzmerkmal darf NICHTS melden.
     * Vor MF-1333 kam hier V2_ENHANCED bedingungslos. */
    {
        diskette_t d;
        diskette_bauen(&d);
        bam_leer_fuellen(&d);
        geos_kennung_setzen(&d, "GEOS format V2.0");

        geos_analysis_result_t r;
        memset(&r, 0xAA, sizeof r);
        uft_geos_analyze_disk(&d.bild, &r);

        zusage(r.is_geos_disk && r.geos_version == 2,
               "GEOS 2.0 wird erkannt und die Fassung gelesen");
        zusage(r.protection_count == 0,
               "ohne Merkmal wird KEIN Schutz gemeldet (kein V2_ENHANCED)");
    }

    /* ── 6. Das Schweigen wird benannt ──────────────────────────────
     * Aus Sektordaten ist der Gap-Schutz prinzipiell unerreichbar. Der
     * Detektor muss das SAGEN, sonst ist "0 Schutzarten" mehrdeutig
     * (MF-1311). */
    {
        diskette_t d;
        diskette_bauen(&d);
        bam_leer_fuellen(&d);
        geos_kennung_setzen(&d, "GEOS format V1.3");

        geos_analysis_result_t r;
        memset(&r, 0xAA, sizeof r);
        uft_geos_analyze_disk(&d.bild, &r);

        zusage((r.unmessbar & (uint32_t)GEOS_UNMESSBAR_GAPS) != 0,
               "der Gap-Schutz wird ausdruecklich als unmessbar gemeldet");
        zusage((r.unmessbar & (uint32_t)GEOS_UNMESSBAR_VERZEICHNIS) != 0,
               "die unverfolgte Blockkette wird ebenso benannt");
    }

    /* ── 7. Keine GEOS-Diskette, kein Schweigen zu deuten ───────────
     * Bei einer gewoehnlichen Diskette wird nichts gemeldet UND nichts
     * als unmessbar beansprucht: der Detektor hat nichts vor. */
    {
        diskette_t d;
        diskette_bauen(&d);
        bam_leer_fuellen(&d);

        geos_analysis_result_t r;
        memset(&r, 0xAA, sizeof r);
        uft_geos_analyze_disk(&d.bild, &r);

        zusage(!r.is_geos_disk && r.protection_count == 0,
               "gewoehnliche Diskette: weder GEOS noch Schutz");
        zusage(r.unmessbar == (uint32_t)GEOS_UNMESSBAR_NICHTS,
               "gewoehnliche Diskette: nichts blieb offen");
        zusage(r.konfidenz == 0,
               "gewoehnliche Diskette: keine Konfidenz beansprucht");
    }

    /* ── 8. "Der erste im Feld" ist nicht "Sektor 0" ────────────────
     * Ein Leser, der Sektoren in LESEREIHENFOLGE ablegt, liefert sie in
     * beliebiger Folge — bei einer geschuetzten Diskette ist das der
     * Normalfall, nicht die Ausnahme. Der BAM-Sektor muss also ueber
     * seine ID gefunden werden. Hier liegt Sektor 0 an zweiter Stelle.
     */
    {
        diskette_t d;
        diskette_bauen(&d);
        bam_leer_fuellen(&d);
        geos_kennung_setzen(&d, "GEOS format V1.3");
        bam_frei_setzen(&d, 21, 0);

        /* Spur 18 bekommt einen zweiten Sektor VOR dem BAM-Sektor. */
        static uint8_t  fremd[256];
        static uft_sector_t zwei[2];
        memset(fremd, 0xEE, sizeof fremd);
        memset(zwei, 0, sizeof zwei);
        zwei[0].data = fremd;  zwei[0].data_len = 256; zwei[0].id.sector = 3;
        zwei[1] = d.sektoren[17];            /* der echte BAM-Sektor */
        d.spuren[17].sectors      = zwei;
        d.spuren[17].sector_count = 2;

        geos_analysis_result_t r;
        memset(&r, 0xAA, sizeof r);
        uft_geos_analyze_disk(&d.bild, &r);

        zusage(r.is_geos_disk,
               "BAM-Sektor wird ueber seine ID gefunden, nicht als [0]");
        zusage(r.spur21_bam_gelesen && r.spur21_frei == 0,
               "und die Zahlen stammen aus dem richtigen Sektor");
    }

    /* ── 8b. Die Kennung am FALSCHEN Versatz zaehlt nicht ───────────
     * GEOS prueft bei $AD, nicht irgendwo. Eine Diskette, die "GEOS
     * format" an anderer Stelle im BAM-Sektor traegt — als Diskettenname
     * etwa, der bei $90 liegt —, ist keine GEOS-Diskette.
     *
     * Diese Zusage ist NACHGETRAGEN: die Mutationsmatrix zu MF-1333 hat
     * gezeigt, dass eine Suche "an beliebigem Versatz" von den uebrigen
     * Faellen nicht gefangen wird, weil in ihnen allen die Kennung
     * ohnehin an der richtigen Stelle steht (Klasse MF-1014 — eine
     * Gegenprobe, die aus dem falschen Grund gruen ist). */
    {
        diskette_t d;
        diskette_bauen(&d);
        bam_leer_fuellen(&d);
        /* Diskettenname bei $90 (GEOS.TXT:221), 16 Byte. */
        memcpy(bam(&d) + 0x90, "GEOS format V1.3", 16);

        geos_analysis_result_t r;
        memset(&r, 0xAA, sizeof r);
        uft_geos_analyze_disk(&d.bild, &r);

        zusage(!r.is_geos_disk,
               "Kennung am falschen Versatz ($90): keine GEOS-Diskette");
        zusage(r.protection_count == 0 && r.konfidenz == 0,
               "Kennung am falschen Versatz: kein Schutz, keine Konfidenz");
    }

    /* ── 9. Ein zu kurzer BAM-Sektor ist "offen", nicht "nein" ──────
     * Ein Sektor unter 256 Byte ist kein CBM-BAM-Sektor. Der Detektor
     * darf daraus weder "keine GEOS-Diskette" folgern noch ueber das
     * Ende hinaus lesen.
     */
    {
        diskette_t d;
        diskette_bauen(&d);
        bam_leer_fuellen(&d);
        geos_kennung_setzen(&d, "GEOS format V1.3");
        d.sektoren[17].data_len = 100;       /* abgeschnitten */

        geos_analysis_result_t r;
        memset(&r, 0xAA, sizeof r);
        uft_geos_analyze_disk(&d.bild, &r);

        zusage(!r.is_geos_disk,
               "zu kurzer BAM-Sektor: keine GEOS-Behauptung");
        zusage((r.unmessbar & (uint32_t)GEOS_UNMESSBAR_BAM) != 0,
               "zu kurzer BAM-Sektor: als unmessbar benannt");
        zusage(!r.spur21_bam_gelesen,
               "zu kurzer BAM-Sektor: Spur 21 gilt als NICHT gelesen");
    }

    printf("%s\n", fehler ? "FEHLGESCHLAGEN" : "BESTANDEN (0 Fehler)");
    return fehler ? 1 : 0;
}

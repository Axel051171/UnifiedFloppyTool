/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * test_geos_rebuild_transaktion.c — die Schutzspur wird frei, und die
 * Dateien ueberleben es (MF-1333, Stufe 4).
 *
 * ── Die eigentliche Zusage ───────────────────────────────────────────
 * Nicht "es hat funktioniert", sondern: **der INHALT jeder Datei ist
 * nach dem Umbau byteidentisch**. Alles andere — verschobene Bloecke,
 * BAM-Zahlen, Kettenlaengen — kann stimmen, waehrend eine Datei
 * zerrissen ist. Deshalb liest dieser Test jede Datei VOR und NACH dem
 * Umbau ueber ihre Kette aus und vergleicht Byte fuer Byte.
 *
 * Das ist die Lehre aus MF-1026 in ihrer schaerfsten Form: eine Summe,
 * die aufgeht, sagt nichts ueber die Verteilung darin. Dort hoben sich
 * zwei Zonengrenzen um +1 und -1 auf, die Summe blieb 1224, und 22
 * Spuren wurden falsch gelesen.
 *
 * ── Und die zweite: das Ziel bleibt unberuehrt ───────────────────────
 * Eine Transaktion, die bei einem Fehlschlag Spuren hinterlaesst, ist
 * keine. Jeder Absagefall prueft deshalb, dass das ZIEL unveraendert
 * ist — nicht nur, dass ein Fehlercode kam.
 *
 * ── Die Referenz ─────────────────────────────────────────────────────
 * Verfahren: Beschreibung des Urhebers von GeoCopy (Christian
 * Meilinger, `neue-ideen/geocopy.zip -> geocopy/READ.ME.cvt`), Kanal
 * *Spec* nach MF-695 — NC-Klausel, kein Port. Diskettenaufbau:
 * `docs/format_specs/commodore/GEOS.TXT`.
 *
 * ── Was dieser Test NICHT belegt ─────────────────────────────────────
 * Dass eine ECHTE GEOS-Bootdiskette so aussieht. Im Korpus liegt
 * keine; CBMFiles untersagt die Weitergabe, und dieses Haus hat keine
 * Hardware (MF-310). Geprueft ist die MECHANIK an Disketten, deren
 * Aufbau der Test selbst kennt.
 *
 * ── Mutationsmatrix: 11 von 15, und die vier sind EINE Klasse ────────
 *
 * Gefangen: Verzeichniseintraege nicht nachgezogen · Blockverkettung
 * nicht nachgezogen · Blockinhalt nicht mitkopiert · alte Bloecke
 * bleiben belegt · neue Bloecke bleiben frei · Ziel auf der Schutzspur
 * erlaubt · Quelle gleich Ziel durchgelassen · Groessenpruefung weg ·
 * Verweis ins Leere angenommen · Verzeichnissektor auf Spur 21
 * angenommen · ein Ziel um einen Sektor verschoben vergeben.
 *
 * DURCHGERUTSCHT sind vier, und sie haben denselben Grund: die
 * NACHpruefungen in Schritt 6 (`blockzahl_gleich`,
 * `spur21_unreferenziert`, `bam_stimmig`) feuern bei korrektem Code
 * **nie** — sie sind Tiefenstaffelung gegen einen Fehler an einer
 * ANDEREN Stelle. Und weil sie nie feuern, ist auch die vierte nicht
 * beobachtbar: dass das Ziel erst NACH ihnen beschrieben wird.
 *
 * Das ist kein Loch im Test, sondern die Bauform. Ohne
 * Fehlerinjektion — also test-only-Code im Produktivpfad, den dieser
 * Baum zu Recht ablehnt — laesst sich ein Waechter, der nie anschlagen
 * darf, nicht beim Anschlagen beobachten. Gestalt von MF-1031
 * (gegenseitig redundante Schranken) und MF-1028 ("neun von zehn, und
 * die zehnte ist benannt statt verschwiegen").
 *
 * Zwei der sechs urspruenglich Entkommenen waren dagegen ECHTE Luecken
 * im Test und sind geschlossen: `freien_finden()` laeuft die Spuren
 * aufsteigend ab und erreichte die Sperre gegen Spur 21 nie, weil auf
 * jeder Pruefdiskette frueher etwas frei war (Fall 6b); und ein
 * Verzeichnissektor auf der Schutzspur kam nicht vor (Fall 6c).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* strstr fuer die Grundpruefung */

#include "uft/protection/uft_geos_rebuild.h"
#include "uft/formats/cbm/uft_cbm_geometry.h"

static int fehler = 0;

static void zusage(int ok, const char *was)
{
    printf("  %-66s %s\n", was, ok ? "ok" : "[ROT]");
    if (!ok) fehler++;
}

#define SPUREN      35
#define SEKTOR_LEN  256
#define D64_LEN     UFT_GEOS_D64_GROESSE
#define VERZ_SPUR   18
#define SCHUTZSPUR  UFT_GEOS_SCHUTZSPUR

static uint8_t g_quelle[D64_LEN];
static uint8_t g_ziel[D64_LEN];

/* Die Geometrie kommt aus derselben Quelle wie beim Pruefling — das ist
 * hier richtig (MF-1177: eine Groesse, eine Rechnung). Gepruefte SACHE
 * ist die Verschiebung, nicht die Zonentafel. */
static size_t versatz(int spur, int sektor)
{
    size_t b = 0;
    for (int t = 1; t < spur; t++)
        b += (size_t)uft_cbm_sectors_per_track(UFT_CBM_1541, t);
    return (b + (size_t)sektor) * SEKTOR_LEN;
}

#define BAM_EINTRAG(spur) (4 + ((spur) - 1) * 4)

static void bam_belegen(uint8_t *d, int spur, int sektor)
{
    uint8_t *e = d + versatz(VERZ_SPUR, 0) + BAM_EINTRAG(spur);
    uint8_t m = (uint8_t)(1u << (sektor & 7));
    if (e[1 + (sektor >> 3)] & m) {
        e[1 + (sektor >> 3)] &= (uint8_t)~m;
        e[0]--;
    }
}

static int bam_ist_frei(const uint8_t *d, int spur, int sektor)
{
    const uint8_t *e = d + versatz(VERZ_SPUR, 0) + BAM_EINTRAG(spur);
    return (e[1 + (sektor >> 3)] >> (sektor & 7)) & 1;
}

/** Eine leere, formatierte 1541-Diskette mit GEOS-Kennung. */
static void diskette_formatieren(uint8_t *d)
{
    memset(d, 0, D64_LEN);

    uint8_t *bam = d + versatz(VERZ_SPUR, 0);
    bam[0] = VERZ_SPUR; bam[1] = 1;      /* erster Verzeichnissektor */
    bam[2] = 0x41; bam[3] = 0x2A;

    for (int spur = 1; spur <= SPUREN; spur++) {
        int n = uft_cbm_sectors_per_track(UFT_CBM_1541, spur);
        uint8_t *e = bam + BAM_EINTRAG(spur);
        e[0] = (uint8_t)n;
        e[1] = e[2] = e[3] = 0;
        for (int s = 0; s < n; s++)
            e[1 + (s >> 3)] |= (uint8_t)(1u << (s & 7));
    }

    /* GEOS-Kennung bei $AD (GEOS.TXT:232). */
    memcpy(bam + 0xAD, "GEOS format V1.3", 16);

    bam_belegen(d, VERZ_SPUR, 0);
    bam_belegen(d, VERZ_SPUR, 1);

    uint8_t *v = d + versatz(VERZ_SPUR, 1);
    v[0] = 0; v[1] = 0xFF;               /* keine Fortsetzung */
}

/** Legt eine Datei aus `n` Bloecken an den genannten Stellen an.
 *  `stellen` traegt 2*n Zahlen: Spur, Sektor, Spur, Sektor … */
static void datei_anlegen(uint8_t *d, int eintrag_nr,
                          const int *stellen, int n)
{
    for (int i = 0; i < n; i++) {
        int spur = stellen[2 * i], sek = stellen[2 * i + 1];
        uint8_t *b = d + versatz(spur, sek);

        if (i + 1 < n) {
            b[0] = (uint8_t)stellen[2 * (i + 1)];
            b[1] = (uint8_t)stellen[2 * (i + 1) + 1];
        } else {
            b[0] = 0;        /* letzter Block */
            b[1] = 0xFF;     /* alle 254 Nutzbyte belegt */
        }
        /* Selbstbenennender Inhalt: Dateinummer und Blockindex. */
        for (int k = 2; k < SEKTOR_LEN; k++)
            b[k] = (uint8_t)(eintrag_nr * 37 + i * 11 + k);

        bam_belegen(d, spur, sek);
    }

    uint8_t *e = d + versatz(VERZ_SPUR, 1) + (size_t)eintrag_nr * 32;
    e[2] = 0x82;                          /* PRG, geschlossen */
    e[3] = (uint8_t)stellen[0];
    e[4] = (uint8_t)stellen[1];
    memset(e + 5, 0xA0, 16);
    e[5] = (uint8_t)('A' + eintrag_nr);
    e[0x1E] = (uint8_t)n;                 /* Blockzahl, LSB */
    e[0x1F] = 0;
}

/** Liest eine Datei ueber ihre Kette. Gibt die Blockzahl zurueck oder
 *  -1; der Inhalt landet in `aus` (254 Byte je Block). */
static int datei_lesen(const uint8_t *d, int eintrag_nr,
                       uint8_t *aus, size_t aus_len)
{
    const uint8_t *e = d + versatz(VERZ_SPUR, 1) + (size_t)eintrag_nr * 32;
    if (e[2] == 0) return -1;
    int spur = e[3], sek = e[4], n = 0;
    size_t p = 0;

    while (spur != 0) {
        if (spur < 1 || spur > SPUREN) return -1;
        int max = uft_cbm_sectors_per_track(UFT_CBM_1541, spur);
        if (sek < 0 || sek >= max) return -1;
        if (++n > 700) return -1;

        const uint8_t *b = d + versatz(spur, sek);
        if (p + 254 > aus_len) return -1;
        memcpy(aus + p, b + 2, 254);
        p += 254;

        int ns = b[0], nsek = b[1];
        spur = ns; sek = nsek;
    }
    return n;
}

/** Zaehlt unabhaengig nach, wie viele Bloecke der Schutzspur noch von
 *  einer Kette benutzt werden. */
static int spur21_noch_benutzt(const uint8_t *d)
{
    int treffer = 0;
    for (int e = 0; e < 8; e++) {
        const uint8_t *ein = d + versatz(VERZ_SPUR, 1) + (size_t)e * 32;
        if (ein[2] == 0) continue;
        int spur = ein[3], sek = ein[4], wache = 0;
        while (spur != 0 && ++wache < 700) {
            if (spur < 1 || spur > SPUREN) break;
            if (spur == SCHUTZSPUR) treffer++;
            const uint8_t *b = d + versatz(spur, sek);
            int ns = b[0], nsek = b[1];
            spur = ns; sek = nsek;
        }
    }
    return treffer;
}

int main(void)
{
    printf("Die Schutzspur wird frei, die Dateien ueberleben (MF-1333)\n");

    if (uft_cbm_sectors_per_track(UFT_CBM_1541, SCHUTZSPUR) != 19) {
        printf("  [ROT] Spur 21 hat nicht 19 Sektoren — Vorbedingung\n");
        return 1;
    }

    static uint8_t vorher_a[8 * 254], nachher_a[8 * 254];

    /* ── 1. Eine Datei, deren Kette MITTEN durch Spur 21 laeuft ────*/
    {
        diskette_formatieren(g_quelle);
        /* 20/0 -> 21/0 -> 21/1 -> 21/2 -> 22/0 : drei Bloecke auf der
         * Schutzspur, AUFEINANDERFOLGEND. Genau dieser Fall bricht
         * einen Umbau, der beim Laufen umschreibt. */
        const int stellen[] = { 20,0, 21,0, 21,1, 21,2, 22,0 };
        datei_anlegen(g_quelle, 0, stellen, 5);

        int n_vor = datei_lesen(g_quelle, 0, vorher_a, sizeof vorher_a);
        zusage(n_vor == 5, "Vorbedingung: die Datei hat 5 Bloecke");
        zusage(spur21_noch_benutzt(g_quelle) == 3,
               "Vorbedingung: drei Bloecke liegen auf Spur 21");

        memset(g_ziel, 0xCC, sizeof g_ziel);
        uft_geos_rebuild_bericht_t b;
        uft_error_t rc = uft_geos_rebuild(g_quelle, D64_LEN,
                                          g_ziel, D64_LEN, &b);

        zusage(rc == UFT_OK, "Umbau gelingt");
        zusage(b.uebernommen, "das Ziel wurde uebernommen");
        zusage(b.gefunden == 3 && b.verschoben == 3,
               "drei Bloecke gefunden, drei verschoben");
        zusage(b.ketten_gueltig && b.spur21_unreferenziert &&
               b.blockzahl_gleich && b.bam_stimmig,
               "alle vier Pruefungen bestanden");
        zusage(b.bloecke_vorher == b.bloecke_nachher,
               "die Blockzahl ist unveraendert");

        zusage(spur21_noch_benutzt(g_ziel) == 0,
               "unabhaengig nachgezaehlt: Spur 21 ist unbenutzt");

        int n_nach = datei_lesen(g_ziel, 0, nachher_a, sizeof nachher_a);
        zusage(n_nach == 5, "die Datei hat weiter 5 Bloecke");
        zusage(n_nach == n_vor &&
               memcmp(vorher_a, nachher_a, (size_t)n_vor * 254) == 0,
               "der Dateiinhalt ist BYTEIDENTISCH");

        zusage(spur21_noch_benutzt(g_quelle) == 3,
               "die Quelle ist unveraendert geblieben");
        zusage(bam_ist_frei(g_ziel, 21, 0) && bam_ist_frei(g_ziel, 21, 1) &&
               bam_ist_frei(g_ziel, 21, 2),
               "die alten Bloecke sind in der BAM wieder frei");
    }

    /* ── 2. Mehrere Dateien, beide betroffen ───────────────────────*/
    {
        diskette_formatieren(g_quelle);
        const int a[] = { 21,0, 21,1, 19,0 };
        const int c[] = { 21,5, 23,0, 21,6 };
        datei_anlegen(g_quelle, 0, a, 3);
        datei_anlegen(g_quelle, 1, c, 3);

        static uint8_t v0[8 * 254], v1[8 * 254], n0[8 * 254], n1[8 * 254];
        int nv0 = datei_lesen(g_quelle, 0, v0, sizeof v0);
        int nv1 = datei_lesen(g_quelle, 1, v1, sizeof v1);

        memset(g_ziel, 0xCC, sizeof g_ziel);
        uft_geos_rebuild_bericht_t b;
        uft_error_t rc = uft_geos_rebuild(g_quelle, D64_LEN,
                                          g_ziel, D64_LEN, &b);

        zusage(rc == UFT_OK && b.uebernommen, "zwei Dateien: Umbau gelingt");
        zusage(b.dateien == 2, "beide Dateien wurden verfolgt");
        zusage(b.gefunden == 4, "vier Bloecke auf Spur 21 gefunden");
        zusage(spur21_noch_benutzt(g_ziel) == 0,
               "zwei Dateien: Spur 21 ist unbenutzt");

        int nn0 = datei_lesen(g_ziel, 0, n0, sizeof n0);
        int nn1 = datei_lesen(g_ziel, 1, n1, sizeof n1);
        zusage(nn0 == nv0 && memcmp(v0, n0, (size_t)nv0 * 254) == 0,
               "Datei A: byteidentisch");
        zusage(nn1 == nv1 && memcmp(v1, n1, (size_t)nv1 * 254) == 0,
               "Datei B: byteidentisch");
    }

    /* ── 3. Der ERSTBLOCK liegt auf der Schutzspur ─────────────────
     * Dann muss auch der VERZEICHNISEINTRAG nachgezogen werden — eine
     * andere Stelle als die Blockverkettung. Genau diese Nachbarstelle
     * hat MF-519/MF-529 dreimal gekostet. */
    {
        diskette_formatieren(g_quelle);
        const int stellen[] = { 21,3, 24,0 };
        datei_anlegen(g_quelle, 0, stellen, 2);

        static uint8_t v[8 * 254], n[8 * 254];
        int nv = datei_lesen(g_quelle, 0, v, sizeof v);

        memset(g_ziel, 0xCC, sizeof g_ziel);
        uft_geos_rebuild_bericht_t b;
        uft_error_t rc = uft_geos_rebuild(g_quelle, D64_LEN,
                                          g_ziel, D64_LEN, &b);

        zusage(rc == UFT_OK && b.uebernommen, "Erstblock: Umbau gelingt");
        const uint8_t *e = g_ziel + versatz(VERZ_SPUR, 1);
        zusage(e[3] != SCHUTZSPUR,
               "der Verzeichniseintrag zeigt nicht mehr auf Spur 21");
        int nn = datei_lesen(g_ziel, 0, n, sizeof n);
        zusage(nn == nv && memcmp(v, n, (size_t)nv * 254) == 0,
               "Erstblock: Dateiinhalt byteidentisch");
    }

    /* ── 4. Nichts zu tun ist kein Fehler ──────────────────────────*/
    {
        diskette_formatieren(g_quelle);
        const int stellen[] = { 20,0, 22,0 };
        datei_anlegen(g_quelle, 0, stellen, 2);

        memset(g_ziel, 0xCC, sizeof g_ziel);
        uft_geos_rebuild_bericht_t b;
        uft_error_t rc = uft_geos_rebuild(g_quelle, D64_LEN,
                                          g_ziel, D64_LEN, &b);

        zusage(rc == UFT_OK && b.uebernommen && b.gefunden == 0,
               "keine Bloecke auf Spur 21: Erfolg mit 0 Verschiebungen");
        zusage(memcmp(g_quelle, g_ziel, D64_LEN) == 0,
               "und das Ziel ist byteidentisch mit der Quelle");
    }

    /* ── 5. Ein ZYKLUS wird abgesagt, Ziel bleibt unberuehrt ───────*/
    {
        diskette_formatieren(g_quelle);
        const int stellen[] = { 21,0, 21,1 };
        datei_anlegen(g_quelle, 0, stellen, 2);
        uint8_t *b2 = g_quelle + versatz(21, 1);
        b2[0] = 21; b2[1] = 0;            /* zurueck auf den ersten */

        memset(g_ziel, 0xCC, sizeof g_ziel);
        uft_geos_rebuild_bericht_t b;
        uft_error_t rc = uft_geos_rebuild(g_quelle, D64_LEN,
                                          g_ziel, D64_LEN, &b);

        zusage(rc == UFT_ERR_CORRUPTED, "Zyklus: abgesagt");
        zusage(!b.uebernommen, "Zyklus: nicht uebernommen");
        zusage(uft_geos_rebuild_grund(&b)[0] != 0,
               "Zyklus: ein Grund steht im Bericht");
        int unberuehrt = 1;
        for (size_t i = 0; i < D64_LEN; i++)
            if (g_ziel[i] != 0xCC) { unberuehrt = 0; break; }
        zusage(unberuehrt, "Zyklus: das Ziel ist UNBERUEHRT geblieben");
    }

    /* ── 6. Ein Verweis ins Leere wird abgesagt ────────────────────*/
    {
        diskette_formatieren(g_quelle);
        const int stellen[] = { 21,0, 20,0 };
        datei_anlegen(g_quelle, 0, stellen, 2);
        uint8_t *b1 = g_quelle + versatz(21, 0);
        b1[0] = 99;                       /* Spur 99 gibt es nicht */

        memset(g_ziel, 0xCC, sizeof g_ziel);
        uft_geos_rebuild_bericht_t b;
        uft_error_t rc = uft_geos_rebuild(g_quelle, D64_LEN,
                                          g_ziel, D64_LEN, &b);
        zusage(rc == UFT_ERR_CORRUPTED && !b.uebernommen,
               "Verweis ins Leere: abgesagt");
        int unberuehrt = 1;
        for (size_t i = 0; i < D64_LEN; i++)
            if (g_ziel[i] != 0xCC) { unberuehrt = 0; break; }
        zusage(unberuehrt, "Verweis ins Leere: Ziel unberuehrt");
    }

    /* ── 6b. Nur noch die Schutzspur ist frei ──────────────────────
     *
     * Dann darf NICHTS verschoben werden: ein Ziel auf Spur 21 waere
     * keine Raeumung, sondern ein Ringtausch. Die Absage muss kommen,
     * BEVOR irgendetwas umgeschrieben wird.
     *
     * NACHGETRAGEN nach der Mutationsmatrix: `freien_finden()` laeuft
     * die Spuren AUFSTEIGEND ab, und auf jeder bisherigen Pruefdiskette
     * ist laengst vor Spur 21 etwas frei — die Sperre war damit nie
     * erreicht, und eine Mutation, die sie entfernt, rutschte durch.
     * Klasse MF-1014: eine Gegenprobe, die aus dem falschen Grund
     * gruen ist. */
    {
        diskette_formatieren(g_quelle);
        const int stellen[] = { 21,0, 21,1 };
        datei_anlegen(g_quelle, 0, stellen, 2);

        /* Alles ausser der Schutzspur als belegt fuehren. */
        for (int spur = 1; spur <= SPUREN; spur++) {
            if (spur == SCHUTZSPUR) continue;
            int n = uft_cbm_sectors_per_track(UFT_CBM_1541, spur);
            for (int s = 0; s < n; s++) bam_belegen(g_quelle, spur, s);
        }

        memset(g_ziel, 0xCC, sizeof g_ziel);
        uft_geos_rebuild_bericht_t b;
        uft_error_t rc = uft_geos_rebuild(g_quelle, D64_LEN,
                                          g_ziel, D64_LEN, &b);

        zusage(rc == UFT_ERR_CORRUPTED && !b.uebernommen,
               "nur Spur 21 frei: abgesagt");
        zusage(b.gefunden == 2 && b.verschoben == 0,
               "nur Spur 21 frei: gefunden, aber NICHTS verschoben");
        zusage(strstr(uft_geos_rebuild_grund(&b), "freie") != NULL,
               "nur Spur 21 frei: der Grund nennt die fehlenden Bloecke");
        int unberuehrt = 1;
        for (size_t i = 0; i < D64_LEN; i++)
            if (g_ziel[i] != 0xCC) { unberuehrt = 0; break; }
        zusage(unberuehrt, "nur Spur 21 frei: Ziel unberuehrt");
    }

    /* ── 6c. Ein VERZEICHNISSEKTOR auf der Schutzspur ───────────────
     *
     * Die Verzeichniskette beginnt fest bei 18/1. Ein Verzeichnissektor
     * auf Spur 21 waere eine Diskette, fuer die es keine Vorlage gibt —
     * abgesagt statt geraten. Auch diese Zusage ist nach der Matrix
     * nachgetragen: der Fall kam im Test nicht vor. */
    {
        diskette_formatieren(g_quelle);
        const int stellen[] = { 20,0, 22,0 };
        datei_anlegen(g_quelle, 0, stellen, 2);

        /* Der erste Verzeichnissektor zeigt auf einen zweiten — auf 21/5. */
        uint8_t *v1 = g_quelle + versatz(VERZ_SPUR, 1);
        v1[0] = SCHUTZSPUR; v1[1] = 5;
        uint8_t *v2 = g_quelle + versatz(SCHUTZSPUR, 5);
        v2[0] = 0; v2[1] = 0xFF;
        bam_belegen(g_quelle, SCHUTZSPUR, 5);

        memset(g_ziel, 0xCC, sizeof g_ziel);
        uft_geos_rebuild_bericht_t b;
        uft_error_t rc = uft_geos_rebuild(g_quelle, D64_LEN,
                                          g_ziel, D64_LEN, &b);

        zusage(rc == UFT_ERR_CORRUPTED && !b.uebernommen,
               "Verzeichnissektor auf Spur 21: abgesagt");
        int unberuehrt = 1;
        for (size_t i = 0; i < D64_LEN; i++)
            if (g_ziel[i] != 0xCC) { unberuehrt = 0; break; }
        zusage(unberuehrt,
               "Verzeichnissektor auf Spur 21: Ziel unberuehrt");
    }

    /* ── 7. Die Eingangspruefungen ─────────────────────────────────*/
    {
        uft_geos_rebuild_bericht_t b;
        zusage(uft_geos_rebuild(NULL, D64_LEN, g_ziel, D64_LEN, &b)
               == UFT_ERR_INVALID_ARG, "Nullzeiger: abgesagt");
        zusage(uft_geos_rebuild(g_quelle, 12345, g_ziel, D64_LEN, &b)
               == UFT_ERR_INVALID_ARG, "falsche Groesse: abgesagt");
        zusage(uft_geos_rebuild(g_quelle, D64_LEN, g_ziel, 100, &b)
               == UFT_ERR_INVALID_ARG, "Ziel zu klein: abgesagt");
        zusage(uft_geos_rebuild(g_quelle, D64_LEN, g_quelle, D64_LEN, &b)
               == UFT_ERR_INVALID_ARG,
               "Quelle gleich Ziel: abgesagt (keine stille Veraenderung)");
        zusage(uft_geos_rebuild_grund(&b)[0] != 0,
               "und jede Absage nennt ihren Grund");
    }

    printf("%s\n", fehler ? "FEHLGESCHLAGEN" : "BESTANDEN (0 Fehler)");
    return fehler ? 1 : 0;
}

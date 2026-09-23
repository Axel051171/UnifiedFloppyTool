/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_geos_rebuild.c
 * @brief Raeumt die GEOS-Schutzspur frei (MF-1333, Stufe 4).
 *
 * Verfahren, Referenz und Grenzen stehen im Header. Hier nur, was die
 * UMSETZUNG entscheidet:
 *
 * 1. ZWEI DURCHGAENGE, nicht einer. Erst wird die ganze Abbildung
 *    alt->neu gebaut, dann werden ALLE Verweise in einem Rutsch
 *    umgeschrieben. Ein Durchgang, der beim Laufen umschreibt, stolpert
 *    ueber sich selbst, sobald zwei zu verschiebende Bloecke
 *    aufeinanderfolgen — und genau das ist auf einer Schutzspur der
 *    Normalfall, weil dort ein ganzes Kettenstueck liegt.
 *
 * 2. DIE ZONENTAFEL WIRD GERUFEN, NICHT NACHGEBAUT.
 *    `uft_cbm_sectors_per_track()` ist die eine Rechnung (MF-1177);
 *    der Baum hat davon bereits drei Kopien gehabt.
 *
 * 3. JEDE KETTE MIT ZYKLUSSCHUTZ. Ein besuchter Block wird vermerkt;
 *    ein zweites Betreten bricht ab. Ohne das laeuft eine verfaelschte
 *    Diskette endlos — und eine verfaelschte Diskette ist bei
 *    Kopierschutz der Regelfall, nicht die Ausnahme.
 *
 * 4. DIE PRUEFUNG LAEUFT NACH DEM UMBAU, auf dem Ergebnis. Eine
 *    Pruefung der Absicht vor dem Umbau sagt nichts darueber, was
 *    wirklich im Puffer steht.
 */

#include "uft/protection/uft_geos_rebuild.h"
#include "uft/formats/cbm/uft_cbm_geometry.h"

#include <stdio.h>
#include <string.h>

#define GEOS_BLOCK_LEN   256
#define SPUREN          35
#define VERZ_SPUR       18
#define VERZ_SEKTOR      1
/* Ein Verzeichnissektor traegt acht Eintraege zu 32 Byte. */
#define VERZ_EINTRAEGE   8
#define VERZ_EINTRAG_LEN 32
/* Im Eintrag: Typ bei +2, Erstblock bei +3/+4. */
#define EINTRAG_TYP      2
#define EINTRAG_SPUR     3
#define EINTRAG_SEKTOR   4

/* Obergrenze fuer jeden Kettenlauf: mehr Bloecke als die Diskette hat,
 * kann keine Kette haben. 35 Spuren zu hoechstens 21 Sektoren. */
#define MAX_KETTE        (SPUREN * 21)

/* ── Blockadressen ───────────────────────────────────────────────────*/

/** Byteversatz eines Blocks, oder (size_t)-1 wenn es ihn nicht gibt. */
static size_t versatz(int spur, int sektor)
{
    if (spur < 1 || spur > SPUREN || sektor < 0) return (size_t)-1;
    int n = uft_cbm_sectors_per_track(UFT_CBM_1541, spur);
    if (n <= 0 || sektor >= n) return (size_t)-1;

    size_t bloecke = 0;
    for (int t = 1; t < spur; t++)
        bloecke += (size_t)uft_cbm_sectors_per_track(UFT_CBM_1541, t);
    return (bloecke + (size_t)sektor) * GEOS_BLOCK_LEN;
}

/** Fortlaufende Blocknummer, fuer die Besuchskarte. */
static int blocknummer(int spur, int sektor)
{
    size_t v = versatz(spur, sektor);
    return v == (size_t)-1 ? -1 : (int)(v / GEOS_BLOCK_LEN);
}

/* ── BAM ─────────────────────────────────────────────────────────────*/

/* Der BAM-Eintrag einer Spur: vier Byte ab `4 + (spur-1)*4`. Erstes
 * Byte ist die Zahl freier Sektoren, danach drei Byte Bitkarte, Bit
 * gesetzt = FREI (GEOS.TXT:220). */
#define BAM_EINTRAG(spur) (4 + ((spur) - 1) * 4)

static bool bam_frei(const uint8_t *d64, int spur, int sektor)
{
    size_t bam = versatz(VERZ_SPUR, 0);
    if (bam == (size_t)-1) return false;
    const uint8_t *e = d64 + bam + BAM_EINTRAG(spur);
    return ((e[1 + (sektor >> 3)] >> (sektor & 7)) & 1) != 0;
}

static void bam_setzen(uint8_t *d64, int spur, int sektor, bool frei)
{
    size_t bam = versatz(VERZ_SPUR, 0);
    if (bam == (size_t)-1) return;
    uint8_t *e = d64 + bam + BAM_EINTRAG(spur);
    uint8_t maske = (uint8_t)(1u << (sektor & 7));
    bool war = (e[1 + (sektor >> 3)] & maske) != 0;
    if (war == frei) return;                 /* steht schon so */

    if (frei) { e[1 + (sektor >> 3)] |= maske;             e[0]++; }
    else      { e[1 + (sektor >> 3)] &= (uint8_t)~maske;   e[0]--; }
}

/* ── Kettenlauf ──────────────────────────────────────────────────────*/

typedef struct {
    int alt_spur, alt_sektor;
    int neu_spur, neu_sektor;
} verschiebung_t;

typedef struct {
    uint8_t       *bild;
    uint8_t        besucht[SPUREN * 21];
    verschiebung_t plan[UFT_GEOS_MAX_VERSCHIEBUNG];
    unsigned       plan_n;
    unsigned       bloecke;
    unsigned       dateien;
} lauf_t;

/** Sucht in der Abbildung. -1, wenn der Block nicht verschoben wird. */
static int plan_finden(const lauf_t *l, int spur, int sektor)
{
    for (unsigned i = 0; i < l->plan_n; i++)
        if (l->plan[i].alt_spur == spur && l->plan[i].alt_sektor == sektor)
            return (int)i;
    return -1;
}

/**
 * Laeuft eine Blockkette ab.
 *
 * @param sammeln true = Bloecke auf der Schutzspur in den Plan
 *                aufnehmen; false = nur zaehlen und pruefen.
 * @return Zahl der Bloecke, oder -1 bei Zyklus / ungueltigem Verweis.
 */
static int kette_laufen(lauf_t *l, int spur, int sektor, bool sammeln)
{
    int n = 0;
    while (spur != 0) {
        size_t v = versatz(spur, sektor);
        if (v == (size_t)-1) return -1;          /* Verweis ins Leere */

        int nr = blocknummer(spur, sektor);
        if (nr < 0 || l->besucht[nr]) return -1; /* Zyklus */
        l->besucht[nr] = 1;

        if (++n > MAX_KETTE) return -1;

        if (sammeln && spur == UFT_GEOS_SCHUTZSPUR) {
            if (l->plan_n >= UFT_GEOS_MAX_VERSCHIEBUNG) return -1;
            l->plan[l->plan_n].alt_spur   = spur;
            l->plan[l->plan_n].alt_sektor = sektor;
            l->plan[l->plan_n].neu_spur   = 0;
            l->plan[l->plan_n].neu_sektor = 0;
            l->plan_n++;
        }

        int ns   = l->bild[v];
        int nsek = l->bild[v + 1];
        spur   = ns;
        sektor = nsek;
    }
    return n;
}

/** Laeuft alle Dateiketten des Verzeichnisses ab. */
static bool alle_ketten(lauf_t *l, bool sammeln, unsigned *bloecke_aus)
{
    memset(l->besucht, 0, sizeof l->besucht);
    l->bloecke = 0;
    l->dateien = 0;

    /* Die Verzeichniskette selbst ist auch eine Kette. */
    int vspur = VERZ_SPUR, vsek = VERZ_SEKTOR;
    int wache = 0;

    while (vspur != 0) {
        size_t v = versatz(vspur, vsek);
        if (v == (size_t)-1) return false;
        int nr = blocknummer(vspur, vsek);
        if (nr < 0 || l->besucht[nr]) return false;
        l->besucht[nr] = 1;
        if (++wache > MAX_KETTE) return false;
        l->bloecke++;

        if (vspur == UFT_GEOS_SCHUTZSPUR) {
            /* Ein Verzeichnissektor auf der Schutzspur waere eine
             * Diskette, die dieses Verfahren nicht hergibt: die
             * Verzeichniskette beginnt fest bei 18/1. Abgesagt statt
             * geraten. */
            return false;
        }

        for (int i = 0; i < VERZ_EINTRAEGE; i++) {
            const uint8_t *e = l->bild + v + (size_t)i * VERZ_EINTRAG_LEN;
            if (e[EINTRAG_TYP] == 0) continue;       /* freier Eintrag */
            int fs = e[EINTRAG_SPUR], fsek = e[EINTRAG_SEKTOR];
            if (fs == 0) continue;                   /* kein Erstblock */

            int n = kette_laufen(l, fs, fsek, sammeln);
            if (n < 0) return false;
            l->bloecke += (unsigned)n;
            l->dateien++;
        }

        int ns = l->bild[v], nsek = l->bild[v + 1];
        vspur = ns;
        vsek  = nsek;
    }

    if (bloecke_aus) *bloecke_aus = l->bloecke;
    return true;
}

/* ── Ziel suchen ─────────────────────────────────────────────────────*/

/** Erster freier Block, der NICHT auf der Schutzspur und nicht auf der
 *  Verzeichnisspur liegt und in diesem Lauf noch nicht vergeben wurde. */
static bool freien_finden(lauf_t *l, int *spur_aus, int *sektor_aus)
{
    for (int spur = 1; spur <= SPUREN; spur++) {
        if (spur == UFT_GEOS_SCHUTZSPUR || spur == VERZ_SPUR) continue;
        int n = uft_cbm_sectors_per_track(UFT_CBM_1541, spur);
        for (int s = 0; s < n; s++) {
            if (!bam_frei(l->bild, spur, s)) continue;
            bool vergeben = false;
            for (unsigned i = 0; i < l->plan_n; i++)
                if (l->plan[i].neu_spur == spur &&
                    l->plan[i].neu_sektor == s) { vergeben = true; break; }
            if (vergeben) continue;
            *spur_aus   = spur;
            *sektor_aus = s;
            return true;
        }
    }
    return false;
}

/* ── oeffentlich ─────────────────────────────────────────────────────*/

const char *uft_geos_rebuild_grund(const uft_geos_rebuild_bericht_t *b)
{
    return (b && b->grund[0]) ? b->grund : "";
}

static uft_error_t absage(uft_geos_rebuild_bericht_t *b,
                          uft_error_t rc, const char *grund)
{
    if (b) {
        snprintf(b->grund, sizeof b->grund, "%s", grund);
        b->uebernommen = false;
    }
    return rc;
}

uft_error_t uft_geos_rebuild(const uint8_t *d64, size_t groesse,
                             uint8_t *aus, size_t aus_groesse,
                             uft_geos_rebuild_bericht_t *bericht)
{
    if (!bericht) return UFT_ERR_INVALID_ARG;
    memset(bericht, 0, sizeof *bericht);

    if (!d64 || !aus)
        return absage(bericht, UFT_ERR_INVALID_ARG, "Nullzeiger");
    if (groesse != UFT_GEOS_D64_GROESSE)
        return absage(bericht, UFT_ERR_INVALID_ARG,
                      "nur 35-Spur-D64 ohne Fehlerkarte (174848 Byte)");
    if (aus_groesse < groesse)
        return absage(bericht, UFT_ERR_INVALID_ARG, "Ziel zu klein");
    if (d64 == aus)
        return absage(bericht, UFT_ERR_INVALID_ARG,
                      "Quelle und Ziel duerfen nicht dasselbe sein");

    /* Die Arbeitskopie. Das Ziel bleibt unberuehrt, bis alles steht. */
    static uint8_t arbeit[UFT_GEOS_D64_GROESSE];
    memcpy(arbeit, d64, groesse);

    static lauf_t l;
    memset(&l, 0, sizeof l);
    l.bild = arbeit;

    /* ── 1. Bestandsaufnahme: Ketten laufen, Schutzspur sammeln ────*/
    unsigned vorher = 0;
    if (!alle_ketten(&l, true, &vorher))
        return absage(bericht, UFT_ERR_CORRUPTED,
                      "Blockkette ungueltig: Zyklus, Verweis ins Leere "
                      "oder Verzeichnissektor auf der Schutzspur");

    bericht->bloecke_vorher = vorher;
    bericht->dateien = l.dateien;
    bericht->gefunden = l.plan_n;

    if (l.plan_n == 0) {
        /* Nichts zu tun ist kein Fehler — aber auch kein Umbau. */
        memcpy(aus, arbeit, groesse);
        bericht->ketten_gueltig = true;
        bericht->spur21_unreferenziert = true;
        bericht->blockzahl_gleich = true;
        bericht->bam_stimmig = true;
        bericht->bloecke_nachher = vorher;
        bericht->uebernommen = true;
        return UFT_OK;
    }

    /* ── 2. Ziele vergeben — VOR jedem Umschreiben ──────────────────*/
    for (unsigned i = 0; i < l.plan_n; i++) {
        int ns, nsek;
        if (!freien_finden(&l, &ns, &nsek))
            return absage(bericht, UFT_ERR_CORRUPTED,
                          "zu wenig freie Bloecke ausserhalb der "
                          "Schutzspur");
        l.plan[i].neu_spur   = ns;
        l.plan[i].neu_sektor = nsek;
    }

    /* ── 3. Bloecke kopieren ────────────────────────────────────────*/
    for (unsigned i = 0; i < l.plan_n; i++) {
        size_t alt = versatz(l.plan[i].alt_spur, l.plan[i].alt_sektor);
        size_t neu = versatz(l.plan[i].neu_spur, l.plan[i].neu_sektor);
        if (alt == (size_t)-1 || neu == (size_t)-1)
            return absage(bericht, UFT_ERR_CORRUPTED,
                          "Blockadresse ausserhalb der Diskette");
        memcpy(arbeit + neu, arbeit + alt, GEOS_BLOCK_LEN);
    }

    /* ── 4. ALLE Verweise nachziehen, in EINEM Durchgang ────────────
     *
     * Erst jetzt, mit vollstaendiger Abbildung. Wer beim Laufen
     * umschreibt, stolpert ueber zwei aufeinanderfolgende verschobene
     * Bloecke — auf einer Schutzspur der Normalfall. */
    for (int spur = 1; spur <= SPUREN; spur++) {
        int n = uft_cbm_sectors_per_track(UFT_CBM_1541, spur);
        for (int s = 0; s < n; s++) {
            size_t v = versatz(spur, s);
            if (v == (size_t)-1) continue;
            int k = plan_finden(&l, arbeit[v], arbeit[v + 1]);
            if (k >= 0) {
                arbeit[v]     = (uint8_t)l.plan[k].neu_spur;
                arbeit[v + 1] = (uint8_t)l.plan[k].neu_sektor;
            }
        }
    }

    /* Und die Verzeichniseintraege — sie zeigen auf ERSTBLOECKE und
     * sind keine Kettenglieder. Genau diese Nachbarstelle hat
     * MF-519/MF-529 dreimal gekostet. */
    {
        int vspur = VERZ_SPUR, vsek = VERZ_SEKTOR, wache = 0;
        while (vspur != 0 && ++wache <= MAX_KETTE) {
            size_t v = versatz(vspur, vsek);
            if (v == (size_t)-1) break;
            for (int i = 0; i < VERZ_EINTRAEGE; i++) {
                uint8_t *e = arbeit + v + (size_t)i * VERZ_EINTRAG_LEN;
                if (e[EINTRAG_TYP] == 0) continue;
                int k = plan_finden(&l, e[EINTRAG_SPUR], e[EINTRAG_SEKTOR]);
                if (k >= 0) {
                    e[EINTRAG_SPUR]   = (uint8_t)l.plan[k].neu_spur;
                    e[EINTRAG_SEKTOR] = (uint8_t)l.plan[k].neu_sektor;
                }
            }
            int ns = arbeit[v], nsek = arbeit[v + 1];
            vspur = ns; vsek = nsek;
        }
    }

    /* ── 5. BAM in denselben Stand bringen ──────────────────────────*/
    for (unsigned i = 0; i < l.plan_n; i++) {
        bam_setzen(arbeit, l.plan[i].neu_spur, l.plan[i].neu_sektor, false);
        bam_setzen(arbeit, l.plan[i].alt_spur, l.plan[i].alt_sektor, true);
    }
    bericht->verschoben = l.plan_n;

    /* ── 6. Pruefen — auf dem ERGEBNIS, nicht auf der Absicht ───────*/
    static lauf_t p;
    memset(&p, 0, sizeof p);
    p.bild = arbeit;

    unsigned nachher = 0;
    bericht->ketten_gueltig = alle_ketten(&p, true, &nachher);
    bericht->bloecke_nachher = nachher;
    bericht->spur21_unreferenziert = (p.plan_n == 0);
    bericht->blockzahl_gleich = (nachher == vorher);

    /* Die BAM muss sagen, was wirklich belegt ist — fuer JEDEN Block
     * der Diskette, nicht nur fuer die verschobenen. */
    bool stimmig = true;
    for (int spur = 1; spur <= SPUREN && stimmig; spur++) {
        int n = uft_cbm_sectors_per_track(UFT_CBM_1541, spur);
        for (int s = 0; s < n; s++) {
            int nr = blocknummer(spur, s);
            if (nr < 0) continue;
            if (p.besucht[nr] && bam_frei(arbeit, spur, s)) {
                stimmig = false;      /* benutzt, aber als frei gefuehrt */
                break;
            }
        }
    }
    bericht->bam_stimmig = stimmig;

    if (!bericht->ketten_gueltig)
        return absage(bericht, UFT_ERR_CORRUPTED,
                      "nach dem Umbau ist eine Blockkette ungueltig");
    if (!bericht->spur21_unreferenziert)
        return absage(bericht, UFT_ERR_CORRUPTED,
                      "nach dem Umbau zeigt noch ein Verweis auf Spur 21");
    if (!bericht->blockzahl_gleich)
        return absage(bericht, UFT_ERR_CORRUPTED,
                      "nach dem Umbau stimmt die Blockzahl nicht mehr");
    if (!bericht->bam_stimmig)
        return absage(bericht, UFT_ERR_CORRUPTED,
                      "nach dem Umbau fuehrt die BAM einen benutzten "
                      "Block als frei");

    /* ── 7. Erst jetzt uebernehmen ──────────────────────────────────*/
    memcpy(aus, arbeit, groesse);
    bericht->uebernommen = true;
    return UFT_OK;
}

/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * test_convert_img_imd_erfindet_nicht.c — IMG->IMD darf keine Sektoren
 * erfinden, und seine Geometrie muss die Datei erklaeren (MF-1332).
 *
 * KEIN ROTBEWEIS — und das ist der Befund. Dieser Test ist als
 * Rotbeweis fuer eine Erfindung begonnen worden und hat beim ersten Lauf
 * etwas anderes gemessen: der Wandler ist ueber den oeffentlichen Weg
 * gar nicht erreichbar.
 *
 * GEMESSEN ueber ALLE 17 IMG-Dateien des freien Korpus:
 *
 *   Wandler erreicht :  0
 *   abgesagt         : 16
 *   uebersprungen    :  1   (gw_pdp.img, 256 256 Byte, kein Vielfaches
 *                            von 512)
 *
 * Die 16 Absagen zerfallen in ZWEI Gruppen, und die erste ist der
 * eigentliche Befund:
 *
 *   9x  rc = -40  „No conversion path from DSK to IMD"
 *   1x  rc = -40  „... from Unknown to IMD"   (uft_pc160.img)
 *   1x  rc = -40  „... from XFD to IMD"       (uft_ti_dssd.img)
 *   5x  rc = -25  „Source format is ambiguous: N plugins claim this
 *                  data at confidence 35/40/45; 'X' wins by
 *                  registration order, not by evidence"
 *
 * ERSTE GRUPPE — die Sonde liefert fuer eine schlichte `.img` das
 * Format **DSK**, waehrend der Verteilerzweig
 * `uft_format_convert_dispatch.c:201` auf `src_format == UFT_FORMAT_IMG`
 * fragt. Die Bedingung trifft also nie zu, was die Sonde erzeugt.
 * `uftc_convert_img_to_imd()` hat genau EINEN Aufrufer, und er wird
 * ueber `uft_convert_file()` nicht erreicht.
 *
 * ZWEITE GRUPPE — die Sonden-Doktrin arbeitet wie vorgesehen: kopflose
 * Formate kommen ueber 45 nicht hinaus, bei Gleichstand gewinnt keiner,
 * und die Meldung sagt das woertlich. Diese Absagen sind RICHTIG.
 *
 * ACHTUNG, EIGENER FEHLER (festgehalten, weil er teuer war): der erste
 * Lauf dieses Tests meldete stattdessen 16x „Could not detect source
 * format". Ursache war nicht das Produkt, sondern der fehlende Aufruf
 * von `uft_register_all_formats()` — nach MF-446/447 fuellt erst
 * `main()` die Registry, und ein Testbinary hat sie sonst LEER. Klasse
 * MF-1125: Werkzeugfehler, nicht Pruefling.
 *
 * DER DEFEKT IM WANDLER IST TROTZDEM ECHT, nur unerreichbar — Klasse
 * MF-635/MF-930/P3-204, „Bestand, nicht Faehigkeit". Statisch gelesen
 * und nachgerechnet, ohne ihn ausfuehren zu koennen:
 *
 *   Quelle  tests/corpus_free/gw_sam.img    819 200 Byte = 1600 x 512
 *   gewaehlt  80 x 2 x 15 x 512           = 1 228 800 Byte
 *   geschrieben                              2400 Sektorsaetze
 *   davon jenseits der Quelle                 800, als GUELTIG gemeldet
 *
 * WAS DIESER TEST ALSO IST: ein Waechter an der Tuer, nicht ein Beweis
 * im Raum. Er wird rot, sobald (a) eine Absage keinen Grund mehr nennt
 * oder (b) eine Datei den Wandler DOCH erreicht und dabei Sektoren
 * erfindet. Solange „Wandler erreicht: 0" in seiner Ausgabe steht, ist
 * die Erfindungs-Zusage UNGEPRUEFT und er sagt das selbst.
 *
 * `uftc_convert_img_to_imd()` (src/formats/uft_format_convert_sector.c)
 * waehlt die Geometrie ueber `<=`-Bereiche aus der Dateigroesse. 819 200
 * faellt in den Zweig `src_size <= 1228800` und wird damit als
 * 80 x 2 x 15 gelesen. Die Datei ist aber 80 x 2 x 10 (Sam Coupe), also
 * stimmt ab dem elften Sektorsatz jeder Spur weder die Spur- noch die
 * Sektornummer, und die fehlenden 800 Saetze werden mit
 *
 *     imd_data[pos++] = UFT_IMD_SEC_COMPRESSED;   // = gueltiger Sektor
 *     imd_data[pos++] = 0xE5;
 *
 * gefuellt und in `result->sectors_converted` mitgezaehlt.
 *
 * Das verletzt zwei Kernprinzipien zugleich: „Keine erfundenen Daten"
 * (ein nicht vorhandener Sektor wird als gelesener gemeldet) und „Keine
 * stille Veraenderung" (der Bericht sagt Erfolg). IMD hat fuer den Fall
 * einen eigenen Kode — `UFT_IMD_SEC_UNAVAIL` (0x00), im Baum seit
 * MF-1287 in der Gegenrichtung benutzt.
 *
 * REFERENZ fuer die IMD-Satzform: `include/uft/formats/uft_imd.h`
 * (`uft_imd_sectype_t`, neun Werte) und ImageDisk 1.18 von Dave
 * Dunfield, dessen Beschreibung dem Format beiliegt — hier nur GELESEN,
 * Kanal *Spec* nach MF-695. Die Sam-Coupe-Geometrie 80 x 2 x 10 x 512
 * ist im Baum unabhaengig belegt: MAMEs `mgt_format` (BSD-3-Clause, nur
 * gelesen), festgenagelt in `tests/test_mgt_gegen_mame.c` seit MF-1006.
 *
 * WAS DIESER TEST NICHT PRUEFT: ob die gewaehlte Geometrie die
 * *richtige* ist. Er prueft nur, dass sie die Dateigroesse restlos
 * erklaert und dass kein Sektor erfunden wird. Eine Datei, deren Groesse
 * zu mehreren Geometrien passt, bleibt mehrdeutig — das ist die Lage aus
 * MF-1039 (`cpm`) und wird hier nicht geloest.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_core.h"
#include "uft/uft_format_convert.h"
#include "uft/uft_format_plugin.h"
#include "uft/formats/uft_imd.h"

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

#define QUELLE UFT_CORPUS_DIR "/gw_sam.img"

/* Gemessen: die Quelle ist 819 200 Byte. Steht hier ein zweites Mal,
 * damit der Test nicht dieselbe Zahl befragt wie der Pruefling
 * (Klasse MF-1000). */
#define QUELLE_BYTE     819200u
#define SEKTORGROESSE   512u
#define QUELLE_SEKTOREN (QUELLE_BYTE / SEKTORGROESSE)   /* 1600 */

static int fehler = 0;

static void zusage(int ok, const char *was)
{
    printf("  %-62s %s\n", was, ok ? "ok" : "[ROT]");
    if (!ok) fehler++;
}

static uint8_t *lies(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long g = ftell(f);
    if (g <= 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    uint8_t *p = (uint8_t *)malloc((size_t)g);
    if (!p) { fclose(f); return NULL; }
    if (fread(p, 1, (size_t)g, f) != (size_t)g) {
        free(p); fclose(f); return NULL;
    }
    fclose(f);
    *n = (size_t)g;
    return p;
}

/* Ergebnis eines IMD-Durchlaufs. */
typedef struct {
    int    heil;             /* Datei vollstaendig zerlegbar           */
    size_t spuren;
    size_t saetze_gesamt;    /* alle Sektorsaetze                      */
    size_t saetze_mit_daten; /* stype != UNAVAIL                       */
    size_t saetze_unavail;
    size_t kapazitaet;       /* Summe nsectors * sektorgroesse          */
    int    fremde_groesse;   /* ein Satz mit anderer Sektorgroesse      */
} imd_sicht_t;

/* Minimaler IMD-Laeufer. Bewusst eigenstaendig: er darf NICHT denselben
 * Leser benutzen wie der Prueflingspfad, sonst befragt der Test seine
 * eigene Quelle (Klasse MF-1000/Tor 64). */
static imd_sicht_t imd_durchlauf(const uint8_t *d, size_t n)
{
    imd_sicht_t s;
    memset(&s, 0, sizeof s);

    size_t p = 0;
    while (p < n && d[p] != 0x1A) p++;   /* ASCII-Kopf */
    if (p >= n) return s;
    p++;                                 /* 0x1A ueberspringen */

    while (p + 5 <= n) {
        /* mode, cyl, head, nsectors, ssize_code */
        uint8_t head  = d[p + 2];
        uint8_t nsec  = d[p + 3];
        uint8_t scode = d[p + 4];
        p += 5;

        if (scode > 6) return s;              /* 128<<6 = 8192 ist das Maximum */
        size_t ssize = (size_t)128u << scode;
        if (ssize != SEKTORGROESSE) s.fremde_groesse = 1;

        if (p + nsec > n) return s;
        p += nsec;                            /* Nummernkarte */

        /* Optionale Zylinder-/Kopfkarten, Bit 7/6 des Kopfbytes. */
        if (head & 0x80) { if (p + nsec > n) return s; p += nsec; }
        if (head & 0x40) { if (p + nsec > n) return s; p += nsec; }

        for (uint8_t k = 0; k < nsec; k++) {
            if (p >= n) return s;
            uint8_t stype = d[p++];
            s.saetze_gesamt++;
            if (stype == UFT_IMD_SEC_UNAVAIL) {
                s.saetze_unavail++;
                continue;                     /* traegt keine Nutzlast */
            }
            s.saetze_mit_daten++;
            if (stype > UFT_IMD_SEC_DEL_ERR_COMP) return s;
            if (uft_imd_sec_is_compressed(stype)) {
                if (p + 1 > n) return s;
                p += 1;
            } else {
                if (p + ssize > n) return s;
                p += ssize;
            }
        }
        s.spuren++;
        s.kapazitaet += (size_t)nsec * ssize;
    }

    s.heil = (p == n);
    return s;
}

/* Alle IMG-Dateien des freien Korpus. Der Test laeuft ueber JEDE, weil
 * die Frage „erreicht ueberhaupt eine den Wandler?" sich nur so
 * beantworten laesst — an einer einzigen Datei haette der Test gruen
 * gemeldet, ohne den Wandler je betreten zu haben (Klasse MF-1000). */
static const char *const KORPUS_IMG[] = {
    "floptool_victor9k_dsdd.img",
    "fluxfox_sector_test_360k.img",
    "gw_img.img",
    "gw_jvc.img",
    "gw_micropolis.img",
    "gw_msx_2dd.img",
    "gw_northstar.img",
    "gw_pdp.img",
    "gw_po.img",
    "gw_sam.img",
    "gw_ssd.img",
    "gw_t1k.img",
    "gw_trd.img",
    "mtools_fat12_720k.img",
    "uft_720k.img",
    "uft_pc160.img",
    "uft_ti_dssd.img",
};
#define KORPUS_N (sizeof KORPUS_IMG / sizeof KORPUS_IMG[0])

int main(void)
{
    printf("IMG->IMD erfindet keine Sektoren (MF-1332)\n");

    /* MF-446/447: erst `main()` fuellt die Registry. Ohne diesen Aufruf
     * ist sie zur Laufzeit LEER und jede Wandlung sagt mit „Could not
     * detect source format" ab — der erste Lauf dieses Tests hat genau
     * das gemessen und haette den Werkzeugfehler beinahe als
     * Produktbefund gemeldet (Klasse MF-1125). */
    if (uft_register_all_formats() != UFT_OK) {
        printf("  UEBERSPRUNGEN: Formatregistrierung fehlgeschlagen\n");
        return 77;
    }

    printf("  %zu Korpus-Dateien; je Datei: Absage MIT Grund ODER "
           "Wandlung OHNE Erfindung\n\n", KORPUS_N);

    size_t erreicht = 0, abgesagt = 0, uebersprungen = 0;

    for (size_t i = 0; i < KORPUS_N; i++) {
        char quelle[512];
        snprintf(quelle, sizeof quelle, "%s/%s", UFT_CORPUS_DIR,
                 KORPUS_IMG[i]);

        size_t   qn = 0;
        uint8_t *q  = lies(quelle, &qn);
        if (!q || qn == 0 || (qn % SEKTORGROESSE) != 0) {
            printf("  %-32s UEBERSPRUNGEN (nicht lesbar oder kein "
                   "Vielfaches von 512)\n", KORPUS_IMG[i]);
            uebersprungen++;
            free(q);
            continue;
        }
        const size_t quell_sektoren = qn / SEKTORGROESSE;

        const char *ziel = "uft_img_imd_erfindet_nicht_out.imd";
        remove(ziel);

        uft_convert_options_t opt = uft_convert_default_options();
        uft_convert_result_t  res;
        memset(&res, 0, sizeof res);

        /* `accept_data_loss` wie im Schwestertest: das Preflight-Tor
         * weist ein Paar ohne Matrix-Eintrag sonst als UNGEPRUEFT ab. */
        opt.accept_data_loss = true;

        const uft_error_t rc = uft_convert_file(quelle, ziel,
                                                UFT_FORMAT_IMD, &opt, &res);

        if (rc != UFT_OK) {
            /* Eine Absage ist erlaubt — aber sie muss einen Grund
             * nennen. Eine stumme Absage waere eine stille Veraenderung
             * der Erwartung des Bedieners. Diese Zusage KANN rot werden. */
            const int mit_grund = (res.warning_count > 0);
            printf("  %-32s ABSAGE rc=%-4d %s\n", KORPUS_IMG[i], (int)rc,
                   mit_grund ? res.warnings[0] : "(KEIN GRUND GENANNT)");
            char was[160];
            snprintf(was, sizeof was, "%s: Absage nennt einen Grund",
                     KORPUS_IMG[i]);
            zusage(mit_grund, was);
            abgesagt++;
            remove(ziel);
            free(q);
            continue;
        }

        erreicht++;

        size_t   an = 0;
        uint8_t *a  = lies(ziel, &an);
        if (!a) {
            char was[160];
            snprintf(was, sizeof was,
                     "%s: Erfolg gemeldet, Datei vorhanden", KORPUS_IMG[i]);
            zusage(0, was);
            free(q);
            continue;
        }

        imd_sicht_t s = imd_durchlauf(a, an);

        printf("  %-32s GEWANDELT %zu Byte, %zu Spuren, Saetze %zu "
               "(mit Daten %zu, UNAVAIL %zu), Kapazitaet %zu, Quelle %zu\n",
               KORPUS_IMG[i], an, s.spuren, s.saetze_gesamt,
               s.saetze_mit_daten, s.saetze_unavail, s.kapazitaet, qn);

        char was[160];

        snprintf(was, sizeof was, "%s: Ausgabe vollstaendig zerlegbar",
                 KORPUS_IMG[i]);
        zusage(s.heil, was);

        /* KERNZUSAGE. Ein Sektorsatz mit Daten behauptet, gelesen worden
         * zu sein. Mehr davon als die Quelle Sektoren hat, heisst
         * erfunden. */
        snprintf(was, sizeof was, "%s: kein Sektor jenseits der Quelle",
                 KORPUS_IMG[i]);
        zusage(s.saetze_mit_daten <= quell_sektoren, was);

        /* Die angesagte Geometrie muss die Datei restlos erklaeren —
         * sonst tragen Spur- und Sektornummern eine falsche Aussage, und
         * das sieht eine Groessenpruefung nicht (Klasse MF-1026). */
        snprintf(was, sizeof was, "%s: Geometrie erklaert die Quellgroesse",
                 KORPUS_IMG[i]);
        zusage(s.kapazitaet == qn, was);

        snprintf(was, sizeof was, "%s: Bericht uebersteigt die Quelle nicht",
                 KORPUS_IMG[i]);
        zusage(res.sectors_converted <= quell_sektoren, was);

        free(a);
        remove(ziel);
        free(q);
    }

    printf("\n  Wandler erreicht: %zu   abgesagt: %zu   uebersprungen: %zu\n",
           erreicht, abgesagt, uebersprungen);

    /* Wenn KEINE Datei den Wandler erreicht, misst dieser Test den
     * Wandler nicht — und sagt das, statt gruen zu melden. Das ist die
     * Lehre aus MF-1000/Tor 64: ein Test, der nicht rot werden kann,
     * bewacht nichts. */
    if (erreicht == 0) {
        printf("  HINWEIS: kein Korpus-IMG erreicht uftc_convert_img_to_imd() "
               "— der Wandler ist ueber uft_convert_file() unerreichbar "
               "(Klasse MF-930/P3-204). Die Erfindungs-Zusage ist damit "
               "UNGEPRUEFT, nicht erfuellt.\n");
    }

    printf("%s\n", fehler ? "FEHLGESCHLAGEN" : "BESTANDEN (0 Fehler)");
    return fehler ? 1 : 0;
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_track_verdikt.c
 * @brief Der eine Verdikt-Bauer (MF-765). Siehe uft_track_verdikt.h.
 *
 * Die Reihenfolge der Prüfungen ist eine **Vorrangfrage**, und sie ist
 * hier bewusst an einer Stelle sichtbar statt über fünf Rückgabestellen
 * verteilt.
 *
 * ── Der Vorrang ist teilweise UNBELEGT, und das steht hier ───────────────
 *
 * Kein Handbuch sagt, welche Prüfung X-Copy zuerst greifen lässt. Für
 * eine leere Spur ist offen, ob die Vorlage Code 1 („weniger als 11
 * Sektoren") oder Code 2 („kein Sync") zeigt — beide treffen zu. Diese
 * Frage kann nur ein Lauf des Originals unter Emulation beantworten
 * (E1), und bis dahin ist die Reihenfolge unten eine **Setzung**, keine
 * Messung.
 *
 * Gewählt ist: **das Speziellere zuerst.** Wer nichts liest, hat kein
 * Format-Problem, sondern gar kein Medium — deshalb steht LEER vor
 * FREMDFORMAT. Das ist begründbar, aber nicht belegt.
 */
#include "uft/flux/uft_track_verdikt.h"

#include <string.h>

void uft_track_verdikt_bilden(const uft_track_befunde_t *b,
                              uft_track_verdikt_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!b) return;

    /* ── Folge ───────────────────────────────────────────────────────────
     *
     * Nur ein Fall sagt hier Nein, und das Handbuch sagt ihn ausdrücklich:
     * eine überlange Spur wurde mit Spezialhardware geschrieben und ist
     * mit gewöhnlichen Laufwerken nicht reproduzierbar. Alles andere ist
     * per Rohkopie übertragbar. */
    out->folge = b->ueberlange_spur ? UFT_FOLGE_NICHT_REPRODUZIERBAR
                                    : UFT_FOLGE_REPRODUZIERBAR;

    /* Der Schreibstartpunkt wird durchgereicht — der Aufrufer sucht,
     * hier wird nur geurteilt (MF-769). */
    out->splice_lage       = b->splice_lage;
    out->splice_pos_ns     = b->splice_pos_ns;
    out->splice_abstand_ns = b->splice_abstand_ns;
    out->splice_alt_ns     = b->splice_alt_ns;

    /* Und ein FEHLENDER Startpunkt ist eine Folge-Aussage.
     *
     * Diese Zeile beansprucht KEINE Herkunft aus der Vorlage (MF-773:
     * die frueher hier behauptete Entsprechung zu deren Ziffer 3 war
     * abgeleitet und ist widerlegt). Sie folgt aus UFTs eigener
     * Anforderung P6: der Startpunkt muss aus der ANALYSE der Spur
     * stammen. Ergibt die Analyse keinen, dann hat ein bitgenaues
     * Rueckschreiben keinen sicheren Anfang — und das ist genau das,
     * was „nicht reproduzierbar" heisst.
     *
     * UNBEKANNT bleibt bewusst folgenlos: eine zu kurze Aufnahme sagt
     * nichts ueber die Reproduzierbarkeit der Diskette. */
    if (b->splice_lage == UFT_SPLICE_FEHLT)
        out->folge = UFT_FOLGE_NICHT_REPRODUZIERBAR;

    /* ── Reparierbarkeit ─────────────────────────────────────────────────
     *
     * „Gerettet" schlägt alles andere. Das Änderungsprotokoll zu X-Copy
     * 5.21 dokumentiert genau diese Korrektur: zuvor wurde eine nach
     * Lesefehler gerettete Spur als fehlerfrei angezeigt, und die Autoren
     * stufen das als falsch ein, weil der Lesefehler real ist und auf der
     * Zieldiskette fortbesteht.
     *
     * Dieselbe Regel steht in diesem Baum als „kein stiller Datenverlust".
     * Ein Werkzeug, das Rettung und Erfolg gleich anzeigt, erzeugt falsche
     * Sicherheit beim Archivar. */
    if (b->corrections_applied > 0) {
        out->reparierbarkeit = UFT_REP_GERETTET;
    } else if (b->bad_id_crc > 0 || b->bad_data_crc > 0) {
        /* Prüfsummenfehler nennt das Handbuch je einzeln als von
         * `DOSCOPY+` behebbar (Codes 4 und 6). */
        out->reparierbarkeit = UFT_REP_KORRIGIERBAR;
    } else if (b->bad_header_format > 0 || b->missing_data > 0) {
        /* Klasse 5 (Kopfinhalt zerstört) und fehlende Daten nennt das
         * Handbuch NICHT als korrigierbar. */
        out->reparierbarkeit = UFT_REP_NICHT_KORRIGIERBAR;
    } else {
        out->reparierbarkeit = UFT_REP_NICHT_NOETIG;
    }

    /* ── Diagnose ────────────────────────────────────────────────────────
     * Vorrang: leer > unlesbar > Schutz > Fremdformat > Schaden > keine. */

    if (b->sector_count == 0) {
        if (b->histogramm_gueltig) {
            if (b->histogramm_berge == 0) {
                out->diagnose = UFT_DIAG_LEER;
                return;
            }
            if (b->histogramm_berge > 1 && !b->histogramm_sicher) {
                out->diagnose = UFT_DIAG_UNLESBAR;
                return;
            }
        }
        /* Struktur da (oder Bandmodell unpassend), aber keine bekannte
         * Marke. Das Handbuch von 1991 liest genau das als
         * „wahrscheinlich ein Kopierschutz oder Fremdformat".
         *
         * Welches von beiden, entscheidet erst eine Suche über einen
         * erweiterbaren Sync-Satz (P2). Bis dahin ist SCHUTZ die
         * vorsichtigere Auskunft: sie behauptet keine fremde Geometrie,
         * die niemand gemessen hat. */
        out->diagnose = UFT_DIAG_SCHUTZ;
        out->schutz_name  = b->marke_gefunden ? b->marke_name : NULL;
        out->schutz_marke = b->marke_gefunden ? b->marke : 0;
        return;
    }

    /* Sektoren gelesen — passt die Anzahl?
     *
     * `expected_sectors == 0` heisst UNBEKANNT, nicht „null erwartet".
     * Der Flussdekoder kennt die Sollzahl heute nicht; sie steht in der
     * OTDR-Konfiguration (`floppy_otdr.c:113-140`, nach Plattformnamen).
     * Sobald ein Aufrufer sie mitgibt, greift diese Prüfung — und das
     * ist X-Copys Code 1, der beide Richtungen kennt: „less or MORE
     * than 11 sectors". Eine Spur mit ZU VIELEN Sektoren ist ein
     * klassisches Schutzmerkmal; eine einseitige Schranke sieht es
     * nicht. */
    if (b->expected_sectors > 0 && b->sector_count != b->expected_sectors) {
        out->diagnose = UFT_DIAG_FREMDFORMAT;
        return;
    }

    if (b->bad_id_crc > 0 || b->bad_data_crc > 0 ||
        b->bad_header_format > 0 || b->missing_data > 0) {
        out->diagnose = UFT_DIAG_SCHADEN;
        return;
    }

    out->diagnose = UFT_DIAG_KEINE;
}

const char *uft_track_diagnose_name(uft_track_diagnose_t d)
{
    switch (d) {
    case UFT_DIAG_KEINE:        return "keine";
    case UFT_DIAG_LEER:         return "leer";
    case UFT_DIAG_FREMDFORMAT:  return "Fremdformat";
    case UFT_DIAG_SCHUTZ:       return "Schutz";
    case UFT_DIAG_SCHADEN:      return "Schaden";
    case UFT_DIAG_UNLESBAR:     return "unlesbar";
    default:                    return "unbekannt";
    }
}

const char *uft_track_folge_name(uft_track_folge_t f)
{
    switch (f) {
    case UFT_FOLGE_REPRODUZIERBAR:        return "reproduzierbar";
    case UFT_FOLGE_NICHT_REPRODUZIERBAR:  return "nicht reproduzierbar";
    default:                              return "unbekannt";
    }
}

const char *uft_track_reparierbarkeit_name(uft_track_reparierbarkeit_t r)
{
    switch (r) {
    case UFT_REP_NICHT_NOETIG:        return "nicht noetig";
    case UFT_REP_KORRIGIERBAR:        return "korrigierbar";
    case UFT_REP_NICHT_KORRIGIERBAR:  return "nicht korrigierbar";
    case UFT_REP_GERETTET:            return "GERETTET";
    default:                          return "unbekannt";
    }
}

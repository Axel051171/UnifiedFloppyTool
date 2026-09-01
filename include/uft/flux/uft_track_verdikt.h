/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_track_verdikt.h
 * @brief Ein Spurverdikt aus drei Feldern statt einer Fehlerziffer (MF-765).
 *
 * ── Warum es diese Stelle gibt ───────────────────────────────────────────
 *
 * `return (sector_count > 0) ? FLUX_OK : FLUX_ERR_NO_SYNC` steht im
 * Flussdekoder an **fünf** Stellen — MFM, FM, GCR-C64, GCR-Apple und der
 * reine Bitstrom-Pfad. Jede Verfeinerung dieses Urteils musste bisher
 * fünfmal gebaut werden, und MF-764 hat genau eine davon verfeinert, weil
 * vier andere ein anderes Bandmodell brauchen.
 *
 * Diese Datei ist die Zusammenführung: **eine** Funktion bildet das
 * Urteil, die fünf Stellen rufen sie. Danach berührt jede weitere
 * Verfeinerung eine Zeile.
 *
 * ── Die drei Felder ──────────────────────────────────────────────────────
 *
 * X-Copy presst drei Aussagen in eine rote Ziffer. Die Zerlegung stammt
 * aus den Handbüchern von 1991 (Fassung 3.4, Abschnitt „7.2) ERRORS",
 * und 5.21), nicht aus dem Quelltext der Vorlage:
 *
 *   * **Diagnose** — was ist mit dem Medium? Das Handbuch liest eine
 *     abweichende Sektorzahl ausdrücklich als „könnte ein Fremdformat
 *     sein" und fehlende Marken als „wahrscheinlich ein Kopierschutz oder
 *     Fremdformat" — also gerade nicht als Schaden.
 *   * **Folge** — lässt sich das auf einem gewöhnlichen Laufwerk
 *     reproduzieren? Nur die überlange Spur sagt hier Nein, und das
 *     Handbuch sagt es ausdrücklich.
 *   * **Reparierbarkeit** — Kopf- und Datenprüfsummen nennt das Handbuch
 *     je einzeln als von `DOSCOPY+` korrigierbar; „gerettet" ist der
 *     dritte Wert und stammt aus dem Änderungsprotokoll zu 5.21, das
 *     ausdrücklich verlangt, eine Rettung nicht als Erfolg anzuzeigen.
 *
 * ── Was hier NICHT steht ─────────────────────────────────────────────────
 *
 * Keine Struktur, keine Zerlegung, keine Namensgebung und kein
 * Datenlayout der Vorlage. Die Zuordnung ist ein Vertrag über UFTs
 * eigenes Verhalten; die Codeziffern stehen als Fundstelle, nicht als zu
 * übernehmender Wert.
 */
#ifndef UFT_TRACK_VERDIKT_H
#define UFT_TRACK_VERDIKT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Was ist mit dem Medium? */
typedef enum {
    UFT_DIAG_UNBEKANNT = 0,     /**< nicht bestimmt */
    UFT_DIAG_KEINE,             /**< gelesen, nichts zu melden */
    UFT_DIAG_LEER,              /**< keine Wechsel-Struktur: leer/gelöscht */
    UFT_DIAG_FREMDFORMAT,       /**< Struktur da, Geometrie nicht die unsere */
    UFT_DIAG_SCHUTZ,            /**< Struktur ohne bekannte Marke */
    UFT_DIAG_SCHADEN,           /**< Prüfsumme oder Kopf beschädigt */
    UFT_DIAG_UNLESBAR           /**< Wechsel ohne gemeinsamen Takt */
} uft_track_diagnose_t;

/** Lässt sich das auf gewöhnlicher Hardware reproduzieren? */
typedef enum {
    UFT_FOLGE_UNBEKANNT = 0,
    UFT_FOLGE_REPRODUZIERBAR,
    UFT_FOLGE_NICHT_REPRODUZIERBAR  /**< z. B. überlange Spur */
} uft_track_folge_t;

/** Kann der Befund behoben werden — und wurde er es? */
typedef enum {
    UFT_REP_UNBEKANNT = 0,
    UFT_REP_NICHT_NOETIG,       /**< nichts zu reparieren */
    UFT_REP_KORRIGIERBAR,       /**< Prüfsummenfehler, behebbar */
    UFT_REP_NICHT_KORRIGIERBAR, /**< kein Verfahren dafür */
    UFT_REP_GERETTET            /**< behoben — und das MUSS sichtbar bleiben */
} uft_track_reparierbarkeit_t;

/** Das Spurverdikt. */
typedef struct {
    uft_track_diagnose_t        diagnose;
    uft_track_folge_t           folge;
    uft_track_reparierbarkeit_t reparierbarkeit;
} uft_track_verdikt_t;

/** Eingaben des Bauers — alles, was der Dekoder ohnehin schon weiss. */
typedef struct {
    size_t   sector_count;      /**< tatsächlich dekodierte Sektoren */
    size_t   expected_sectors;  /**< Sollzahl; **0 = unbekannt** */
    size_t   bad_id_crc;        /**< X-Copy-Klasse 4 */
    size_t   bad_header_format; /**< X-Copy-Klasse 5 */
    size_t   bad_data_crc;      /**< X-Copy-Klasse 6 */
    size_t   missing_data;
    size_t   corrections_applied; /**< >0 ⇒ „gerettet", nicht „in Ordnung" */
    bool     ueberlange_spur;   /**< X-Copy-Klasse 7 */

    /* Aus dem Intervall-Histogramm (MF-488). `histogramm_gueltig` ist
     * false, wenn die Kodierung nicht zum Bandmodell des Moduls passt —
     * siehe die Warnung unten. */
    bool     histogramm_gueltig;
    size_t   histogramm_berge;
    bool     histogramm_sicher;
} uft_track_befunde_t;

/**
 * @brief Aus den Befunden ein Verdikt bilden.
 *
 * ── Die Bandmodell-Falle, gemessen und nicht theoretisch ─────────────────
 *
 * Das Histogrammodul ist **auf MFM gebaut**: ein MFM-Strom trägt Abstände
 * von 2T, 3T und 4T, also drei Berge. Ein **FM**-Strom trägt zwei. Wer
 * `histogramm_gueltig = true` für einen FM-Strom setzt, bekommt
 * `sicher == false` und damit die Diagnose **UNLESBAR** — eine gültige
 * Spur als defekt gemeldet.
 *
 * Deshalb entscheidet der Aufrufer, ob sein Bandmodell passt, und der
 * Bauer benutzt das Histogramm nur dann. Falsch grob ist besser als
 * präzise falsch.
 *
 * @param b   die Befunde
 * @param out das gefüllte Verdikt (darf nicht NULL sein)
 */
void uft_track_verdikt_bilden(const uft_track_befunde_t *b,
                              uft_track_verdikt_t *out);

/** Klartext für Berichte und Tests. */
const char *uft_track_diagnose_name(uft_track_diagnose_t d);
const char *uft_track_folge_name(uft_track_folge_t f);
const char *uft_track_reparierbarkeit_name(uft_track_reparierbarkeit_t r);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_VERDIKT_H */

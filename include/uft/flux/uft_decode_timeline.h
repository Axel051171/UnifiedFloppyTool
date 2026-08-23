/**
 * @file uft_decode_timeline.h
 * @brief Wo auf der Spur steht was — Scheibenkarte einer Dekodierung (MF-501).
 *
 * Baustein 2.3 des Mammut-Plans.
 *
 * ── Was hier fehlte ──────────────────────────────────────────────────────
 *
 * Der Decoder weiss seit jeher, WO ein Sektor lag: `flux_decoded_sector_t`
 * traegt `id_position` und `data_position`, und vier Stellen in
 * `uft_flux_decoder.c` fuellen sie. Gelesen werden sie von genau einer
 * Funktion — `uft_otdr_adaptive_decode()` —, und die hat **keinen
 * Aufrufer**; die beiden Erwaehnungen in der Oberflaeche sind Kommentare,
 * die erklaeren, warum sie nicht verdrahtet ist.
 *
 * Die Ortsangabe wird also berechnet und weggeworfen. Was fehlt, ist
 * nichts Neues, sondern ein Abnehmer: eine Karte ueber der Spur, die sagt,
 * welcher Abschnitt heil gelesen wurde, welcher beschaedigt ist und
 * welchen niemand angefasst hat.
 *
 * ── Warum das mehr ist als ein Bericht ───────────────────────────────────
 *
 * Zwei Dinge haengen daran:
 *
 * 1. **Die Polarkarte** (Mammut 3.2) hatte keine Datenquelle — das
 *    Multiread-Ergebnis traegt `weak_offset`, einen Byte-Versatz IM
 *    Sektor, aber keine Winkellage auf der Spur. Eine Bitposition laesst
 *    sich in eine Winkellage umrechnen, sobald Zellendauer und
 *    Umdrehungsdauer bekannt sind — beides misst der Decoder seit
 *    MF-488/492/496 und meldet es seit MF-496.
 * 2. **Teure Stufen gezielt einsetzen.** Wer weiss, welche Abschnitte
 *    schon heil sind, muss sie nicht noch einmal durch Mehrfachlesung
 *    oder Entzerrung schicken.
 *
 * Punkt 2 ist hier NICHT umgesetzt: die Recovery-Stufen arbeiten weiter
 * ueber die ganze Spur. Diese Datei liefert die Karte; wer sie zum
 * Steuern benutzt, ist ein eigener Schritt mit eigener Messung.
 *
 * ── Die Zusicherungen ────────────────────────────────────────────────────
 *
 * Eine Karte, die luegt, ist schlimmer als keine. Deshalb gelten:
 *
 * - **Lueckenlos und ueberschneidungsfrei.** Die Scheiben decken den
 *   Bitstrom genau einmal ab. Ein nicht abgedeckter Bereich waere ein
 *   Bereich, ueber den die Karte schweigt, ohne es zu sagen.
 * - **Aufsteigend und nicht leer.** Jede Scheibe hat mindestens ein Bit.
 * - **Keine Erfindung.** @ref UFT_SLICE_DECODED gibt es nur, wo ein
 *   Sektor mit heilen Pruefsummen lag. Was niemand behauptet hat, heisst
 *   @ref UFT_SLICE_UNTOUCHED — nicht „vermutlich in Ordnung".
 * - **Kein Winkel ohne Umdrehung.** Ohne gemessene Umdrehungsdauer gibt
 *   @ref uft_timeline_angle einen negativen Wert zurueck statt einer
 *   plausibel aussehenden Zahl.
 */
#ifndef UFT_DECODE_TIMELINE_H
#define UFT_DECODE_TIMELINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/flux/uft_flux_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Was ueber einen Spurabschnitt bekannt ist. */
typedef enum {
    /** Niemand hat hier etwas behauptet. Der Default ist bewusst 0:
     *  eine genullte Karte behauptet nichts. */
    UFT_SLICE_UNTOUCHED = 0,
    /** Ein Sektor mit heilen Pruefsummen lag hier. */
    UFT_SLICE_DECODED,
    /** Ein Sektor lag hier, aber seine Pruefsumme stimmte nicht. */
    UFT_SLICE_DAMAGED
} uft_slice_status_t;

/** Ein Abschnitt des Bitstroms. */
typedef struct {
    size_t             first_bit;   /**< erstes Bit, einschliesslich */
    size_t             end_bit;     /**< erstes Bit DAHINTER */
    uft_slice_status_t status;
    /** Sektornummer, oder -1 wenn zu diesem Abschnitt keine gehoert. */
    int                sector;
} uft_decode_slice_t;

/** Die Karte einer Spur. */
typedef struct {
    uft_decode_slice_t *slices;
    size_t              count;
    size_t              bit_count;      /**< Laenge des Bitstroms */
    double              cell_ns;        /**< 0 = unbekannt */
    double              revolution_ns;  /**< 0 = unbekannt */
    /** Gemessene Schwankung der Zellendauer entlang der Spur
     *  (`flux_decoded_track_t::warp_span`, MF-495). 0 oder 1 = keine.
     *  Bestimmt die Genauigkeit des Winkels — siehe
     *  @ref uft_timeline_angle_error. */
    double              warp_span;
} uft_decode_timeline_t;

/**
 * Grad Winkelfehler je Einheit gemessener Spanne (MF-502).
 *
 * **Gemessen, nicht geschaetzt.** Der Winkel entsteht aus
 * `Bitindex * Zellendauer`, nimmt also eine konstante Zellendauer an. Wo
 * die Spur nicht gleichmaessig lief, stimmt das nicht mehr. Verglichen
 * wurde gegen die WAHRE Lage — die kumulierte Zeit bis zur Sync-Marke,
 * eine Summe und keine Schaetzung:
 *
 *   Spanne   Fehler
 *   ------   ------
 *   1,000     0,0 Grad
 *   1,020     2,7
 *   1,050     5,3
 *   1,080    10,8
 *   1,153    35,5      <- engster Fall
 *
 * 250 Grad je Einheit deckt jede dieser Zeilen ab (bei 1,153 sind das
 * 38,3 Grad Schranke gegen 35,5 gemessen). Die Schranke ist bewusst
 * konservativ: der Spannen-Schaetzer meldet bei starkem Verzug WENIGER
 * als eingebaut wurde, und eine Schranke, die davon ausgeht, waere zu
 * knapp.
 */
#define UFT_TIMELINE_ANGLE_ERR_PER_SPAN   250.0

/**
 * @brief Karte aus einem Dekodier-Ergebnis bauen.
 *
 * @param track          Ergebnis einer Dekodierung
 * @param bit_count      Laenge des Bitstroms, auf den sich die Positionen
 *                       beziehen. 0 ist ein Fehler — ohne Laenge laesst
 *                       sich „lueckenlos" nicht zusichern.
 * @param cell_ns        Zellendauer in ns, oder 0 wenn unbekannt
 * @param revolution_ns  Umdrehungsdauer in ns, oder 0 wenn unbekannt
 * @param out            Ergebnis; mit @ref uft_timeline_free freigeben
 * @return false bei unbrauchbaren Argumenten oder fehlendem Speicher
 */
bool uft_timeline_build(const flux_decoded_track_t *track, size_t bit_count,
                        double cell_ns, double revolution_ns,
                        uft_decode_timeline_t *out);

/**
 * @brief Wie genau ist die Winkelangabe dieser Karte?
 *
 * @return Schranke des Winkelfehlers in **Grad**, oder negativ, wenn es
 *         gar keinen Winkel gibt (fehlende Zeitbasis). 0 heisst: die Spur
 *         lief gleichmaessig, der Winkel ist exakt.
 *
 * Wer eine Winkellage weiterverarbeitet, muss diesen Wert kennen. Eine
 * Polarkarte, die einen Punkt mit 40 Grad Unsicherheit als Punkt malt,
 * behauptet eine Genauigkeit, die niemand gemessen hat.
 */
double uft_timeline_angle_error(const uft_decode_timeline_t *t);

/** Gibt die Scheibenliste frei und nullt die Struktur. */
void uft_timeline_free(uft_decode_timeline_t *t);

/**
 * @brief Winkellage des Scheibenanfangs, als Anteil einer Umdrehung.
 *
 * @return 0…1, oder **negativ**, wenn Zellendauer oder Umdrehungsdauer
 *         fehlen. Ein Winkel ohne Zeitbasis waere eine erfundene Zahl.
 */
double uft_timeline_angle(const uft_decode_timeline_t *t, size_t slice);

/**
 * @brief Welcher Anteil der Spur traegt diesen Zustand?
 *
 * Gemessen in Bits, nicht in Scheiben — zehn winzige beschaedigte
 * Abschnitte sind nicht dasselbe wie ein halb zerstoerter Sektor.
 *
 * @return 0…1; 0 bei leerer Karte
 */
double uft_timeline_fraction(const uft_decode_timeline_t *t,
                             uft_slice_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DECODE_TIMELINE_H */

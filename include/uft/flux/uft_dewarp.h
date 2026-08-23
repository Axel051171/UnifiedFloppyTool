/**
 * @file uft_dewarp.h
 * @brief Gleichlauffehler aus dem Flussstrom herausrechnen (MF-495).
 *
 * Baustein 2.2 des Mammut-Plans.
 *
 * ── Das Problem ──────────────────────────────────────────────────────────
 *
 * Eine reale Diskette laeuft nicht ueberall gleich schnell: ein verzogenes
 * Band, eine schwankende Andruckrolle, ein Laufwerk mit Gleichlauffehler.
 * Dann traegt ein Abschnitt der Spur eine andere Zellendauer als der Rest —
 * und es gibt keinen richtigen EINZELNEN Taktwert mehr. Gemessen an einer
 * Spur, deren erste 35 % um 70 % gedehnt sind: mit der besten festen
 * Vorgabe kommen 3 von 11 Sektoren zurueck, automatisch 8, davon 7 heil.
 *
 * ── Das Verfahren ────────────────────────────────────────────────────────
 *
 * Zu jedem Intervall gehoert eine ganze Zahl von Zellen — bei MFM 2, 3
 * oder 4. Also ist `d / k` ein Sofort-Takt, und die geglaettete Folge
 * dieser Sofort-Takte IST der Geschwindigkeitsverlauf. Geglaettet wird
 * exponentiell, und zwar **vorwaerts und rueckwaerts**: eine einseitige
 * Glaettung laeuft der Wahrheit immer hinterher, und der Nachlauf waere
 * ausgerechnet an den Sprungstellen am groessten, um die es geht.
 *
 * Danach wird jedes Intervall auf einen gemeinsamen Bezugstakt umgerechnet.
 * Der Ausgabestrom hat denselben Inhalt, nur ohne den Gleichlauffehler.
 *
 * ── Was das NICHT ist ────────────────────────────────────────────────────
 *
 * **Keine Aufnahme.** Der entzerrte Strom ist ein Rechenzwischenschritt
 * zum Dekodieren und darf niemals als Aufnahme gespeichert werden — er
 * traegt veraenderte Zeiten, und veraenderte Zeiten sind veraenderte
 * Rohdaten. Wer ihn schreibt, schreibt eine Diskette, die es nie gab.
 *
 * ── Gemessene Grenzen ────────────────────────────────────────────────────
 *
 * Es gibt **kein sicheres festes @p alpha**. Gemessen (synthetische
 * AmigaDOS-Spuren, Spalten gefunden/heil):
 *
 *   Fall                        ohne     a=0,01    a=0,05    a=0,20
 *   --------------------------  -------  --------  --------  --------
 *   sauber                       11/11    11/11     11/11     11/11
 *   zwei Tempi (35 % x1,7)        8/7      8/7      11/10     11/10
 *   Rampe +25 % + 6 % Zittern    11/11    11/11     10/10     10/10
 *
 * Bei 0,05 gewinnt die Zwei-Tempo-Spur drei heile Sektoren — und die Rampe
 * verliert einen. Das ist die Ueberanpassung, vor der flux-analyze im
 * Kommentar warnt: ein genauerer Fit ist nicht automatisch besser. Deshalb
 * darf diese Stufe nur als ZUSAETZLICHER Kandidat laufen, dessen Ergebnis
 * gegen den unentzerrten Durchlauf antritt — gewinnen ja, kosten nein.
 *
 * In keinem gemessenen Fall entstand ein Sektor mit heiler Pruefsumme und
 * falschem Inhalt. Das ist die Bedingung, ohne die die Stufe nichts im
 * Lesepfad zu suchen haette.
 */
#ifndef UFT_DEWARP_H
#define UFT_DEWARP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Voreinstellung der Glaettung. Siehe die Tabelle oben: 0,05 holt den
 *  vollen Gewinn auf der Zwei-Tempo-Spur. Groessere Werte heben bei
 *  starkem Zittern zwar die Zahl GEFUNDENER Sektoren, aber nicht die der
 *  heilen — dafuer steigt die Ueberanpassung. */
#define UFT_DEWARP_DEFAULT_ALPHA   0.05

/** Ergebnis einer Entzerrung. */
typedef struct {
    /** Bezugstakt, auf den der Ausgabestrom umgerechnet wurde (ns). */
    double ref_clock;

    /** Verhaeltnis groesster zu kleinster gemessener Zellendauer entlang
     *  der Spur. 1,0 heisst: kein Gleichlauffehler gefunden. Das ist der
     *  Diagnosewert — „das Medium eiert um 12 %". */
    double warp_span;
} uft_dewarp_result_t;

/**
 * @brief Gleichlauffehler aus einem Intervallstrom herausrechnen.
 *
 * @param in     Abstaende zwischen Flusswechseln (INTERVALLE, keine
 *               kumulierten Zeiten)
 * @param n      Anzahl
 * @param t0     Startwert der Zellendauer in ns; muss > 0 sein. Sinnvoll
 *               ist ein GEMESSENER Wert (Histogramm MF-488 oder
 *               `uft_sync_median_clock()` MF-492) — mit einem groben
 *               Startwert braucht die Glaettung laenger, bis sie sitzt.
 * @param alpha  Glaettung in (0, 1]; <= 0 waehlt @ref
 *               UFT_DEWARP_DEFAULT_ALPHA
 * @param out    Zielfeld, mindestens @p n Eintraege. Darf @p in sein.
 * @param res    Ergebnisangaben, oder NULL
 * @return false bei unbrauchbaren Argumenten oder wenn kein einziger
 *         Sofort-Takt gebildet werden konnte
 */
bool uft_dewarp_intervals(const uint32_t *in, size_t n, double t0,
                          double alpha, uint32_t *out,
                          uft_dewarp_result_t *res);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DEWARP_H */

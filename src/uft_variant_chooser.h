/**
 * @file uft_variant_chooser.h
 * @brief Die Zielvariante beim Speichern — zeigen, was geht, und warum
 *        der Rest nicht (MF-666)
 *
 * Stufe 4 aus `docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md`, letzter Teil.
 *
 * ── Was dieser Wähler tut, und was er bewusst NICHT tut ──────────────────
 *
 * Er zeigt **alle** Varianten, die das Zielformat kennt, und macht die
 * nicht schreibbaren **unanwählbar** — mit der Begründung daneben. Das
 * ist die Vorgabe des Eigentümers, wörtlich: „sollte es bei einigen
 * Formaten nicht möglich sein, die Bedienelemente zu nutzen, sollen sie
 * für diese Formate ausgeblendet und in dem Moment nicht nutzbar sein."
 *
 * Bei HFE heißt das: `HFEv1` wählbar und vorausgewählt, `HFEv3`
 * sichtbar aber grau, mit dem Hinweis „UFT liest HFEv3, schreibt es
 * aber nicht."
 *
 * ── Die Grenze, die hier ausgesprochen gehört ────────────────────────────
 *
 * `uft_convert_file()` nimmt ein **Format** entgegen, keine Variante —
 * und der HFE-Schreiber erzeugt immer v1 (`uft_hfe.c:567-568`, gemessen).
 *
 * Solange je Format **genau eine** Variante schreibbar ist, ist die
 * Auswahl eindeutig und braucht keine Verkabelung nach unten: was der
 * Wähler zeigt, ist auch das, was entsteht. Sobald ein Format eine
 * **zweite** schreibbare Variante bekommt, braucht `uft_convert_file()`
 * einen Varianten-Parameter — sonst wäre die Auswahl ein Bedienelement
 * ohne Wirkung, also genau das, was Stufe 5 beseitigt.
 *
 * Deshalb prüft `uftSaveImageAs()` die Wahl gegen `can_write` und lehnt
 * ab, statt stillschweigend etwas anderes zu schreiben. Der Wähler
 * verspricht nichts, was die Maschine nicht hält.
 */

#ifndef UFT_VARIANT_CHOOSER_H
#define UFT_VARIANT_CHOOSER_H

#include <QString>

class QWidget;

/**
 * @brief Fragt die Zielvariante ab, wenn es etwas zu fragen gibt.
 *
 * @param parent       Elternfenster für den Dialog.
 * @param zielformat   Formatname, wie das Plugin ihn führt („HFE").
 * @param abgebrochen  wird true, wenn der Benutzer abgebrochen hat.
 *
 * @return der gewählte Variantenname, oder ein leerer QString wenn das
 *         Format keine Varianten führt (dann gibt es nichts zu wählen
 *         und es erscheint kein Dialog).
 *
 * Führt das Format Varianten, aber **keine** davon ist schreibbar, wird
 * das gesagt und @p abgebrochen gesetzt — speichern wäre dann eine
 * Zusage ohne Deckung.
 */
QString uftAskWriteVariant(QWidget *parent,
                           const QString &zielformat,
                           bool *abgebrochen);

#endif /* UFT_VARIANT_CHOOSER_H */

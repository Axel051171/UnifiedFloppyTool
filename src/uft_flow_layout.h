#ifndef UFT_FLOW_LAYOUT_H
#define UFT_FLOW_LAYOUT_H

/*
 * MF-1293 - das Fliesslayout, das Qt fehlt.
 *
 * Der GUI-Entwurf ordnet die Einstellungsgruppen in SPALTEN an: von oben
 * nach unten fuellen, und wenn die Hoehe erschoepft ist, rechts daneben
 * weitermachen. Im WinForms-Entwurf macht das ein `FlowLayoutPanel` mit
 * `FlowDirection.TopDown` und `WrapContents`; der Generator markiert den
 * Traeger dafuer mit der Eigenschaft `uft_flow = topdown`.
 *
 * Qt hat kein Fliesslayout. `uic` ignoriert die unbekannte Eigenschaft
 * stillschweigend und laesst das `QVBoxLayout` stehen - und genau deshalb
 * sah der Reiter im Programm aus wie eine EINZIGE lange Spalte, waehrend
 * die Vorschau drei nebeneinander zeigte. Das war kein Fehler im Entwurf
 * und keiner im Uebertragen: es war eine Eigenschaft ohne Leser.
 *
 * Diese Klasse ist dieser Leser.
 *
 * Wie es rechnet, damit es nachlesbar ist statt geraten:
 *
 *   Spaltenbreite  = die groesste Wunschbreite aller Elemente. Alle
 *                    Spalten sind gleich breit; ungleiche Spalten lassen
 *                    eine Gruppenreihe wie einen Treppenabsatz aussehen.
 *   Spaltenzahl    = wie viele davon in die angebotene Breite passen,
 *                    mindestens eine.
 *   Verteilung     = der Reihe nach fuellen, bis die Zielhoehe erreicht
 *                    ist. Die Zielhoehe ist die Gesamthoehe geteilt durch
 *                    die Spaltenzahl - so werden die Spalten aehnlich
 *                    hoch, statt die erste zu ueberfuellen und die letzte
 *                    fast leer zu lassen.
 *
 * `hasHeightForWidth()` ist wahr, weil die Hoehe hier WIRKLICH von der
 * Breite abhaengt: mehr Breite heisst mehr Spalten heisst weniger Hoehe.
 * Ohne das rechnet der Rollbereich mit der Hoehe einer einzigen Spalte
 * und zeigt einen Rollbalken, den es nicht braucht.
 */

#include <QLayout>
#include <QList>
#include <QRect>
#include <QSize>

class UftFlowLayout : public QLayout
{
public:
    explicit UftFlowLayout(QWidget *parent = nullptr, int rand = 0,
                           int abstandX = 6, int abstandY = 4);
    ~UftFlowLayout() override;

    void addItem(QLayoutItem *item) override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;

    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int breite) const override;

    QSize sizeHint() const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect &r) override;

    /* MF-1299 - die zuletzt gerechnete Spaltenzahl. Damit kann das Layout
     * seine eigene Messung melden, statt dass jemand sie schaetzt. */
    int spalten() const { return m_spalten; }

    /* MF-1299 - Posten an den ANFANG stellen. addItem() haengt hinten an;
     * der Kopiermodus gehoert aber nach oben links, weil er die Auswahl
     * ist, von der alles andere abhaengt. */
    void vornAnstellen(QWidget *w);

    /* MF-1302 - einen bereits eingehaengten Posten an eine andere Stelle
     * der Reihenfolge setzen. Die Reihenfolge bestimmt, in welcher Spalte
     * und wie weit oben eine Gruppe landet. */
    void verschiebeAn(QWidget *w, int ziel);

private:
    /* Legt an, wenn `anwenden` wahr ist; gibt in jedem Fall die noetige
     * Hoehe zurueck. EINE Rechnung fuer beide Fragen - zwei getrennte
     * waeren die Bauform, die MF-1177 verbietet. */
    int verteile(const QRect &r, bool anwenden) const;

    QList<QLayoutItem *> m_posten;
    mutable int m_spalten = 0;
    int m_abstandX;
    int m_abstandY;
};

#endif // UFT_FLOW_LAYOUT_H

/*
 * MF-1293 - Umsetzung des Spalten-Fliesslayouts. Kopf: uft_flow_layout.h
 */

#include "uft_flow_layout.h"

#include <QWidget>
#include <QDebug>
#include <QStringList>
#include <algorithm>

UftFlowLayout::UftFlowLayout(QWidget *parent, int rand, int abstandX,
                             int abstandY)
    : QLayout(parent), m_abstandX(abstandX), m_abstandY(abstandY)
{
    setContentsMargins(rand, rand, rand, rand);
}

UftFlowLayout::~UftFlowLayout()
{
    while (QLayoutItem *p = takeAt(0)) delete p;
}

void UftFlowLayout::addItem(QLayoutItem *item) { m_posten.append(item); }

void UftFlowLayout::vornAnstellen(QWidget *w)
{
    if (!w) return;
    addWidget(w);                       /* haengt hinten an ... */
    if (m_posten.size() > 1)
        m_posten.move(m_posten.size() - 1, 0);   /* ... und wandert nach vorn */
    invalidate();
}

void UftFlowLayout::verschiebeAn(QWidget *w, int ziel)
{
    if (!w || ziel < 0) return;
    for (int i = 0; i < m_posten.size(); i++) {
        if (m_posten.at(i)->widget() != w) continue;
        const int z = std::min(ziel, int(m_posten.size()) - 1);
        if (i != z) {
            m_posten.move(i, z);
            invalidate();
        }
        return;
    }
}

int UftFlowLayout::count() const { return int(m_posten.size()); }

QLayoutItem *UftFlowLayout::itemAt(int index) const
{
    return (index >= 0 && index < m_posten.size()) ? m_posten.at(index)
                                                   : nullptr;
}

QLayoutItem *UftFlowLayout::takeAt(int index)
{
    if (index < 0 || index >= m_posten.size()) return nullptr;
    return m_posten.takeAt(index);
}

Qt::Orientations UftFlowLayout::expandingDirections() const
{
    return Qt::Orientations();
}

bool UftFlowLayout::hasHeightForWidth() const { return true; }

int UftFlowLayout::heightForWidth(int breite) const
{
    return verteile(QRect(0, 0, breite, 0), false);
}

QSize UftFlowLayout::minimumSize() const
{
    /* Schmaler als die breiteste Gruppe geht nicht - sonst wuerde eine
     * Gruppe beschnitten, und ein beschnittenes Bedienelement ist
     * schlimmer als ein Rollbalken. */
    int b = 0;
    for (QLayoutItem *p : m_posten)
        b = std::max(b, p->minimumSize().width());
    const QMargins m = contentsMargins();
    return QSize(b + m.left() + m.right(), 0);
}

QSize UftFlowLayout::sizeHint() const { return minimumSize(); }

void UftFlowLayout::setGeometry(const QRect &r)
{
    QLayout::setGeometry(r);
    verteile(r, true);
}

/* MF-1301 - Spaltenzahl nach dem MINDESTMASS, Breite nach dem Wunsch.
 *
 * Drei Fassungen, drei Messungen:
 *
 *   MF-1293  alle Spalten gleich breit (= breitester Wunsch). Der schmale
 *            Kopiermodus wurde auf ueber 500 px aufgeblasen.
 *   MF-1300  eigene Breite je Spalte, Zahl aber weiter am WUNSCH. Gemessen
 *            am laufenden Reiter: breiteste Gruppe 856 px, Flaeche 1442 px
 *            -> EINE Spalte. Schlechter als vorher.
 *   MF-1301  Zahl am Mindestmass, Breite danach aus dem Rest verteilt.
 *
 * Der Punkt ist Qts eigene Regel: eine Gruppe darf unter ihren Wunsch
 * gestaucht werden, bis zu ihrem Mindestmass. Wer die Spaltenzahl am
 * Wunsch festmacht, verschenkt genau diesen Spielraum - und das war in
 * beiden Vorfassungen der Fehler.
 *
 * Die Zuteilung bleibt nach Zielhoehe, damit die Spalten aehnlich hoch
 * werden und nicht die erste vollaeuft. */
static void uft_flow_zuteilen(const QList<QSize> &wunsch, int abstandY,
                              int spalten, QList<QList<int>> &aus)
{
    aus.clear();
    if (spalten <= 0) return;
    for (int i = 0; i < spalten; i++) aus.append(QList<int>());

    int gesamt = 0;
    for (const QSize &s : wunsch) gesamt += s.height() + abstandY;
    const int ziel = (gesamt + spalten - 1) / spalten;

    int k = 0;
    int hoehe = 0;
    for (int i = 0; i < wunsch.size(); i++) {
        const int h = wunsch.at(i).height() + abstandY;
        const int rest = int(wunsch.size()) - i;
        /* MF-1304 - abbrechen nur, wenn Aufhoeren NAEHER am Ziel liegt
         * als Mitnehmen.
         *
         * Die Vorfassung brach ab, sobald die Zielhoehe ueberschritten
         * WUERDE. Gemessen am Settings-Reiter: Ziel 801 px, Spalte 0 stand
         * bei 530 und die naechste Gruppe war 340 hoch. Sie brach ab - und
         * lag damit 271 px unter dem Ziel, waehrend Mitnehmen nur 61 px
         * darueber gelegen haette. Ergebnis war eine halb leere linke und
         * eine ueberlange rechte Spalte, genau wie im Bildschirmfoto des
         * Eigentuemers.
         *
         * Jetzt entscheidet der Abstand zum Ziel, nicht das blosse
         * Ueberschreiten. */
        if (hoehe > 0 && k < spalten - 1 && rest > (spalten - 1 - k)) {
            const int ohne = std::abs(hoehe - ziel);
            const int mit = std::abs(hoehe + h - ziel);
            if (ohne <= mit) {
                k++;
                hoehe = 0;
            }
        }
        aus[k].append(i);
        hoehe += h;
    }
}

static int uft_flow_spaltenmass(const QList<int> &mass,
                                const QList<int> &spalte)
{
    int b = 0;
    for (int i : spalte) b = std::max(b, mass.at(i));
    return b;
}

int UftFlowLayout::verteile(const QRect &r, bool anwenden) const
{
    if (m_posten.isEmpty()) return 0;

    const QMargins m = contentsMargins();
    const QRect innen = r.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
    const int platz = innen.width();

    /* Beide Masse EINMAL einsammeln. `minimumSize()` ist das, worunter
     * eine Gruppe nicht darf; `sizeHint()` das, was sie gern haette. */
    QList<QSize> wunsch;
    QList<int> mindest;
    wunsch.reserve(m_posten.size());
    mindest.reserve(m_posten.size());
    for (QLayoutItem *p : m_posten) {
        const QSize s = p->sizeHint();
        wunsch.append(s);
        const int mw = p->minimumSize().width();
        mindest.append(mw > 0 ? mw : s.width());
    }

    /* Von vielen Spalten nach wenigen: die erste Aufteilung, deren
     * MINDESTmasse in die Breite passen. */
    QList<QList<int>> plan;
    int gewaehlt = 1;
    const int hoechstens = std::min(int(m_posten.size()), 6);
    for (int n = hoechstens; n >= 1; n--) {
        QList<QList<int>> k;
        uft_flow_zuteilen(wunsch, m_abstandY, n, k);

        bool leer = false;
        int summe = (n - 1) * m_abstandX;
        for (const QList<int> &s : k) {
            if (s.isEmpty()) { leer = true; break; }
            summe += uft_flow_spaltenmass(mindest, s);
        }
        if (leer) continue;
        if (platz <= 0 || summe <= platz || n == 1) {
            plan = k;
            gewaehlt = n;
            break;
        }
    }
    if (plan.isEmpty()) {
        uft_flow_zuteilen(wunsch, m_abstandY, 1, plan);
        gewaehlt = 1;
    }

    /* Breiten: vom Mindestmass aus den Rest verteilen, im Verhaeltnis zu
     * dem, was jede Spalte ueber ihr Mindestmass hinaus noch wuenscht.
     * Keine Spalte wird breiter als ihr Wunsch - sonst entstuende genau
     * die Dehnung, die MF-1300 beseitigen sollte. */
    QList<int> breite;
    QList<int> hunger;
    int summeMin = (int(plan.size()) - 1) * m_abstandX;
    int summeHunger = 0;
    for (const QList<int> &s : plan) {
        const int mi = uft_flow_spaltenmass(mindest, s);
        int wi = 0;
        for (int i : s) wi = std::max(wi, wunsch.at(i).width());
        breite.append(mi);
        hunger.append(std::max(0, wi - mi));
        summeMin += mi;
        summeHunger += std::max(0, wi - mi);
    }
    const int rest = (platz > 0) ? (platz - summeMin) : summeHunger;
    if (rest > 0 && summeHunger > 0) {
        for (int c = 0; c < breite.size(); c++) {
            const int dazu = int(qint64(rest) * hunger.at(c) / summeHunger);
            breite[c] += std::min(dazu, hunger.at(c));
        }
    }

    int x = innen.x();
    int hoechste = 0;
    for (int c = 0; c < plan.size(); c++) {
        int y = innen.y();
        for (int i : plan.at(c)) {
            if (anwenden)
                m_posten.at(i)->setGeometry(
                    QRect(x, y, breite.at(c), wunsch.at(i).height()));
            y += wunsch.at(i).height() + m_abstandY;
        }
        hoechste = std::max(hoechste, y - innen.y());
        x += breite.at(c) + m_abstandX;
    }

    const int noetig = hoechste + m.top() + m.bottom();

    /* MF-1293 - damit der Rollbereich ueberhaupt rollen KANN. sizeHint()
     * kann die noetige Hoehe nicht nennen, weil sie von der Breite
     * abhaengt; ein QScrollArea mit widgetResizable macht das Feld sonst
     * nur so hoch wie das Sichtfenster, schneidet ab und zeigt keinen
     * Rollbalken. */
    if (anwenden)
        if (QWidget *w = const_cast<UftFlowLayout *>(this)->parentWidget())
            if (w->minimumHeight() != noetig) w->setMinimumHeight(noetig);

    if (gewaehlt != m_spalten) {
        m_spalten = gewaehlt;
        QStringList b;
        for (int c = 0; c < breite.size(); c++)
            b << QString::number(breite.at(c));
        int minMax = 0, wunschMax = 0;
        for (int i = 0; i < mindest.size(); i++) {
            minMax = std::max(minMax, mindest.at(i));
            wunschMax = std::max(wunschMax, wunsch.at(i).width());
        }
        /* MF-1304 - jede Gruppe einzeln nennen: Name, Wunschhoehe,
         * Mindestbreite, zugeteilte Spalte. Ohne das laesst sich eine
         * schiefe Verteilung nicht erklaeren, nur vermuten. */
        for (int c = 0; c < plan.size(); c++)
            for (int i : plan.at(c)) {
                QWidget *w = m_posten.at(i)->widget();
                qInfo("[UftFlowLayout] MF-1304:   Spalte %d  %-26s "
                      "h=%4d  minB=%4d",
                      c, w ? qPrintable(w->objectName()) : "?",
                      wunsch.at(i).height(), mindest.at(i));
            }
        qInfo("[UftFlowLayout] MF-1301: %d Posten auf %d Spalten "
              "(Breiten %s px), Flaeche %d px; breiteste Gruppe: Wunsch "
              "%d px, Mindestmass %d px -> %d px hoch.",
              int(m_posten.size()), gewaehlt,
              qPrintable(b.join(QStringLiteral("+"))), platz,
              wunschMax, minMax, noetig);
    }

    return noetig;
}

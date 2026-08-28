/**
 * @file test_flux_visualizer.cpp
 * @brief FluxVisualizerWidget, kopflos angetrieben (MF-631, SCOUT-F2 Stufe 1)
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * `src/widgets/fluxvisualizerwidget.cpp` hat 1092 Zeilen, steht in
 * `UnifiedFloppyTool.pro` und wird in jeden Bau uebersetzt. Gemessen
 * (MF-630): **es wird nirgends instanziiert.** Die Suche nach
 * `FluxVisualizerWidget` ueber alle `*.cpp`/`*.h` unter `src/` liefert
 * Treffer nur in den beiden Widget-Dateien selbst.
 *
 * Uebersetzt heisst nicht ausgefuehrt. Dieser Code ist **nie gelaufen** —
 * kein Konstruktor, kein `paintEvent`, keine der fuenf Ansichten. Bevor
 * er in die Oberflaeche gehaengt wird (SCOUT-F2 Stufe 2), muss belegt
 * sein, dass er ueberhaupt traegt; sonst verdrahtet man Unbekanntes und
 * findet die Fehler beim Klicken.
 *
 * Warum der Eigentuemer ihn will: der Scatterplot aus dem
 * FloppyControl-Gutachten ist nicht blosse Anzeige, sondern das fehlende
 * **Bedienelement der Rettungskette** — `addRegion()` und
 * `setMarkerPosition()` sind die Fensterwahl fuer das CRC-Orakel, und
 * `addRevolution()` mit `COMPARISON` ist die Multirev-Sicht. Beides gibt
 * es hier bereits; es fehlte nur der Zugang.
 *
 * ── Was hier geprueft wird ───────────────────────────────────────────────
 *
 * 1. Der Datenweg: was hineingegeben wird, kommt ueber `exportToCSV()`
 *    wieder heraus — kein stiller Verlust, keine Umrechnung.
 * 2. Die Statistik rechnet gegen von Hand nachgerechnete Werte, nicht
 *    gegen sich selbst.
 * 3. **Jede der fuenf Ansichten wird tatsaechlich gezeichnet**
 *    (`QWidget::render` auf ein QImage). Das fuehrt `paintEvent` und die
 *    `draw*`-Zweige aus. Ein Absturz oder eine leere Flaeche faellt hier
 *    auf, nicht beim Eigentuemer.
 * 4. Mehrere Umdrehungen und Regionen ueberleben den Weg.
 *
 * Kopflos ueber `QT_QPA_PLATFORM=offscreen`, wie die uebrigen Qt-Tests
 * dieses Baums (MF-574).
 *
 * ── Eine Eigenheit unter MinGW, die man kennen muss ──────────────────────
 *
 * Das erzeugte Programm haengt an keiner Konsole; QtTest schreibt seinen
 * Bericht ins Leere. Ein Lauf von Hand gibt hier **null Byte** aus, auch
 * wenn alles laeuft — der Rueckgabewert ist der einzige verlaessliche
 * Kanal. Nachgemessen (MF-631): ohne eingebauten Fehler rc=0, mit einem
 * absichtlich falschen `QCOMPARE` rc=1. Der Test prueft also wirklich;
 * wer ihn hier scheitern sieht, liest den Grund in der CI (Linux, mit
 * Konsole) oder baut den Fehler nach.
 */

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>

#include "widgets/fluxvisualizerwidget.h"

#include <cmath>
#include <vector>

namespace {

/** Zellzeiten einer MFM-Spur in Nanosekunden: 4, 6 und 8 µs. */
const uint32_t ZELLE_4US = 4000;
const uint32_t ZELLE_6US = 6000;
const uint32_t ZELLE_8US = 8000;

/**
 * Ein wiederholbarer Strom aus den drei MFM-Zelllaengen.
 *
 * Bewusst ohne Zufall: der Test soll bei jedem Lauf dieselben Zahlen
 * pruefen, und die Statistik unten ist von Hand gegen genau diese Folge
 * nachgerechnet.
 */
std::vector<uint32_t> mfm_strom(int wiederholungen)
{
    std::vector<uint32_t> t;
    t.reserve(static_cast<size_t>(wiederholungen) * 3);
    for (int i = 0; i < wiederholungen; ++i) {
        t.push_back(ZELLE_4US);
        t.push_back(ZELLE_6US);
        t.push_back(ZELLE_8US);
    }
    return t;
}

} // namespace

class TestFluxVisualizer : public QObject
{
    Q_OBJECT

private slots:
    void der_datenweg_verliert_nichts();
    void die_statistik_stimmt_gegen_handrechnung();
    void jede_ansicht_zeichnet_wirklich();
    void mehrere_umdrehungen_und_regionen_ueberleben();
    void leere_daten_stuerzen_nicht_ab();
};

/**
 * Was hineingegeben wird, muss herauskommen.
 *
 * `exportToCSV()` ist der einzige Weg, die gespeicherten Werte wieder
 * abzufragen. Wenn dort etwas anderes steht als eingegeben, ist der
 * Anzeigepfad kein Beleg fuer die Daten — und genau das waere fuer ein
 * forensisches Werkzeug der schlimmste Fall.
 */
void TestFluxVisualizer::der_datenweg_verliert_nichts()
{
    FluxVisualizerWidget w;
    const std::vector<uint32_t> eingabe = mfm_strom(20);   // 60 Werte
    w.setFluxData(eingabe);

    const QString csv = w.exportToCSV();
    QVERIFY(!csv.isEmpty());

    // Alle drei Zelllaengen muessen im Ausgabetext vorkommen.
    QVERIFY(csv.contains(QString::number(ZELLE_4US)));
    QVERIFY(csv.contains(QString::number(ZELLE_6US)));
    QVERIFY(csv.contains(QString::number(ZELLE_8US)));

    // Und zwar so oft, wie sie eingegeben wurden. Gezaehlt wird ueber die
    // Zeilen, nicht ueber Teilzeichenketten — "4000" steckt sonst auch in
    // "14000" und der Test waere blind fuer eine Verschiebung.
    int treffer4 = 0, treffer6 = 0, treffer8 = 0;
    const QStringList zeilen = csv.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &z : zeilen) {
        const QStringList felder = z.split(QLatin1Char(','));
        for (const QString &f : felder) {
            const QString s = f.trimmed();
            if (s == QString::number(ZELLE_4US)) ++treffer4;
            else if (s == QString::number(ZELLE_6US)) ++treffer6;
            else if (s == QString::number(ZELLE_8US)) ++treffer8;
        }
    }
    QCOMPARE(treffer4, 20);
    QCOMPARE(treffer6, 20);
    QCOMPARE(treffer8, 20);
}

/**
 * Die Statistik gegen eine Handrechnung, nicht gegen sich selbst.
 *
 * Fuer die Folge {4000, 6000, 8000} x N gilt exakt:
 *   Mittelwert = 6000
 *   Minimum    = 4000, Maximum = 8000
 *   Standardabweichung = sqrt(((2000)^2 + 0 + (2000)^2) / 3)
 *                      = 2000 * sqrt(2/3) ~ 1632,993
 * Die Toleranz deckt nur die Wahl zwischen N und N-1 im Nenner ab; die
 * Groessenordnung ist damit gepinnt.
 */
void TestFluxVisualizer::die_statistik_stimmt_gegen_handrechnung()
{
    FluxVisualizerWidget w;
    w.setFluxData(mfm_strom(30));                          // 90 Werte

    const FluxStatistics s = w.getStatistics();
    QCOMPARE(s.sample_count, 90);
    QVERIFY2(std::fabs(s.mean_time - 6000.0) < 1.0,
             qPrintable(QStringLiteral("Mittelwert %1, erwartet 6000")
                        .arg(s.mean_time)));
    QVERIFY2(std::fabs(s.min_time - 4000.0) < 1.0,
             qPrintable(QStringLiteral("Minimum %1").arg(s.min_time)));
    QVERIFY2(std::fabs(s.max_time - 8000.0) < 1.0,
             qPrintable(QStringLiteral("Maximum %1").arg(s.max_time)));
    QVERIFY2(s.std_dev > 1500.0 && s.std_dev < 1700.0,
             qPrintable(QStringLiteral("Streuung %1, erwartet ~1633")
                        .arg(s.std_dev)));
}

/**
 * Jede der fuenf Ansichten wird wirklich gezeichnet.
 *
 * `QWidget::render()` ruft `paintEvent` auf. Ohne diesen Test bliebe der
 * gesamte Zeichenpfad ungelaufen — 1092 Zeilen, die nur der Uebersetzer
 * je gesehen hat.
 *
 * Geprueft wird nicht "sieht huebsch aus", sondern zwei nachpruefbare
 * Dinge: es stuerzt nicht ab, und die Flaeche ist danach **nicht mehr
 * einfarbig**. Eine Ansicht, die nichts zeichnet, faellt damit auf.
 */
void TestFluxVisualizer::jede_ansicht_zeichnet_wirklich()
{
    const QList<QPair<FluxViewMode, QString>> ansichten = {
        { FluxViewMode::WAVEFORM,    QStringLiteral("WAVEFORM") },
        { FluxViewMode::HISTOGRAM,   QStringLiteral("HISTOGRAM") },
        { FluxViewMode::SPECTROGRAM, QStringLiteral("SPECTROGRAM") },
        { FluxViewMode::CELL_VIEW,   QStringLiteral("CELL_VIEW") },
        { FluxViewMode::COMPARISON,  QStringLiteral("COMPARISON") },
    };

    for (const auto &a : ansichten) {
        FluxVisualizerWidget w;
        w.resize(640, 240);
        w.setFluxData(mfm_strom(200));
        w.addRevolution(mfm_strom(200));    // COMPARISON braucht zwei
        w.setCellTime(4000.0);
        w.setViewMode(a.first);
        w.setShowGrid(true);
        w.setShowStatistics(true);

        QImage bild(w.size(), QImage::Format_ARGB32);
        bild.fill(Qt::magenta);             // Fuellfarbe, die niemand malt
        w.render(&bild);

        // Wie viele verschiedene Farben stehen danach auf der Flaeche?
        QSet<QRgb> farben;
        for (int y = 0; y < bild.height() && farben.size() < 4; y += 3)
            for (int x = 0; x < bild.width() && farben.size() < 4; x += 3)
                farben.insert(bild.pixel(x, y));

        QVERIFY2(farben.size() >= 2,
                 qPrintable(QStringLiteral("Ansicht %1 hat nichts gezeichnet "
                                           "(%2 Farbe(n) auf der Flaeche)")
                            .arg(a.second).arg(farben.size())));
    }
}

/**
 * Mehrere Umdrehungen und markierte Bereiche ueberleben den Weg.
 *
 * `addRegion()` und `setMarkerPosition()` sind laut Eigentuemer-Vorgabe
 * die Fensterwahl fuer das CRC-Orakel. Sie muessen also den Aufruf
 * ueberstehen und die Anzeige nicht zerlegen.
 */
void TestFluxVisualizer::mehrere_umdrehungen_und_regionen_ueberleben()
{
    FluxVisualizerWidget w;
    w.resize(400, 200);
    w.setFluxData(mfm_strom(50));
    w.addRevolution(mfm_strom(50));
    w.addRevolution(mfm_strom(50));

    FluxRegion r;
    r.start_index = 10;
    r.end_index   = 40;
    r.label       = QStringLiteral("Fenster");
    r.color       = QColor(255, 0, 0, 80);
    w.addRegion(r);
    w.setMarkerPosition(25);
    w.zoomToFit();
    QVERIFY(w.zoom() > 0.0);

    QImage bild(w.size(), QImage::Format_ARGB32);
    bild.fill(Qt::magenta);
    w.render(&bild);                        // darf nicht abstuerzen

    const FluxStatistics s = w.getStatistics();
    QVERIFY2(s.sample_count > 0, "nach drei Umdrehungen keine Werte mehr");

    w.clearRegions();
    w.clearData();
    QCOMPARE(w.getStatistics().sample_count, 0);
}

/**
 * Der leere Fall. Ein Widget ohne Daten wird beim Verdrahten als Erstes
 * gezeigt — vor dem ersten Einlesen. Es darf dabei nicht abstuerzen.
 */
void TestFluxVisualizer::leere_daten_stuerzen_nicht_ab()
{
    FluxVisualizerWidget w;
    w.resize(320, 120);
    for (auto m : { FluxViewMode::WAVEFORM, FluxViewMode::HISTOGRAM,
                    FluxViewMode::SPECTROGRAM, FluxViewMode::CELL_VIEW,
                    FluxViewMode::COMPARISON }) {
        w.setViewMode(m);
        QImage bild(w.size(), QImage::Format_ARGB32);
        bild.fill(Qt::black);
        w.render(&bild);                    // kein Absturz, keine Zusicherung
    }
    QCOMPARE(w.getStatistics().sample_count, 0);
}

QTEST_MAIN(TestFluxVisualizer)
#include "test_flux_visualizer.moc"

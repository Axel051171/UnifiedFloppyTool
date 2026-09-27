// SPDX-License-Identifier: MIT
/**
 * @file test_otdr_panel_deepread.cpp
 * @brief Die DeepRead-Messwerte erreichen die Oberflaeche (MF-1430)
 *
 * Bis MF-1430 hatten die DeepRead-Forensikmodule keinen Aufrufer
 * (MF-627/MF-767). Seitdem ruft `UftOtdrPanel::analyzeFullDisk()`
 * Alterung und Nachbarspur-Korrelation. Dieser Test ist der
 * Bedienungs-Smoke-Test in Code: Datei laden, "Analyze All", Anzeige
 * lesen — derselbe Weg wie ein Klick.
 *
 * Erwartet an `gw_fm_acorn_3trk.scp` (gw-erzeugt, SCP-Spuren 0/2/4):
 *   - das Panel legt 5 SCP-Plaetze als 3 Zylinder x 2 Koepfe an; belegt
 *     sind C0/C1/C2 auf Kopf 0 -> 2 radiale Paare derselben Oberflaeche
 *   - mittlerer Rauschabstand 42,0 dB (vor MF-1430: 21,0, weil leere
 *     Plaetze als SNR 0 zaehlten — test_deepread_messung.c A1)
 *   - die Klassen stehen NUR mit dem Zusatz "Heuristik" da
 *   - "may_be_protection" erscheint nicht
 */

#include <QtTest/QtTest>
#include <QLabel>

#include "gui/uft_otdr_panel.h"

#ifndef UFT_CORPUS_FREE_DIR
#define UFT_CORPUS_FREE_DIR "tests/corpus_free"
#endif

class TestOtdrPanelDeepRead : public QObject {
    Q_OBJECT
private slots:
    void anzeigeNachAnalyzeAll();
};

void TestOtdrPanelDeepRead::anzeigeNachAnalyzeAll()
{
    UftOtdrPanel panel;
    auto *lbl = panel.findChild<QLabel *>(QStringLiteral("lblDeepReadDisk"));
    QVERIFY2(lbl, "Anzeige lblDeepReadDisk fehlt");
    QVERIFY(lbl->text().contains(QStringLiteral("Analyze All")));

    QVERIFY(panel.loadFluxImage(
        QStringLiteral(UFT_CORPUS_FREE_DIR "/gw_fm_acorn_3trk.scp")));
    panel.onAnalyzeAllClicked();

    const QString t = lbl->text();
    qInfo("%s", qPrintable(t));
    QVERIFY2(t.contains(QStringLiteral("SNR 42.0 dB")), qPrintable(t));
    QVERIFY2(t.contains(QStringLiteral("ueber 2 Paare")), qPrintable(t));
    QVERIFY2(t.contains(QStringLiteral("Heuristik (Schwellen ohne Quelle)")),
             qPrintable(t));
    QVERIFY2(!t.contains(QStringLiteral("rotect"), Qt::CaseInsensitive),
             qPrintable(t));
    /* Gemessen beim Anbinden: QStringLiteral mit \x-Bytes ergab "Â·" und
     * "â" in der Anzeige. U+00C2/U+00E2 kommen im Text sonst nicht vor. */
    QVERIFY2(!t.contains(QChar(0x00C2)) && !t.contains(QChar(0x00E2)),
             "UTF-8-Bytes als Einzelzeichen in der Anzeige");
    QVERIFY2(t.startsWith(QString::fromUtf8("DeepRead \xe2\x80\x94 ")),
             qPrintable(t));

    /* MF-1435: die Schreibnaht der gewaehlten Spur. Die gw-Aufnahme hat
     * zwei bitgleiche Umdrehungen — die Wiederkehr ist also 0 Zellen. */
    auto *naht = panel.findChild<QLabel *>(QStringLiteral("lblSplice"));
    QVERIFY2(naht, "Anzeige lblSplice fehlt");
    const QString n = naht->text();
    qInfo("Naht: %s", qPrintable(n));
    QVERIFY2(n.contains(QStringLiteral("nach Index")), qPrintable(n));
    QVERIFY2(n.contains(QString::fromUtf8("Wiederkehr \xc2\xb1" "0.0 Zellen (2 Umdr.)")),
             qPrintable(n));
    QVERIFY2(!n.contains(QChar(0x00C2)), "UTF-8-Bytes als Einzelzeichen");
}

QTEST_MAIN(TestOtdrPanelDeepRead)
#include "test_otdr_panel_deepread.moc"

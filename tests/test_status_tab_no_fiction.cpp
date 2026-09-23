/**
 * @file test_status_tab_no_fiction.cpp
 * @brief Drei Aussagen des Status-Reiters, die niemand nachgesehen hat
 *
 * ── Was hier bewacht wird ────────────────────────────────────────────────
 *
 * [[gui-statustab-decorative]] hat am 2026-09-05 fuenf Stellen gemessen.
 * Zwei davon sind seither behoben (die Belegungskarte, MF-569/576, haengt
 * an `test_status_tab_unknown_allocation.cpp`). Drei standen am 2026-09-16
 * unveraendert im Baum, und dieser Test nagelt sie fest.
 *
 * **1. Der Bootblock-Dialog parst nichts und sagt es nicht.**
 * `onBootblockClicked()` baut eine Tafel mit der Ueberschrift "--- Amiga
 * Bootblock (T0 S0-1) ---" und den Zeilen
 *
 *     Type:            DOS\x00 (OFS) or DOS\x01 (FFS)
 *     Checksum:        (parsed from bytes 4-7)
 *     Root Block:      880
 *
 * Gemessen hat der Dialog **kein einziges Byte der Diskette**:
 * `m_currentImage` ist ein `DecodeResult` mit Geometrie und Namen, ohne
 * Rohdaten. "Root Block: 880" ist damit keine Messung, sondern eine
 * Konstante — und fuer eine HD-ADF ist sie 1760, also schlicht falsch.
 * Der Hexdump zwei Bildschirmzeilen weiter macht es die ganze Zeit
 * richtig: er schreibt IN DIE ANZEIGE, dass er nichts zeigt.
 *
 * **2. Der Schutz-Dialog spricht frei, ohne geprueft zu haben.**
 * Sind `badSectors == 0` und keine WEAK-Sektoren gezaehlt, meldet der Kopf
 * gruen "No obvious copy protection detected". Das ist Abwesenheit von
 * Beweis als Beweis von Abwesenheit — genau die Klasse, die MF-570 im
 * Forensik-Reiter und MF-893 in `ToolsTab::onRepair()` beseitigt hat
 * ("das ist kein Freispruch"). Zwei gezaehlte Kennzahlen sind keine
 * Schutzanalyse: Fuzzy Bits, lange Spuren, No-Flux-Bereiche, Desync und
 * Illegal GCR kommen darin gar nicht vor.
 *
 * **3. Das Protokoll erreicht den Bildschirm nicht (P3-265).**
 * `appendLog()` besteht aus Zeitstempel, Praefix-Zeichen und `qDebug()`.
 * Es wird an **13** Stellen gerufen — und `forms/tab_status.ui` hat
 * gemessen kein Log-Widget (die Formulardatei fuehrt `textSectorInfo`,
 * `textHexDump`, zwei Fortschrittsbalken, fuenf Knoepfe, ein Etikett und
 * einen Schieber). Jede dieser 13 Meldungen ist fuer den Benutzer
 * unsichtbar. Ein Protokoll, das seinen Namen nicht haelt.
 *
 * ── Warum das kopflos geht ───────────────────────────────────────────────
 *
 * Wie bei MF-576: die Dialoge benutzen `show()`, nicht `exec()`, halten
 * den Lauf also nicht an. `onImageInfo()` ist ein OEFFENTLICHER Slot —
 * der Test reicht eine Geometrie hinein, ohne Datei und ohne Dateiauswahl.
 * Die Knopf-Behandler sind private Slots und werden ueber
 * `QMetaObject::invokeMethod` ausgeloest.
 */
#include <QtTest/QtTest>
#include <QDialog>
#include <QLabel>
#include <QRegularExpression>
#include <QTextEdit>

#include "statustab.h"
#include "decodejob.h"

class TestStatusTabNoFiction : public QObject
{
    Q_OBJECT

private:
    /** Eine Amiga-Diskette, wie sie der Reiter nach dem Dekodieren bekommt. */
    static DecodeResult amigaDisk()
    {
        DecodeResult r;
        r.formatName      = "ADF";
        r.platformName    = "Amiga";
        r.tracks          = 80;
        r.heads           = 2;
        r.sectorsPerTrack = 11;
        r.sectorSize      = 512;
        r.totalSize       = 901120;
        r.totalSectors    = 1760;
        r.goodSectors     = 1760;
        r.badSectors      = 0;
        return r;
    }

    /** Der zuletzt geoeffnete Dialog des Reiters. */
    static QDialog *openedDialog(StatusTab *tab)
    {
        const auto dlgs = tab->findChildren<QDialog *>();
        return dlgs.isEmpty() ? nullptr : dlgs.last();
    }

    /** Alles, was in einem Dialog als Text auf dem Schirm steht. */
    static QString screenText(QDialog *dlg)
    {
        QString s;
        for (auto *l : dlg->findChildren<QLabel *>())    s += l->text()        + "\n";
        for (auto *t : dlg->findChildren<QTextEdit *>()) s += t->toPlainText() + "\n";
        return s;
    }

private slots:

    /* ── 1. Der Bootblock behauptet keinen Wert, den er nicht gelesen hat ─ */
    void bootblockClaimsNoValueItHasNotRead()
    {
        StatusTab tab;
        tab.onImageInfo(amigaDisk());

        QVERIFY2(QMetaObject::invokeMethod(&tab, "onBootblockClicked"),
                 "Der Bootblock-Betrachter liess sich nicht ausloesen.");

        QDialog *dlg = openedDialog(&tab);
        QVERIFY2(dlg, "Es wurde kein Dialog geoeffnet.");

        const QString text = screenText(dlg);
        QVERIFY2(!text.isEmpty(), "Der Dialog zeigt gar nichts.");

        /* Der Kern des Befundes: eine Zahl, die wie eine Messung aussieht,
         * obwohl kein Byte der Diskette gelesen wurde. Geprueft wird die
         * ZUWEISUNG "Root Block: <zahl>", nicht die blosse Ziffernfolge —
         * 880 darf als Erklaerungstext durchaus vorkommen. */
        const QRegularExpression claimedRootBlock(
            QStringLiteral("Root Block:\\s*\\d"));
        QVERIFY2(!claimedRootBlock.match(text).hasMatch(),
                 qPrintable(QString("Der Dialog nennt einen Root Block als "
                                    "Wert, obwohl er den Bootblock nicht "
                                    "gelesen hat. Angezeigt:\n%1").arg(text)));

        /* Und der Vorbehalt gehoert IN die Anzeige, nicht in den Quelltext —
         * dieselbe Formulierung wie bei der Belegungskarte (MF-576). */
        QVERIFY2(text.contains("not read from this image"),
                 qPrintable(QString("Kein Vorbehalt in der Anzeige. "
                                    "Angezeigt:\n%1").arg(text)));
    }

    /* ── 2. Der Schutz-Dialog spricht nicht frei ──────────────────────────── */
    void protectionHeaderIsNotAClearance()
    {
        StatusTab tab;
        tab.onImageInfo(amigaDisk());   /* 0 bad sectors, 0 weak bits */

        QVERIFY2(QMetaObject::invokeMethod(&tab, "onProtectionClicked"),
                 "Die Schutzanalyse liess sich nicht ausloesen.");

        QDialog *dlg = openedDialog(&tab);
        QVERIFY2(dlg, "Es wurde kein Dialog geoeffnet.");

        const QString text = screenText(dlg);

        QVERIFY2(!text.contains("No obvious copy protection detected"),
                 qPrintable(QString("Der Dialog spricht die Diskette frei, "
                                    "nachdem er zwei Zaehler angesehen hat. "
                                    "Angezeigt:\n%1").arg(text)));

        /* Ein ehrlicher Kopf sagt, WAS er angesehen hat — und dass das
         * Ergebnis kein Freispruch ist. */
        QVERIFY2(text.contains("not a clearance"),
                 qPrintable(QString("Der Dialog sagt nicht, dass sein "
                                    "Ergebnis kein Freispruch ist. "
                                    "Angezeigt:\n%1").arg(text)));
    }

    /* ── 3. Das Protokoll erreicht den Bildschirm (P3-265) ────────────────── */
    void theLogReachesTheScreen()
    {
        StatusTab tab;

        /* `onImageInfo()` ruft appendLog("Image loaded: ...") — Zeile 626.
         * Wenn das Protokoll ein Widget hat, steht die Meldung danach dort. */
        tab.onImageInfo(amigaDisk());

        auto *log = tab.findChild<QTextEdit *>(QStringLiteral("textLog"));
        QVERIFY2(log, "Der Reiter hat kein Protokoll-Widget 'textLog' — "
                      "appendLog() schreibt seine 13 Meldungen nur nach "
                      "qDebug(), wo kein Benutzer sie sieht (P3-265).");

        QVERIFY2(log->toPlainText().contains("Image loaded"),
                 qPrintable(QString("Die Meldung steht nicht im Protokoll. "
                                    "Inhalt:\n%1").arg(log->toPlainText())));
    }
};

QTEST_MAIN(TestStatusTabNoFiction)
#include "test_status_tab_no_fiction.moc"

/**
 * @file test_status_tab_unknown_allocation.cpp
 * @brief Die Belegungskarte behauptet nichts mehr (MF-576)
 *
 * ── Was hier bewacht wird ────────────────────────────────────────────────
 *
 * Die BAM/FAT-Belegungskarte des Status-Reiters zeigte bis MF-569 **jede**
 * Diskette als vollstaendig frei: jeder Block gruen, Text "F", ueber alle
 * Spuren und Sektoren, mit echten Spur- und Sektorbeschriftungen.
 *
 * Im Quelltext stand "Placeholder: mark all as allocated for now" und "In
 * a full implementation, this would read from the actual BAM/FAT data".
 * Auf dem Bildschirm stand eine farbcodierte Belegungskarte.
 *
 * Ein Pruefer, der eine volle Diskette oeffnet und eine leere Karte sieht,
 * hat keinen Anlass zu zweifeln — die Tabelle sieht aus wie eine Messung.
 * Genau das meint „Keine erfundenen Daten".
 *
 * Der Bootsektor-Hexdump ein paar Zeilen weiter machte es die ganze Zeit
 * richtig: er schreibt IN DIE ANZEIGE, dass er nichts zeigt. Es ging also.
 *
 * ── Warum das kopflos geht ───────────────────────────────────────────────
 *
 * Die Dialoge des Status-Reiters benutzen `show()`, nicht `exec()` — sie
 * halten den Lauf also nicht an. Und `onImageInfo()` ist ein OEFFENTLICHER
 * Slot: der Test reicht eine Geometrie hinein, ohne Datei und ohne
 * Dateiauswahl-Dialog.
 *
 * Das ist der dritte Reiter mit Test (nach ToolsTab MF-574 und Explorer
 * MF-575). Der Baum hatte vor dieser Sitzung zwei Qt-Tests, beide auf dem
 * Hardware-Reiter — und genau deshalb ergaben MF-568 bis MF-570 sechs
 * Klasse-A-Befunde (BACKLOG C14).
 */
#include <QtTest/QtTest>
#include <QDialog>
#include <QLabel>
#include <QTableWidget>

#include "statustab.h"
#include "decodejob.h"

class TestStatusTabUnknownAllocation : public QObject
{
    Q_OBJECT

private:
    /** Eine Diskette, wie sie der Reiter nach dem Dekodieren bekommt. */
    static DecodeResult c64Disk()
    {
        DecodeResult r;
        r.formatName     = "D64";
        r.platformName   = "Commodore 64";
        r.tracks         = 35;
        r.heads          = 1;
        r.sectorsPerTrack = 21;
        r.sectorSize     = 256;
        r.totalSize      = 174848;
        r.totalSectors   = 683;
        r.goodSectors    = 683;
        return r;
    }

    /** Der zuletzt geoeffnete Dialog des Reiters. */
    static QDialog *openedDialog(StatusTab *tab)
    {
        const auto dlgs = tab->findChildren<QDialog *>();
        return dlgs.isEmpty() ? nullptr : dlgs.last();
    }

private slots:

    /* ── Kein Block behauptet "frei" ───────────────────────────────────── */
    void allocationMapClaimsNothing()
    {
        StatusTab tab;
        tab.onImageInfo(c64Disk());

        QVERIFY2(QMetaObject::invokeMethod(&tab, "onBAMViewerClicked"),
                 "Der BAM-Betrachter liess sich nicht ausloesen.");

        QDialog *dlg = openedDialog(&tab);
        QVERIFY2(dlg, "Es wurde kein Dialog geoeffnet.");

        auto *table = dlg->findChild<QTableWidget *>();
        QVERIFY2(table, "Im Dialog steht keine Tabelle.");

        /* Ohne Zeilen prueft der Rest nichts. */
        QVERIFY2(table->rowCount() > 0 && table->columnCount() > 1,
                 qPrintable(QString("Tabelle ist leer: %1 x %2")
                            .arg(table->rowCount()).arg(table->columnCount())));

        int freeClaims = 0, unknown = 0, cells = 0;
        for (int r = 0; r < table->rowCount(); r++) {
            for (int c = 0; c < table->columnCount(); c++) {
                auto *it = table->item(r, c);
                if (!it) continue;
                cells++;
                const QString t = it->text();
                if (t == "F") freeClaims++;
                if (t == "?") unknown++;
            }
        }

        QVERIFY2(cells > 0, "Kein einziges Feld gefuellt.");
        QVERIFY2(freeClaims == 0,
                 qPrintable(QString("%1 Felder behaupten 'F' (frei), obwohl "
                                    "die BAM nicht gelesen wird — das ist die "
                                    "Erfindung aus MF-569.").arg(freeClaims)));
        QVERIFY2(unknown == cells,
                 qPrintable(QString("Nur %1 von %2 Feldern sagen '?' — der "
                                    "Rest behauptet etwas anderes.")
                            .arg(unknown).arg(cells)));
    }

    /* ── Und der Vorbehalt steht IN der Anzeige ────────────────────────── */
    void theCaveatIsOnScreenNotOnlyInTheSource()
    {
        StatusTab tab;
        tab.onImageInfo(c64Disk());
        QVERIFY(QMetaObject::invokeMethod(&tab, "onBAMViewerClicked"));

        QDialog *dlg = openedDialog(&tab);
        QVERIFY(dlg);

        QString labels;
        for (auto *l : dlg->findChildren<QLabel *>())
            labels += l->text() + "\n";

        /* Das ist der Kern des Befundes: der Hinweis stand im QUELLTEXT,
         * also an der einen Stelle, wo ihn niemand liest, der das Werkzeug
         * BENUTZT. */
        QVERIFY2(labels.contains("not read from this image"),
                 qPrintable(QString("Kein Vorbehalt in der Anzeige. "
                                    "Gefundene Beschriftungen:\n%1")
                            .arg(labels)));
    }

    /* ── Die Karte haengt an der Geometrie, nicht an einer Annahme ─────── */
    void mapFollowsTheGeometryItWasGiven()
    {
        /* Eine andere Diskette muss eine andere Karte ergeben. Eine
         * Tabelle mit fester Groesse waere ein zweiter Platzhalter. */
        DecodeResult amiga;
        amiga.formatName      = "ADF";
        amiga.tracks          = 80;
        amiga.heads           = 2;
        amiga.sectorsPerTrack = 11;
        amiga.sectorSize      = 512;

        StatusTab tab;
        tab.onImageInfo(amiga);
        QVERIFY(QMetaObject::invokeMethod(&tab, "onBAMViewerClicked"));

        QDialog *dlg = openedDialog(&tab);
        QVERIFY(dlg);
        auto *table = dlg->findChild<QTableWidget *>();
        QVERIFY(table);

        /* 11 Sektoren je Spur plus die "Free"-Spalte. */
        QCOMPARE(table->columnCount(), 12);
        QVERIFY2(table->rowCount() >= 80,
                 qPrintable(QString("80 Spuren erwartet, %1 bekommen")
                            .arg(table->rowCount())));
    }
};

QTEST_MAIN(TestStatusTabUnknownAllocation)
#include "test_status_tab_unknown_allocation.moc"

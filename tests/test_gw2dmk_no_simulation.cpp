/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_gw2dmk_no_simulation.cpp
 * @brief Das GW-nach-DMK-Panel meldet kein Geraet und keine Aufnahme (MF-891)
 *
 * ── Was hier stand ───────────────────────────────────────────────────────
 *
 * `UftGw2DmkWorker::run()` war eine Schlafschleife mit fest verdrahteten
 * Ergebnissen. Der Quelltext sagte es selbst — `// Simulate device
 * detection`, `// Simulate reading`, `// Simulate track result`:
 *
 *     OP_DETECT     msleep(500)
 *                   -> deviceDetected("Greaseweazle F7 Plus v1.3 on
 *                                      /dev/ttyACM0")
 *     OP_READ_DISK  je Spur msleep(100)
 *                   -> trackRead(cyl, head, 10, errors)
 *                      errors = (cyl == 15 && head == 0) ? 1 : 0
 *                   -> operationComplete(true, "Disk read complete: <pfad>")
 *
 * Ein Geraetename mit Modell, Firmware-Fassung und Schnittstelle — alles
 * erfunden. Zehn Sektoren je Spur und ein Lesefehler auf Spur 15 — alles
 * erfunden. Und am Ende „Disk read complete" mit einem Dateipfad, unter dem
 * **keine Datei entsteht**: im ganzen Panel steht kein einziger Schreib-
 * aufruf (gemessen: `QFile`, `fopen`, `write(` -> nur ein
 * `QFileDialog::getSaveFileName`, dessen Ergebnis nirgends beschrieben
 * wird).
 *
 * Fuer einen Benutzer ist das von einer echten Aufnahme nicht zu
 * unterscheiden — bis er das Zielverzeichnis oeffnet.
 *
 * ── Warum hier nicht die echte Hardware verdrahtet wird ──────────────────
 *
 * Es gibt einen funktionierenden Greaseweazle-Weg: `GreaseweazleProviderV2`
 * ueber `src/hal/uft_greaseweazle_full.c`, production-wired und im
 * Hardware-Reiter erreichbar. Ihn hierher zu ziehen waere richtig — aber
 * nicht pruefbar: das Projekt hat **kein Geraet** (MF-310), und
 * `uft_greaseweazle_full.c` ist ein geschuetzter Pfad
 * (`.claude/CLAUDE.md` §2), der nicht ohne Rueckfrage angefasst wird.
 *
 * Also gilt die `honest-stub`-Regel, die derselbe Text fuer unverdrahtete
 * Hardware-Pfade ausdruecklich vorsieht: ein Fehler mit Begruendung, nie
 * ein stiller Erfolg. Vorbild im Baum: `src/xcopytab.cpp:145-162` — Knopf
 * abgeschaltet, Grund im Tooltip, die UI-Form bleibt sichtbar.
 */
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QPushButton>

#include "uft_gw2dmk_panel.h"

class TestGw2DmkNoSimulation : public QObject
{
    Q_OBJECT

private slots:

    /* ── Kein erfundener Geraetename ──────────────────────────────────── */
    void detectReportsNoDeviceInsteadOfInventingOne()
    {
        UftGw2DmkWorker w;
        QSignalSpy gefunden(&w, &UftGw2DmkWorker::deviceDetected);
        QSignalSpy fehler(&w, &UftGw2DmkWorker::deviceError);

        w.setOperation(UftGw2DmkWorker::OP_DETECT);
        w.start();
        QVERIFY(w.wait(5000));

        if (gefunden.count() > 0) {
            QFAIL(qPrintable(QString("Ein Geraet wurde gemeldet, obwohl "
                                     "keines befragt wurde: \"%1\"")
                             .arg(gefunden.at(0).at(0).toString())));
        }
        QVERIFY2(fehler.count() == 1,
                 "Die Suche muss einen Fehler MIT BEGRUENDUNG melden, nicht "
                 "schweigen.");
        QVERIFY2(!fehler.at(0).at(0).toString().isEmpty(),
                 "Der Fehler traegt keine Begruendung.");
    }

    /* ── Kein „Disk read complete" ohne Datei ─────────────────────────── */
    void readDiskDoesNotReportSuccess()
    {
        UftGw2DmkWorker w;
        QSignalSpy fertig(&w, &UftGw2DmkWorker::operationComplete);
        QSignalSpy spuren(&w, &UftGw2DmkWorker::trackRead);

        w.setOperation(UftGw2DmkWorker::OP_READ_DISK);
        w.setTrackRange(0, 4);
        w.setHeads(2);
        w.setOutputPath("/nirgendwo/test.dmk");
        w.start();
        QVERIFY(w.wait(10000));

        QVERIFY2(fertig.count() == 1, "Es kam kein Abschluss.");
        QVERIFY2(fertig.at(0).at(0).toBool() == false,
                 qPrintable(QString("Erfolg gemeldet, ohne dass eine Datei "
                                    "entsteht: \"%1\"")
                            .arg(fertig.at(0).at(1).toString())));
        QVERIFY2(!fertig.at(0).at(1).toString().isEmpty(),
                 "Der Abschluss traegt keine Begruendung.");

        /* Und es duerfen keine Spurergebnisse gemeldet werden — die waren
         * fest verdrahtet (10 Sektoren, ein Fehler auf Spur 15). */
        QVERIFY2(spuren.count() == 0,
                 qPrintable(QString("%1 Spurergebnis(se) gemeldet, obwohl "
                                    "keine Spur gelesen wurde.")
                            .arg(spuren.count())));
    }

    /* ── Dasselbe fuer die Einzelspur ─────────────────────────────────── */
    void readTrackDoesNotReportSuccess()
    {
        UftGw2DmkWorker w;
        QSignalSpy fertig(&w, &UftGw2DmkWorker::operationComplete);
        QSignalSpy spuren(&w, &UftGw2DmkWorker::trackRead);

        w.setOperation(UftGw2DmkWorker::OP_READ_TRACK);
        w.start();
        QVERIFY(w.wait(5000));

        QVERIFY2(fertig.count() == 1 && fertig.at(0).at(0).toBool() == false,
                 "Auch die Einzelspur meldet Erfolg ohne Tat.");
        QCOMPARE(spuren.count(), 0);
    }

    /* ── Und die Knoepfe sagen es, bevor man sie drueckt ──────────────── */
    void theActionButtonsAreDisabledWithAReason()
    {
        UftGw2DmkPanel panel;

        const QStringList erwartet{"Detect", "Read Disk", "Read Track"};
        int geprueft = 0;

        for (QPushButton *b : panel.findChildren<QPushButton *>()) {
            if (!erwartet.contains(b->text())) continue;
            geprueft++;
            QVERIFY2(!b->isEnabled(),
                     qPrintable(QString("Der Knopf \"%1\" ist bedienbar, "
                                        "obwohl kein Geraet angesprochen "
                                        "wird.").arg(b->text())));
            QVERIFY2(!b->toolTip().isEmpty(),
                     qPrintable(QString("Der Knopf \"%1\" ist abgeschaltet, "
                                        "sagt aber nicht warum.")
                                .arg(b->text())));
        }
        QCOMPARE(geprueft, erwartet.size());
    }
};

QTEST_MAIN(TestGw2DmkNoSimulation)
#include "test_gw2dmk_no_simulation.moc"

/**
 * @file test_honest_stubs_stay_honest.cpp
 * @brief Ein ehrlicher Stub, der aufhoert ehrlich zu sein (MF-577)
 *
 * ── Warum ausgerechnet diese beiden ──────────────────────────────────────
 *
 * Der Workflow- und der XCopy-Reiter waren in der Pruefsitzung MF-568…576
 * die einzigen, an denen NICHTS zu beanstanden war. Beide sagen von sich
 * aus, was sie nicht koennen:
 *
 *     XCopy     `btnStartCopy` ist ABGESCHALTET, mit einem Tooltip, der
 *               nennt was fehlt (Virenscanner, Bootblock-Analyse,
 *               BAMCOPY, AmigaDOS-Sektorkopie) und wohin es gehoert
 *               (docs/XCOPY_INTEGRATION_TODO.md, v4.2.0).
 *
 *     Workflow  der Start-Knopf haengt an der Quelle/Ziel-Kombination,
 *               und der Tooltip nennt den Grund, wenn er aus ist.
 *
 * Das ist die `honest-stub`-Konvention des Projekts, richtig angewandt:
 * eine architektonische Luecke mit Plan, kein Lazy-Stub.
 *
 * ── Was daran zu bewachen ist ────────────────────────────────────────────
 *
 * Genau das. Diese Sitzung hat sechs Klasse-A-Befunde gefunden, und jeder
 * einzelne war eine Stelle, die einmal ehrlich gemeint war und dann
 * anfing, etwas zu behaupten:
 *
 *     "For now, just copy the file"      -> "Conversion complete!"
 *     "sample entries based on format"   -> eine Dateiliste
 *     "mark all as allocated for now"    -> eine gruene Belegungskarte
 *
 * Der Weg von „ehrlich unfertig" zu „behauptet etwas" ist kurz, und
 * niemand geht ihn absichtlich. Ein abgeschalteter Knopf, den jemand
 * beim Aufraeumen einschaltet, ist genau dieser Schritt.
 *
 * Dieser Test haelt die zwei sauberen Reiter dort fest, wo sie sind.
 *
 * ── Was er NICHT prueft ──────────────────────────────────────────────────
 *
 * Den Ablehnungs-Dialog des Workflow-Reiters (Disk-zu-Disk-Fluss). Der
 * kommt als `QMessageBox::warning`, also modal, und ein modaler Dialog
 * haelt einen kopflosen Lauf an. Geprueft wird die Torwaechter-Logik
 * davor, die ohne Dialog auskommt.
 */
#include <QtTest/QtTest>
#include <QPushButton>

#include "xcopytab.h"
#include "workflowtab.h"

class TestHonestStubsStayHonest : public QObject
{
    Q_OBJECT

private slots:

    /* ── XCopy: der Knopf bleibt aus, und sagt warum ───────────────────── */
    void xcopyStartStaysDisabledAndSaysWhy()
    {
        XCopyTab tab;
        auto *btn = tab.findChild<QPushButton *>("btnStartCopy");
        QVERIFY2(btn, "btnStartCopy ist nicht erreichbar — dann prueft "
                      "dieser Test nichts.");

        QVERIFY2(!btn->isEnabled(),
                 "Der XCopy-Start ist eingeschaltet. Der Reiter hat aber "
                 "keinen Amiga-XCopy-Unterbau: er wuerde eine generische "
                 "Dateikopie als XCopy ausgeben.");

        /* Abgeschaltet allein reicht nicht — ein Knopf, der ohne Grund
         * aus ist, sieht aus wie ein Fehler und wird eingeschaltet. Der
         * Tooltip ist der Grund. */
        const QString tip = btn->toolTip();
        QVERIFY2(!tip.isEmpty(),
                 "Der abgeschaltete Knopf nennt keinen Grund.");
        QVERIFY2(tip.contains("XCOPY_INTEGRATION_TODO") ||
                 tip.contains("planned"),
                 qPrintable("Der Tooltip sagt nicht, was fehlt und wohin es "
                            "gehoert:\n" + tip));
        QVERIFY2(tip.contains("file-copy") || tip.contains("generic"),
                 qPrintable("Der Tooltip sagt nicht, was der Reiter STATT "
                            "XCopy tut — und genau das waere die "
                            "Verwechslung:\n" + tip));
    }

    /* ── Workflow: ohne Quelldatei kein Start, und der Grund steht dran ── */
    void workflowStartIsGatedAndSaysWhy()
    {
        WorkflowTab tab;
        auto *btn = tab.findChild<QPushButton *>("btnStartAbort");
        QVERIFY2(btn, "btnStartAbort ist nicht erreichbar.");

        /* Frisch gebaut ist keine Datei gewaehlt. Der Knopf muss aus sein
         * und sagen, was fehlt. */
        QVERIFY2(!btn->isEnabled(),
                 "Der Start ist eingeschaltet, obwohl weder Quelle noch "
                 "Ziel gewaehlt sind.");

        const QString tip = btn->toolTip();
        QVERIFY2(!tip.isEmpty(),
                 "Der abgeschaltete Start-Knopf nennt keinen Grund.");
        QVERIFY2(tip.contains("Select") || tip.contains("Invalid"),
                 qPrintable("Der Tooltip nennt nicht, was fehlt:\n" + tip));
    }

    /* ── Und der Pause-Knopf, der nur waehrend eines Laufs gilt ────────── */
    void workflowPauseIsOffWhenNothingRuns()
    {
        /* Kleiner Fall, aber dieselbe Klasse: ein bedienbarer Knopf, der
         * nichts zu tun hat, verspricht eine Wirkung. */
        WorkflowTab tab;
        auto *pause = tab.findChild<QPushButton *>("btnPause");
        QVERIFY2(pause, "btnPause ist nicht erreichbar.");
        QVERIFY2(!pause->isEnabled(),
                 "Pause ist bedienbar, obwohl nichts laeuft.");
    }
};

QTEST_MAIN(TestHonestStubsStayHonest)
#include "test_honest_stubs_stay_honest.moc"

/**
 * @file test_tools_tab_convert.cpp
 * @brief Der Konvertieren-Knopf, kopflos angetrieben (MF-574)
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * MF-568 hat `ToolsTab::onConvert()` umgebaut. Vorher stand dort
 * woertlich:
 *
 *     // For now, just copy the file
 *     if (QFile::copy(source, target))
 *         appendOutput(tr("Conversion complete!"));
 *
 * Wer SCP -> D64 waehlte, bekam eine byteweise Kopie der SCP-Datei unter
 * dem Namen `.d64` — und die Meldung, es sei fertig. Seither geht der
 * Knopf durch `uft_convert_file()`, also durch das Preflight-Tor.
 *
 * **Geklickt hat das niemand.** Der Baum hat 15 216 Zeilen Oberflaeche und
 * zwei Qt-Tests, beide auf dem Hardware-Reiter (BACKLOG C14). Genau
 * deshalb ergaben MF-568 und MF-569 in einer einzigen Pruefrunde vier
 * Klasse-A-Befunde.
 *
 * Der Bedien-Nachweis war als „nur der Eigentuemer kann klicken" (D4)
 * gefuehrt. Das war zu pessimistisch — dieselbe Fehleinordnung wie bei C2
 * („blockiert — Hardware-Sitzung", und dann nahm die Funktion gar keine
 * Hardware). Der Baum hat kopflose Qt-Tests, `QT_QPA_PLATFORM=offscreen`,
 * und sie laufen in ctest.
 *
 * ── Was hier geprueft wird ───────────────────────────────────────────────
 *
 * Nicht „der Knopf existiert", sondern die drei Zusagen aus MF-568:
 *
 *   1. Eine abgelehnte Wandlung **schreibt nichts** und behauptet nichts.
 *      Der alte Code haette hier eine Kopie hinterlassen — das ist der
 *      Rotbeweis, den dieser Test dauerhaft festhaelt.
 *   2. Die Zielliste kommt aus der **Maschine**, nicht aus der geloeschten
 *      Handliste. `SCP -> ATR` und `SCP -> WOZ` standen dort und gibt es
 *      nicht.
 *   3. Ein unbekanntes Zielformat wird abgelehnt, nicht geraten.
 *
 * ── Warum kein verlustbehafteter Fall ────────────────────────────────────
 *
 * Bei Verlust fragt `onConvert()` per `QMessageBox::question` nach. Ein
 * modaler Dialog haelt einen kopflosen Lauf an. Gewaehlt sind darum Paare,
 * die das Tor als UNGEPRUEFT abweist — dort erscheint kein Dialog.
 *
 * Dass die Zustimmung ueberhaupt verlangt wird, ist an anderer Stelle
 * belegt: `tests/test_destructive_op_consent.c` und
 * `tests/test_convert_table_has_dispatch.c`.
 */
#include <QtTest/QtTest>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextEdit>

#include "toolstab.h"

class TestToolsTabConvert : public QObject
{
    Q_OBJECT

private:
    /** Der Ausgabetext des Reiters, egal ob QTextEdit oder QPlainTextEdit. */
    static QString outputOf(ToolsTab *tab)
    {
        if (auto *te = tab->findChild<QTextEdit *>("textOutput"))
            return te->toPlainText();
        if (auto *pe = tab->findChild<QPlainTextEdit *>("textOutput"))
            return pe->toPlainText();
        return QString();
    }

private slots:

    /* ── Zusage 1: eine abgelehnte Wandlung hinterlaesst nichts ────────── */
    void refusedConversionWritesNothing()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        /* Eine Quelle mit erkennbarem Inhalt. Der alte Code haette genau
         * diese Bytes unter dem Zielnamen abgelegt. */
        const QByteArray payload(4096, '\x5A');
        const QString src = dir.filePath("quelle.bin");
        {
            QFile f(src);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QCOMPARE(f.write(payload), qint64(payload.size()));
        }

        const QString dst = dir.filePath("ziel.img");

        ToolsTab tab;
        auto *editSrc = tab.findChild<QLineEdit *>("editConvertSource");
        auto *editDst = tab.findChild<QLineEdit *>("editConvertTarget");
        auto *btn     = tab.findChild<QPushButton *>("btnConvert");
        QVERIFY2(editSrc && editDst && btn,
                 "Die Objektnamen aus forms/tab_tools.ui muessen erreichbar "
                 "sein — ohne sie prueft dieser Test nichts.");

        editSrc->setText(src);
        editDst->setText(dst);

        btn->click();

        /* Der Kern: keine Datei. Mit `QFile::copy()` laege hier eine
         * 4096-Byte-Kopie der Quelle unter dem Namen `ziel.img`. */
        QVERIFY2(!QFile::exists(dst),
                 "Eine abgelehnte Wandlung hat eine Datei hinterlassen — "
                 "genau das Verhalten, das MF-568 entfernt hat.");

        const QString out = outputOf(&tab);
        QVERIFY2(!out.contains("Conversion complete"),
                 qPrintable("Erfolg gemeldet, obwohl nichts geschrieben "
                            "wurde:\n" + out));
        QVERIFY2(out.contains("refused") || out.contains("failed") ||
                 out.contains("Unknown target format"),
                 qPrintable("Kein Wort dazu, warum es nicht ging:\n" + out));
    }

    /* ── Zusage 2: die Zielliste kommt aus der Maschine ────────────────── */
    void targetListComesFromTheEngine()
    {
        ToolsTab tab;
        auto *from = tab.findChild<QComboBox *>("comboConvertFrom");
        auto *to   = tab.findChild<QComboBox *>("comboConvertTo");
        QVERIFY(from && to);

        const int i = from->findText("SCP");
        QVERIFY2(i >= 0, "SCP steht als Quellformat im Formular");
        from->setCurrentIndex(i);

        QStringList targets;
        for (int k = 0; k < to->count(); k++) targets << to->itemText(k);

        /* Diese vier standen in der geloeschten Handliste
         * `m_conversionMap` und gibt es in der Wandlungstabelle nicht.
         * Tauchen sie wieder auf, ist die Handliste zurueck. */
        for (const QString &phantom : {"ATR", "WOZ", "TAP", "ADZ"}) {
            QVERIFY2(!targets.contains(phantom),
                     qPrintable(QString("Ziel '%1' wird angeboten, obwohl die "
                                        "Maschine es nicht kennt. Angeboten: %2")
                                .arg(phantom, targets.join(", "))));
        }

        /* Und die Gegenprobe: der Test misst nur etwas, wenn ueberhaupt
         * eine Liste da ist. Eine leere Liste bestuende die Pruefung
         * darueber trivial. */
        QVERIFY2(!targets.isEmpty(),
                 "Fuer SCP wird gar kein Ziel angeboten — dann prueft der "
                 "Phantom-Test oben nichts.");
    }

    /* ── Zusage 3: ein unbekanntes Ziel wird nicht geraten ─────────────── */
    void unknownTargetIsRefused()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString src = dir.filePath("quelle.bin");
        {
            QFile f(src);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QCOMPARE(f.write(QByteArray(2048, '\x11')), qint64(2048));
        }
        /* Eine Endung, die `uft_format_from_name()` nicht kennt. */
        const QString dst = dir.filePath("ziel.gibtesnicht");

        ToolsTab tab;
        auto *editSrc = tab.findChild<QLineEdit *>("editConvertSource");
        auto *editDst = tab.findChild<QLineEdit *>("editConvertTarget");
        auto *to      = tab.findChild<QComboBox *>("comboConvertTo");
        auto *btn     = tab.findChild<QPushButton *>("btnConvert");
        QVERIFY(editSrc && editDst && to && btn);

        editSrc->setText(src);
        editDst->setText(dst);
        /* Damit auch das Kuerzel aus der Auswahlliste nicht traegt. */
        to->clear();
        to->addItem("GIBTESNICHT");

        btn->click();

        QVERIFY2(!QFile::exists(dst),
                 "Bei unbekanntem Zielformat wurde trotzdem geschrieben.");
        const QString out = outputOf(&tab);
        QVERIFY2(out.contains("Unknown target format"),
                 qPrintable("Das unbekannte Zielformat wird nicht benannt:\n"
                            + out));
    }
};

QTEST_MAIN(TestToolsTabConvert)
#include "test_tools_tab_convert.moc"

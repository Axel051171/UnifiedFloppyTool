/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_disk_analyzer_no_fiction.cpp
 * @brief Der Sektorbericht erfindet keine CRC-Urteile mehr (MF-890)
 *
 * ── Was hier stand ───────────────────────────────────────────────────────
 *
 * `DiskAnalyzerWindow::updateSectorInfo()` baute seinen ganzen Bericht aus
 * einer festen Zeichenkette. Eingesetzt wurden GENAU drei Werte — Sektor,
 * Spur, Seite. Alles andere war fuer jede Spur jeder Diskette dasselbe:
 *
 *     Size: 00256 (ID: 0x01)
 *     Data checksum: 0x5600 (OK)
 *     Head CRC: 0x3FFF (BAD CRC!)
 *     Data CRC: 0xFFFF (BAD CRC!)
 *     Start sector cell: 95821
 *     Start sector Data cell: 96525
 *     End sector cell: 200
 *     Number of cells: 4896
 *
 * Zwei erfundene CRC-FEHLER, eine erfundene Pruefsumme, vier erfundene
 * Zellpositionen. Und der Text ging woertlich in den Bericht:
 * `onExportClicked()` schreibt `ui->textSectorInfo->toPlainText()` unter
 * der Ueberschrift „Current Sector" in die HTML- UND die Textfassung
 * (`src/diskanalyzerwindow.cpp`).
 *
 * Damit war es nicht nur eine falsche Anzeige, sondern ein falscher
 * **Befund in einem ausgelieferten Dokument**. Das Original wandert danach
 * in den Container, und die Erfindung ist ab da die einzige Wahrheit.
 *
 * Bemerkenswert: der Rest des Fensters war ehrlich. Die Geometrie kommt
 * aus `uft_disk_open()`, die CRC32 wird ueber die Datei gerechnet, und
 * `labelSide0Format` sagt seit MF-662 ausdruecklich „nicht ermittelt",
 * statt „ISO MFM" zu raten. Nur dieser eine Kasten fabrizierte.
 *
 * ── Was ein D64 ueber CRCs sagen kann: nichts ────────────────────────────
 *
 * Gemessen am Korpus-Abbild `vice_c1541_35trk.d64` ueber den echten
 * Plugin-Pfad (`uft_format_plugin_d64.read_track`):
 *
 *     Zylinder 0: 21 Sektoren, IDs 0..20, C=0 H=0 N=1, je 256 Byte
 *     crc_ok = 1,  crc_stored = 0x0,  crc_calculated = 0x0
 *
 * Das Plugin setzt `crc_ok` auf wahr, OHNE dass eine Pruefsumme existiert
 * — ein D64 ist ein Sektorabbild und traegt keine. „Data CRC: OK" waere
 * dort dieselbe Erfindung wie „BAD CRC!", nur in die andere Richtung.
 * Der Bericht nennt CRC-Werte darum nur, wenn das Plugin welche geliefert
 * hat, und sagt sonst, dass dieses Format keine fuehrt.
 *
 * Die 21 Sektoren sind keine Ablesung aus dem Code, sondern eine
 * Struktureigenschaft von CBM DOS: Zone 0 (Spuren 1-17) hat 21 Sektoren.
 * Deshalb darf der Test darauf bestehen.
 */
#include <QtTest/QtTest>
#include <QTextEdit>
#include <QSpinBox>

#include "diskanalyzerwindow.h"

#include <uft/uft_format_plugin.h>

class TestDiskAnalyzerNoFiction : public QObject
{
    Q_OBJECT

private:
    static QString korpusD64()
    {
#ifdef UFT_CORPUS_DIR
        return QString(UFT_CORPUS_DIR) + "/vice_c1541_35trk.d64";
#else
        return QString();
#endif
    }

    /** Laedt das Abbild und tut, was ein Benutzer tut: eine Spur waehlen.
     *
     *  `updateSectorInfo()` haengt an `onTrackChanged()`; nach dem Laden
     *  allein steht im Kasten noch der Platzhalter aus dem .ui (lauter
     *  Striche). Ein Test, der nur laedt, prueft den Platzhalter — das ist
     *  genau die Zusicherung, die nicht feuern kann. */
    static QString sektorTextNachSpurwahl(DiskAnalyzerWindow *w,
                                          const QString &img, int spur)
    {
        w->loadImage(img);
        auto *spin = w->findChild<QSpinBox *>("spinTrackNumber");
        if (!spin) return QString();
        spin->setValue(spur);            /* loest onTrackChanged aus */
        auto *t = w->findChild<QTextEdit *>("textSectorInfo");
        return t ? t->toPlainText() : QString();
    }

private slots:

    /** Was `main()` tut, bevor irgendein Fenster aufgeht.
     *
     *  MF-447: ohne diesen Aufruf ist die Plugin-Registry LEER und
     *  `uft_disk_open()` liefert fuer jede Datei NULL. Ein Test, der das
     *  vergisst, misst den Fehlerpfad und nennt ihn Ergebnis — das ist
     *  hier beim ersten Lauf genau passiert. */
    void initTestCase()
    {
        QCOMPARE(uft_register_all_formats(), UFT_OK);
    }

    /* ── Die acht erfundenen Zeilen sind weg ───────────────────────────── */
    void theInventedSectorReportIsGone()
    {
        const QString img = korpusD64();
        if (img.isEmpty() || !QFile::exists(img))
            QSKIP("Korpus-Abbild vice_c1541_35trk.d64 fehlt");

        DiskAnalyzerWindow w;
        const QString text = sektorTextNachSpurwahl(&w, img, 1);
        QVERIFY2(!text.isEmpty(),
                 "Der Sektorkasten ist leer - dann prueft dieser Test nichts.");

        /* Die Konstanten aus der entfernten Erfindung. Sie sind
         * eindeutig genug, um nicht zufaellig zu entstehen. */
        for (const QString &fiktion : {"0x5600", "0x3FFF", "95821",
                                       "96525", "4896"}) {
            QVERIFY2(!text.contains(fiktion),
                     qPrintable(QString("Der erfundene Wert '%1' steht wieder "
                                        "im Sektorbericht:\n%2")
                                .arg(fiktion, text)));
        }
    }

    /* ── Und es steht drin, was wirklich auf der Spur liegt ────────────── */
    void theReportShowsWhatTheTrackActuallyHas()
    {
        const QString img = korpusD64();
        if (img.isEmpty() || !QFile::exists(img))
            QSKIP("Korpus-Abbild vice_c1541_35trk.d64 fehlt");

        DiskAnalyzerWindow w;
        const QString text = sektorTextNachSpurwahl(&w, img, 1);

        /* CBM DOS Zone 0 (Spuren 1-17) hat 21 Sektoren. Das ist eine
         * Eigenschaft des Formats, keine Ablesung aus unserem Code. */
        QVERIFY2(text.contains("21 Sektoren"),
                 qPrintable(QString("Zylinder 0 einer 1541-Diskette hat 21 "
                                    "Sektoren; der Bericht nennt sie nicht:"
                                    "\n%1").arg(text)));
    }

    /* ── Kein CRC-Urteil ueber ein Format, das keine CRC fuehrt ────────── */
    void noCrcVerdictForAFormatThatStoresNone()
    {
        const QString img = korpusD64();
        if (img.isEmpty() || !QFile::exists(img))
            QSKIP("Korpus-Abbild vice_c1541_35trk.d64 fehlt");

        DiskAnalyzerWindow w;
        const QString text = sektorTextNachSpurwahl(&w, img, 1);

        /* Gemessen: das D64-Plugin liefert crc_stored = crc_calculated = 0.
         * Ein Urteil in EINE der beiden Richtungen waere erfunden. */
        QVERIFY2(!text.contains("BAD CRC"),
                 qPrintable("Ein CRC-Fehler wird behauptet, obwohl ein D64 "
                            "keine Pruefsumme traegt:\n" + text));
        QVERIFY2(!text.contains("Data CRC: 0x"),
                 qPrintable("Ein CRC-Wert wird genannt, den das Plugin nicht "
                            "geliefert hat:\n" + text));
    }
};

QTEST_MAIN(TestDiskAnalyzerNoFiction)
#include "test_disk_analyzer_no_fiction.moc"

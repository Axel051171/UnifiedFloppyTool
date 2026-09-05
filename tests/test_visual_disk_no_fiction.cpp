/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_visual_disk_no_fiction.cpp
 * @brief Die Belegungskarte zeigt die Diskette, nicht eine Vorlage (MF-892)
 *
 * ── Drei Erfindungen in einem Fenster ────────────────────────────────────
 *
 * 1. `generateSampleData()` — im KONSTRUKTOR gerufen und als Rueckfall.
 *    80 Spuren x 2 Seiten x 18 Sektoren, dazu erfundene Fehlstellen bei
 *    15/3, 42/7, 71/12 und schwache Bits bei 20/5 und 55/2, alle CRCs
 *    `0x1234`/`0x5678`. Der Quelltext sagt es selbst: „Simulate some bad
 *    sectors for visual effect". Folge: ein frisch geoeffnetes Fenster
 *    meldete „Side 0, 80 Tracks | 1440 Sectors, 3 bad | 737280 Bytes",
 *    BEVOR irgendetwas geladen war.
 *
 * 2. Der ERFOLGSPFAD von `loadDiskImage()`. Er oeffnet die Datei mit
 *    `uft_disk_open()`, nimmt die Geometrie — und ruft NIE `read_track()`.
 *    Jeder Sektor wird `SectorGood`, beide CRC-Flaggen wahr, das Format
 *    fest „ISO MFM". Die Karte war gruen, unabhaengig davon, was auf der
 *    Diskette steht: eine Unbedenklichkeitsbescheinigung ohne Messung.
 *    Dieselbe Klasse wie MF-570 — und das Gegenstueck zu MF-890, wo
 *    derselbe Baum CRC-FEHLER erfunden hat.
 *
 * 3. `updateHexDump()` erzeugt die Rampe `00 01 02 03 ...`, wenn keine
 *    Daten da sind — und `SectorInfo::data` wird im ganzen Fenster
 *    NIRGENDS zugewiesen (gemessen). Der „Sektorinhalt" war also immer
 *    dieselbe Rampe, fuer jeden Sektor jeder Diskette.
 *
 * ── Warum der Sektorinhalt der schaerfste Pruefstein ist ─────────────────
 *
 * Er ist unabhaengig nachrechenbar. Der Test nimmt den BAM-Sektor —
 * CBM-Spur 18, Sektor 0 —, dessen Versatz sich aus der Zonenaufteilung
 * ergibt: die Spuren 1..17 haben je 21 Sektoren zu 256 Byte, also
 * 17 * 21 * 256 = 0x16500. Er liest diese Bytes SELBST aus der Datei und
 * haelt sie gegen das, was das Fenster anzeigt.
 *
 * Bewusst nicht Versatz 0: dort steht auf einer frisch formatierten
 * Diskette nur 00 00 00 ..., und eine Zusicherung gegen lauter Nullen
 * traegt kaum. Der BAM beginnt mit 12 01 41 und ist unterscheidbar.
 *
 * Damit prueft der Test nicht „hat sich etwas geaendert", sondern „steht
 * da, was auf der Diskette steht".
 */
#include <QtTest/QtTest>
#include <QTextEdit>
#include <QLabel>
#include <QFile>

#include "visualdiskdialog.h"

#include <uft/uft_format_plugin.h>

class TestVisualDiskNoFiction : public QObject
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

    static QString kindText(QObject *o, const char *name)
    {
        if (auto *t = o->findChild<QTextEdit *>(QString::fromLatin1(name)))
            return t->toPlainText();
        if (auto *l = o->findChild<QLabel *>(QString::fromLatin1(name)))
            return l->text();
        return QString();
    }

private slots:

    /** Was `main()` tut. Ohne das ist die Registry leer und
     *  `uft_disk_open()` liefert NULL (MF-447). */
    void initTestCase()
    {
        QCOMPARE(uft_register_all_formats(), UFT_OK);
    }

    /* ── Ein frisches Fenster behauptet keine Diskette ─────────────────── */
    void afreshDialogClaimsNoDisk()
    {
        VisualDiskDialog d;      /* nichts geladen */

        const QString info = kindText(&d, "labelSide0Info");
        QVERIFY2(!info.isEmpty(),
                 "Die Infozeile ist leer - dann prueft dieser Test nichts.");

        /* „3 bad" stammt aus den drei erfundenen Fehlstellen; „1440
         * Sectors" aus 80 x 18. Beides ohne geladenes Abbild. */
        QVERIFY2(!info.contains("3 bad"),
                 qPrintable("Ohne geladenes Abbild werden drei fehlerhafte "
                            "Sektoren gemeldet:\n" + info));
        QVERIFY2(!info.contains("1440"),
                 qPrintable("Ohne geladenes Abbild wird eine vollstaendige "
                            "Sektorzahl gemeldet:\n" + info));
    }

    /* ── Ohne Daten wird keine Rampe erfunden ──────────────────────────── */
    void anEmptySectorSaysSoInsteadOfShowingARamp()
    {
        VisualDiskDialog d;
        const QString hex = kindText(&d, "textHexDump");
        QVERIFY2(!hex.contains("00 01 02 03 04 05 06 07"),
                 qPrintable("Ohne geladenes Abbild zeigt der Hexbereich eine "
                            "erzeugte Rampe:\n" + hex));
    }

    /* ── Der Sektorinhalt kommt aus dem Abbild ─────────────────────────── */
    void theHexDumpShowsTheImageNotARamp()
    {
        const QString img = korpusD64();
        if (img.isEmpty() || !QFile::exists(img))
            QSKIP("Korpus-Abbild vice_c1541_35trk.d64 fehlt");

        /* Der BAM-Sektor: CBM-Spur 18, Sektor 0 — also Zylinder 17 in der
         * Zaehlung dieses Fensters. Sein Versatz ist unabhaengig
         * ausrechenbar: die Spuren 1..17 haben je 21 Sektoren zu 256 Byte,
         * also 17 * 21 * 256 = 0x16500.
         *
         * Warum nicht Versatz 0: dort steht auf einer frisch formatierten
         * Diskette nur 00 00 00 ... Eine Zusicherung gegen lauter Nullen
         * traegt kaum — sie waere auch erfuellt, wenn das Fenster aus
         * einem ganz anderen Grund Nullen zeigte. Der BAM beginnt mit
         * 12 01 41 (Verweis auf 18/1, DOS-Fassung 'A') und ist damit
         * unterscheidbar. */
        QFile f(img);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(f.seek(17 * 21 * 256));
        const QByteArray echt = f.read(8);
        f.close();
        QCOMPARE(echt.size(), 8);
        QCOMPARE(static_cast<quint8>(echt[0]), quint8(0x12));
        QCOMPARE(static_cast<quint8>(echt[2]), quint8(0x41));

        VisualDiskDialog d;
        d.loadDiskImage(img);

        /* Tun, was ein Klick tut. Der Slot ist privat, aber im
         * Metaobjekt — genau dafuer ist es da. */
        QVERIFY(QMetaObject::invokeMethod(&d, "onSectorClicked",
                                          Qt::DirectConnection,
                                          Q_ARG(int, 17), Q_ARG(int, 0),
                                          Q_ARG(int, 0)));

        const QString hex = kindText(&d, "textHexDump");
        QVERIFY2(!hex.isEmpty(), "Der Hexbereich ist leer.");
        QVERIFY2(!hex.contains("00 01 02 03 04 05 06 07"),
                 qPrintable("Der Hexbereich zeigt die erzeugte Rampe statt "
                            "des Sektorinhalts:\n" + hex));

        QString erwartet;
        for (int i = 0; i < 8; i++)
            erwartet += QString("%1 ").arg(
                static_cast<quint8>(echt[i]), 2, 16, QChar('0')).toUpper();
        QVERIFY2(hex.contains(erwartet.trimmed()),
                 qPrintable(QString("Die ersten acht Bytes des Abbilds (%1) "
                                    "stehen nicht im Hexbereich:\n%2")
                            .arg(erwartet.trimmed(), hex)));
    }
};

QTEST_MAIN(TestVisualDiskNoFiction)
#include "test_visual_disk_no_fiction.moc"

/**
 * @file test_explorer_tab_no_fiction.cpp
 * @brief Der Datei-Browser erfindet keine Verzeichnisse mehr (MF-575)
 *
 * ── Was hier bewacht wird ────────────────────────────────────────────────
 *
 * `ExplorerTab::readDirectory()` lieferte bis MF-569 dreizehn ERFUNDENE
 * Eintraege, je nach Endung:
 *
 *     ADF     s, c, devs, libs, Disk.info, Startup-Sequence
 *     D64     GAME 17280, DEMO 8192, MUSIC 4096, DATA 2048
 *             (GAME sogar mit Splat-Flag "*")
 *     ST/MSA  AUTO, DESKTOP.INF, GAME.PRG 65536
 *
 * Im Quelltext stand „Generate sample entries based on format". Auf dem
 * Bildschirm stand eine Dateiliste. Plausible Namen, plausible Groessen,
 * plausible Attribute — und kein Hinweis, dass nichts davon aus dem
 * Abbild gelesen wurde.
 *
 * Fuer einen Forensiker ist so eine Liste von einer echten nicht zu
 * unterscheiden. Das Original wandert danach in den Container, und die
 * Erfindung ist ab da die einzige Wahrheit. „Keine erfundenen Daten" ist
 * der erste Satz dieses Projekts.
 *
 * ── Warum ein Tor allein nicht reicht ────────────────────────────────────
 *
 * `scripts/audit_display_admits_placeholder.py` (34. Kategorie) faengt
 * Anzeigen, die sich im Quelltext SELBST als Platzhalter beschriften. Wer
 * eine Dateiliste fest verdrahtet, ohne einen Kommentar dazuzuschreiben,
 * faellt dort nicht auf — das steht im Kopf des Tors, und beim
 * Forensik-Bericht (MF-570) ist genau das passiert.
 *
 * Dieser Test prueft nicht den Quelltext, sondern die ANZEIGE: er laedt
 * ein Abbild und sieht in der Tabelle nach.
 *
 * ── Warum kein echtes Verzeichnis geprueft wird ──────────────────────────
 *
 * Weil es keines gibt. MF-569 hat die Erfindung entfernt und das Lesen
 * BEWUSST nicht verdrahtet: `uft_amiga_foreach_entry()` existiert, hat
 * aber null Aufrufer, und die AmigaDOS-Tests decken die Datei-Extraktion
 * gegen synthetische ADFs ab — nicht das Verzeichnislesen. Erfindung
 * durch Ungeprueftes zu ersetzen waere derselbe Fehler in neu.
 *
 * Wird das Lesen verdrahtet, gehoert hierher ein zweiter Test, der echte
 * Namen gegen ein benanntes Abbild prueft. Bis dahin bewacht dieser, dass
 * die Anzeige nichts behauptet.
 */
#include <QtTest/QtTest>
#include <QTableWidget>
#include <QTemporaryDir>

#include "explorertab.h"

class TestExplorerTabNoFiction : public QObject
{
    Q_OBJECT

private:
    /** Alles, was in der Dateitabelle steht, als eine Zeichenkette. */
    static QString tableText(ExplorerTab *tab)
    {
        auto *t = tab->findChild<QTableWidget *>("tableFiles");
        if (!t) return QString();
        QStringList out;
        for (int r = 0; r < t->rowCount(); r++) {
            QStringList cells;
            for (int c = 0; c < t->columnCount(); c++) {
                if (auto *it = t->item(r, c)) cells << it->text();
            }
            out << cells.join(" | ");
        }
        return out.join("\n");
    }

    /** Ein Abbild anlegen, das `DiskImageValidator` an der GROESSE
     *  erkennt — es prueft ADF/IMG ueber bekannte Groessen. */
    static bool makeImage(const QString &path, qint64 size)
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return false;
        QByteArray b(static_cast<int>(size), '\0');
        b[0] = 'D'; b[1] = 'O'; b[2] = 'S'; b[3] = '\0';
        for (int i = 1024; i < b.size(); i++)
            b[i] = static_cast<char>((i * 7 + (i >> 9)) & 0xFF);
        return f.write(b) == size;
    }

private slots:

    /* ── ADF: die sechs erfundenen Namen sind weg ──────────────────────── */
    void adfListingIsNotInvented()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString img = dir.filePath("probe.adf");
        QVERIFY2(makeImage(img, 901120), "Amiga-DD-Groesse");

        ExplorerTab tab;
        tab.loadImage(img);

        const QString shown = tableText(&tab);
        QVERIFY2(!shown.isEmpty(),
                 "Die Tabelle ist leer — dann prueft dieser Test nichts.");

        /* Die Namen aus der entfernten Erfindung. Tauchen sie auf, ist sie
         * zurueck. `Disk.info` und `Startup-Sequence` sind eindeutig genug,
         * um nicht zufaellig zu entstehen. */
        for (const QString &fiction : {"Startup-Sequence", "Disk.info",
                                       "devs", "libs"}) {
            QVERIFY2(!shown.contains(fiction),
                     qPrintable(QString("Erfundener Eintrag '%1' steht wieder "
                                        "in der Anzeige:\n%2")
                                .arg(fiction, shown)));
        }

        /* Und die Anzeige sagt, dass sie nichts weiss — statt zu
         * schweigen. Eine leere Tabelle waere ehrlich, aber ein leeres
         * Verzeichnis sieht genauso aus. */
        QVERIFY2(shown.contains("no directory listing"),
                 qPrintable(QString("Die Anzeige sagt nicht, dass sie das "
                                    "Dateisystem nicht liest:\n%1")
                            .arg(shown)));
    }

    /* ── D64: dieselbe Zusage fuer die Commodore-Seite ─────────────────── */
    void d64ListingIsNotInvented()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString img = dir.filePath("probe.d64");
        QVERIFY2(makeImage(img, 174848), "D64-Groesse");

        ExplorerTab tab;
        tab.loadImage(img);

        const QString shown = tableText(&tab);
        QVERIFY(!shown.isEmpty());

        /* Der erste Anlauf prueste hier auf die GROESSEN (17280, 8192,
         * ...). Der Rotbeweis fiel durch: die Tabelle zeigt
         * `formatSize()`, also "16.9 KB" — die rohe Zahl steht dort nie.
         * Eine Zusicherung, die nicht feuern kann, ist keine.
         *
         * Geprueft wird jetzt die TYP-Spalte: "PRG" und "SEQ" gibt es nur
         * in den erfundenen Commodore-Eintraegen, und die Namen dazu. */
        for (const QString &fiction : {"PRG", "SEQ", "GAME", "MUSIC"}) {
            QVERIFY2(!shown.contains(fiction),
                     qPrintable(QString("Erfundener Eintrag '%1' steht wieder "
                                        "in der Anzeige:\n%2")
                                .arg(fiction, shown)));
        }
        QVERIFY2(shown.contains("no directory listing"),
                 qPrintable("Auch fuer D64 muss dranstehen, dass nicht "
                            "gelesen wird:\n" + shown));
    }

    /* ── Und die Anzeige haengt nicht am Format ────────────────────────── */
    void everyFormatGetsTheSameHonestAnswer()
    {
        /* Der generische Zweig war schon vor MF-569 ehrlich („Directory
         * listing not available for this format"). Nur die drei Formate,
         * die ein Benutzer am ehesten oeffnet, fabrizierten. Diese
         * Ungleichbehandlung darf nicht zurueckkommen. */
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        struct { const char *name; qint64 size; } cases[] = {
            { "probe.adf", 901120 },
            { "probe.d64", 174848 },
            { "probe.img", 1474560 },
        };

        for (const auto &c : cases) {
            const QString img = dir.filePath(c.name);
            QVERIFY(makeImage(img, c.size));
            ExplorerTab tab;
            tab.loadImage(img);
            const QString shown = tableText(&tab);
            QVERIFY2(shown.contains("no directory listing"),
                     qPrintable(QString("%1 bekommt eine andere Antwort als "
                                        "die uebrigen:\n%2")
                                .arg(c.name, shown)));
        }
    }
};

QTEST_MAIN(TestExplorerTabNoFiction)
#include "test_explorer_tab_no_fiction.moc"

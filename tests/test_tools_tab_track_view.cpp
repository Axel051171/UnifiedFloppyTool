/**
 * @file test_tools_tab_track_view.cpp
 * @brief Der Spuransicht-Knopf zeigt Spuren (MF-1197)
 *
 * ── Was hier bewacht wird ────────────────────────────────────────────────
 *
 * `ToolsTab::onTrackView()` bestand aus drei Zeilen Text:
 *
 *     appendOutput(tr("Track View: Feature not yet implemented"));
 *     appendOutput(tr("This will show track-level analysis with sector headers."));
 *
 * Gemessen war das eine **Untertreibung**: `src/widgets/trackgridwidget.cpp`
 * ist ein fertiges Spurraster mit 882 Zeilen — Geometrie, neun Zustaende,
 * Konfidenz, Schutzmerkmale, Heatmap-Modi — und hatte im ganzen Baum
 * **null Aufrufer**. Das Programm konnte es; es fuehrte keine Tuer hin.
 * Klasse P3-204/MF-930, gehalten von Tor 68.
 *
 * ── Warum das Verdrahten hier zulaessig war ──────────────────────────────
 *
 * `.claude/CLAUDE.md` verlangt, eine erfundene Anzeige NICHT an einen
 * ungeprueften Leser zu haengen (die Lehre aus MF-568..570, sechs
 * Klasse-A-Befunde). Gemessen vorher:
 *
 *   * `TrackGridWidget::reset()` setzt jede Spur auf `TrackStatus::UNKNOWN`,
 *     gezeichnet grau, Legende "Unknown". Es behauptet ohne Daten NICHTS —
 *     das genaue Gegenteil der Belegungskarte aus MF-569, die auf "frei"
 *     vorbelegt war.
 *   * Die Daten liegen bereits vor: `onRepair()` faehrt seit MF-893 die
 *     vollstaendige Schleife ueber Zylinder x Koepfe mit `read_track()` und
 *     zaehlt je Spur Sektoren, CRC-Fehler, schwache Bits und Leerlaeufe —
 *     und warf das Ergebnis in ein Textprotokoll.
 *
 * Die Schleife ein zweites Mal zu schreiben waere MF-1177 gewesen ("eine
 * Groesse, eine Rechnung"). Sie liegt deshalb in EINER Funktion, und
 * `onRepair()` UND `onTrackView()` rufen dieselbe.
 *
 * ── Was dieser Test NICHT prueft ─────────────────────────────────────────
 *
 * Ob die Farben richtig gewaehlt sind. Geprueft wird, dass das Raster die
 * Geometrie DES ABBILDS bekommt und dass kein Feld einen Zustand
 * behauptet, den die Messung nicht hergibt.
 */
#include <QtTest/QtTest>
#include <QDialog>
#include <QFile>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTemporaryFile>
#include <QTextEdit>

#include <uft/uft_format_plugin.h>   /* uft_register_all_formats() */

#include "toolstab.h"
#include "widgets/trackgridwidget.h"

class TestToolsTabTrackView : public QObject
{
    Q_OBJECT

private:
    static QString korpus(const char *name)
    {
#ifdef UFT_CORPUS_DIR
        return QString(UFT_CORPUS_DIR) + "/" + name;
#else
        Q_UNUSED(name);
        return QString();
#endif
    }

    static QDialog *openedDialog(ToolsTab *tab)
    {
        const auto dlgs = tab->findChildren<QDialog *>();
        return dlgs.isEmpty() ? nullptr : dlgs.last();
    }

    /** Setzt das Feld, das auch `onHexView()` benutzt. */
    static bool setzeDatei(ToolsTab *tab, const QString &pfad)
    {
        auto *feld = tab->findChild<QLineEdit *>(QStringLiteral("editAnalyzeFile"));
        if (!feld) return false;
        feld->setText(pfad);
        return true;
    }

private slots:

    /**
     * Ohne diesen Aufruf ist die Format-Registry LEER und `uft_disk_open()`
     * liefert fuer jede Datei NULL — die Lage, die MF-447 beschreibt.
     * Der Test waere dann gruen aus dem falschen Grund: kein Dialog, kein
     * Raster, keine Behauptung. Ein erster Entwurf lief genau so.
     */
    void initTestCase()
    {
        QCOMPARE(uft_register_all_formats(), UFT_OK);
    }

    /* ── Der Knopf behauptet nicht mehr, es gaebe die Faehigkeit nicht ── */
    void theButtonNoLongerDeniesWhatExists()
    {
        const QString d64 = korpus("vice_c1541_35trk.d64");
        if (d64.isEmpty() || !QFile::exists(d64))
            QSKIP("Korpus-Abbild vice_c1541_35trk.d64 nicht vorhanden");

        ToolsTab tab;
        QVERIFY2(setzeDatei(&tab, d64), "Feld editAnalyzeFile nicht gefunden.");
        QVERIFY2(QMetaObject::invokeMethod(&tab, "onTrackView"),
                 "onTrackView liess sich nicht ausloesen.");

        /* BEIDE Klassen einsammeln. `ui->textOutput` ist ein
         * QPlainTextEdit, und das ist KEINE Unterklasse von QTextEdit —
         * beide stammen von QAbstractScrollArea. Die Erstfassung suchte
         * nur QTextEdit, fand nichts, und "enthaelt nicht" war trivial
         * wahr: gruen aus dem falschen Grund (Klasse MF-1014/1026). */
        QString ausgabe;
        for (auto *t : tab.findChildren<QTextEdit *>())
            ausgabe += t->toPlainText() + "\n";
        for (auto *t : tab.findChildren<QPlainTextEdit *>())
            ausgabe += t->toPlainText() + "\n";

        QVERIFY2(!ausgabe.isEmpty(),
                 "Der Reiter hat gar nichts ausgegeben — die Pruefung "
                 "darunter wuerde ins Leere greifen.");

        QVERIFY2(!ausgabe.contains("not yet implemented"),
                 qPrintable(QString("Der Knopf sagt weiterhin, die Faehigkeit "
                                    "gaebe es nicht — waehrend "
                                    "TrackGridWidget mit 882 Zeilen im Binary "
                                    "liegt. Ausgabe:\n%1").arg(ausgabe)));
    }

    /* ── Das Raster bekommt die Geometrie DIESES Abbilds ──────────────── */
    void theGridFollowsTheImageGeometry()
    {
        const QString d64 = korpus("vice_c1541_35trk.d64");
        if (d64.isEmpty() || !QFile::exists(d64))
            QSKIP("Korpus-Abbild vice_c1541_35trk.d64 nicht vorhanden");

        ToolsTab tab;
        QVERIFY(setzeDatei(&tab, d64));
        QVERIFY(QMetaObject::invokeMethod(&tab, "onTrackView"));

        QDialog *dlg = openedDialog(&tab);
        QVERIFY2(dlg, "Es wurde kein Dialog geoeffnet.");

        auto *grid = dlg->findChild<TrackGridWidget *>();
        QVERIFY2(grid, "Im Dialog steht kein TrackGridWidget.");

        /* VICEs c1541 schreibt 35 Spuren auf EINEM Kopf. Ein festes Raster
         * waere ein zweiter Platzhalter — deshalb haengt die Pruefung an
         * der Geometrie des Abbilds, nicht an einer Zahl im Code. */
        QCOMPARE(grid->totalTracks(), 35);

        /* Gelesene Spuren duerfen nicht als "unbekannt" stehen bleiben:
         * sonst zeigte das Raster grau, obwohl gemessen wurde. */
        QVERIFY2(grid->goodTracks() + grid->warningTracks()
                     + grid->errorTracks() > 0,
                 "Alle 35 Spuren stehen auf UNKNOWN — das Raster haengt an "
                 "keiner Messung.");
    }

    /* ── Nicht lesbare Datei: Raster ohne Behauptung ──────────────────── */
    void anUnreadableFileClaimsNothing()
    {
        /* Der LEERE Pfad laesst sich kopflos NICHT pruefen: er landet in
         * `QMessageBox::information()`, und ein modaler Dialog wartet ohne
         * Benutzer ewig. Das ist eine benannte Grenze dieses Tests, keine
         * Auslassung — gemessen, weil ein erster Entwurf genau daran
         * haengenblieb. Geprueft wird stattdessen der Fall, der forensisch
         * zaehlt: eine Datei, die KEIN Plugin oeffnen kann. */
        QTemporaryFile muell;
        QVERIFY(muell.open());
        muell.write("nicht-das-geringste", 19);
        muell.close();

        ToolsTab tab;
        QVERIFY(setzeDatei(&tab, muell.fileName()));
        QVERIFY(QMetaObject::invokeMethod(&tab, "onTrackView"));

        QDialog *dlg = openedDialog(&tab);
        if (dlg) {
            auto *grid = dlg->findChild<TrackGridWidget *>();
            QVERIFY2(!grid || (grid->goodTracks() == 0
                               && grid->warningTracks() == 0),
                     "Ohne lesbares Abbild behauptet das Raster einen Zustand.");
        }

        /* Und der Vorbehalt gehoert in die Anzeige, nicht in den Quelltext. */
        QString ausgabe;
        for (auto *t : tab.findChildren<QPlainTextEdit *>())
            ausgabe += t->toPlainText() + "\n";
        QVERIFY2(ausgabe.contains("kein Freispruch"),
                 qPrintable(QString("Die Absage nennt nicht, dass ueber die "
                                    "Diskette damit nichts bekannt ist. "
                                    "Ausgabe:\n%1").arg(ausgabe)));
    }
};

QTEST_MAIN(TestToolsTabTrackView)
#include "test_tools_tab_track_view.moc"

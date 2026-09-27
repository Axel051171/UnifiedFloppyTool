// SPDX-License-Identifier: MIT
/**
 * @file test_protection_bedienelemente.cpp
 * @brief P3-602: zwei der drei toten Bedienelemente der Schutzanalyse (MF-1460)
 *
 * (1) Der Filter in `ProtectionAnalysisWidget` verwarf seinen Index
 *     (`Q_UNUSED(index)`) — jede Auswahl zeigte dieselbe Liste. Und
 *     "High Confidence Only" versprach eine Konfidenz, die MF-508 als
 *     erfunden entfernt hat. Seit MF-1460 filtert er nach dem, was die
 *     Zeilen tragen: Basis "Signal" oder "auf schwachen Bits".
 *
 * (2) `checkC64Alignment` hat keinen Erkenner im Baum — die laufende
 *     C64-Analyse kennt sieben Merkmale, Spurausrichtung ist keins. Der
 *     Kasten ist abgeschaltet, ungesetzt und nennt den Grund; ein
 *     gespeicherter Haken aus einer frueheren Sitzung gilt nicht.
 *
 * (3) steht in test_tools_tab_track_view.cpp (das Spurraster).
 *
 * DER BEFUND DAHINTER (gemessen beim Anbinden): der Filter haette auch
 * mit Index nichts gefiltert, weil die Liste IMMER leer war. Die Analyse
 * `ufm_c64_prot_analyze()` meldet `UFM_PROT_LONG_TRACK`, `_HALF_TRACK`,
 * `_CUSTOM_SYNC`, `_BAD_GCR`, `_DUPLICATE_ID`; Waermekarte und
 * Schemaliste fragten nur `UFM_C64_PROT_*` ab — andere Werte desselben
 * Enums, die laut MF-842 KEIN Erkenner setzt. Jeder Treffer fiel in
 * `default: continue`. Auf Bounty Bob (80 Treffer, MF-405) blieb die
 * Anzeige leer, nur der Balken bewegte sich.
 *
 * Die Eingabe: `vice_c1541_35trk.g64` liefert sauber 0 Treffer. Der
 * Test verlaengert darin Spur 1 von 7692 auf 7928 Byte (Luecke 0x55) —
 * Verhaeltnis 1,031 gegen die Schwelle 1,02 der Analyse, also GENAU EIN
 * bekannter Treffer `UFM_PROT_LONG_TRACK` auf Spur 1.
 */

#include <QtTest/QtTest>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QSettings>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTemporaryFile>

#include "gui/ProtectionAnalysisWidget.h"
#include "protectiontab.h"

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

class TestProtectionBedienelemente : public QObject {
    Q_OBJECT

    static QStringList zeilen(QTableWidget *t, int spalte)
    {
        QStringList r;
        for (int i = 0; i < t->rowCount(); i++)
            r << (t->item(i, spalte) ? t->item(i, spalte)->text() : QString());
        return r;
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("UFT-Test-MF1436"));
        QCoreApplication::setApplicationName(QStringLiteral("test_protection_bedienelemente"));
    }

    /* VICE-G64 mit verlaengerter Spur 1 (Kopfdaten des Formats: Kennung
     * "GCR-1541", Byte 10-11 Platz je Spur, ab 12 die Versatztafel; je
     * Spur 2 Byte Laenge + Daten). */
    static QString langeSpur1(QTemporaryFile &f)
    {
        QFile q(QStringLiteral(UFT_CORPUS_DIR "/vice_c1541_35trk.g64"));
        if (!q.open(QIODevice::ReadOnly)) return {};
        QByteArray d = q.readAll();
        if (d.size() < 16 || !d.startsWith("GCR-1541")) return {};
        const int platz = (uchar)d[10] | ((uchar)d[11] << 8);
        const int off = (uchar)d[12] | ((uchar)d[13] << 8) |
                        ((uchar)d[14] << 16) | ((uchar)d[15] << 24);
        const int alt = (uchar)d[off] | ((uchar)d[off + 1] << 8);
        if (platz != 7928 || alt != 7692) return {};
        for (int i = alt; i < platz; i++) d[off + 2 + i] = char(0x55);
        d[off] = char(platz & 0xFF);
        d[off + 1] = char(platz >> 8);
        if (!f.open()) return {};
        f.write(d);
        f.flush();
        return f.fileName();
    }

    void treffErreichenDieAnzeige()
    {
        QTemporaryFile f(QDir::tempPath() + QStringLiteral("/uft_mf1436_XXXXXX.g64"));
        const QString pfad = langeSpur1(f);
        QVERIFY2(!pfad.isEmpty(), "Eingabe nicht herstellbar");

        ProtectionAnalysisWidget w;
        w.loadG64(pfad.toUtf8().constData());
        w.runAnalysis();
        QVERIFY2(w.getConfidence() > 0, "die Analyse meldet keinen Treffer");

        /* Waermekarte: Zeile 0 = Spur 1, Spalte 1 = "Long Track" */
        QTableWidget *karte = nullptr, *liste = nullptr;
        for (auto *t : w.findChildren<QTableWidget *>())
            (t->columnCount() == 3 ? liste : karte) = t;
        QVERIFY(karte && liste);
        QVERIFY2(karte->item(0, 1) && !karte->item(0, 1)->text().isEmpty(),
                 "der Treffer UFM_PROT_LONG_TRACK erreicht die Waermekarte nicht");
        QVERIFY2(liste->rowCount() > 0,
                 "der Treffer erreicht die Schemaliste nicht");
    }

    void derFilterWirkt()
    {
        QTemporaryFile f(QDir::tempPath() + QStringLiteral("/uft_mf1436_XXXXXX.g64"));
        const QString pfad = langeSpur1(f);
        QVERIFY(!pfad.isEmpty());

        ProtectionAnalysisWidget w;
        auto *filter = w.findChild<QComboBox *>(QStringLiteral("schemeFilter"));
        QVERIFY2(filter, "Filter schemeFilter fehlt");
        QCOMPARE(filter->count(), 3);
        for (int i = 0; i < filter->count(); i++)
            QVERIFY2(!filter->itemText(i).contains(QStringLiteral("Confidence")),
                     "ein Filtereintrag verspricht wieder eine Konfidenz (MF-508)");
        /* "Weak Bit Based": keine C64-Analyse im Baum meldet schwache Bits */
        auto *m = qobject_cast<QStandardItemModel *>(filter->model());
        QVERIFY(m && m->item(2));
        QVERIFY2(!m->item(2)->isEnabled(), "Weak-Bit-Filter waehlbar, obwohl nichts ihn fuellen kann");

        w.loadG64(pfad.toUtf8().constData());
        w.runAnalysis();
        QTableWidget *liste = nullptr;
        for (auto *t : w.findChildren<QTableWidget *>())
            if (t->columnCount() == 3) liste = t;
        QVERIFY(liste);

        filter->setCurrentIndex(0);
        const QStringList alle = zeilen(liste, 0), alleBasis = zeilen(liste, 1);
        qInfo("alle: %s | Basis: %s", qPrintable(alle.join(", ")),
              qPrintable(alleBasis.join(", ")));
        QVERIFY2(alle.size() >= 2, "Signal- und Regelzeile erwartet");

        filter->setCurrentIndex(1);
        const QStringList sigBasis = zeilen(liste, 1);
        int erwartet = 0;
        for (const QString &b : alleBasis) if (b == tr("Signal")) erwartet++;
        QCOMPARE(sigBasis.size(), erwartet);
        for (const QString &b : sigBasis) QCOMPARE(b, tr("Signal"));
        QVERIFY2(sigBasis.size() < alle.size(), "der Filter nimmt nichts heraus");

        filter->setCurrentIndex(0);
        QCOMPARE(zeilen(liste, 0), alle);
    }

    void spurausrichtungSagtDassSieFehlt()
    {
        /* Ein gespeicherter Haken aus einer frueheren Sitzung */
        {
            QSettings s;
            s.beginGroup(QStringLiteral("ProtectionTab"));  /* ProtectionTab::SETTINGS_GROUP */
            s.setValue(QStringLiteral("c64Alignment"), true);
            s.endGroup();
        }
        ProtectionTab tab;
        auto *k = tab.findChild<QCheckBox *>(QStringLiteral("checkC64Alignment"));
        QVERIFY(k);
        QVERIFY2(!k->isEnabled(), "Spurausrichtung ist bedienbar, hat aber keinen Erkenner");
        QVERIFY2(!k->isChecked(), "ein Haken ohne Wirkung");
        QVERIFY2(k->toolTip().contains(QStringLiteral("P3-602")), qPrintable(k->toolTip()));
    }

    /* ── P3-650 Weg A (MF-1461) ──────────────────────────────────────────
     *
     * Der Protection Analyzer hatte 54 Bedienelemente ohne Leser. Gemessen
     * beim Anbinden: die Gruppe "Detected Protection Features" (Long Track,
     * Duplicate IDs, Sync Anomaly …) ist im Formular AUSGESCHALTET — eine
     * Ergebnisanzeige, keine Eingabe. Unter den Eingaben hat genau EINE ein
     * Merkmal in der laufenden Analyse: "Enable Half-Track Detection" ↔
     * UFM_PROT_HALF_TRACK. Sie waehlt, ob die Analyse Halbspur-Treffer
     * ZEIGT; alles andere ist abgeschaltet und nennt den Grund. */

    /* VICE-G64, dazu die Daten von Spur 1 auch auf Halbspur 1.5 (Platz 1
     * der Versatztafel; Geschwindigkeitszone von Platz 0 uebernommen). */
    static QString halbspur(QTemporaryFile &f)
    {
        QFile q(QStringLiteral(UFT_CORPUS_DIR "/vice_c1541_35trk.g64"));
        if (!q.open(QIODevice::ReadOnly)) return {};
        QByteArray d = q.readAll();
        if (d.size() < 700 || !d.startsWith("GCR-1541") || (uchar)d[9] != 84) return {};
        auto u32 = [&d](int o) {
            return (uint32_t)(uchar)d[o] | ((uint32_t)(uchar)d[o + 1] << 8) |
                   ((uint32_t)(uchar)d[o + 2] << 16) | ((uint32_t)(uchar)d[o + 3] << 24); };
        auto setze32 = [&d](int o, uint32_t v) {
            for (int i = 0; i < 4; i++) d[o + i] = char((v >> (8 * i)) & 0xFF); };
        const int platz = (uchar)d[10] | ((uchar)d[11] << 8);
        const uint32_t off0 = u32(12);
        if (u32(12 + 4) != 0 || off0 == 0) return {};          /* 1.5 war leer */
        const uint32_t neu = (uint32_t)d.size();
        d += d.mid((int)off0, 2 + platz);                       /* Laenge + Daten */
        setze32(12 + 4, neu);
        setze32(12 + 4 * 84 + 4, u32(12 + 4 * 84));             /* Zone wie Spur 1 */
        if (!f.open()) return {};
        f.write(d);
        f.flush();
        return f.fileName();
    }

    void halbspurKaestchenSteuertDieAnzeige()
    {
        QTemporaryFile f(QDir::tempPath() + QStringLiteral("/uft_mf1437_XXXXXX.g64"));
        const QString pfad = halbspur(f);
        QVERIFY2(!pfad.isEmpty(), "Eingabe nicht herstellbar");

        auto liste = [](ProtectionAnalysisWidget &w) -> QTableWidget * {
            for (auto *t : w.findChildren<QTableWidget *>())
                if (t->columnCount() == 3) return t;
            return nullptr;
        };
        const QString name = QString::fromUtf8(ufm_c64_prot_type_name(UFM_PROT_HALF_TRACK));

        {   /* an: der Treffer ist zu sehen */
            ProtectionTab tab;
            auto *k = tab.findChild<QCheckBox *>(QStringLiteral("checkHalfTrack"));
            QVERIFY(k);
            QVERIFY2(k->isEnabled(), "checkHalfTrack ist verdrahtet und muss bedienbar sein");
            k->setChecked(false);
            k->setChecked(true);
        }
        ProtectionAnalysisWidget w;
        w.loadG64(pfad.toUtf8().constData());
        w.runAnalysis();
        QVERIFY(liste(w));
        qInfo("an: %s", qPrintable(zeilen(liste(w), 0).join(", ")));
        QVERIFY2(zeilen(liste(w), 0).contains(name),
                 "die Eingabe erzeugt keinen Halbspur-Treffer");

        {   /* aus — und der Reiter wird VOR dem Analysieren zerstoert: die
               Wahl muss sofort gespeichert sein, nicht erst beim Beenden */
            ProtectionTab tab;
            tab.findChild<QCheckBox *>(QStringLiteral("checkHalfTrack"))->setChecked(false);
        }
        w.runAnalysis();
        qInfo("aus: %s", qPrintable(zeilen(liste(w), 0).join(", ")));
        QVERIFY2(!zeilen(liste(w), 0).contains(name), "abgewaehlt, aber gezeigt");
        auto *hinweis = w.findChild<QLabel *>(QStringLiteral("lblSchutzAuswahl"));
        QVERIFY2(hinweis, "kein Hinweis, dass ausgeblendet wurde");
        qInfo("Hinweis: %s", qPrintable(hinweis->text()));
        QVERIFY2(hinweis->text().contains(name), qPrintable(hinweis->text()));

        {   ProtectionTab tab;
            tab.findChild<QCheckBox *>(QStringLiteral("checkHalfTrack"))->setChecked(true);
        }
        w.runAnalysis();
        QVERIFY(zeilen(liste(w), 0).contains(name));
        QVERIFY2(!hinweis->text().contains(name), qPrintable(hinweis->text()));
    }

    void derRestSagtDassNichtsIhnLiest()
    {
        ProtectionTab tab;
        const QStringList verdrahtet = {
            QStringLiteral("checkHalfTrack"),
            QStringLiteral("comboProfile"), QStringLiteral("btnSaveProfile"),
            QStringLiteral("btnLoadProfile"), QStringLiteral("btnDeleteProfile")};
        int gesperrt = 0;
        for (QWidget *e : tab.findChildren<QWidget *>()) {
            if (!qobject_cast<QAbstractButton *>(e) && !qobject_cast<QComboBox *>(e) &&
                !qobject_cast<QAbstractSpinBox *>(e) && !qobject_cast<QAbstractSlider *>(e))
                continue;
            if (e->objectName().isEmpty() || e->objectName().startsWith(QLatin1String("qt_")))
                continue;
            if (verdrahtet.contains(e->objectName())) {
                QVERIFY2(e->isEnabled(), qPrintable(e->objectName() + " ist verdrahtet, aber gesperrt"));
                continue;
            }
            QVERIFY2(!e->isEnabled(), qPrintable(e->objectName() + " bedienbar, aber ohne Leser"));
            QVERIFY2(e->toolTip().contains(QStringLiteral("P3-6")),
                     qPrintable(e->objectName() + ": " + e->toolTip()));
            gesperrt++;
        }
        qInfo("gesperrt: %d, verdrahtet: %d", gesperrt, int(verdrahtet.size()));
        QVERIFY(gesperrt > 40);

        /* Ein Moduswechsel darf die Sperren nicht aufheben */
        QMetaObject::invokeMethod(&tab, "applyProfile",
                                  Q_ARG(QString, QStringLiteral("C64 Advanced")));
        for (const char *n : {"checkC64Enable", "checkErr1", "checkDD1", "groupGCR"}) {
            auto *e = tab.findChild<QWidget *>(QString::fromLatin1(n));
            QVERIFY(e);
            if (qobject_cast<QGroupBox *>(e)) continue;
            QVERIFY2(!e->isEnabled(), n);
        }
        QVERIFY(tab.findChild<QCheckBox *>(QStringLiteral("checkHalfTrack"))->isEnabled());
    }
};

QTEST_MAIN(TestProtectionBedienelemente)
#include "test_protection_bedienelemente.moc"

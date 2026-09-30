/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_format_tab_bindung.cpp
 * @brief Was die Felder des Settings-Reiters erreichen (MF-1618)
 *
 * Der Reiter ist nach dem Entwurf des Eigentuemers neu gebaut (MF-1618).
 * Die Zeugen des frueheren Tests — Geometrie-, Fluss- und GCR-Felder —
 * sind mit dem alten Formular gegangen; ihre Zusagen (MF-1282: die
 * Beschriftung geht mit ihrem Feld; MF-1306: ein verborgenes Feld steuert
 * nichts bei) gelten weiter und stehen hier an den Feldern, die es gibt.
 *
 * Neu, aus den Eigentuemer-Antworten vom 2026-09-29:
 *   B1  Felder ohne Leser sind sichtbar, gesperrt, markiert — und KEIN Tor
 *       gibt sie frei, auch nicht nach Format- oder Moduswechsel.
 *   B2  „Nach dem Wandeln nachpruefen" ist eine Sicht auf die Richtlinie
 *       des Plans, kein zweites Feld: sie folgt ihr und setzt sie.
 *   B3  Der Hashsatz steht nur bei der Beweisrichtlinie.
 *   B4  Ein .uftsetup uebersteht den Rundlauf; Werte fuer Felder, die im
 *       Ziel verborgen sind, werden NICHT angewandt und genannt.
 *   B5  Die alte presets.json wird uebernommen, ohne sie anzufassen.
 *   B6  Der Pfad des IPF-Hilfsprogramms wirkt ueber die Umgebung, die
 *       uft_ipf_helper.c liest.
 *   B7  Die Kopfzeile nennt die verbundene Hardware.
 */

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>

#include "formattab.h"

extern "C" {
#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/formats/ipf/uft_ipf_helper.h"
}

class TestFormatTabBindung : public QObject {
    Q_OBJECT

private:
    static void waehle(QComboBox *box, int wert)
    {
        const int i = box->findData(wert);
        QVERIFY2(i >= 0, "Wert nicht im Auswahlfeld");
        box->setCurrentIndex(i);
    }
    static void benutzerdefiniert(FormatTab &tab)
    {
        auto *b = tab.findChild<QPushButton *>("btnModus_custom");
        QVERIFY(b);
        b->click();
    }

private slots:
    void initTestCase()
    {
        QVERIFY2(uft_register_all_formats() == UFT_OK,
                 "Format-Plugins liessen sich nicht registrieren");
    }

    /* B1 */
    void unverdrahtete_felder_sind_gesperrt_und_markiert()
    {
        FormatTab tab;
        const QStringList namen = FormatTab::unverdrahteteFelder();
        /* gemessen: 29 Felder des Entwurfs ohne Leser (MF-1618), siehe die
         * Liste in formattab.cpp und ZUORDNUNG_settings_redesign.md; seit
         * MF-1619 ist spinMaxRetries verdrahtet -> 28 */
        QCOMPARE(namen.size(), 28);
        QVERIFY(!namen.contains(QStringLiteral("spinMaxRetries")));
        auto pruefe = [&](const char *wann) {
            for (const QString &n : namen) {
                QWidget *w = tab.findChild<QWidget *>(n);
                QVERIFY2(w, qPrintable(n + " fehlt"));
                QVERIFY2(!w->isEnabled(), qPrintable(QStringLiteral("%1 ist bedienbar (%2)")
                                                         .arg(n, QLatin1String(wann))));
                QVERIFY2(w->property("uft_unverdrahtet").toBool(), qPrintable(n));
                QVERIFY2(w->toolTip().contains(QStringLiteral("Noch nicht verdrahtet")),
                         qPrintable(n + ": kein Hinweis"));
            }
        };
        pruefe("beim Oeffnen");
        QVERIFY(tab.waehleFormat(QStringLiteral("SCP")));
        pruefe("nach SCP");
        benutzerdefiniert(tab);
        pruefe("im Modus Benutzerdefiniert");
        QVERIFY(tab.waehleFormat(QStringLiteral("D64")));
        pruefe("nach D64");
        /* die Markierung steht im Text: an der Checkbox selbst … */
        auto *c = tab.findChild<QCheckBox *>("checkPreserveTiming");
        QVERIFY(c && c->text().endsWith(QStringLiteral(" *")));
        /* … und an der Beschriftung eines Auswahlfelds */
        auto *l = tab.findChild<QLabel *>("labelRpm");
        QVERIFY(l && l->text().contains(QStringLiteral("*")));
    }

    /* MF-1282 an einem Feld, das es gibt: `read.revolutions` ist auf der
     * Dateiebene verboten. Feld UND Beschriftung gehen — und kommen auf
     * der Spurebene wieder. Das gilt auch bei „Automatisch" (MF-1618:
     * die Ebenenregel haengt nicht am Format). */
    void beschriftung_geht_mit_und_kommt_wieder()
    {
        FormatTab tab;
        benutzerdefiniert(tab);
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *rev = tab.findChild<QWidget *>("spinRevolutions");
        auto *lab = tab.findChild<QLabel *>("labelRevolutions");
        QVERIFY(lvl && rev && lab);
        waehle(lvl, UFT_COPY_FILE);
        QVERIFY2(rev->isHidden() && lab->isHidden(),
                 "Dateiebene: Umdrehungen und ihre Beschriftung muessen weg");
        waehle(lvl, UFT_COPY_TRACK);
        QVERIFY2(!rev->isHidden() && !lab->isHidden(),
                 "Spurebene: beide muessen wieder da sein");
    }

    /* MF-1619: „Max. Wiederholungen" erreicht den Plan — aber nur, wenn
     * das Feld frei ist. Legt die Lesestrategie `read.retries` fest,
     * sperrt das Parameter-Tor das Feld, und der Plan traegt KEINE eigene
     * Zahl. Gesucht wird ueber die Strategien, nicht angenommen. */
    void wiederholungen_erreichen_den_plan_nur_wenn_frei()
    {
        FormatTab tab;
        benutzerdefiniert(tab);
        auto *str = tab.findChild<QComboBox *>("comboPlanStrategy");
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *spin = tab.findChild<QSpinBox *>("spinMaxRetries");
        QVERIFY(str && lvl && spin);
        waehle(lvl, UFT_COPY_FLUX);
        bool frei = false, fest = false;
        for (int s = 0; s < str->count(); s++) {
            str->setCurrentIndex(s);
            if (spin->isHidden()) continue;
            if (spin->isEnabled()) {
                spin->setValue(7);
                const uft_copy_plan_t p = tab.copyPlan();
                QVERIFY2(p.read_retries_gesetzt && p.read_retries == 7u,
                         qPrintable(str->currentText()));
                frei = true;
            } else {
                const uft_copy_plan_t p = tab.copyPlan();
                QVERIFY2(!p.read_retries_gesetzt,
                         qPrintable(QStringLiteral("festgelegt, aber gesetzt: ")
                                    + str->currentText()));
                fest = true;
            }
        }
        QVERIFY2(frei, "keine Strategie laesst das Feld frei — dann ist es nie wirksam");
        (void)fest;   /* ob eine Strategie es festlegt, ist Sache des Kerns */
    }

    /* B2 */
    void nachpruefen_ist_eine_sicht_auf_die_richtlinie()
    {
        FormatTab tab;
        auto *chk = tab.findChild<QCheckBox *>("checkVerify");
        auto *pol = tab.findChild<QComboBox *>("comboPlanPolicy");
        QVERIFY(chk && pol);
        QVERIFY2(!chk->isEnabled(), "ausserhalb von Benutzerdefiniert gesperrt");
        tab.findChild<QPushButton *>("btnModus_evidence")->click();
        QVERIFY2(chk->isChecked(), "EvidenceCopy verlangt die Nachpruefung");
        benutzerdefiniert(tab);
        QVERIFY(chk->isEnabled());
        chk->setChecked(false);
        QCOMPARE(tab.copyPlan().policy, UFT_POLICY_NORMAL);
        chk->setChecked(true);
        QCOMPARE(tab.copyPlan().policy, UFT_POLICY_VERIFY);
        waehle(pol, UFT_POLICY_NORMAL);
        QVERIFY2(!chk->isChecked(), "die Sicht folgt der Richtlinie");
    }

    /* B3 */
    void pruefsummen_nur_bei_beweisrichtlinie()
    {
        FormatTab tab;
        auto *satz = tab.findChild<QWidget *>("unter_nachweis_Pruefsummen");
        QVERIFY(satz);
        QVERIFY2(satz->isHidden(), "Standardkopie: kein Hashsatz");
        tab.findChild<QPushButton *>("btnModus_evidence")->click();
        QVERIFY2(!satz->isHidden(), "EvidenceCopy: Hashsatz sichtbar");
        auto *sha = tab.findChild<QCheckBox *>("checkSHA256");
        QVERIFY(sha && sha->isChecked() && !sha->isEnabled());
        QVERIFY(tab.copyPlan().hashes & UFT_HASH_SHA256);
    }

    /* B4 */
    void setup_rundlauf()
    {
        FormatTab a;
        benutzerdefiniert(a);
        waehle(a.findChild<QComboBox *>("comboPlanLevel"), UFT_COPY_TRACK);
        waehle(a.findChild<QComboBox *>("comboPlanStrategy"), UFT_READ_CONSENSUS);
        waehle(a.findChild<QComboBox *>("comboPlanPolicy"), UFT_POLICY_EVIDENCE);
        QVERIFY(a.waehleFormat(QStringLiteral("SCP")));
        a.findChild<QCheckBox *>("checkCRC32")->setChecked(true);
        a.findChild<QSpinBox *>("spinRevolutions")->setValue(7);
        const QJsonObject o = a.setupAlsJson();

        FormatTab b;
        QString fehler;
        QStringList weg;
        QVERIFY2(b.setupAnwenden(o, &fehler, &weg), qPrintable(fehler));
        QVERIFY2(weg.isEmpty(), qPrintable(weg.join(", ")));
        const uft_copy_plan_t pa = a.copyPlan(), pb = b.copyPlan();
        QCOMPARE(pb.level, pa.level);
        QCOMPARE(pb.strategy, pa.strategy);
        QCOMPARE(pb.policy, pa.policy);
        QCOMPARE(pb.hashes, pa.hashes);
        QCOMPARE(b.getSelectedFormat(), QStringLiteral("SCP"));
        QCOMPARE(b.leseUmdrehungen(), 7);
        QVERIFY(!QJsonDocument(o).toJson().isEmpty());
    }

    void setup_uebergeht_verborgene_werte()
    {
        FormatTab a;
        a.findChild<QSpinBox *>("spinRevolutions")->setValue(9);
        QJsonObject o = a.setupAlsJson();
        /* ein Setup auf der Dateiebene: dort ist read.revolutions verboten */
        o[QStringLiteral("modus")] = QString();
        QJsonObject plan = o.value(QStringLiteral("plan")).toObject();
        plan[QStringLiteral("level")] =
            QString::fromUtf8(uft_copy_level_name(UFT_COPY_FILE));
        o[QStringLiteral("plan")] = plan;

        FormatTab b;
        const int vorher = b.leseUmdrehungen();
        QString fehler;
        QStringList weg;
        QVERIFY2(b.setupAnwenden(o, &fehler, &weg), qPrintable(fehler));
        QVERIFY2(weg.contains(QStringLiteral("spinRevolutions")),
                 qPrintable(QStringLiteral("nicht genannt: %1").arg(weg.join(", "))));
        QCOMPARE(b.leseUmdrehungen(), vorher);
    }

    void setup_falsche_fassung_abgelehnt()
    {
        FormatTab t;
        QJsonObject o;
        o[QStringLiteral("uftsetup")] = 2;
        QString fehler;
        QVERIFY(!t.setupAnwenden(o, &fehler));
        QVERIFY(!fehler.isEmpty());
    }

    /* B5 */
    void alte_presets_werden_uebernommen_ohne_die_quelle_anzufassen()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString alt = dir.filePath(QStringLiteral("presets.json"));
        {
            QFile f(alt);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(R"({"Meine C64":{"system":"Commodore 64/128","format":"D64",)"
                    R"("version":"35 Track","encoding":"GCR","tracks":35,)"
                    R"("halfTracks":false,"gcrType":"Off"},)"
                    R"("PC/HD":{"system":"PC/DOS","format":"IMG","version":"1.44M"}})");
        }
        auto summe = [&]() {
            QFile f(alt);
            f.open(QIODevice::ReadOnly);
            return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256);
        };
        const QByteArray vorher = summe();
        const QString ziel = dir.filePath(QStringLiteral("setups"));
        QStringList m;
        QCOMPARE(FormatTab::uebernimmAltePresets(alt, ziel, &m), 2);
        QCOMPARE(summe(), vorher);
        QFile s(ziel + QStringLiteral("/Meine C64.uftsetup"));
        QVERIFY2(s.open(QIODevice::ReadOnly), qPrintable(m.join("; ")));
        const QJsonObject o = QJsonDocument::fromJson(s.readAll()).object();
        QCOMPARE(o.value(QStringLiteral("format")).toString(), QStringLiteral("D64"));
        QCOMPARE(o.value(QStringLiteral("variante")).toString(), QStringLiteral("35 Track"));
        const QJsonObject weg = o.value(QStringLiteral("nicht_uebernommen")).toObject();
        QVERIFY2(weg.contains(QStringLiteral("tracks")) && weg.contains(QStringLiteral("gcrType")),
                 "was nicht uebernommen wird, muss genannt sein");
    }

    /* B6 */
    void helferpfad_wirkt_ueber_die_umgebung()
    {
        const QByteArray alt = qgetenv(UFT_IPF_HELPER_ENV);
        FormatTab tab;
        auto *e = tab.findChild<QLineEdit *>("editHelperPath");
        QVERIFY(e);
        e->setText(QStringLiteral("C:/nirgends/uft_capsimg_helper.exe"));
        emit e->editingFinished();
        QCOMPARE(QString::fromLocal8Bit(qgetenv(UFT_IPF_HELPER_ENV)),
                 QStringLiteral("C:/nirgends/uft_capsimg_helper.exe"));
        QCOMPARE(QString::fromUtf8(uft_ipf_helper_path()),
                 QStringLiteral("C:/nirgends/uft_capsimg_helper.exe"));
        auto *st = tab.findChild<QLabel *>("labelHelperStatus");
        QVERIFY(st && st->text().contains(QStringLiteral("nicht gefunden")));
        e->clear();
        emit e->editingFinished();
        QVERIFY2(uft_ipf_helper_path() == nullptr, "geleertes Feld hebt die Einrichtung auf");
        QSettings().remove(QStringLiteral("Werkzeuge"));
        if (!alt.isEmpty()) qputenv(UFT_IPF_HELPER_ENV, alt);
    }

    /* B7 */
    void kopfzeile_nennt_die_hardware()
    {
        FormatTab tab;
        auto *l = tab.findChild<QLabel *>("labelHardwareStatus");
        QVERIFY(l);
        QVERIFY(l->text().contains(QStringLiteral("Keine")));
        tab.setHardwareVerbunden(true);
        tab.setHardwareGeraet(QStringLiteral("Greaseweazle F7"), QStringLiteral("1.4"));
        QVERIFY(l->text().contains(QStringLiteral("Greaseweazle F7")));
        QVERIFY(l->text().contains(QStringLiteral("1.4")));
        tab.setHardwareVerbunden(false);
        QVERIFY(l->text().contains(QStringLiteral("Keine")));
    }
};

QTEST_MAIN(TestFormatTabBindung)
#include "test_format_tab_bindung.moc"

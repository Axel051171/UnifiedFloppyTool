/**
 * @file test_format_tab_capability_gating.cpp
 * @brief Zwei Formate, verschiedene Bedienelemente (MF-661, neu MF-1618)
 *
 * Abnahme fuer Stufe 2 aus `docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md`:
 * **die Oberflaeche zeigt fuer verschiedene Formate wirklich Verschiedenes,
 * und zwar das, was deren Manifest ansagt.** Der Test waehlt das Format
 * ueber die Auswahlfelder — den Weg eines Benutzers — und liest danach die
 * echten Widgets (MF-649).
 *
 * MF-1618: der Settings-Reiter ist neu gebaut. Die Zusagen sind dieselben;
 * geaendert haben sich die ZEUGEN. `comboSampleRate` und die Gruppen
 * groupFlux/groupPLL/groupProtection gibt es nicht mehr. Das Merkmal
 * „Flux" haengt jetzt an den zwei Flussfeldern (`checkIncludeRawFlux`,
 * `checkReadBetweenIndex`), „Write" an `groupWrite`; die Unterscheidung am
 * Parameter-Tor zeigt `checkWeakBits` (`preserve_weak_bits`). Die zwei
 * Formate dafuer sucht der Test zur Laufzeit in der Registry, statt Namen
 * festzuschreiben, die sich aendern koennen.
 *
 * Und die Regel fuer den Ungluecksfall bleibt: zu einem Format, das sich
 * nicht nachschlagen laesst, wird NICHTS ausgeblendet.
 */

#include <QtTest/QtTest>
#include <QComboBox>
#include <QWidget>
#include <QPushButton>

#include "formattab.h"

extern "C" {
#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
}

class TestFormatTabCapabilityGating : public QObject
{
    Q_OBJECT

private:
    static bool sichtbar(FormatTab &tab, const char *name, bool *gefunden)
    {
        QWidget *w = tab.findChild<QWidget *>(QString::fromLatin1(name));
        *gefunden = (w != nullptr);
        return w && !w->isHidden();
    }

    /* Ein registriertes Format mit bzw. ohne die Flagge. */
    static QString formatMit(uint32_t flagge, bool mit)
    {
        for (size_t i = 0; i < uft_registered_format_plugin_count(); i++) {
            const uft_format_plugin_t *p = uft_registered_format_plugin_at(i);
            if (!p || !p->name || !*p->name) continue;
            if (((p->capabilities & flagge) != 0) == mit)
                return QString::fromUtf8(p->name);
        }
        return QString();
    }

    /* Eine Planstellung, auf der die EBENE das Weak-Bits-Feld zeigt — bei
     * einem Format, das die Flagge hat. Auf der Sektorebene der
     * Standardkopie verbirgt schon die Ebene es, bei jedem Format; wer
     * dort Formate vergleicht, vergleicht nichts. Gesucht wird ueber die
     * Oberflaeche, nicht ueber eine angenommene Stellung. */
    static bool stellungMitWeakBits(FormatTab &tab, const QString &format)
    {
        auto *custom = tab.findChild<QPushButton *>("btnModus_custom");
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *erh = tab.findChild<QComboBox *>("comboPlanPreserve");
        if (!custom || !lvl || !erh || !tab.waehleFormat(format)) return false;
        custom->click();
        bool gef = false;
        for (int l = 0; l < lvl->count(); l++) {
            lvl->setCurrentIndex(l);
            for (int e = 0; e < erh->count(); e++) {
                erh->setCurrentIndex(e);
                if (sichtbar(tab, "checkWeakBits", &gef)) return true;
            }
        }
        return false;
    }

private slots:

    void initTestCase()
    {
        QVERIFY2(uft_register_all_formats() == UFT_OK,
                 "Format-Registry liess sich nicht aufbauen");
    }

    /* Die Voraussetzung: die beiden Formate sagen wirklich Verschiedenes. */
    void manifeste_unterscheiden_sich()
    {
        const uft_format_plugin_t *d64 = uft_get_format_plugin_by_name("D64");
        const uft_format_plugin_t *scp = uft_get_format_plugin_by_name("SCP");
        QVERIFY2(d64, "Plugin D64 nicht in der Registry");
        QVERIFY2(scp, "Plugin SCP nicht in der Registry");
        QVERIFY2(uft_plugin_control_visibility(d64, "Flux") !=
                 uft_plugin_control_visibility(scp, "Flux"),
                 "D64 und SCP fuehren \"Flux\" gleich — dann kann dieser "
                 "Test die Verdrahtung nicht belegen.");
        QCOMPARE(uft_plugin_control_visibility(d64, "Flux"), UFT_CONTROL_HIDE);
    }

    /* MF-1320 (E-15): ein Tor, dessen Anker fehlen, darf das nicht
     * verschweigen. Der alte Reiter meldete 4 fehlende Anker (alle vier
     * Gruppen waren beim Umbau verschwunden, das Tor blendete fuer KEIN
     * Format etwas aus). Im neuen Formular sind alle drei Anker da — und
     * dann muss das Ausblenden auch belegt sein: die Flussfelder
     * verschwinden bei D64 und stehen bei SCP. */
    void gruppentor_hat_seine_anker_und_blendet_aus()
    {
        FormatTab tab;
        QVERIFY(tab.waehleFormat(QStringLiteral("D64")));
        QCOMPARE(tab.gruppenTorFehlendeAnker(), 0);
        bool gef = false;
        const bool beiD64 = sichtbar(tab, "checkIncludeRawFlux", &gef);
        QVERIFY2(gef, "checkIncludeRawFlux fehlt im Formular");
        QVERIFY(tab.waehleFormat(QStringLiteral("SCP")));
        const bool beiSCP = sichtbar(tab, "checkIncludeRawFlux", &gef);
        QVERIFY2(!beiD64, "D64 kennt keinen Fluss — das Flussfeld gehoert weg");
        QVERIFY2(beiSCP, "SCP ist ein Flussformat — das Flussfeld gehoert hin");
        /* Die Beschriftung-lose Checkbox und ihr Nachbar gehen gemeinsam. */
        QCOMPARE(sichtbar(tab, "checkReadBetweenIndex", &gef), beiSCP);
    }

    /* Die Unterscheidung am Parameter-Tor: `preserve_weak_bits` haengt an
     * UFT_CAP_WEAK_BITS. Ein Format mit der Flagge und eines ohne muessen
     * dasselbe Feld verschieden zeigen — sonst hat das Tor nichts getan. */
    void parametertor_unterscheidet_die_formate()
    {
        const QString mit  = formatMit(UFT_FORMAT_CAP_WEAK_BITS, true);
        const QString ohne = formatMit(UFT_FORMAT_CAP_WEAK_BITS, false);
        QVERIFY2(!mit.isEmpty() && !ohne.isEmpty(),
                 "Kein Formatpaar mit/ohne Weak-Bits-Flagge in der Registry");
        FormatTab tab;
        bool gef = false;
        QVERIFY2(stellungMitWeakBits(tab, mit), qPrintable(QStringLiteral(
            "Keine Planstellung zeigt das Weak-Bits-Feld bei %1").arg(mit)));
        /* dieselbe Stellung, anderes Format */
        QVERIFY2(tab.waehleFormat(ohne), qPrintable(ohne));
        const bool b = sichtbar(tab, "checkWeakBits", &gef);
        QVERIFY(gef);
        QVERIFY2(!b, qPrintable(QStringLiteral(
            "Weak-Bits-Feld bei %1 (ohne Flagge) sichtbar, bei gleicher "
            "Planstellung wie %2 (mit Flagge) — das Tor unterscheidet nicht")
            .arg(ohne, mit)));
    }

    /* MF-1323/1618: jede angebotene Format-Zeile hat ein Plugin, und jedes
     * registrierte Plugin ist irgendwo anwaehlbar. Gemessen zur LAUFZEIT
     * ueber die Registry. „Automatisch" traegt keinen Namen (leere Daten)
     * und ist die Abwesenheit einer Wahl. */
    void die_formatliste_nennt_nur_was_es_gibt()
    {
        FormatTab tab;
        auto *sys = tab.findChild<QComboBox *>("comboSystem");
        auto *fmt = tab.findChild<QComboBox *>("comboFormat");
        QVERIFY(sys && fmt);
        QSet<QString> angeboten;
        QStringList ohne;
        for (int s = 0; s < sys->count(); s++) {
            sys->setCurrentIndex(s);
            for (int i = 0; i < fmt->count(); i++) {
                const QString name = fmt->itemData(i).toString();
                if (name.isEmpty()) continue;
                angeboten.insert(name);
                if (!uft_get_format_plugin_by_name(name.toUtf8().constData()))
                    ohne << name;
            }
        }
        QVERIFY2(ohne.isEmpty(), qPrintable(QStringLiteral(
            "Formatnamen ohne Plugin: %1").arg(ohne.join(", "))));
        QStringList nie;
        for (size_t i = 0; i < uft_registered_format_plugin_count(); i++) {
            const uft_format_plugin_t *p = uft_registered_format_plugin_at(i);
            if (p && p->name && *p->name &&
                !angeboten.contains(QString::fromUtf8(p->name)))
                nie << QString::fromUtf8(p->name);
        }
        QVERIFY2(nie.isEmpty(), qPrintable(QStringLiteral(
            "Registrierte Formate, die nirgends anwaehlbar sind: %1").arg(nie.join(", "))));
    }

    /* Unbekanntes Format: NICHTS wegen einer Faehigkeit ausblenden. Der
     * Reiter oeffnet auf „Automatisch" — das ist genau dieser Fall. */
    void unbekanntes_format_versteckt_nichts()
    {
        FormatTab tab;
        QVERIFY2(tab.getSelectedFormat().isEmpty(), "Reiter oeffnet nicht auf Automatisch");
        QVERIFY2(!tab.copyPlanCapsBekannt(),
                 "Automatisch gilt als nachschlagbar — dann sagt die Zusage nichts");
        bool gef = false;
        QVERIFY2(sichtbar(tab, "checkIncludeRawFlux", &gef) && gef,
                 "Flussfeld bei unbekanntem Format ausgeblendet");
        /* Auf einer Stellung, auf der die Ebene das Weak-Bits-Feld zeigt,
         * darf ein UNBEKANNTES Format es nicht verbergen. */
        const QString mit = formatMit(UFT_FORMAT_CAP_WEAK_BITS, true);
        QVERIFY(stellungMitWeakBits(tab, mit));
        QVERIFY(tab.waehleFormat(QString()));        /* Automatisch */
        QVERIFY2(sichtbar(tab, "checkWeakBits", &gef) && gef,
                 "Weak-Bits-Feld bei unbekanntem Format ausgeblendet — ein "
                 "Nachschlagefehler kostete dann Funktion");
    }
};

QTEST_MAIN(TestFormatTabCapabilityGating)
#include "test_format_tab_capability_gating.moc"

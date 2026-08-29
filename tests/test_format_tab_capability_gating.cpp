/**
 * @file test_format_tab_capability_gating.cpp
 * @brief Zwei Formate, verschiedene Bedienelemente (MF-661)
 *
 * Abnahme für Stufe 2 aus `docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md`.
 *
 * ── Was hier bewiesen wird ───────────────────────────────────────────────
 *
 * Nicht „die Methode läuft durch", sondern: **die Oberfläche zeigt für
 * verschiedene Formate wirklich Verschiedenes, und zwar das, was deren
 * Manifest ansagt.** Eine Verdrahtung, die für alle Formate dasselbe
 * anzeigt, hat nichts verdrahtet.
 *
 * Der Test geht deshalb nicht über `applyPluginCapabilities()` direkt,
 * sondern setzt das **Format-Auswahlfeld** — also den Weg, den ein
 * Benutzer nimmt — und liest danach die Sichtbarkeit der echten
 * Widgets. Ein Beweis, der den fraglichen Pfad nicht durchläuft,
 * beweist nichts (MF-649).
 *
 * ── Die Erwartung, und woher sie kommt ───────────────────────────────────
 *
 * Über alle 88 Manifeste gemessen (MF-660):
 *
 *     Flux         zeigen  7   verstecken 81
 *     Weak Bits    zeigen  2   verstecken 86
 *
 * Ein Sektorformat wie D64 kann mit Fluss-Bedienelementen nichts
 * anfangen; ein Flussformat wie SCP schon. Genau dieser Unterschied
 * muss in der Oberfläche ankommen.
 *
 * ── Und die Regel für den Unglücksfall ───────────────────────────────────
 *
 * Findet sich kein Plugin zum Namen, wird NICHTS ausgeblendet. Auf
 * Unwissen zu verstecken nähme dem Benutzer Funktion wegen eines
 * Nachschlagefehlers von uns. Auch das steht hier als Zusicherung.
 */

#include <QtTest/QtTest>
#include <QComboBox>
#include <QWidget>

#include "formattab.h"
#include "ui_tab_format.h"

extern "C" {
#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
}

class TestFormatTabCapabilityGating : public QObject
{
    Q_OBJECT

private:
    /* Sichtbarkeit einer Gruppe nach Auswahl eines Formats.
     * Geht ueber das Auswahlfeld, nicht ueber die Methode. */
    static bool gruppeSichtbar(FormatTab &tab, const QString &format,
                               const char *gruppe, bool *gefunden)
    {
        QComboBox *combo = tab.findChild<QComboBox *>("comboFormat");
        *gefunden = false;
        if (!combo) return false;

        int idx = combo->findText(format);
        if (idx < 0) {
            /* Format steht nicht im aktuellen System-Filter — dann
             * direkt eintragen, damit der Test nicht an der
             * System-Auswahl haengt. */
            combo->addItem(format);
            idx = combo->findText(format);
        }
        if (idx < 0) return false;
        combo->setCurrentIndex(idx);

        QWidget *w = tab.findChild<QWidget *>(QString::fromLatin1(gruppe));
        if (!w) return false;
        *gefunden = true;
        return w->isVisible() || !w->isHidden();
    }

private slots:

    void initTestCase()
    {
        QVERIFY2(uft_register_all_formats() == UFT_OK,
                 "Format-Registry liess sich nicht aufbauen");
    }

    /* Die Voraussetzung: die beiden Formate sagen wirklich Verschiedenes.
     * Ohne diese Pruefung koennte der Test spaeter still bedeutungslos
     * werden, wenn jemand ein Manifest aendert. */
    void manifeste_unterscheiden_sich()
    {
        const uft_format_plugin_t *d64 = uft_get_format_plugin_by_name("D64");
        const uft_format_plugin_t *scp = uft_get_format_plugin_by_name("SCP");
        QVERIFY2(d64, "Plugin D64 nicht in der Registry");
        QVERIFY2(scp, "Plugin SCP nicht in der Registry");

        const uft_control_visibility_t vd =
            uft_plugin_control_visibility(d64, "Flux");
        const uft_control_visibility_t vs =
            uft_plugin_control_visibility(scp, "Flux");

        QVERIFY2(vd != vs,
                 "D64 und SCP fuehren \"Flux\" gleich — dann kann dieser "
                 "Test die Verdrahtung nicht belegen. Ein anderes Paar "
                 "waehlen oder das Manifest pruefen.");
        QCOMPARE(vd, UFT_CONTROL_HIDE);   /* Sektorformat, kein Fluss */
    }

    /* Der Kern. */
    void sektorformat_versteckt_flussgruppe()
    {
        FormatTab tab;
        bool gefunden = false;

        const bool sichtbarD64 =
            gruppeSichtbar(tab, "D64", "groupFlux", &gefunden);
        QVERIFY2(gefunden, "groupFlux nicht im Format-Tab gefunden");
        QVERIFY2(!sichtbarD64,
                 "D64 fuehrt \"Flux\" als UNSUPPORTED — die Flussgruppe "
                 "gehoert ausgeblendet");

        const bool sichtbarSCP =
            gruppeSichtbar(tab, "SCP", "groupFlux", &gefunden);
        QVERIFY2(gefunden, "groupFlux nach SCP-Wahl verschwunden");
        QVERIFY2(sichtbarSCP,
                 "SCP ist ein Flussformat — die Flussgruppe gehoert "
                 "gezeigt");
    }

    /* Dasselbe fuer die PLL-Gruppe, die an demselben Merkmal haengt. */
    void pll_folgt_demselben_merkmal()
    {
        FormatTab tab;
        bool gefunden = false;

        const bool d64 = gruppeSichtbar(tab, "D64", "groupPLL", &gefunden);
        if (!gefunden) QSKIP("groupPLL nicht vorhanden");
        QVERIFY2(!d64, "D64: PLL-Gruppe gehoert ausgeblendet");

        const bool scp = gruppeSichtbar(tab, "SCP", "groupPLL", &gefunden);
        QVERIFY2(scp, "SCP: PLL-Gruppe gehoert gezeigt");
    }

    /* Unbekanntes Format: NICHTS ausblenden. */
    void unbekanntes_format_versteckt_nichts()
    {
        FormatTab tab;
        bool gefunden = false;

        const bool sichtbar =
            gruppeSichtbar(tab, "GIBTESNICHT", "groupFlux", &gefunden);
        QVERIFY2(gefunden, "groupFlux nicht gefunden");
        QVERIFY2(sichtbar,
                 "Zu einem unbekannten Format darf NICHTS ausgeblendet "
                 "werden — sonst kostet ein Nachschlagefehler von uns den "
                 "Benutzer Funktion");
    }
};

QTEST_MAIN(TestFormatTabCapabilityGating)
#include "test_format_tab_capability_gating.moc"

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

    /* MF-1320 (E-15): der Kern hat sich verschoben, und zwar aus einem
     * gemessenen Grund.
     *
     * Hier standen zwei Faelle, die `groupFlux` und `groupPLL` auf
     * Sichtbarkeit prueften. Beide Gruppen gibt es nicht mehr: der
     * GUI-Umbau hat alle VIER Anker von `applyPluginCapabilities()`
     * entfernt (je 1 Treffer in der HEAD-Fassung von
     * `forms/tab_format.ui`, je 0 in der neuen). Das Tor lief seither
     * durch und blendete fuer KEIN Format etwas aus — lautlos, weil es
     * `if (!w) continue;` sagte.
     *
     * Der Test bleibt stehen, weil er den Fund GEFANGEN hat. Er prueft
     * jetzt zwei Dinge statt einer Gruppe:
     *   1. dass das Gruppentor seine fehlenden Anker MELDET,
     *   2. dass die Faehigkeits-Unterscheidung trotzdem stattfindet —
     *      auf der Parameter-Ebene, wo sie hingehoert (E-15).
     */
    void gruppentor_meldet_seine_fehlenden_anker()
    {
        FormatTab tab;
        bool gefunden = false;
        (void)gruppeSichtbar(tab, "D64", "groupFlux", &gefunden);

        const int fehlt = tab.gruppenTorFehlendeAnker();

        /* GEMESSEN, nicht gewaehlt: `applyPluginCapabilities()` kennt vier
         * Anker — groupFlux, groupPLL, groupWrite, groupProtection — und
         * `forms/tab_format.ui` traegt heute keinen davon (je 1 Treffer in
         * der HEAD-Fassung, je 0 in der neuen).
         *
         * Die erste Fassung dieser Zusage lautete `fehlt >= 0` und war zu
         * schwach: der Zaehler startet bei -1, und ohne die Ruecksetzung
         * auf 0 zaehlt er von dort hoch und landet bei 3 — auch das ist
         * >= 0. Die Mutation blieb gruen. Eine Zusage, die ihre eigene
         * Verletzung nicht sieht, ist keine. */
        QVERIFY2(fehlt == 4,
                 qPrintable(QString(
                     "Das Gruppentor meldet %1 fehlende Anker, gemessen sind "
                     "es 4 (groupFlux, groupPLL, groupWrite, "
                     "groupProtection — keiner mehr in tab_format.ui). "
                     "Weicht die Zahl ab, hat entweder jemand eine Gruppe "
                     "zurueckgebracht — dann gehoert hier belegt, dass sie "
                     "auch ausgeblendet wird — oder das Tor laeuft nicht "
                     "mehr.").arg(fehlt)));

        /* Der eigentliche Satz: ein Tor, dessen Anker fehlen, darf das
         * nicht verschweigen. Heute fehlen alle vier. Findet jemand die
         * Gruppen wieder ein, faellt die Zahl auf 0 — und dann muss der
         * Fall darunter das Ausblenden belegen. */
        if (fehlt > 0) {
            QVERIFY2(!gefunden,
                     "Widerspruch: das Tor meldet fehlende Anker, aber die "
                     "Gruppe wurde gefunden.");
        }
    }

    /* Die Faehigkeits-Unterscheidung, dort gemessen, wo sie seit dem
     * Umbau wirklich stattfindet: am Parameter-Tor.
     *
     * `flux.sample_clock` ist an `ui->comboSampleRate` gebunden
     * (`src/formattab.cpp`, Tafel `bindung[]`) und verlangt
     * `UFT_CAP_FLUX_IO`. D64 fuehrt „Flux" als UNSUPPORTED, SCP nicht —
     * das ist im Fall `manifeste_unterscheiden_sich` oben belegt. Also
     * muss dasselbe Bedienelement bei den beiden Formaten verschieden
     * dastehen. Tut es das nicht, hat das Tor nichts getan. */
    void parametertor_unterscheidet_die_formate()
    {
        FormatTab tab;
        bool gefunden = false;

        const bool beiD64 =
            gruppeSichtbar(tab, "D64", "comboSampleRate", &gefunden);
        QVERIFY2(gefunden,
                 "comboSampleRate nicht im Format-Tab — dann ist der Zeuge "
                 "dieses Tests weg und die Zusage unbelegt");

        const bool beiSCP =
            gruppeSichtbar(tab, "SCP", "comboSampleRate", &gefunden);
        QVERIFY2(gefunden, "comboSampleRate nach SCP-Wahl verschwunden");

        QVERIFY2(beiD64 != beiSCP,
                 "D64 und SCP zeigen `flux.sample_clock` gleich. Der "
                 "Parameter verlangt UFT_CAP_FLUX_IO, und nur eines der "
                 "beiden Formate hat es — ein Tor, das fuer beide dasselbe "
                 "tut, hat nichts verdrahtet.");
        QVERIFY2(!beiD64,
                 "D64 ist ein Sektorformat ohne Fluss — der Abtasttakt "
                 "gehoert dort ausgeblendet");
    }

    /* MF-1323: die Systemliste der Oberflaeche gegen die Registry.
     *
     * `FormatTab` haelt in `m_systemFormats` eine von HAND gepflegte
     * Zuordnung „System -> Formatnamen" und fuellt daraus `comboFormat`.
     * MF-1245 hat dieselbe Klasse bei den DATEIDIALOGEN behoben — sieben
     * gepflegte Endungslisten, alle verschieden — und dabei die
     * Systemliste nicht angefasst.
     *
     * Der Anlass ist ein Einzelfund aus dem Formate-Integrationsplan:
     * die ZX-Spectrum-Zeile nennt „OPD", und ein Plugin dieses Namens
     * gibt es nicht — das Format liest `OPUS`. Wer „OPD" waehlt, waehlt
     * nichts.
     *
     * Gemessen wird hier zur LAUFZEIT ueber die Registry und nicht per
     * Textsuche im Quelltext: zwei eigene Versuche, die Namen aus
     * `formattab.cpp` und den Plugin-Definitionen zu greppen, lieferten
     * 29 bzw. 42 angeblich fehlende Namen — darunter ATR, IMG und EDSK,
     * die es sicher gibt. Eine Messung, die in beide Richtungen irrt,
     * ist keine. Die Registry weiss es selbst. */
    void die_formatliste_nennt_nur_was_es_gibt()
    {
        FormatTab tab;
        QComboBox *combo = tab.findChild<QComboBox *>("comboFormat");
        QVERIFY2(combo, "comboFormat nicht gefunden");
        QVERIFY2(combo->count() > 0, "comboFormat ist leer");

        QStringList ohne;
        for (int i = 0; i < combo->count(); i++) {
            const QString name = combo->itemText(i).trimmed();
            if (name.isEmpty()) continue;
            if (!uft_get_format_plugin_by_name(name.toUtf8().constData()))
                ohne << name;
        }

        QVERIFY2(ohne.isEmpty(),
                 qPrintable(QString(
                     "Die Formatauswahl bietet %1 von %2 Namen an, zu denen "
                     "die Registry KEIN Plugin hat: %3. Wer einen davon "
                     "waehlt, waehlt nichts — und das Faehigkeits-Tor "
                     "bekommt kein Plugin, also blendet es nichts aus.")
                     .arg(ohne.size()).arg(combo->count())
                     .arg(ohne.join(", "))));
    }

    /* Unbekanntes Format: NICHTS ausblenden. */
    void unbekanntes_format_versteckt_nichts()
    {
        FormatTab tab;
        bool gefunden = false;

        /* MF-1320: derselbe Satz, an einem Zeugen, den es noch gibt.
         *
         * `comboSampleRate` traegt `flux.sample_clock` und verlangt
         * UFT_CAP_FLUX_IO. Bei einem unbekannten Format liefert
         * `copyPlanCaps()` 0 — genau wie bei einem Format ohne Fluss. Wer
         * die beiden Faelle zusammenwirft, blendet wegen eines
         * NACHSCHLAGEFEHLERS aus. `copyPlanCapsBekannt()` trennt sie. */
        const bool sichtbar =
            gruppeSichtbar(tab, "GIBTESNICHT", "comboSampleRate", &gefunden);
        QVERIFY2(gefunden, "comboSampleRate nicht gefunden");
        QVERIFY2(sichtbar,
                 "Zu einem unbekannten Format darf NICHTS ausgeblendet "
                 "werden — sonst kostet ein Nachschlagefehler von uns den "
                 "Benutzer Funktion");
        QVERIFY2(!tab.copyPlanCapsBekannt(),
                 "Der Reiter haelt ein erfundenes Format fuer nachschlagbar "
                 "— dann sagt die Zusage darueber nichts.");
    }
};

QTEST_MAIN(TestFormatTabCapabilityGating)
#include "test_format_tab_capability_gating.moc"

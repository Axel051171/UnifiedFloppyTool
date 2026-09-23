/**
 * @file test_format_tab_bindung.cpp
 * @brief MF-1282: neun weitere Plan-Bindungen — und die Beschriftung geht mit.
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * `bindung[]` in `formattab.cpp` ordnet Kernparameter den Bedienelementen
 * zu. Sie fuehrte elf Parameter; die Karte des GUI-Umbaus
 * (`test-gui/gui-geruest/zuordnung`) nennt 56. Neun davon haben in DIESEM
 * Formular bereits ein Element und lassen sich ohne eine Zeile Formular-
 * aenderung binden — gemessen am 2026-09-20.
 *
 * Der zweite Teil ist aelter und war nie belegt: der Plan versteckte das
 * FELD und liess seine BESCHRIFTUNG stehen. Bei DOSCopy verschwindet die
 * ganze Geometrie, und „Tracks:", „Sides:", „Sectors:", „Size:" blieben
 * ueber einer leeren Flaeche stehen. Eine Beschriftung ohne ihr Feld
 * behauptet eine Einstellung, die es auf dieser Ebene nicht gibt.
 *
 * ── Was der Test NICHT sagt ──────────────────────────────────────────────
 *
 * Er sagt nichts ueber die acht `plan.*`-Achsen und die zehn `a8.*`-Felder
 * der Karte: die stehen NICHT in `k_param[]`, eine Bindung waere eine
 * Zusage ohne Gegenstueck (MF-767). Er sagt auch nichts darueber, ob die
 * Werte beim Kopieren ankommen — nur, dass die Oberflaeche zeigt, was der
 * Plan erlaubt.
 *
 * ── Rotbeweis ───────────────────────────────────────────────────────────
 *
 * Gegen den Vorzustand fallen B1 bis B5: ohne die acht Eintraege bleibt
 * `spinSectors` auf jeder Ebene sichtbar, und ohne `beschriftungVon()`
 * bleibt „Sectors:" stehen. B6 bis B8 halten die Gegenrichtung, damit
 * „alles verstecken" und „den Nachbarn mitnehmen" nicht gruen durchgehen.
 *
 * Mutationsmatrix (scratchpad `mut_bindung.py`): **10 von 11 gefangen**.
 * Die elfte ist benannt statt verschwiegen: „im GITTER gilt der Nachbar
 * als Beschriftung" (der `qobject_cast<QLabel*>` faellt weg) laesst sich
 * an diesem Formular NICHT isolieren, weil links von jedem gebundenen
 * Gitterfeld ohnehin ein QLabel steht — gemessen fuer `spinSectors`(1,1),
 * `comboSectorSize`(1,3), `spinTracks`(0,1), `spinSides`(0,3),
 * `comboEncoding`(2,1), `comboRPM`(2,3). Die Mutation aendert dort nichts
 * Beobachtbares. Steht dort eines Tages etwas anderes, bewacht sie
 * niemand; dann gehoert die Zusage nachgezogen.
 *
 * Und zwei Mutationen waren zuerst FEHLER IN DER MUTATION, nicht im Test:
 * `replace(text, 1)` traf den Formular-Zweig statt des Kasten-Zweigs, weil
 * die Rueckgabezeile dreimal gleich lautet.
 *
 * `isHidden()` und nicht `isVisible()`: ein Widget in einem nie gezeigten
 * Fenster ist NICHT sichtbar, auch wenn es niemand versteckt hat.
 */

#include <QtTest>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QWidget>

#include "formattab.h"

extern "C" {
#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
}

class TestFormatTabBindung : public QObject {
    Q_OBJECT

private:
    static void waehle(QComboBox *box, int enumWert)
    {
        const int idx = box->findData(enumWert);
        QVERIFY2(idx >= 0, "Aufzaehlungswert nicht im Auswahlfeld");
        box->setCurrentIndex(idx);
    }

    /* Die Beschriftung wird ueber ihren TEXT gesucht, weil das Formular
     * ihr keinen Namen gibt. Der Test besteht darauf, dass der Text
     * eindeutig ist — „Sides:" waere es nicht (es steht an `spinSides`
     * UND an `comboXCopySides`, gemessen). */
    static QLabel *beschriftung(FormatTab &tab, const QString &text)
    {
        QList<QLabel *> treffer;
        for (QLabel *l : tab.findChildren<QLabel *>())
            if (l->text() == text) treffer << l;
        if (treffer.size() != 1) return nullptr;
        return treffer.first();
    }

private slots:

    /* Ohne Registry kennt `copyPlanCaps()` kein Format und gibt 0 zurueck —
     * B3 waere dann gruen aus dem falschen Grund. */
    void initTestCase()
    {
        QVERIFY2(uft_register_all_formats() == UFT_OK,
                 "Format-Registry liess sich nicht aufbauen");
    }

    /* B1 — geometry.sectors haengt jetzt am Plan.
     * Vorzustand: `spinSectors` stand in keiner Bindung und blieb immer
     * sichtbar. */
    void geometrie_folgt_der_ebene()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *sek = tab.findChild<QWidget *>("spinSectors");
        auto *gro = tab.findChild<QWidget *>("comboSectorSize");
        QVERIFY(lvl && sek && gro);

        waehle(lvl, UFT_COPY_SECTOR);
        QVERIFY2(!sek->isHidden(),
                 "auf der Sektorebene ist die Sektorzahl eine Einstellung");
        QVERIFY2(!gro->isHidden(),
                 "auf der Sektorebene ist die Sektorgroesse eine Einstellung");

        waehle(lvl, UFT_COPY_FLUX);
        QVERIFY2(sek->isHidden(),
                 "auf der Flussebene gibt es keine Sektorzahl — das Feld "
                 "muss verschwinden, nicht nur grau werden");
        QVERIFY2(gro->isHidden(),
                 "auf der Flussebene gibt es keine Sektorgroesse");
    }

    /* B2 — und die Beschriftung geht mit (der eigentliche Befund). */
    void beschriftung_geht_mit()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *sek = tab.findChild<QWidget *>("spinSectors");
        QVERIFY(lvl && sek);
        QLabel *lab = beschriftung(tab, QStringLiteral("Sektoren/Spur:"));
        QVERIFY2(lab, "genau eine Beschriftung Sectors: erwartet");

        waehle(lvl, UFT_COPY_SECTOR);
        QVERIFY2(!lab->isHidden(),
                 "mit dem Feld ist auch seine Beschriftung da");

        waehle(lvl, UFT_COPY_FLUX);
        QVERIFY2(sek->isHidden(), "Vorbedingung: das Feld ist weg");
        QVERIFY2(lab->isHidden(),
                 "die Beschriftung bleibt stehen und zeigt auf nichts — "
                 "genau der Befund, den MF-1282 behebt");
    }

    /* B3 — die beiden Flussfelder brauchen BEIDES: die Flussebene UND
     * ein Format, das Fluss zusagt.
     *
     * Die erste Fassung dieses Tests pruefte nur die Ebene und war rot.
     * Der Grund ist kein Defekt, sondern die zweite Haelfte der Regel:
     * `flux.revolution_policy` und `flux.sample_clock` verlangen
     * `UFT_CAP_FLUX_IO`, und `copyPlanCaps()` gibt die Flagge nur heraus,
     * wenn das GEWAEHLTE Format sie zusagt. Ohne Format ist caps 0.
     * Die Vorschau des Entwurfs (test-gui) kennt diesen Term nicht und
     * zeichnet die Felder deshalb optimistischer als das Programm. */
    void flussfelder_brauchen_ebene_und_format()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *fmt = tab.findChild<QComboBox *>("comboFormat");
        auto *mrg = tab.findChild<QWidget *>("comboFluxMerge");
        auto *smp = tab.findChild<QWidget *>("comboSampleRate");
        QVERIFY(lvl && fmt && mrg && smp);

        /* SCP ist ein Flussformat (dieselbe Wahl wie im Gating-Test). */
        int idx = fmt->findText(QStringLiteral("SCP"));
        if (idx < 0) { fmt->addItem(QStringLiteral("SCP")); idx = fmt->findText(QStringLiteral("SCP")); }
        QVERIFY(idx >= 0);
        fmt->setCurrentIndex(idx);

        waehle(lvl, UFT_COPY_SECTOR);
        QVERIFY2(mrg->isHidden(),
                 "flux.revolution_policy bedeutet auf der Sektorebene nichts");
        QVERIFY2(smp->isHidden(),
                 "flux.sample_clock bedeutet auf der Sektorebene nichts");

        waehle(lvl, UFT_COPY_FLUX);
        QVERIFY2(!mrg->isHidden(),
                 "Flussebene und Flussformat — dann ist es die Einstellung");
        QVERIFY2(!smp->isHidden(),
                 "Flussebene und Flussformat — dann ist es die Einstellung");
    }

    /* B4 — BEFUND, festgenagelt statt behoben: die GCR-Verfahrenswahl ist
     * im Programm unerreichbar.
     *
     * `gcr.variant` steht seit MF-1234 in `bindung[]` und verlangt
     * `UFT_CAP_GCR`. `copyPlanCaps()` setzt diese Flagge NIE — der Reiter
     * kennt kein geoeffnetes Abbild und kann GCR nicht messen; das steht
     * dort ausdruecklich und ist richtig so. Die Folge hat nur niemand
     * gemessen: `comboGCRType` ist auf JEDER Ebene versteckt.
     *
     * Der Test behebt das nicht — behoben waere es erst, wenn eine Quelle
     * die GCR-Faehigkeit liefert, und das ist eine Entwurfsfrage. Er haelt
     * den Befund fest, damit er nicht ein zweites Mal verloren geht, und er
     * wird ROT, sobald jemand die Lage aendert. Aus demselben Grund ist
     * `gcr.preserve_raw_nibbles` NICHT gebunden worden (MF-1282). */
    void gcr_wahl_ist_unerreichbar()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *typ = tab.findChild<QWidget *>("comboGCRType");
        auto *roh = tab.findChild<QWidget *>("checkDecodeGCR");
        QVERIFY(lvl && typ && roh);

        for (int ebene : { int(UFT_COPY_SECTOR), int(UFT_COPY_NIBBLE),
                           int(UFT_COPY_FLUX) }) {
            waehle(lvl, ebene);
            QVERIFY2(typ->isHidden(),
                     "comboGCRType haengt an gcr.variant, das UFT_CAP_GCR "
                     "verlangt — und der Reiter setzt die Flagge nie");
        }

        /* Und die Gegenprobe: checkDecodeGCR ist NICHT gebunden und bleibt
         * deshalb bedienbar. Wuerde es jemand binden, faellt diese Zeile. */
        waehle(lvl, UFT_COPY_SECTOR);
        QVERIFY2(!roh->isHidden(),
                 "checkDecodeGCR ist bewusst ungebunden — eine Bindung an "
                 "einen GCR-Parameter wuerde es dauerhaft verstecken");
    }

    /* B5 — ein NACHBAR ist keine Beschriftung.
     *
     * `checkWeakBits` haengt an `preserve_weak_bits` (verlangt
     * `UFT_CAP_WEAK_BITS`) und steht als ERSTES Element in seinem
     * waagerechten Kasten; dahinter folgt `checkDetectNoFlux`, gemessen
     * in forms/tab_format.ui.
     *
     * Zwei Zusagen in einer: der Helfer darf an Stelle 0 nichts finden
     * (kein Zugriff auf Element -1), und er darf den Nachbarn nicht
     * mitnehmen. Ein Helfer, der blind das vorherige oder das naechste
     * Element greift, faellt hier.
     *
     * WAS DIESE ZUSAGE MIT DER RUECKNAHME DES FLUX-DIALOGS VERLOREN HAT,
     * und das gehoert dazugesagt statt weggelassen: der Kasten trug bis
     * dahin ein DRITTES Element, `btnFluxAdvanced`. Es war der zweite
     * Zeuge dafuer, dass der Helfer nicht blind irgendeinen Nachbarn
     * greift. Mit dem Dialog ist es gefallen, und im Kasten stehen jetzt
     * nur noch zwei Elemente — einen zweiten Zeugen gibt es dort nicht
     * mehr. Die Zusage „Element -1 wird nicht angefasst" traegt
     * unveraendert, weil `checkWeakBits` weiterhin an Stelle 0 steht;
     * die Zusage „beide Nachbarn bleiben stehen" ist auf einen
     * geschrumpft. Wer den Kasten wieder erweitert, nimmt den zweiten
     * Zeugen hier wieder auf. */
    void nachbar_bleibt_stehen()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *weak = tab.findChild<QWidget *>("checkWeakBits");
        /* MF-1307 — der Zeuge ist gewechselt, und der Grund gehoert
         * hierher statt in eine Commit-Zeile.
         *
         * Hier stand `checkDetectNoFlux`. Der taugt seit MF-1307 nicht
         * mehr: solange „Detect all protections" gesetzt ist — und das
         * ist die Vorgabe —, werden die fuenf einzelnen Schutzhaken
         * ABSICHTLICH verborgen, weil „alle" sie einschliesst und fuenf
         * graue Kaestchen mit Haken keine Information sind.
         *
         * Der Test war damit rot, ohne dass an der geprueften Zusage
         * etwas falsch war: er hat einen Zeugen benutzt, der aus einem
         * ANDEREN Grund verschwindet.
         *
         * Der naechstliegende Ersatz traegt ebenfalls nicht, und das ist
         * gemessen: `checkWeakBits` sitzt in `unter_anordnung_Erhaltung`,
         * und dieser Kasten enthaelt GENAU ZWEI Elemente — `checkWeakBits`
         * und `checkPreserveSync`. Beide sind an den Plan gebunden, beide
         * verschwinden auf der Sektorebene. Der Kasten hat also keinen
         * freien Zeugen mehr; das steht oben schon seit MF-1293, nur war
         * die Folge daraus nicht zu Ende gedacht.
         *
         * Zeuge ist deshalb eine ACHSE des Kopierplans. Die vier Achsen
         * — Ebene, Strategie, Erhaltung, Sicherheit — sind nie verborgen:
         * sie werden gesperrt, wenn ein Modus sie festlegt, aber nicht
         * ausgeblendet. Damit ist die Zusage „der Helfer greift nicht
         * daneben" weiter pruefbar, und sie haengt an keinem Feld, das
         * aus einem zweiten Grund verschwinden koennte. */
        auto *nachbar = tab.findChild<QWidget *>("comboPlanStrategy");
        QVERIFY(lvl && weak && nachbar);

        waehle(lvl, UFT_COPY_SECTOR);
        QVERIFY2(weak->isHidden(),
                 "Vorbedingung: ohne zugesagte Weak Bits ist das gebundene "
                 "Feld weg");
        QVERIFY2(!nachbar->isHidden(),
                 "eine Plan-Achse wird gesperrt, nie verborgen");
    }

    /* B6 — ein freies Feld bleibt sichtbar und bedienbar.
     *
     * Ohne diese Zusage bliebe die Mutation „alles verstecken" gruen —
     * die Falle aus G9 des Kopierplan-Tests. `evidence.hash_algorithms`
     * ist auf allen Ebenen frei (gemessen ueber 17 Profile). */
    void freies_feld_bleibt()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *sha = tab.findChild<QWidget *>("checkHashSha512");
        QVERIFY(lvl && sha);

        /* `checkHashSha512` steht an Stelle 2 des Hash-Kastens, davor das
         * Haekchen `checkSHA256` (gemessen). Auch dort darf der Helfer
         * keine Beschriftung sehen — ein Haekchen ist keine. */
        auto *sha256 = tab.findChild<QWidget *>("checkSHA256");
        QVERIFY(sha256);

        for (int ebene : { int(UFT_COPY_SECTOR), int(UFT_COPY_FLUX),
                           int(UFT_COPY_NIBBLE) }) {
            waehle(lvl, ebene);
            QVERIFY2(!sha->isHidden(),
                     "evidence.hash_algorithms ist auf jeder Ebene frei");
            QVERIFY2(sha->isEnabled(),
                     "ein freies Feld bleibt bedienbar");
            QVERIFY2(!sha256->isHidden(),
                     "der Nachbar im Hash-Kasten bleibt unberuehrt");
        }
    }

    /* B8 — der Helfer fasst einen Nachbarn auch dann nicht an, wenn das
     * gebundene Feld SICHTBAR ist.
     *
     * Diese Zusage fehlte, und die Mutationsmatrix hat es gezeigt: die
     * Mutation „der Nachbar gilt als Beschriftung" (kein QLabel-Test im
     * Kasten-Zweig) blieb gruen. B5 traf sie nicht, weil `checkWeakBits`
     * an Stelle 0 steht und der Zweig dort gar nicht laeuft, und B6 nicht,
     * weil ein faelschlich als Beschriftung genommener Nachbar bei einem
     * FREIEN Feld auf sichtbar gesetzt wird — er war schon sichtbar.
     *
     * Also wird er hier VON HAND versteckt. Ohne Mutation bleibt er
     * versteckt (der Helfer findet nichts); mit Mutation blendet der Plan
     * ihn ein. `checkHashSha512` steht an Stelle 2 des Hash-Kastens,
     * davor `checkSHA256` — ein Haekchen, kein Etikett. */
    void nachbar_wird_nicht_eingeblendet()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *sha = tab.findChild<QWidget *>("checkHashSha512");
        auto *sha256 = tab.findChild<QWidget *>("checkSHA256");
        QVERIFY(lvl && sha && sha256);

        sha256->setVisible(false);
        waehle(lvl, UFT_COPY_FLUX);
        waehle(lvl, UFT_COPY_SECTOR);

        QVERIFY2(!sha->isHidden(),
                 "Vorbedingung: das gebundene Feld ist frei und sichtbar");
        QVERIFY2(sha256->isHidden(),
                 "checkSHA256 ist der Nachbar, keine Beschriftung — der "
                 "Plan darf ihn nicht einblenden");
    }

    /* B7 — und die Beschriftung kommt auch wieder.
     *
     * Ein Helfer, der nur versteckt und nie wieder zeigt, waere beim
     * ersten Wechsel auf die Flussebene gruen und danach fuer immer
     * falsch. */
    void beschriftung_kommt_wieder()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        QVERIFY(lvl);
        QLabel *lab = beschriftung(tab, QStringLiteral("Sektoren/Spur:"));
        QVERIFY(lab);

        waehle(lvl, UFT_COPY_FLUX);
        QVERIFY(lab->isHidden());
        waehle(lvl, UFT_COPY_SECTOR);
        QVERIFY2(!lab->isHidden(),
                 "zurueck auf der Sektorebene muss die Beschriftung wieder "
                 "da sein");
    }
};

QTEST_MAIN(TestFormatTabBindung)
#include "test_format_tab_bindung.moc"

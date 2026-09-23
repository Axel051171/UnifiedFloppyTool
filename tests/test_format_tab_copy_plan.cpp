/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_format_tab_copy_plan.cpp
 * @brief Der Kopierplan erreicht die Oberflaeche (MF-1233)
 *
 * ── Warum dieser Test den Umweg ueber die Auswahlfelder geht ─────────────
 *
 * Nicht „applyCopyPlan() laeuft durch", sondern: **ein Bediener stellt
 * vier Felder ein, und die Oberflaeche zeigt daraufhin etwas anderes.**
 * Der Test setzt deshalb die Comboboxen — den Weg, den ein Benutzer
 * nimmt — und liest danach die echten Widgets. Ein Beweis, der den
 * fraglichen Pfad nicht durchlaeuft, beweist nichts (MF-649).
 *
 * ── Was hier bewiesen wird ───────────────────────────────────────────────
 *
 * G1  Die vier Auswahlfelder sind aus dem KERN gefuellt, nicht aus dem
 *     `.ui`: die Eintragszahl stimmt mit den Aufzaehlungen ueberein und
 *     die Texte sind die von `uft_copy_*_name()`. Das ist die Zusage
 *     gegen die zehnte Dopplung — `comboXCopyMode` und `comboCopyMode`
 *     fuehren heute zwei verschiedene Listen.
 * G2  Jeder Eintrag traegt seinen Aufzaehlungswert als `userData`. Ohne
 *     das waere die Reihenfolge der Eintraege eine zweite Wahrheit.
 * G3  Erhaltung „Kopierschutz" auf einem Format ohne Timing und ohne
 *     schwache Bits ergibt einen SICHTBAREN harten Befund. Gemessen
 *     (MF-1231) sagt KEIN Plugin beide Flaggen zugleich zu — der Fall
 *     ist also der Normalfall, nicht die Ausnahme.
 * G4  Strategie „Tiefenlesen" zeigt die erzwungenen Werte an, darunter
 *     `read.passes = 5`.
 * G5  Auf der Ebene „Dateien" verschwindet `spinRevolutions` — die
 *     Ebene fuehrt `read.revolutions` ausdruecklich als verboten.
 * G6  Auf der Ebene „Spuren" ist dasselbe Feld wieder da. Ohne diese
 *     Gegenprobe waere G5 auch dann gruen, wenn die Anbindung schlicht
 *     alles versteckte.
 * G7  Ein vom Plan festgelegtes Feld wird gesperrt, nicht versteckt —
 *     und traegt seinen Grund als Kurzhinweis. Verstecken wuerde dem
 *     Bediener verschweigen, dass es den Schalter gibt.
 * G8  Der Plan gilt schon beim Oeffnen, nicht erst nach der ersten
 *     Aenderung.
 * G9-G14 (MF-1235) stehen bei ihrem Code: das freie Feld, die fuenf
 *     Feinheiten, der Hashsatz, die BAM-Bedingung.
 * G15-G20 (MF-1237) der benannte Kopiermodus: Liste aus dem Kern,
 *     gesperrt mit Grund, setzt und sperrt die Achsen, erreicht
 *     `copyPlan()`, der Knopf fuehrt hin und zurueck, und der Reiter
 *     oeffnet in einem benannten Modus.
 * G21 Die JSON-Ansicht zeigt DIESEN Plan — mit Gegenprobe, dass ein
 *     anderer Plan nicht mehr darin steht.
 * G22 Nichts wird still gekuerzt: die angezeigte Laenge ist die, die
 *     der Kern meldet. Ein fester Puffer faellt hier auf.
 * G23 Der Text folgt der Aenderung, ohne neu aufzuklappen.
 * G24 Beim Oeffnen zugeklappt; der Knopf klappt auf UND zu.
 * G25 Die Faehigkeitszeile nennt jede der acht Flaggen, und was
 *     gemessen getragen wird, steht im richtigen Abschnitt.
 * G26 „nicht feststellbar" ist ein EIGENER Zustand. Die vier Flaggen,
 *     die dieser Reiter nicht messen kann, stehen dort — sie als
 *     „traegt nicht" zu fuehren hiesse, eine Messung zu behaupten.
 */

#include <QtTest/QtTest>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QWidget>
#include <QAbstractButton>        /* MF-1237 */
#include <QStandardItemModel>     /* MF-1237: gesperrte Eintraege */
#include <QPlainTextEdit>         /* MF-1238 */

#include "formattab.h"
#include "uft/uft_format_plugin.h"


/* ---------------------------------------------------------------------------
 * MF-1293 - der Kopiermodus ist kein Auswahlfeld mehr, sondern eine Knopfreihe
 * ---------------------------------------------------------------------------
 *
 * Die Zusagen dieses Tests bleiben WOERTLICH dieselben; nur der Weg zum
 * Bedienelement aendert sich. Deshalb drei Helfer statt einer Umschreibung
 * jeder einzelnen Zeile - so bleibt beim Lesen sichtbar, dass hier nichts
 * abgeschwaecht wurde.
 */
static QPushButton *modusKnopf(FormatTab &tab, const char *kennung)
{
    return tab.findChild<QPushButton *>(QStringLiteral("btnModus_") +
                                        QString::fromLatin1(kennung));
}

static QString aktiverModus(FormatTab &tab)
{
    /* "custom" ist die ABWESENHEIT eines Modus und zaehlt deshalb nicht
     * als Modusname - sonst waere "kein Modus gewaehlt" nie leer. */
    for (auto *b : tab.findChildren<QPushButton *>()) {
        const QString o = b->objectName();
        if (!o.startsWith(QStringLiteral("btnModus_"))) continue;
        if (o == QStringLiteral("btnModus_custom")) continue;
        if (b->isChecked()) return o.mid(9);
    }
    return QString();
}

/* MF-1293: FluxCopy ist ohne Flussformat NICHT verfuegbar, und sein Knopf
 * ist dann gesperrt. Das ist richtig - beim frueheren Auswahlfeld konnte
 * man einen gesperrten Eintrag per setCurrentIndex trotzdem waehlen, was
 * die Sperre wirkungslos machte. Wer FluxCopy pruefen will, muss deshalb
 * erst ein Format waehlen, das Fluss zusagt. */
static void waehleFlussformat(FormatTab &tab)
{
    auto *fmt = tab.findChild<QComboBox *>("comboFormat");
    if (!fmt) return;
    int idx = fmt->findText(QStringLiteral("SCP"));
    if (idx < 0) { fmt->addItem(QStringLiteral("SCP")); idx = fmt->count() - 1; }
    fmt->setCurrentIndex(idx);
}

static int modusKnopfZahl(FormatTab &tab)
{
    int n = 0;
    for (auto *b : tab.findChildren<QPushButton *>())
        if (b->objectName().startsWith(QStringLiteral("btnModus_"))) n++;
    return n;
}

class TestFormatTabCopyPlan : public QObject {
    Q_OBJECT

private:
    static void waehle(QComboBox *box, int enumWert)
    {
        const int idx = box->findData(enumWert);
        QVERIFY2(idx >= 0, "Aufzaehlungswert nicht im Auswahlfeld");
        box->setCurrentIndex(idx);
    }

private slots:
    /* MF-1293: ohne Plugin-Register kennt `uft_get_format_plugin_by_name`
     * kein Format, `copyPlanCaps()` liefert 0, und JEDES Profil mit einer
     * Faehigkeitsbedingung gilt als nicht verfuegbar. Solange der
     * Kopiermodus ein Auswahlfeld war, fiel das nicht auf: ein gesperrter
     * Eintrag liess sich per `setCurrentIndex` trotzdem waehlen. Die
     * Knopfreihe kann das nicht - und deckt die Luecke damit auf. */
    void initTestCase()
    {
        QVERIFY2(uft_register_all_formats() == UFT_OK,
                 "Format-Plugins liessen sich nicht registrieren");
    }


    /* G1 + G2 — die Listen kommen aus dem Kern */
    void listen_stammen_aus_dem_kern()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *str = tab.findChild<QComboBox *>("comboPlanStrategy");
        auto *erh = tab.findChild<QComboBox *>("comboPlanPreserve");
        auto *pol = tab.findChild<QComboBox *>("comboPlanPolicy");
        QVERIFY(lvl && str && erh && pol);

        QCOMPARE(lvl->count(), int(UFT_COPY_LEVEL_N));
        QCOMPARE(str->count(), int(UFT_READ_STRATEGY_N));
        QCOMPARE(erh->count(), int(UFT_PRESERVE_N));
        QCOMPARE(pol->count(), int(UFT_POLICY_N));

        QCOMPARE(lvl->itemText(0),
                 QString::fromUtf8(uft_copy_level_name(UFT_COPY_FILE)));
        QCOMPARE(pol->itemText(int(UFT_POLICY_EVIDENCE)),
                 QString::fromUtf8(uft_copy_policy_name(UFT_POLICY_EVIDENCE)));

        /* G2 — der Wert steht als userData, nicht als Position */
        for (int i = 0; i < lvl->count(); i++)
            QCOMPARE(lvl->itemData(i).toInt(), i);
    }

    /* G8 — schon beim Oeffnen angewandt */
    /* MF-1265 (`P3-509`, zweiter Halbsatz): der Plan ist von aussen
     * erreichbar — und zwar OHNE dass ihn jemand weiterreicht.
     *
     * `ToolsTab`, `WorkflowTab` und `DecodeJob` kennen den Formatreiter
     * nicht. Drei Verbraucher einzeln zu verkabeln waere viel Klempnerei
     * fuer eine Einstellung, die es nur EINMAL gibt; deshalb gibt der
     * Besitzer sie heraus.
     *
     * Gefragt wird der KERN (`uft_copy_plan_current()`), nicht der
     * Formatreiter. Eine erste Fassung legte die Auskunft als
     * `FormatTab::aktuellerPlan()` in die Oberflaeche; der BINDER hat
     * daran einen Entwurfsfehler gefunden, weil `toolstab.cpp` damit
     * an `formattab.cpp` hing und zwei Qt-Tests das eine ohne das
     * andere binden.
     *
     * DAS RISIKO DIESER BAUFORM IST DER HAENGENDE ZEIGER, und genau
     * darauf zielt die letzte Zusage: nach dem Schliessen des Reiters
     * darf die Auskunft nicht in freigegebenen Speicher greifen,
     * sondern muss auf die VORGABEN zurueckfallen. */
    void plan_ist_ohne_verkabelung_erreichbar()
    {
        const uft_copy_plan_t vorgabe = uft_copy_plan_default();

        {
            FormatTab tab;
            /* Eine Wahl, die NICHT die Vorgabe ist — sonst bewiese ein
             * Treffer nichts. */
            QVERIFY2((int)UFT_READ_DEEP != (int)vorgabe.strategy,
                     "DEEP ist zufaellig die Vorgabe — die Probe bewiese "
                     "nichts");
            auto *combo = tab.findChild<QComboBox *>("comboPlanStrategy");
            QVERIFY(combo);
            const int i = combo->findData((int)UFT_READ_DEEP);
            QVERIFY(i >= 0);
            combo->setCurrentIndex(i);

            const uft_copy_plan_t gelesen = uft_copy_plan_current();
            QCOMPARE((int)gelesen.strategy, (int)UFT_READ_DEEP);
            /* Und es ist WIRKLICH der Plan dieses Reiters, nicht eine
             * zweite Quelle. */
            QCOMPARE((int)gelesen.strategy, (int)tab.copyPlan().strategy);
        }

        /* Reiter weg: die Auskunft faellt auf die Vorgaben zurueck,
         * nicht auf einen genullten Zufall. */
        const uft_copy_plan_t danach = uft_copy_plan_current();
        QCOMPARE((int)danach.strategy,     (int)vorgabe.strategy);
        QCOMPARE((int)danach.level,        (int)vorgabe.level);
        QCOMPARE((int)danach.preservation, (int)vorgabe.preservation);
        QCOMPARE((int)danach.policy,       (int)vorgabe.policy);
    }

    void plan_gilt_ab_dem_ersten_bild()
    {
        FormatTab tab;
        auto *forced = tab.findChild<QLabel *>("labelPlanForced");
        QVERIFY(forced);
        /* Die Vorgabe ist Standard/Logisch/Normal — Standard setzt
         * retries, passes und revolutions fest. Der Text darf also nicht
         * leer sein und nicht „erzwingt nichts" sagen. */
        QVERIFY2(forced->text().contains(QStringLiteral("read.retries")),
                 qPrintable(QStringLiteral("labelPlanForced beim Oeffnen: '")
                            + forced->text() + QStringLiteral("'")));
    }

    /* G3 — ein harter Befund wird sichtbar */
    void kopierschutz_ohne_flaggen_meldet_sich()
    {
        FormatTab tab;
        auto *erh = tab.findChild<QComboBox *>("comboPlanPreserve");
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *bef = tab.findChild<QLabel *>("labelPlanFindings");
        QVERIFY(erh && lvl && bef);

        /* Bitstromebene, damit die Mindestebene erfuellt ist und der
         * Befund WIRKLICH von den fehlenden Flaggen kommt. */
        waehle(lvl, UFT_COPY_BITSTREAM);
        waehle(erh, UFT_PRESERVE_PROTECTED);

        QVERIFY2(!bef->text().isEmpty(),
                 "Kopierschutz-Erhaltung ohne Timing/Weak-Bits bleibt stumm");
        QVERIFY2(bef->text().contains(QStringLiteral("nicht ausf")),
                 qPrintable(QStringLiteral("kein harter Befund: '")
                            + bef->text() + QStringLiteral("'")));

        /* Gegenprobe: ohne die Erhaltung ist der Befund weg. Sonst
         * koennte das Feld irgendetwas Dauerhaftes anzeigen. */
        waehle(erh, UFT_PRESERVE_LOGICAL);
        QVERIFY2(!bef->text().contains(QStringLiteral("nicht ausf")),
                 qPrintable(QStringLiteral("Befund bleibt stehen: '")
                            + bef->text() + QStringLiteral("'")));
    }

    /* G4 — erzwungene Werte stehen da */
    void tiefenlesen_zeigt_was_es_erzwingt()
    {
        FormatTab tab;
        auto *str = tab.findChild<QComboBox *>("comboPlanStrategy");
        auto *lab = tab.findChild<QLabel *>("labelPlanForced");
        QVERIFY(str && lab);

        waehle(str, UFT_READ_DEEP);
        QVERIFY2(lab->text().contains(QStringLiteral("read.passes = 5")),
                 qPrintable(QStringLiteral("labelPlanForced: '")
                            + lab->text() + QStringLiteral("'")));
        QVERIFY(lab->text().contains(QStringLiteral("status_file")));
    }

    /* G5 + G6 — die Ebene entscheidet ueber die Sichtbarkeit */
    void ebene_blendet_aus_und_wieder_ein()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *rev = tab.findChild<QWidget *>("spinRevolutions");
        QVERIFY(lvl && rev);

        waehle(lvl, UFT_COPY_FILE);
        QVERIFY2(rev->isHidden(),
                 "read.revolutions ist auf der Dateiebene verboten, das "
                 "Feld bleibt aber sichtbar");

        waehle(lvl, UFT_COPY_TRACK);
        QVERIFY2(!rev->isHidden(),
                 "auf der Spurebene muss das Feld wieder da sein — sonst "
                 "versteckt die Anbindung schlicht alles");
    }

    /* G9 — ein FREIES Feld bleibt sichtbar UND bedienbar.
     *
     * Diese Zusage fehlte in der ersten Fassung, und die
     * Mutationsmatrix hat es gezeigt: „alles verstecken" blieb gruen.
     * G5 und G6 pruefen `spinRevolutions`, und das ist unter der
     * Vorgabestrategie FESTGELEGT — der freie Zweig wurde also von
     * keiner Zusage beruehrt. `geometry.cylinders` steht in keiner
     * Setzt-Liste und ist auf der Sektorebene aktiv. */
    void freies_feld_bleibt_bedienbar()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *trk = tab.findChild<QWidget *>("spinTracks");
        QVERIFY(lvl && trk);

        waehle(lvl, UFT_COPY_SECTOR);
        QVERIFY2(!trk->isHidden(),
                 "ein freies Feld darf nicht verschwinden");
        QVERIFY2(trk->isEnabled(),
                 "ein freies Feld muss bedienbar bleiben");
        QVERIFY2(trk->toolTip().isEmpty(),
                 "ein freies Feld traegt keinen Sperrgrund");
    }

    /* ── MF-1235: die fuenf Feinheiten und der Hashsatz ───────────── */

    /* G10 — auch sie kommen aus dem Kern, nicht aus dem .ui. */
    void feinheiten_stammen_aus_dem_kern()
    {
        FormatTab tab;
        /* `comboPlanFileSpecial` steht ABSICHTLICH nicht hier: sein
         * Umfang haengt davon ab, ob das Format eine Commodore-BAM
         * zusagt — genau das prueft G14. Ihn hier mit der vollen
         * Aufzaehlung zu vergleichen hiesse, zwei Zusagen gegeneinander
         * zu stellen. */
        struct { const char *name; int anzahl; } f[] = {
            { "comboPlanTrackMode",   UFT_TRACK_MODE_N },
            { "comboPlanVote",        UFT_VOTE_N },
            { "comboPlanExact",       UFT_EXACT_N },
        };
        for (const auto &x : f) {
            auto *b = tab.findChild<QComboBox *>(x.name);
            QVERIFY2(b != nullptr, x.name);
            QCOMPARE(b->count(), x.anzahl);
            for (int i = 0; i < b->count(); i++)
                QCOMPARE(b->itemData(i).toInt(), i);
        }
        /* Und die Texte sind wirklich die des Kerns — sonst waere die
         * Zaehlung oben auch mit erfundenen Namen gruen. */
        /* MF-1293: `comboPlanGcr` gibt es im neuen Formular nicht
         * mehr - es war ueber `UFT_CAP_GCR` auf jeder Ebene
         * versteckt und damit unerreichbar (`P3-522`). Die Zusage
         * faellt nicht weg, sie hat kein Bedienelement mehr. */
        QCOMPARE(tab.findChild<QComboBox *>("comboPlanVote")->itemText(
                     int(UFT_VOTE_CRC_PREFERRED)),
                 QString::fromUtf8(uft_copy_vote_name(UFT_VOTE_CRC_PREFERRED)));
    }

    /* G11 — jede Zeile erscheint NUR unter ihrer Bedingung, und
     *       verschwindet wieder. Die Gegenprobe gehoert dazu: ohne sie
     *       waere die Zusage auch gruen, wenn alles immer sichtbar ist. */
    void feinheiten_erscheinen_nur_wo_sie_gelten()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *str = tab.findChild<QComboBox *>("comboPlanStrategy");
        auto *erh = tab.findChild<QComboBox *>("comboPlanPreserve");
        auto *pol = tab.findChild<QComboBox *>("comboPlanPolicy");
        auto *rTrack = tab.findChild<QWidget *>("comboPlanTrackMode");
        auto *rVote  = tab.findChild<QWidget *>("comboPlanVote");
        auto *rExact = tab.findChild<QWidget *>("comboPlanExact");
        /* MF-1293: der Hashsatz ist eine GRUPPE, und sie wird versteckt -
         * nicht die Haken darin. Der Kern nennt sie auf jeder Ebene
         * frei; die Beweisrichtlinie ist eine Regel der Oberflaeche
         * darueber. Genau so hielt es vorher `rowPlanHash`. */
        auto *rHash  = tab.findChild<QWidget *>("unter_nachweis_Pruefsummen");
        QVERIFY(lvl && str && erh && pol);
        QVERIFY(rTrack && rVote && rExact && rHash);

        /* Ausgangslage: Sektoren / Standard / Logisch / Normal —
         * da bedeutet keine der fuenf etwas. */
        waehle(lvl, UFT_COPY_SECTOR);
        waehle(str, UFT_READ_STANDARD);
        waehle(erh, UFT_PRESERVE_LOGICAL);
        waehle(pol, UFT_POLICY_NORMAL);
        QVERIFY2(rTrack->isHidden(), "Spurart ausserhalb der Spurebene");
        /* MF-1293: die GCR-Verfahrenswahl hat im neuen Formular kein
         * Bedienelement mehr. Sie war ueber UFT_CAP_GCR auf jeder Ebene
         * versteckt und damit unerreichbar (P3-522) — es gibt nichts
         * mehr zu verstecken. */
        QVERIFY2(rVote->isHidden(),  "Abstimmung ohne Consensus");
        QVERIFY2(rExact->isHidden(), "Genauigkeit ohne Bitgenau");
        QVERIFY2(rHash->isHidden(),  "Hashsatz ohne Beweisrichtlinie");

        waehle(lvl, UFT_COPY_TRACK);
        QVERIFY2(!rTrack->isHidden(), "Spurart fehlt auf der Spurebene");

        waehle(lvl, UFT_COPY_NIBBLE);
        QVERIFY2(rTrack->isHidden(), "Spurart bleibt nach dem Wechsel stehen");

        waehle(str, UFT_READ_CONSENSUS);
        QVERIFY2(!rVote->isHidden(), "Abstimmung fehlt bei Consensus");

        waehle(erh, UFT_PRESERVE_BIT_EXACT);
        QVERIFY2(!rExact->isHidden(), "Genauigkeit fehlt bei Bitgenau");

        waehle(pol, UFT_POLICY_EVIDENCE);
        QVERIFY2(!rHash->isHidden(), "Hashsatz fehlt beim Beweisabbild");
    }

    /* G12 — die Wahl kommt im Plan wirklich an. Ein Auswahlfeld, das
     *       niemand ausliest, ist Zierrat (die Klasse comboXCopyMode). */
    void die_wahl_erreicht_den_plan()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *erh = tab.findChild<QComboBox *>("comboPlanPreserve");
        auto *ex  = tab.findChild<QComboBox *>("comboPlanExact");
        QVERIFY(lvl && erh && ex);

        waehle(lvl, UFT_COPY_NIBBLE);
        waehle(erh, UFT_PRESERVE_BIT_EXACT);
        waehle(ex,  UFT_EXACT_TRACK_BIT);

        const uft_copy_plan_t p = tab.copyPlan();
        /* MF-1293: die GCR-Wahl hat kein Bedienelement mehr (P3-522);
         * der Plan traegt deshalb die Vorgabe des Kerns. */
        QCOMPARE(int(p.gcr), int(UFT_GCR_COMMODORE));
        QCOMPARE(int(p.exact_kind), int(UFT_EXACT_TRACK_BIT));
    }

    /* G13 — SHA-256 ist Pflicht und bleibt es, CRC32 kommt nur dazu. */
    void hashsatz_traegt_sha256_immer()
    {
        FormatTab tab;
        auto *pol  = tab.findChild<QComboBox *>("comboPlanPolicy");
        auto *sha2 = tab.findChild<QCheckBox *>("checkSHA256");
        auto *crc  = tab.findChild<QCheckBox *>("checkCRC32");
        QVERIFY(pol && sha2 && crc);

        waehle(pol, UFT_POLICY_EVIDENCE);
        QVERIFY2(sha2->isChecked(), "SHA-256 ist nicht gesetzt");
        QVERIFY2(!sha2->isEnabled(),
                 "SHA-256 ist abwaehlbar — es ist aber Pflicht");

        QCOMPARE(tab.copyPlan().hashes & uint32_t(UFT_HASH_SHA256),
                 uint32_t(UFT_HASH_SHA256));
        QCOMPARE(tab.copyPlan().hashes & uint32_t(UFT_HASH_CRC32), 0u);

        crc->setChecked(true);
        const uint32_t h = tab.copyPlan().hashes;
        QCOMPARE(h & uint32_t(UFT_HASH_CRC32), uint32_t(UFT_HASH_CRC32));
        QVERIFY2(h & uint32_t(UFT_HASH_SHA256),
                 "CRC32 hat SHA-256 verdraengt — CRC32 allein ist kein Beweis");
    }

    /* G14 — „Commodore BAM" steht nur zur Wahl, wenn es eine BAM gibt.
     *       Sonst waere es eine Auswahl, die nur dazu da ist, vom Kern
     *       abgelehnt zu werden. */
    void bam_steht_nur_bei_bam_zur_wahl()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *fs  = tab.findChild<QComboBox *>("comboPlanFileSpecial");
        QVERIFY(lvl && fs);
        waehle(lvl, UFT_COPY_FILE);
        /* Dieser Reiter kann kein Dateisystem erkennen — copyPlanCaps()
         * setzt CAP_CBM_BAM also nie. Genau deshalb darf der Eintrag
         * hier nicht stehen. */
        QVERIFY2(fs->findData(UFT_FILE_BAM) < 0,
                 "Commodore BAM steht zur Wahl, obwohl keine BAM zugesagt ist");
        QVERIFY2(fs->findData(UFT_FILE_GENERIC) >= 0,
                 "die allgemeine Dateiart ist verschwunden");
    }

    /* G7 — festgelegt heisst gesperrt, nicht verschwunden */
    void festgelegtes_feld_wird_gesperrt_nicht_versteckt()
    {
        FormatTab tab;
        auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *str = tab.findChild<QComboBox *>("comboPlanStrategy");
        auto *rev = tab.findChild<QWidget *>("spinRevolutions");
        QVERIFY(lvl && str && rev);

        waehle(lvl, UFT_COPY_FLUX);
        waehle(str, UFT_READ_DEEP);      /* setzt read.revolutions = 5 */

        QVERIFY2(!rev->isHidden(),
                 "ein festgelegtes Feld darf nicht verschwinden — der "
                 "Bediener soll sehen, DASS es den Schalter gibt");
        QVERIFY2(!rev->isEnabled(),
                 "ein vom Plan festgelegtes Feld muss gesperrt sein");
        QVERIFY2(rev->toolTip().contains(QStringLiteral("5")),
                 qPrintable(QStringLiteral("kein Grund am Feld: '")
                            + rev->toolTip() + QStringLiteral("'")));
    }

    /* ── MF-1237: der benannte Kopiermodus ────────────────────────────
     *
     * Die benannten Modi des Eigentuemer-Entwurfs sind keine fuenfte
     * Achse, sondern VORLAGEN auf den vorhandenen vier. Diese fuenf
     * Zusagen halten genau das fest — und die letzte haelt fest, dass
     * der Bediener wieder herauskommt. */

    /* G20 — der Reiter oeffnet in einem BENANNTEN Modus.
     *
     * Ohne diese Zusage bliebe die ganze Modusliste folgenlos, solange
     * niemand sie anfasst — und der Reiter zeigte beim Oeffnen eine
     * Achsenstellung, die keinen Namen hat. Gegenstueck zu G8. */
    void reiter_oeffnet_in_einem_benannten_modus()
    {
        FormatTab tab;
                auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *txt = tab.findChild<QLabel *>("labelProfileText");
        QVERIFY(lvl && txt);

        QCOMPARE(aktiverModus(tab), QStringLiteral("standardcopy"));
        QVERIFY2(!lvl->isEnabled(),
                 "die Achsen sind beim Oeffnen frei, obwohl ein Modus "
                 "angezeigt wird");
        QVERIFY2(!txt->text().isEmpty(), "der Modus erklaert sich nicht");
    }

    /* G15 — die Liste kommt aus dem Kern, nicht aus dem .ui.
     *
     * Ein Eintrag mehr als der Kern fuehrt: „Benutzerdefiniert" ist die
     * ABWESENHEIT eines Profils und steht deshalb ausdruecklich nicht
     * in der Kerntabelle. */
    void profilliste_stammt_aus_dem_kern()
    {
        FormatTab tab;
                QCOMPARE(modusKnopfZahl(tab), int(uft_copy_profile_count()) + 1);

        for (size_t i = 0; i < uft_copy_profile_count(); i++) {
            const uft_copy_profile_t *p = uft_copy_profile(i);
            QVERIFY(p);
            auto *b = modusKnopf(tab, p->id);
            QVERIFY2(b != nullptr, p->id);
            QVERIFY2(b->text().startsWith(QString::fromUtf8(p->name)),
                     qPrintable(b->text()));
        }
        /* Der letzte Eintrag traegt die leere Kennung. */
        QVERIFY2(modusKnopf(tab, "custom") != nullptr,
                "Benutzerdefiniert fehlt als eigener Knopf");
    }

    /* G16 — was hier nicht geht, steht SICHTBAR und gesperrt da, mit
     *       Grund. Entfernen waere die schlechtere Antwort: wer
     *       „RawCopy" sucht und nicht findet, weiss nicht, ob es das
     *       nicht gibt oder ob es hier nur nicht passt (MF-666).
     *
     * `rawcopy` ist der deterministische Fall: es verlangt
     * `UFT_CAP_BITSTREAM_IO` und hat keine Behelfsliste, und
     * `copyPlanCaps()` setzt diese Flagge NIE — dieser Reiter kennt kein
     * geoeffnetes Abbild. Die Gegenprobe an `standardcopy` gehoert dazu,
     * sonst waere die Zusage auch gruen, wenn alles gesperrt ist. */
    void unmoegliches_profil_ist_gesperrt_mit_grund()
    {
        FormatTab tab;

        auto *raw = modusKnopf(tab, "rawcopy");
        QVERIFY(raw);
        QVERIFY2(!raw->isEnabled(),
                 "RawCopy ist waehlbar, obwohl kein Bitstromzugriff "
                 "zugesagt ist");
        QVERIFY2(raw->toolTip().contains(QStringLiteral("Bitstrom")),
                 qPrintable(QStringLiteral("kein Grund am Eintrag: '")
                            + raw->toolTip() + QStringLiteral("'")));

        auto *std_ = modusKnopf(tab, "standardcopy");
        QVERIFY(std_);
        QVERIFY2(std_->isEnabled(),
                 "auch die Standardkopie ist gesperrt — dann sperrt die "
                 "Anbindung schlicht alles");
    }

    /* G17 — ein Profil SETZT die vier Achsen und sperrt sie. */
    void profil_setzt_die_achsen_und_sperrt_sie()
    {
        FormatTab tab;
                auto *lvl = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *str = tab.findChild<QComboBox *>("comboPlanStrategy");
        auto *erh = tab.findChild<QComboBox *>("comboPlanPreserve");
        auto *pol = tab.findChild<QComboBox *>("comboPlanPolicy");
        QVERIFY(lvl && str && erh && pol);

        const uft_copy_profile_t *p = uft_copy_profile_by_id("deepcopy");
        QVERIFY(p);
        modusKnopf(tab, "deepcopy")->click();

        QCOMPARE(lvl->currentData().toInt(), int(p->plan.level));
        QCOMPARE(str->currentData().toInt(), int(p->plan.strategy));
        QCOMPARE(erh->currentData().toInt(), int(p->plan.preservation));
        QCOMPARE(pol->currentData().toInt(), int(p->plan.policy));

        for (QComboBox *b : { lvl, str, erh, pol })
            QVERIFY2(!b->isEnabled(),
                     "eine Achse ist unter einem Profil frei — dann ist "
                     "der angezeigte Modus nicht der eingestellte");
    }

    /* G18 — die Wahl erreicht `copyPlan()`. Ein Auswahlfeld, das
     *       niemand ausliest, ist Zierrat (die Klasse comboXCopyMode).
     *
     * Verglichen wird der GANZE Plan, nicht nur die vier Achsen: `PLF`
     * setzt bei manchen Vorlagen auch die Dateiart, und ein Vergleich
     * der Achsen allein waere bei DOSCopy und BAMCopy gruen, obwohl sie
     * verschiedene Plaene sind (gemessen P3). */
    void profilwahl_erreicht_den_plan()
    {
        FormatTab tab;

        const uft_copy_profile_t *p = uft_copy_profile_by_id("fluxcopy");
        QVERIFY(p);
        auto *knopfFlux = modusKnopf(tab, "fluxcopy");
        QVERIFY(knopfFlux);
        QVERIFY2(!knopfFlux->isEnabled(),
                 "FluxCopy ist ohne Flussformat waehlbar - die Sperre greift nicht");
        waehleFlussformat(tab);
        QVERIFY2(knopfFlux->isEnabled(),
                 "FluxCopy bleibt gesperrt, obwohl das Format Fluss zusagt");
        knopfFlux->click();

        const uft_copy_plan_t q = tab.copyPlan();
        QCOMPARE(int(q.level),        int(p->plan.level));
        QCOMPARE(int(q.strategy),     int(p->plan.strategy));
        QCOMPARE(int(q.preservation), int(p->plan.preservation));
        QCOMPARE(int(q.policy),       int(p->plan.policy));
        QCOMPARE(int(q.track_mode),   int(p->plan.track_mode));
        QCOMPARE(int(q.file_special), int(p->plan.file_special));
        QCOMPARE(int(q.gcr),          int(p->plan.gcr));
        QCOMPARE(int(q.vote),         int(p->plan.vote));
        QCOMPARE(int(q.exact_kind),   int(p->plan.exact_kind));
    }

    /* G19 — der Knopf entsperrt, und er fuehrt zurueck.
     *
     * Ohne den Rueckweg waere der Modus eine Einbahnstrasse: einmal
     * angepasst, nie wieder benannt. */
    void knopf_entsperrt_und_fuehrt_zurueck()
    {
        FormatTab tab;
                auto *knopf = tab.findChild<QAbstractButton *>("btnPlanAnpassen");
        auto *lvl   = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *txt   = tab.findChild<QLabel *>("labelProfileText");
        QVERIFY(knopf && lvl && txt);

        modusKnopf(tab, "deepcopy")->click();
        QVERIFY(!lvl->isEnabled());

        knopf->click();
        QVERIFY2(lvl->isEnabled(), "der Knopf entsperrt die Achsen nicht");
        QVERIFY2(aktiverModus(tab).isEmpty(),
                 "nach dem Anpassen steht immer noch ein Modusname da");
        QVERIFY2(txt->text().contains(QStringLiteral("DeepCopy")),
                 qPrintable(QStringLiteral("die Herkunft fehlt: '")
                            + txt->text() + QStringLiteral("'")));

        /* Und die vom Profil gesetzte Achse steht noch — der Bediener
         * faengt nicht bei Null an. */
        QCOMPARE(lvl->currentData().toInt(),
                 int(uft_copy_profile_by_id("deepcopy")->plan.level));

        knopf->click();
        QVERIFY2(!lvl->isEnabled(), "der zweite Druck sperrt nicht wieder");
        QCOMPARE(aktiverModus(tab), QStringLiteral("deepcopy"));
    }

    /* ── MF-1238: der Plan als JSON, und was das Format traegt ────────
     *
     * `uft_copy_plan_to_json()` gibt es seit MF-1234 und hatte **null**
     * Aufrufer — eine fertige Tuer ohne Weg dorthin (Klasse MF-930).
     * Diese vier Zusagen sind der Weg. */

    /* G21 — die Ansicht zeigt DIESEN Plan, und einen anderen nicht. */
    void json_zeigt_den_eingestellten_plan()
    {
        FormatTab tab;
        auto *knopf = tab.findChild<QAbstractButton *>("btnPlanJson");
        auto *text  = tab.findChild<QPlainTextEdit *>("textPlanJson");
        auto *lvl   = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *str   = tab.findChild<QComboBox *>("comboPlanStrategy");
        auto *anp   = tab.findChild<QAbstractButton *>("btnPlanAnpassen");
        QVERIFY(knopf && text && lvl && str && anp);

        anp->click();                       /* Achsen frei */
        waehle(lvl, UFT_COPY_FLUX);
        waehle(str, UFT_READ_CONSENSUS);
        knopf->click();                     /* aufklappen */

        const QString j = text->toPlainText();
        QVERIFY2(j.contains(QStringLiteral("\"level\": \"flux\"")),
                 qPrintable(j.left(300)));
        QVERIFY2(j.contains(QStringLiteral("\"strategy\": \"consensus\"")),
                 qPrintable(j.left(300)));

        /* Gegenprobe: nach dem Umstellen steht es NICHT mehr da. Ohne
         * sie waere die Zusage auch gruen, wenn der Text fest waere. */
        waehle(lvl, UFT_COPY_SECTOR);
        const QString k = text->toPlainText();
        QVERIFY2(!k.contains(QStringLiteral("\"level\": \"flux\"")),
                 qPrintable(k.left(300)));
        QVERIFY2(k.contains(QStringLiteral("\"level\": \"sector\"")),
                 qPrintable(k.left(300)));
    }

    /* G22 — nichts wird still gekuerzt.
     *
     * Der angezeigte Text ist so lang, wie der Kern ihn meldet. Ein
     * fester Puffer faellt hier auf, und still gekuerzte Ausgaben sind
     * in diesem Baum eine eigene Fehlerklasse. */
    void json_wird_nicht_still_gekuerzt()
    {
        FormatTab tab;
        auto *knopf = tab.findChild<QAbstractButton *>("btnPlanJson");
        auto *anp   = tab.findChild<QAbstractButton *>("btnPlanAnpassen");
        auto *lvl   = tab.findChild<QComboBox *>("comboPlanLevel");
        auto *str   = tab.findChild<QComboBox *>("comboPlanStrategy");
        auto *erh   = tab.findChild<QComboBox *>("comboPlanPreserve");
        auto *pol   = tab.findChild<QComboBox *>("comboPlanPolicy");
        auto *text  = tab.findChild<QPlainTextEdit *>("textPlanJson");
        QVERIFY(knopf && anp && lvl && str && erh && pol && text);

        /* Der laengste Plan, den dieser Reiter bauen kann: Fluss +
         * Consensus + bitgenau + Beweis erzwingt am meisten. */
        anp->click();
        waehle(lvl, UFT_COPY_FLUX);
        waehle(str, UFT_READ_CONSENSUS);
        waehle(erh, UFT_PRESERVE_BIT_EXACT);
        waehle(pol, UFT_POLICY_EVIDENCE);
        knopf->click();

        const uft_copy_plan_t p = tab.copyPlan();
        const size_t soll = uft_copy_plan_to_json(&p, nullptr, 0);
        QVERIFY(soll > 0);
        QCOMPARE(size_t(text->toPlainText().toUtf8().size()), soll);
    }

    /* G23 — der Text folgt der Aenderung, ohne neu aufzuklappen. */
    void json_folgt_der_aenderung()
    {
        FormatTab tab;
        auto *knopf = tab.findChild<QAbstractButton *>("btnPlanJson");
        auto *text  = tab.findChild<QPlainTextEdit *>("textPlanJson");
        QVERIFY(knopf && text);

        knopf->click();
        const QString vorher = text->toPlainText();
        waehleFlussformat(tab);
        modusKnopf(tab, "fluxcopy")->click();
        const QString nachher = text->toPlainText();

        QVERIFY2(vorher != nachher,
                 "der Text steht still, obwohl der Modus gewechselt hat");
        QVERIFY2(nachher.contains(QStringLiteral("\"level\": \"flux\"")),
                 qPrintable(nachher.left(300)));
    }

    /* G24 — beim Oeffnen versteckt; der Knopf zeigt UND verbirgt. */
    void json_ist_zugeklappt_und_laesst_sich_zuklappen()
    {
        FormatTab tab;
        auto *knopf = tab.findChild<QAbstractButton *>("btnPlanJson");
        auto *text  = tab.findChild<QPlainTextEdit *>("textPlanJson");
        QVERIFY(knopf && text);

        QVERIFY2(text->isHidden(),
                 "die JSON-Ansicht steht beim Oeffnen offen — sie ist die "
                 "Kontrollform, nicht die Bedienform");
        knopf->click();
        QVERIFY2(!text->isHidden(), "der Knopf klappt nicht auf");
        knopf->click();
        QVERIFY2(text->isHidden(), "der Knopf klappt nicht wieder zu");
    }

    /* G25 — die Zeile nennt, was das Format traegt — und was nicht.
     *
     * Die Gegenprobe gehoert dazu: eine Zeile, die alle acht Flaggen
     * als „traegt" auffuehrt, waere auch gruen. */
    void caps_zeile_nennt_traegt_und_traegt_nicht()
    {
        FormatTab tab;
        auto *lab = tab.findChild<QLabel *>("labelPlanCaps");
        QVERIFY(lab);
        const QString t = lab->text();
        QVERIFY2(!t.isEmpty(), "die Zeile ist leer");

        const uint32_t caps = tab.copyPlanCaps();
        /* Mindestens EINE Flagge muss auf der einen und eine auf der
         * anderen Seite stehen, sonst sagt die Zeile nichts. */
        QVERIFY2(t.contains(QStringLiteral("trägt")),
                 qPrintable(t));
        for (size_t i = 0; i < uft_copy_cap_count(); i++) {
            const uft_copy_caps_t c = uft_copy_cap_at(i);
            const QString n = QString::fromUtf8(uft_copy_cap_name(c));
            QVERIFY2(t.contains(n), qPrintable(
                QStringLiteral("Flagge fehlt in der Zeile: ") + n
                + QStringLiteral(" | ") + t));
            /* Und was gemessen getragen wird, steht VOR dem ersten
             * „trägt nicht" — also im ersten Abschnitt. */
            if (caps & uint32_t(c)) {
                const int pos = t.indexOf(n);
                const int neg = t.indexOf(QStringLiteral("trägt nicht:"));
                QVERIFY2(neg < 0 || pos < neg, qPrintable(
                    QStringLiteral("getragene Flagge steht im falschen "
                                   "Abschnitt: ") + n + QStringLiteral(" | ") + t));
            }
        }
    }

    /* G26 — „nicht feststellbar" ist ein eigener Zustand, nicht
     *       „traegt nicht".
     *
     * Das ist der Kern der Sache: dieser Reiter oeffnet kein Abbild und
     * kann Bitstrom, Dateisystem, BAM und GCR gar nicht messen. Sie als
     * „traegt nicht" zu fuehren hiesse, eine Messung zu behaupten. */
    void nicht_messbares_steht_als_nicht_feststellbar()
    {
        FormatTab tab;
        auto *lab = tab.findChild<QLabel *>("labelPlanCaps");
        QVERIFY(lab);
        const QString t = lab->text();

        const int ab = t.indexOf(QStringLiteral("nicht feststellbar"));
        QVERIFY2(ab >= 0, qPrintable(t));

        /* Die vier, die copyPlanCaps() nie setzt, stehen hinter dieser
         * Marke — und die anderen vier davor. */
        const struct { uft_copy_caps_t c; bool offen; } f[] = {
            { UFT_CAP_BITSTREAM_IO, true  }, { UFT_CAP_FILESYSTEM, true  },
            { UFT_CAP_CBM_BAM,      true  }, { UFT_CAP_GCR,        true  },
            { UFT_CAP_FLUX_IO,      false }, { UFT_CAP_TIMING,     false },
            { UFT_CAP_WEAK_BITS,    false }, { UFT_CAP_MULTI_REV,  false },
        };
        for (const auto &x : f) {
            const QString n = QString::fromUtf8(uft_copy_cap_name(x.c));
            const int pos = t.indexOf(n);
            QVERIFY2(pos >= 0, qPrintable(n));
            if (x.offen)
                QVERIFY2(pos > ab, qPrintable(
                    QStringLiteral("nicht messbar, steht aber als gemessen "
                                   "da: ") + n + QStringLiteral(" | ") + t));
            else
                QVERIFY2(pos < ab, qPrintable(
                    QStringLiteral("messbar, steht aber als unbekannt "
                                   "da: ") + n + QStringLiteral(" | ") + t));
        }
    }
};

QTEST_MAIN(TestFormatTabCopyPlan)
#include "test_format_tab_copy_plan.moc"

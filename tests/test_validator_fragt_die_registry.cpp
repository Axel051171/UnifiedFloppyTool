/**
 * @file test_validator_fragt_die_registry.cpp
 * @brief Das Pruefetor vor dem Oeffnen kennt, was die Registry oeffnet
 *        (P2-2, MF-1351)
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * `MainWindow::openFile()`, der Explorer, der Forensik-Reiter und
 * `DecodeJob` fragen zuerst `DiskImageValidator::validate()` und brechen
 * ab, wenn es ablehnt. Das Pruefetor kannte eine feste Endungsliste
 * (`DISK_FORMATS[]` in `src/disk_image_validator.h`, 17 Endungen) und
 * fuenf Kennungen. Eine `.imd`, `.d88`, `.td0`, `.woz`, `.stx`, `.atr`,
 * `.2mg` ging damit nicht auf — „Unknown format: .imd" —, obwohl
 * `uft_disk_open()` sie ueber die Registry oeffnet. Die Liste war eine
 * gepflegte Aufzaehlung NEBEN der Registry (Klasse MF-636).
 *
 * ── Was hier festgehalten ist ────────────────────────────────────────────
 *
 *   1. Die Liste bleibt ZUERST fuer ihre eigenen Endungen. Eine 720K-`.st`
 *      meldet weiter den Listeneintrag „Atari ST Image" — ein Pruefetor,
 *      das die Registry zuerst fragt, meldete hier MSX, weil MSX-2DD im
 *      Erkennungsrennen dieselbe Groesse gewinnt
 *      (`tests/test_oeffentliche_api_am_korpus.c`, Zeile „st -> MSX").
 *   2. Wo Liste UND Kennungen ablehnen, fragt es die Registry; oeffnet ein
 *      Plugin die Datei, ist sie gueltig, der Name kommt aus dem Plugin,
 *      die Geometrie aus der geoeffneten Scheibe — KEINE Zahl aus der
 *      Dateigroesse dazugeraten.
 *   3. Oeffnet kein Plugin die Datei, bleibt sie ungueltig und die Meldung
 *      sagt es.
 *   4. Ist die Registry leer (nicht registriert), verhaelt sich alles wie
 *      die Liste allein — kein Absturz, keine erfundene Formatliste.
 *   5. Der Dateifilter bietet die Endungen der Registry an und behaelt die
 *      der Liste.
 *
 * Die Korpusdateien stammen aus `tests/corpus_free/` (fremde Haende:
 * hxcfe, fluxfox, to_woz2, atrcopy, floptool — siehe dort). Fehlt das
 * Korpus, UEBERSPRINGT sich der Test benannt statt still gruen zu sein.
 */
#include <QtTest/QtTest>
#include <QFile>
#include <QRandomGenerator>
#include <QTemporaryDir>

#include "disk_image_validator.h"

#include <uft/uft_core.h>
#include <uft/uft_format_plugin.h>
#include <uft/uft_types.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR muss gesetzt sein (tests/CMakeLists.txt)"
#endif

class TestValidatorFragtDieRegistry : public QObject
{
    Q_OBJECT

private:
    static QString korpus(const char *name)
    {
        return QString::fromUtf8(UFT_CORPUS_DIR) + QLatin1Char('/')
             + QString::fromUtf8(name);
    }

    static void registriert()
    {
        if (uft_registered_format_plugin_count() == 0)
            QCOMPARE(uft_register_all_formats(), UFT_OK);
        QVERIFY(uft_registered_format_plugin_count() > 0);
    }

    /** Die Geometrie so, wie das Plugin sie meldet; 0 heisst „nicht
     *  genannt" und wird im DiskImageInfo zu -1. */
    static int gemeldet(unsigned v) { return v > 0 ? int(v) : -1; }

private slots:

    /* ── 4. Leere Registry: die Liste allein, kein Absturz ───────────────
     *
     * MUSS als erster Slot laufen: danach ist die Registry gefuellt, und
     * `uft_register_all_formats()` laesst sich nicht rueckgaengig machen.
     * QtTest ruft die Slots in Deklarationsreihenfolge. */
    void leereRegistryHeisstListeAllein()
    {
        QCOMPARE(uft_registered_format_plugin_count(), size_t(0));

        const QString imd = korpus("hxcfe_pc160.imd");
        if (!QFile::exists(imd))
            QSKIP("Korpus fehlt: hxcfe_pc160.imd");

        const DiskImageInfo info = DiskImageValidator::validate(imd);
        QVERIFY(!info.isValid);
        QVERIFY2(info.errorMessage.contains(QLatin1String("Registry")),
                 qPrintable(QString("Die Absage nennt nicht, dass die "
                                    "Registry leer ist: %1")
                                .arg(info.errorMessage)));

        /* Der Listeneintrag geht auch ohne Registry. */
        const DiskImageInfo st =
            DiskImageValidator::validate(korpus("hxcfe_720k.st"));
        QVERIFY(st.isValid);
        QCOMPARE(st.formatName, QStringLiteral("Atari ST Image"));

        const QString filter = DiskImageValidator::fileDialogFilter();
        QVERIFY(filter.contains(QLatin1String("*.adf")));
        QVERIFY2(!filter.contains(QLatin1String("*.imd")),
                 "Ohne Registry darf der Filter keine Registry-Endung "
                 "behaupten");
        QVERIFY(!DiskImageValidator::isSupportedExtension("imd"));
        QVERIFY(DiskImageValidator::isSupportedExtension("adf"));
    }

    /* ── 2. Wo die Liste ablehnt, fragt es die Registry ─────────────────── */
    void registryOeffnetWasDieListeNichtKennt_data()
    {
        QTest::addColumn<QString>("datei");
        QTest::addColumn<QString>("plugin");
        QTest::addColumn<int>("zyl");
        QTest::addColumn<int>("koepfe");
        QTest::addColumn<int>("sektoren");
        QTest::addColumn<int>("groesse");
        QTest::addColumn<bool>("fluss");
        QTest::addColumn<bool>("fest");

        /* Plugin-Name und Fluss-Flagge GEMESSEN am 2026-09-26 ueber
         * `uft_disk_open_ranked()` (Protokoll von `korpusDurchlauf`).
         *
         * Feste Geometriezahlen (`fest` = true) nur dort, wo die Zahl,
         * die das Plugin meldet, zur Diskette passt — gemessen und von
         * einer zweiten Stelle getragen: die 360K-TD0 hat eine
         * gleichnamige 368 640-Byte-IMG daneben (40x2x9x512), die WOZ
         * und die 140K-2IMG sind DOS-3.3-Abbilder (35x1x16x256), die
         * 1600-Block-2MG ist die 800K-Apple-Diskette mit Zonen 12..8
         * (MF-1085), die ATR ist SD (40x1x18x128), die G71 eine
         * 1571-GCR mit 42 Spuren und hoechstens 21 Sektoren.
         *
         * Bei `hxcfe_pc160.imd/.d88/.stx` meldet das Plugin Zahlen, die
         * einander widersprechen (IMD 42/1/8/128, D88 80/2/8/512, STX
         * 42/2/9/512 — alle aus derselben 160K-Diskette); welche davon
         * stimmt, ist hier NICHT gemessen. Diese Zeilen pruefen deshalb
         * nur, dass das Pruefetor dasselbe sagt wie die geoeffnete
         * Scheibe — keine festgeschriebene Zahl, die einen Defekt
         * bewachen koennte (MF-1016). */
        QTest::newRow("imd") << "hxcfe_pc160.imd" << "IMD"
                             << 0 << 0 << 0 << 0 << false << false;
        QTest::newRow("d88") << "hxcfe_pc160.d88" << "D88"
                             << 0 << 0 << 0 << 0 << false << false;
        QTest::newRow("stx") << "hxcfe_pc160.stx" << "STX"
                             << 0 << 0 << 0 << 0 << false << false;
        QTest::newRow("td0") << "fluxfox_sector_test_360k.td0" << "TD0"
                             << 40 << 2 << 9 << 512 << false << true;
        QTest::newRow("woz") << "to_woz2_uftk_dos33.woz" << "WOZ"
                             << 35 << 1 << 16 << 256 << false << true;
        QTest::newRow("2img") << "2img_spec_140k.2img" << "2IMG"
                              << 35 << 1 << 16 << 256 << false << true;
        QTest::newRow("2mg") << "floptool_2img_1600.2mg" << "2IMG"
                             << 80 << 2 << 12 << 512 << false << true;
        QTest::newRow("atr") << "atrcopy_dos2sd.atr" << "ATR"
                             << 40 << 1 << 18 << 128 << false << true;
        /* Fluss ausserhalb der Liste: `.g71` steht nicht in DISK_FORMATS,
         * und das G71-Plugin traegt UFT_FORMAT_CAP_FLUX. */
        QTest::newRow("g71") << "vice_c1541_1571.g71" << "G71"
                             << 42 << 2 << 21 << 256 << true << true;
        /* 86F meldet Sektoren und Sektorgroesse mit 0 („nicht genannt")
         * — das Pruefetor muss -1 sagen, nicht 9/512 dazuraten. */
        QTest::newRow("86f") << "fluxfox_sector_test_360k.86f" << "86F"
                             << 0 << 0 << -1 << -1 << false << false;
    }

    void registryOeffnetWasDieListeNichtKennt()
    {
        registriert();

        QFETCH(QString, datei);
        QFETCH(QString, plugin);
        QFETCH(int, zyl);
        QFETCH(int, koepfe);
        QFETCH(int, sektoren);
        QFETCH(int, groesse);
        QFETCH(bool, fluss);
        QFETCH(bool, fest);

        const QString pfad = korpus(datei.toUtf8().constData());
        if (!QFile::exists(pfad))
            QSKIP(qPrintable(QString("Korpus fehlt: %1").arg(datei)));

        const DiskImageInfo info = DiskImageValidator::validate(pfad);
        QVERIFY2(info.isValid,
                 qPrintable(QString("%1 abgewiesen: %2")
                                .arg(datei, info.errorMessage)));
        QVERIFY2(info.formatName.contains(QLatin1Char('(') + plugin
                                          + QLatin1Char(')')),
                 qPrintable(QString("formatName nennt das Plugin %1 nicht: "
                                    "\"%2\"").arg(plugin, info.formatName)));
        QVERIFY2(info.platform.isEmpty(),
                 "Die Registry nennt keine Plattform — keine erfinden");
        QVERIFY(info.errorMessage.isEmpty());
        if (fest) {
            QCOMPARE(info.tracks, zyl);
            QCOMPARE(info.heads, koepfe);
        }
        if (fest || sektoren == -1) {
            QCOMPARE(info.sectorsPerTrack, sektoren);
            QCOMPARE(info.sectorSize, groesse);
        }
        QCOMPARE(info.isFluxFormat, fluss);

        /* Dieselbe Frage direkt an die Registry — das Pruefetor darf
         * nichts anderes sagen als die Scheibe, die es geoeffnet hat. */
        uft_probe_ranking_t rang;
        uft_disk_t *disk = uft_disk_open_ranked(pfad.toUtf8().constData(),
                                                true, &rang);
        QVERIFY(disk != nullptr);
        const uft_format_plugin_t *p = uft_disk_plugin(disk);
        uft_geometry_t g;
        memset(&g, 0, sizeof g);
        uft_disk_get_geometry(disk, &g);
        uft_disk_close(disk);
        QVERIFY(p != nullptr);
        QCOMPARE(QString::fromUtf8(p->name), plugin);
        QCOMPARE(info.tracks, gemeldet(g.cylinders));
        QCOMPARE(info.heads, gemeldet(g.heads));
        QCOMPARE(info.sectorsPerTrack, gemeldet(g.sectors));
        QCOMPARE(info.sectorSize, gemeldet(g.sector_size));
        QCOMPARE(info.isFluxFormat,
                 (p->capabilities & UFT_FORMAT_CAP_FLUX) != 0);
    }

    /* ── 1. Die Liste bleibt zuerst fuer ihre eigenen Endungen ─────────── */
    void listeBleibtZuerst()
    {
        registriert();
        const QString st = korpus("hxcfe_720k.st");
        if (!QFile::exists(st))
            QSKIP("Korpus fehlt: hxcfe_720k.st");

        const DiskImageInfo info = DiskImageValidator::validate(st);
        QVERIFY(info.isValid);
        QCOMPARE(info.formatName, QStringLiteral("Atari ST Image"));
        QCOMPARE(info.platform, QStringLiteral("Atari"));
        QCOMPARE(info.tracks, 80);
        QCOMPARE(info.heads, 2);
        QCOMPARE(info.sectorsPerTrack, 9);
        QCOMPARE(info.sectorSize, 512);
        QVERIFY(!info.isFluxFormat);
    }

    /* ── 3. Kein Plugin oeffnet sie: ungueltig, und die Meldung sagt es ── */
    void unbekanntesBleibtUnbekannt()
    {
        registriert();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        /* 4099 Byte, Samen fest: keine Kennung, keine Tafelgroesse. */
        const QString pfad = dir.filePath("keinabbild.xyz");
        {
            QFile f(pfad);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QRandomGenerator gen(0x50322D32u);   /* "P2-2" */
            QByteArray b(4099, '\0');
            for (int i = 0; i < b.size(); ++i)
                b[i] = char(gen.generate() & 0xFF);
            QCOMPARE(f.write(b), qint64(b.size()));
        }

        const DiskImageInfo info = DiskImageValidator::validate(pfad);
        QVERIFY(!info.isValid);
        QVERIFY(info.formatName.isEmpty());
        QCOMPARE(info.tracks, -1);
        QVERIFY2(info.errorMessage.contains(QLatin1String("kein Plugin")),
                 qPrintable(QString("Die Absage sagt nicht, dass kein "
                                    "Plugin die Datei oeffnet: %1")
                                .arg(info.errorMessage)));
    }

    /* ── Gleichstand: „mehrdeutig" mit Namen, keines gewaehlt ──────────────
     *
     * `hxcfe_pc160.dsk` (194 816 Byte) passt zu keinem `.dsk`-Eintrag der
     * Liste (143 360), und in der Registry liegen gemessen zwei Plugins
     * gleichauf, ohne dass die Endung entscheidet (MF-1251). */
    void gleichstandHeisstMehrdeutig()
    {
        registriert();
        const QString pfad = korpus("hxcfe_pc160.dsk");
        if (!QFile::exists(pfad))
            QSKIP("Korpus fehlt: hxcfe_pc160.dsk");

        uft_probe_ranking_t rang;
        memset(&rang, 0, sizeof rang);
        uft_disk_t *disk = uft_disk_open_ranked(pfad.toUtf8().constData(),
                                                true, &rang);
        QVERIFY2(disk == nullptr, "Vorbedingung: die Registry oeffnet sie "
                                  "NICHT (Gleichstand)");
        QVERIFY(rang.tied > 1);

        const DiskImageInfo info = DiskImageValidator::validate(pfad);
        QVERIFY(!info.isValid);
        QVERIFY2(info.errorMessage.contains(QLatin1String("mehrdeutig")),
                 qPrintable(info.errorMessage));
        for (size_t i = 0; i < rang.tied_listed; ++i) {
            const QString n = QString::fromUtf8(rang.tied_with[i]->name);
            QVERIFY2(info.errorMessage.contains(n),
                     qPrintable(QString("Die Absage nennt %1 nicht: %2")
                                    .arg(n, info.errorMessage)));
        }
    }

    /* ── Die Endung hat einen Gleichstand entschieden: sichtbar lassen ────
     *
     * `atrcopy_dos2sd.xfd`: gemessen 5 gleichauf, die Endung `.xfd`
     * waehlt XFD (Regel 2b, MF-1252). Das ist eine Verengung, kein
     * Beleg — der Name sagt es. */
    void endungsEntscheidBleibtSichtbar()
    {
        registriert();
        const QString pfad = korpus("atrcopy_dos2sd.xfd");
        if (!QFile::exists(pfad))
            QSKIP("Korpus fehlt: atrcopy_dos2sd.xfd");

        const DiskImageInfo info = DiskImageValidator::validate(pfad);
        QVERIFY(info.isValid);
        QVERIFY2(info.formatName.contains(QLatin1String("(XFD)")),
                 qPrintable(info.formatName));
        QVERIFY2(info.formatName.contains(QLatin1String("Endung .xfd")),
                 qPrintable(info.formatName));
    }

    /* ── Nur die Groesse: gesagt, nicht verschwiegen ──────────────────────
     *
     * `fluxfox_sector_test_360k.img` ist eine PC-360K-Diskette (368 640
     * Byte, dieselbe Diskette wie die `.td0`/`.86f` daneben). Gemessen
     * oeffnet die Registry sie als MSX — allein im Groessenband (MF-729:
     * 30..49, Konfidenz 45), genau der Fall „PC-360K an MSX" aus MF-729.
     * Das Pruefetor folgt der EINEN Entscheidungsstelle (MF-1251), sagt
     * aber, worauf sie beruht. */
    void nurGroesseWirdGesagt()
    {
        registriert();
        const QString pfad = korpus("fluxfox_sector_test_360k.img");
        if (!QFile::exists(pfad))
            QSKIP("Korpus fehlt: fluxfox_sector_test_360k.img");

        uft_probe_ranking_t rang;
        memset(&rang, 0, sizeof rang);
        uft_disk_t *disk = uft_disk_open_ranked(pfad.toUtf8().constData(),
                                                true, &rang);
        QVERIFY(disk != nullptr);
        uft_disk_close(disk);
        QCOMPARE(int(rang.band), int(UFT_PROBE_BAND_SIZE));

        const DiskImageInfo info = DiskImageValidator::validate(pfad);
        QVERIFY(info.isValid);
        QVERIFY2(info.formatName.contains(
                     QLatin1String("nur an der Dateigroesse")),
                 qPrintable(info.formatName));

        /* Und umgekehrt: ein Merkmalstreffer bekommt den Zusatz nicht. */
        const DiskImageInfo woz =
            DiskImageValidator::validate(korpus("to_woz2_uftk_dos33.woz"));
        QVERIFY(woz.isValid);
        QVERIFY2(!woz.formatName.contains(QLatin1String("Dateigroesse")),
                 qPrintable(woz.formatName));
    }

    /* ── Erkannt, aber nicht geoeffnet: kein Freispruch ─────────────────── */
    void erkanntAberNichtGeoeffnet()
    {
        registriert();
        const QString pfad = korpus("cas_msx_nurkopf.cas");
        if (!QFile::exists(pfad))
            QSKIP("Korpus fehlt: cas_msx_nurkopf.cas");

        const DiskImageInfo info = DiskImageValidator::validate(pfad);
        QVERIFY(!info.isValid);
        QVERIFY2(info.errorMessage.contains(QLatin1String("kein Plugin")),
                 qPrintable(info.errorMessage));
        QCOMPARE(info.tracks, -1);
    }

    /* ── 5. Der Dateifilter: Registry dazu, Liste behalten ──────────────── */
    void dateifilterBietetDieRegistryAn()
    {
        registriert();
        const QString filter = DiskImageValidator::fileDialogFilter();
        QVERIFY2(filter.contains(QLatin1String("*.imd")), qPrintable(filter));
        QVERIFY2(filter.contains(QLatin1String("*.d88")), qPrintable(filter));
        QVERIFY2(filter.contains(QLatin1String("*.adf")), qPrintable(filter));
        QVERIFY2(filter.contains(QLatin1String("*.st")), qPrintable(filter));
        QVERIFY(filter.endsWith(QLatin1String("All Files (*)")));

        /* Keine Endung doppelt in der Gesamtgruppe. */
        const QString alle = filter.section(QLatin1String(";;"), 0, 0);
        const int auf = alle.indexOf(QLatin1Char('('));
        const int zu = alle.lastIndexOf(QLatin1Char(')'));
        QVERIFY(auf > 0 && zu > auf);
        const QStringList exts =
            alle.mid(auf + 1, zu - auf - 1).split(QLatin1Char(' '),
                                                  Qt::SkipEmptyParts);
        QCOMPARE(QSet<QString>(exts.begin(), exts.end()).size(),
                 exts.size());

        QVERIFY(DiskImageValidator::isSupportedExtension("imd"));
        QVERIFY(DiskImageValidator::isSupportedExtension(".D88"));
        QVERIFY(DiskImageValidator::isSupportedExtension("adf"));
        QVERIFY(!DiskImageValidator::isSupportedExtension("xyz"));
        QVERIFY(DiskImageValidator::supportedExtensions()
                    .contains(QLatin1String(".imd")));
        QVERIFY(DiskImageValidator::supportedExtensions()
                    .contains(QLatin1String(".adf")));
    }

    /* ── Der Pfad, nicht die Datei (Nachbesserung P2-2, MF-1351) ─────────
     *
     * Die Format-Schicht oeffnet mit einem schmalen fopen(), also in der
     * Form von QFile::encodeName() (Windows: ANSI-Codepage). Gemessen in
     * der Gegenpruefung: mit toUtf8() statt encodeName ging „Übung_ä.imd"
     * nicht mehr auf, und KEIN Test wurde rot; und „Ωα.imd" meldete „kein
     * Plugin oeffnet die Datei" — eine falsche Aussage ueber eine Datei,
     * die ein Plugin gemessen oeffnet. Beide Zeilen sind derselbe Inhalt
     * (hxcfe_pc160.imd) unter einem anderen Namen.
     *
     * Ob ein Name in der Codepage darstellbar ist, haengt am System (die
     * CI-Linux-Laeufe haben UTF-8, dort geht auch „Ωα" auf); die Zusage
     * gilt fuer beide Ausgaenge: darstellbar -> gueltig, sonst eine
     * Absage, die den PFAD nennt und nicht „kein Plugin" sagt. */
    void pfadInDerSystemcodepage_data()
    {
        QTest::addColumn<QString>("name");
        QTest::newRow("umlaut")
            << QString::fromUtf8("\xC3\x9C" "bung_\xC3\xA4.imd");
        QTest::newRow("griechisch")
            << QString::fromUtf8("\xCE\xA9\xCE\xB1.imd");
    }

    void pfadInDerSystemcodepage()
    {
        registriert();
        QFETCH(QString, name);
        const QString quelle = korpus("hxcfe_pc160.imd");
        if (!QFile::exists(quelle))
            QSKIP("Korpus fehlt: hxcfe_pc160.imd");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString pfad = dir.filePath(name);
        QVERIFY(QFile::copy(quelle, pfad));

        const bool darstellbar =
            QFile::decodeName(QFile::encodeName(pfad)) == pfad;
        qInfo().noquote() << QString("PFAD %1 darstellbar=%2")
                                 .arg(name).arg(darstellbar);

        const DiskImageInfo info = DiskImageValidator::validate(pfad);
        if (darstellbar) {
            QVERIFY2(info.isValid, qPrintable(info.errorMessage));
            QVERIFY2(info.formatName.contains(QLatin1String("(IMD)")),
                     qPrintable(info.formatName));
        } else {
            QVERIFY(!info.isValid);
            QVERIFY2(info.errorMessage.contains(QLatin1String("Systemcodepage")),
                     qPrintable(info.errorMessage));
            QVERIFY2(!info.errorMessage.contains(QLatin1String("kein Plugin")),
                     qPrintable(QString("Urteil ueber die Datei, obwohl es am "
                                        "Pfad liegt: %1")
                                    .arg(info.errorMessage)));
        }
    }

    /* ── Eine andere Datei unter dem Ersatznamen ──────────────────────────
     *
     * Gemessen im Rotbeweis dieser Nachbesserung: unter Windows (ANSI-
     * Codepage 1252) wird „Ωα" beim Kodieren nicht zu '?', sondern per
     * Ersatzabbildung zu „Oa". Die C-Schicht oeffnet dann „Oa.imd" — eine
     * ANDERE Datei, wenn es sie gibt. Hier liegt unter dem Ersatznamen
     * ein gueltiges IMD und unter dem eigentlichen Namen 4099 Byte ohne
     * jede Kennung: das Pruefetor darf die fremde Datei nicht als diese
     * ausgeben. Ist der Name darstellbar (UTF-8-Systeme), gibt es keinen
     * Ersatznamen — dann uebersprungen, benannt. */
    void fremdeDateiUnterDemErsatznamen()
    {
        registriert();
        const QString quelle = korpus("hxcfe_pc160.imd");
        if (!QFile::exists(quelle))
            QSKIP("Korpus fehlt: hxcfe_pc160.imd");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString pfad = dir.filePath(QString::fromUtf8("\xCE\xA9\xCE\xB1.imd"));
        const QString ersatz = QFile::decodeName(QFile::encodeName(pfad));
        if (ersatz == pfad)
            QSKIP("Name in dieser Codepage darstellbar — kein Ersatzname");
        if (!QFile::copy(quelle, ersatz))
            QSKIP(qPrintable("Ersatzname nicht anlegbar: " + ersatz));
        {
            QFile f(pfad);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QRandomGenerator gen(0x4F61u);   /* "Oa" */
            QByteArray b(4099, '\0');
            for (int i = 0; i < b.size(); ++i)
                b[i] = char(gen.generate() & 0xFF);
            QCOMPARE(f.write(b), qint64(b.size()));
        }
        qInfo().noquote() << QString("ERSATZ %1 -> %2").arg(pfad, ersatz);

        const DiskImageInfo info = DiskImageValidator::validate(pfad);
        QVERIFY2(!info.isValid,
                 qPrintable(QString("Das Pruefetor hat %1 geoeffnet und als "
                                    "%2 ausgegeben: %3")
                                .arg(ersatz, pfad, info.formatName)));
    }

    /* ── Eine leere Datei heisst leer ─────────────────────────────────────
     *
     * Gemessen in der Gegenpruefung: 0 Byte .imd/.st/.xyz meldeten „kein
     * Plugin oeffnet die Datei" — zutreffend, aber die Ursache ist eine
     * andere, und sie ist bekannt. */
    void leereDateiHeisstLeer()
    {
        registriert();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString pfad = dir.filePath("leer.imd");
        {
            QFile f(pfad);
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
        const DiskImageInfo info = DiskImageValidator::validate(pfad);
        QVERIFY(!info.isValid);
        QVERIFY2(info.errorMessage.contains(QLatin1String("Datei leer")),
                 qPrintable(info.errorMessage));
    }

    /* ── Messprotokoll: der ganze Korpus durch das Pruefetor ──────────────
     *
     * Keine Zusage — eine Messung. Sie druckt je Datei, was das Pruefetor
     * und was die Registry sagt, damit der Unterschied vor und nach P2-2
     * am selben Korpus nachzaehlbar ist. */
    void korpusDurchlauf()
    {
        registriert();
        QDir d(QString::fromUtf8(UFT_CORPUS_DIR));
        const QStringList dateien = d.entryList(QDir::Files, QDir::Name);
        if (dateien.isEmpty())
            QSKIP("Korpus fehlt");

        int gueltig = 0;
        for (const QString &n : dateien) {
            const QString pfad = d.filePath(n);
            const DiskImageInfo info = DiskImageValidator::validate(pfad);
            if (info.isValid) ++gueltig;

            uft_probe_ranking_t rang;
            memset(&rang, 0, sizeof rang);
            uft_disk_t *disk = uft_disk_open_ranked(
                pfad.toUtf8().constData(), true, &rang);
            QString reg = QStringLiteral("-");
            if (disk) {
                const uft_format_plugin_t *p = uft_disk_plugin(disk);
                uft_geometry_t g;
                memset(&g, 0, sizeof g);
                uft_disk_get_geometry(disk, &g);
                reg = QString("%1 %2/%3/%4/%5 flux=%6 tied=%7")
                          .arg(p && p->name ? p->name : "?")
                          .arg(g.cylinders).arg(g.heads)
                          .arg(g.sectors).arg(g.sector_size)
                          .arg(p && (p->capabilities & UFT_FORMAT_CAP_FLUX)
                                   ? 1 : 0)
                          .arg(rang.tied);
                uft_disk_close(disk);
            } else if (rang.tied > 1) {
                reg = QString("mehrdeutig tied=%1").arg(rang.tied);
            }
            qInfo().noquote()
                << QString("KORPUS %1 | %2 | %3 | %4/%5/%6/%7 | %8 | REG %9")
                       .arg(n, info.isValid ? "gueltig" : "ABGEWIESEN",
                            info.isValid ? info.formatName
                                         : info.errorMessage)
                       .arg(info.tracks).arg(info.heads)
                       .arg(info.sectorsPerTrack).arg(info.sectorSize)
                       .arg(info.platform, reg);
        }
        qInfo().noquote() << QString("KORPUS SUMME %1 von %2 gueltig")
                                 .arg(gueltig).arg(dateien.size());
    }
};

QTEST_MAIN(TestValidatorFragtDieRegistry)
#include "test_validator_fragt_die_registry.moc"

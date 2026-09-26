/**
 * @file test_decode_job_no_fiction.cpp
 * @brief Der Oberflaechen-Scan meldet nur, was gelesen wurde (P0-17,
 *        Tuer Stufe 2)
 *
 * ── Was hier bewacht wird ────────────────────────────────────────────────
 *
 * `DecodeJob` (`src/decodejob.cpp`) ist der Hintergrundauftrag hinter dem
 * Status-Reiter und dem Workflow-Reiter. Gemessen am Vorzustand (Stand
 * `0143483b`) erfand er sein Ergebnis auf drei Wegen:
 *
 *  a) **Fluss.** Fuer die Endungen scp/raw/g64/nib/hfe/ipf setzte er
 *     `goodSectors = totalSectors, badSectors = 0` („For flux, mark all as
 *     OK (we can't verify individual sectors)") — und `totalSectors` war
 *     eine Vorgabe (80 x 2 x 9), weil der Validator fuer Flussformate keine
 *     Geometrie kennt. An `hxcfe_kfx_t00.0.raw`, einem EINZIGEN
 *     KryoFlux-Spurstrom, meldete er „1440/1440 sectors OK". Das Plugin
 *     liefert daraus gemessen 0 Sektoren.
 *
 *  b) **Flacher Versatz.** Ein Sektor galt als „OK", sobald an
 *     `((track*heads+head)*spt+sector)*size` genug Bytes standen — ohne
 *     Plugin. An einer MSA (10 Byte Kopf, je Spur ein Laengenwort, RLE)
 *     ist das in BEIDE Richtungen falsch: `hxcfe_msa.msa` bekam 1440 „OK"
 *     fuer verschobene Bytes, `hxcfe_msa_rle.msa` 30 „OK" und 1410
 *     „READ_ERROR" fuer eine einwandfreie Diskette. Die Datei traegt
 *     84 x 2 x 9 = 1512 Sektoren, nicht 1440.
 *
 *  c) **Kein Kopf.** `sectorUpdate(int track, int sector, ...)` — Seite 0
 *     und Seite 1 landeten in derselben Zelle.
 *
 * Seither geht der Auftrag den Weg des Analysators: `uft_disk_open()` ->
 * `uft_d2_from_disk()` -> Status je Sektor aus dem Modell.
 *
 * ── Warum der Test gegen BEIDE Staende uebersetzt ────────────────────────
 *
 * Das Signal wird ueber das Metaobjekt gesucht, nicht ueber seinen Typ:
 * gegen den Vorzustand hat es drei Argumente, und der Test wird ROT
 * („traegt keinen Kopf"), statt nicht zu bauen. Was es nur im neuen Stand
 * gibt, haengt an `UFT_DECODEJOB_MODEL_STATUS`.
 *
 * ── Woher die Erwartungen kommen ────────────────────────────────────────
 *
 * Nicht aus dem Modell, das der Pruefling selbst benutzt (Klasse MF-1000):
 * die MSA-Sektorzahl rechnet der Test aus dem MSA-Kopf, die ADF-Zahl aus
 * der Dateigroesse, und die Flusszahlen stehen fest, gemessen am
 * 2026-09-26 — ein Plugin, das kuenftig Sektoren liefert, macht den Test
 * rot, und dann gehoert er berichtigt, nicht die Zahl still angepasst.
 */
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QMetaMethod>
#include <QFile>
#include <QSet>
#include <QTextEdit>
#include <QLabel>
#include <QProgressBar>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <cstring>
#include <memory>

#include "decodejob.h"
#include "statustab.h"

extern "C" {
#include "uft/uft_format_plugin.h"
}

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR fehlt — ohne Korpus liefe dieser Test leer (MF-598)"
#endif

namespace {

struct Emission {
    int track = -1;
    int head = -1;     ///< -1: das Signal traegt keinen Kopf
    int sector = -1;
    QString status;
};

struct JobRun {
    QList<Emission> em;
    bool headKnown = false;
    bool signalFound = false;
    QString finished;
    QString error;
    DecodeResult result;
};

QString corpus(const QString &name)
{
    return QString(UFT_CORPUS_DIR) + "/" + name;
}

/** Laeuft den Auftrag synchron im Testfaden und sammelt jedes
 *  `sectorUpdate`, gleich welcher Signatur. */
JobRun runJob(const QString &path, StatusTab *tab = nullptr)
{
    JobRun r;
    DecodeJob job;
    job.setSourcePath(path);
    if (tab) tab->connectToDecodeJob(&job);

    QMetaMethod sig;
    for (int i = 0; i < job.metaObject()->methodCount(); ++i) {
        const QMetaMethod m = job.metaObject()->method(i);
        if (m.methodType() == QMetaMethod::Signal && m.name() == "sectorUpdate")
            sig = m;
    }
    r.signalFound = sig.isValid();
    if (!r.signalFound) return r;
    r.headKnown = sig.parameterCount() == 4;

    const QByteArray s = "2" + sig.methodSignature();
    QSignalSpy spy(&job, s.constData());
    QSignalSpy fin(&job, SIGNAL(finished(QString)));
    QSignalSpy err(&job, SIGNAL(error(QString)));
    job.run();

    for (const QList<QVariant> &a : spy) {
        Emission e;
        e.track = a.at(0).toInt();
        if (r.headKnown) {
            e.head = a.at(1).toInt();
            e.sector = a.at(2).toInt();
            e.status = a.at(3).toString();
        } else {
            e.sector = a.at(1).toInt();
            e.status = a.at(2).toString();
        }
        r.em.append(e);
    }
    if (!fin.isEmpty()) r.finished = fin.first().first().toString();
    if (!err.isEmpty()) r.error = err.first().first().toString();
    r.result = job.result();
    if (tab) tab->disconnectFromDecodeJob();
    return r;
}

QMap<QString, int> tally(const QList<Emission> &em)
{
    QMap<QString, int> t;
    for (const Emission &e : em) t[e.status]++;
    return t;
}

QString show(const QMap<QString, int> &t)
{
    QStringList parts;
    for (auto it = t.cbegin(); it != t.cend(); ++it)
        parts << QString("%1=%2").arg(it.key()).arg(it.value());
    return parts.join(' ');
}

quint16 be16(const QByteArray &b, int off)
{
    return quint16((quint8(b.at(off)) << 8) | quint8(b.at(off + 1)));
}

/** Leer, wenn `text` keine Gut-Aussage traegt, die das Modell nicht deckt.
 *  Das Modell deckt hier NULL gute Sektoren (keine Datei des Korpus traegt
 *  eine stimmende Pruefsumme, die den Auftrag erreicht) — also ist jede
 *  positive Zahl vor „Pruefsumme stimmt" / „OK" / „gut" eine Erfindung, und
 *  „sectors OK" ist die Wendung des Vorzustands. */
QString unbackedOkClaim(const QString &text)
{
    if (text.contains("sectors OK", Qt::CaseInsensitive))
        return "\"sectors OK\" im Text";
    if (text.contains("Sektoren OK", Qt::CaseInsensitive))
        return "\"Sektoren OK\" im Text";
    static const QRegularExpression positiv(
        "\\b([1-9][0-9]*)\\s*(?:/\\s*[0-9]+\\s*)?"
        "(?:Pruefsumme stimmt|sectors? (?:OK|good)|Sektoren gut|OK\\b)");
    const QRegularExpressionMatch m = positiv.match(text);
    if (m.hasMatch()) return "\"" + m.captured(0) + "\" im Text";
    return QString();
}

/** Die Seitenzeilen des Status-Reiters: „Seite N: X Sektoren — a K1, b K2,
 *  ...; Spuren ohne Sektoraussage T". Gibt je Seite X und die Summe der
 *  genannten Klassen zurueck — der Test verlangt, dass sie aufgeht. */
struct SeitenZeile {
    int sektoren = -1;
    int summe = 0;
    QString zeile;
};

QMap<int, SeitenZeile> seitenZeilen(const QString &text)
{
    QMap<int, SeitenZeile> out;
    static const QRegularExpression kopf(
        "Seite (\\d+): (\\d+) Sektoren \\x{2014} ([^;\\n]*);");
    static const QRegularExpression zahl("(\\d+)");
    auto it = kopf.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        SeitenZeile z;
        z.sektoren = m.captured(2).toInt();
        z.zeile = m.captured(0);
        auto zi = zahl.globalMatch(m.captured(3));
        while (zi.hasNext()) z.summe += zi.next().captured(1).toInt();
        out.insert(m.captured(1).toInt(), z);
    }
    return out;
}

}  // namespace

class TestDecodeJobNoFiction : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        /* Wie `main()` im Produkt (MF-447): ohne Registrierung oeffnet
         * `uft_disk_open()` nichts, und jede Zusage unten waere leer. */
        QCOMPARE(uft_register_all_formats(), UFT_OK);
    }

    /* ── a) Fluss, den das Plugin nicht in Sektoren zerlegt ─────────────
     *
     * Drei Dateien, drei verschiedene Gruende, warum es keine Sektoren
     * gibt — gemessen am 2026-09-26 ueber `uft_disk_open()` +
     * `uft_d2_from_disk()`:
     *   hxcfe_kfx_t00.0.raw   KFX, 1 x 1 Spur, liefert gar keine Schicht,
     *                         die die Bruecke uebernimmt
     *   gw_amigados.hfe       HFE, 80 x 2, liefert 160 Bitstroeme, keinen
     *                         Sektor
     *   gw_fm_acorn_3trk.scp  SCP, 3 x 2, 5 Spuren Fluss und EINE Spur,
     *                         deren read_track einen Fehlercode liefert
     * Vorher meldete der Auftrag fuer alle drei „1440/1440 sectors OK". */
    void fluxWithoutSectorsClaimsNothing_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<int>("tracks");
        QTest::newRow("kfx-strom") << "hxcfe_kfx_t00.0.raw" << 1;
        QTest::newRow("hfe-bitstrom") << "gw_amigados.hfe" << 160;
        QTest::newRow("scp-fluss") << "gw_fm_acorn_3trk.scp" << 6;
    }

    void fluxWithoutSectorsClaimsNothing()
    {
        QFETCH(QString, name);
        QFETCH(int, tracks);
        StatusTab tab;
        const JobRun r = runJob(corpus(name), &tab);
        QVERIFY2(r.signalFound, "DecodeJob hat kein Signal sectorUpdate.");
        QVERIFY2(r.error.isEmpty(), qPrintable("Fehler statt Ergebnis: " + r.error));
        QVERIFY2(!r.finished.isEmpty(), "Der Auftrag endete nicht.");

        const QMap<QString, int> t = tally(r.em);
        qInfo().noquote() << name << "| gesendet:" << show(t)
                          << "| gut" << r.result.goodSectors
                          << "schlecht" << r.result.badSectors
                          << "gesamt" << r.result.totalSectors
                          << "|" << r.finished;

        /* Kein Sektor wurde gelesen, also keiner ist gut und keiner ist
         * schlecht — die Kernaussage von P0-17. */
        QVERIFY2(r.result.goodSectors == 0,
                 qPrintable(QString("%1 Sektoren \"gut\" gemeldet, gelesen "
                                    "wurde keiner.").arg(r.result.goodSectors)));
        QCOMPARE(r.result.badSectors, 0);
        QCOMPARE(r.result.totalSectors, 0);

        /* Jede Meldung ist eine Spur OHNE Sektoraussage, nichts sonst. */
        int keine = 0;
        for (const Emission &e : r.em) {
            QVERIFY2(e.status == "NO_SECTORS",
                     qPrintable(QString("Spur %1 meldet \"%2\" — fuer eine "
                                        "Spur ohne gelesenen Sektor gibt es "
                                        "keinen Sektorstatus.")
                                    .arg(e.track).arg(e.status)));
            QCOMPARE(e.sector, -1);
            keine++;
        }
        QCOMPARE(keine, tracks);

        QVERIFY2(r.finished.contains("keine Sektoraussage"),
                 qPrintable("Das Ergebnis sagt nicht, dass nichts dekodiert "
                            "wurde: " + r.finished));
        QVERIFY2(!r.finished.contains("sectors OK"),
                 qPrintable("Das Ergebnis nennt Sektoren OK: " + r.finished));
#ifdef UFT_DECODEJOB_MODEL_STATUS
        QCOMPARE(r.result.tracksWithoutSectors, tracks);
        QCOMPARE(r.result.uncheckedSectors, 0);
#endif

        /* Nichts dekodiert heisst: kein „Decode complete", kein Haken —
         * weder im Ergebnis des Auftrags noch im Reiter noch in seinem
         * Protokoll. Der Lauf ist beendet, mehr nicht. */
        QVERIFY2(!r.finished.contains("Decode complete", Qt::CaseInsensitive),
                 qPrintable("Ergebnis behauptet einen Dekodiervorgang, der "
                            "nichts dekodiert hat: " + r.finished));
        QVERIFY2(r.finished.startsWith(QString::fromUtf8("Lauf beendet — keine Sektoraussage")),
                 qPrintable("Ergebnis beginnt nicht neutral: " + r.finished));
        auto *info = tab.findChild<QTextEdit *>("textSectorInfo");
        auto *logw = tab.findChild<QTextEdit *>("textLog");
        QVERIFY(info && logw);
        const QString reiter = info->toPlainText();
        const QString protokoll = logw->toPlainText();
        QVERIFY2(!reiter.contains(QChar(0x2713)) && !protokoll.contains(QChar(0x2713)),
                 qPrintable("Haken ohne dekodierten Sektor:\n" + reiter + "\n---\n" + protokoll));
        QVERIFY2(!reiter.contains("Decode Complete", Qt::CaseInsensitive),
                 qPrintable("Reiter sagt \"Decode Complete\":\n" + reiter));
        QVERIFY2(reiter.contains(QString::fromUtf8("Lauf beendet — keine Sektoraussage")),
                 qPrintable("Reiter sagt nicht, dass nichts dekodiert wurde:\n" + reiter));
        QVERIFY2(unbackedOkClaim(r.finished + reiter).isEmpty(),
                 qPrintable(unbackedOkClaim(r.finished + reiter)));
#ifdef UFT_DECODEJOB_STAGE2_V2
        /* Gemessen am 2026-09-26: keine Datei des Korpus erreicht die
         * Abweisung im Modell — die Zahl steht trotzdem im Ergebnis. */
        QCOMPARE(r.result.rejectedSectors, 0);
#endif
    }

    /* ── b) Ein Behaelter, dessen Anordnung nicht flach ist ─────────────
     *
     * MSA: 10 Byte Kopf, je Spur ein Laengenwort, wahlweise RLE. Die
     * Sektorzahl rechnet der Test aus dem Kopf der Datei selbst (Big
     * Endian: Sektoren je Spur, Seiten-1, erste und letzte Spur) — nicht
     * aus dem Plugin, das der Auftrag benutzt. MSA traegt keine
     * Pruefsumme; der Status jedes Sektors ist deshalb „UNCHECKED". */
    void containerStatusComesFromModel_data()
    {
        QTest::addColumn<QString>("name");
        QTest::newRow("msa-roh") << "hxcfe_msa.msa";
        QTest::newRow("msa-rle") << "hxcfe_msa_rle.msa";
    }

    void containerStatusComesFromModel()
    {
        QFETCH(QString, name);
        QFile f(corpus(name));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray kopf = f.read(10);
        QCOMPARE(kopf.size(), 10);
        QCOMPARE(int(be16(kopf, 0)), 0x0E0F);
        const int spt = be16(kopf, 2);
        const int seiten = be16(kopf, 4) + 1;
        const int spuren = be16(kopf, 8) - be16(kopf, 6) + 1;
        const int erwartet = spt * seiten * spuren;
        qInfo().noquote() << name << "| Kopf:" << spuren << "x" << seiten << "x"
                          << spt << "=" << erwartet;
        QCOMPARE(erwartet, 1512);   /* 84 x 2 x 9, am Kopf abgelesen */

        const JobRun r = runJob(corpus(name));
        QVERIFY2(r.signalFound, "DecodeJob hat kein Signal sectorUpdate.");
        QVERIFY2(r.error.isEmpty(), qPrintable("Fehler statt Ergebnis: " + r.error));

        const QMap<QString, int> t = tally(r.em);
        qInfo().noquote() << name << "| gesendet:" << show(t) << "|" << r.finished;

        int sektoren = 0;
        for (const Emission &e : r.em)
            if (e.sector >= 0) sektoren++;
        QVERIFY2(sektoren == erwartet,
                 qPrintable(QString("%1 Sektoren gemeldet, die Datei traegt %2")
                                .arg(sektoren).arg(erwartet)));
        QVERIFY2(t.value("UNCHECKED") == erwartet,
                 qPrintable("Status nicht aus dem Modell: " + show(t)));
        QVERIFY2(!t.contains("OK") && !t.contains("READ_ERROR"),
                 qPrintable("Status aus dem flachen Versatz: " + show(t)));
        QCOMPARE(r.result.goodSectors, 0);
        QCOMPARE(r.result.badSectors, 0);
        QCOMPARE(r.result.totalSectors, erwartet);
#ifdef UFT_DECODEJOB_MODEL_STATUS
        QCOMPARE(r.result.uncheckedSectors, erwartet);
        QCOMPARE(r.result.tracksWithoutSectors, 0);
#endif

        /* Der TEXT sagt dasselbe wie die Zahlen — und nichts, was das
         * Modell nicht deckt. Hier stand nur die Pruefung der Zahlen; ein
         * Ergebnis „1512 sectors OK" waere gruen durchgegangen. */
        const QString gelesen = QString("%1 Sektoren gelesen").arg(erwartet);
        const QString ohne = QString("%1 ohne Pruefsummenangabe").arg(erwartet);
        QVERIFY2(r.finished.contains(gelesen),
                 qPrintable("Ergebnis nennt nicht \"" + gelesen + "\": " + r.finished));
        QVERIFY2(r.finished.contains("0 Pruefsumme stimmt"),
                 qPrintable("Ergebnis nennt nicht \"0 Pruefsumme stimmt\": " + r.finished));
        QVERIFY2(r.finished.contains(ohne),
                 qPrintable("Ergebnis nennt nicht \"" + ohne + "\": " + r.finished));
        QVERIFY2(unbackedOkClaim(r.finished).isEmpty(),
                 qPrintable(unbackedOkClaim(r.finished) + ": " + r.finished));
#ifdef UFT_DECODEJOB_STAGE2_V2
        QCOMPARE(r.result.rejectedSectors, 0);
#endif
    }

    /* ── c) Seite 1 ueberschreibt Seite 0 nicht ───────────────────────── */
    void sidesStaySeparate()
    {
        const QString name = "xdftool_dd_ofs.adf";
        const qint64 groesse = QFileInfo(corpus(name)).size();
        QCOMPARE(groesse, qint64(901120));
        const int erwartet = int(groesse / 512);          /* 1760 */
        const int jeSeite = erwartet / 2;                  /* 880  */

        StatusTab tab;
        const JobRun r = runJob(corpus(name), &tab);
        QVERIFY2(r.signalFound, "DecodeJob hat kein Signal sectorUpdate.");
        QVERIFY2(r.error.isEmpty(), qPrintable("Fehler statt Ergebnis: " + r.error));
        QVERIFY2(r.headKnown,
                 "sectorUpdate traegt keinen Kopf — Seite 0 und Seite 1 "
                 "landen in derselben Zelle.");

        QSet<QString> zellen;
        QMap<int, int> jeKopf;
        QMap<int, QSet<int>> koepfeJeSpur;
        for (const Emission &e : r.em) {
            if (e.sector < 0) continue;
            zellen.insert(QString("%1/%2/%3").arg(e.track).arg(e.head).arg(e.sector));
            jeKopf[e.head]++;
            koepfeJeSpur[e.track].insert(e.head);
        }
        qInfo().noquote() << name << "| Meldungen" << r.em.size() << "Zellen"
                          << zellen.size() << "Kopf0" << jeKopf.value(0)
                          << "Kopf1" << jeKopf.value(1);
        QCOMPARE(r.em.size(), erwartet);
        QCOMPARE(int(zellen.size()), erwartet);
        QCOMPARE(jeKopf.value(0), jeSeite);
        QCOMPARE(jeKopf.value(1), jeSeite);
        for (auto it = koepfeJeSpur.cbegin(); it != koepfeJeSpur.cend(); ++it)
            QVERIFY2(it.value().size() == 2,
                     qPrintable(QString("Spur %1 hat nicht beide Seiten").arg(it.key())));

        /* Und der Reiter zeigt beide Seiten GETRENNT. */
        auto *info = tab.findChild<QTextEdit *>("textSectorInfo");
        QVERIFY(info);
        const QString text = info->toPlainText();
        const QString s0 = QString("Seite 0: %1 Sektoren").arg(jeSeite);
        const QString s1 = QString("Seite 1: %1 Sektoren").arg(jeSeite);
        QVERIFY2(text.contains(s0) && text.contains(s1),
                 qPrintable(QString("Der Status-Reiter trennt die Seiten nicht. "
                                    "Erwartet \"%1\" und \"%2\", angezeigt:\n%3")
                                .arg(s0, s1, text)));

        /* Ergebnis und Reiter: gelesen ja, geprueft nein — ADF traegt in
         * diesem Leser keine Pruefsummenangabe. */
        QVERIFY2(r.finished.contains(QString("%1 Sektoren gelesen").arg(erwartet)),
                 qPrintable("Ergebnis: " + r.finished));
        QVERIFY2(r.finished.contains("0 Pruefsumme stimmt"),
                 qPrintable("Ergebnis: " + r.finished));
        QVERIFY2(r.finished.contains(QString("%1 ohne Pruefsummenangabe").arg(erwartet)),
                 qPrintable("Ergebnis: " + r.finished));
        QVERIFY2(unbackedOkClaim(r.finished + text).isEmpty(),
                 qPrintable(unbackedOkClaim(r.finished + text)));
        QVERIFY2(text.contains("Good:    0 sectors"),
                 qPrintable("Reiter nennt nicht \"Good:    0 sectors\":\n" + text));

        const QMap<int, SeitenZeile> z = seitenZeilen(text);
        QCOMPARE(z.size(), 2);
        for (auto it = z.cbegin(); it != z.cend(); ++it)
            QVERIFY2(it.value().summe == it.value().sektoren,
                     qPrintable(QString("Klassen summieren sich nicht: %1")
                                    .arg(it.value().zeile)));
#ifdef UFT_DECODEJOB_STAGE2_V2
        QCOMPARE(r.result.rejectedSectors, 0);
#endif
    }

    /* ── c2) Abbruch greift INNERHALB der Spur ───────────────────────────
     *
     * Anforderung 4 des Auftrags: das Abbruchverhalten bleibt. Vorher
     * fragte die Schleife je Sektor; die erste Fassung dieser Tuer fragte
     * nur je Spur — gemessen: Abbruch beim 5. Sektor, danach kamen noch
     * 6 (die ganze Spur, 11 je Seite bei AmigaDOS). Jetzt vor JEDER
     * Meldung. Der eine nicht unterbrechbare Schritt ist
     * `uft_d2_from_disk()` (Kopf von decodejob.h). */
    void cancelStopsWithinTrack()
    {
        DecodeJob job;
        job.setSourcePath(corpus("xdftool_dd_ofs.adf"));
        int n = 0;
        QObject::connect(&job, &DecodeJob::sectorUpdate,
                         [&n, &job](int, int, int, const QString &) {
                             if (++n == 5) job.requestCancel();
                         });
        QSignalSpy fin(&job, SIGNAL(finished(QString)));
        QSignalSpy err(&job, SIGNAL(error(QString)));
        job.run();
        qInfo().noquote() << "Abbruch beim 5. sectorUpdate -> insgesamt" << n
                          << "Meldungen, finished" << fin.size() << "error" << err.size();
        QVERIFY2(n == 5,
                 qPrintable(QString("Nach dem Abbruch beim 5. Sektor kamen noch %1 "
                                    "Meldungen").arg(n - 5)));
        QCOMPARE(fin.size(), 0);
        QCOMPARE(err.size(), 1);
    }

    /* ── c3) Der Status-Reiter zaehlt Pruefsummenfehler je Seite ────────
     *
     * Direkt ueber die Slots, ohne Korpus: keine Datei des Korpus traegt
     * eine falsche Pruefsumme, die den Auftrag erreicht (Validator-Tor,
     * P0-17a). Vorher zaehlte „Bad" die Zeichenkette „BAD", die der
     * Auftrag nie sendet. */
    void statusTabCountsBadPerSide()
    {
        StatusTab tab;
        tab.clear();
        tab.onSectorUpdate(0, 0, 1, "CRC_BAD");
        tab.onSectorUpdate(0, 0, 2, "ID_CRC_BAD");
        tab.onSectorUpdate(0, 0, 3, "OK");
        tab.onSectorUpdate(0, 1, 1, "CRC_BAD");
        tab.onSectorUpdate(0, 1, 2, "OK");
        tab.onSectorUpdate(0, 1, 3, "WEAK");
        tab.onSectorUpdate(0, 1, 4, "DELETED");
        tab.onSectorUpdate(0, 1, 5, "UNCHECKED");
        tab.onSectorUpdate(0, 1, 6, "MISSING");
        tab.onSectorUpdate(0, 1, 7, "REJECTED");
        tab.onSectorUpdate(1, 0, -1, "NO_SECTORS");

        auto *label = tab.findChild<QLabel *>("labelTrackSide");
        QVERIFY(label);
        QVERIFY2(label->text().contains("Bad: 3"),
                 qPrintable("Kopfzeile: " + label->text()));
        QVERIFY2(label->text().contains("Good: 2"),
                 qPrintable("Kopfzeile: " + label->text()));

        tab.onDecodeFinished("Decode complete! Test");
        auto *info = tab.findChild<QTextEdit *>("textSectorInfo");
        QVERIFY(info);
        const QString text = info->toPlainText();
        QVERIFY2(text.contains("Bad:     3 sectors"), qPrintable(text));

        const QMap<int, SeitenZeile> z = seitenZeilen(text);
        QVERIFY2(z.contains(0) && z.contains(1), qPrintable(text));
        QCOMPARE(z.value(0).sektoren, 3);
        QCOMPARE(z.value(1).sektoren, 7);
        for (auto it = z.cbegin(); it != z.cend(); ++it)
            QVERIFY2(it.value().summe == it.value().sektoren,
                     qPrintable(QString("Klassen summieren sich nicht: %1")
                                    .arg(it.value().zeile)));
        QVERIFY2(z.value(0).zeile.contains("Pruefsumme falsch 2"), qPrintable(z.value(0).zeile));
        QVERIFY2(z.value(1).zeile.contains("Pruefsumme falsch 1"), qPrintable(z.value(1).zeile));
        QVERIFY2(z.value(1).zeile.contains("Flackerbits 1"), qPrintable(z.value(1).zeile));
        QVERIFY2(z.value(1).zeile.contains(QString::fromUtf8("geloescht 1")),
                 qPrintable(z.value(1).zeile));
        QVERIFY2(z.value(1).zeile.contains("abgewiesen 1"), qPrintable(z.value(1).zeile));
        /* NO_SECTORS ist eine Spur, kein Sektor: Seite 0 hat 3 Sektoren
         * und EINE Spur ohne Sektoraussage. */
        QVERIFY2(text.contains("Spuren ohne Sektoraussage 1"), qPrintable(text));
    }

    /* ── c4) Ein Spurbalken, eine Formel ─────────────────────────────── */
    void progressUsesOneFormula()
    {
        StatusTab tab;
        tab.clear();
        DecodeResult info;
        info.tracks = 80;
        info.heads = 2;
        tab.onImageInfo(info);
        auto *bar = tab.findChild<QProgressBar *>("progressTrack");
        QVERIFY(bar);
        tab.onSectorUpdate(40, 1, 1, "UNCHECKED");
        const int nachSektor = bar->value();
        tab.onProgress(55);
        const int nachFortschritt = bar->value();
        qInfo() << "Spurbalken nach sectorUpdate(40,1):" << nachSektor
                << "nach progress(55):" << nachFortschritt;
        QVERIFY2(nachSektor == nachFortschritt,
                 qPrintable(QString("Spurbalken springt zwischen zwei Formeln: "
                                    "%1 %% nach sectorUpdate, %2 %% nach progress")
                                .arg(nachSektor).arg(nachFortschritt)));
        /* Zylinder 40, Kopf 1 von 80 x 2: die Haelfte ist erreicht. */
        QCOMPARE(nachSektor, (40 * 2 + 1) * 100 / 160);
    }

    /* ── c5) Jede Statusklasse hat ihr Zeichen ───────────────────────── */
    void iconsCoverEveryStatus()
    {
#ifndef UFT_DECODEJOB_STAGE2_V2
        QFAIL("StatusTab::statusToIcon ist nicht erreichbar (privat, "
              "kennt ID_CRC_BAD/UNCHECKED/NO_SECTORS/DELETED nicht).");
#else
        const QString fehler = StatusTab::statusToIcon("CRC_BAD");
        QCOMPARE(StatusTab::statusToIcon("ID_CRC_BAD"), fehler);
        QCOMPARE(StatusTab::statusToIcon("BAD"), fehler);
        const QStringList alle = {"OK", "CRC_BAD", "WEAK", "DELETED", "MISSING",
                                  "UNCHECKED", "NO_SECTORS", "REJECTED"};
        QSet<QString> zeichen;
        for (const QString &s : alle) {
            const QString z = StatusTab::statusToIcon(s);
            QVERIFY2(z != StatusTab::statusToIcon("GIBT_ES_NICHT"),
                     qPrintable(s + " faellt auf das neutrale Zeichen"));
            zeichen.insert(z);
        }
        QCOMPARE(int(zeichen.size()), int(alle.size()));
        QVERIFY(StatusTab::statusToIcon("UNCHECKED") != StatusTab::statusToIcon("OK"));
#endif
    }

    /* ── c6) Abgewiesene Sektoren werden genannt, nicht verschluckt ─────
     *
     * `uft_d2_add_sector()` weist einen Sektor ab, der Zuversicht ohne
     * Beleg beansprucht (CONF_UNEARNED) oder fuer den kein Speicher da ist.
     * Ueber das Korpus ist das nicht erreichbar — gemessen am 2026-09-26
     * mit einem Wegwerf-Lauf ueber alle 110 Dateien von tests/corpus_free:
     * 19 erreichen den Auftrag, 17 910 Sektoren im Modell, 0 abgewiesen.
     * Der Grund steht im Code: die Bruecke deckelt die Zuversicht nur nach
     * UNTEN (`deckel()`), und 255 vergibt sie nur bei stimmender
     * Pruefsumme ohne Flackerbits. Also ein Modell von Hand:
     *   (0,0)  ein Sektor mit stimmender Pruefsumme
     *   (0,1)  ein Sektor, der Zuversicht 255 ohne Pruefsumme beansprucht
     *          -> abgewiesen; die Spur ist NICHT „ohne Sektoren"
     *   (1,0)  nichts
     *   (1,1)  ein guter und ein abgewiesener Sektor */
    void rejectedSectorsAreNamed()
    {
#ifndef UFT_DECODEJOB_STAGE2_V2
        QFAIL("DecodeJob verwirft sectors_rejected der Bruecke.");
#else
        std::unique_ptr<uft_disk2_t, void (*)(uft_disk2_t *)> d(uft_d2_create(),
                                                              uft_d2_destroy);
        QVERIFY(d);
        uint8_t daten[512];
        memset(daten, 0xE5, sizeof(daten));
        auto sektor = [&daten](uint8_t id, bool belegt) {
            uft_d2_sector_t s;
            memset(&s, 0, sizeof(s));
            s.id_sec = id;
            s.data = daten;
            s.data_len = sizeof(daten);
            s.has_data = true;
            s.origin = UFT_D2_ORIGIN_CONTAINER;
            s.conf = UFT_D2_CONF_CERTAIN;
            s.data_crc_known = belegt;
            s.data_crc_ok = belegt;
            s.idam_bit = s.dam_bit = s.data_end_bit = SIZE_MAX;
            return s;
        };
        int abgewiesen = 0;
        uft_d2_sector_t s = sektor(1, true);
        QVERIFY(uft_d2_add_sector(d.get(), uft_d2_track(d.get(), 0, 0), &s));
        s = sektor(7, false);
        if (!uft_d2_add_sector(d.get(), uft_d2_track(d.get(), 0, 1), &s)) abgewiesen++;
        s = sektor(2, true);
        QVERIFY(uft_d2_add_sector(d.get(), uft_d2_track(d.get(), 1, 1), &s));
        s = sektor(3, false);
        if (!uft_d2_add_sector(d.get(), uft_d2_track(d.get(), 1, 1), &s)) abgewiesen++;
        QCOMPARE(abgewiesen, 2);

        DecodeJob job;
        QList<Emission> em;
        QObject::connect(&job, &DecodeJob::sectorUpdate,
                         [&em](int t, int h, int sec, const QString &st) {
                             Emission e; e.track = t; e.head = h; e.sector = sec; e.status = st;
                             em.append(e);
                         });
        QVERIFY(job.tallyModel(d.get(), 2, 2, 4, 0, abgewiesen));
        const DecodeResult r = job.result();
        QStringList seen;
        for (const Emission &e : em)
            seen << QString("%1/%2/%3:%4").arg(e.track).arg(e.head).arg(e.sector).arg(e.status);
        qInfo().noquote() << "Meldungen:" << seen.join(' ');
        QCOMPARE(seen, QStringList({"0/0/1:OK", "0/1/7:REJECTED", "1/0/-1:NO_SECTORS",
                                    "1/1/2:OK", "1/1/3:REJECTED"}));
        QCOMPARE(r.totalSectors, 2);
        QCOMPARE(r.goodSectors, 2);
        QCOMPARE(r.rejectedSectors, 2);
        QCOMPARE(r.rejectedWithoutPlace, 0);
        QCOMPARE(r.tracksWithoutSectors, 1);
        const QString msg = DecodeJob::resultMessage(r);
        qInfo().noquote() << msg;
        QVERIFY2(msg.contains("2 vom Modell abgewiesen"), qPrintable(msg));

        /* Mehr abgewiesen, als Befunde mit Ort da sind (die Befundliste
         * ist begrenzt): die Differenz wird genannt, nicht verschluckt. */
        em.clear();
        QVERIFY(job.tallyModel(d.get(), 2, 2, 4, 0, abgewiesen + 3));
        QCOMPARE(job.result().rejectedSectors, 5);
        QCOMPARE(job.result().rejectedWithoutPlace, 3);
        const QString msg2 = DecodeJob::resultMessage(job.result());
        QVERIFY2(msg2.contains("5 vom Modell abgewiesen") && msg2.contains("3 davon ohne Spurangabe"),
                 qPrintable(msg2));

        /* Alle abgewiesen: keine Sektoraussage, aber auch nicht „das Plugin
         * lieferte keinen Sektor". */
        std::unique_ptr<uft_disk2_t, void (*)(uft_disk2_t *)> d1(uft_d2_create(),
                                                               uft_d2_destroy);
        s = sektor(9, false);
        QVERIFY(!uft_d2_add_sector(d1.get(), uft_d2_track(d1.get(), 0, 0), &s));
        em.clear();
        DecodeJob job1;
        QObject::connect(&job1, &DecodeJob::sectorUpdate,
                         [&em](int t, int h, int sec, const QString &st) {
                             Emission e; e.track = t; e.head = h; e.sector = sec; e.status = st;
                             em.append(e);
                         });
        QVERIFY(job1.tallyModel(d1.get(), 1, 1, 1, 0, 1));
        QCOMPARE(em.size(), 1);
        QCOMPARE(em.first().status, QString("REJECTED"));
        QCOMPARE(job1.result().tracksWithoutSectors, 0);
        const QString msg1 = DecodeJob::resultMessage(job1.result());
        qInfo().noquote() << msg1;
        QVERIFY2(msg1.startsWith(QString::fromUtf8("Lauf beendet — keine Sektoraussage")),
                 qPrintable(msg1));
        QVERIFY2(msg1.contains("1 vom Modell abgewiesen"), qPrintable(msg1));
        QVERIFY2(!msg1.contains("keinen Sektor"), qPrintable(msg1));
#endif
    }

    /* ── d0) Die Klassenzaehlung selbst, je Status (MF-1350) ───────────
     * The review of the rework let X1 (CRC_BAD/ID_CRC_BAD counted as
     * good), X4 (WEAK as good) and X8 (DELETED as good) survive: the
     * counting in tallyModel() was only exercised with OK, UNCHECKED and
     * REJECTED sectors. One sector per class, one empty track, one failed
     * read — every class and both text parts pinned (X5, X6). */
    void classCountingFromModel()
    {
        std::unique_ptr<uft_disk2_t, void (*)(uft_disk2_t *)> d(uft_d2_create(),
                                                              uft_d2_destroy);
        QVERIFY(d);
        uint8_t daten[512];
        memset(daten, 0xE5, sizeof(daten));
        auto sektor = [&daten](uint8_t id) {
            uft_d2_sector_t s;
            memset(&s, 0, sizeof(s));
            s.id_sec = id;
            s.data = daten;
            s.data_len = sizeof(daten);
            s.has_data = true;
            s.origin = UFT_D2_ORIGIN_CONTAINER;
            s.conf = UFT_D2_CONF_UNVERIFIED;
            s.idam_bit = s.dam_bit = s.data_end_bit = SIZE_MAX;
            return s;
        };
        uft_d2_track_t *t = uft_d2_track(d.get(), 0, 0);
        QVERIFY(t);
        uft_d2_sector_t s = sektor(1);                       /* OK */
        s.data_crc_known = true; s.data_crc_ok = true;
        QVERIFY(uft_d2_add_sector(d.get(), t, &s));
        s = sektor(2);                                       /* CRC_BAD */
        s.data_crc_known = true; s.data_crc_ok = false;
        QVERIFY(uft_d2_add_sector(d.get(), t, &s));
        s = sektor(3);                                       /* ID_CRC_BAD */
        s.id_crc_known = true; s.id_crc_ok = false;
        s.data_crc_known = true; s.data_crc_ok = true;
        QVERIFY(uft_d2_add_sector(d.get(), t, &s));
        s = sektor(4);                                       /* WEAK */
        s.data_crc_known = true; s.data_crc_ok = true; s.weak_bits = 2;
        QVERIFY(uft_d2_add_sector(d.get(), t, &s));
        s = sektor(5);                                       /* DELETED */
        s.data_crc_known = true; s.data_crc_ok = true; s.dam = 0xF8;
        QVERIFY(uft_d2_add_sector(d.get(), t, &s));
        s = sektor(6);                                       /* MISSING */
        s.has_data = false; s.data = nullptr; s.data_len = 0;
        QVERIFY(uft_d2_add_sector(d.get(), t, &s));
        s = sektor(7);                                       /* UNCHECKED */
        QVERIFY(uft_d2_add_sector(d.get(), t, &s));
        /* track (1,0) stays empty -> NO_SECTORS */

        DecodeJob job;
        QList<Emission> em;
        QObject::connect(&job, &DecodeJob::sectorUpdate,
                         [&em](int tr, int h, int sec, const QString &st) {
                             Emission e; e.track = tr; e.head = h; e.sector = sec; e.status = st;
                             em.append(e);
                         });
        QVERIFY(job.tallyModel(d.get(), 2, 1, 2, 1, 0));
        const DecodeResult r = job.result();
        QStringList seen;
        for (const Emission &e : em) seen << e.status;
        QCOMPARE(seen, QStringList({"OK", "CRC_BAD", "ID_CRC_BAD", "WEAK", "DELETED",
                                    "MISSING", "UNCHECKED", "NO_SECTORS"}));
        QCOMPARE(r.totalSectors, 7);
        QCOMPARE(r.goodSectors, 1);
        QCOMPARE(r.badSectors, 2);
        QCOMPARE(r.weakSectors, 1);
        QCOMPARE(r.deletedSectors, 1);
        QCOMPARE(r.missingSectors, 1);
        QCOMPARE(r.uncheckedSectors, 1);
        QCOMPARE(r.tracksWithoutSectors, 1);

        const QString msg = DecodeJob::resultMessage(r);
        qInfo().noquote() << msg;
        QVERIFY2(msg.contains("7 Sektoren gelesen"), qPrintable(msg));
        QVERIFY2(msg.contains("1 Pruefsumme stimmt"), qPrintable(msg));
        QVERIFY2(msg.contains("2 Pruefsumme falsch"), qPrintable(msg));
        /* X5: a partial read must say how many tracks gave nothing. */
        QVERIFY2(msg.contains("1 von 2 Spuren ohne Sektoraussage"), qPrintable(msg));
        /* X6: the error-code note, and what it does NOT say. */
        QVERIFY2(msg.contains("1 mit Fehlercode von read_track"), qPrintable(msg));
        QVERIFY2(msg.contains("sagt der Rueckgabewert nicht"), qPrintable(msg));
    }

    /* ── d1) Ein abgebrochener Lesevorgang ist kein fertiger (MF-1350) ── */
    void abortedReadIsNotComplete()
    {
        DecodeResult r;
        r.formatName = "X";
        r.platformName = "Y";
        r.readAborted = true;
        r.totalSectors = 5;
        r.goodSectors = 5;
        r.tracksAsked = 3;
        const QString msg = DecodeJob::resultMessage(r);
        qInfo().noquote() << msg;
        QVERIFY2(!msg.contains("Decode complete"), qPrintable(msg));
        QVERIFY2(msg.startsWith(QString::fromUtf8("Lesen abgebrochen — Teilergebnis")),
                 qPrintable(msg));
        r.totalSectors = 0;
        r.goodSectors = 0;
        const QString leer = DecodeJob::resultMessage(r);
        QVERIFY2(leer.startsWith(QString::fromUtf8("Lesen abgebrochen — keine Sektoraussage")),
                 qPrintable(leer));
    }

    /* ── d1a) Keine Plattform: keine leere Klammer (P2-2, MF-1351) ───────
     *
     * Seit das Pruefetor die Registry fragt, kommt fuer jedes Format
     * ausserhalb seiner festen Liste KEINE Plattform — die Registry nennt
     * keine, und das Pruefetor erfindet keine. Der Fertigtext lautete
     * dann gemessen „Decode complete! ImageDisk (IMD) (): 320 Sektoren". */
    void emptyPlatformNotShown()
    {
        DecodeResult r;
        r.formatName = "ImageDisk (IMD)";
        r.totalSectors = 3;
        r.uncheckedSectors = 3;
        r.tracksAsked = 1;
        QString msg = DecodeJob::resultMessage(r);
        QVERIFY2(!msg.contains("()"), qPrintable(msg));
        QVERIFY2(msg.contains("ImageDisk (IMD):"), qPrintable(msg));
        r.totalSectors = r.uncheckedSectors = 0;
        msg = DecodeJob::resultMessage(r);
        QVERIFY2(!msg.contains("()"), qPrintable(msg));
        r.platformName = "Atari";
        msg = DecodeJob::resultMessage(r);
        QVERIFY2(msg.contains("ImageDisk (IMD) (Atari)"), qPrintable(msg));
    }

    /* ── d1b) Der Pfad, nicht die Datei (P2-2 Nachbesserung, MF-1351) ────
     *
     * Der Auftrag oeffnete mit `m_sourcePath.toUtf8()`; die C-Schicht
     * oeffnet mit einem schmalen fopen() in der Codepage des Systems
     * (Windows: ANSI). Zwei Namen fuer bekannte Inhalte:
     *   „Übung_ä.imd"  (hxcfe_pc160.imd) — darstellbar, muss ein Plugin
     *                  finden wie unter jedem anderen Namen
     *   „Ωα.st"        (hxcfe_720k.st)   — die feste Liste des Validators
     *                  nimmt sie an (sie liest ueber QFile, nicht fopen);
     *                  ist der Name nicht darstellbar, endet der Auftrag
     *                  mit einer Absage, die den PFAD nennt — nicht mit
     *                  „kein Plugin hat die Datei geoeffnet", das waere
     *                  ein Urteil ueber die Datei. */
    void pathInSystemCodePage_data()
    {
        QTest::addColumn<QString>("quelle");
        QTest::addColumn<QString>("name");
        QTest::newRow("umlaut") << "hxcfe_pc160.imd"
                                << QString::fromUtf8("\xC3\x9C" "bung_\xC3\xA4.imd");
        QTest::newRow("griechisch") << "hxcfe_720k.st"
                                    << QString::fromUtf8("\xCE\xA9\xCE\xB1.st");
    }

    void pathInSystemCodePage()
    {
        QFETCH(QString, quelle);
        QFETCH(QString, name);
        if (!QFile::exists(corpus(quelle)))
            QSKIP(qPrintable("Korpus fehlt: " + quelle));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString pfad = dir.filePath(name);
        QVERIFY(QFile::copy(corpus(quelle), pfad));
        const bool darstellbar =
            QFile::decodeName(QFile::encodeName(pfad)) == pfad;

        const JobRun r = runJob(pfad);
        qInfo().noquote() << QString("PFAD %1 darstellbar=%2 leser=\"%3\" "
                                     "fertig=\"%4\" fehler=\"%5\"")
                                 .arg(name).arg(darstellbar)
                                 .arg(r.result.readerName, r.finished, r.error);
        if (darstellbar) {
            QVERIFY2(!r.result.readerName.isEmpty(),
                     qPrintable("kein Plugin fuer einen darstellbaren Pfad: "
                                + r.finished + r.error));
            QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
        } else {
            QVERIFY2(r.finished.isEmpty(), qPrintable(r.finished));
            QVERIFY2(r.error.contains("Systemcodepage"), qPrintable(r.error));
            QVERIFY(r.em.isEmpty());
        }
    }

    /* ── d2) Nur Abgewiesenes: kein Haken im Reiter (Gegenpruefung X2) ── */
    void tabNoCheckMarkForRejectedOnly()
    {
        StatusTab tab;
        tab.clear();
        tab.onSectorUpdate(0, 0, 7, "REJECTED");
        tab.onSectorUpdate(0, 0, 8, "REJECTED");
        tab.onSectorUpdate(1, 0, -1, "NO_SECTORS");
        tab.onDecodeFinished(QString::fromUtf8("Lauf beendet — keine Sektoraussage: X"));
        auto *info = tab.findChild<QTextEdit *>("textSectorInfo");
        auto *logw = tab.findChild<QTextEdit *>("textLog");
        QVERIFY(info && logw);
        const QString reiter = info->toPlainText();
        const QString protokoll = logw->toPlainText();
        QVERIFY2(!reiter.contains(QChar(0x2713)) && !protokoll.contains(QChar(0x2713)),
                 qPrintable(reiter + " | " + protokoll));
        QVERIFY2(!reiter.contains("Decode Complete") && !protokoll.contains("Decode Complete"),
                 qPrintable(reiter + " | " + protokoll));
        QVERIFY2(reiter.contains("keine Sektoraussage"), qPrintable(reiter));
    }

    /* ── d) Die Uebersetzung Modell -> Status, je Fall ───────────────── */
    void statusMapping()
    {
#ifndef UFT_DECODEJOB_MODEL_STATUS
        QFAIL("DecodeJob kennt keinen Sektorstatus aus dem Modell.");
#else
        uft_d2_sector_t s;
        auto frisch = [&s]() {
            memset(&s, 0, sizeof(s));
            s.has_data = true;
            s.data_len = 512;
        };

        frisch();                                        /* keine Angabe */
        QCOMPARE(DecodeJob::statusOf(s), QString("UNCHECKED"));

        frisch(); s.data_crc_known = true; s.data_crc_ok = true;
        QCOMPARE(DecodeJob::statusOf(s), QString("OK"));

        frisch(); s.data_crc_known = true; s.data_crc_ok = false;
        QCOMPARE(DecodeJob::statusOf(s), QString("CRC_BAD"));

        frisch(); s.id_crc_known = true; s.id_crc_ok = false;
        s.data_crc_known = true; s.data_crc_ok = true;
        QCOMPARE(DecodeJob::statusOf(s), QString("ID_CRC_BAD"));

        frisch(); s.data_crc_known = true; s.data_crc_ok = true; s.dam = 0xF8;
        QCOMPARE(DecodeJob::statusOf(s), QString("DELETED"));

        frisch(); s.data_crc_known = true; s.data_crc_ok = true; s.weak_bits = 3;
        QCOMPARE(DecodeJob::statusOf(s), QString("WEAK"));

        /* Eine falsche Pruefsumme wiegt schwerer als die Flackerbits. */
        frisch(); s.data_crc_known = true; s.data_crc_ok = false; s.weak_bits = 3;
        QCOMPARE(DecodeJob::statusOf(s), QString("CRC_BAD"));

        /* ... und schwerer als die Loeschmarke: ein geloeschter Sektor mit
         * BEKANNT falscher Pruefsumme ist ein schlechter Sektor. */
        frisch(); s.data_crc_known = true; s.data_crc_ok = false; s.dam = 0xF8;
        QCOMPARE(DecodeJob::statusOf(s), QString("CRC_BAD"));
        frisch(); s.id_crc_known = true; s.id_crc_ok = false; s.dam = 0xF8;
        s.data_crc_known = true; s.data_crc_ok = true;
        QCOMPARE(DecodeJob::statusOf(s), QString("ID_CRC_BAD"));

        frisch(); s.has_data = false; s.data_len = 0;
        s.data_crc_known = true; s.data_crc_ok = true;
        QCOMPARE(DecodeJob::statusOf(s), QString("MISSING"));

        /* „nicht bekannt" ist nicht „stimmt": ein id_crc_ok ohne
         * id_crc_known ist die Vorgabe des Nullens, keine Messung. */
        frisch(); s.id_crc_ok = false; s.id_crc_known = false;
        QCOMPARE(DecodeJob::statusOf(s), QString("UNCHECKED"));

        /* The same for the DATA field (review mutation X3): a data_crc_ok
         * without data_crc_known is no "OK". */
        frisch(); s.data_crc_ok = true; s.data_crc_known = false;
        QCOMPARE(DecodeJob::statusOf(s), QString("UNCHECKED"));
#endif
    }
};

QTEST_MAIN(TestDecodeJobNoFiction)
#include "test_decode_job_no_fiction.moc"

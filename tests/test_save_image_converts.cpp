/**
 * @file test_save_image_converts.cpp
 * @brief Speichern unter wandelt oder lehnt ab — es etikettiert nicht um
 *        (MF-664)
 *
 * ── Der gemessene Ausgangszustand ────────────────────────────────────────
 *
 * `MainWindow::onSave()` las die Quelldatei und schrieb **dieselben
 * Bytes** (`src/mainwindow.cpp`, vor MF-664: `outFile.write(imageData)`).
 * Ein Format-Schreiber kam nicht vor. „Speichern unter → HFE" legte bei
 * einem geladenen D64 also dessen Bytes in einer `.hfe`-Datei ab.
 *
 * Dieselbe Klasse wie GUI-1 (MF-635, „Convert = `QFile::copy`") und
 * dieselbe, die MF-568 im Dekodier-Auftrag schon einmal beheben musste.
 *
 * Und ein zweiter Fehler in derselben Funktion: `onSaveAs()` setzte
 * `m_currentFile` auf den **Zielnamen** und rief dann `onSave()`, das
 * mit `if (!srcFile.exists())` prüft. Speichern unter einem NEUEN Namen
 * scheiterte damit stets — mit der irreführenden Meldung „Source file no
 * longer exists".
 *
 * ── Was dieser Test festhält ─────────────────────────────────────────────
 *
 * 1. Gleiches Format → Byte-Kopie, und die ist bitgleich.
 * 2. **Verschiedene Formate → gewandelt ODER abgelehnt, nie umbenannt.**
 *    Insbesondere darf am Ziel niemals der Quell-Kopf stehen.
 * 3. Ein neuer Zielname funktioniert (der zweite Fehler oben).
 * 4. Unbekannte Endung → Ablehnung, und es wird nichts geschrieben.
 *
 * Punkt 2 ist der eigentliche Rotbeweis. Er prüft **nicht**, dass die
 * Wandlung gelingt — die meisten Paare weist das Preflight-Tor zu Recht
 * als UNGEPRÜFT ab. Er prüft, dass im Ablehnungsfall **keine Datei**
 * entsteht, und im Erfolgsfall eine, die nicht mehr wie die Quelle
 * aussieht. Beides ist falsifizierbar; „die Wandlung klappt" wäre es
 * nicht.
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "uft_save_image.h"

extern "C" {
#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
}

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "."
#endif

class TestSaveImageConverts : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString korpus(const char *name) const
    {
        return QString(UFT_CORPUS_DIR) + "/" + QString::fromLatin1(name);
    }

    static QByteArray kopf(const QString &pfad, int n = 16)
    {
        QFile f(pfad);
        if (!f.open(QIODevice::ReadOnly)) return QByteArray();
        return f.read(n);
    }

private slots:

    void initTestCase()
    {
        QVERIFY2(m_dir.isValid(), "kein Temporaerverzeichnis");
        QVERIFY2(uft_register_all_formats() == UFT_OK,
                 "Format-Registry liess sich nicht aufbauen");
        QVERIFY2(QFileInfo::exists(korpus("vice_c1541_35trk.d64")),
                 "Korpus-D64 fehlt");
    }

    /* 1. Gleiches Format: Kopie, bitgleich. */
    void gleiches_format_ist_bitgleich()
    {
        const QString src = korpus("vice_c1541_35trk.d64");
        const QString dst = m_dir.filePath("kopie.d64");

        const UftSaveOutcome r = uftSaveImageAs(src, dst);
        QVERIFY2(r.ok, qPrintable("abgelehnt: " + r.message));
        QVERIFY2(!r.converted, "gleiches Format darf nicht 'gewandelt' heissen");

        QFile a(src), b(dst);
        QVERIFY(a.open(QIODevice::ReadOnly));
        QVERIFY(b.open(QIODevice::ReadOnly));
        QCOMPARE(b.readAll(), a.readAll());
    }

    /* 2. DER ROTBEWEIS: ein D64 darf nicht als HFE durchgehen. */
    void d64_wird_nicht_zu_hfe_umetikettiert()
    {
        const QString src = korpus("vice_c1541_35trk.d64");
        const QString dst = m_dir.filePath("umetikettiert.hfe");

        const UftSaveOutcome r = uftSaveImageAs(src, dst);

        if (!r.ok) {
            // Abgelehnt ist ein voellig richtiges Ergebnis — dann darf
            // aber auch NICHTS am Ziel liegen.
            QVERIFY2(!QFileInfo::exists(dst),
                     "abgelehnt, aber es liegt trotzdem eine Datei am Ziel");
            QVERIFY2(!r.message.isEmpty(), "Ablehnung ohne Begruendung");
            return;
        }

        // Gelungen? Dann muss es ein HFE sein — und nicht der D64-Inhalt.
        const QByteArray k = kopf(dst, 8);
        QVERIFY2(k != kopf(src, 8),
                 "die Zieldatei traegt den Kopf der QUELLE — sie wurde nur "
                 "umbenannt, nicht gewandelt");
        QVERIFY2(k.startsWith("HXCPICFE") || k.startsWith("HXCHFEV3"),
                 qPrintable(QString("Ziel traegt kein HFE-Magic, sondern: %1")
                                .arg(QString::fromLatin1(k.toHex()))));
    }

    /* 2b. MF-879: Speichern auf sich selbst schreibt NICHTS.
     *
     * `MainWindow::onSave()` (Strg+S) ruft `uftSaveImageAs(x, x)`. Ohne
     * die Identitaets-Erkennung lief das in `kopiere()`, und dort steht
     * `out.open(WriteOnly)` auf der QUELLE — das kuerzt sie, bevor
     * irgendetwas geschrieben ist. Bei einer Kurzschreibung entfernt die
     * Aufraeumzeile `QFile::remove(target)` anschliessend genau das
     * Original.
     *
     * Der Rotbeweis nutzt eine SCHREIBGESCHUETZTE Datei. Sie deckt beides
     * auf einmal auf:
     *   - vorher: `out.open(WriteOnly)` schlaegt fehl, das Speichern wird
     *     abgelehnt. Eine Datei, die bereits gespeichert IST, laesst sich
     *     also nicht speichern.
     *   - nachher: es wird gar nicht erst geoeffnet, also gelingt es.
     *
     * Und er ist gefahrlos: gerade weil die Datei schreibgeschuetzt ist,
     * kann der Rotbeweis das Korpus-Abbild nicht beschaedigen. */
    void speichern_auf_sich_selbst_schreibt_nichts()
    {
        const QString datei = m_dir.filePath("identitaet.d64");
        QVERIFY2(QFile::copy(korpus("vice_c1541_35trk.d64"), datei),
                 "Arbeitskopie liess sich nicht anlegen");

        QFile f(datei);
        const qint64 groesseVorher = QFileInfo(datei).size();
        QVERIFY(groesseVorher > 0);
        const QByteArray kopfVorher = kopf(datei, 32);

        /* Schreibgeschuetzt: jeder Schreibversuch muss auffallen. */
        QVERIFY2(f.setPermissions(QFile::ReadOwner | QFile::ReadUser),
                 "Schreibschutz liess sich nicht setzen");

        const UftSaveOutcome r = uftSaveImageAs(datei, datei);

        /* Zuruecksetzen, damit das Temporaerverzeichnis aufraeumen kann —
         * vor den Pruefungen, sonst bleibt es bei einem Fehlschlag liegen. */
        f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                         QFile::ReadUser  | QFile::WriteUser);

        QVERIFY2(r.ok, qPrintable(
            "Speichern auf sich selbst wurde abgelehnt: " + r.message +
            "  — die Datei ist bereits gespeichert; hier wird versucht, "
            "sie neu zu schreiben, und genau dieser Schreibversuch kann "
            "das Original kuerzen"));
        QVERIFY2(!r.converted, "eine Identitaet ist keine Wandlung");
        QCOMPARE(QFileInfo(datei).size(), groesseVorher);
        QCOMPARE(kopf(datei, 32), kopfVorher);
    }

    /* 3. Ein NEUER Zielname muss funktionieren. */
    void neuer_zielname_funktioniert()
    {
        const QString src = korpus("vice_c1541_35trk.d64");
        const QString dst = m_dir.filePath("noch_nicht_da.d64");
        QVERIFY(!QFileInfo::exists(dst));

        const UftSaveOutcome r = uftSaveImageAs(src, dst);
        QVERIFY2(r.ok, qPrintable("Speichern unter neuem Namen abgelehnt: "
                                  + r.message));
        QVERIFY(QFileInfo::exists(dst));
    }

    /* 4. Unbekannte Endung: ablehnen, nichts schreiben. */
    void unbekannte_endung_schreibt_nichts()
    {
        const QString src = korpus("vice_c1541_35trk.d64");
        const QString dst = m_dir.filePath("was_ist_das.qqq");

        const UftSaveOutcome r = uftSaveImageAs(src, dst);
        QVERIFY2(!r.ok, "eine unbekannte Endung darf nicht durchgehen");
        QVERIFY2(!QFileInfo::exists(dst),
                 "abgelehnt, aber es liegt eine Datei am Ziel");
    }

    /* 6. MF-666: eine nicht schreibbare Variante wird ABGELEHNT, nicht
     * stillschweigend durch die schreibbare ersetzt.
     *
     * Das ist der Rotbeweis fuer den Waehler: HFEv3 ist lesbar und NICHT
     * schreibbar (uft_hfe.c:797). Wuerde uftSaveImageAs() die Wahl
     * ignorieren und einfach v1 schreiben, waere der Waehler ein
     * Bedienelement ohne Wirkung — genau das, was Stufe 5 beseitigt. */
    void nicht_schreibbare_variante_wird_abgelehnt()
    {
        const QString src = korpus("vice_c1541_35trk.d64");
        const QString dst = m_dir.filePath("v3_versuch.hfe");

        const UftSaveOutcome r = uftSaveImageAs(src, dst, "HFEv3");
        QVERIFY2(!r.ok, "HFEv3 ist nicht schreibbar und muss abgelehnt werden");
        QVERIFY2(!QFileInfo::exists(dst),
                 "abgelehnt, aber es liegt eine Datei am Ziel");
        QVERIFY2(r.message.contains("HFEv3"),
                 "die Ablehnung muss die Variante benennen");
    }

    /* 7. Ein Name, den es gar nicht gibt: ebenfalls Ablehnung. */
    void unbekannte_variante_wird_abgelehnt()
    {
        const QString src = korpus("vice_c1541_35trk.d64");
        const QString dst = m_dir.filePath("phantasie.hfe");

        const UftSaveOutcome r = uftSaveImageAs(src, dst, "HFEv99");
        QVERIFY2(!r.ok, "eine erfundene Variante darf nicht durchgehen");
        QVERIFY2(!QFileInfo::exists(dst), "es darf nichts geschrieben werden");
    }

    /* 5. Kein Abbild geladen: sagen, nicht schweigen. */
    void ohne_quelle_wird_nichts_geschrieben()
    {
        const QString dst = m_dir.filePath("aus_dem_nichts.d64");
        const UftSaveOutcome r = uftSaveImageAs(QString(), dst);
        QVERIFY(!r.ok);
        QVERIFY2(!r.message.isEmpty(), "Ablehnung ohne Begruendung");
        QVERIFY(!QFileInfo::exists(dst));
    }
};

QTEST_MAIN(TestSaveImageConverts)
#include "test_save_image_converts.moc"

/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_sector_editor_geometry.cpp
 * @brief Der Sektoreditor zeigt die Spur, die dasteht (MF-895)
 *
 * ── Was gemessen wurde ────────────────────────────────────────────────────
 *
 * `UftSectorEditor::sectorOffset()` fuehrte eine EIGENE Zonentabelle mit
 * 35 Eintraegen und summierte sie so:
 *
 *     for (int t = 0; t < track && t < 35; t++)
 *         offset += sectorsPerTrackD64[t] * 256;
 *
 * Der Zweig gilt fuer `m_format == "D64" || m_format == "D71"`, und
 * `loadDisk()` setzt bei einer `.d71` `m_totalTracks = 70`. Die Klammer
 * `t < 35` haelt die Summe aber bei 683 Bloecken an. Folge, gemessen:
 * **alle 35 Spuren der zweiten Seite** — Spurindex 35 bis 69 — liefern
 * denselben Versatz 683*256 = 0x2AB00.
 *
 * Das ist nicht nur eine falsche Anzeige. Derselbe Versatz speist
 * `saveSector()`, und `save()` schreibt `m_diskData` mit `file.write()`
 * zurueck. Wer auf Spur 53 ein Byte aendert, aendert Spur 36. Stille
 * Veraenderung auf einem Schreibpfad — die Klasse aus MF-877.
 *
 * Dazu kannte `sectorsPerTrack()` nur `"D64"`, nicht `"D71"`, und fiel
 * sonst auf `return 1` durch. Der Sektor-Zaehler stand bei einer D71
 * deshalb auf 0..0: von 21 Sektoren der Spur 1 war **einer** erreichbar.
 *
 * ── Warum die zweite Seite der Diskette der Pruefstein ist ────────────────
 *
 * `tests/corpus_free/vice_c1541_70trk.d71` (VICE `c1541`, 1366 Bloecke)
 * ist frisch formatiert. Gemessen ueber alle 70 Spuren traegt genau EINE
 * Spur der zweiten Seite Daten: die zweite BAM auf CBM-Spur 53
 * (= Seite 1, Spur 18) bei 0x41000, beginnend `FF FF 1F FF FF 1F`.
 * An der Stelle, die der Editor stattdessen zeigte — 0x2AB00 —, steht
 * lauter 0x00.
 *
 * Damit ist der Beweis nicht "es hat sich etwas geaendert", sondern
 * "an Spur 53 steht das, was in der Datei an Spur 53 steht" — und die
 * Gegenprobe faellt sofort auf, weil Null und `FF FF 1F` nicht zu
 * verwechseln sind.
 *
 * ── Die Zahlen kommen aus dem SSOT, nicht aus einer vierten Tabelle ───────
 *
 * `include/uft/formats/cbm/uft_cbm_geometry.h` (MF-434) haelt die
 * Zonenaufteilung; ihr Kopf zaehlt auf, dass der Baum vorher 24 Kopien
 * davon trug. Die Tabelle im Sektoreditor war die 25.
 * Wichtig fuer die Umrechnung: die SSOT zaehlt Spuren **1-basiert**, die
 * Oberflaeche 0-basiert — und `UFT_CBM_1571` beschreibt EINE Seite
 * (`max_track` 42, `heads` 2), nicht 70 fortlaufende Spuren. Der
 * D71-Aufbau ist deshalb "zweimal die 1541-Seite hintereinander", und
 * genau so rechnet der Editor jetzt.
 *
 * ── Was der Test NICHT prueft ─────────────────────────────────────────────
 *
 * Den Schreibpfad `save()` — er stellt eine Rueckfrage per QMessageBox und
 * ist kopflos nicht anzutreiben. Geprueft wird der Versatz, den er
 * benutzt; das ist die Stelle, an der die Verwechslung entstand.
 */
#include <QtTest/QtTest>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QFile>
#include <QTemporaryDir>

#include "gui/uft_sector_editor.h"

class TestSectorEditorGeometry : public QObject
{
    Q_OBJECT

private:
    static QString korpusD71()
    {
#ifdef UFT_CORPUS_DIR
        return QString(UFT_CORPUS_DIR) + "/vice_c1541_70trk.d71";
#else
        return QString();
#endif
    }

    /** Bytes direkt aus der Datei — unabhaengig von jeder Editor-Rechnung. */
    static QByteArray ausDerDatei(const QString &pfad, qint64 versatz, int laenge)
    {
        QFile f(pfad);
        if (!f.open(QIODevice::ReadOnly)) return QByteArray();
        if (!f.seek(versatz)) return QByteArray();
        return f.read(laenge);
    }

    static UftHexEdit *hex(UftSectorEditor *e)
    {
        return e->findChild<UftHexEdit *>();
    }

    static QSpinBox *spin(UftSectorEditor *e, const char *name)
    {
        return e->findChild<QSpinBox *>(QString::fromLatin1(name));
    }

private slots:

    /* ─────────────────────────────────────────────────────────────────────
     *  1. Die zweite Seite kollabierte auf eine einzige Spur.
     * ───────────────────────────────────────────────────────────────────── */
    void zweiteSeiteKollabiertNicht()
    {
        const QString d71 = korpusD71();
        if (d71.isEmpty() || !QFile::exists(d71))
            QSKIP("Korpus-Abbild vice_c1541_70trk.d71 nicht vorhanden");

        UftSectorEditor editor;
        editor.loadDisk(d71);

        /* CBM-Spur 53 = Spurindex 52. Dort liegt die zweite BAM. */
        editor.goToSector(52, 0);
        const QByteArray gezeigt = hex(&editor) ? hex(&editor)->data() : QByteArray();

        /* 0x41000: 683 Bloecke Seite 0 + 357 Bloecke bis Spur 18, mal 256.
         * Selbst nachgerechnet, nicht vom Editor uebernommen. */
        const QByteArray erwartet = ausDerDatei(d71, 0x41000, 256);
        QVERIFY2(!erwartet.isEmpty(), "Korpus-Abbild nicht lesbar");
        QCOMPARE(erwartet.left(6), QByteArray("\xFF\xFF\x1F\xFF\xFF\x1F", 6));

        QCOMPARE(gezeigt.size(), 256);
        QVERIFY2(gezeigt == erwartet,
                 "Spur 53 zeigt nicht die Bytes, die in der Datei auf Spur 53 "
                 "stehen — die zweite Seite kollabiert auf einen Versatz");
    }

    /* ─────────────────────────────────────────────────────────────────────
     *  2. Zwei verschiedene Spuren der zweiten Seite sind verschieden.
     *
     *  Der schaerfere Ausdruck desselben Fehlers: vorher lieferten
     *  Spurindex 35 und 52 BEIDE den Versatz 0x2AB00.
     * ───────────────────────────────────────────────────────────────────── */
    void verschiedeneSpurenVerschiedenerInhalt()
    {
        const QString d71 = korpusD71();
        if (d71.isEmpty() || !QFile::exists(d71))
            QSKIP("Korpus-Abbild vice_c1541_70trk.d71 nicht vorhanden");

        UftSectorEditor editor;
        editor.loadDisk(d71);

        editor.goToSector(35, 0);
        const QByteArray spur36 = hex(&editor) ? hex(&editor)->data() : QByteArray();
        editor.goToSector(52, 0);
        const QByteArray spur53 = hex(&editor) ? hex(&editor)->data() : QByteArray();

        QCOMPARE(spur36.size(), 256);
        QCOMPARE(spur53.size(), 256);
        QVERIFY2(spur36 != spur53,
                 "Spur 36 und Spur 53 zeigen denselben Inhalt — 35 Spuren "
                 "der zweiten Seite bilden auf denselben Versatz ab");

        /* Und die erste Seite bleibt, wo sie war: CBM-Spur 18, die BAM. */
        editor.goToSector(17, 0);
        const QByteArray bam = hex(&editor) ? hex(&editor)->data() : QByteArray();
        QCOMPARE(bam, ausDerDatei(d71, 0x16500, 256));
    }

    /* ─────────────────────────────────────────────────────────────────────
     *  3. Bei einer D71 sind alle 21 Sektoren der Spur 1 erreichbar.
     * ───────────────────────────────────────────────────────────────────── */
    void sektorzaehlerKenntD71()
    {
        const QString d71 = korpusD71();
        if (d71.isEmpty() || !QFile::exists(d71))
            QSKIP("Korpus-Abbild vice_c1541_70trk.d71 nicht vorhanden");

        UftSectorEditor editor;
        editor.loadDisk(d71);

        QSpinBox *sektor = spin(&editor, "sectorEditorSectorSpin");
        QSpinBox *spur   = spin(&editor, "sectorEditorTrackSpin");
        QVERIFY2(sektor && spur, "Zaehler nicht auffindbar (Objektname fehlt)");

        editor.goToSector(0, 0);
        QCOMPARE(sektor->maximum(), 20);   /* Zone 3: 21 Sektoren */
        editor.goToSector(24, 0);
        QCOMPARE(sektor->maximum(), 17);   /* CBM-Spur 25, Zone 1: 18 */
        editor.goToSector(52, 0);
        QCOMPARE(sektor->maximum(), 18);   /* CBM-Spur 53 = Seite 1 Spur 18 */
        QCOMPARE(spur->maximum(), 69);
    }

    /* ─────────────────────────────────────────────────────────────────────
     *  4. Ein Bereichsfehler zeigt keine 256 Nullbytes als "Sektorinhalt".
     *
     *  `loadSector()` setzte bei jedem Bereichsfehler
     *  `QByteArray(m_sectorSize, 0)` in die Hex-Ansicht — 256 Nullbytes,
     *  von einem echten leeren Sektor nicht zu unterscheiden. Dieselbe
     *  Klasse wie die Rampe aus MF-892.
     * ───────────────────────────────────────────────────────────────────── */
    void bereichsfehlerZeigtKeineErfundenenNullen()
    {
        const QString d71 = korpusD71();
        if (d71.isEmpty() || !QFile::exists(d71))
            QSKIP("Korpus-Abbild vice_c1541_70trk.d71 nicht vorhanden");

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString kurz = tmp.path() + "/gekuerzt.d71";
        QVERIFY(QFile::copy(d71, kurz));
        {
            QFile f(kurz);
            QVERIFY(f.open(QIODevice::ReadWrite));
            QVERIFY(f.resize(100 * 256));   /* nur 100 Bloecke */
        }

        UftSectorEditor editor;
        editor.loadDisk(kurz);
        editor.goToSector(60, 0);           /* weit hinter dem Dateiende */

        const QByteArray gezeigt = hex(&editor) ? hex(&editor)->data() : QByteArray();
        QVERIFY2(gezeigt != QByteArray(256, '\0'),
                 "Ein Sektor ausserhalb des Abbilds wird als 256 Nullbytes "
                 "angezeigt — eine Erfindung");
        QVERIFY2(gezeigt.isEmpty(),
                 "Ausserhalb des Abbilds gehoert nichts in die Hex-Ansicht");

        QLabel *versatz = editor.findChild<QLabel *>("sectorEditorOffsetLabel");
        QVERIFY2(versatz, "Versatz-Feld nicht auffindbar (Objektname fehlt)");
        QVERIFY2(versatz->text().contains("ausserhalb"),
                 "Die Anzeige sagt nicht, dass die Auswahl ausserhalb liegt");
    }

    /* ─────────────────────────────────────────────────────────────────────
     *  5. Die Format-Auswahl sagt, welches Format geladen ist.
     *
     *  `m_formatCombo` wurde angelegt, gefuellt, eingehaengt — und nie
     *  gelesen, nie gesetzt, nie verbunden (gemessen ueber die ganze
     *  Datei). Nach dem Laden einer .d71 stand dort weiter "D64 (C64)".
     * ───────────────────────────────────────────────────────────────────── */
    void formatAuswahlIstNichtDekorativ()
    {
        const QString d71 = korpusD71();
        if (d71.isEmpty() || !QFile::exists(d71))
            QSKIP("Korpus-Abbild vice_c1541_70trk.d71 nicht vorhanden");

        UftSectorEditor editor;
        editor.loadDisk(d71);

        QComboBox *kombo = editor.findChild<QComboBox *>("sectorEditorFormatCombo");
        QVERIFY2(kombo, "Format-Auswahl nicht auffindbar (Objektname fehlt)");
        QVERIFY2(kombo->currentText().startsWith("D71"),
                 "Die Format-Auswahl nennt ein anderes Format als das geladene");

        /* Und sie wirkt: auf D64 umgestellt, endet die Diskette bei Spur 35. */
        const int d64 = kombo->findText("D64", Qt::MatchStartsWith);
        QVERIFY(d64 >= 0);
        kombo->setCurrentIndex(d64);
        QSpinBox *spur = spin(&editor, "sectorEditorTrackSpin");
        QVERIFY(spur);
        QCOMPARE(spur->maximum(), 34);
    }
};

QTEST_MAIN(TestSectorEditorGeometry)
#include "test_sector_editor_geometry.moc"

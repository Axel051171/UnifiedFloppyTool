// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file test_otdr_panel_scp_ablage.cpp
 * @brief The OTDR panel serves every SCP cylinder under its real
 *        (cylinder, head) — modern, legacy single-sided and odd start
 *        track (P3-706, MF-1605).
 *
 * UftOtdrPanel::loadFluxImage() filed each slot under slot - start_track
 * and labelled it t / 2, t % 2; trackFlux(), trackRevolutions() and
 * analyzeTrack() look up cylinder * 2 + head. The two agree only for
 * start_track 0 in the interleaved layout. The flux view (fluxTrackReady)
 * reads trackFlux(), so a wrong filing is a wrong picture.
 *
 * The file: tests/corpus_free/gw_fm_acorn_3trk.scp (greaseweazle,
 * single-sided, slots 0/2/4). Two copies are built in memory, as in
 * tests/test_scp_einseitig_fortlaufend.c:
 *   - legacy: slots 0/1/2 (greaseweazle image/scp.py:246-254);
 *   - side 1: heads field 2, slots 1/3/5, start_track 1 — the same data
 *     as head 1, the case where slot - start_track is off by one.
 * The flux of every cylinder must match the original's.
 */
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "gui/uft_otdr_panel.h"

class TestOtdrPanelScpAblage : public QObject
{
    Q_OBJECT

    static void put32(QByteArray &b, int at, quint32 v)
    {
        b[at] = static_cast<char>(v & 0xFF);
        b[at + 1] = static_cast<char>((v >> 8) & 0xFF);
        b[at + 2] = static_cast<char>((v >> 16) & 0xFF);
        b[at + 3] = static_cast<char>((v >> 24) & 0xFF);
    }
    static quint32 le32(const QByteArray &b, int at)
    {
        const auto *p = reinterpret_cast<const unsigned char *>(b.constData() + at);
        return quint32(p[0]) | quint32(p[1]) << 8 | quint32(p[2]) << 16 | quint32(p[3]) << 24;
    }
    static void pruefsumme(QByteArray &b)
    {
        quint32 sum = 0;
        for (int i = 0x10; i < b.size(); i++)
            sum += static_cast<unsigned char>(b[i]);
        put32(b, 0x0C, sum);
    }

    /* plaetze[c] = table slot for cylinder c; the TDH number follows it */
    static QByteArray kopie(const QByteArray &orig, const int plaetze[3],
                            int heads, int start, int end)
    {
        QByteArray l = orig;
        quint32 off[3];
        for (int c = 0; c < 3; c++) off[c] = le32(orig, 0x10 + 4 * (2 * c));
        for (int i = 0; i < 168; i++) put32(l, 0x10 + 4 * i, 0);
        for (int c = 0; c < 3; c++) {
            put32(l, 0x10 + 4 * plaetze[c], off[c]);
            l[int(off[c]) + 3] = static_cast<char>(plaetze[c]);
        }
        l[0x0A] = static_cast<char>(heads);
        l[6] = static_cast<char>(start);
        l[7] = static_cast<char>(end);
        pruefsumme(l);
        return l;
    }

    static bool schreibe(const QString &pfad, const QByteArray &b)
    {
        QFile f(pfad);
        return f.open(QIODevice::WriteOnly) && f.write(b) == b.size();
    }

private slots:
    void everyCylinderUnderItsRealAddress()
    {
#ifndef UFT_CORPUS_DIR
        QSKIP("UFT_CORPUS_DIR not set");
#else
        QFile f(QString(UFT_CORPUS_DIR) + "/gw_fm_acorn_3trk.scp");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray orig = f.readAll();
        QVERIFY(orig.size() > 0x10 + 4 * 168);
        QCOMPARE(int(static_cast<unsigned char>(orig[0x0A])), 1);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const int fortlaufend[3] = { 0, 1, 2 };
        const int seite1[3] = { 1, 3, 5 };
        const QString pLeg = dir.filePath("legacy.scp");
        const QString pS1 = dir.filePath("side1.scp");
        QVERIFY(schreibe(pLeg, kopie(orig, fortlaufend, 1, 0, 2)));
        QVERIFY(schreibe(pS1, kopie(orig, seite1, 2, 1, 5)));

        UftOtdrPanel original, legacy, side1;
        QVERIFY(original.loadFluxImage(QString(UFT_CORPUS_DIR) + "/gw_fm_acorn_3trk.scp"));
        QVERIFY(legacy.loadFluxImage(pLeg));
        QVERIFY(side1.loadFluxImage(pS1));

        for (int c = 0; c < 3; c++) {
            const auto soll = original.trackFlux(c, 0);
            QVERIFY2(!soll.empty(), qPrintable(QString("original cylinder %1 empty").arg(c)));
            QVERIFY2(legacy.trackFlux(c, 0) == soll,
                     qPrintable(QString("legacy: cylinder %1 head 0 is not the original's").arg(c)));
            QVERIFY2(side1.trackFlux(c, 1) == soll,
                     qPrintable(QString("side 1, start 1: cylinder %1 head 1 is not the original's").arg(c)));
            QVERIFY2(side1.trackFlux(c, 0).empty(),
                     qPrintable(QString("side 1: cylinder %1 head 0 must be empty").arg(c)));
        }
#endif
    }
};

QTEST_MAIN(TestOtdrPanelScpAblage)
#include "test_otdr_panel_scp_ablage.moc"

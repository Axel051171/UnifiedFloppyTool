/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_forensic_tab_no_fiction.cpp
 * @brief The forensic tab does not call a structure validation "complete"
 *        while its own rows say the checks were not run (MF-1628, P3-714).
 *
 * ForensicTab::analyzeStructure() writes one result row per requested
 * check. Directory, FAT and filesystem validation are not wired, and since
 * MF-570 their rows say so ("— not checked"). The closing line, however,
 * read "Structure validation complete." — a summary that contradicts the
 * rows above it. Found by the measurement behind MF-1626.
 *
 * Asserted on a free corpus file with every structure check requested:
 *   (1) at least one row says "not checked" (the precondition — otherwise
 *       the test measures nothing);
 *   (2) the summary does not say "complete";
 *   (3) the summary names how many requested checks were not run.
 */
#include <QtTest/QtTest>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QTableWidget>

#include "forensictab.h"
#include "uft/uft_format_plugin.h"

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must point at tests/corpus_free"
#endif

class TestForensicTabNoFiction : public QObject
{
    Q_OBJECT

private:
    static QString alleZeilen(ForensicTab &tab)
    {
        QString s;
        for (auto *t : tab.findChildren<QTableWidget *>())
            for (int r = 0; r < t->rowCount(); r++)
                for (int c = 0; c < t->columnCount(); c++)
                    if (auto *it = t->item(r, c)) s += it->text() + "\t";
        return s;
    }

private slots:
    void initTestCase()
    {
        QCOMPARE(uft_register_all_formats(), UFT_OK);
    }

    void structureSummaryDoesNotClaimCompletion()
    {
        ForensicTab tab;
        for (const char *n : { "checkValidateStructure", "checkValidateBootblock",
                               "checkValidateDirectory", "checkValidateFAT",
                               "checkValidateFilesystem" }) {
            auto *c = tab.findChild<QCheckBox *>(QString::fromLatin1(n));
            QVERIFY2(c, n);
            c->setChecked(true);
        }
        tab.analyzeImage(QStringLiteral(UFT_CORPUS_DIR "/mtools_fat12_720k.img"));

        const QString zeilen = alleZeilen(tab);
        QVERIFY2(zeilen.contains(QStringLiteral("not checked")),
                 qPrintable("Vorbedingung: keine Zeile sagt 'not checked':\n" + zeilen));

        auto *details = tab.findChild<QPlainTextEdit *>(QStringLiteral("textDetails"));
        QVERIFY(details);
        const QString text = details->toPlainText();
        QVERIFY2(!text.contains(QStringLiteral("validation complete")), qPrintable(text));

        /* the count in the summary is the count of rows that say so */
        int nichtGeprueft = 0, gelaufen = 0;
        for (auto *t : tab.findChildren<QTableWidget *>())
            for (int r = 0; r < t->rowCount(); r++) {
                const QString st = t->item(r, 1) ? t->item(r, 1)->text() : QString();
                const QString chk = t->item(r, 0) ? t->item(r, 0)->text() : QString();
                if (st.contains(QStringLiteral("not checked"))) ++nichtGeprueft;
                if (chk == QStringLiteral("Boot Signature")) ++gelaufen;
            }
        const QString soll = QStringLiteral("%1 run, %2 requested but not run")
                                 .arg(gelaufen).arg(nichtGeprueft);
        QVERIFY2(text.contains(soll), qPrintable("erwartet '" + soll + "' in:\n" + text));
    }
};

QTEST_MAIN(TestForensicTabNoFiction)
#include "test_forensic_tab_no_fiction.moc"

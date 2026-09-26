/*
 * Issue #44 — "screen on master version is very big on height" (MF-1366).
 *
 * The main window is a QTabWidget, and a QStackedLayout's minimum size is
 * the MAXIMUM over all its pages. Two pages were each taller than a FullHD
 * screen on their own:
 *
 *   Workflow         forms/tab_workflow.ui grew with e60d3e7d
 *   Signal Analysis  UftOtdrPanel + FluxVisualizerWidget in a splitter
 *                    (MF-632); the OTDR widgets carry fixed minimum sizes
 *                    (FloppyOtdrWidget.h: trace 600x300, heatmap 400x200)
 *
 * Fixing one of them leaves the window just as tall, which is why this test
 * builds BOTH pages, and does it the way the application does:
 *
 *   - the chrome is Ui::MainWindow from forms/mainwindow.ui (menu, tool bar,
 *     LED row, tab bar, status bar) — the same setupUi() MainWindow calls;
 *   - the Workflow page is the real WorkflowTab, put into ui.tab_workflow
 *     with the zero-margin QVBoxLayout of MainWindow::loadTabWidgets();
 *   - the Signal page is the real UftOtdrPanel and FluxVisualizerWidget,
 *     laid out by uftBuildSignalPage() — the function MainWindow calls, so
 *     the test cannot drift from the application (MF-1177).
 *
 * NOT built here: Status, Hardware, Settings and Explorer. Each needs its
 * own large source set (four other Qt test targets link them); linking all
 * of them is linking the whole application. Measured once for #44 against
 * the real application objects (all six pages, after this fix): the window
 * needs 757 px offscreen, 840 px with Segoe UI 9pt and 922 px with 11pt,
 * and Explorer is then the tallest page (609 / 680 / 747 px). A page that
 * grows past the budget later is NOT caught by this test.
 *
 * Budget: FullHD is 1080 px. Measured on Windows 11 at 100 % scaling:
 * available geometry 1920 x 1032 (48 px task bar), title bar plus frame
 * 31 px, so at most 1001 px of client area. 41 px of that are kept as
 * reserve for font metrics that differ between this test's platform and a
 * desktop (at 3557ecfb the whole window measured 1296 px offscreen and
 * 1362 px with Segoe UI 9pt on Windows — 66 px apart). Linux desktop
 * fonts, window managers and DPI scaling were NOT measured.
 *
 * The fix is an interim one (owner decision 2026-09-26, "Scrollbereiche
 * jetzt"): both pages scroll. It contradicts the GUI rework plan's
 * "ganzer Bildschirm ohne Scrollen"; the height budget below stays valid
 * for whatever layout replaces it. What must NOT scroll is the start
 * button — a Workflow START reachable only after scrolling would be a new
 * defect, so its position is asserted separately.
 */
#include <QtTest>
#include <QMainWindow>
#include <QTabWidget>
#include <QScrollArea>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QSettings>
#include <QTemporaryDir>

#include "ui_mainwindow.h"
#include "workflowtab.h"
#include "uft_otdr_panel.h"
#include "widgets/fluxvisualizerwidget.h"
#include "gui/uft_signal_page.h"

static const int kFullHdClientBudget = 960;

namespace {

enum Pages { WorkflowPage = 1, SignalPage = 2, BothPages = 3 };

/* The main window as MainWindow builds it, restricted to the pages asked
 * for. The widgets are owned by the QMainWindow. */
struct Fenster {
    QMainWindow mw;
    Ui::MainWindow ui;

    explicit Fenster(int pages)
    {
        ui.setupUi(&mw);
        if (pages & WorkflowPage) {
            // Same three lines as MainWindow::loadTabWidgets(), Tab 1.
            WorkflowTab *workflowTab = new WorkflowTab();
            QVBoxLayout *layout1 = new QVBoxLayout(ui.tab_workflow);
            layout1->setContentsMargins(0, 0, 0, 0);
            layout1->addWidget(workflowTab);
        }
        if (pages & SignalPage) {
            UftOtdrPanel *otdr = new UftOtdrPanel();
            FluxVisualizerWidget *flux = new FluxVisualizerWidget();
            flux->setViewMode(FluxViewMode::WAVEFORM);
            uftBuildSignalPage(ui.tab_signal_analysis, otdr, flux);
        }
    }

    void protokoll(const char *wer) const
    {
        const QSize m = mw.minimumSizeHint();
        qInfo("%s: MainWindow minimumSizeHint %d x %d (budget height %d)",
              wer, m.width(), m.height(), kFullHdClientBudget);
        for (int i = 0; i < ui.tabWidget->count(); ++i) {
            const QSize s = ui.tabWidget->widget(i)->minimumSizeHint();
            qInfo("  page %d %-22s %5d x %5d", i,
                  qPrintable(ui.tabWidget->widget(i)->objectName()),
                  s.width(), s.height());
        }
    }
};

} // namespace

class TestMainWindowFitsFullHd : public QObject
{
    Q_OBJECT

    QTemporaryDir m_settingsDir;

private slots:
    void initTestCase()
    {
        // main.cpp draws with Fusion unless the user overrides it
        // (MF-1298); the metrics depend on it.
        QApplication::setStyle(QStringLiteral("Fusion"));

        // UftOtdrPanel reads AND writes a default-constructed QSettings in
        // its constructor. Keep that out of the user's registry.
        QVERIFY(m_settingsDir.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("UFT-test"));
        QCoreApplication::setApplicationName(
            QStringLiteral("test_main_window_fits_fullhd"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDir.path());
    }

    void workflowPageFits()
    {
        Fenster f(WorkflowPage);
        f.protokoll("Workflow only");
        const int h = f.mw.minimumSizeHint().height();
        QVERIFY2(h <= kFullHdClientBudget,
                 qPrintable(QStringLiteral("Workflow page: main window minimum "
                                           "height %1 > %2")
                                .arg(h).arg(kFullHdClientBudget)));
    }

    void signalPageFits()
    {
        Fenster f(SignalPage);
        f.protokoll("Signal only");
        const int h = f.mw.minimumSizeHint().height();
        QVERIFY2(h <= kFullHdClientBudget,
                 qPrintable(QStringLiteral("Signal Analysis page: main window "
                                           "minimum height %1 > %2")
                                .arg(h).arg(kFullHdClientBudget)));
    }

    void bothPagesFit()
    {
        Fenster f(BothPages);
        f.protokoll("Workflow + Signal");
        const int h = f.mw.minimumSizeHint().height();
        QVERIFY2(h <= kFullHdClientBudget,
                 qPrintable(QStringLiteral("main window minimum height %1 > %2")
                                .arg(h).arg(kFullHdClientBudget)));
    }

    /* START must stay reachable without scrolling: not inside any
     * QScrollArea, and — with the window at the budget height — inside the
     * visible client area. */
    void startButtonVisibleWithoutScrolling()
    {
        Fenster f(BothPages);
        f.ui.tabWidget->setCurrentWidget(f.ui.tab_workflow);
        f.mw.resize(1600, kFullHdClientBudget);
        f.mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&f.mw));

        QPushButton *start =
            f.mw.findChild<QPushButton *>(QStringLiteral("btnStartAbort"));
        QVERIFY(start);
        for (QWidget *w = start->parentWidget(); w; w = w->parentWidget())
            QVERIFY2(!qobject_cast<QScrollArea *>(w),
                     "btnStartAbort sits inside a QScrollArea");

        QVERIFY(start->isVisible());
        const QPoint top = start->mapTo(&f.mw, QPoint(0, 0));
        const int bottom = top.y() + start->height();
        qInfo("window %d x %d, btnStartAbort y %d..%d",
              f.mw.width(), f.mw.height(), top.y(), bottom);
        QVERIFY2(top.y() >= 0 && bottom <= kFullHdClientBudget,
                 qPrintable(QStringLiteral("btnStartAbort spans y %1..%2, "
                                           "budget %3")
                                .arg(top.y()).arg(bottom)
                                .arg(kFullHdClientBudget)));
    }

    /* The height must not have been won by removing or hiding content:
     * every group of the Workflow page is still there (owner rule MF-1077:
     * removing widgets is an owner decision, not a layout fix). */
    void workflowGroupsStillPresent()
    {
        Fenster f(WorkflowPage);
        const char *namen[] = {
            "groupSource", "groupDestination", "groupOperation",
            "groupModusAnzeige", "groupWeitereVorgaenge", "groupVorschau",
            "groupTracks", "groupProgress",
        };
        for (const char *n : namen) {
            QGroupBox *g = f.mw.findChild<QGroupBox *>(QLatin1String(n));
            QVERIFY2(g, n);
            QVERIFY2(!g->isHidden(), n);
        }
    }
};

QTEST_MAIN(TestMainWindowFitsFullHd)
#include "test_main_window_fits_fullhd.moc"

/*
 * The "Signal Analysis" tab page — see uft_signal_page.h (issue #44, MF-1366).
 */
#include "uft_signal_page.h"

#include <QFrame>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>

void uftBuildSignalPage(QWidget *page, QWidget *otdrPanel, QWidget *fluxView)
{
    // A null argument is a caller bug, not a normal case: say so instead of
    // returning silently with an empty page (review note on #44).
    if (!page || !otdrPanel || !fluxView) {
        qWarning("uftBuildSignalPage: null argument (page=%p otdr=%p flux=%p) "
                 "- Signal Analysis page left empty",
                 static_cast<void *>(page), static_cast<void *>(otdrPanel),
                 static_cast<void *>(fluxView));
        return;
    }

    // Issue #44, INTERIM until the GUI rework (owner decision 2026-09-26,
    // "Scrollbereiche jetzt"). Without it this page needs 946 px (offscreen)
    // to 1002 px (Segoe UI 11pt) of height — measured against the real
    // application at 3557ecfb — because the OTDR widgets carry fixed
    // minimum sizes (FloppyOtdrWidget.h: trace 600x300, heatmap 400x200).
    // A QTabWidget is as tall as its TALLEST page, so this page alone kept
    // the main window above a FullHD screen.
    //
    // The minimum sizes stay as they are. Only the OTDR panel scrolls, and
    // it scrolls INSIDE the splitter: the flux view below stays on screen
    // next to it, which is the reason the two share a splitter in the first
    // place (MF-632, see below). Wrapping the whole splitter instead would
    // pin the flux view to its 120 px minimum below the fold on FullHD.
    //
    // This contradicts test-gui/PLAN_gui-umbau.md ("ganzer Bildschirm ohne
    // Scrollen"). Whatever replaces it must keep
    // tests/test_main_window_fits_fullhd.cpp green.
    QScrollArea *otdrScroll = new QScrollArea;
    otdrScroll->setObjectName(QStringLiteral("scrollOtdrPanel"));
    otdrScroll->setWidgetResizable(true);
    otdrScroll->setFrameShape(QFrame::NoFrame);
    otdrScroll->setWidget(otdrPanel);

    // MF-632: the flux view is a second view of the data the OTDR panel
    // already holds. A splitter rather than a sub-tab, because choosing the
    // window for the CRC oracle needs both at once — quality above, time
    // domain below. 60/40 in favour of the OTDR panel, which carries the
    // controls.
    // CORRECTED #44: the stretch factors below still say 6:4, but with the
    // scroll area the effective split is about 55/45 — QScrollArea caps its
    // sizeHint, so the factors only share out the space left over. Measured
    // on native Windows, 9pt: window 1920x1001 -> OTDR viewport 456-458 px,
    // flux view 380-381 px; 1920x960 -> 435 / 363.
    fluxView->setMinimumHeight(120);

    QSplitter *signalSplit = new QSplitter(Qt::Vertical, page);
    signalSplit->addWidget(otdrScroll);
    signalSplit->addWidget(fluxView);
    signalSplit->setStretchFactor(0, 6);
    signalSplit->setStretchFactor(1, 4);

    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(signalSplit);
}

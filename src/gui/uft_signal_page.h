#ifndef UFT_SIGNAL_PAGE_H
#define UFT_SIGNAL_PAGE_H

/*
 * The "Signal Analysis" tab page, built in ONE place (issue #44, MF-1366).
 *
 * MainWindow::loadTabWidgets() used to assemble this page inline: the OTDR
 * panel above, the flux view below, in a vertical splitter. The regression
 * test for #44 (tests/test_main_window_fits_fullhd.cpp) has to measure the
 * page exactly as the application builds it; a copy of these lines in the
 * test would be a second rule for the same thing that drifts silently
 * (MF-1177). So both call this function.
 *
 * The function only arranges widgets. It takes QWidget pointers so it does
 * not depend on UftOtdrPanel or FluxVisualizerWidget; the caller keeps its
 * class-specific setup (view mode, signal connections).
 *
 * The OTDR panel sits in a QScrollArea inside the splitter — an interim
 * measure for #44 that contradicts the GUI rework plan; the reason is in
 * uft_signal_page.cpp.
 */

class QWidget;

/**
 * @brief Lay out the Signal Analysis page.
 *
 * @param page       the tab page (ui->tab_signal_analysis); receives a
 *                   zero-margin QVBoxLayout holding the splitter. Must not
 *                   have a layout yet.
 * @param otdrPanel  upper part of the splitter (stretch 6), wrapped in a
 *                   frameless, resizable QScrollArea
 * @param fluxView   lower part of the splitter (stretch 4, min height 120)
 */
void uftBuildSignalPage(QWidget *page, QWidget *otdrPanel, QWidget *fluxView);

#endif /* UFT_SIGNAL_PAGE_H */

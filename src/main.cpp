#include "uft/uft_version.h"
/**
 * @file main.cpp
 * @brief UnifiedFloppyTool - Qt Designer Edition
 * 
 * Main entry point for GUI application.
 * All UI is defined in .ui files for Qt Designer editing.
 */

#include <QApplication>
#include <QMessageBox>
#include "mainwindow.h"

#include "uft/uft_format_plugin.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Application metadata
    app.setApplicationName("UnifiedFloppyTool");
    app.setApplicationVersion(UFT_VERSION_STRING);
    app.setOrganizationName("UFT Project");

    /* Register the format plugins before anything can ask for one (MF-447).
     *
     * This call is new, and its absence is the reason a chain of registry bugs
     * (ARCH-9 … ARCH-11) survived the whole life of the project: with an empty
     * registry every lookup answers NULL, so nothing that depended on it could
     * ever be seen to be wrong. uft_disk_open() returned NULL for every file,
     * and DiskAnalyzerWindow::loadImage() therefore always took its fallback
     * branch — geometry estimated from the file size, 512-byte sectors, two
     * sides, 18 sectors per track, for a D64 as readily as for an ADF.
     *
     * Registration is pure bookkeeping: all 137 plugins have .init == NULL, so
     * this costs a loop and no I/O.
     *
     * A short count is a real problem and is shown rather than logged: the
     * formats that did not make it are silently absent from every open dialog,
     * and "this tool cannot read that disk" is exactly the wrong thing to be
     * quiet about. */
    {
        const uft_error_t rc = uft_register_all_formats();
        const size_t registered = uft_registered_format_plugin_count();
        const size_t available  = uft_get_format_count();
        if (rc != UFT_OK || registered != available) {
            QMessageBox::warning(
                nullptr,
                QObject::tr("Format-Plugins"),
                QObject::tr("Nur %1 von %2 Format-Plugins konnten registriert "
                            "werden (Fehlercode %3).\n\n"
                            "Die fehlenden Formate stehen in dieser Sitzung "
                            "nicht zur Verfügung — Dateien dieser Typen werden "
                            "als unbekannt gemeldet, nicht etwa geraten.")
                    .arg(registered).arg(available).arg(static_cast<int>(rc)));
        }
    }
    
    // Create and show main window
    MainWindow window;
    window.show();
    
    return app.exec();
}

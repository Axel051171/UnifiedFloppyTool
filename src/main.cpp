#include "uft/uft_version.h"
/**
 * @file main.cpp
 * @brief UnifiedFloppyTool - Qt Designer Edition
 * 
 * Main entry point for GUI application.
 * All UI is defined in .ui files for Qt Designer editing.
 */

#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Application metadata
    app.setApplicationName("UnifiedFloppyTool");
    app.setApplicationVersion(UFT_VERSION_STRING);
    app.setOrganizationName("UFT Project");
    
    // Create and show main window
    MainWindow window;
    window.show();
    
    return app.exec();
}

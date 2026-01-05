/**
 * @file main.cpp
 * @brief UFT GUI Application Entry Point
 */

#include <QApplication>
#include <QStyleFactory>
#include <QFont>
#include <QFile>
#include <QTextStream>
#include "uft_main_window.h"

int main(int argc, char *argv[])
{
    /* Create application */
    QApplication app(argc, argv);
    
    /* Application info */
    app.setApplicationName("UnifiedFloppyTool");
    app.setApplicationVersion("5.32.0");
    app.setOrganizationName("UFT");
    app.setOrganizationDomain("uft.local");
    
    /* Use Fusion style for consistent look across platforms */
    app.setStyle(QStyleFactory::create("Fusion"));
    
    /* Set default font */
    QFont defaultFont("Segoe UI", 9);
    if (!defaultFont.exactMatch()) {
        defaultFont = QFont("Arial", 9);
    }
    app.setFont(defaultFont);
    
    /* Light palette */
    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, QColor(245, 245, 245));
    lightPalette.setColor(QPalette::WindowText, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::Base, QColor(255, 255, 255));
    lightPalette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
    lightPalette.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
    lightPalette.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::Text, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
    lightPalette.setColor(QPalette::ButtonText, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::BrightText, QColor(255, 0, 0));
    lightPalette.setColor(QPalette::Link, QColor(25, 118, 210));
    lightPalette.setColor(QPalette::Highlight, QColor(25, 118, 210));
    lightPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    
    app.setPalette(lightPalette);
    
    /* Create and show main window */
    UftMainWindow mainWindow;
    mainWindow.show();
    
    /* Process command line arguments */
    QStringList args = app.arguments();
    if (args.size() > 1) {
        /* Open file passed as argument */
        mainWindow.openImage(args.at(1));
    }
    
    return app.exec();
}

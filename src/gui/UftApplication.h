/**
 * @file UftApplication.h
 * @brief Application Singleton - Zentrale Ressourcenverwaltung
 * 
 * P2-003: GUI Konsolidierung
 * 
 * Verwaltet:
 * - MainController
 * - Settings
 * - Recent Files
 * - Logging
 * - Theme/Style
 */

#ifndef UFT_APPLICATION_H
#define UFT_APPLICATION_H

#include <QObject>
#include <QApplication>
#include <QSettings>
#include <QStringList>
#include "uft/uft_version.h"

class UftMainController;

/**
 * @brief Application singleton for global resources
 */
class UftApplication : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool darkMode READ isDarkMode WRITE setDarkMode NOTIFY themeChanged)

public:
    static UftApplication* instance();
    
    /* Version info */
    QString version() const { return UFT_VERSION_STRING; }
    QString buildDate() const { return __DATE__; }
    
    /* Main controller */
    UftMainController* controller() const { return m_controller; }
    
    /* Settings */
    QSettings* settings() const { return m_settings; }
    
    void saveSetting(const QString &key, const QVariant &value);
    QVariant loadSetting(const QString &key, const QVariant &defaultValue = QVariant());
    
    /* Recent files */
    QStringList recentFiles() const { return m_recentFiles; }
    void addRecentFile(const QString &path);
    void clearRecentFiles();
    
    /* Theme */
    bool isDarkMode() const { return m_darkMode; }
    void setDarkMode(bool dark);
    void applyTheme();
    
    /* Logging */
    void log(const QString &message);
    void logError(const QString &error);
    QString logFilePath() const;
    
signals:
    void themeChanged();
    void recentFilesChanged();
    void logMessage(const QString &message);

public slots:
    void saveSettings();
    void loadSettings();

private:
    explicit UftApplication(QObject *parent = nullptr);
    ~UftApplication();
    
    static UftApplication *s_instance;
    
    UftMainController *m_controller = nullptr;
    QSettings *m_settings = nullptr;
    QStringList m_recentFiles;
    bool m_darkMode = false;
    
    void initController();
    void initSettings();
    void updateRecentFilesMenu();
};

/* Convenience macro */
#define uftApp UftApplication::instance()

/* ═══════════════════════════════════════════════════════════════════════════════
 * Theme Stylesheets
 * ═══════════════════════════════════════════════════════════════════════════════ */

namespace UftTheme {

/**
 * @brief Dark mode stylesheet
 */
inline QString darkStyleSheet() {
    return R"(
        QMainWindow, QDialog {
            background-color: #2b2b2b;
            color: #e0e0e0;
        }
        QTabWidget::pane {
            border: 1px solid #3d3d3d;
            background: #2b2b2b;
        }
        QTabBar::tab {
            background: #3d3d3d;
            color: #e0e0e0;
            padding: 8px 16px;
            border: 1px solid #4d4d4d;
            border-bottom: none;
        }
        QTabBar::tab:selected {
            background: #4a90d9;
            color: white;
        }
        QGroupBox {
            border: 1px solid #4d4d4d;
            margin-top: 12px;
            padding-top: 8px;
        }
        QGroupBox::title {
            color: #8ab4f8;
            subcontrol-origin: margin;
            left: 10px;
        }
        QPushButton {
            background: #4d4d4d;
            color: #e0e0e0;
            border: 1px solid #5d5d5d;
            padding: 6px 16px;
            border-radius: 4px;
        }
        QPushButton:hover {
            background: #5d5d5d;
        }
        QPushButton:pressed {
            background: #3d3d3d;
        }
        QPushButton:disabled {
            background: #3d3d3d;
            color: #808080;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background: #3d3d3d;
            color: #e0e0e0;
            border: 1px solid #5d5d5d;
            padding: 4px;
            border-radius: 3px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border-color: #4a90d9;
        }
        QTableView, QTreeView, QListView {
            background: #2b2b2b;
            alternate-background-color: #323232;
            color: #e0e0e0;
            border: 1px solid #4d4d4d;
        }
        QTableView::item:selected {
            background: #4a90d9;
        }
        QHeaderView::section {
            background: #3d3d3d;
            color: #e0e0e0;
            padding: 4px;
            border: 1px solid #4d4d4d;
        }
        QProgressBar {
            border: 1px solid #4d4d4d;
            border-radius: 3px;
            background: #2b2b2b;
            text-align: center;
        }
        QProgressBar::chunk {
            background: #4a90d9;
            border-radius: 2px;
        }
        QScrollBar:vertical {
            background: #2b2b2b;
            width: 12px;
        }
        QScrollBar::handle:vertical {
            background: #5d5d5d;
            border-radius: 5px;
            min-height: 20px;
        }
        QStatusBar {
            background: #252525;
            color: #b0b0b0;
        }
        QMenuBar {
            background: #2b2b2b;
            color: #e0e0e0;
        }
        QMenuBar::item:selected {
            background: #4a90d9;
        }
        QMenu {
            background: #3d3d3d;
            color: #e0e0e0;
            border: 1px solid #4d4d4d;
        }
        QMenu::item:selected {
            background: #4a90d9;
        }
        QToolTip {
            background: #4d4d4d;
            color: #e0e0e0;
            border: 1px solid #5d5d5d;
        }
    )";
}

/**
 * @brief Light mode (default Qt style with minor tweaks)
 */
inline QString lightStyleSheet() {
    return R"(
        QGroupBox {
            border: 1px solid #c0c0c0;
            margin-top: 12px;
            padding-top: 8px;
        }
        QGroupBox::title {
            color: #2196F3;
            subcontrol-origin: margin;
            left: 10px;
        }
        QPushButton {
            padding: 6px 16px;
            border-radius: 4px;
        }
        QProgressBar {
            border-radius: 3px;
        }
        QProgressBar::chunk {
            background: #4CAF50;
            border-radius: 2px;
        }
    )";
}

} /* namespace UftTheme */

#endif /* UFT_APPLICATION_H */

/**
 * @file mainwindow.h
 * @brief Main Window - Uses Qt Designer .ui file
 */

#pragma once

#include <QMainWindow>
#include <QStringList>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QDragEnterEvent;
class QDropEvent;
QT_END_NAMESPACE

class VisualDiskWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    enum class LEDStatus { Disconnected, Connected, Busy, Error };
    void setLEDStatus(LEDStatus status);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    // File menu
    void onOpen();
    void onSave();
    void onSaveAs();
    
    // Settings menu
    void onDarkModeToggled(bool enabled);
    void onPreferences();
    
    // Help menu
    void onHelp();
    void onAbout();
    void onKeyboardShortcuts();

private:
    void loadTabWidgets();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void openFile(const QString &filename);
    void updateRecentFilesMenu();
    void applyDarkMode(bool enabled);
    
    Ui::MainWindow *ui;
    VisualDiskWindow *m_visualDiskWindow;
    
    QString m_currentFile;
    QStringList m_recentFiles;
    bool m_darkMode;
};

/**
 * @file mainwindow.h
 * @brief Main Window with file state tracking and format auto-detection
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
class FormatTab;
class WorkflowTab;
class StatusTab;

/**
 * @brief Information about the currently loaded file
 */
struct LoadedFileInfo {
    QString filePath;
    QString fileName;
    QString detectedSystem;      // e.g. "Commodore", "Amiga"
    QString detectedFormat;      // e.g. "D64", "ADF"
    qint64 fileSize;
    bool isLoaded;
    bool isModified;
    
    void clear() {
        filePath.clear();
        fileName.clear();
        detectedSystem.clear();
        detectedFormat.clear();
        fileSize = 0;
        isLoaded = false;
        isModified = false;
    }
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    enum class LEDStatus { Disconnected, Connected, Busy, Error };
    void setLEDStatus(LEDStatus status);
    
    // File state access
    bool hasLoadedFile() const { return m_loadedFile.isLoaded; }
    const LoadedFileInfo& loadedFileInfo() const { return m_loadedFile; }

signals:
    void fileLoaded(const LoadedFileInfo &info);
    void fileUnloaded();
    void formatAutoDetected(const QString &system, const QString &format);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

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
    
    // Format change handling
    void onOutputFormatChanged(const QString &system, const QString &format);

private:
    void loadTabWidgets();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void openFile(const QString &filename);
    void updateRecentFilesMenu();
    void applyDarkMode(bool enabled);
    
    // Format auto-detection
    bool autoDetectFormat(const QString &filename);
    QString detectSystemFromExtension(const QString &ext);
    QString detectFormatFromFile(const QString &filename, qint64 fileSize);
    
    // File state management
    void setFileLoaded(const QString &filename, const QString &system, const QString &format);
    void clearLoadedFile();
    bool confirmFormatChange(const QString &newSystem, const QString &newFormat);
    
    Ui::MainWindow *ui;
    VisualDiskWindow *m_visualDiskWindow;
    
    // Tab references for cross-communication
    FormatTab *m_formatTab;
    WorkflowTab *m_workflowTab;
    StatusTab *m_statusTab;
    
    // File state
    LoadedFileInfo m_loadedFile;
    QString m_currentFile;
    QStringList m_recentFiles;
    bool m_darkMode;
};

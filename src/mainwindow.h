/**
 * @file mainwindow.h
 * @brief Main Window - Uses Qt Designer .ui file
 * 
 * P1-2: StatusTab connection to DecodeJob
 */

#pragma once

#include <QMainWindow>
#include <QStringList>
#include "disk_image_validator.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QDragEnterEvent;
class QDropEvent;
class QThread;
QT_END_NAMESPACE

class VisualDiskWindow;
class StatusTab;
class DecodeJob;
class UftOtdrPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    enum class LEDStatus { Disconnected, Connected, Busy, Error };
    void setLEDStatus(LEDStatus status);
    
    // Access to current image info
    const DiskImageInfo& currentImageInfo() const { return m_currentImageInfo; }
    const QString& currentFile() const { return m_currentFile; }

signals:
    /**
     * @brief Emitted when an image file is loaded
     */
    void imageLoaded(const QString& filename, const DiskImageInfo& info);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    // File menu
    void onOpen();
    void onSave();
    void onSaveAs();
    
    // Decode operations
    void startDecode(const QString& path);
    void onDecodeProgress(int percentage);
    void onDecodeFinished(const QString& message);
    void onDecodeError(const QString& error);
    
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
    
    // Tab references for signal connections
    StatusTab* m_statusTab = nullptr;
    UftOtdrPanel* m_otdrPanel = nullptr;
    
    // Decode thread
    QThread* m_decodeThread = nullptr;
    DecodeJob* m_decodeJob = nullptr;
    
    QString m_currentFile;
    QStringList m_recentFiles;
    bool m_darkMode;
    DiskImageInfo m_currentImageInfo;
};

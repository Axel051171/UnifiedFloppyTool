/**
 * @file mainwindow.h
 * @brief UnifiedFloppyTool - Main Window Header
 * @version 3.1.4.010
 * 
 * Architecture:
 * - MVVM Pattern: View (MainWindow) ↔ ViewModel (UFTController) ↔ Model (C Core)
 * - Async Operations: QThread Worker für non-blocking UI
 * - Signal/Slot: Modern functor-based connections
 */

#ifndef UFT_MAINWINDOW_H
#define UFT_MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QSettings>
#include <QProgressBar>
#include <QLabel>
#include <QTimer>
#include <QAction>

// Forward declarations
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class UFTWorker;
class UFTController;
class TrackGridWidget;
class FluxVisualizerWidget;
class SettingsDialog;
enum class Theme;

/*============================================================================
 * WORKER THREAD - Runs C Core Operations
 *============================================================================*/

/**
 * @brief Worker für asynchrone Operationen
 * 
 * Führt rechenintensive C-Operationen in separatem Thread aus.
 * Kommuniziert über Signals mit dem UI Thread.
 */
class UFTWorker : public QObject
{
    Q_OBJECT

public:
    enum class Operation {
        None,
        ReadDisk,
        WriteDisk,
        AnalyzeTrack,
        ConvertFormat,
        ForensicImage,
        ErrorCorrection
    };
    Q_ENUM(Operation)

    explicit UFTWorker(QObject* parent = nullptr);
    ~UFTWorker() override;

    void setOperation(Operation op) { m_operation = op; }
    void setSourcePath(const QString& path) { m_sourcePath = path; }
    void setDestPath(const QString& path) { m_destPath = path; }
    
    bool isRunning() const { return m_running; }
    void requestCancel() { m_cancelRequested = true; }

public slots:
    void process();

signals:
    void started();
    void progress(int percent, const QString& status);
    void trackCompleted(int track, int head, int goodSectors, int badSectors);
    void sectorStatus(int track, int head, int sector, int status);
    void logMessage(const QString& message, int level);
    void finished(bool success, const QString& result);
    void error(const QString& message);

private:
    Operation m_operation = Operation::None;
    QString m_sourcePath;
    QString m_destPath;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cancelRequested{false};
    
    void processReadDisk();
    void processWriteDisk();
    void processAnalyzeTrack();
    void processForensicImage();
};

/*============================================================================
 * CONTROLLER - ViewModel Layer
 *============================================================================*/

/**
 * @brief Controller/ViewModel für UFT
 * 
 * Verbindet UI mit C-Core, verwaltet State und Konfiguration.
 */
class UFTController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit UFTController(QObject* parent = nullptr);
    ~UFTController() override;

    bool isBusy() const { return m_busy; }
    QString status() const { return m_status; }

    // Settings Access
    void loadSettings();
    void saveSettings();
    void loadPreset(int presetId);

    // Operations
    void startReadDisk(const QString& source, const QString& dest);
    void startWriteDisk(const QString& source, const QString& dest);
    void startForensicImage(const QString& source, const QString& dest);
    void cancelOperation();

signals:
    void busyChanged(bool busy);
    void statusChanged(const QString& status);
    void progressUpdated(int percent, const QString& message);
    void operationFinished(bool success, const QString& result);
    void trackStatusUpdated(int track, int head, int goodSectors, int badSectors);
    void sectorStatusUpdated(int track, int head, int sector, int status);
    void logAppended(const QString& message, int level);

private slots:
    void onWorkerStarted();
    void onWorkerProgress(int percent, const QString& status);
    void onWorkerFinished(bool success, const QString& result);
    void onWorkerError(const QString& message);

private:
    QScopedPointer<QThread> m_workerThread;
    QScopedPointer<UFTWorker> m_worker;
    bool m_busy = false;
    QString m_status;
    
    void setBusy(bool busy);
    void setStatus(const QString& status);
};

/*============================================================================
 * MAIN WINDOW - View Layer
 *============================================================================*/

/**
 * @brief UFT Hauptfenster
 * 
 * Features:
 * - 6-Tab Interface (Simple, Processing, PLL, Forensic, Flux, Geometry)
 * - Track Grid Visualization
 * - Real-time Progress
 * - Dark Mode UI
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    // File Operations
    void onOpenFile();
    void onSaveFile();
    void onExportAs();
    
    // Processing
    void onStartClicked();
    void onStopClicked();
    void onPresetChanged(int index);
    void onPlatformChanged(int index);
    
    // Settings
    void onProcessingTypeChanged(int index);
    void onAdaptiveParamsChanged();
    void onDPLLParamsChanged();
    void onForensicParamsChanged();
    void onGeometryChanged();
    
    // Controller Events
    void onProgressUpdated(int percent, const QString& message);
    void onOperationFinished(bool success, const QString& result);
    void onTrackStatusUpdated(int track, int head, int goodSectors, int badSectors);
    void onSectorStatusUpdated(int track, int head, int sector, int status);
    void onLogAppended(const QString& message, int level);
    
    // UI Helpers
    void updateStatusBar();
    void showAboutDialog();
    void showSettingsDialog();
    void onThemeChanged(Theme theme);
    void setDarkMode();
    void setLightMode();
    void setAutoMode();

private:
    QScopedPointer<Ui::MainWindow> ui;
    QScopedPointer<UFTController> m_controller;
    
    // Status Bar Widgets
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_busyIndicator = nullptr;
    
    // Custom Widgets
    TrackGridWidget* m_trackGrid = nullptr;
    FluxVisualizerWidget* m_fluxVis = nullptr;
    
    // Timers
    QTimer* m_statusTimer = nullptr;
    
    // Theme Actions
    QAction* m_actionDarkMode = nullptr;
    QAction* m_actionLightMode = nullptr;
    QAction* m_actionAutoMode = nullptr;
    
    // Setup Methods
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupConnections();
    void setupShortcuts();
    
    // Tab Setup
    void setupSimpleTab();
    void setupProcessingTab();
    void setupPLLTab();
    void setupForensicTab();
    void setupFluxTab();
    void setupGeometryTab();
    
    // Helpers
    void loadStyleSheet();
    void updateAllControls();
    void updateCalculatedFields();
    bool validateSettings();
    void saveWindowState();
    void restoreWindowState();
    
    // Settings Sync
    void syncUIToSettings();
    void syncSettingsToUI();
};

#endif // UFT_MAINWINDOW_H

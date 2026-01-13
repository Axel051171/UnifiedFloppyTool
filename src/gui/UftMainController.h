/**
 * @file UftMainController.h
 * @brief Central GUI Controller - Koordiniert alle GUI-Module
 * 
 * P2-003: GUI Konsolidierung
 * 
 * Der UftMainController verbindet:
 * - UftParameterModel (Bidirektionale Parameter)
 * - UftFormatDetectionModel (Format Auto-Detection)
 * - Protection Pipeline
 * - Writer Backend
 * - Hardware Abstraction
 * 
 * Architektur: MVVM-ähnlich
 *   View (Qt Widgets/UI) <-> Controller <-> Models <-> Core (C Backend)
 */

#ifndef UFT_MAIN_CONTROLLER_H
#define UFT_MAIN_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QThread>
#include <QMutex>

/* Forward declarations */
class UftParameterModel;
class UftFormatDetectionModel;
class UftWidgetBinder;
class QProgressBar;
class QLabel;

/**
 * @brief Operation types for background tasks
 */
enum class UftOperation {
    None,
    Read,
    Write,
    Verify,
    Analyze,
    Convert,
    Format,
    Compare
};

/**
 * @brief Task status
 */
struct UftTaskStatus {
    UftOperation operation = UftOperation::None;
    bool running = false;
    bool cancelled = false;
    int progress = 0;           /* 0-100 */
    int currentTrack = 0;
    int totalTracks = 0;
    int errorsFound = 0;
    QString statusMessage;
    QString lastError;
    double elapsedSeconds = 0;
    double estimatedRemaining = 0;
};

/**
 * @brief Central controller coordinating all GUI modules
 */
class UftMainController : public QObject
{
    Q_OBJECT
    
    /* Status properties */
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY currentFileChanged)

public:
    explicit UftMainController(QObject *parent = nullptr);
    ~UftMainController();
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * Module Access
     * ═══════════════════════════════════════════════════════════════════════════ */
    
    UftParameterModel* parameterModel() const { return m_paramModel; }
    UftFormatDetectionModel* formatDetectionModel() const { return m_formatModel; }
    UftWidgetBinder* widgetBinder() const { return m_binder; }
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * Status Properties
     * ═══════════════════════════════════════════════════════════════════════════ */
    
    bool isBusy() const { return m_status.running; }
    int progress() const { return m_status.progress; }
    QString statusText() const { return m_status.statusMessage; }
    QString currentFile() const { return m_currentFile; }
    UftTaskStatus taskStatus() const { return m_status; }
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * File Operations
     * ═══════════════════════════════════════════════════════════════════════════ */
    
    Q_INVOKABLE bool openFile(const QString &path);
    Q_INVOKABLE bool saveFile(const QString &path);
    Q_INVOKABLE bool closeFile();
    
    Q_INVOKABLE QString suggestOutputPath(const QString &inputPath, 
                                           const QString &targetFormat);
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * Disk Operations
     * ═══════════════════════════════════════════════════════════════════════════ */
    
public slots:
    /**
     * @brief Start disk read operation
     */
    void startRead();
    
    /**
     * @brief Start disk write operation
     */
    void startWrite();
    
    /**
     * @brief Start verify operation
     */
    void startVerify();
    
    /**
     * @brief Start format conversion
     */
    void startConvert(const QString &targetFormat);
    
    /**
     * @brief Start format analysis
     */
    void startAnalyze();
    
    /**
     * @brief Cancel current operation
     */
    void cancelOperation();
    
    /**
     * @brief Apply detected format to parameters
     */
    void applyDetectedFormat();
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * Hardware Operations
     * ═══════════════════════════════════════════════════════════════════════════ */
    
    /**
     * @brief Refresh available hardware list
     */
    void refreshHardware();
    
    /**
     * @brief Get available hardware devices
     */
    QStringList availableDevices() const { return m_availableDevices; }
    
    /**
     * @brief Select hardware device
     */
    void selectDevice(const QString &device);
    
signals:
    /* Status signals */
    void busyChanged(bool busy);
    void progressChanged(int progress);
    void statusChanged(const QString &message);
    void currentFileChanged(const QString &path);
    
    /* Operation signals */
    void operationStarted(UftOperation op);
    void operationProgress(int track, int total, const QString &message);
    void operationCompleted(UftOperation op, bool success);
    void operationCancelled(UftOperation op);
    void errorOccurred(const QString &error);
    
    /* Data signals */
    void fileOpened(const QString &path);
    void fileClosed();
    void formatDetected(const QString &format, int confidence);
    void hardwareListChanged();
    
    /* Track-level signals for visualization */
    void trackRead(int cylinder, int head, int sectors, int errors);
    void trackWritten(int cylinder, int head, bool verified);
    void sectorError(int cylinder, int head, int sector, const QString &error);

private slots:
    void onParametersChanged();
    void onFormatDetected(const QString &formatId, const QString &formatName, int confidence);
    void onWorkerProgress(int current, int total, const QString &msg);
    void onWorkerFinished(bool success, const QString &message);

private:
    void initModels();
    void connectSignals();
    void updateStatus(const QString &message, int progress = -1);
    void setRunning(bool running);
    
    /* Models */
    UftParameterModel *m_paramModel = nullptr;
    UftFormatDetectionModel *m_formatModel = nullptr;
    UftWidgetBinder *m_binder = nullptr;
    
    /* State */
    UftTaskStatus m_status;
    QString m_currentFile;
    QStringList m_availableDevices;
    
    /* Threading */
    QThread *m_workerThread = nullptr;
    QMutex m_mutex;
    
    /* Backend pointer (wenn geladen) */
    void *m_backendHandle = nullptr;
};

/**
 * @brief Worker class for background operations
 */
class UftOperationWorker : public QObject
{
    Q_OBJECT

public:
    explicit UftOperationWorker(UftMainController *controller);
    
    void setOperation(UftOperation op) { m_operation = op; }
    void setParameters(const QVariantMap &params) { m_params = params; }

public slots:
    void process();
    void cancel();

signals:
    void progress(int current, int total, const QString &message);
    void finished(bool success, const QString &message);
    void trackProcessed(int cylinder, int head, int errors);

private:
    UftMainController *m_controller;
    UftOperation m_operation = UftOperation::None;
    QVariantMap m_params;
    bool m_cancelled = false;
};

#endif /* UFT_MAIN_CONTROLLER_H */

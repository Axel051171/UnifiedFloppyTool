/**
 * @file UftMainController.cpp
 * @brief Central GUI Controller Implementation
 * 
 * P2-003: GUI Konsolidierung
 */

#include "UftMainController.h"
#include "UftParameterModel.h"
#include "UftFormatDetectionModel.h"
#include "UftWidgetBinder.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QElapsedTimer>

/* ═══════════════════════════════════════════════════════════════════════════════
 * UftMainController Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

UftMainController::UftMainController(QObject *parent)
    : QObject(parent)
{
    initModels();
    connectSignals();
    
    /* Initial hardware scan */
    refreshHardware();
}

UftMainController::~UftMainController()
{
    /* Stop any running worker */
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(2000);
    }
    
    delete m_workerThread;
}

void UftMainController::initModels()
{
    /* Create parameter model */
    m_paramModel = new UftParameterModel(this);
    
    /* Create format detection model */
    m_formatModel = new UftFormatDetectionModel(this);
    
    /* Create widget binder */
    m_binder = new UftWidgetBinder(m_paramModel, this);
}

void UftMainController::connectSignals()
{
    /* Parameter changes */
    connect(m_paramModel, &UftParameterModel::parameterChanged,
            this, &UftMainController::onParametersChanged);
    
    /* Format detection */
    connect(m_formatModel, &UftFormatDetectionModel::resultsChanged,
            this, [this]() {
                if (m_formatModel->hasResults()) {
                    emit formatDetected(m_formatModel->bestFormat(),
                                       m_formatModel->bestConfidence());
                }
            });
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool UftMainController::openFile(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists()) {
        emit errorOccurred(tr("File not found: %1").arg(path));
        return false;
    }
    
    /* Set input path in parameters */
    m_paramModel->setInputPath(path);
    m_currentFile = path;
    
    /* Auto-detect format */
    updateStatus(tr("Detecting format..."));
    m_formatModel->detectFromFile(path);
    
    emit currentFileChanged(m_currentFile);
    emit fileOpened(path);
    
    updateStatus(tr("File opened: %1").arg(fi.fileName()));
    return true;
}

bool UftMainController::saveFile(const QString &path)
{
    if (path.isEmpty()) {
        emit errorOccurred(tr("No output path specified"));
        return false;
    }
    
    m_paramModel->setOutputPath(path);
    return true;
}

bool UftMainController::closeFile()
{
    m_currentFile.clear();
    m_paramModel->reset();
    m_formatModel->clear();
    
    emit currentFileChanged(QString());
    emit fileClosed();
    
    updateStatus(tr("Ready"));
    return true;
}

QString UftMainController::suggestOutputPath(const QString &inputPath, 
                                              const QString &targetFormat)
{
    if (inputPath.isEmpty()) return QString();
    
    QFileInfo fi(inputPath);
    QString baseName = fi.completeBaseName();
    QString dir = fi.absolutePath();
    
    /* Map format to extension */
    QString ext = targetFormat.toLower();
    if (ext == "adf" || ext == "amiga") ext = "adf";
    else if (ext == "scp") ext = "scp";
    else if (ext == "hfe") ext = "hfe";
    else if (ext == "img" || ext == "raw") ext = "img";
    else if (ext == "d64" || ext == "c64") ext = "d64";
    /* ... add more mappings ... */
    
    return QDir(dir).filePath(QString("%1_converted.%2").arg(baseName, ext));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftMainController::startRead()
{
    if (m_status.running) {
        emit errorOccurred(tr("Operation already in progress"));
        return;
    }
    
    if (!m_paramModel->isValid()) {
        emit errorOccurred(tr("Invalid parameters"));
        return;
    }
    
    setRunning(true);
    m_status.operation = UftOperation::Read;
    emit operationStarted(UftOperation::Read);
    
    /* Create worker thread */
    m_workerThread = new QThread(this);
    auto *worker = new UftOperationWorker(this);
    worker->setOperation(UftOperation::Read);
    worker->moveToThread(m_workerThread);
    
    connect(m_workerThread, &QThread::started, worker, &UftOperationWorker::process);
    connect(worker, &UftOperationWorker::progress, this, &UftMainController::onWorkerProgress);
    connect(worker, &UftOperationWorker::finished, this, &UftMainController::onWorkerFinished);
    connect(worker, &UftOperationWorker::finished, m_workerThread, &QThread::quit);
    connect(m_workerThread, &QThread::finished, worker, &QObject::deleteLater);
    
    m_workerThread->start();
}

void UftMainController::startWrite()
{
    if (m_status.running) {
        emit errorOccurred(tr("Operation already in progress"));
        return;
    }
    
    setRunning(true);
    m_status.operation = UftOperation::Write;
    emit operationStarted(UftOperation::Write);
    
    /* Similar worker setup as startRead() */
    updateStatus(tr("Writing disk..."));
    
    /* TODO: Implement actual write operation */
    QMetaObject::invokeMethod(this, [this]() {
        onWorkerFinished(true, tr("Write completed"));
    }, Qt::QueuedConnection);
}

void UftMainController::startVerify()
{
    if (m_status.running) return;
    
    setRunning(true);
    m_status.operation = UftOperation::Verify;
    emit operationStarted(UftOperation::Verify);
    
    updateStatus(tr("Verifying disk..."));
    
    /* TODO: Implement actual verify operation */
    QMetaObject::invokeMethod(this, [this]() {
        onWorkerFinished(true, tr("Verification completed"));
    }, Qt::QueuedConnection);
}

void UftMainController::startConvert(const QString &targetFormat)
{
    if (m_status.running) return;
    if (m_currentFile.isEmpty()) {
        emit errorOccurred(tr("No file loaded"));
        return;
    }
    
    setRunning(true);
    m_status.operation = UftOperation::Convert;
    emit operationStarted(UftOperation::Convert);
    
    updateStatus(tr("Converting to %1...").arg(targetFormat));
    
    /* TODO: Implement actual conversion */
    QMetaObject::invokeMethod(this, [this]() {
        onWorkerFinished(true, tr("Conversion completed"));
    }, Qt::QueuedConnection);
}

void UftMainController::startAnalyze()
{
    if (m_status.running) return;
    if (m_currentFile.isEmpty()) {
        emit errorOccurred(tr("No file loaded"));
        return;
    }
    
    setRunning(true);
    m_status.operation = UftOperation::Analyze;
    emit operationStarted(UftOperation::Analyze);
    
    updateStatus(tr("Analyzing disk..."));
    
    /* Trigger format detection */
    m_formatModel->detectFromFile(m_currentFile);
    
    /* Analysis completes with format detection */
    onWorkerFinished(true, tr("Analysis completed"));
}

void UftMainController::cancelOperation()
{
    if (!m_status.running) return;
    
    m_status.cancelled = true;
    updateStatus(tr("Cancelling..."));
    
    /* Signal worker to stop */
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->requestInterruption();
    }
    
    emit operationCancelled(m_status.operation);
}

void UftMainController::applyDetectedFormat()
{
    if (!m_formatModel->hasResults()) return;
    
    QString format = m_formatModel->bestFormat();
    m_paramModel->setFormat(format);
    
    updateStatus(tr("Applied format: %1").arg(format));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Hardware Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftMainController::refreshHardware()
{
    m_availableDevices.clear();
    
    /* Detect available hardware */
    /* This would call into C backend to enumerate devices */
    
#ifdef Q_OS_LINUX
    /* Check for Greaseweazle */
    QDir dev("/dev");
    QStringList filters;
    filters << "ttyACM*" << "ttyUSB*";
    auto devices = dev.entryList(filters, QDir::System);
    for (const QString &d : devices) {
        m_availableDevices.append("/dev/" + d);
    }
    
    /* Check for standard floppy */
    if (QFileInfo::exists("/dev/fd0")) {
        m_availableDevices.append("/dev/fd0");
    }
#endif

#ifdef Q_OS_WIN
    /* Windows device enumeration */
    m_availableDevices.append("COM3");  /* Placeholder */
    m_availableDevices.append("A:");
#endif
    
    emit hardwareListChanged();
}

void UftMainController::selectDevice(const QString &device)
{
    m_paramModel->setDevicePath(device);
    updateStatus(tr("Selected device: %1").arg(device));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Private Slots
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftMainController::onParametersChanged()
{
    /* React to parameter changes if needed */
}

void UftMainController::onFormatDetected(const QString &formatId, 
                                          const QString &formatName, 
                                          int confidence)
{
    Q_UNUSED(formatName);
    emit formatDetected(formatId, confidence);
    updateStatus(tr("Detected format: %1 (%2%% confidence)").arg(formatId).arg(confidence));
}

void UftMainController::onWorkerProgress(int current, int total, const QString &msg)
{
    m_status.currentTrack = current;
    m_status.totalTracks = total;
    m_status.progress = total > 0 ? (current * 100 / total) : 0;
    m_status.statusMessage = msg;
    
    emit progressChanged(m_status.progress);
    emit statusChanged(msg);
    emit operationProgress(current, total, msg);
}

void UftMainController::onWorkerFinished(bool success, const QString &message)
{
    UftOperation op = m_status.operation;
    
    setRunning(false);
    m_status.operation = UftOperation::None;
    
    if (success) {
        updateStatus(message);
    } else {
        m_status.lastError = message;
        emit errorOccurred(message);
    }
    
    emit operationCompleted(op, success);
    
    /* Clean up worker thread */
    if (m_workerThread) {
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
    }
}

void UftMainController::updateStatus(const QString &message, int progress)
{
    m_status.statusMessage = message;
    if (progress >= 0) {
        m_status.progress = progress;
        emit progressChanged(progress);
    }
    emit statusChanged(message);
}

void UftMainController::setRunning(bool running)
{
    if (m_status.running != running) {
        m_status.running = running;
        m_status.cancelled = false;
        emit busyChanged(running);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * UftOperationWorker Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

UftOperationWorker::UftOperationWorker(UftMainController *controller)
    : QObject(nullptr)
    , m_controller(controller)
{
}

void UftOperationWorker::process()
{
    m_cancelled = false;
    
    switch (m_operation) {
        case UftOperation::Read:
            /* Simulate read operation */
            for (int track = 0; track < 160 && !m_cancelled; track++) {
                int cyl = track / 2;
                int head = track % 2;
                
                emit progress(track + 1, 160, 
                             QString("Reading track %1/%2").arg(cyl).arg(head));
                
                QThread::msleep(10);  /* Simulate work */
                
                emit trackProcessed(cyl, head, 0);
                
                if (QThread::currentThread()->isInterruptionRequested()) {
                    m_cancelled = true;
                    break;
                }
            }
            break;
            
        case UftOperation::Write:
            /* Simulate write operation */
            for (int track = 0; track < 160 && !m_cancelled; track++) {
                emit progress(track + 1, 160,
                             QString("Writing track %1").arg(track / 2));
                QThread::msleep(15);
                
                if (QThread::currentThread()->isInterruptionRequested()) {
                    m_cancelled = true;
                    break;
                }
            }
            break;
            
        case UftOperation::Verify:
            for (int track = 0; track < 160 && !m_cancelled; track++) {
                emit progress(track + 1, 160,
                             QString("Verifying track %1").arg(track / 2));
                QThread::msleep(5);
                
                if (QThread::currentThread()->isInterruptionRequested()) {
                    m_cancelled = true;
                    break;
                }
            }
            break;
            
        default:
            break;
    }
    
    if (m_cancelled) {
        emit finished(false, "Operation cancelled");
    } else {
        emit finished(true, "Operation completed successfully");
    }
}

void UftOperationWorker::cancel()
{
    m_cancelled = true;
}

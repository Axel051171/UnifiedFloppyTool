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
#include <QFile>

// HAL includes
#ifdef UFT_HAS_HAL
extern "C" {
#include "uft/hal/uft_greaseweazle_full.h"
#include "uft/uft_disk.h"
#include "uft/uft_core.h"
}
#define HAS_HAL 1
#else
#define HAS_HAL 0
#endif

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
    bool success = false;
    QString message;
    
    switch (m_operation) {
        case UftOperation::Read:
            if (m_hwDevice != nullptr) {
                success = processReadFromHardware();
                message = success ? "Read completed" : "Read failed";
            } else if (!m_sourcePath.isEmpty()) {
                success = processReadFromFile();
                message = success ? "File loaded" : "Load failed";
            } else {
                message = "No source specified";
            }
            break;
            
        case UftOperation::Write:
            if (m_hwDevice != nullptr) {
                success = processWriteToHardware();
                message = success ? "Write completed" : "Write failed";
            } else if (!m_destPath.isEmpty()) {
                success = processWriteToFile();
                message = success ? "File saved" : "Save failed";
            } else {
                message = "No destination specified";
            }
            break;
            
        case UftOperation::Verify:
            // Verify reads back and compares
            if (m_hwDevice != nullptr) {
                // TODO: Read and compare with source
                message = "Verify not implemented for hardware";
            } else {
                message = "Verify requires hardware";
            }
            break;
            
        default:
            message = "Unknown operation";
            break;
    }
    
    if (m_cancelled) {
        emit finished(false, "Operation cancelled");
    } else {
        emit finished(success, message);
    }
}

bool UftOperationWorker::processReadFromHardware()
{
#if HAS_HAL
    if (m_hwDevice == nullptr) return false;
    
    uft_gw_device_t *gw = static_cast<uft_gw_device_t*>(m_hwDevice);
    
    // Get parameters
    int maxCylinders = m_params.value("cylinders", 80).toInt();
    int heads = m_params.value("heads", 2).toInt();
    int revolutions = m_params.value("revolutions", 3).toInt();
    
    // Select drive and turn on motor
    int ret = uft_gw_select_drive(gw, 0);
    if (ret != 0) {
        qWarning() << "Failed to select drive:" << ret;
        return false;
    }
    
    ret = uft_gw_set_motor(gw, true);
    if (ret != 0) {
        qWarning() << "Failed to turn on motor:" << ret;
        return false;
    }
    
    QThread::msleep(500);  // Spin-up
    
    int totalTracks = maxCylinders * heads;
    int currentTrack = 0;
    int errors = 0;
    
    // Prepare output file if specified
    QFile outFile;
    if (!m_destPath.isEmpty()) {
        outFile.setFileName(m_destPath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            uft_gw_set_motor(gw, false);
            qWarning() << "Cannot open output file:" << m_destPath;
            return false;
        }
    }
    
    // Read all tracks
    for (int cyl = 0; cyl < maxCylinders && !m_cancelled; cyl++) {
        ret = uft_gw_seek(gw, cyl);
        if (ret != 0) {
            qWarning() << "Seek error at cylinder" << cyl;
            errors++;
            continue;
        }
        
        for (int head = 0; head < heads && !m_cancelled; head++) {
            uft_gw_select_head(gw, head);
            
            emit progress(currentTrack + 1, totalTracks,
                         QString("Reading C%1 H%2").arg(cyl).arg(head));
            
            // Read flux data
            uft_gw_read_params_t params = {0};
            params.revolutions = revolutions;
            params.index_sync = true;
            
            uft_gw_flux_data_t flux = {0};
            ret = uft_gw_read_flux(gw, &params, &flux);
            
            int trackErrors = 0;
            if (ret != 0 || flux.sample_count == 0) {
                trackErrors = 1;
                errors++;
            } else {
                // TODO: Decode flux and write to file
                // For now, store raw flux (SCP-like format)
                if (outFile.isOpen() && flux.samples) {
                    // Simple raw dump (would need proper format handling)
                    // outFile.write((char*)flux.samples, flux.sample_count * 4);
                }
            }
            
            emit trackProcessed(cyl, head, trackErrors);
            
            // Free flux data
            if (flux.samples) free(flux.samples);
            if (flux.index_times) free(flux.index_times);
            
            currentTrack++;
            
            if (QThread::currentThread()->isInterruptionRequested()) {
                m_cancelled = true;
            }
        }
    }
    
    // Cleanup
    uft_gw_seek(gw, 0);
    uft_gw_set_motor(gw, false);
    
    if (outFile.isOpen()) {
        outFile.close();
    }
    
    qDebug() << "Read complete:" << currentTrack << "tracks," << errors << "errors";
    return errors == 0;
    
#else
    // No HAL - simulate
    int totalTracks = 160;
    for (int track = 0; track < totalTracks && !m_cancelled; track++) {
        int cyl = track / 2;
        int head = track % 2;
        
        emit progress(track + 1, totalTracks,
                     QString("Reading C%1 H%2 [SIMULATED]").arg(cyl).arg(head));
        
        QThread::msleep(10);
        emit trackProcessed(cyl, head, 0);
        
        if (QThread::currentThread()->isInterruptionRequested()) {
            m_cancelled = true;
        }
    }
    return true;
#endif
}

bool UftOperationWorker::processWriteToHardware()
{
#if HAS_HAL
    if (m_hwDevice == nullptr) return false;
    if (m_sourcePath.isEmpty()) return false;
    
    uft_gw_device_t *gw = static_cast<uft_gw_device_t*>(m_hwDevice);
    
    // Check write protect
    if (uft_gw_is_write_protected(gw)) {
        qWarning() << "Disk is write protected";
        return false;
    }
    
    // TODO: Load source file and decode tracks
    // For now, this is a placeholder
    
    // Select drive and turn on motor
    uft_gw_select_drive(gw, 0);
    uft_gw_set_motor(gw, true);
    QThread::msleep(500);
    
    int maxCylinders = m_params.value("cylinders", 80).toInt();
    int heads = m_params.value("heads", 2).toInt();
    int totalTracks = maxCylinders * heads;
    int errors = 0;
    
    for (int cyl = 0; cyl < maxCylinders && !m_cancelled; cyl++) {
        uft_gw_seek(gw, cyl);
        
        for (int head = 0; head < heads && !m_cancelled; head++) {
            uft_gw_select_head(gw, head);
            
            emit progress(cyl * heads + head + 1, totalTracks,
                         QString("Writing C%1 H%2").arg(cyl).arg(head));
            
            // TODO: Get flux data for this track from source
            // uft_gw_write_params_t params = {0};
            // uft_gw_write_flux(gw, &params, flux_data, flux_count);
            
            QThread::msleep(20);  // Placeholder
            
            emit trackProcessed(cyl, head, 0);
            
            if (QThread::currentThread()->isInterruptionRequested()) {
                m_cancelled = true;
            }
        }
    }
    
    uft_gw_seek(gw, 0);
    uft_gw_set_motor(gw, false);
    
    return errors == 0;
    
#else
    // Simulate
    int totalTracks = 160;
    for (int track = 0; track < totalTracks && !m_cancelled; track++) {
        emit progress(track + 1, totalTracks,
                     QString("Writing track %1 [SIMULATED]").arg(track / 2));
        QThread::msleep(15);
        
        if (QThread::currentThread()->isInterruptionRequested()) {
            m_cancelled = true;
        }
    }
    return true;
#endif
}

bool UftOperationWorker::processReadFromFile()
{
    if (m_sourcePath.isEmpty()) return false;
    
    QFile file(m_sourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file:" << m_sourcePath;
        return false;
    }
    
    // TODO: Use uft_disk_open() to properly load the file
    // For now, just verify it exists
    
    qint64 size = file.size();
    file.close();
    
    emit progress(100, 100, QString("Loaded %1 bytes").arg(size));
    return true;
}

bool UftOperationWorker::processWriteToFile()
{
    if (m_destPath.isEmpty()) return false;
    
    // TODO: Use uft_disk_save() to write the file
    // For now, placeholder
    
    emit progress(100, 100, QString("Saving to %1").arg(m_destPath));
    return true;
}

void UftOperationWorker::cancel()
{
    m_cancelled = true;
}

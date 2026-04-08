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
#include "uft/uft_format_convert.h"
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
    
    /* Wire write to background worker thread */
    updateStatus(tr("Writing disk..."));

    if (m_currentFile.isEmpty()) {
        emit errorOccurred(tr("No source file loaded for writing"));
        setRunning(false);
        return;
    }

    m_workerThread = new QThread(this);
    auto *worker = new UftOperationWorker(this);
    worker->setOperation(UftOperation::Write);
    worker->setSourcePath(m_currentFile);
    worker->setHardwareDevice(m_backendHandle);

    /* Pass current parameters to worker */
    QVariantMap params;
    params["cylinders"] = m_paramModel->getValue("cylinders");
    params["heads"]     = m_paramModel->getValue("heads");
    worker->setParameters(params);

    worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, worker, &UftOperationWorker::process);
    connect(worker, &UftOperationWorker::progress, this, &UftMainController::onWorkerProgress);
    connect(worker, &UftOperationWorker::finished, this, &UftMainController::onWorkerFinished);
    connect(worker, &UftOperationWorker::finished, m_workerThread, &QThread::quit);
    connect(m_workerThread, &QThread::finished, worker, &QObject::deleteLater);

    m_workerThread->start();
}

void UftMainController::startVerify()
{
    if (m_status.running) return;
    
    setRunning(true);
    m_status.operation = UftOperation::Verify;
    emit operationStarted(UftOperation::Verify);
    
    updateStatus(tr("Verifying disk..."));

    if (m_currentFile.isEmpty()) {
        emit errorOccurred(tr("No source file loaded for verification"));
        setRunning(false);
        return;
    }

    if (!m_backendHandle) {
        emit errorOccurred(tr("Verify requires a hardware device"));
        setRunning(false);
        return;
    }

    m_workerThread = new QThread(this);
    auto *worker = new UftOperationWorker(this);
    worker->setOperation(UftOperation::Verify);
    worker->setSourcePath(m_currentFile);
    worker->setHardwareDevice(m_backendHandle);

    QVariantMap params;
    params["cylinders"] = m_paramModel->getValue("cylinders");
    params["heads"]     = m_paramModel->getValue("heads");
    params["revolutions"] = m_paramModel->getValue("revolutions");
    worker->setParameters(params);

    worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, worker, &UftOperationWorker::process);
    connect(worker, &UftOperationWorker::progress, this, &UftMainController::onWorkerProgress);
    connect(worker, &UftOperationWorker::finished, this, &UftMainController::onWorkerFinished);
    connect(worker, &UftOperationWorker::finished, m_workerThread, &QThread::quit);
    connect(m_workerThread, &QThread::finished, worker, &QObject::deleteLater);

    m_workerThread->start();
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

    /* Determine output path */
    QString outputPath = m_paramModel->outputPath();
    if (outputPath.isEmpty()) {
        outputPath = suggestOutputPath(m_currentFile, targetFormat);
    }

    if (outputPath.isEmpty()) {
        emit errorOccurred(tr("Cannot determine output path for conversion"));
        setRunning(false);
        return;
    }

#ifdef UFT_HAS_HAL
    /* Resolve target format enum from string */
    QByteArray fmtBa = targetFormat.toLower().toUtf8();
    uft_format_t dstFormat = uft_format_for_extension(fmtBa.constData());
    if (dstFormat == UFT_FORMAT_UNKNOWN) {
        emit errorOccurred(tr("Unknown target format: %1").arg(targetFormat));
        setRunning(false);
        return;
    }

    /* Run conversion via C backend */
    QByteArray srcBa = m_currentFile.toUtf8();
    QByteArray dstBa = outputPath.toUtf8();

    uft_convert_options_ext_t opts = {};
    opts.verify_after = true;
    opts.preserve_errors = true;
    opts.preserve_weak_bits = true;
    opts.decode_retries = 3;
    opts.use_multiple_revs = true;

    uft_convert_result_t result = {};

    uft_error_t err = uft_convert_file(srcBa.constData(),
                                        dstBa.constData(),
                                        dstFormat, &opts, &result);

    if (err == UFT_OK && result.success) {
        QString msg = tr("Conversion completed: %1 tracks, %2 sectors written to %3")
                      .arg(result.tracks_converted)
                      .arg(result.sectors_converted)
                      .arg(QFileInfo(outputPath).fileName());
        if (result.warning_count > 0) {
            msg += tr(" (%1 warnings)").arg(result.warning_count);
        }
        onWorkerFinished(true, msg);
    } else {
        QString errMsg = tr("Conversion failed");
        if (result.tracks_failed > 0) {
            errMsg += tr(": %1 tracks failed").arg(result.tracks_failed);
        }
        onWorkerFinished(false, errMsg);
    }
#else
    /* No backend - simulate conversion */
    QMetaObject::invokeMethod(this, [this, targetFormat, outputPath]() {
        onWorkerFinished(true, tr("Conversion to %1 completed (simulated): %2")
                                .arg(targetFormat, QFileInfo(outputPath).fileName()));
    }, Qt::QueuedConnection);
#endif
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
            // Verify: read back from hardware and compare with source
            if (m_hwDevice != nullptr && !m_sourcePath.isEmpty()) {
                success = processVerifyFromHardware();
                message = success ? "Verification passed" : "Verification failed - mismatches detected";
            } else if (m_hwDevice == nullptr) {
                message = "Verify requires a hardware device";
            } else {
                message = "Verify requires a source file to compare against";
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
            uft_gw_read_params_t params = {};
            params.revolutions = revolutions;
            params.index_sync = true;
            
            uft_gw_flux_data_t *flux = nullptr;
            ret = uft_gw_read_flux(gw, &params, &flux);
            
            int trackErrors = 0;
            if (ret != 0 || flux == nullptr || flux->sample_count == 0) {
                trackErrors = 1;
                errors++;
            } else {
                // TODO: Decode flux and write to file
                // For now, store raw flux (SCP-like format)
                if (outFile.isOpen() && flux->samples) {
                    // Simple raw dump (would need proper format handling)
                    // outFile.write((char*)flux->samples, flux->sample_count * 4);
                }
            }
            
            emit trackProcessed(cyl, head, trackErrors);
            
            // Free flux data
            if (flux) {
                if (flux->samples) free(flux->samples);
                if (flux->index_times) free(flux->index_times);
                free(flux);
            }
            
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

    // Load source file using uft_disk_open()
    if (m_sourcePath.isEmpty()) {
        qWarning() << "No source file specified for write operation";
        return false;
    }

    QByteArray srcBa = m_sourcePath.toUtf8();
    uft_disk_t *srcDisk = uft_disk_open(srcBa.constData(), true /* read_only */);
    if (!srcDisk) {
        qWarning() << "Cannot open source file:" << m_sourcePath;
        return false;
    }

    // Get geometry from source
    uft_geometry_t geom = {};
    uft_disk_get_geometry(srcDisk, &geom);

    int maxCylinders = geom.cylinders > 0 ? geom.cylinders
                     : m_params.value("cylinders", 80).toInt();
    int heads = geom.heads > 0 ? geom.heads
              : m_params.value("heads", 2).toInt();
    int totalTracks = maxCylinders * heads;
    int errors = 0;

    // Select drive and turn on motor
    uft_gw_select_drive(gw, 0);
    uft_gw_set_motor(gw, true);
    QThread::msleep(500);

    for (int cyl = 0; cyl < maxCylinders && !m_cancelled; cyl++) {
        uft_gw_seek(gw, cyl);

        for (int head = 0; head < heads && !m_cancelled; head++) {
            uft_gw_select_head(gw, head);

            int trackNum = cyl * heads + head + 1;
            emit progress(trackNum, totalTracks,
                         QString("Writing C%1 H%2").arg(cyl).arg(head));

            // Read track from source disk image
            uft_track_t *track = uft_track_read(srcDisk, cyl, head, nullptr);
            int trackErrors = 0;

            if (track && uft_track_has_flux(track)) {
                // Source has flux data - write it directly
                uint32_t *fluxData = nullptr;
                size_t fluxCount = 0;
                uft_error_t ferr = uft_track_get_flux(track, &fluxData, &fluxCount);

                if (ferr == UFT_OK && fluxData && fluxCount > 0) {
                    uft_gw_write_params_t wparams = {};
                    wparams.index_sync = true;
                    wparams.erase_empty = false;
                    wparams.verify = false;
                    wparams.terminate_at_index = 1;

                    int ret = uft_gw_write_flux(gw, &wparams, fluxData,
                                                 (uint32_t)fluxCount);
                    if (ret != 0) {
                        qWarning() << "Write flux failed at C" << cyl << "H" << head;
                        trackErrors = 1;
                        errors++;
                    }
                    free(fluxData);
                } else {
                    trackErrors = 1;
                    errors++;
                }
            } else if (track) {
                // Sector-based source - write sector data via flux synthesis
                // For sector images (ADF, D64, etc.), we need the track data
                // The hardware write requires flux; log that synthesis is needed
                qWarning() << "Track C" << cyl << "H" << head
                           << "has no flux data; flux synthesis required";
                trackErrors = 1;
                errors++;
            } else {
                qWarning() << "Cannot read track C" << cyl << "H" << head
                           << "from source";
                trackErrors = 1;
                errors++;
            }

            if (track) uft_track_free(track);
            emit trackProcessed(cyl, head, trackErrors);

            if (QThread::currentThread()->isInterruptionRequested()) {
                m_cancelled = true;
            }
        }
    }

    uft_gw_seek(gw, 0);
    uft_gw_set_motor(gw, false);
    uft_disk_close(srcDisk);

    qDebug() << "Write complete:" << totalTracks << "tracks," << errors << "errors";
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

bool UftOperationWorker::processVerifyFromHardware()
{
#if HAS_HAL
    if (m_hwDevice == nullptr || m_sourcePath.isEmpty()) return false;

    uft_gw_device_t *gw = static_cast<uft_gw_device_t*>(m_hwDevice);

    /* Load the source file to compare against */
    QByteArray srcBa = m_sourcePath.toUtf8();
    uft_disk_t *srcDisk = uft_disk_open(srcBa.constData(), true);
    if (!srcDisk) {
        qWarning() << "Cannot open source file for verify:" << m_sourcePath;
        return false;
    }

    uft_geometry_t geom = {};
    uft_disk_get_geometry(srcDisk, &geom);

    int maxCylinders = geom.cylinders > 0 ? geom.cylinders
                     : m_params.value("cylinders", 80).toInt();
    int heads = geom.heads > 0 ? geom.heads
              : m_params.value("heads", 2).toInt();
    int revolutions = m_params.value("revolutions", 3).toInt();
    int totalTracks = maxCylinders * heads;
    int errors = 0;
    int mismatches = 0;

    uft_gw_select_drive(gw, 0);
    uft_gw_set_motor(gw, true);
    QThread::msleep(500);

    for (int cyl = 0; cyl < maxCylinders && !m_cancelled; cyl++) {
        int ret = uft_gw_seek(gw, cyl);
        if (ret != 0) {
            errors++;
            continue;
        }

        for (int head = 0; head < heads && !m_cancelled; head++) {
            uft_gw_select_head(gw, head);

            int trackNum = cyl * heads + head + 1;
            emit progress(trackNum, totalTracks,
                         QString("Verifying C%1 H%2").arg(cyl).arg(head));

            /* Read source track */
            uft_track_t *srcTrack = uft_track_read(srcDisk, cyl, head, nullptr);

            /* Read flux from hardware */
            uft_gw_read_params_t rparams = {};
            rparams.revolutions = revolutions;
            rparams.index_sync = true;

            uft_gw_flux_data_t *flux = nullptr;
            ret = uft_gw_read_flux(gw, &rparams, &flux);

            int trackErrors = 0;

            if (ret != 0 || flux == nullptr || flux->sample_count == 0) {
                trackErrors = 1;
                errors++;
            } else if (srcTrack) {
                /* Compare: check that we got valid flux data back.
                   Full byte-level comparison requires decoding the flux
                   into sectors, which depends on the encoding.
                   For now, verify that the readback produced a reasonable
                   number of flux transitions relative to the source track. */
                size_t srcSectorCount = uft_track_get_sector_count(srcTrack);
                if (srcSectorCount > 0 && flux->sample_count < 100) {
                    /* Readback has far too few transitions - likely blank */
                    qWarning() << "Verify mismatch at C" << cyl << "H" << head
                               << ": expected sectors but got" << flux->sample_count
                               << "flux transitions";
                    trackErrors = 1;
                    mismatches++;
                }
            }

            if (srcTrack) uft_track_free(srcTrack);
            if (flux) {
                if (flux->samples) free(flux->samples);
                if (flux->index_times) free(flux->index_times);
                free(flux);
            }

            emit trackProcessed(cyl, head, trackErrors);

            if (QThread::currentThread()->isInterruptionRequested()) {
                m_cancelled = true;
            }
        }
    }

    uft_gw_seek(gw, 0);
    uft_gw_set_motor(gw, false);
    uft_disk_close(srcDisk);

    qDebug() << "Verify complete:" << totalTracks << "tracks,"
             << errors << "errors," << mismatches << "mismatches";
    return (errors == 0 && mismatches == 0);

#else
    /* Simulate verify */
    int totalTracks = 160;
    for (int track = 0; track < totalTracks && !m_cancelled; track++) {
        int cyl = track / 2;
        int head = track % 2;
        emit progress(track + 1, totalTracks,
                     QString("Verifying C%1 H%2 [SIMULATED]").arg(cyl).arg(head));
        QThread::msleep(8);
        emit trackProcessed(cyl, head, 0);

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

    qint64 fileSize = file.size();
    file.close();

#if HAS_HAL
    /* Use uft_disk_open() from the C backend to load and parse the file */
    QByteArray pathBa = m_sourcePath.toUtf8();
    uft_disk_t *disk = uft_disk_open(pathBa.constData(), true /* read_only */);
    if (!disk) {
        qWarning() << "uft_disk_open() failed for:" << m_sourcePath;
        emit progress(100, 100, QString("Loaded %1 bytes (raw, parser unavailable)").arg(fileSize));
        return true;  /* Non-fatal: file exists, parser may not support format */
    }

    /* Extract geometry and iterate tracks for progress reporting */
    uft_geometry_t geom = {};
    uft_disk_get_geometry(disk, &geom);

    int totalTracks = geom.cylinders * geom.heads;
    if (totalTracks <= 0) totalTracks = 1;

    int currentTrack = 0;
    int errors = 0;

    for (int cyl = 0; cyl < geom.cylinders && !m_cancelled; cyl++) {
        for (int head = 0; head < geom.heads && !m_cancelled; head++) {
            emit progress(currentTrack + 1, totalTracks,
                         QString("Loading C%1 H%2").arg(cyl).arg(head));

            uft_track_t *track = uft_track_read(disk, cyl, head, nullptr);
            int trackErrors = 0;
            if (!track) {
                trackErrors = 1;
                errors++;
            } else {
                /* Check sector status */
                size_t sectorCount = uft_track_get_sector_count(track);
                for (size_t s = 0; s < sectorCount; s++) {
                    const uft_sector_t *sec = uft_track_get_sector(track, s);
                    (void)sec;  /* Sector data is now loaded into disk handle */
                }
                uft_track_free(track);
            }
            emit trackProcessed(cyl, head, trackErrors);
            currentTrack++;

            if (QThread::currentThread()->isInterruptionRequested()) {
                m_cancelled = true;
            }
        }
    }

    uft_disk_close(disk);
    emit progress(totalTracks, totalTracks,
                 QString("Loaded %1 tracks (%2 errors)").arg(currentTrack).arg(errors));
    return errors == 0;
#else
    /* No HAL - just report file size */
    emit progress(100, 100, QString("Loaded %1 bytes").arg(fileSize));
    return true;
#endif
}

bool UftOperationWorker::processWriteToFile()
{
    if (m_destPath.isEmpty()) return false;

#if HAS_HAL
    /* Open the source file first to get the disk handle */
    if (m_sourcePath.isEmpty()) {
        qWarning() << "No source path for write-to-file operation";
        return false;
    }

    QByteArray srcBa = m_sourcePath.toUtf8();
    uft_disk_t *disk = uft_disk_open(srcBa.constData(), true);
    if (!disk) {
        qWarning() << "Cannot open source disk:" << m_sourcePath;
        return false;
    }

    emit progress(50, 100, QString("Writing to %1...").arg(QFileInfo(m_destPath).fileName()));

    /* Determine target format from file extension */
    QByteArray dstBa = m_destPath.toUtf8();
    QByteArray extBa = QFileInfo(m_destPath).suffix().toLower().toUtf8();
    uft_format_t dstFormat = uft_format_for_extension(extBa.constData());

    uft_error_t err;
    if (dstFormat != UFT_FORMAT_UNKNOWN && dstFormat != uft_disk_get_format(disk)) {
        /* Save as different format */
        err = uft_disk_save_as(disk, dstBa.constData(), dstFormat);
    } else {
        /* Save in current format to new path */
        err = uft_disk_save_as(disk, dstBa.constData(), uft_disk_get_format(disk));
    }

    uft_disk_close(disk);

    if (err != UFT_OK) {
        qWarning() << "uft_disk_save_as() failed with error:" << err;
        emit progress(100, 100, QString("Save failed"));
        return false;
    }

    emit progress(100, 100, QString("Saved to %1").arg(QFileInfo(m_destPath).fileName()));
    return true;
#else
    /* No HAL - copy source to dest as raw bytes */
    if (!m_sourcePath.isEmpty() && QFile::exists(m_sourcePath)) {
        emit progress(50, 100, QString("Copying to %1...").arg(QFileInfo(m_destPath).fileName()));
        bool ok = QFile::copy(m_sourcePath, m_destPath);
        emit progress(100, 100, ok ? QString("Saved to %1").arg(m_destPath) : "Save failed");
        return ok;
    }
    emit progress(100, 100, QString("Saving to %1 (no data)").arg(m_destPath));
    return false;
#endif
}

void UftOperationWorker::cancel()
{
    m_cancelled = true;
}

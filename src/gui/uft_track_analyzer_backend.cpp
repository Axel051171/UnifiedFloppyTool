/**
 * @file uft_track_analyzer_backend.cpp
 * @brief C Backend Implementation for TrackAnalyzerWidget
 * 
 * P2-14: TrackAnalyzerWidget C-Backend
 */

#include "uft_track_analyzer_backend.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

/* ============================================================================
 * AnalysisWorker Implementation
 * ============================================================================ */

AnalysisWorker::AnalysisWorker(QObject *parent)
    : QObject(parent)
    , m_trackCount(0)
    , m_sides(2)
    , m_trackSize(0)
    , m_profile(nullptr)
    , m_autoDetect(true)
    , m_cancelled(false)
{
}

void AnalysisWorker::setTrackData(const QByteArray &data, int trackCount, int sides, size_t trackSize)
{
    QMutexLocker lock(&m_mutex);
    m_trackData = data;
    m_trackCount = trackCount;
    m_sides = sides;
    m_trackSize = trackSize;
}

void AnalysisWorker::setProfile(const uft_platform_profile_t *profile)
{
    QMutexLocker lock(&m_mutex);
    m_profile = profile;
}

void AnalysisWorker::setAutoDetect(bool autoDetect)
{
    QMutexLocker lock(&m_mutex);
    m_autoDetect = autoDetect;
}

void AnalysisWorker::cancel()
{
    m_cancelled = true;
}

const uint8_t* AnalysisWorker::getTrackPointer(int track, int side) const
{
    if (m_trackData.isEmpty() || m_trackSize == 0) return nullptr;
    
    size_t offset = (track * m_sides + side) * m_trackSize;
    if (offset + m_trackSize > (size_t)m_trackData.size()) return nullptr;
    
    return reinterpret_cast<const uint8_t*>(m_trackData.constData() + offset);
}

TrackResult AnalysisWorker::analyzeTrackInternal(int track, int side)
{
    TrackResult result;
    result.track = track;
    result.side = side;
    result.analyzed = false;
    
    const uint8_t *trackPtr = getTrackPointer(track, side);
    if (!trackPtr) {
        emit error(QString("Cannot get track data for T%1.%2").arg(track).arg(side));
        return result;
    }
    
    /* Prepare analysis config */
    uft_analysis_config_t config;
    memset(&config, 0, sizeof(config));
    config.track_data = trackPtr;
    config.track_size = m_trackSize;
    config.profile = m_autoDetect ? nullptr : m_profile;
    config.auto_detect_platform = m_autoDetect;
    config.detect_protection = true;
    config.detect_breakpoints = true;
    
    /* Run analysis */
    uft_track_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));
    
    int rc;
    if (m_autoDetect) {
        rc = uft_analyze_track(trackPtr, m_trackSize, &analysis);
    } else if (m_profile) {
        rc = uft_analyze_track_profile(trackPtr, m_trackSize, m_profile, &analysis);
    } else {
        rc = uft_analyze_track(trackPtr, m_trackSize, &analysis);
    }
    
    if (rc != 0) {
        emit error(QString("Analysis failed for T%1.%2: %3").arg(track).arg(side).arg(rc));
        return result;
    }
    
    /* Copy results */
    result.analyzed = true;
    result.type = analysis.type;
    result.platform = analysis.detected_platform;
    result.encoding = analysis.detected_encoding;
    result.confidence = analysis.confidence;
    
    result.trackLength = analysis.track_length;
    result.dataStart = analysis.data_start;
    result.dataEnd = analysis.data_end;
    
    result.syncCount = analysis.sync.count;
    result.primarySync = analysis.sync.primary_pattern;
    result.bitShifted = analysis.sync.bit_shifted;
    
    result.sectorCount = analysis.sectors.sector_count;
    result.isUniform = analysis.sectors.is_uniform;
    result.nominalLength = analysis.sectors.nominal_length;
    
    result.isProtected = analysis.is_protected;
    result.isLongTrack = analysis.is_long_track;
    result.hasWeakBits = analysis.has_weak_bits;
    result.breakpointCount = analysis.breakpoint_count;
    
    result.protectionName = QString::fromUtf8(analysis.protection_name);
    result.formatName = QString::fromUtf8(analysis.format_name);
    
    return result;
}

void AnalysisWorker::analyzeTrack(int track, int side)
{
    TrackResult result = analyzeTrackInternal(track, side);
    emit trackAnalyzed(track, side, result);
}

void AnalysisWorker::analyzeAllTracks()
{
    m_cancelled = false;
    int total = m_trackCount * m_sides;
    int current = 0;
    
    for (int t = 0; t < m_trackCount && !m_cancelled; t++) {
        for (int s = 0; s < m_sides && !m_cancelled; s++) {
            TrackResult result = analyzeTrackInternal(t, s);
            emit trackAnalyzed(t, s, result);
            
            current++;
            emit progress(current, total);
        }
    }
    
    if (!m_cancelled) {
        emit allComplete();
    }
}

void AnalysisWorker::quickScan()
{
    m_cancelled = false;
    
    /* Analyze first few tracks for quick detection */
    int tracksToScan = qMin(5, m_trackCount);
    QVector<TrackResult> quickResults;
    
    for (int t = 0; t < tracksToScan && !m_cancelled; t++) {
        for (int s = 0; s < m_sides && !m_cancelled; s++) {
            TrackResult result = analyzeTrackInternal(t, s);
            if (result.analyzed) {
                quickResults.append(result);
            }
            emit progress(t * m_sides + s + 1, tracksToScan * m_sides);
        }
    }
    
    if (m_cancelled || quickResults.isEmpty()) return;
    
    /* Summarize results */
    QuickScanSummary summary;
    
    /* Find most common platform/encoding */
    int platformCounts[16] = {0};
    int protectedCount = 0;
    float totalConfidence = 0;
    
    for (const auto &r : quickResults) {
        if (r.platform >= 0 && r.platform < 16) {
            platformCounts[r.platform]++;
        }
        if (r.isProtected) protectedCount++;
        totalConfidence += r.confidence;
    }
    
    /* Find dominant platform */
    int maxPlatform = PLATFORM_UNKNOWN;
    int maxCount = 0;
    for (int i = 0; i < 16; i++) {
        if (platformCounts[i] > maxCount) {
            maxCount = platformCounts[i];
            maxPlatform = i;
        }
    }
    
    /* Get profile for platform */
    const uft_platform_profile_t *detectedProfile = 
        uft_get_profile_by_platform(static_cast<uft_platform_t>(maxPlatform), false);
    
    if (detectedProfile) {
        summary.platform = QString::fromUtf8(detectedProfile->name);
        summary.sectorsPerTrack = detectedProfile->sectors_per_track;
        
        switch (detectedProfile->encoding) {
            case ENCODING_FM:  summary.encoding = "FM"; break;
            case ENCODING_MFM: summary.encoding = "MFM"; break;
            case ENCODING_GCR_APPLE: summary.encoding = "GCR (Apple)"; break;
            case ENCODING_GCR_C64: summary.encoding = "GCR (C64)"; break;
            case ENCODING_GCR_VICTOR: summary.encoding = "GCR (Victor)"; break;
            default: summary.encoding = "Unknown"; break;
        }
    } else {
        summary.platform = "Unknown";
        summary.encoding = "Unknown";
        summary.sectorsPerTrack = 0;
    }
    
    /* Protection info */
    summary.protectionDetected = protectedCount > 0;
    if (summary.protectionDetected && !quickResults.isEmpty()) {
        for (const auto &r : quickResults) {
            if (!r.protectionName.isEmpty()) {
                summary.protectionName = r.protectionName;
                break;
            }
        }
        if (summary.protectionName.isEmpty()) {
            summary.protectionName = "Unknown Protection";
        }
    }
    
    /* Confidence */
    summary.confidence = quickResults.isEmpty() ? 0 : 
        static_cast<int>(totalConfidence * 100 / quickResults.size());
    
    /* Recommended mode */
    if (summary.protectionDetected) {
        /* Check for weak bits or long tracks */
        bool hasWeakBits = false;
        bool hasLongTrack = false;
        for (const auto &r : quickResults) {
            if (r.hasWeakBits) hasWeakBits = true;
            if (r.isLongTrack) hasLongTrack = true;
        }
        
        if (hasWeakBits) {
            summary.recommendedMode = 3;  /* Flux */
        } else if (hasLongTrack) {
            summary.recommendedMode = 2;  /* Nibble */
        } else {
            summary.recommendedMode = 1;  /* Track */
        }
    } else {
        summary.recommendedMode = 0;  /* Normal */
    }
    
    emit quickScanComplete(summary);
}

/* ============================================================================
 * UftTrackAnalyzerBackend Implementation
 * ============================================================================ */

UftTrackAnalyzerBackend::UftTrackAnalyzerBackend(QObject *parent)
    : QObject(parent)
    , m_workerThread(nullptr)
    , m_worker(nullptr)
    , m_trackCount(0)
    , m_sides(2)
    , m_trackSize(0)
    , m_profile(nullptr)
    , m_autoDetect(true)
    , m_analyzing(false)
{
}

UftTrackAnalyzerBackend::~UftTrackAnalyzerBackend()
{
    stopWorkerThread();
}

void UftTrackAnalyzerBackend::setTrackData(const QByteArray &data, int trackCount, int sides, size_t trackSize)
{
    m_trackData = data;
    m_trackCount = trackCount;
    m_sides = sides;
    m_trackSize = trackSize;
    
    /* Resize results array */
    m_results.resize(trackCount * sides);
    for (auto &r : m_results) {
        r.analyzed = false;
    }
}

void UftTrackAnalyzerBackend::setTrackDataFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit error(QString("Cannot open file: %1").arg(path));
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    /* Try to detect format from file size */
    QFileInfo info(path);
    QString ext = info.suffix().toLower();
    
    const uft_platform_profile_t *profile = uft_detect_profile_by_size(data.size());
    
    if (profile) {
        m_profile = profile;
        
        /* Calculate track count and size */
        size_t trackSize = profile->track_length_nominal;
        int trackCount = data.size() / (trackSize * 2);  /* 2 sides */
        
        setTrackData(data, trackCount, 2, trackSize);
    } else {
        emit error(QString("Cannot detect format for: %1").arg(path));
    }
}

void UftTrackAnalyzerBackend::setProfile(int platformIndex)
{
    int count;
    const uft_platform_profile_t* const* all = uft_get_all_profiles(&count);
    
    if (platformIndex >= 0 && platformIndex < count) {
        m_profile = all[platformIndex];
    }
}

void UftTrackAnalyzerBackend::setProfileByName(const QString &name)
{
    m_profile = uft_find_profile_by_name(name.toUtf8().constData());
}

void UftTrackAnalyzerBackend::setAutoDetect(bool autoDetect)
{
    m_autoDetect = autoDetect;
}

QStringList UftTrackAnalyzerBackend::getAvailableProfiles() const
{
    QStringList list;
    int count;
    const uft_platform_profile_t* const* all = uft_get_all_profiles(&count);
    
    for (int i = 0; i < count; i++) {
        list.append(QString::fromUtf8(all[i]->name));
    }
    
    return list;
}

int UftTrackAnalyzerBackend::getProfileCount()
{
    return uft_get_profile_count();
}

void UftTrackAnalyzerBackend::startWorkerThread()
{
    if (m_workerThread) return;
    
    m_workerThread = new QThread();
    m_worker = new AnalysisWorker();
    m_worker->moveToThread(m_workerThread);
    
    /* Setup worker */
    m_worker->setTrackData(m_trackData, m_trackCount, m_sides, m_trackSize);
    m_worker->setProfile(m_profile);
    m_worker->setAutoDetect(m_autoDetect);
    
    /* Connections */
    connect(m_worker, &AnalysisWorker::trackAnalyzed, 
            this, &UftTrackAnalyzerBackend::onTrackAnalyzed);
    connect(m_worker, &AnalysisWorker::quickScanComplete,
            this, &UftTrackAnalyzerBackend::onQuickScanComplete);
    connect(m_worker, &AnalysisWorker::allComplete,
            this, &UftTrackAnalyzerBackend::onAllComplete);
    connect(m_worker, &AnalysisWorker::progress,
            this, &UftTrackAnalyzerBackend::progress);
    connect(m_worker, &AnalysisWorker::error,
            this, &UftTrackAnalyzerBackend::error);
    
    m_workerThread->start();
}

void UftTrackAnalyzerBackend::stopWorkerThread()
{
    if (!m_workerThread) return;
    
    if (m_worker) {
        m_worker->cancel();
    }
    
    m_workerThread->quit();
    m_workerThread->wait();
    
    delete m_worker;
    delete m_workerThread;
    m_worker = nullptr;
    m_workerThread = nullptr;
}

void UftTrackAnalyzerBackend::startQuickScan()
{
    if (m_analyzing) return;
    
    m_analyzing = true;
    startWorkerThread();
    
    QMetaObject::invokeMethod(m_worker, "quickScan", Qt::QueuedConnection);
}

void UftTrackAnalyzerBackend::startFullAnalysis()
{
    if (m_analyzing) return;
    
    m_analyzing = true;
    startWorkerThread();
    
    QMetaObject::invokeMethod(m_worker, "analyzeAllTracks", Qt::QueuedConnection);
}

void UftTrackAnalyzerBackend::analyzeTrack(int track, int side)
{
    startWorkerThread();
    
    QMetaObject::invokeMethod(m_worker, "analyzeTrack", Qt::QueuedConnection,
                              Q_ARG(int, track), Q_ARG(int, side));
}

void UftTrackAnalyzerBackend::cancel()
{
    if (m_worker) {
        m_worker->cancel();
    }
    m_analyzing = false;
}

bool UftTrackAnalyzerBackend::hasResult(int track, int side) const
{
    int idx = track * m_sides + side;
    if (idx < 0 || idx >= m_results.size()) return false;
    return m_results[idx].analyzed;
}

TrackResult UftTrackAnalyzerBackend::getResult(int track, int side) const
{
    int idx = track * m_sides + side;
    if (idx < 0 || idx >= m_results.size()) {
        TrackResult empty;
        empty.analyzed = false;
        return empty;
    }
    return m_results[idx];
}

QuickScanSummary UftTrackAnalyzerBackend::getQuickScanResult() const
{
    return m_quickScanResult;
}

QVector<TrackResult> UftTrackAnalyzerBackend::getAllResults() const
{
    return m_results;
}

bool UftTrackAnalyzerBackend::isAnalyzing() const
{
    return m_analyzing;
}

void UftTrackAnalyzerBackend::onTrackAnalyzed(int track, int side, const TrackResult &result)
{
    int idx = track * m_sides + side;
    if (idx >= 0 && idx < m_results.size()) {
        m_results[idx] = result;
    }
    emit trackAnalyzed(track, side, result);
}

void UftTrackAnalyzerBackend::onQuickScanComplete(const QuickScanSummary &summary)
{
    m_quickScanResult = summary;
    m_analyzing = false;
    emit quickScanComplete(summary);
}

void UftTrackAnalyzerBackend::onAllComplete()
{
    m_analyzing = false;
    emit fullAnalysisComplete();
}

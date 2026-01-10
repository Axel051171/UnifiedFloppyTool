/**
 * @file uft_track_analyzer_backend.h
 * @brief C Backend for TrackAnalyzerWidget
 * 
 * Bridges the Qt GUI TrackAnalyzerWidget with the C track analysis functions.
 * Provides a clean C++ wrapper around the C API.
 * 
 * P2-14: TrackAnalyzerWidget C-Backend
 * 
 * @author UFT Team
 * @date 2025
 */

#ifndef UFT_TRACK_ANALYZER_BACKEND_H
#define UFT_TRACK_ANALYZER_BACKEND_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QVector>
#include <QThread>
#include <QMutex>

/* C Headers */
extern "C" {
    #include "uft_track_analysis.h"
    #include "profiles/uft_profiles_all.h"
}

/**
 * @brief Result of analyzing a single track
 */
struct TrackResult {
    int track;
    int side;
    bool analyzed;
    
    /* From uft_track_analysis_t */
    int type;           /* uft_track_type_t */
    int platform;       /* uft_platform_t */
    int encoding;       /* uft_encoding_t */
    float confidence;
    
    /* Geometry */
    size_t trackLength;
    size_t dataStart;
    size_t dataEnd;
    
    /* Sync */
    int syncCount;
    uint32_t primarySync;
    bool bitShifted;
    
    /* Sectors */
    int sectorCount;
    bool isUniform;
    size_t nominalLength;
    
    /* Protection */
    bool isProtected;
    bool isLongTrack;
    bool hasWeakBits;
    int breakpointCount;
    QString protectionName;
    QString formatName;
};

/**
 * @brief Quick scan result summary
 */
struct QuickScanSummary {
    QString platform;
    QString encoding;
    int sectorsPerTrack;
    bool protectionDetected;
    QString protectionName;
    int recommendedMode;    /* 0=Normal, 1=Track, 2=Nibble, 3=Flux */
    int confidence;         /* 0-100 */
};

/**
 * @brief Worker thread for track analysis
 */
class AnalysisWorker : public QObject
{
    Q_OBJECT
    
public:
    explicit AnalysisWorker(QObject *parent = nullptr);
    
    void setTrackData(const QByteArray &data, int trackCount, int sides, size_t trackSize);
    void setProfile(const uft_platform_profile_t *profile);
    void setAutoDetect(bool autoDetect);
    
public slots:
    void analyzeTrack(int track, int side);
    void analyzeAllTracks();
    void quickScan();
    void cancel();
    
signals:
    void trackAnalyzed(int track, int side, const TrackResult &result);
    void progress(int current, int total);
    void quickScanComplete(const QuickScanSummary &summary);
    void allComplete();
    void error(const QString &message);
    
private:
    TrackResult analyzeTrackInternal(int track, int side);
    const uint8_t* getTrackPointer(int track, int side) const;
    
    QByteArray m_trackData;
    int m_trackCount;
    int m_sides;
    size_t m_trackSize;
    
    const uft_platform_profile_t *m_profile;
    bool m_autoDetect;
    bool m_cancelled;
    
    QMutex m_mutex;
};

/**
 * @brief Main backend class for track analysis
 * 
 * Usage:
 *   UftTrackAnalyzerBackend backend;
 *   backend.setTrackData(data, 80, 2, 12668);
 *   connect(&backend, &UftTrackAnalyzerBackend::trackAnalyzed, ...);
 *   backend.startQuickScan();
 */
class UftTrackAnalyzerBackend : public QObject
{
    Q_OBJECT
    
public:
    explicit UftTrackAnalyzerBackend(QObject *parent = nullptr);
    ~UftTrackAnalyzerBackend();
    
    /* Data setup */
    void setTrackData(const QByteArray &data, int trackCount, int sides, size_t trackSize);
    void setTrackDataFromFile(const QString &path);
    
    /* Profile selection */
    void setProfile(int platformIndex);
    void setProfileByName(const QString &name);
    void setAutoDetect(bool autoDetect);
    
    /* Available profiles */
    QStringList getAvailableProfiles() const;
    static int getProfileCount();
    
    /* Start analysis */
    void startQuickScan();
    void startFullAnalysis();
    void analyzeTrack(int track, int side);
    void cancel();
    
    /* Results */
    bool hasResult(int track, int side) const;
    TrackResult getResult(int track, int side) const;
    QuickScanSummary getQuickScanResult() const;
    QVector<TrackResult> getAllResults() const;
    
    /* Status */
    bool isAnalyzing() const;
    
signals:
    void trackAnalyzed(int track, int side, const TrackResult &result);
    void quickScanComplete(const QuickScanSummary &summary);
    void fullAnalysisComplete();
    void progress(int current, int total);
    void error(const QString &message);
    
private slots:
    void onTrackAnalyzed(int track, int side, const TrackResult &result);
    void onQuickScanComplete(const QuickScanSummary &summary);
    void onAllComplete();
    
private:
    void startWorkerThread();
    void stopWorkerThread();
    
    QThread *m_workerThread;
    AnalysisWorker *m_worker;
    
    QByteArray m_trackData;
    int m_trackCount;
    int m_sides;
    size_t m_trackSize;
    
    const uft_platform_profile_t *m_profile;
    bool m_autoDetect;
    bool m_analyzing;
    
    QVector<TrackResult> m_results;
    QuickScanSummary m_quickScanResult;
};

#endif /* UFT_TRACK_ANALYZER_BACKEND_H */

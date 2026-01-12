/**
 * @file TrackAnalyzerWidget.h
 * @brief Universal Track Analyzer Widget
 * 
 * Multi-platform track analysis GUI component using XCopy Pro algorithms.
 * Supports: Amiga, Atari ST, IBM PC, Apple II, C64, BBC, MSX, Amstrad
 * 
 * Features:
 * - Quick Scan (auto-detect platform)
 * - Full track-by-track analysis
 * - Protection detection with heatmap
 * - Copy mode recommendation
 * - Export to JSON/Report
 * 
 * @author UFT Team
 * @version 1.0.0
 */

#ifndef TRACK_ANALYZER_WIDGET_H
#define TRACK_ANALYZER_WIDGET_H

#include <QWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QCheckBox>
#include <QSpinBox>
#include <QTimer>
#include <QThread>

/* Forward declare C structs */
extern "C" {
    #include "uft_track_analysis.h"
}

/**
 * @brief Copy mode recommendation
 */
enum class CopyModeRecommendation {
    Normal,         /**< Standard sector copy */
    TrackCopy,      /**< Track-level copy */
    NibbleCopy,     /**< Raw nibble copy */
    FluxCopy,       /**< Flux-level preservation */
    Mixed           /**< Different modes per track */
};

/**
 * @brief Per-track analysis result for GUI
 */
struct TrackAnalysisResult {
    int track;
    int side;
    uft_track_analysis_t analysis;
    CopyModeRecommendation recommendedMode;
    bool analyzed;
};

/**
 * @brief Quick scan result
 */
struct QuickScanResult {
    uft_platform_t platform;
    uft_encoding_t encoding;
    QString platformName;
    QString encodingName;
    int sectorsPerTrack;
    bool protectionDetected;
    QString protectionName;
    CopyModeRecommendation recommendedMode;
    float confidence;
};

/**
 * @brief Universal Track Analyzer Widget
 */
class TrackAnalyzerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrackAnalyzerWidget(QWidget *parent = nullptr);
    ~TrackAnalyzerWidget();

    /**
     * @brief Set track data for analysis
     * @param trackData Raw track data (all tracks concatenated)
     * @param trackCount Number of tracks
     * @param sides Number of sides (1 or 2)
     */
    void setTrackData(const QByteArray &trackData, int trackCount, int sides);
    
    /**
     * @brief Set track data from flux/image file
     */
    void setTrackDataFromFile(const QString &filename);

    /**
     * @brief Get recommended copy mode for specific track
     */
    CopyModeRecommendation getTrackCopyMode(int track, int side) const;
    
    /**
     * @brief Get all track modes as array (for XCopy integration)
     */
    QVector<CopyModeRecommendation> getAllTrackModes() const;
    
    /**
     * @brief Get overall recommendation
     */
    CopyModeRecommendation getOverallRecommendation() const;

    /**
     * @brief Quick scan - analyze first few tracks only
     */
    QuickScanResult quickScan();

signals:
    /**
     * @brief Emitted when analysis completes
     */
    void analysisComplete(int tracksAnalyzed, int protectedTracks);
    
    /**
     * @brief Emitted when quick scan completes
     */
    void quickScanComplete(const QuickScanResult &result);
    
    /**
     * @brief Emitted when user wants to apply settings to XCopy
     */
    void applyToXCopy(CopyModeRecommendation mode, 
                      const QVector<CopyModeRecommendation> &trackModes);
    
    /**
     * @brief Emitted when a track is selected in the heatmap
     */
    void trackSelected(int track, int side);

public slots:
    /**
     * @brief Run quick scan
     */
    void runQuickScan();
    
    /**
     * @brief Run full analysis on all tracks
     */
    void runFullAnalysis();
    
    /**
     * @brief Analyze single track
     */
    void analyzeTrack(int track, int side);
    
    /**
     * @brief Export analysis to JSON
     */
    void exportToJson();
    
    /**
     * @brief Export analysis to text report
     */
    void exportToReport();
    
    /**
     * @brief Clear all results
     */
    void clearResults();
    
    /**
     * @brief Apply recommended settings to XCopy panel
     */
    void applySettings();

protected slots:
    void onPlatformChanged(int index);
    void onTrackClicked(int row, int column);
    void onAnalysisProgress(int current, int total);

private:
    void setupUi();
    void createPlatformGroup();
    void createHeatmapGroup();
    void createDetailsGroup();
    void createActionsGroup();
    void createQuickScanGroup();
    
    void updateHeatmap();
    void updateTrackDetails(int track, int side);
    void updateSummary();
    
    QString getCopyModeName(CopyModeRecommendation mode) const;
    QColor getTraitColor(float intensity) const;
    CopyModeRecommendation determineTrackMode(const uft_track_analysis_t &analysis) const;

    /* UI Components */
    QSplitter *m_mainSplitter;
    
    /* Platform selection */
    QGroupBox *m_platformGroup;
    QComboBox *m_platformCombo;
    QLabel *m_detectedLabel;
    QCheckBox *m_autoDetect;
    
    /* Quick Scan */
    QGroupBox *m_quickScanGroup;
    QLabel *m_quickPlatform;
    QLabel *m_quickEncoding;
    QLabel *m_quickSectors;
    QLabel *m_quickProtection;
    QLabel *m_quickRecommendation;
    QProgressBar *m_quickConfidence;
    
    /* Heatmap */
    QGroupBox *m_heatmapGroup;
    QTableWidget *m_heatmapTable;
    QStringList m_traitNames;
    
    /* Track details */
    QGroupBox *m_detailsGroup;
    QLabel *m_trackLabel;
    QTextEdit *m_detailsText;
    QLabel *m_recommendedModeLabel;
    
    /* Summary */
    QGroupBox *m_summaryGroup;
    QLabel *m_totalTracks;
    QLabel *m_protectedTracks;
    QLabel *m_overallMode;
    QProgressBar *m_overallConfidence;
    
    /* Actions */
    QGroupBox *m_actionsGroup;
    QPushButton *m_quickScanBtn;
    QPushButton *m_fullAnalysisBtn;
    QPushButton *m_exportJsonBtn;
    QPushButton *m_exportReportBtn;
    QPushButton *m_applyBtn;
    QPushButton *m_clearBtn;
    
    /* Data */
    QByteArray m_trackData;
    int m_trackCount;
    int m_sides;
    size_t m_trackSize;  /* Size per track */
    
    QVector<TrackAnalysisResult> m_results;
    QuickScanResult m_quickResult;
    
    const uft_platform_profile_t *m_currentProfile;
    
    /* Heatmap colors */
    static const QColor COLOR_NONE;
    static const QColor COLOR_LOW;
    static const QColor COLOR_MEDIUM;
    static const QColor COLOR_HIGH;
    static const QColor COLOR_CRITICAL;
};

#endif /* TRACK_ANALYZER_WIDGET_H */

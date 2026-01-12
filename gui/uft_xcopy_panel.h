/**
 * @file uft_xcopy_panel.h
 * @brief XCopy Panel - Disk Copy and Duplication Settings
 * 
 * Extended with XCopy Pro Track Analyzer integration for automatic
 * copy mode selection based on protection detection.
 */

#ifndef UFT_XCOPY_PANEL_H
#define UFT_XCOPY_PANEL_H

#include <QWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QVector>

/* Forward declaration */
class TrackAnalyzerWidget;

/**
 * @brief Copy mode recommendation from analysis
 */
enum class CopyModeRecommendation {
    Normal = 0,     /**< Standard sector copy */
    TrackCopy = 1,  /**< Track-level copy */
    FluxCopy = 2,   /**< Flux-level preservation */
    NibbleCopy = 3, /**< Raw nibble copy */
    Auto = 4,       /**< Auto from analysis */
    Mixed = 5       /**< Different modes per track */
};

class UftXCopyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftXCopyPanel(QWidget *parent = nullptr);

    struct XCopyParams {
        /* Source/Destination */
        int source_drive;
        int dest_drive;
        bool source_is_image;
        bool dest_is_image;
        QString source_path;
        QString dest_path;
        
        /* Track Range */
        int start_track;
        int end_track;
        int sides;              /* 0=Side 0, 1=Side 1, 2=Both */
        
        /* Copy Mode */
        int copy_mode;          /* 0=Normal, 1=Track, 2=Flux, 3=Nibble, 4=Auto, 5=Mixed */
        bool verify_after_write;
        int verify_retries;
        
        /* Per-track modes (for Mixed mode) */
        QVector<int> track_modes;
        
        /* Error Handling */
        bool ignore_errors;
        bool retry_on_error;
        int max_retries;
        bool skip_bad_sectors;
        bool fill_bad_sectors;
        uint8_t fill_byte;
        
        /* Speed */
        int read_speed;         /* 0=Normal, 1=Fast, 2=Maximum */
        int write_speed;
        bool buffer_entire_disk;
        
        /* Multiple Copies */
        int num_copies;
        bool auto_eject;
        bool wait_for_disk;
        
        /* Analysis results */
        bool analysis_available;
        int protected_tracks;
        QString protection_info;
    };

    XCopyParams getParams() const;
    void setParams(const XCopyParams &params);
    
    /**
     * @brief Get copy mode for specific track (for Mixed mode)
     */
    int getTrackCopyMode(int track, int side) const;
    
    /**
     * @brief Check if analysis is available
     */
    bool hasAnalysis() const { return m_hasAnalysis; }

signals:
    void copyStarted();
    void copyProgress(int track, int side, int percent);
    void copyFinished(bool success);
    void paramsChanged();
    
    /* New: Request analysis from TrackAnalyzerWidget */
    void requestAnalysis();
    void requestQuickScan();

public slots:
    void startCopy();
    void stopCopy();
    void selectSource();
    void selectDest();
    
    /**
     * @brief Apply analysis results from TrackAnalyzerWidget
     * @param mode Overall recommended mode
     * @param trackModes Per-track modes (for Mixed mode)
     */
    void applyAnalysisResults(CopyModeRecommendation mode,
                              const QVector<CopyModeRecommendation> &trackModes);
    
    /**
     * @brief Set analysis info for display
     */
    void setAnalysisInfo(int protectedTracks, const QString &protectionInfo);
    
    /**
     * @brief Clear analysis data
     */
    void clearAnalysis();

private slots:
    void onCopyModeChanged(int index);
    void onAnalyzeClicked();

private:
    void setupUi();
    void createSourceGroup();
    void createDestGroup();
    void createRangeGroup();
    void createModeGroup();
    void createErrorGroup();
    void createSpeedGroup();
    void createMultipleGroup();
    void createProgressGroup();
    void createAnalysisGroup();
    
    void updateAnalysisDisplay();
    void updateMixedModePreview();

    /* Source */
    QGroupBox *m_sourceGroup;
    QComboBox *m_sourceDrive;
    QRadioButton *m_sourceIsDrive;
    QRadioButton *m_sourceIsImage;
    QLineEdit *m_sourcePath;
    QPushButton *m_browseSource;
    
    /* Destination */
    QGroupBox *m_destGroup;
    QComboBox *m_destDrive;
    QRadioButton *m_destIsDrive;
    QRadioButton *m_destIsImage;
    QLineEdit *m_destPath;
    QPushButton *m_browseDest;
    
    /* Range */
    QGroupBox *m_rangeGroup;
    QSpinBox *m_startTrack;
    QSpinBox *m_endTrack;
    QComboBox *m_sides;
    QCheckBox *m_allTracks;
    
    /* Mode */
    QGroupBox *m_modeGroup;
    QComboBox *m_copyMode;
    QCheckBox *m_verifyWrite;
    QSpinBox *m_verifyRetries;
    
    /* Analysis integration (NEW) */
    QGroupBox *m_analysisGroup;
    QLabel *m_analysisStatus;
    QLabel *m_protectionInfo;
    QLabel *m_recommendedMode;
    QPushButton *m_analyzeBtn;
    QPushButton *m_quickScanBtn;
    QLabel *m_mixedModePreview;
    
    /* Error Handling */
    QGroupBox *m_errorGroup;
    QCheckBox *m_ignoreErrors;
    QCheckBox *m_retryErrors;
    QSpinBox *m_maxRetries;
    QCheckBox *m_skipBadSectors;
    QCheckBox *m_fillBadSectors;
    QSpinBox *m_fillByte;
    
    /* Speed */
    QGroupBox *m_speedGroup;
    QComboBox *m_readSpeed;
    QComboBox *m_writeSpeed;
    QCheckBox *m_bufferDisk;
    
    /* Multiple */
    QGroupBox *m_multipleGroup;
    QSpinBox *m_numCopies;
    QCheckBox *m_autoEject;
    QCheckBox *m_waitForDisk;
    
    /* Progress */
    QGroupBox *m_progressGroup;
    QProgressBar *m_totalProgress;
    QProgressBar *m_trackProgress;
    QLabel *m_statusLabel;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    
    /* Analysis data */
    bool m_hasAnalysis;
    int m_protectedTracks;
    QString m_protectionInfoText;
    CopyModeRecommendation m_recommendedCopyMode;
    QVector<CopyModeRecommendation> m_trackModes;
};

#endif /* UFT_XCOPY_PANEL_H */

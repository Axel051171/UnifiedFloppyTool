/**
 * @file recoveryworkflowwidget.h
 * @brief Recovery Workflow and Multi-Pass Visualization Widget
 * @version 4.0.0
 * 
 * FEATURES:
 * - Automatic problem detection display
 * - Recovery recommendation UI
 * - Multi-pass progress visualization
 * - Confidence scoring display
 * - Pass-by-pass comparison
 * - Recovery statistics
 */

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <vector>

/**
 * @enum RecoveryProblemType
 * @brief Type of problem detected
 */
enum class RecoveryProblemType
{
    CRC_ERROR,
    MISSING_SECTOR,
    WEAK_BITS,
    TIMING_DRIFT,
    PROTECTION,
    HEADER_ERROR,
    SYNC_ERROR
};

/**
 * @enum RecoverySuggestion
 * @brief Suggested recovery action
 */
enum class RecoverySuggestion
{
    NONE,
    MULTI_PASS,
    WEIGHTED_VOTING,
    ADAPTIVE_PLL,
    ADJACENT_RECOVERY,
    PROTECTION_DB,
    MANUAL_OVERRIDE
};

/**
 * @struct RecoveryProblem
 * @brief Detected problem with recovery suggestion
 */
struct RecoveryProblem
{
    int track;
    int head;
    int sector;
    RecoveryProblemType type;
    RecoverySuggestion suggestion;
    int estimatedTime;      // Additional time in seconds
    int successProbability; // 0-100%
    QString description;
};

/**
 * @struct RecoveryPassResult
 * @brief Result of a single recovery pass
 */
struct RecoveryPassResult
{
    int passNumber;
    int goodSectors;
    int totalSectors;
    int confidence;
    double timingVariance;
    bool crcOk;
    std::vector<uint8_t> data;
};

/**
 * @struct RecoveryTrackResult
 * @brief Complete recovery result for a track
 */
struct RecoveryTrackResult
{
    int track;
    int head;
    std::vector<RecoveryPassResult> passes;
    int finalConfidence;
    int weakBits;
    bool recovered;
    QString notes;
};

/**
 * @class RecoveryWorkflowWidget
 * @brief Recovery workflow control and visualization
 */
class RecoveryWorkflowWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecoveryWorkflowWidget(QWidget *parent = nullptr);
    virtual ~RecoveryWorkflowWidget();
    
    // Problem display
    void setProblems(const std::vector<RecoveryProblem>& problems);
    void clearProblems();
    int problemCount() const { return m_problems.size(); }
    
    // Recovery control
    void setRecoveryEnabled(bool enabled);
    void setMaxPasses(int passes);
    void setMinConfidence(int confidence);
    
    // Progress
    void setCurrentTrack(int track, int head);
    void setCurrentPass(int pass, int totalPasses);
    void setPassProgress(int percent);
    void setOverallProgress(int percent);
    
    // Results
    void addPassResult(int track, int head, const RecoveryPassResult& result);
    void setTrackResult(int track, int head, const RecoveryTrackResult& result);
    void clearResults();
    
    // Statistics
    void updateStatistics(int totalTracks, int goodTracks, 
                         int recoveredTracks, int failedTracks,
                         double avgConfidence);

signals:
    void startRecoveryClicked();
    void cancelRecoveryClicked();
    void applyRecommendationsClicked();
    void skipRecoveryClicked();
    void customSettingsClicked();
    void trackSelected(int track, int head);
    void problemSelected(int index);

private slots:
    void onProblemTableClicked(int row, int column);
    void onPassTableClicked(int row, int column);
    void onStartClicked();
    void onCancelClicked();
    void onApplyClicked();
    void onSkipClicked();
    void onCustomClicked();

private:
    void setupUI();
    void updateProblemTable();
    void updatePassTable();
    void updateSummary();
    QString problemTypeToString(RecoveryProblemType type) const;
    QString suggestionToString(RecoverySuggestion suggestion) const;
    QColor confidenceColor(int confidence) const;
    
    // Data
    std::vector<RecoveryProblem> m_problems;
    std::vector<RecoveryTrackResult> m_results;
    int m_currentTrack;
    int m_currentHead;
    int m_currentPass;
    int m_totalPasses;
    int m_maxPasses;
    int m_minConfidence;
    bool m_isRunning;
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    
    // Problem section
    QGroupBox* m_problemGroup;
    QTableWidget* m_problemTable;
    QLabel* m_problemSummaryLabel;
    
    // Control section
    QGroupBox* m_controlGroup;
    QPushButton* m_applyButton;
    QPushButton* m_skipButton;
    QPushButton* m_customButton;
    QPushButton* m_startButton;
    QPushButton* m_cancelButton;
    
    // Progress section
    QGroupBox* m_progressGroup;
    QLabel* m_currentTrackLabel;
    QLabel* m_currentPassLabel;
    QProgressBar* m_passProgress;
    QProgressBar* m_overallProgress;
    
    // Pass results section
    QGroupBox* m_passGroup;
    QTableWidget* m_passTable;
    
    // Statistics section
    QGroupBox* m_statsGroup;
    QLabel* m_totalTracksLabel;
    QLabel* m_goodTracksLabel;
    QLabel* m_recoveredTracksLabel;
    QLabel* m_failedTracksLabel;
    QLabel* m_avgConfidenceLabel;
    QProgressBar* m_confidenceBar;
};

/**
 * @file UftMergeStrategyWidget.h
 * @brief Widget for configuring and visualizing merge strategies
 * 
 * P2-004: Merge Strategy GUI
 * 
 * Allows configuration of:
 * - Merge strategy selection
 * - Minimum agreement threshold
 * - Weak bit preservation
 * - Timing preservation
 * - Max revolutions
 * 
 * Visualizes:
 * - Merge results per sector
 * - Agreement statistics
 * - Weak bit locations
 */

#ifndef UFT_MERGE_STRATEGY_WIDGET_H
#define UFT_MERGE_STRATEGY_WIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>

/**
 * @brief Merge strategy enum (mirrors C backend)
 */
enum class UftMergeStrategy {
    Majority = 0,      /* Majority voting */
    CrcWins = 1,       /* CRC-OK sectors have priority */
    HighestScore = 2,  /* Highest scored sector wins */
    Latest = 3         /* Last read wins */
};

/**
 * @brief Configuration for merge operation
 */
struct UftMergeConfig {
    UftMergeStrategy strategy = UftMergeStrategy::HighestScore;
    int minAgreements = 2;
    bool preserveWeakBits = true;
    bool preserveTiming = true;
    int maxRevolutions = 10;
};

/**
 * @brief Result for single sector merge
 */
struct UftMergeSectorResult {
    int cylinder;
    int head;
    int sector;
    int sourceRevolution;
    int agreementCount;
    int totalCandidates;
    int score;
    bool crcOk;
    QString reason;
};

/**
 * @brief Result for track merge
 */
struct UftMergeTrackResult {
    int cylinder;
    int head;
    int goodSectors;
    int recoveredSectors;
    int failedSectors;
    int totalScore;
    QList<UftMergeSectorResult> sectors;
};

/**
 * @brief Widget for merge strategy configuration and visualization
 */
class UftMergeStrategyWidget : public QWidget
{
    Q_OBJECT
    
    Q_PROPERTY(UftMergeStrategy strategy READ strategy WRITE setStrategy NOTIFY configChanged)
    Q_PROPERTY(int minAgreements READ minAgreements WRITE setMinAgreements NOTIFY configChanged)

public:
    explicit UftMergeStrategyWidget(QWidget *parent = nullptr);
    ~UftMergeStrategyWidget();
    
    /* Configuration */
    UftMergeConfig config() const;
    void setConfig(const UftMergeConfig &config);
    
    UftMergeStrategy strategy() const;
    void setStrategy(UftMergeStrategy strategy);
    
    int minAgreements() const;
    void setMinAgreements(int min);
    
    bool preserveWeakBits() const;
    void setPreserveWeakBits(bool preserve);
    
    bool preserveTiming() const;
    void setPreserveTiming(bool preserve);
    
    int maxRevolutions() const;
    void setMaxRevolutions(int max);
    
    /* Results */
    void clearResults();
    void addTrackResult(const UftMergeTrackResult &result);
    void updateStatistics();

public slots:
    /**
     * @brief Apply configuration to backend
     */
    void applyConfig();
    
    /**
     * @brief Reset to defaults
     */
    void resetDefaults();

signals:
    void configChanged();
    void configApplied(const UftMergeConfig &config);

private slots:
    void onStrategyChanged(int index);
    void onSpinValueChanged(int value);
    void onCheckChanged(bool checked);

private:
    void setupUi();
    void updatePreview();
    QString strategyDescription(UftMergeStrategy strategy) const;
    
    /* Configuration widgets */
    QGroupBox *m_configGroup = nullptr;
    QComboBox *m_strategyCombo = nullptr;
    QSpinBox *m_minAgreementsSpin = nullptr;
    QSpinBox *m_maxRevolutionsSpin = nullptr;
    QCheckBox *m_preserveWeakCheck = nullptr;
    QCheckBox *m_preserveTimingCheck = nullptr;
    QLabel *m_strategyDescLabel = nullptr;
    
    /* Results widgets */
    QGroupBox *m_resultsGroup = nullptr;
    QTableWidget *m_resultsTable = nullptr;
    QLabel *m_goodSectorsLabel = nullptr;
    QLabel *m_recoveredLabel = nullptr;
    QLabel *m_failedLabel = nullptr;
    QProgressBar *m_successBar = nullptr;
    
    /* Actions */
    QPushButton *m_applyButton = nullptr;
    QPushButton *m_resetButton = nullptr;
    
    /* State */
    QList<UftMergeTrackResult> m_results;
    int m_totalGood = 0;
    int m_totalRecovered = 0;
    int m_totalFailed = 0;
};

#endif /* UFT_MERGE_STRATEGY_WIDGET_H */

/**
 * @file uft_recovery_panel.h
 * @brief Data Recovery Configuration Panel for UnifiedFloppyTool
 * @version 3.3.2
 * @date 2025-01-03
 *
 * Features:
 * - 7 recovery presets (Quick to Forensic)
 * - Multi-pass retry configuration
 * - CRC correction settings
 * - Weak bit handling
 * - Progress visualization
 */

#ifndef UFT_RECOVERY_PANEL_H
#define UFT_RECOVERY_PANEL_H

#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QProgressBar>
#include <QVector>

/**
 * @brief Recovery strategy modes
 */
enum class RecoveryStrategy {
    Quick = 0,      // Single pass, fast
    Standard,       // Multiple passes, balanced
    Thorough,       // Many passes, slow
    Forensic,       // Maximum effort, very slow
    WeakBitFocus,   // Focus on weak bits
    CRCFocus        // Focus on CRC correction
};

/**
 * @brief Recovery result for a single sector
 */
struct RecoveryResult {
    int  track;
    int  head;
    int  sector;
    bool recovered;
    int  passes;
    int  bitsFixed;
    double confidence;
};

/**
 * @brief Data Recovery Configuration Panel Widget
 *
 * Provides comprehensive control over recovery parameters
 * with real-time progress feedback.
 */
class UftRecoveryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftRecoveryPanel(QWidget* parent = nullptr);
    ~UftRecoveryPanel() override;

    // --- Parameter Access ---

    /**
     * @brief Get maximum retry count
     */
    int maxRetries() const;
    void setMaxRetries(int retries);

    /**
     * @brief Get maximum CRC bits to correct
     */
    int maxCRCBits() const;
    void setMaxCRCBits(int bits);

    /**
     * @brief Get weak bit threshold
     */
    double weakBitThreshold() const;
    void setWeakBitThreshold(double threshold);

    /**
     * @brief Get minimum confidence level
     */
    double minConfidence() const;
    void setMinConfidence(double confidence);

    /**
     * @brief Is multi-revolution fusion enabled?
     */
    bool multiRevEnabled() const;
    void setMultiRevEnabled(bool enabled);

    /**
     * @brief Get number of revolutions to use
     */
    int revolutionCount() const;
    void setRevolutionCount(int count);

    /**
     * @brief Is weak bit interpolation enabled?
     */
    bool weakBitInterpolation() const;
    void setWeakBitInterpolation(bool enabled);

    /**
     * @brief Is CRC brute-force enabled?
     */
    bool crcBruteForce() const;
    void setCrcBruteForce(bool enabled);

    /**
     * @brief Get current recovery strategy
     */
    RecoveryStrategy strategy() const;
    void setStrategy(RecoveryStrategy strategy);

    // --- Preset Management ---

    int currentPreset() const;
    void setPreset(int index);
    void setPresetByName(const QString& name);
    QStringList presetNames() const;

    // --- Status ---

    void setProgress(int current, int total);
    void setCurrentTrack(int track, int head);
    void addResult(const RecoveryResult& result);
    void clearResults();

    // --- Statistics ---

    int totalSectors() const;
    int recoveredSectors() const;
    int failedSectors() const;
    double averageConfidence() const;

signals:
    void parametersChanged();
    void presetChanged(int index);
    void startRequested();
    void stopRequested();
    void pauseRequested();

public slots:
    void start();
    void stop();
    void pause();
    void resetStatistics();

private slots:
    void onPresetChanged(int index);
    void onParameterChanged();
    void updateStatistics();

private:
    void setupUI();
    void setupPresets();
    void loadPreset(int index);
    void connectSignals();

    // --- UI Elements ---

    // Preset selection
    QComboBox*      m_presetCombo;
    QLabel*         m_presetDescription;

    // Retry settings
    QGroupBox*      m_retryGroup;
    QSpinBox*       m_maxRetriesSpin;
    QSpinBox*       m_revolutionCountSpin;
    QCheckBox*      m_multiRevCheck;

    // CRC settings
    QGroupBox*      m_crcGroup;
    QSpinBox*       m_maxCRCBitsSpin;
    QCheckBox*      m_crcBruteForceCheck;

    // Weak bit settings
    QGroupBox*      m_weakBitGroup;
    QDoubleSpinBox* m_weakThresholdSpin;
    QCheckBox*      m_weakInterpolationCheck;

    // Quality settings
    QGroupBox*      m_qualityGroup;
    QDoubleSpinBox* m_minConfidenceSpin;

    // Progress display
    QGroupBox*      m_progressGroup;
    QProgressBar*   m_progressBar;
    QLabel*         m_currentTrackLabel;
    QLabel*         m_statusLabel;

    // Statistics display
    QGroupBox*      m_statsGroup;
    QLabel*         m_totalLabel;
    QLabel*         m_recoveredLabel;
    QLabel*         m_failedLabel;
    QLabel*         m_confidenceLabel;

    // Buttons
    QPushButton*    m_startButton;
    QPushButton*    m_stopButton;
    QPushButton*    m_pauseButton;

    // --- Internal State ---

    struct RecoveryPreset {
        QString name;
        QString description;
        int     maxRetries;
        int     maxCRCBits;
        double  weakThreshold;
        double  minConfidence;
        bool    multiRev;
        int     revCount;
        bool    weakInterpolation;
        bool    crcBruteForce;
    };

    QVector<RecoveryPreset>  m_presets;
    QVector<RecoveryResult>  m_results;
    int  m_currentPresetIndex;
    bool m_blockSignals;
    bool m_isRunning;
    bool m_isPaused;
    int  m_progressCurrent;
    int  m_progressTotal;
};

#endif // UFT_RECOVERY_PANEL_H

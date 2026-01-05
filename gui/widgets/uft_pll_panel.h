/**
 * @file uft_pll_panel.h
 * @brief PLL Configuration Panel for UnifiedFloppyTool
 * @version 3.3.2
 * @date 2025-01-03
 *
 * Features:
 * - Preset selection (16 presets)
 * - Real-time parameter adjustment
 * - Visual PLL status feedback
 * - Live preview graph
 */

#ifndef UFT_PLL_PANEL_H
#define UFT_PLL_PANEL_H

#include <QWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QGroupBox>
#include <QCheckBox>
#include <QPushButton>
#include <QVector>
#include <QTimer>

// Forward declarations
class QCustomPlot;  // Optional: for live graph

/**
 * @brief PLL Preset categories for combo box
 */
enum class PLLPresetCategory {
    General = 0,    // Default, Aggressive, Conservative, Forensic
    Platform,       // IBM, Amiga, Atari, C64, Apple, Mac
    Hardware        // Greaseweazle, KryoFlux, FluxEngine, SCP
};

/**
 * @brief PLL Configuration Panel Widget
 *
 * Provides full control over PLL parameters with presets
 * and real-time visualization.
 */
class UftPLLPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftPLLPanel(QWidget* parent = nullptr);
    ~UftPLLPanel() override;

    // --- Parameter Access ---
    
    /**
     * @brief Get current PLL gain (Kp)
     */
    double gain() const;
    void setGain(double gain);

    /**
     * @brief Get integral gain (Ki)
     */
    double integralGain() const;
    void setIntegralGain(double ki);

    /**
     * @brief Get lock threshold (cycles)
     */
    double lockThreshold() const;
    void setLockThreshold(double threshold);

    /**
     * @brief Get bit cell tolerance (percentage)
     */
    double bitCellTolerance() const;
    void setBitCellTolerance(double tolerance);

    /**
     * @brief Get maximum frequency deviation
     */
    double maxFreqDeviation() const;
    void setMaxFreqDeviation(double deviation);

    /**
     * @brief Get window size (transitions)
     */
    int windowSize() const;
    void setWindowSize(int size);

    /**
     * @brief Get sync pattern for format
     */
    uint32_t syncPattern() const;
    void setSyncPattern(uint32_t pattern);

    /**
     * @brief Get minimum sync bits required
     */
    int minSyncBits() const;
    void setMinSyncBits(int bits);

    /**
     * @brief Is adaptive mode enabled?
     */
    bool adaptiveEnabled() const;
    void setAdaptiveEnabled(bool enabled);

    // --- Preset Management ---

    /**
     * @brief Get current preset index
     */
    int currentPreset() const;
    
    /**
     * @brief Set preset by index
     */
    void setPreset(int index);

    /**
     * @brief Set preset by name
     */
    void setPresetByName(const QString& name);

    /**
     * @brief Get all preset names
     */
    QStringList presetNames() const;

    // --- Status ---

    /**
     * @brief Update PLL lock status display
     */
    void setLockStatus(bool locked, double frequency);

    /**
     * @brief Update phase error display
     */
    void setPhaseError(double error);

    /**
     * @brief Update jitter display
     */
    void setJitter(double jitter);

signals:
    /**
     * @brief Emitted when any parameter changes
     */
    void parametersChanged();

    /**
     * @brief Emitted when preset selection changes
     */
    void presetChanged(int index);

    /**
     * @brief Emitted when user requests apply
     */
    void applyRequested();

    /**
     * @brief Emitted when user requests reset to defaults
     */
    void resetRequested();

public slots:
    /**
     * @brief Apply current parameters
     */
    void apply();

    /**
     * @brief Reset to default preset
     */
    void resetToDefaults();

    /**
     * @brief Load preset from JSON file
     */
    void loadPresetFromFile(const QString& path);

    /**
     * @brief Save current settings to JSON file
     */
    void savePresetToFile(const QString& path);

    /**
     * @brief Update live graph with new flux data
     */
    void updateLiveGraph(const QVector<double>& fluxTimes);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onPresetChanged(int index);
    void onParameterChanged();
    void onAdaptiveToggled(bool checked);
    void updateStatusDisplay();

private:
    void setupUI();
    void setupPresets();
    void loadPreset(int index);
    void updateUIFromParams();
    void connectSignals();

    // --- UI Elements ---
    
    // Preset selection
    QComboBox*      m_presetCombo;
    QLabel*         m_presetDescription;
    
    // Core parameters
    QGroupBox*      m_coreGroup;
    QDoubleSpinBox* m_gainSpin;
    QDoubleSpinBox* m_integralGainSpin;
    QDoubleSpinBox* m_lockThresholdSpin;
    QDoubleSpinBox* m_bitCellToleranceSpin;
    
    // Advanced parameters
    QGroupBox*      m_advancedGroup;
    QDoubleSpinBox* m_maxFreqDeviationSpin;
    QSpinBox*       m_windowSizeSpin;
    QSpinBox*       m_syncPatternSpin;
    QSpinBox*       m_minSyncBitsSpin;
    QCheckBox*      m_adaptiveCheck;
    
    // Status display
    QGroupBox*      m_statusGroup;
    QLabel*         m_lockStatusLabel;
    QLabel*         m_frequencyLabel;
    QLabel*         m_phaseErrorLabel;
    QLabel*         m_jitterLabel;
    
    // Buttons
    QPushButton*    m_applyButton;
    QPushButton*    m_resetButton;
    QPushButton*    m_loadButton;
    QPushButton*    m_saveButton;
    
    // Live graph (optional)
    QWidget*        m_graphWidget;
    
    // Status timer
    QTimer*         m_statusTimer;
    
    // --- Internal State ---
    
    struct PLLPreset {
        QString name;
        QString description;
        double  gain;
        double  integralGain;
        double  lockThreshold;
        double  bitCellTolerance;
        double  maxFreqDeviation;
        int     windowSize;
        uint32_t syncPattern;
        int     minSyncBits;
        bool    adaptive;
    };
    
    QVector<PLLPreset> m_presets;
    int m_currentPresetIndex;
    bool m_blockSignals;
    
    // Status values
    bool   m_isLocked;
    double m_currentFreq;
    double m_phaseError;
    double m_jitter;
};

#endif // UFT_PLL_PANEL_H

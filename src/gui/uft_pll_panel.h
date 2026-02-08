/**
 * @file uft_pll_panel.h
 * @brief PLL Configuration Panel for UFT GUI
 * @version 3.3.2
 * @date 2025-01-03
 *
 * Provides interactive PLL (Phase-Locked Loop) parameter configuration
 * for flux decoding. Integrates with the preset system and supports
 * real-time visualization of PLL behavior.
 */

#ifndef UFT_PLL_PANEL_H
#define UFT_PLL_PANEL_H

#include <QWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <memory>

QT_BEGIN_NAMESPACE
class QCustomPlot;  // Optional: For visualization
QT_END_NAMESPACE

/**
 * @class UftPllPanel
 * @brief Interactive PLL parameter configuration panel
 *
 * Features:
 * - Preset selection with instant parameter update
 * - Manual parameter tuning with sliders
 * - Real-time lock indicator
 * - Histogram of bit cell timing
 * - Export/Import of custom configurations
 */
class UftPllPanel : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief PLL preset types matching uft_pll_preset_id_t
     */
    enum class PllPreset {
        Default = 0,
        Aggressive,
        Conservative,
        Forensic,
        IbmPcDd,
        IbmPcHd,
        AmigaDd,
        AmigaHd,
        AtariSt,
        C64,
        AppleIi,
        MacGcr,
        Greaseweazle,
        KryoFlux,
        FluxEngine,
        Scp
    };
    Q_ENUM(PllPreset)

    /**
     * @brief PLL filter type
     */
    enum class FilterType {
        FirstOrder = 0,
        SecondOrder,
        PiLoop,
        Adaptive
    };
    Q_ENUM(FilterType)

    /**
     * @brief Current PLL parameters (mirrors C struct)
     */
    struct PllParams {
        double  clockRate;          ///< Base clock rate (Hz)
        double  bitCellTime;        ///< Expected bit cell time (ns)
        double  tolerance;          ///< Tolerance (0.0-1.0)
        double  pGain;              ///< Proportional gain
        double  iGain;              ///< Integral gain
        double  dGain;              ///< Derivative gain (PI-D)
        int     filterOrder;        ///< Filter order (1 or 2)
        int     lockThreshold;      ///< Consecutive good bits for lock
        int     unlockThreshold;    ///< Consecutive bad bits for unlock
        int     historyDepth;       ///< Transition history depth
        bool    adaptiveMode;       ///< Enable adaptive gain
        bool    weakBitDetect;      ///< Enable weak bit detection
        double  weakBitThreshold;   ///< Weak bit variance threshold
    };

    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit UftPllPanel(QWidget* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~UftPllPanel() override;

    /**
     * @brief Get current parameters
     * @return Current PLL parameters
     */
    PllParams getParams() const;

    /**
     * @brief Set parameters
     * @param params PLL parameters to apply
     */
    void setParams(const PllParams& params);

    /**
     * @brief Load preset by ID
     * @param preset Preset ID
     */
    void loadPreset(PllPreset preset);

    /**
     * @brief Export parameters to JSON
     * @return JSON object with parameters
     */
    QJsonObject exportToJson() const;

    /**
     * @brief Import parameters from JSON
     * @param json JSON object with parameters
     * @return true on success
     */
    bool importFromJson(const QJsonObject& json);

    /**
     * @brief Update lock status indicator
     * @param locked true if PLL is locked
     * @param lockQuality Lock quality (0-100)
     */
    void updateLockStatus(bool locked, int lockQuality);

    /**
     * @brief Update timing histogram
     * @param timings Array of bit cell timings (ns)
     * @param count Number of timings
     */
    void updateHistogram(const double* timings, size_t count);

signals:
    /**
     * @brief Emitted when parameters change
     * @param params New parameters
     */
    void paramsChanged(const PllParams& params);

    /**
     * @brief Emitted when preset is selected
     * @param preset Selected preset
     */
    void presetSelected(PllPreset preset);

    /**
     * @brief Emitted when export is requested
     */
    void exportRequested();

    /**
     * @brief Emitted when import is requested
     */
    void importRequested();

    /**
     * @brief Emitted when reset to defaults is requested
     */
    void resetRequested();

public slots:
    /**
     * @brief Reset all parameters to defaults
     */
    void resetToDefaults();

    /**
     * @brief Enable/disable real-time updates
     * @param enabled Enable state
     */
    void setRealTimeUpdates(bool enabled);

private slots:
    void onPresetChanged(int index);
    void onClockRateChanged(double value);
    void onBitCellChanged(double value);
    void onToleranceChanged(int value);
    void onPGainChanged(int value);
    void onIGainChanged(int value);
    void onDGainChanged(int value);
    void onFilterTypeChanged(int index);
    void onLockThresholdChanged(int value);
    void onUnlockThresholdChanged(int value);
    void onHistoryDepthChanged(int value);
    void onAdaptiveModeChanged(bool checked);
    void onWeakBitDetectChanged(bool checked);
    void onWeakBitThresholdChanged(int value);
    void onExportClicked();
    void onImportClicked();
    void onResetClicked();
    void emitParamsChanged();

private:
    void setupUi();
    void setupPresetGroup();
    void setupClockGroup();
    void setupGainGroup();
    void setupFilterGroup();
    void setupLockGroup();
    void setupWeakBitGroup();
    void setupVisualizationGroup();
    void setupButtonBar();
    void connectSignals();
    void updateSliderLabels();
    void updateFromParams(const PllParams& params);

    // Preset group
    QGroupBox*      m_presetGroup;
    QComboBox*      m_presetCombo;
    QLabel*         m_presetDesc;

    // Clock parameters
    QGroupBox*      m_clockGroup;
    QComboBox*      m_clockRateCombo;
    QDoubleSpinBox* m_clockRateSpin;
    QDoubleSpinBox* m_bitCellSpin;
    QLabel*         m_bitRateLabel;

    // Gain parameters
    QGroupBox*      m_gainGroup;
    QSlider*        m_pGainSlider;
    QSlider*        m_iGainSlider;
    QSlider*        m_dGainSlider;
    QLabel*         m_pGainLabel;
    QLabel*         m_iGainLabel;
    QLabel*         m_dGainLabel;

    // Filter parameters
    QGroupBox*      m_filterGroup;
    QComboBox*      m_filterTypeCombo;
    QSpinBox*       m_historyDepthSpin;
    QCheckBox*      m_adaptiveCheck;
    QSlider*        m_toleranceSlider;
    QLabel*         m_toleranceLabel;

    // Lock detection
    QGroupBox*      m_lockGroup;
    QSpinBox*       m_lockThresholdSpin;
    QSpinBox*       m_unlockThresholdSpin;
    QFrame*         m_lockIndicator;
    QLabel*         m_lockQualityLabel;

    // Weak bit detection
    QGroupBox*      m_weakBitGroup;
    QCheckBox*      m_weakBitCheck;
    QSlider*        m_weakBitSlider;
    QLabel*         m_weakBitLabel;

    // Visualization
    QGroupBox*      m_visGroup;
    QTableWidget*   m_statsTable;
    QWidget*        m_histogramWidget;

    // Buttons
    QPushButton*    m_exportBtn;
    QPushButton*    m_importBtn;
    QPushButton*    m_resetBtn;

    // State
    PllParams       m_params;
    bool            m_realTimeUpdates;
    QTimer*         m_updateTimer;

    // Preset data
    static const char* presetNames[];
    static const char* presetDescriptions[];
};

#endif /* UFT_PLL_PANEL_H */

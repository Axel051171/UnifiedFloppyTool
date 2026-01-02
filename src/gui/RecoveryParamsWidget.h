// SPDX-License-Identifier: MIT
/*
 * RecoveryParamsWidget.h - Qt Widget for Recovery Parameters
 * 
 * Ready-to-use Qt widget that automatically generates controls
 * from the recovery_params.h definitions.
 * 
 * Usage:
 *   RecoveryParamsWidget *widget = new RecoveryParamsWidget(parent);
 *   widget->loadPreset(PRESET_AMIGA_DAMAGED);
 *   // ... user edits ...
 *   recovery_config_t config = widget->getConfig();
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#ifndef RECOVERYPARAMSWIDGET_H
#define RECOVERYPARAMSWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QMap>

extern "C" {
#include "uft/recovery_params.h"
}

class RecoveryParamsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecoveryParamsWidget(QWidget *parent = nullptr);
    ~RecoveryParamsWidget();

    // Get/Set configuration
    recovery_config_t getConfig() const;
    void setConfig(const recovery_config_t &config);

    // Presets
    void loadPreset(recovery_preset_t preset);
    recovery_preset_t currentPreset() const { return m_currentPreset; }

signals:
    void configChanged();
    void presetChanged(int preset);

public slots:
    void resetToDefaults();
    void saveToFile();
    void loadFromFile();

private slots:
    void onValueChanged();
    void onPresetSelected(int index);

private:
    void setupUI();
    void createTimingGroup(QFormLayout *layout);
    void createAdaptiveGroup(QFormLayout *layout);
    void createPLLGroup(QFormLayout *layout);
    void createErrorCorrectionGroup(QFormLayout *layout);
    void createRetryGroup(QFormLayout *layout);
    void createAnalysisGroup(QFormLayout *layout);
    void updateWidgetsFromConfig();
    void updateConfigFromWidgets();

    // Current configuration
    recovery_config_t m_config;
    recovery_preset_t m_currentPreset;

    // Widget references for easy access
    QTabWidget *m_tabWidget;
    QComboBox *m_presetCombo;

    // Timing widgets
    QSpinBox *m_timing4us;
    QSpinBox *m_timing6us;
    QSpinBox *m_timing8us;
    QSlider *m_thresholdOffset;
    QLabel *m_offsetLabel;
    QCheckBox *m_highDensity;

    // Adaptive widgets
    QCheckBox *m_adaptiveEnabled;
    QDoubleSpinBox *m_rateOfChange;
    QSpinBox *m_lowpassRadius;
    QSpinBox *m_warmupSamples;
    QSpinBox *m_maxDrift;
    QCheckBox *m_lockOnSuccess;

    // PLL widgets
    QCheckBox *m_pllEnabled;
    QDoubleSpinBox *m_pllGain;
    QDoubleSpinBox *m_phaseTolerance;
    QDoubleSpinBox *m_freqTolerance;
    QCheckBox *m_resetOnSync;
    QCheckBox *m_softPll;

    // Error correction widgets
    QCheckBox *m_ecEnabled;
    QSpinBox *m_maxBitFlips;
    QSpinBox *m_searchRegion;
    QSpinBox *m_ecTimeout;
    QCheckBox *m_trySingleFirst;
    QCheckBox *m_useMultiCapture;
    QSpinBox *m_minCaptures;

    // Retry widgets
    QSpinBox *m_maxRetries;
    QSpinBox *m_retryDelay;
    QCheckBox *m_seekRetry;
    QSpinBox *m_seekDistance;
    QCheckBox *m_varySpeed;
    QDoubleSpinBox *m_speedVariation;
    QCheckBox *m_progressiveRelax;

    // Analysis widgets
    QCheckBox *m_generateHistogram;
    QCheckBox *m_generateEntropy;
    QCheckBox *m_generateScatter;
    QComboBox *m_logLevel;
    QCheckBox *m_saveRawFlux;
};

#endif // RECOVERYPARAMSWIDGET_H

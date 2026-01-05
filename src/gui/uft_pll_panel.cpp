/**
 * @file uft_pll_panel.cpp
 * @brief PLL Configuration Panel Implementation
 * @version 3.3.2
 * @date 2025-01-03
 */

#include "uft_pll_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QFile>
#include <QPainter>
#include <cmath>

/*============================================================================
 * STATIC DATA
 *============================================================================*/

const char* UftPllPanel::presetNames[] = {
    "Default",
    "Aggressive",
    "Conservative", 
    "Forensic",
    "IBM PC DD",
    "IBM PC HD",
    "Amiga DD",
    "Amiga HD",
    "Atari ST",
    "Commodore 64",
    "Apple II",
    "Macintosh GCR",
    "Greaseweazle",
    "KryoFlux",
    "FluxEngine",
    "SCP"
};

const char* UftPllPanel::presetDescriptions[] = {
    "Balanced settings for general use",
    "Fast lock, higher jitter tolerance",
    "Slow lock, low jitter, high accuracy",
    "Maximum accuracy for damaged disks",
    "250 Kbit/s MFM (5.25\" DD, 3.5\" DD)",
    "500 Kbit/s MFM (3.5\" HD, 5.25\" HD)",
    "250 Kbit/s MFM (Amiga DD)",
    "500 Kbit/s MFM (Amiga HD)",
    "250 Kbit/s MFM (Atari ST)",
    "300 RPM GCR (1541/1571)",
    "125 Kbit/s GCR (5.25\")",
    "394/590 Kbit/s GCR (400K/800K)",
    "Optimized for Greaseweazle hardware",
    "Optimized for KryoFlux hardware",
    "Optimized for FluxEngine hardware",
    "Optimized for SCP files"
};

/*============================================================================
 * PRESET PARAMETER VALUES
 *============================================================================*/

static const UftPllPanel::PllParams presetParams[] = {
    // Default
    { 24000000.0, 2000.0, 0.15, 0.08, 0.004, 0.0, 1, 6, 3, 8, false, true, 0.15 },
    // Aggressive
    { 24000000.0, 2000.0, 0.25, 0.15, 0.008, 0.001, 1, 4, 2, 4, true, true, 0.20 },
    // Conservative
    { 24000000.0, 2000.0, 0.10, 0.04, 0.002, 0.0, 2, 10, 5, 16, false, true, 0.10 },
    // Forensic
    { 24000000.0, 2000.0, 0.05, 0.02, 0.001, 0.0, 2, 16, 8, 32, false, true, 0.05 },
    // IBM PC DD (250 Kbit/s)
    { 24000000.0, 4000.0, 0.15, 0.08, 0.004, 0.0, 1, 6, 3, 8, false, true, 0.15 },
    // IBM PC HD (500 Kbit/s)
    { 24000000.0, 2000.0, 0.15, 0.08, 0.004, 0.0, 1, 6, 3, 8, false, true, 0.15 },
    // Amiga DD (250 Kbit/s)
    { 24000000.0, 4000.0, 0.12, 0.06, 0.003, 0.0, 1, 8, 4, 12, false, true, 0.12 },
    // Amiga HD (500 Kbit/s)
    { 24000000.0, 2000.0, 0.12, 0.06, 0.003, 0.0, 1, 8, 4, 12, false, true, 0.12 },
    // Atari ST (250 Kbit/s)
    { 24000000.0, 4000.0, 0.15, 0.08, 0.004, 0.0, 1, 6, 3, 8, false, true, 0.15 },
    // C64 (GCR)
    { 16000000.0, 3333.0, 0.18, 0.10, 0.005, 0.0, 1, 5, 3, 6, true, true, 0.18 },
    // Apple II (GCR)
    { 16000000.0, 4000.0, 0.20, 0.12, 0.006, 0.0, 1, 5, 3, 6, true, true, 0.20 },
    // Mac GCR (variable rate)
    { 20000000.0, 2000.0, 0.18, 0.10, 0.005, 0.0, 1, 6, 3, 8, true, true, 0.15 },
    // Greaseweazle
    { 24000000.0, 2000.0, 0.15, 0.08, 0.004, 0.0, 1, 6, 3, 8, false, true, 0.15 },
    // KryoFlux
    { 24027428.0, 2000.0, 0.15, 0.08, 0.004, 0.0, 1, 6, 3, 8, false, true, 0.15 },
    // FluxEngine
    { 24000000.0, 2000.0, 0.15, 0.08, 0.004, 0.0, 1, 6, 3, 8, false, true, 0.15 },
    // SCP
    { 40000000.0, 2000.0, 0.15, 0.08, 0.004, 0.0, 1, 6, 3, 8, false, true, 0.15 }
};

/*============================================================================
 * CONSTRUCTOR / DESTRUCTOR
 *============================================================================*/

UftPllPanel::UftPllPanel(QWidget* parent)
    : QWidget(parent)
    , m_realTimeUpdates(true)
    , m_updateTimer(new QTimer(this))
{
    // Initialize with default params
    m_params = presetParams[0];
    
    setupUi();
    connectSignals();
    
    // Debounce timer for real-time updates
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(100);
    connect(m_updateTimer, &QTimer::timeout, this, &UftPllPanel::emitParamsChanged);
    
    // Load default preset
    loadPreset(PllPreset::Default);
}

UftPllPanel::~UftPllPanel() = default;

/*============================================================================
 * UI SETUP
 *============================================================================*/

void UftPllPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    
    // Top row: Preset + Clock
    auto* topRow = new QHBoxLayout();
    setupPresetGroup();
    setupClockGroup();
    topRow->addWidget(m_presetGroup);
    topRow->addWidget(m_clockGroup);
    mainLayout->addLayout(topRow);
    
    // Middle row: Gain + Filter
    auto* midRow = new QHBoxLayout();
    setupGainGroup();
    setupFilterGroup();
    midRow->addWidget(m_gainGroup);
    midRow->addWidget(m_filterGroup);
    mainLayout->addLayout(midRow);
    
    // Bottom row: Lock + WeakBit
    auto* botRow = new QHBoxLayout();
    setupLockGroup();
    setupWeakBitGroup();
    botRow->addWidget(m_lockGroup);
    botRow->addWidget(m_weakBitGroup);
    mainLayout->addLayout(botRow);
    
    // Visualization
    setupVisualizationGroup();
    mainLayout->addWidget(m_visGroup);
    
    // Buttons
    setupButtonBar();
    
    mainLayout->addStretch();
}

void UftPllPanel::setupPresetGroup()
{
    m_presetGroup = new QGroupBox(tr("Preset"), this);
    auto* layout = new QVBoxLayout(m_presetGroup);
    
    m_presetCombo = new QComboBox();
    for (int i = 0; i < 16; ++i) {
        m_presetCombo->addItem(presetNames[i]);
    }
    
    m_presetDesc = new QLabel(presetDescriptions[0]);
    m_presetDesc->setWordWrap(true);
    m_presetDesc->setStyleSheet("color: gray; font-style: italic;");
    
    layout->addWidget(m_presetCombo);
    layout->addWidget(m_presetDesc);
}

void UftPllPanel::setupClockGroup()
{
    m_clockGroup = new QGroupBox(tr("Clock Parameters"), this);
    auto* layout = new QFormLayout(m_clockGroup);
    
    // Clock rate combo with common values
    m_clockRateCombo = new QComboBox();
    m_clockRateCombo->addItem("16 MHz (C64/Apple)", 16000000.0);
    m_clockRateCombo->addItem("20 MHz (Mac GCR)", 20000000.0);
    m_clockRateCombo->addItem("24 MHz (Standard)", 24000000.0);
    m_clockRateCombo->addItem("24.027428 MHz (KryoFlux)", 24027428.0);
    m_clockRateCombo->addItem("40 MHz (SCP)", 40000000.0);
    m_clockRateCombo->addItem("Custom...", -1.0);
    m_clockRateCombo->setCurrentIndex(2);  // 24 MHz default
    
    m_clockRateSpin = new QDoubleSpinBox();
    m_clockRateSpin->setRange(1000000.0, 100000000.0);
    m_clockRateSpin->setSuffix(" Hz");
    m_clockRateSpin->setDecimals(0);
    m_clockRateSpin->setValue(24000000.0);
    m_clockRateSpin->setVisible(false);
    
    m_bitCellSpin = new QDoubleSpinBox();
    m_bitCellSpin->setRange(500.0, 20000.0);
    m_bitCellSpin->setSuffix(" ns");
    m_bitCellSpin->setDecimals(1);
    m_bitCellSpin->setValue(2000.0);
    
    m_bitRateLabel = new QLabel("500.0 Kbit/s");
    m_bitRateLabel->setStyleSheet("font-weight: bold;");
    
    layout->addRow(tr("Sample Clock:"), m_clockRateCombo);
    layout->addRow("", m_clockRateSpin);
    layout->addRow(tr("Bit Cell:"), m_bitCellSpin);
    layout->addRow(tr("Bit Rate:"), m_bitRateLabel);
}

void UftPllPanel::setupGainGroup()
{
    m_gainGroup = new QGroupBox(tr("PLL Gains"), this);
    auto* layout = new QGridLayout(m_gainGroup);
    
    // P Gain
    layout->addWidget(new QLabel(tr("P (Proportional):")), 0, 0);
    m_pGainSlider = new QSlider(Qt::Horizontal);
    m_pGainSlider->setRange(0, 1000);
    m_pGainSlider->setValue(80);
    layout->addWidget(m_pGainSlider, 0, 1);
    m_pGainLabel = new QLabel("0.080");
    m_pGainLabel->setMinimumWidth(50);
    layout->addWidget(m_pGainLabel, 0, 2);
    
    // I Gain
    layout->addWidget(new QLabel(tr("I (Integral):")), 1, 0);
    m_iGainSlider = new QSlider(Qt::Horizontal);
    m_iGainSlider->setRange(0, 500);
    m_iGainSlider->setValue(40);
    layout->addWidget(m_iGainSlider, 1, 1);
    m_iGainLabel = new QLabel("0.004");
    m_iGainLabel->setMinimumWidth(50);
    layout->addWidget(m_iGainLabel, 1, 2);
    
    // D Gain
    layout->addWidget(new QLabel(tr("D (Derivative):")), 2, 0);
    m_dGainSlider = new QSlider(Qt::Horizontal);
    m_dGainSlider->setRange(0, 100);
    m_dGainSlider->setValue(0);
    layout->addWidget(m_dGainSlider, 2, 1);
    m_dGainLabel = new QLabel("0.000");
    m_dGainLabel->setMinimumWidth(50);
    layout->addWidget(m_dGainLabel, 2, 2);
}

void UftPllPanel::setupFilterGroup()
{
    m_filterGroup = new QGroupBox(tr("Filter Settings"), this);
    auto* layout = new QFormLayout(m_filterGroup);
    
    m_filterTypeCombo = new QComboBox();
    m_filterTypeCombo->addItem(tr("First Order (Simple)"));
    m_filterTypeCombo->addItem(tr("Second Order (Smooth)"));
    m_filterTypeCombo->addItem(tr("PI Loop (Standard)"));
    m_filterTypeCombo->addItem(tr("Adaptive (Auto-tune)"));
    
    m_historyDepthSpin = new QSpinBox();
    m_historyDepthSpin->setRange(2, 64);
    m_historyDepthSpin->setValue(8);
    m_historyDepthSpin->setSuffix(tr(" transitions"));
    
    m_adaptiveCheck = new QCheckBox(tr("Adaptive gain adjustment"));
    
    m_toleranceSlider = new QSlider(Qt::Horizontal);
    m_toleranceSlider->setRange(1, 50);
    m_toleranceSlider->setValue(15);
    
    m_toleranceLabel = new QLabel("15%");
    
    auto* tolLayout = new QHBoxLayout();
    tolLayout->addWidget(m_toleranceSlider);
    tolLayout->addWidget(m_toleranceLabel);
    
    layout->addRow(tr("Filter Type:"), m_filterTypeCombo);
    layout->addRow(tr("History:"), m_historyDepthSpin);
    layout->addRow(tr("Tolerance:"), tolLayout);
    layout->addRow("", m_adaptiveCheck);
}

void UftPllPanel::setupLockGroup()
{
    m_lockGroup = new QGroupBox(tr("Lock Detection"), this);
    auto* layout = new QFormLayout(m_lockGroup);
    
    m_lockThresholdSpin = new QSpinBox();
    m_lockThresholdSpin->setRange(2, 32);
    m_lockThresholdSpin->setValue(6);
    m_lockThresholdSpin->setSuffix(tr(" good bits"));
    
    m_unlockThresholdSpin = new QSpinBox();
    m_unlockThresholdSpin->setRange(1, 16);
    m_unlockThresholdSpin->setValue(3);
    m_unlockThresholdSpin->setSuffix(tr(" bad bits"));
    
    // Lock indicator
    auto* indicatorLayout = new QHBoxLayout();
    m_lockIndicator = new QFrame();
    m_lockIndicator->setFixedSize(24, 24);
    m_lockIndicator->setFrameStyle(QFrame::StyledPanel);
    m_lockIndicator->setStyleSheet(
        "background-color: #444; border: 2px solid #666; border-radius: 12px;");
    
    m_lockQualityLabel = new QLabel(tr("-- %"));
    m_lockQualityLabel->setMinimumWidth(60);
    
    indicatorLayout->addWidget(m_lockIndicator);
    indicatorLayout->addWidget(new QLabel(tr("Lock:")));
    indicatorLayout->addWidget(m_lockQualityLabel);
    indicatorLayout->addStretch();
    
    layout->addRow(tr("Lock after:"), m_lockThresholdSpin);
    layout->addRow(tr("Unlock after:"), m_unlockThresholdSpin);
    layout->addRow(indicatorLayout);
}

void UftPllPanel::setupWeakBitGroup()
{
    m_weakBitGroup = new QGroupBox(tr("Weak Bit Detection"), this);
    auto* layout = new QVBoxLayout(m_weakBitGroup);
    
    m_weakBitCheck = new QCheckBox(tr("Enable weak bit detection"));
    m_weakBitCheck->setChecked(true);
    
    auto* threshLayout = new QHBoxLayout();
    threshLayout->addWidget(new QLabel(tr("Threshold:")));
    
    m_weakBitSlider = new QSlider(Qt::Horizontal);
    m_weakBitSlider->setRange(1, 50);
    m_weakBitSlider->setValue(15);
    threshLayout->addWidget(m_weakBitSlider);
    
    m_weakBitLabel = new QLabel("0.15");
    m_weakBitLabel->setMinimumWidth(40);
    threshLayout->addWidget(m_weakBitLabel);
    
    layout->addWidget(m_weakBitCheck);
    layout->addLayout(threshLayout);
    layout->addStretch();
}

void UftPllPanel::setupVisualizationGroup()
{
    m_visGroup = new QGroupBox(tr("Statistics"), this);
    auto* layout = new QHBoxLayout(m_visGroup);
    
    m_statsTable = new QTableWidget(4, 2);
    m_statsTable->setHorizontalHeaderLabels({tr("Parameter"), tr("Value")});
    m_statsTable->verticalHeader()->setVisible(false);
    m_statsTable->setMaximumHeight(140);
    
    // Initialize stats
    m_statsTable->setItem(0, 0, new QTableWidgetItem(tr("Expected Period")));
    m_statsTable->setItem(0, 1, new QTableWidgetItem("--"));
    m_statsTable->setItem(1, 0, new QTableWidgetItem(tr("Lock Quality")));
    m_statsTable->setItem(1, 1, new QTableWidgetItem("--"));
    m_statsTable->setItem(2, 0, new QTableWidgetItem(tr("Jitter (RMS)")));
    m_statsTable->setItem(2, 1, new QTableWidgetItem("--"));
    m_statsTable->setItem(3, 0, new QTableWidgetItem(tr("Weak Bits")));
    m_statsTable->setItem(3, 1, new QTableWidgetItem("--"));
    
    m_statsTable->horizontalHeader()->setStretchLastSection(true);
    m_statsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    layout->addWidget(m_statsTable);
    
    // Placeholder for histogram (could use QCustomPlot)
    m_histogramWidget = new QWidget();
    m_histogramWidget->setMinimumSize(200, 120);
    m_histogramWidget->setStyleSheet("background-color: #1a1a1a; border: 1px solid #333;");
    layout->addWidget(m_histogramWidget);
}

void UftPllPanel::setupButtonBar()
{
    auto* buttonLayout = new QHBoxLayout();
    
    m_exportBtn = new QPushButton(tr("Export..."));
    m_importBtn = new QPushButton(tr("Import..."));
    m_resetBtn = new QPushButton(tr("Reset"));
    
    buttonLayout->addWidget(m_exportBtn);
    buttonLayout->addWidget(m_importBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_resetBtn);
    
    static_cast<QVBoxLayout*>(layout())->addLayout(buttonLayout);
}

/*============================================================================
 * SIGNAL CONNECTIONS
 *============================================================================*/

void UftPllPanel::connectSignals()
{
    // Preset
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftPllPanel::onPresetChanged);
    
    // Clock
    connect(m_clockRateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                double val = m_clockRateCombo->itemData(idx).toDouble();
                if (val > 0) {
                    m_clockRateSpin->setValue(val);
                    m_clockRateSpin->setVisible(false);
                    onClockRateChanged(val);
                } else {
                    m_clockRateSpin->setVisible(true);
                }
            });
    connect(m_clockRateSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &UftPllPanel::onClockRateChanged);
    connect(m_bitCellSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &UftPllPanel::onBitCellChanged);
    
    // Gains
    connect(m_pGainSlider, &QSlider::valueChanged, this, &UftPllPanel::onPGainChanged);
    connect(m_iGainSlider, &QSlider::valueChanged, this, &UftPllPanel::onIGainChanged);
    connect(m_dGainSlider, &QSlider::valueChanged, this, &UftPllPanel::onDGainChanged);
    
    // Filter
    connect(m_filterTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftPllPanel::onFilterTypeChanged);
    connect(m_toleranceSlider, &QSlider::valueChanged, this, &UftPllPanel::onToleranceChanged);
    connect(m_historyDepthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftPllPanel::onHistoryDepthChanged);
    connect(m_adaptiveCheck, &QCheckBox::toggled, this, &UftPllPanel::onAdaptiveModeChanged);
    
    // Lock
    connect(m_lockThresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftPllPanel::onLockThresholdChanged);
    connect(m_unlockThresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftPllPanel::onUnlockThresholdChanged);
    
    // Weak bit
    connect(m_weakBitCheck, &QCheckBox::toggled, this, &UftPllPanel::onWeakBitDetectChanged);
    connect(m_weakBitSlider, &QSlider::valueChanged, this, &UftPllPanel::onWeakBitThresholdChanged);
    
    // Buttons
    connect(m_exportBtn, &QPushButton::clicked, this, &UftPllPanel::onExportClicked);
    connect(m_importBtn, &QPushButton::clicked, this, &UftPllPanel::onImportClicked);
    connect(m_resetBtn, &QPushButton::clicked, this, &UftPllPanel::onResetClicked);
}

/*============================================================================
 * SLOT IMPLEMENTATIONS
 *============================================================================*/

void UftPllPanel::onPresetChanged(int index)
{
    if (index >= 0 && index < 16) {
        loadPreset(static_cast<PllPreset>(index));
    }
}

void UftPllPanel::onClockRateChanged(double value)
{
    m_params.clockRate = value;
    // Update bit rate display
    double bitRate = 1000000000.0 / m_params.bitCellTime / 1000.0;
    m_bitRateLabel->setText(QString("%1 Kbit/s").arg(bitRate, 0, 'f', 1));
    if (m_realTimeUpdates) m_updateTimer->start();
}

void UftPllPanel::onBitCellChanged(double value)
{
    m_params.bitCellTime = value;
    double bitRate = 1000000000.0 / value / 1000.0;
    m_bitRateLabel->setText(QString("%1 Kbit/s").arg(bitRate, 0, 'f', 1));
    if (m_realTimeUpdates) m_updateTimer->start();
}

void UftPllPanel::onToleranceChanged(int value)
{
    m_params.tolerance = value / 100.0;
    m_toleranceLabel->setText(QString("%1%").arg(value));
    if (m_realTimeUpdates) m_updateTimer->start();
}

void UftPllPanel::onPGainChanged(int value)
{
    m_params.pGain = value / 1000.0;
    m_pGainLabel->setText(QString::number(m_params.pGain, 'f', 3));
    if (m_realTimeUpdates) m_updateTimer->start();
}

void UftPllPanel::onIGainChanged(int value)
{
    m_params.iGain = value / 10000.0;
    m_iGainLabel->setText(QString::number(m_params.iGain, 'f', 4));
    if (m_realTimeUpdates) m_updateTimer->start();
}

void UftPllPanel::onDGainChanged(int value)
{
    m_params.dGain = value / 10000.0;
    m_dGainLabel->setText(QString::number(m_params.dGain, 'f', 4));
    if (m_realTimeUpdates) m_updateTimer->start();
}

void UftPllPanel::onFilterTypeChanged(int index)
{
    m_params.filterOrder = (index == 1 || index == 3) ? 2 : 1;
    if (m_realTimeUpdates) m_updateTimer->start();
}

void UftPllPanel::onLockThresholdChanged(int value)
{
    m_params.lockThreshold = value;
    if (m_realTimeUpdates) m_updateTimer->start();
}

void UftPllPanel::onUnlockThresholdChanged(int value)
{
    m_params.unlockThreshold = value;
    if (m_realTimeUpdates) m_updateTimer->start();
}

void UftPllPanel::onHistoryDepthChanged(int value)
{
    m_params.historyDepth = value;
    if (m_realTimeUpdates) m_updateTimer->start();
}

void UftPllPanel::onAdaptiveModeChanged(bool checked)
{
    m_params.adaptiveMode = checked;
    if (m_realTimeUpdates) m_updateTimer->start();
}

void UftPllPanel::onWeakBitDetectChanged(bool checked)
{
    m_params.weakBitDetect = checked;
    m_weakBitSlider->setEnabled(checked);
    if (m_realTimeUpdates) m_updateTimer->start();
}

void UftPllPanel::onWeakBitThresholdChanged(int value)
{
    m_params.weakBitThreshold = value / 100.0;
    m_weakBitLabel->setText(QString::number(m_params.weakBitThreshold, 'f', 2));
    if (m_realTimeUpdates) m_updateTimer->start();
}

void UftPllPanel::onExportClicked()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export PLL Configuration"),
        "pll_config.json", tr("JSON Files (*.json)"));
    
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            QJsonDocument doc(exportToJson());
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();
        } else {
            QMessageBox::warning(this, tr("Export Failed"),
                                 tr("Could not write to file."));
        }
    }
    emit exportRequested();
}

void UftPllPanel::onImportClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Import PLL Configuration"),
        QString(), tr("JSON Files (*.json)"));
    
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();
            
            if (importFromJson(doc.object())) {
                QMessageBox::information(this, tr("Import Successful"),
                                         tr("Configuration loaded."));
            } else {
                QMessageBox::warning(this, tr("Import Failed"),
                                     tr("Invalid configuration file."));
            }
        }
    }
    emit importRequested();
}

void UftPllPanel::onResetClicked()
{
    resetToDefaults();
    emit resetRequested();
}

void UftPllPanel::emitParamsChanged()
{
    emit paramsChanged(m_params);
}

/*============================================================================
 * PUBLIC METHODS
 *============================================================================*/

UftPllPanel::PllParams UftPllPanel::getParams() const
{
    return m_params;
}

void UftPllPanel::setParams(const PllParams& params)
{
    m_params = params;
    updateFromParams(params);
}

void UftPllPanel::loadPreset(PllPreset preset)
{
    int idx = static_cast<int>(preset);
    if (idx >= 0 && idx < 16) {
        m_params = presetParams[idx];
        m_presetDesc->setText(presetDescriptions[idx]);
        updateFromParams(m_params);
        emit presetSelected(preset);
        emit paramsChanged(m_params);
    }
}

QJsonObject UftPllPanel::exportToJson() const
{
    QJsonObject obj;
    obj["clockRate"] = m_params.clockRate;
    obj["bitCellTime"] = m_params.bitCellTime;
    obj["tolerance"] = m_params.tolerance;
    obj["pGain"] = m_params.pGain;
    obj["iGain"] = m_params.iGain;
    obj["dGain"] = m_params.dGain;
    obj["filterOrder"] = m_params.filterOrder;
    obj["lockThreshold"] = m_params.lockThreshold;
    obj["unlockThreshold"] = m_params.unlockThreshold;
    obj["historyDepth"] = m_params.historyDepth;
    obj["adaptiveMode"] = m_params.adaptiveMode;
    obj["weakBitDetect"] = m_params.weakBitDetect;
    obj["weakBitThreshold"] = m_params.weakBitThreshold;
    return obj;
}

bool UftPllPanel::importFromJson(const QJsonObject& json)
{
    if (!json.contains("clockRate") || !json.contains("bitCellTime")) {
        return false;
    }
    
    PllParams params;
    params.clockRate = json["clockRate"].toDouble(24000000.0);
    params.bitCellTime = json["bitCellTime"].toDouble(2000.0);
    params.tolerance = json["tolerance"].toDouble(0.15);
    params.pGain = json["pGain"].toDouble(0.08);
    params.iGain = json["iGain"].toDouble(0.004);
    params.dGain = json["dGain"].toDouble(0.0);
    params.filterOrder = json["filterOrder"].toInt(1);
    params.lockThreshold = json["lockThreshold"].toInt(6);
    params.unlockThreshold = json["unlockThreshold"].toInt(3);
    params.historyDepth = json["historyDepth"].toInt(8);
    params.adaptiveMode = json["adaptiveMode"].toBool(false);
    params.weakBitDetect = json["weakBitDetect"].toBool(true);
    params.weakBitThreshold = json["weakBitThreshold"].toDouble(0.15);
    
    setParams(params);
    return true;
}

void UftPllPanel::updateLockStatus(bool locked, int lockQuality)
{
    if (locked) {
        m_lockIndicator->setStyleSheet(
            "background-color: #0a0; border: 2px solid #0f0; border-radius: 12px;");
    } else {
        m_lockIndicator->setStyleSheet(
            "background-color: #a00; border: 2px solid #f00; border-radius: 12px;");
    }
    m_lockQualityLabel->setText(QString("%1%").arg(lockQuality));
    
    // Update stats table
    if (m_statsTable->item(1, 1)) {
        m_statsTable->item(1, 1)->setText(QString("%1%").arg(lockQuality));
    }
}

void UftPllPanel::updateHistogram(const double* timings, size_t count)
{
    if (!timings || count == 0) return;
    
    // Calculate statistics
    double sum = 0.0, sumSq = 0.0;
    for (size_t i = 0; i < count; ++i) {
        sum += timings[i];
        sumSq += timings[i] * timings[i];
    }
    double mean = sum / count;
    double variance = (sumSq / count) - (mean * mean);
    double rmsJitter = std::sqrt(variance);
    
    // Update stats
    if (m_statsTable->item(0, 1)) {
        m_statsTable->item(0, 1)->setText(QString("%1 ns").arg(mean, 0, 'f', 1));
    }
    if (m_statsTable->item(2, 1)) {
        m_statsTable->item(2, 1)->setText(QString("%1 ns").arg(rmsJitter, 0, 'f', 2));
    }
    
    // TODO: Draw histogram in m_histogramWidget
}

void UftPllPanel::resetToDefaults()
{
    m_presetCombo->setCurrentIndex(0);
    loadPreset(PllPreset::Default);
}

void UftPllPanel::setRealTimeUpdates(bool enabled)
{
    m_realTimeUpdates = enabled;
}

/*============================================================================
 * PRIVATE METHODS
 *============================================================================*/

void UftPllPanel::updateFromParams(const PllParams& params)
{
    // Block signals during update
    bool blocked = signalsBlocked();
    blockSignals(true);
    
    // Clock
    m_clockRateSpin->setValue(params.clockRate);
    m_bitCellSpin->setValue(params.bitCellTime);
    
    // Gains
    m_pGainSlider->setValue(static_cast<int>(params.pGain * 1000));
    m_iGainSlider->setValue(static_cast<int>(params.iGain * 10000));
    m_dGainSlider->setValue(static_cast<int>(params.dGain * 10000));
    
    // Filter
    m_filterTypeCombo->setCurrentIndex(params.filterOrder - 1);
    m_toleranceSlider->setValue(static_cast<int>(params.tolerance * 100));
    m_historyDepthSpin->setValue(params.historyDepth);
    m_adaptiveCheck->setChecked(params.adaptiveMode);
    
    // Lock
    m_lockThresholdSpin->setValue(params.lockThreshold);
    m_unlockThresholdSpin->setValue(params.unlockThreshold);
    
    // Weak bit
    m_weakBitCheck->setChecked(params.weakBitDetect);
    m_weakBitSlider->setValue(static_cast<int>(params.weakBitThreshold * 100));
    m_weakBitSlider->setEnabled(params.weakBitDetect);
    
    // Update labels
    updateSliderLabels();
    
    blockSignals(blocked);
}

void UftPllPanel::updateSliderLabels()
{
    m_pGainLabel->setText(QString::number(m_params.pGain, 'f', 3));
    m_iGainLabel->setText(QString::number(m_params.iGain, 'f', 4));
    m_dGainLabel->setText(QString::number(m_params.dGain, 'f', 4));
    m_toleranceLabel->setText(QString("%1%").arg(static_cast<int>(m_params.tolerance * 100)));
    m_weakBitLabel->setText(QString::number(m_params.weakBitThreshold, 'f', 2));
    
    double bitRate = 1000000000.0 / m_params.bitCellTime / 1000.0;
    m_bitRateLabel->setText(QString("%1 Kbit/s").arg(bitRate, 0, 'f', 1));
}

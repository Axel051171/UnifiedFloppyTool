#include "advanceddialogs.h"
#include <QLineEdit>
#include <QToolTip>

// ============================================================================
// PLL ADVANCED DIALOG
// ============================================================================

PLLAdvancedDialog::PLLAdvancedDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Advanced PLL Settings"));
    setMinimumWidth(380);
    setupUi();
}

void PLLAdvancedDialog::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // === Clock Settings ===
    QGroupBox *clockGroup = new QGroupBox(tr("Clock Settings"));
    QGridLayout *clockLayout = new QGridLayout(clockGroup);
    
    clockLayout->addWidget(new QLabel(tr("Clock Rate (MHz):")), 0, 0);
    m_clockRate = new QDoubleSpinBox();
    m_clockRate->setRange(1.0, 100.0);
    m_clockRate->setValue(25.0);
    clockLayout->addWidget(m_clockRate, 0, 1);
    
    clockLayout->addWidget(new QLabel(tr("Bit Cell (µs):")), 1, 0);
    m_bitCell = new QDoubleSpinBox();
    m_bitCell->setRange(0.5, 10.0);
    m_bitCell->setValue(2.0);
    m_bitCell->setDecimals(3);
    clockLayout->addWidget(m_bitCell, 1, 1);
    
    mainLayout->addWidget(clockGroup);
    
    // === Filter Settings ===
    QGroupBox *filterGroup = new QGroupBox(tr("Filter"));
    QGridLayout *filterLayout = new QGridLayout(filterGroup);
    
    filterLayout->addWidget(new QLabel(tr("Type:")), 0, 0);
    m_filterType = new QComboBox();
    m_filterType->addItems({tr("Simple"), tr("PID"), tr("Adaptive")});
    m_filterType->setCurrentIndex(2);
    filterLayout->addWidget(m_filterType, 0, 1);
    
    filterLayout->addWidget(new QLabel(tr("History Depth:")), 1, 0);
    m_historyDepth = new QSpinBox();
    m_historyDepth->setRange(1, 100);
    m_historyDepth->setValue(16);
    filterLayout->addWidget(m_historyDepth, 1, 1);
    
    mainLayout->addWidget(filterGroup);
    
    // === Gain Settings ===
    QGroupBox *gainGroup = new QGroupBox(tr("Gain"));
    QVBoxLayout *gainLayout = new QVBoxLayout(gainGroup);
    
    m_adaptiveGain = new QCheckBox(tr("Adaptive Gain Adjustment"));
    m_adaptiveGain->setChecked(true);
    gainLayout->addWidget(m_adaptiveGain);
    
    QHBoxLayout *tolLayout = new QHBoxLayout();
    tolLayout->addWidget(new QLabel(tr("Tolerance:")));
    m_tolerance = new QSlider(Qt::Horizontal);
    m_tolerance->setRange(1, 50);
    m_tolerance->setValue(15);
    tolLayout->addWidget(m_tolerance);
    m_toleranceLabel = new QLabel("15%");
    m_toleranceLabel->setMinimumWidth(40);
    tolLayout->addWidget(m_toleranceLabel);
    gainLayout->addLayout(tolLayout);
    
    connect(m_tolerance, &QSlider::valueChanged, [this](int v) {
        m_toleranceLabel->setText(QString("%1%").arg(v));
    });
    
    mainLayout->addWidget(gainGroup);
    
    // === Lock Detection ===
    QGroupBox *lockGroup = new QGroupBox(tr("Lock Detection"));
    QGridLayout *lockLayout = new QGridLayout(lockGroup);
    
    lockLayout->addWidget(new QLabel(tr("Lock Threshold (%):")), 0, 0);
    m_lockThreshold = new QSpinBox();
    m_lockThreshold->setRange(1, 100);
    m_lockThreshold->setValue(80);
    lockLayout->addWidget(m_lockThreshold, 0, 1);
    
    lockLayout->addWidget(new QLabel(tr("Unlock Threshold (%):")), 1, 0);
    m_unlockThreshold = new QSpinBox();
    m_unlockThreshold->setRange(1, 100);
    m_unlockThreshold->setValue(50);
    lockLayout->addWidget(m_unlockThreshold, 1, 1);
    
    mainLayout->addWidget(lockGroup);
    
    // === Weak Bit Detection ===
    QGroupBox *weakGroup = new QGroupBox(tr("Weak Bit Detection"));
    QGridLayout *weakLayout = new QGridLayout(weakGroup);
    
    m_weakBitDetection = new QCheckBox(tr("Enable"));
    m_weakBitDetection->setChecked(true);
    weakLayout->addWidget(m_weakBitDetection, 0, 0);
    
    weakLayout->addWidget(new QLabel(tr("Window (bits):")), 0, 1);
    m_weakBitWindow = new QSpinBox();
    m_weakBitWindow->setRange(1, 64);
    m_weakBitWindow->setValue(8);
    weakLayout->addWidget(m_weakBitWindow, 0, 2);
    
    mainLayout->addWidget(weakGroup);
    
    // === Buttons ===
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

PLLAdvancedDialog::PLLAdvancedParams PLLAdvancedDialog::getParams() const {
    PLLAdvancedParams p;
    p.clockRate = m_clockRate->value();
    p.bitCell = m_bitCell->value();
    p.filterType = m_filterType->currentIndex();
    p.historyDepth = m_historyDepth->value();
    p.adaptiveGain = m_adaptiveGain->isChecked();
    p.tolerance = m_tolerance->value();
    p.lockThreshold = m_lockThreshold->value();
    p.unlockThreshold = m_unlockThreshold->value();
    p.weakBitDetection = m_weakBitDetection->isChecked();
    p.weakBitWindow = m_weakBitWindow->value();
    return p;
}

void PLLAdvancedDialog::setParams(const PLLAdvancedParams &p) {
    m_clockRate->setValue(p.clockRate);
    m_bitCell->setValue(p.bitCell);
    m_filterType->setCurrentIndex(p.filterType);
    m_historyDepth->setValue(p.historyDepth);
    m_adaptiveGain->setChecked(p.adaptiveGain);
    m_tolerance->setValue(p.tolerance);
    m_lockThreshold->setValue(p.lockThreshold);
    m_unlockThreshold->setValue(p.unlockThreshold);
    m_weakBitDetection->setChecked(p.weakBitDetection);
    m_weakBitWindow->setValue(p.weakBitWindow);
}

// ============================================================================
// NIBBLE ADVANCED DIALOG
// ============================================================================

NibbleAdvancedDialog::NibbleAdvancedDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Advanced GCR/Nibble Settings"));
    setMinimumWidth(420);
    setupUi();
}

void NibbleAdvancedDialog::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // === GCR Settings ===
    QGroupBox *gcrGroup = new QGroupBox(tr("GCR Decoding"));
    QGridLayout *gcrLayout = new QGridLayout(gcrGroup);
    
    gcrLayout->addWidget(new QLabel(tr("Variant:")), 0, 0);
    m_gcrVariant = new QComboBox();
    m_gcrVariant->addItems({tr("Standard GCR"), tr("Apple II"), tr("C64/1541"), tr("Victor 9000")});
    gcrLayout->addWidget(m_gcrVariant, 0, 1);
    
    m_rawNibble = new QCheckBox(tr("Raw Nibble (no decoding)"));
    gcrLayout->addWidget(m_rawNibble, 1, 0, 1, 2);
    
    m_decodeToSectors = new QCheckBox(tr("Decode to Sectors"));
    m_decodeToSectors->setChecked(true);
    gcrLayout->addWidget(m_decodeToSectors, 2, 0, 1, 2);
    
    mainLayout->addWidget(gcrGroup);
    
    // === Track Options ===
    QGroupBox *trackGroup = new QGroupBox(tr("Track Options"));
    QGridLayout *trackLayout = new QGridLayout(trackGroup);
    
    m_includeHalfTracks = new QCheckBox(tr("Include Half-Tracks"));
    trackLayout->addWidget(m_includeHalfTracks, 0, 0);
    
    m_includeQuarterTracks = new QCheckBox(tr("Include Quarter-Tracks"));
    trackLayout->addWidget(m_includeQuarterTracks, 0, 1);
    
    trackLayout->addWidget(new QLabel(tr("Track Step:")), 1, 0);
    m_trackStep = new QSpinBox();
    m_trackStep->setRange(1, 4);
    m_trackStep->setValue(1);
    trackLayout->addWidget(m_trackStep, 1, 1);
    
    mainLayout->addWidget(trackGroup);
    
    // === Sync Detection ===
    QGroupBox *syncGroup = new QGroupBox(tr("Sync Detection"));
    QGridLayout *syncLayout = new QGridLayout(syncGroup);
    
    syncLayout->addWidget(new QLabel(tr("Sync Pattern (hex):")), 0, 0);
    m_syncPattern = new QLineEdit("FF");
    m_syncPattern->setMaximumWidth(80);
    syncLayout->addWidget(m_syncPattern, 0, 1);
    
    syncLayout->addWidget(new QLabel(tr("Sync Length (bits):")), 0, 2);
    m_syncLength = new QSpinBox();
    m_syncLength->setRange(1, 64);
    m_syncLength->setValue(10);
    syncLayout->addWidget(m_syncLength, 0, 3);
    
    m_autoDetectSync = new QCheckBox(tr("Auto-detect Sync Pattern"));
    m_autoDetectSync->setChecked(true);
    syncLayout->addWidget(m_autoDetectSync, 1, 0, 1, 4);
    
    mainLayout->addWidget(syncGroup);
    
    // === Error Handling ===
    QGroupBox *errorGroup = new QGroupBox(tr("Error Handling"));
    QGridLayout *errorLayout = new QGridLayout(errorGroup);
    
    m_ignoreBadGCR = new QCheckBox(tr("Ignore Bad GCR"));
    errorLayout->addWidget(m_ignoreBadGCR, 0, 0);
    
    m_fillBadSectors = new QCheckBox(tr("Fill Bad Sectors"));
    errorLayout->addWidget(m_fillBadSectors, 0, 1);
    
    errorLayout->addWidget(new QLabel(tr("Fill Byte:")), 1, 0);
    m_fillByte = new QSpinBox();
    m_fillByte->setRange(0, 255);
    m_fillByte->setValue(0);
    m_fillByte->setDisplayIntegerBase(16);
    m_fillByte->setPrefix("0x");
    errorLayout->addWidget(m_fillByte, 1, 1);
    
    mainLayout->addWidget(errorGroup);
    
    // === Output Options ===
    QGroupBox *outputGroup = new QGroupBox(tr("Output"));
    QHBoxLayout *outputLayout = new QHBoxLayout(outputGroup);
    
    m_preserveGaps = new QCheckBox(tr("Preserve Gaps"));
    m_preserveGaps->setChecked(true);
    outputLayout->addWidget(m_preserveGaps);
    
    m_preserveSync = new QCheckBox(tr("Preserve Sync"));
    m_preserveSync->setChecked(true);
    outputLayout->addWidget(m_preserveSync);
    
    mainLayout->addWidget(outputGroup);
    
    // === Buttons ===
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

NibbleAdvancedDialog::NibbleAdvancedParams NibbleAdvancedDialog::getParams() const {
    NibbleAdvancedParams p;
    p.gcrVariant = m_gcrVariant->currentIndex();
    p.rawNibble = m_rawNibble->isChecked();
    p.decodeToSectors = m_decodeToSectors->isChecked();
    p.includeHalfTracks = m_includeHalfTracks->isChecked();
    p.includeQuarterTracks = m_includeQuarterTracks->isChecked();
    p.trackStep = m_trackStep->value();
    p.syncPattern = m_syncPattern->text().toInt(nullptr, 16);
    p.syncLength = m_syncLength->value();
    p.autoDetectSync = m_autoDetectSync->isChecked();
    p.ignoreBadGCR = m_ignoreBadGCR->isChecked();
    p.fillBadSectors = m_fillBadSectors->isChecked();
    p.fillByte = static_cast<uint8_t>(m_fillByte->value());
    p.preserveGaps = m_preserveGaps->isChecked();
    p.preserveSync = m_preserveSync->isChecked();
    return p;
}

void NibbleAdvancedDialog::setParams(const NibbleAdvancedParams &p) {
    m_gcrVariant->setCurrentIndex(p.gcrVariant);
    m_rawNibble->setChecked(p.rawNibble);
    m_decodeToSectors->setChecked(p.decodeToSectors);
    m_includeHalfTracks->setChecked(p.includeHalfTracks);
    m_includeQuarterTracks->setChecked(p.includeQuarterTracks);
    m_trackStep->setValue(p.trackStep);
    m_syncPattern->setText(QString::number(p.syncPattern, 16).toUpper());
    m_syncLength->setValue(p.syncLength);
    m_autoDetectSync->setChecked(p.autoDetectSync);
    m_ignoreBadGCR->setChecked(p.ignoreBadGCR);
    m_fillBadSectors->setChecked(p.fillBadSectors);
    m_fillByte->setValue(p.fillByte);
    m_preserveGaps->setChecked(p.preserveGaps);
    m_preserveSync->setChecked(p.preserveSync);
}

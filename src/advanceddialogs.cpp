#include "advanceddialogs.h"
#include <QLineEdit>
#include <QToolTip>

// ============================================================================
// FLUX ADVANCED DIALOG
// ============================================================================

FluxAdvancedDialog::FluxAdvancedDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Advanced Flux Settings"));
    setMinimumWidth(400);
    setupUi();
}

void FluxAdvancedDialog::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // === PLL Fine-Tuning ===
    QGroupBox *pllGroup = new QGroupBox(tr("PLL Fine-Tuning"));
    QGridLayout *pllLayout = new QGridLayout(pllGroup);
    
    pllLayout->addWidget(new QLabel(tr("Frequency (Hz):")), 0, 0);
    m_pllFrequency = new QDoubleSpinBox();
    m_pllFrequency->setRange(100000, 10000000);
    m_pllFrequency->setValue(2000000);
    m_pllFrequency->setDecimals(0);
    m_pllFrequency->setToolTip(tr(
        "<b>PLL-Frequenz</b><br><br>"
        "Standard-Werte:<br>"
        "• MFM DD: 2.000.000 Hz<br>"
        "• MFM HD: 4.000.000 Hz<br>"
        "• FM: 1.000.000 Hz<br><br>"
        "<i>💡 Niedrigere Werte für beschädigte Disks</i>"));
    pllLayout->addWidget(m_pllFrequency, 0, 1);
    
    pllLayout->addWidget(new QLabel(tr("Bandwidth:")), 0, 2);
    m_pllBandwidth = new QDoubleSpinBox();
    m_pllBandwidth->setRange(0.0, 1.0);
    m_pllBandwidth->setValue(0.5);
    m_pllBandwidth->setSingleStep(0.1);
    m_pllBandwidth->setToolTip(tr(
        "<b>PLL-Bandbreite</b><br><br>"
        "0.0 = Sehr eng (stabil, langsam)<br>"
        "0.5 = Standard (empfohlen)<br>"
        "1.0 = Sehr weit (schnell, instabil)<br><br>"
        "<i>💡 Niedrigere Werte bei Timing-Schwankungen</i>"));
    pllLayout->addWidget(m_pllBandwidth, 0, 3);
    
    pllLayout->addWidget(new QLabel(tr("Phase Adjust:")), 1, 0);
    m_pllPhase = new QDoubleSpinBox();
    m_pllPhase->setRange(-1.0, 1.0);
    m_pllPhase->setValue(0.0);
    m_pllPhase->setSingleStep(0.1);
    m_pllPhase->setToolTip(tr(
        "<b>Phasen-Anpassung</b><br><br>"
        "-1.0 = Früh abtasten<br>"
        " 0.0 = Neutral (Standard)<br>"
        "+1.0 = Spät abtasten<br><br>"
        "<i>💡 Anpassen bei systematischen Bitfehlern</i>"));
    pllLayout->addWidget(m_pllPhase, 1, 1);
    
    pllLayout->addWidget(new QLabel(tr("Lock Threshold (%):")), 1, 2);
    m_pllLockThreshold = new QSpinBox();
    m_pllLockThreshold->setRange(1, 100);
    m_pllLockThreshold->setValue(80);
    m_pllLockThreshold->setToolTip(tr(
        "<b>Lock-Schwellwert</b><br><br>"
        "Prozent der Bits, die korrekt sein müssen:<br>"
        "• 80%+ = Normale Disks<br>"
        "• 50-79% = Beschädigte Disks<br>"
        "• <50% = Stark beschädigt<br><br>"
        "<i>💡 Niedrigere Werte für Recovery-Versuche</i>"));
    pllLayout->addWidget(m_pllLockThreshold, 1, 3);
    
    mainLayout->addWidget(pllGroup);
    
    // === Timing ===
    QGroupBox *timingGroup = new QGroupBox(tr("Timing"));
    QGridLayout *timingLayout = new QGridLayout(timingGroup);
    
    timingLayout->addWidget(new QLabel(tr("Bitcell Period (µs):")), 0, 0);
    m_bitcellPeriod = new QDoubleSpinBox();
    m_bitcellPeriod->setRange(0.5, 10.0);
    m_bitcellPeriod->setValue(2.0);
    m_bitcellPeriod->setDecimals(3);
    m_bitcellPeriod->setToolTip(tr(
        "<b>Bitcell-Periode</b><br><br>"
        "Dauer einer Bitzelle in Mikrosekunden:<br>"
        "• MFM DD (250kbit/s): 2.0 µs<br>"
        "• MFM HD (500kbit/s): 1.0 µs<br>"
        "• FM (125kbit/s): 4.0 µs<br><br>"
        "<i>💡 Formel: 1.000.000 / Bitrate</i>"));
    timingLayout->addWidget(m_bitcellPeriod, 0, 1);
    
    timingLayout->addWidget(new QLabel(tr("Clock Tolerance (%):")), 0, 2);
    m_clockTolerance = new QDoubleSpinBox();
    m_clockTolerance->setRange(1.0, 50.0);
    m_clockTolerance->setValue(10.0);
    m_clockTolerance->setToolTip(tr(
        "<b>Takt-Toleranz</b><br><br>"
        "Akzeptable Abweichung vom Soll-Timing:<br>"
        "• 5-10% = Normale Disks<br>"
        "• 15-25% = Alte/abgenutzte Disks<br>"
        "• >30% = Stark beschädigt<br><br>"
        "<i>💡 Höhere Toleranz = mehr Fehler akzeptiert</i>"));
    timingLayout->addWidget(m_clockTolerance, 0, 3);
    
    mainLayout->addWidget(timingGroup);
    
    // === Revolution Handling ===
    QGroupBox *revGroup = new QGroupBox(tr("Revolution Handling"));
    QGridLayout *revLayout = new QGridLayout(revGroup);
    
    revLayout->addWidget(new QLabel(tr("Revs to Read:")), 0, 0);
    m_revsToRead = new QSpinBox();
    m_revsToRead->setRange(1, 20);
    m_revsToRead->setValue(5);
    m_revsToRead->setToolTip(tr(
        "<b>Umdrehungen lesen</b><br><br>"
        "Anzahl der Disk-Umdrehungen:<br>"
        "• 1-3: Schnell, weniger Daten<br>"
        "• 5: Standard (empfohlen)<br>"
        "• 10+: Recovery, mehr Chancen<br><br>"
        "<i>💡 Mehr Revs = bessere Fehlerkorrektur</i>"));
    revLayout->addWidget(m_revsToRead, 0, 1);
    
    revLayout->addWidget(new QLabel(tr("Revs to Use:")), 0, 2);
    m_revsToUse = new QSpinBox();
    m_revsToUse->setRange(1, 20);
    m_revsToUse->setValue(3);
    m_revsToUse->setToolTip(tr(
        "<b>Umdrehungen verwenden</b><br><br>"
        "Anzahl für die Dekodierung:<br>"
        "• Sollte ≤ 'Revs to Read' sein<br>"
        "• 3: Guter Kompromiss<br>"
        "• 1: Schnellste Verarbeitung<br><br>"
        "<i>💡 Beste Revs werden automatisch gewählt</i>"));
    revLayout->addWidget(m_revsToUse, 0, 3);
    
    m_mergeRevs = new QCheckBox(tr("Merge Revolutions"));
    m_mergeRevs->setChecked(true);
    m_mergeRevs->setToolTip(tr(
        "<b>Umdrehungen zusammenführen</b><br><br>"
        "Kombiniert Daten mehrerer Umdrehungen<br>"
        "für bessere Fehlerkorrektur.<br><br>"
        "<i>💡 Für Recovery empfohlen!</i>"));
    revLayout->addWidget(m_mergeRevs, 1, 0, 1, 2);
    
    revLayout->addWidget(new QLabel(tr("Merge Mode:")), 1, 2);
    m_mergeMode = new QComboBox();
    m_mergeMode->addItems({tr("First"), tr("Best"), tr("All")});
    m_mergeMode->setToolTip(tr(
        "<b>Merge-Modus</b><br><br>"
        "• <b>First</b>: Erste gültige Rev<br>"
        "• <b>Best</b>: Beste Qualität (empfohlen)<br>"
        "• <b>All</b>: Alle kombinieren"));
    revLayout->addWidget(m_mergeMode, 1, 3);
    
    mainLayout->addWidget(revGroup);
    
    // === Detection Thresholds ===
    QGroupBox *threshGroup = new QGroupBox(tr("Detection Thresholds"));
    QGridLayout *threshLayout = new QGridLayout(threshGroup);
    
    threshLayout->addWidget(new QLabel(tr("Weak Bit (%):")), 0, 0);
    m_weakBitThreshold = new QSpinBox();
    m_weakBitThreshold->setRange(1, 100);
    m_weakBitThreshold->setValue(30);
    threshLayout->addWidget(m_weakBitThreshold, 0, 1);
    
    threshLayout->addWidget(new QLabel(tr("No-Flux (µs):")), 0, 2);
    m_noFluxThreshold = new QSpinBox();
    m_noFluxThreshold->setRange(1, 1000);
    m_noFluxThreshold->setValue(100);
    threshLayout->addWidget(m_noFluxThreshold, 0, 3);
    
    mainLayout->addWidget(threshGroup);
    
    // === Index ===
    QGroupBox *indexGroup = new QGroupBox(tr("Index Signal"));
    QGridLayout *indexLayout = new QGridLayout(indexGroup);
    
    m_useIndex = new QCheckBox(tr("Use Index Signal"));
    m_useIndex->setChecked(true);
    indexLayout->addWidget(m_useIndex, 0, 0, 1, 2);
    
    indexLayout->addWidget(new QLabel(tr("Offset (µs):")), 0, 2);
    m_indexOffset = new QDoubleSpinBox();
    m_indexOffset->setRange(-1000, 1000);
    m_indexOffset->setValue(0);
    indexLayout->addWidget(m_indexOffset, 0, 3);
    
    m_softIndex = new QCheckBox(tr("Soft Index (Sector 0)"));
    indexLayout->addWidget(m_softIndex, 1, 0, 1, 4);
    
    mainLayout->addWidget(indexGroup);
    
    // === Buttons ===
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

FluxAdvancedDialog::FluxAdvancedParams FluxAdvancedDialog::getParams() const {
    FluxAdvancedParams p;
    p.pllFrequency = m_pllFrequency->value();
    p.pllBandwidth = m_pllBandwidth->value();
    p.pllPhase = m_pllPhase->value();
    p.pllLockThreshold = m_pllLockThreshold->value();
    p.bitcellPeriod = m_bitcellPeriod->value();
    p.clockTolerance = m_clockTolerance->value();
    p.revsToRead = m_revsToRead->value();
    p.revsToUse = m_revsToUse->value();
    p.mergeRevs = m_mergeRevs->isChecked();
    p.mergeMode = m_mergeMode->currentIndex();
    p.weakBitThreshold = m_weakBitThreshold->value();
    p.noFluxThreshold = m_noFluxThreshold->value();
    p.useIndex = m_useIndex->isChecked();
    p.indexOffset = m_indexOffset->value();
    p.softIndex = m_softIndex->isChecked();
    return p;
}

void FluxAdvancedDialog::setParams(const FluxAdvancedParams &p) {
    m_pllFrequency->setValue(p.pllFrequency);
    m_pllBandwidth->setValue(p.pllBandwidth);
    m_pllPhase->setValue(p.pllPhase);
    m_pllLockThreshold->setValue(p.pllLockThreshold);
    m_bitcellPeriod->setValue(p.bitcellPeriod);
    m_clockTolerance->setValue(p.clockTolerance);
    m_revsToRead->setValue(p.revsToRead);
    m_revsToUse->setValue(p.revsToUse);
    m_mergeRevs->setChecked(p.mergeRevs);
    m_mergeMode->setCurrentIndex(p.mergeMode);
    m_weakBitThreshold->setValue(p.weakBitThreshold);
    m_noFluxThreshold->setValue(p.noFluxThreshold);
    m_useIndex->setChecked(p.useIndex);
    m_indexOffset->setValue(p.indexOffset);
    m_softIndex->setChecked(p.softIndex);
}

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

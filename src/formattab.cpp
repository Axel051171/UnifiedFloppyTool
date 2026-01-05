/**
 * @file formattab.cpp
 * @brief Format/Settings Tab Widget - Implementation
 * 
 * P1-GUI-001: Extended Protection settings integration
 * 
 * @author UFT Team
 * @date 2026-01-05
 */

#include "formattab.h"
#include "ui_tab_format.h"

// ============================================================================
// Construction / Destruction
// ============================================================================

FormatTab::FormatTab(QWidget *parent) 
    : QWidget(parent)
    , ui(new Ui::TabFormat) 
{
    ui->setupUi(this);
    setupConnections();
    loadSettings();
}

FormatTab::~FormatTab() {
    saveSettings();
    delete ui;
}

// ============================================================================
// Protection Settings API
// ============================================================================

uint32_t FormatTab::getProtectionFlags() const {
    uint32_t flags = 0;
    
    if (ui->checkDetectAll->isChecked()) {
        // Detect all = enable everything
        flags = UFT_PROT_ANAL_ALL;
    } else {
        // Individual flags
        if (ui->checkDetectWeakBitsProt->isChecked()) flags |= UFT_PROT_ANAL_WEAK_BITS;
        if (ui->checkDetectLongTracks->isChecked())   flags |= UFT_PROT_ANAL_TIMING;
        if (ui->checkDetectHalfTracks->isChecked())   flags |= UFT_PROT_ANAL_HALF_TRACKS;
        if (ui->checkDetectTiming->isChecked())       flags |= UFT_PROT_ANAL_TIMING;
        if (ui->checkDetectNoFlux->isChecked())       flags |= UFT_PROT_ANAL_WEAK_BITS;
        if (ui->checkDetectCustomSync->isChecked())   flags |= UFT_PROT_ANAL_SIGNATURES;
    }
    
    // Quick scan if nothing selected
    if (flags == 0) {
        flags = UFT_PROT_ANAL_QUICK;
    }
    
    return flags;
}

uft_platform_t FormatTab::getPlatformHint() const {
    int idx = ui->comboPlatform->currentIndex();
    switch (idx) {
        case 0: return UFT_PLATFORM_UNKNOWN;  // Auto
        case 1: return UFT_PLATFORM_C64;
        case 2: return UFT_PLATFORM_AMIGA;
        case 3: return UFT_PLATFORM_ATARI_ST;
        case 4: return UFT_PLATFORM_APPLE_II;
        case 5: return UFT_PLATFORM_PC_DOS;
        default: return UFT_PLATFORM_UNKNOWN;
    }
}

bool FormatTab::isPreserveProtection() const {
    return ui->radioPreserve->isChecked();
}

void FormatTab::getProtectionConfig(uft_prot_config_t* config) const {
    if (!config) return;
    
    uft_prot_config_init(config);
    config->flags = getProtectionFlags();
    config->platform_hint = getPlatformHint();
    config->confidence_threshold = 70;  // Default
}

// ============================================================================
// Slots
// ============================================================================

void FormatTab::onDetectAllToggled(bool checked) {
    syncProtectionWidgets(checked);
    emit protectionSettingsChanged();
}

void FormatTab::onPlatformChanged(int /*index*/) {
    emit protectionSettingsChanged();
}

void FormatTab::onProtectionCheckChanged() {
    emit protectionSettingsChanged();
}

// ============================================================================
// Settings Persistence
// ============================================================================

void FormatTab::loadSettings() {
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);
    
    // Protection
    ui->checkDetectAll->setChecked(settings.value("detectAll", true).toBool());
    ui->checkDetectWeakBitsProt->setChecked(settings.value("detectWeakBits", true).toBool());
    ui->checkDetectLongTracks->setChecked(settings.value("detectLongTracks", true).toBool());
    ui->checkDetectHalfTracks->setChecked(settings.value("detectHalfTracks", false).toBool());
    ui->checkDetectTiming->setChecked(settings.value("detectTiming", false).toBool());
    ui->checkDetectNoFlux->setChecked(settings.value("detectNoFlux", true).toBool());
    ui->checkDetectCustomSync->setChecked(settings.value("detectCustomSync", false).toBool());
    ui->comboPlatform->setCurrentIndex(settings.value("platform", 0).toInt());
    ui->radioPreserve->setChecked(settings.value("preserve", true).toBool());
    ui->radioRemove->setChecked(!settings.value("preserve", true).toBool());
    
    settings.endGroup();
    
    // Sync widget states
    syncProtectionWidgets(ui->checkDetectAll->isChecked());
}

void FormatTab::saveSettings() {
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);
    
    // Protection
    settings.setValue("detectAll", ui->checkDetectAll->isChecked());
    settings.setValue("detectWeakBits", ui->checkDetectWeakBitsProt->isChecked());
    settings.setValue("detectLongTracks", ui->checkDetectLongTracks->isChecked());
    settings.setValue("detectHalfTracks", ui->checkDetectHalfTracks->isChecked());
    settings.setValue("detectTiming", ui->checkDetectTiming->isChecked());
    settings.setValue("detectNoFlux", ui->checkDetectNoFlux->isChecked());
    settings.setValue("detectCustomSync", ui->checkDetectCustomSync->isChecked());
    settings.setValue("platform", ui->comboPlatform->currentIndex());
    settings.setValue("preserve", ui->radioPreserve->isChecked());
    
    settings.endGroup();
    settings.sync();
}

// ============================================================================
// Internal Helpers
// ============================================================================

void FormatTab::setupConnections() {
    // Detect all toggle
    connect(ui->checkDetectAll, &QCheckBox::toggled, 
            this, &FormatTab::onDetectAllToggled);
    
    // Platform combo
    connect(ui->comboPlatform, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onPlatformChanged);
    
    // Individual detection checkboxes
    connect(ui->checkDetectWeakBitsProt, &QCheckBox::toggled, 
            this, &FormatTab::onProtectionCheckChanged);
    connect(ui->checkDetectLongTracks, &QCheckBox::toggled, 
            this, &FormatTab::onProtectionCheckChanged);
    connect(ui->checkDetectHalfTracks, &QCheckBox::toggled, 
            this, &FormatTab::onProtectionCheckChanged);
    connect(ui->checkDetectTiming, &QCheckBox::toggled, 
            this, &FormatTab::onProtectionCheckChanged);
    connect(ui->checkDetectNoFlux, &QCheckBox::toggled, 
            this, &FormatTab::onProtectionCheckChanged);
    connect(ui->checkDetectCustomSync, &QCheckBox::toggled, 
            this, &FormatTab::onProtectionCheckChanged);
    
    // Preserve/Remove radios
    connect(ui->radioPreserve, &QRadioButton::toggled, 
            this, &FormatTab::onProtectionCheckChanged);
}

void FormatTab::syncProtectionWidgets(bool detectAll) {
    // When "Detect all" is checked, disable individual checkboxes
    // (they're implied to be all on)
    bool enableIndividual = !detectAll;
    
    ui->checkDetectWeakBitsProt->setEnabled(enableIndividual);
    ui->checkDetectLongTracks->setEnabled(enableIndividual);
    ui->checkDetectHalfTracks->setEnabled(enableIndividual);
    ui->checkDetectTiming->setEnabled(enableIndividual);
    ui->checkDetectNoFlux->setEnabled(enableIndividual);
    ui->checkDetectCustomSync->setEnabled(enableIndividual);
    
    // If detect all, visually check all boxes
    if (detectAll) {
        ui->checkDetectWeakBitsProt->setChecked(true);
        ui->checkDetectLongTracks->setChecked(true);
        ui->checkDetectHalfTracks->setChecked(true);
        ui->checkDetectTiming->setChecked(true);
        ui->checkDetectNoFlux->setChecked(true);
        ui->checkDetectCustomSync->setChecked(true);
    }
}

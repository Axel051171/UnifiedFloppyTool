/**
 * @file formattab.cpp
 * @brief Format/Settings Tab Widget - Complete Implementation
 * 
 * ALL UI Dependencies + 25+ System/Format Mappings
 * 
 * @author UFT Team
 * @date 2026-01-12
 */

#include "formattab.h"
#include "ui_tab_format.h"
#include "uft_gw2dmk_panel.h"
#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QDialog>
#include <QVBoxLayout>

// ============================================================================
// Construction / Destruction
// ============================================================================

FormatTab::FormatTab(QWidget *parent) 
    : QWidget(parent)
    , ui(new Ui::TabFormat)

{
    ui->setupUi(this);
    
    setupFormatDatabase();
    
    // Initialize presets
    setupBuiltinPresets();
    loadPresetsFromFile();
    setupConnections();
    
    // Populate system combo with all systems
    populateSystemCombo();
    
    // Initialize with first system
    onSystemChanged(0);
    setupInitialState();
    loadSettings();
}

FormatTab::~FormatTab() {
    saveSettings();
    delete ui;
}



// ============================================================================
// Format Database Setup - 25+ Systems, 100+ Formats
// ============================================================================

void FormatTab::setupFormatDatabase() {
    // ========================================================================
    // COMMODORE FAMILY
    // ========================================================================
    m_systemFormats["Commodore 64/128"] = {
        "D64", "G64", "D71", "D81", "NIB", "NBZ", "P64", "X64", "T64", "TAP"
    };
    
    m_systemFormats["Commodore Plus/4"] = {
        "D64", "D71", "TAP"
    };
    
    m_systemFormats["Commodore VIC-20"] = {
        "D64", "TAP", "PRG"
    };
    
    m_systemFormats["Commodore PET/CBM"] = {
        "D64", "D80", "D82", "D67"
    };
    
    // ========================================================================
    // AMIGA
    // ========================================================================
    m_systemFormats["Amiga"] = {
        "ADF", "ADF-OFS", "ADF-FFS", "ADF-HD", "ADZ", "HDF", "DMS", "IPF"
    };
    
    // ========================================================================
    // APPLE FAMILY
    // ========================================================================
    m_systemFormats["Apple II"] = {
        "WOZ", "A2R", "NIB", "PO", "DO", "2IMG", "DSK", "D13"
    };
    
    m_systemFormats["Apple III"] = {
        "PO", "2IMG", "DSK"
    };
    
    m_systemFormats["Macintosh (400K/800K)"] = {
        "DC42", "IMG", "DiskCopy", "DART"
    };
    
    // ========================================================================
    // ATARI FAMILY
    // ========================================================================
    m_systemFormats["Atari ST/STE"] = {
        "ST", "STX", "MSA", "DIM", "STT", "IPF"
    };
    
    m_systemFormats["Atari 8-bit (400/800/XL/XE)"] = {
        "ATR", "ATX", "XFD", "DCM", "PRO", "XEX"
    };
    
    // ========================================================================
    // SINCLAIR / SPECTRUM
    // ========================================================================
    m_systemFormats["ZX Spectrum"] = {
        "TRD", "SCL", "TZX", "TAP", "DSK", "FDI", "TD0", "UDI", "OPD", "MGT"
    };
    
    m_systemFormats["SAM Coupé"] = {
        "MGT", "SAD", "DSK"
    };
    
    // ========================================================================
    // AMSTRAD
    // ========================================================================
    m_systemFormats["Amstrad CPC"] = {
        "DSK", "EDSK", "RAW", "IPF", "SCP", "CPT"
    };
    
    m_systemFormats["Amstrad PCW"] = {
        "DSK", "EDSK", "IMG"
    };
    
    // ========================================================================
    // MSX
    // ========================================================================
    m_systemFormats["MSX"] = {
        "DSK", "DMK", "IMG", "DI", "XSA"
    };
    
    // ========================================================================
    // BBC / ACORN
    // ========================================================================
    m_systemFormats["BBC Micro"] = {
        "SSD", "DSD", "ADF", "ADL", "UEF", "MMB"
    };
    
    m_systemFormats["Acorn Archimedes"] = {
        "ADF", "ADL", "APD", "JFD"
    };
    
    // ========================================================================
    // PC / DOS
    // ========================================================================
    m_systemFormats["PC/DOS"] = {
        "IMG", "IMA", "XDF", "DMF", "2M", "TD0", "IMD", "CQM", 
        "360K", "720K", "1.2M", "1.44M", "2.88M", "86F"
    };
    
    // ========================================================================
    // JAPANESE SYSTEMS
    // ========================================================================
    m_systemFormats["NEC PC-98"] = {
        "D88", "D77", "NFD", "FDI", "HDM", "XDF", "DUP"
    };
    
    m_systemFormats["Sharp X68000"] = {
        "XDF", "DIM", "D88", "HDS"
    };
    
    m_systemFormats["FM Towns"] = {
        "D88", "D77", "IMG"
    };
    
    // ========================================================================
    // TRS-80
    // ========================================================================
    m_systemFormats["TRS-80 (Model I/III/4)"] = {
        "DMK", "JV1", "JV3", "DSK", "IMD"
    };
    
    m_systemFormats["TRS-80 Color Computer"] = {
        "VDK", "DSK", "DMK", "JVC"
    };
    
    // ========================================================================
    // TEXAS INSTRUMENTS
    // ========================================================================
    m_systemFormats["TI-99/4A"] = {
        "DSK", "V9T9", "PC99"
    };
    
    // ========================================================================
    // FRENCH SYSTEMS
    // ========================================================================
    m_systemFormats["Thomson MO/TO"] = {
        "FD", "SAP", "HFE", "QD"
    };
    
    m_systemFormats["Oric Atmos"] = {
        "DSK", "TAP", "ORI"
    };
    
    // ========================================================================
    // CP/M SYSTEMS
    // ========================================================================
    m_systemFormats["Kaypro"] = {
        "IMG", "TD0", "IMD", "DSK"
    };
    
    m_systemFormats["Osborne"] = {
        "IMG", "TD0", "IMD"
    };
    
    m_systemFormats["North Star"] = {
        "NSI", "IMG", "TD0"
    };
    
    // ========================================================================
    // DEC
    // ========================================================================
    m_systemFormats["DEC PDP/VAX"] = {
        "RX01", "RX02", "IMG"
    };
    
    // ========================================================================
    // OTHER SYSTEMS
    // ========================================================================
    m_systemFormats["Heathkit/Zenith"] = {
        "IMG", "TD0", "IMD"
    };
    
    m_systemFormats["Victor 9000"] = {
        "IMG", "TD0", "SCP"
    };
    
    // ========================================================================
    // FLUX / RAW FORMATS
    // ========================================================================
    m_systemFormats["Flux (raw)"] = {
        "SCP", "HFE", "RAW", "KF", "CT", "A2R", "WOZ", "IPF", "FDI", "MFM"
    };

    // ========================================================================
    // Format Info Database (Key formats with metadata)
    // ========================================================================
    
    // Commodore
    m_formatInfo["D64"] = {"D64", "C64/1541 Disk Image", {"Standard", "35 Track", "40 Track", "42 Track"},
        false, false, false, true, false, 35, 21, 256};
    m_formatInfo["G64"] = {"G64", "C64 GCR Disk Image", {"Standard", "Extended"},
        true, true, false, true, false, 42, 21, 256};
    m_formatInfo["D71"] = {"D71", "C128/1571 Disk Image", {"Standard"},
        false, false, false, true, false, 70, 21, 256};
    m_formatInfo["D81"] = {"D81", "C128/1581 Disk Image", {"Standard"},
        false, false, false, false, true, 80, 10, 512};
    m_formatInfo["NIB"] = {"NIB", "Nibble Image", {"C64", "Apple"},
        true, true, false, true, false, 35, 0, 0};
        
    // Amiga
    m_formatInfo["ADF"] = {"ADF", "Amiga Disk File", {"DD (880K)", "HD (1.76M)"},
        false, false, false, false, true, 80, 11, 512};
    m_formatInfo["IPF"] = {"IPF", "Interchangeable Preservation Format", {"Standard"},
        true, true, true, true, true, 84, 0, 0};
        
    // Apple
    m_formatInfo["WOZ"] = {"WOZ", "Apple II Flux Image", {"WOZ 1.0", "WOZ 2.0"},
        true, true, true, true, false, 35, 16, 256};
    m_formatInfo["A2R"] = {"A2R", "Applesauce Flux", {"Standard"},
        true, true, true, true, false, 35, 16, 256};
    m_formatInfo["PO"] = {"PO", "ProDOS Order", {"140K", "800K"},
        false, false, false, true, false, 35, 16, 256};
        
    // Atari
    m_formatInfo["ST"] = {"ST", "Atari ST Sector Image", {"SS/DD", "DS/DD", "DS/HD"},
        false, false, false, false, true, 80, 9, 512};
    m_formatInfo["STX"] = {"STX", "Atari ST Extended", {"Standard"},
        true, true, false, false, true, 80, 9, 512};
    m_formatInfo["ATR"] = {"ATR", "Atari 8-bit Image", {"SD (90K)", "ED (130K)", "DD (180K)"},
        false, false, false, false, true, 40, 18, 128};
        
    // Spectrum
    m_formatInfo["TRD"] = {"TRD", "TR-DOS Image", {"DS/DD 640K", "SS/DD 320K"},
        false, false, false, false, true, 80, 16, 256};
    m_formatInfo["SCL"] = {"SCL", "Sinclair Container", {"Standard"},
        false, false, false, false, true, 0, 0, 0};
    m_formatInfo["TZX"] = {"TZX", "ZX Spectrum Tape", {"Standard"},
        false, false, false, false, false, 0, 0, 0};
        
    // Amstrad
    m_formatInfo["DSK"] = {"DSK", "Amstrad/Spectrum DSK", {"Standard", "Extended (EDSK)"},
        false, false, false, false, true, 40, 9, 512};
    m_formatInfo["EDSK"] = {"EDSK", "Extended DSK", {"Standard"},
        true, true, false, false, true, 42, 9, 512};
        
    // PC
    m_formatInfo["IMG"] = {"IMG", "Raw Sector Image", {"360K", "720K", "1.2M", "1.44M", "2.88M"},
        false, false, false, false, true, 80, 18, 512};
    m_formatInfo["XDF"] = {"XDF", "Extended Density Format", {"XDF 5.25\" (1.86M)", "XDF 3.5\" (1.86M)"},
        false, false, false, false, true, 80, 23, 512};
    m_formatInfo["DMF"] = {"DMF", "Distribution Media Format", {"DMF 1.68M", "DMF 1.72M"},
        false, false, false, false, true, 80, 21, 512};
    m_formatInfo["TD0"] = {"TD0", "Teledisk Image", {"Normal", "Advanced"},
        true, true, false, false, true, 80, 18, 512};
        
    // Japanese
    m_formatInfo["D88"] = {"D88", "PC-98/X1 Image", {"2D", "2DD", "2HD"},
        false, false, false, false, true, 80, 16, 256};
        
    // Flux
    m_formatInfo["SCP"] = {"SCP", "SuperCard Pro Flux", {"Single Rev", "Multi Rev"},
        true, true, true, true, true, 84, 0, 0};
    m_formatInfo["HFE"] = {"HFE", "HxC Floppy Emulator", {"HFE v1", "HFE v3"},
        true, true, true, true, true, 84, 0, 0};
}

// ============================================================================
// Populate System Combo
// ============================================================================

void FormatTab::populateSystemCombo() {
    ui->comboSystem->blockSignals(true);
    ui->comboSystem->clear();
    
    // Add systems in logical order
    QStringList systemOrder = {
        // Commodore
        "Commodore 64/128", "Commodore Plus/4", "Commodore VIC-20", "Commodore PET/CBM",
        // Amiga
        "Amiga",
        // Apple
        "Apple II", "Apple III", "Macintosh (400K/800K)",
        // Atari
        "Atari ST/STE", "Atari 8-bit (400/800/XL/XE)",
        // Sinclair
        "ZX Spectrum", "SAM Coupé",
        // Amstrad
        "Amstrad CPC", "Amstrad PCW",
        // MSX
        "MSX",
        // BBC/Acorn
        "BBC Micro", "Acorn Archimedes",
        // PC
        "PC/DOS",
        // Japanese
        "NEC PC-98", "Sharp X68000", "FM Towns",
        // TRS-80
        "TRS-80 (Model I/III/4)", "TRS-80 Color Computer",
        // TI
        "TI-99/4A",
        // French
        "Thomson MO/TO", "Oric Atmos",
        // CP/M
        "Kaypro", "Osborne", "North Star",
        // DEC
        "DEC PDP/VAX",
        // Other
        "Heathkit/Zenith", "Victor 9000",
        // Flux
        "Flux (raw)"
    };
    
    for (const QString& sys : systemOrder) {
        if (m_systemFormats.contains(sys)) {
            ui->comboSystem->addItem(sys);
        }
    }
    
    ui->comboSystem->blockSignals(false);
}

// ============================================================================
// Connection Setup - ALL Dependencies
// ============================================================================

void FormatTab::setupConnections() {
    // System/Format cascade
    connect(ui->comboSystem, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onSystemChanged);
    connect(ui->comboFormat, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onFormatChanged);
    connect(ui->comboVersion, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onVersionChanged);
    connect(ui->comboEncoding, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onEncodingChanged);
    
    // XCopy
    connect(ui->checkAllTracks, &QCheckBox::toggled,
            this, &FormatTab::onAllTracksToggled);
    connect(ui->spinStartTrack, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->spinEndTrack, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    
    // Nibble/GCR
    connect(ui->comboGCRType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onGCRTypeChanged);
    
    // Logging
    connect(ui->checkLogToFile, &QCheckBox::toggled,
            this, &FormatTab::onLogToFileToggled);
    connect(ui->btnBrowseLog, &QPushButton::clicked,
            this, &FormatTab::onBrowseLogPath);
    
    // Protection
    connect(ui->checkDetectAll, &QCheckBox::toggled,
            this, &FormatTab::onDetectAllToggled);
    connect(ui->comboPlatform, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    
    // Advanced dialog buttons
    connect(ui->btnFluxAdvanced, &QPushButton::clicked,
            this, &FormatTab::onFluxAdvanced);
    connect(ui->btnPLLAdvanced, &QPushButton::clicked,
            this, &FormatTab::onPLLAdvanced);
    connect(ui->btnNibbleAdvanced, &QPushButton::clicked,
            this, &FormatTab::onNibbleAdvanced);
    
    // Presets
    connect(ui->btnLoadPreset, &QPushButton::clicked,
            this, &FormatTab::onLoadPreset);
    connect(ui->btnSavePreset, &QPushButton::clicked,
            this, &FormatTab::onSavePreset);
    connect(ui->comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    
    // Format parameters
    connect(ui->spinTracks, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->spinSides, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->spinSectors, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->comboSectorSize, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->comboRPM, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    
    // Flux parameters
    connect(ui->comboFluxSpeed, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->spinRevolutions, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->comboFluxErrors, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->comboFluxMerge, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->checkWeakBits, &QCheckBox::toggled,
            this, [this](bool) { emit formatSettingsChanged(); });
    connect(ui->checkNoFluxAreas, &QCheckBox::toggled,
            this, [this](bool) { emit formatSettingsChanged(); });
    
    // PLL parameters
    connect(ui->comboSampleRate, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit formatSettingsChanged(); });
    connect(ui->checkAdaptivePLL, &QCheckBox::toggled,
            this, [this](bool) { emit formatSettingsChanged(); });
    connect(ui->checkUseIndex, &QCheckBox::toggled,
            this, [this](bool) { emit formatSettingsChanged(); });
    
    // GW→DMK Direct
    connect(ui->btnGw2DmkOpen, &QPushButton::clicked,
            this, &FormatTab::onGw2DmkOpenClicked);
}


// ============================================================================
// Initial State Setup
// ============================================================================

void FormatTab::setupInitialState() {
    updateXCopyTrackRange(ui->checkAllTracks->isChecked());
    updateNibbleOptions(ui->comboGCRType->currentText());
    updateLogFileOptions(ui->checkLogToFile->isChecked());
    syncProtectionWidgets(ui->checkDetectAll->isChecked());
    // PLL handled by embedded panel
}

// ============================================================================
// OUTPUT FORMAT - System/Format Cascade
// ============================================================================

void FormatTab::onSystemChanged(int index) {
    QString system = ui->comboSystem->itemText(index);
    populateFormatsForSystem(system);
    emit systemChanged(system);
    emit formatSettingsChanged();
}

void FormatTab::onFormatChanged(int index) {
    if (index < 0) return;
    
    QString format = ui->comboFormat->itemText(index);
    populateVersionsForFormat(format);
    updateFormatSpecificOptions(format);
    emit formatChanged(format);
    emit formatSettingsChanged();
}

void FormatTab::onVersionChanged(int /*index*/) {
    emit formatSettingsChanged();
}

void FormatTab::onEncodingChanged(int index) {
    QString encoding = ui->comboEncoding->itemText(index);
    bool isGCR = encoding.contains("GCR");
    updateGCROptions(isGCR);
    emit formatSettingsChanged();
}

void FormatTab::populateFormatsForSystem(const QString& system) {
    ui->comboFormat->blockSignals(true);
    ui->comboFormat->clear();
    
    if (m_systemFormats.contains(system)) {
        ui->comboFormat->addItems(m_systemFormats[system]);
    }
    
    ui->comboFormat->blockSignals(false);
    
    if (ui->comboFormat->count() > 0) {
        onFormatChanged(0);
    }
}

void FormatTab::populateVersionsForFormat(const QString& format) {
    ui->comboVersion->blockSignals(true);
    ui->comboVersion->clear();
    
    if (m_formatInfo.contains(format)) {
        ui->comboVersion->addItems(m_formatInfo[format].versions);
    } else {
        ui->comboVersion->addItem("Standard");
    }
    
    ui->comboVersion->blockSignals(false);
}

void FormatTab::updateFormatSpecificOptions(const QString& format) {
    if (!m_formatInfo.contains(format)) {
        return;
    }
    
    const FormatInfo& info = m_formatInfo[format];
    
    ui->checkHalfTracks->setEnabled(info.supportsHalfTracks);
    if (!info.supportsHalfTracks) ui->checkHalfTracks->setChecked(false);
    
    
    updateFluxOptions(info.supportsFlux);
    updateGCROptions(info.supportsGCR);
    
    if (info.defaultTracks > 0) ui->spinTracks->setValue(info.defaultTracks);
    
    if (info.defaultSectorSize > 0) {
    }
    
    if (format == "XDF" || format == "DMF" || format == "2M") {
    } else {
    }
}

// ============================================================================
// XCOPY Dependencies
// ============================================================================

void FormatTab::onCopyModeChanged(int index) {
    Q_UNUSED(index);
    emit formatSettingsChanged();
}

void FormatTab::onAllTracksToggled(bool checked) {
    updateXCopyTrackRange(!checked);
    emit formatSettingsChanged();
}

void FormatTab::updateXCopyTrackRange(bool enabled) {
    Q_UNUSED(enabled);
    // UI elements removed in simplified layout
}

void FormatTab::updateXCopyModeOptions(const QString& mode) {
    bool isFluxMode = (mode == "Flux");
    Q_UNUSED(isFluxMode);  // Reserved for future flux-specific XCopy options
    
    // Flux settings handled by embedded panel
    
    // Panel handles its own styling
}

// ============================================================================
// NIBBLE Dependencies
// ============================================================================

void FormatTab::onGCRTypeChanged(int index) {
    QString gcrType = ui->comboGCRType->itemText(index);
    updateNibbleOptions(gcrType);
    emit formatSettingsChanged();
}

void FormatTab::updateNibbleOptions(const QString& gcrType) {
    bool enabled = (gcrType != "Off" && !gcrType.isEmpty());
    
    ui->checkDecodeGCR->setEnabled(enabled);
    // Handled by flux panel
    ui->checkHalfTracks->setEnabled(enabled);
    
    QString style = enabled ? "" : "color: gray;";
    ui->checkDecodeGCR->setStyleSheet(style);
    // Handled by flux panel
    ui->checkHalfTracks->setStyleSheet(style);
    
    // Options set based on GCR type
}

// ============================================================================
// WRITE Dependencies
// ============================================================================

void FormatTab::onRetryErrorsToggled(bool checked) {
    updateRetryOptions(checked);
    emit formatSettingsChanged();
}

void FormatTab::updateRetryOptions(bool enabled) {
    Q_UNUSED(enabled);
    // UI elements removed in simplified layout
}

// ============================================================================
// LOGGING Dependencies
// ============================================================================

void FormatTab::onLogToFileToggled(bool checked) {
    updateLogFileOptions(checked);
    emit formatSettingsChanged();
}

void FormatTab::updateLogFileOptions(bool enabled) {
    ui->editLogPath->setEnabled(enabled);
    ui->btnBrowseLog->setEnabled(enabled);
    ui->checkLogTimestamps->setEnabled(enabled);
    ui->checkVerboseLog->setEnabled(enabled);
}

// ============================================================================
// FORENSIC Dependencies
// ============================================================================

void FormatTab::onValidateStructureToggled(bool checked) {
    updateForensicValidation(checked);
    emit formatSettingsChanged();
}

void FormatTab::onReportFormatChanged(int index) {
    Q_UNUSED(index);
    // Report format combo removed in simplified layout
    emit formatSettingsChanged();
}

void FormatTab::updateForensicValidation(bool enabled) {
    Q_UNUSED(enabled);
    // Validation checkbox removed in simplified layout
}

void FormatTab::updateForensicReport(const QString& format) {
    Q_UNUSED(format);
    // Report format removed in simplified layout
}

// ============================================================================
// PLL Dependencies
// ============================================================================

void FormatTab::onAdaptivePLLToggled(bool checked) {
    updatePLLOptions(checked);
    emit formatSettingsChanged();
}

void FormatTab::onPreserveTimingToggled(bool /*checked*/) {
    emit formatSettingsChanged();
}

void FormatTab::updatePLLOptions(bool enabled) {
    Q_UNUSED(enabled);
    // Advanced PLL settings removed in simplified layout
}

void FormatTab::updateFluxOptions(bool /* isFluxFormat */) {
    // Use embedded panels
    // Flux/PLL options handled by UI widgets and Advanced dialogs
}

void FormatTab::updateGCROptions(bool isGCRFormat) {
    ui->comboGCRType->setEnabled(isGCRFormat);
    ui->checkDecodeGCR->setEnabled(isGCRFormat);
}

// ============================================================================
// PROTECTION Dependencies
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

void FormatTab::syncProtectionWidgets(bool detectAll) {
    bool enableIndividual = !detectAll;
    
    ui->checkDetectWeakBitsProt->setEnabled(enableIndividual);
    ui->checkDetectLongTracks->setEnabled(enableIndividual);
    ui->checkDetectHalfTracks->setEnabled(enableIndividual);
    ui->checkDetectTiming->setEnabled(enableIndividual);
    ui->checkDetectNoFlux->setEnabled(enableIndividual);
    ui->checkDetectCustomSync->setEnabled(enableIndividual);
    
    if (detectAll) {
        ui->checkDetectWeakBitsProt->setChecked(true);
        ui->checkDetectLongTracks->setChecked(true);
        ui->checkDetectHalfTracks->setChecked(true);
        ui->checkDetectTiming->setChecked(true);
        ui->checkDetectNoFlux->setChecked(true);
        ui->checkDetectCustomSync->setChecked(true);
    }
}

// ============================================================================
// Protection Settings API
// ============================================================================

uint32_t FormatTab::getProtectionFlags() const {
    uint32_t flags = 0;
    
    if (ui->checkDetectAll->isChecked()) {
        flags = UFT_PROT_ANAL_ALL;
    } else {
        if (ui->checkDetectWeakBitsProt->isChecked()) flags |= UFT_PROT_ANAL_WEAK_BITS;
        if (ui->checkDetectLongTracks->isChecked())   flags |= UFT_PROT_ANAL_TIMING;
        if (ui->checkDetectHalfTracks->isChecked())   flags |= UFT_PROT_ANAL_HALF_TRACKS;
        if (ui->checkDetectTiming->isChecked())       flags |= UFT_PROT_ANAL_TIMING;
        if (ui->checkDetectNoFlux->isChecked())       flags |= UFT_PROT_ANAL_WEAK_BITS;
        if (ui->checkDetectCustomSync->isChecked())   flags |= UFT_PROT_ANAL_SIGNATURES;
    }
    
    if (flags == 0) flags = UFT_PROT_ANAL_QUICK;
    
    return flags;
}

uft_platform_t FormatTab::getPlatformHint() const {
    int idx = ui->comboPlatform->currentIndex();
    switch (idx) {
        case 0: return UFT_PLATFORM_UNKNOWN;
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
    config->confidence_threshold = 70;
}

// ============================================================================
// Format Settings API
// ============================================================================

QString FormatTab::getSelectedSystem() const {
    return ui->comboSystem->currentText();
}

QString FormatTab::getSelectedFormat() const {
    return ui->comboFormat->currentText();
}

QString FormatTab::getSelectedVersion() const {
    return ui->comboVersion->currentText();
}

// ============================================================================
// Settings Persistence
// ============================================================================

void FormatTab::loadSettings() {
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);
    
    int sysIdx = settings.value("system", 0).toInt();
    if (sysIdx < ui->comboSystem->count()) {
        ui->comboSystem->setCurrentIndex(sysIdx);
    }
    
    ui->checkAllTracks->setChecked(settings.value("allTracks", true).toBool());
    ui->checkDetectAll->setChecked(settings.value("detectAll", true).toBool());
    ui->comboPlatform->setCurrentIndex(settings.value("platform", 0).toInt());
    ui->radioPreserve->setChecked(settings.value("preserve", true).toBool());
    ui->radioRemove->setChecked(!settings.value("preserve", true).toBool());
    // PLL settings handled by embedded panel
    ui->checkLogToFile->setChecked(settings.value("logToFile", false).toBool());
    
    
    // Read Options
    
    // Write Options
    
    settings.endGroup();
    
    setupInitialState();
}

void FormatTab::saveSettings() {
    QSettings settings;
    settings.beginGroup(SETTINGS_GROUP);
    
    settings.setValue("system", ui->comboSystem->currentIndex());
    settings.setValue("format", ui->comboFormat->currentIndex());
    settings.setValue("allTracks", ui->checkAllTracks->isChecked());
    settings.setValue("gcrType", ui->comboGCRType->currentIndex());
    settings.setValue("detectAll", ui->checkDetectAll->isChecked());
    settings.setValue("platform", ui->comboPlatform->currentIndex());
    settings.setValue("preserve", ui->radioPreserve->isChecked());
    // PLL settings handled by embedded panel
    settings.setValue("logToFile", ui->checkLogToFile->isChecked());
    
    // Read Options
    
    // Write Options
    
    settings.endGroup();
    settings.sync();
}

// ============================================================================
// Preset Management
// ============================================================================

void FormatTab::setupBuiltinPresets() {
    // Default preset
    Preset defPreset;
    defPreset.name = tr("(Default)");
    defPreset.system = "Commodore 64/128";
    defPreset.format = "D64";
    defPreset.version = "Standard";
    defPreset.encoding = "GCR";
    defPreset.tracks = 35;
    defPreset.heads = 1;
    defPreset.density = "DD";
    defPreset.halfTracks = false;
    defPreset.preserveTiming = true;
    defPreset.adaptivePLL = true;
    defPreset.copyMode = "Sector";
    defPreset.gcrType = "C64";
    defPreset.detectProtection = true;
    m_presets["(Default)"] = defPreset;
    
    // C64 Preservation preset
    Preset c64Pres;
    c64Pres.name = tr("C64 Preservation");
    c64Pres.system = "Commodore 64/128";
    c64Pres.format = "G64";
    c64Pres.version = "G64 v1.2";
    c64Pres.encoding = "GCR";
    c64Pres.tracks = 42;
    c64Pres.heads = 1;
    c64Pres.density = "DD";
    c64Pres.halfTracks = true;
    c64Pres.preserveTiming = true;
    c64Pres.adaptivePLL = true;
    c64Pres.copyMode = "Flux";
    c64Pres.gcrType = "C64";
    c64Pres.detectProtection = true;
    m_presets["C64 Preservation"] = c64Pres;
    
    // Amiga OCS/ECS preset
    Preset amigaOCS;
    amigaOCS.name = tr("Amiga OCS/ECS");
    amigaOCS.system = "Amiga";
    amigaOCS.format = "ADF";
    amigaOCS.version = "OFS";
    amigaOCS.encoding = "MFM";
    amigaOCS.tracks = 80;
    amigaOCS.heads = 2;
    amigaOCS.density = "DD";
    amigaOCS.halfTracks = false;
    amigaOCS.preserveTiming = true;
    amigaOCS.adaptivePLL = true;
    amigaOCS.copyMode = "Sector";
    amigaOCS.gcrType = "Off";
    amigaOCS.detectProtection = true;
    m_presets["Amiga OCS/ECS"] = amigaOCS;
    
    // Amiga Preservation preset
    Preset amigaPres;
    amigaPres.name = tr("Amiga Preservation");
    amigaPres.system = "Amiga";
    amigaPres.format = "IPF";
    amigaPres.version = "IPF v2";
    amigaPres.encoding = "MFM";
    amigaPres.tracks = 84;
    amigaPres.heads = 2;
    amigaPres.density = "DD";
    amigaPres.halfTracks = false;
    amigaPres.preserveTiming = true;
    amigaPres.adaptivePLL = true;
    amigaPres.copyMode = "Flux";
    amigaPres.gcrType = "Off";
    amigaPres.detectProtection = true;
    m_presets["Amiga Preservation"] = amigaPres;
    
    // Atari ST preset
    Preset atariST;
    atariST.name = tr("Atari ST");
    atariST.system = "Atari ST/STE";
    atariST.format = "ST";
    atariST.version = "Standard";
    atariST.encoding = "MFM";
    atariST.tracks = 80;
    atariST.heads = 2;
    atariST.density = "DD";
    atariST.halfTracks = false;
    atariST.preserveTiming = false;
    atariST.adaptivePLL = true;
    atariST.copyMode = "Sector";
    atariST.gcrType = "Off";
    atariST.detectProtection = true;
    m_presets["Atari ST"] = atariST;
    
    // PC DOS 1.44MB preset
    Preset pcHD;
    pcHD.name = tr("PC DOS 1.44MB");
    pcHD.system = "PC/DOS";
    pcHD.format = "IMG";
    pcHD.version = "HD 1.44M";
    pcHD.encoding = "MFM";
    pcHD.tracks = 80;
    pcHD.heads = 2;
    pcHD.density = "HD";
    pcHD.halfTracks = false;
    pcHD.preserveTiming = false;
    pcHD.adaptivePLL = true;
    pcHD.copyMode = "Sector";
    pcHD.gcrType = "Off";
    pcHD.detectProtection = false;
    m_presets["PC DOS 1.44MB"] = pcHD;
    
    // PC DOS 720K preset
    Preset pcDD;
    pcDD.name = tr("PC DOS 720K");
    pcDD.system = "PC/DOS";
    pcDD.format = "IMG";
    pcDD.version = "DD 720K";
    pcDD.encoding = "MFM";
    pcDD.tracks = 80;
    pcDD.heads = 2;
    pcDD.density = "DD";
    pcDD.halfTracks = false;
    pcDD.preserveTiming = false;
    pcDD.adaptivePLL = true;
    pcDD.copyMode = "Sector";
    pcDD.gcrType = "Off";
    pcDD.detectProtection = false;
    m_presets["PC DOS 720K"] = pcDD;
    
    // Apple II preset
    Preset apple2;
    apple2.name = tr("Apple II DOS 3.3");
    apple2.system = "Apple II";
    apple2.format = "DSK";
    apple2.version = "DOS 3.3";
    apple2.encoding = "GCR";
    apple2.tracks = 35;
    apple2.heads = 1;
    apple2.density = "DD";
    apple2.halfTracks = false;
    apple2.preserveTiming = true;
    apple2.adaptivePLL = true;
    apple2.copyMode = "Sector";
    apple2.gcrType = "Apple";
    apple2.detectProtection = true;
    m_presets["Apple II DOS 3.3"] = apple2;
    
    // ZX Spectrum preset
    Preset zxSpec;
    zxSpec.name = tr("ZX Spectrum +3");
    zxSpec.system = "ZX Spectrum";
    zxSpec.format = "DSK";
    zxSpec.version = "Extended";
    zxSpec.encoding = "MFM";
    zxSpec.tracks = 40;
    zxSpec.heads = 1;
    zxSpec.density = "DD";
    zxSpec.halfTracks = false;
    zxSpec.preserveTiming = true;
    zxSpec.adaptivePLL = true;
    zxSpec.copyMode = "Sector";
    zxSpec.gcrType = "Off";
    zxSpec.detectProtection = true;
    m_presets["ZX Spectrum +3"] = zxSpec;
    
    // Flux Analysis preset
    Preset fluxAnalysis;
    fluxAnalysis.name = tr("Flux Analysis");
    fluxAnalysis.system = "Flux/Raw";
    fluxAnalysis.format = "SCP";
    fluxAnalysis.version = "v2.4";
    fluxAnalysis.encoding = "Raw Flux";
    fluxAnalysis.tracks = 84;
    fluxAnalysis.heads = 2;
    fluxAnalysis.density = "Auto";
    fluxAnalysis.halfTracks = true;
    fluxAnalysis.preserveTiming = true;
    fluxAnalysis.adaptivePLL = true;
    fluxAnalysis.copyMode = "Flux";
    fluxAnalysis.gcrType = "Off";
    fluxAnalysis.detectProtection = true;
    m_presets["Flux Analysis"] = fluxAnalysis;
    
    // Update combo
    updatePresetCombo();
}

void FormatTab::updatePresetCombo() {
    ui->comboPreset->blockSignals(true);
    ui->comboPreset->clear();
    
    // Add builtin presets first
    QStringList builtins = {"(Default)", "C64 Preservation", "Amiga OCS/ECS", 
                           "Amiga Preservation", "Atari ST", "PC DOS 1.44MB",
                           "PC DOS 720K", "Apple II DOS 3.3", "ZX Spectrum +3",
                           "Flux Analysis"};
    
    for (const QString& name : builtins) {
        if (m_presets.contains(name)) {
            ui->comboPreset->addItem(name);
        }
    }
    
    // Add user presets (those not in builtins)
    for (auto it = m_presets.begin(); it != m_presets.end(); ++it) {
        if (!builtins.contains(it.key())) {
            ui->comboPreset->addItem("📁 " + it.key());
        }
    }
    
    ui->comboPreset->blockSignals(false);
}

void FormatTab::onPresetChanged(int index) {
    Q_UNUSED(index);
    // Preview only - don't apply until Load clicked
}

void FormatTab::onLoadPreset() {
    QString name = ui->comboPreset->currentText();
    // Remove user prefix if present
    if (name.startsWith("📁 ")) {
        name = name.mid(3);
    }
    
    if (!m_presets.contains(name)) {
        return;
    }
    
    applyPreset(m_presets[name]);
}

void FormatTab::onSavePreset() {
    QString name = ui->comboPreset->currentText();
    // Remove user prefix if present
    if (name.startsWith("📁 ")) {
        name = name.mid(3);
    }
    
    // Don't overwrite builtins - create new
    QStringList builtins = {"(Default)", "C64 Preservation", "Amiga OCS/ECS", 
                           "Amiga Preservation", "Atari ST", "PC DOS 1.44MB",
                           "PC DOS 720K", "Apple II DOS 3.3", "ZX Spectrum +3",
                           "Flux Analysis"};
    
    if (builtins.contains(name)) {
        // Ask for new name
        bool ok;
        QString newName = QInputDialog::getText(this, tr("Save Preset"),
            tr("Enter preset name:"), QLineEdit::Normal,
            tr("My Preset"), &ok);
        
        if (!ok || newName.isEmpty()) {
            return;
        }
        name = newName;
    }
    
    // Get current settings
    Preset preset = getCurrentSettings();
    preset.name = name;
    
    m_presets[name] = preset;
    savePresetsToFile();
    updatePresetCombo();
    
    // Select the saved preset
    int idx = ui->comboPreset->findText("📁 " + name);
    if (idx >= 0) {
        ui->comboPreset->setCurrentIndex(idx);
    }
}

FormatTab::Preset FormatTab::getCurrentSettings() const {
    Preset p;
    p.system = ui->comboSystem->currentText();
    p.format = ui->comboFormat->currentText();
    p.version = ui->comboVersion->currentText();
    p.encoding = ui->comboEncoding->currentText();
    p.tracks = ui->spinTracks->value();
    p.heads = 2;  // Default
    p.density = "DD";  // Default
    p.halfTracks = ui->checkHalfTracks->isChecked();
    p.preserveTiming = true; // From flux panel
    p.adaptivePLL = true; // From embedded panel
    p.gcrType = ui->comboGCRType->currentText();
    p.detectProtection = ui->checkDetectAll->isChecked();
    return p;
}

void FormatTab::applyPreset(const Preset& preset) {
    // Block signals during update
    ui->comboSystem->blockSignals(true);
    ui->comboFormat->blockSignals(true);
    
    // Set system
    int sysIdx = ui->comboSystem->findText(preset.system, Qt::MatchContains);
    if (sysIdx >= 0) {
        ui->comboSystem->setCurrentIndex(sysIdx);
        populateFormatsForSystem(preset.system);
    }
    
    // Set format
    int fmtIdx = ui->comboFormat->findText(preset.format, Qt::MatchContains);
    if (fmtIdx >= 0) {
        ui->comboFormat->setCurrentIndex(fmtIdx);
        populateVersionsForFormat(preset.format);
    }
    
    // Set version
    int verIdx = ui->comboVersion->findText(preset.version, Qt::MatchContains);
    if (verIdx >= 0) {
        ui->comboVersion->setCurrentIndex(verIdx);
    }
    
    // Set encoding
    int encIdx = ui->comboEncoding->findText(preset.encoding, Qt::MatchContains);
    if (encIdx >= 0) {
        ui->comboEncoding->setCurrentIndex(encIdx);
    }
    
    // Set other values
    ui->spinTracks->setValue(preset.tracks);
    
    ui->checkHalfTracks->setChecked(preset.halfTracks);
    // Preserve timing handled by flux panel
    // PLL preset handled by embedded panel
    
    if (false) { // Copy mode removed
    }
    
    int gcrIdx = ui->comboGCRType->findText(preset.gcrType, Qt::MatchContains);
    if (gcrIdx >= 0) {
        ui->comboGCRType->setCurrentIndex(gcrIdx);
    }
    
    ui->checkDetectAll->setChecked(preset.detectProtection);
    
    // Restore signals
    ui->comboSystem->blockSignals(false);
    ui->comboFormat->blockSignals(false);
    
    // Update dependent options
    updateFormatSpecificOptions(preset.format);
    updateNibbleOptions(preset.gcrType);
    updatePLLOptions(preset.adaptivePLL);
    
    emit formatSettingsChanged();
}

QStringList FormatTab::getPresetNames() const {
    return m_presets.keys();
}

void FormatTab::loadPresetsFromFile() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    m_presetsFilePath = path + "/presets.json";
    
    QFile file(m_presetsFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;  // No user presets yet
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        return;
    }
    
    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        QJsonObject obj = it.value().toObject();
        Preset p;
        p.name = it.key();
        p.system = obj["system"].toString();
        p.format = obj["format"].toString();
        p.version = obj["version"].toString();
        p.encoding = obj["encoding"].toString();
        p.tracks = obj["tracks"].toInt(80);
        p.heads = obj["heads"].toInt(2);
        p.density = obj["density"].toString("DD");
        p.halfTracks = obj["halfTracks"].toBool(false);
        p.preserveTiming = obj["preserveTiming"].toBool(true);
        p.adaptivePLL = obj["adaptivePLL"].toBool(true);
        p.copyMode = obj["copyMode"].toString("Sector");
        p.gcrType = obj["gcrType"].toString("Off");
        p.detectProtection = obj["detectProtection"].toBool(true);
        
        m_presets[p.name] = p;
    }
}

void FormatTab::savePresetsToFile() {
    QJsonObject root;
    
    // Only save user presets (not builtins)
    QStringList builtins = {"(Default)", "C64 Preservation", "Amiga OCS/ECS", 
                           "Amiga Preservation", "Atari ST", "PC DOS 1.44MB",
                           "PC DOS 720K", "Apple II DOS 3.3", "ZX Spectrum +3",
                           "Flux Analysis"};
    
    for (auto it = m_presets.begin(); it != m_presets.end(); ++it) {
        if (builtins.contains(it.key())) {
            continue;  // Skip builtins
        }
        
        const Preset& p = it.value();
        QJsonObject obj;
        obj["system"] = p.system;
        obj["format"] = p.format;
        obj["version"] = p.version;
        obj["encoding"] = p.encoding;
        obj["tracks"] = p.tracks;
        obj["heads"] = p.heads;
        obj["density"] = p.density;
        obj["halfTracks"] = p.halfTracks;
        obj["preserveTiming"] = p.preserveTiming;
        obj["adaptivePLL"] = p.adaptivePLL;
        obj["copyMode"] = p.copyMode;
        obj["gcrType"] = p.gcrType;
        obj["detectProtection"] = p.detectProtection;
        
        root[p.name] = obj;
    }
    
    QFile file(m_presetsFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.close();
    }
}

// ============================================================================
// Read Options Slots
// ============================================================================

void FormatTab::onReadSpeedChanged(int index)
{
    Q_UNUSED(index);
    emit readOptionsChanged();
}

void FormatTab::onIgnoreReadErrorsChanged(bool checked)
{
    Q_UNUSED(checked);
    emit readOptionsChanged();
}

void FormatTab::onFastErrorSkipChanged(bool checked)
{
    Q_UNUSED(checked);
    emit readOptionsChanged();
}

void FormatTab::onAdvancedScanningChanged(bool checked)
{
    Q_UNUSED(checked);
    emit readOptionsChanged();
}

void FormatTab::onScanFactorChanged(int value)
{
    Q_UNUSED(value);
    emit readOptionsChanged();
}

void FormatTab::onReadTimingDataChanged(bool checked)
{
    // If reading timing data, sub-channel might be useful too
    Q_UNUSED(checked);
    emit readOptionsChanged();
}

void FormatTab::onDPMAnalysisChanged(bool /* checked */)
{
    // Enable/disable the DPM accuracy combo
    emit readOptionsChanged();
}

void FormatTab::onReadSubChannelChanged(bool checked)
{
    Q_UNUSED(checked);
    emit readOptionsChanged();
}

// ============================================================================
// Write Options Slots
// ============================================================================

void FormatTab::onVerifyAfterWriteChanged(bool checked)
{
    Q_UNUSED(checked);
    emit writeOptionsChanged();
}

void FormatTab::onIgnoreWriteErrorsChanged(bool checked)
{
    // Warning: ignoring write errors can corrupt data
    if (checked) {
        qWarning() << "Warning: Ignoring write errors may result in corrupted output";
    }
    emit writeOptionsChanged();
}

void FormatTab::onWriteTimingDataChanged(bool checked)
{
    Q_UNUSED(checked);
    emit writeOptionsChanged();
}

void FormatTab::onCorrectSubChannelChanged(bool checked)
{
    Q_UNUSED(checked);
    emit writeOptionsChanged();
}

// ============================================================================
// Read/Write Options Getters
// ============================================================================

FormatTab::ReadOptions FormatTab::getReadOptions() const
{
    ReadOptions opts = {};
    
    
    return opts;
}

FormatTab::WriteOptions FormatTab::getWriteOptions() const
{
    WriteOptions opts = {};
    
    
    return opts;
}

// ============================================================================
// XCopy Handlers
// ============================================================================


// ============================================================================
// Logging Handlers
// ============================================================================

void FormatTab::onBrowseLogPath() {
    QString path = QFileDialog::getSaveFileName(this,
        tr("Select Log File"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("Log Files (*.log *.txt);;All Files (*)"));
    
    if (!path.isEmpty()) {
        ui->editLogPath->setText(path);
        emit formatSettingsChanged();
    }
}

// ============================================================================
// ADVANCED DIALOG HANDLERS
// ============================================================================

void FormatTab::onFluxAdvanced() {
    FluxAdvancedDialog dlg(this);
    dlg.setParams(m_fluxAdvParams);
    
    if (dlg.exec() == QDialog::Accepted) {
        m_fluxAdvParams = dlg.getParams();
        emit formatSettingsChanged();
    }
}

void FormatTab::onPLLAdvanced() {
    PLLAdvancedDialog dlg(this);
    dlg.setParams(m_pllAdvParams);
    
    if (dlg.exec() == QDialog::Accepted) {
        m_pllAdvParams = dlg.getParams();
        emit formatSettingsChanged();
    }
}

void FormatTab::onNibbleAdvanced() {
    NibbleAdvancedDialog dlg(this);
    dlg.setParams(m_nibbleAdvParams);
    
    if (dlg.exec() == QDialog::Accepted) {
        m_nibbleAdvParams = dlg.getParams();
        emit formatSettingsChanged();
    }
}

void FormatTab::onGw2DmkOpenClicked()
{
    // Create dialog with GW→DMK Panel
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("GW→DMK Direct Read (TRS-80)"));
    dlg->setMinimumSize(800, 600);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    
    QVBoxLayout *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(4, 4, 4, 4);
    
    UftGw2DmkPanel *panel = new UftGw2DmkPanel(dlg);
    layout->addWidget(panel);
    
    // Apply preset from combo if available
    QString preset = ui->comboGw2DmkPreset->currentText();
    if (preset.contains("SSSD")) {
        panel->setPreset(0);  // Model I/III SSSD
    } else if (preset.contains("SSDD")) {
        panel->setPreset(1);  // Model I/III SSDD
    } else if (preset.contains("Model 4")) {
        panel->setPreset(2);  // Model 4 DSDD
    }
    
    dlg->show();
}

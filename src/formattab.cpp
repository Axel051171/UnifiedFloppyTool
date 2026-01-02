/**
 * @file formattab.cpp
 * @brief Format/Settings Tab Implementation with smart UI activation
 * 
 * Only enables UI elements that are relevant for the selected format.
 * Prevents users from setting options that don't apply to their disk type.
 */

#include "formattab.h"
#include "ui_tab_format.h"
#include <QDebug>

FormatTab::FormatTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabFormat)
{
    ui->setupUi(this);
    
    initializeFormatDatabase();
    setupConnections();
    
    // Initialize with first system
    onSystemChanged(0);
}

FormatTab::~FormatTab()
{
    delete ui;
}

void FormatTab::initializeFormatDatabase()
{
    // ═══════════════════════════════════════════════════════════════════════
    // COMMODORE FORMATS
    // ═══════════════════════════════════════════════════════════════════════
    
    FormatConfig d64 = {
        "Commodore", "D64", {"Standard 35T", "40 Track", "42 Track"},
        true,   // flux
        true,   // pll
        true,   // geometry
        false,  // encoding (fixed GCR)
        true,   // timing
        true,   // retry
        true,   // verify
        true,   // nibble
        true,   // GCR
        false,  // MFM
        35, 21, 1, 300, "GCR"
    };
    
    FormatConfig g64 = {
        "Commodore", "G64", {"Standard", "Extended"},
        true, true, true, false, true, true, true, true, true, false,
        35, 21, 1, 300, "GCR"
    };
    
    FormatConfig d71 = {
        "Commodore", "D71", {"Standard"},
        true, true, true, false, true, true, true, true, true, false,
        35, 21, 2, 300, "GCR"
    };
    
    FormatConfig d81 = {
        "Commodore", "D81", {"Standard"},
        true, true, true, false, true, true, true, false, false, true,
        80, 10, 2, 300, "MFM"  // D81 uses MFM!
    };
    
    m_formatDB["Commodore"] = {d64, g64, d71, d81};
    
    // ═══════════════════════════════════════════════════════════════════════
    // AMIGA FORMATS
    // ═══════════════════════════════════════════════════════════════════════
    
    FormatConfig adf_ofs = {
        "Amiga", "ADF (OFS)", {"Standard DD", "HD"},
        true, true, true, false, true, true, true, false, false, true,
        80, 11, 2, 300, "MFM"
    };
    
    FormatConfig adf_ffs = {
        "Amiga", "ADF (FFS)", {"Standard DD", "HD", "International"},
        true, true, true, false, true, true, true, false, false, true,
        80, 11, 2, 300, "MFM"
    };
    
    FormatConfig ipf = {
        "Amiga", "IPF", {"Standard", "CAPS"},
        true, true, false, false, true, false, false, false, false, true,
        80, 11, 2, 300, "MFM"
    };
    
    m_formatDB["Amiga"] = {adf_ofs, adf_ffs, ipf};
    
    // ═══════════════════════════════════════════════════════════════════════
    // APPLE FORMATS
    // ═══════════════════════════════════════════════════════════════════════
    
    FormatConfig nib = {
        "Apple", "NIB", {"DOS 3.3", "ProDOS"},
        true, true, true, false, true, true, true, true, true, false,
        35, 16, 1, 300, "GCR"
    };
    
    FormatConfig woz = {
        "Apple", "WOZ", {"WOZ 1.0", "WOZ 2.0"},
        true, true, true, false, true, true, true, true, true, false,
        35, 16, 1, 300, "GCR"
    };
    
    FormatConfig dsk = {
        "Apple", "DSK", {"DOS 3.3", "ProDOS", "Pascal"},
        false, false, true, false, false, true, true, false, true, false,
        35, 16, 1, 300, "GCR"
    };
    
    m_formatDB["Apple"] = {nib, woz, dsk};
    
    // ═══════════════════════════════════════════════════════════════════════
    // ATARI FORMATS
    // ═══════════════════════════════════════════════════════════════════════
    
    FormatConfig atr = {
        "Atari", "ATR", {"SD 90K", "ED 130K", "DD 180K", "QD 360K"},
        true, true, true, false, true, true, true, false, false, true,
        40, 18, 1, 288, "FM/MFM"
    };
    
    FormatConfig xfd = {
        "Atari", "XFD", {"Standard"},
        false, false, true, false, false, true, true, false, false, true,
        40, 18, 1, 288, "FM/MFM"
    };
    
    FormatConfig atx = {
        "Atari", "ATX", {"VAPI"},
        true, true, true, false, true, true, true, true, false, true,
        40, 18, 1, 288, "FM/MFM"
    };
    
    m_formatDB["Atari"] = {atr, xfd, atx};
    
    // ═══════════════════════════════════════════════════════════════════════
    // PC/DOS FORMATS
    // ═══════════════════════════════════════════════════════════════════════
    
    FormatConfig img_360 = {
        "PC/DOS", "IMG 360K", {"5.25\" DD"},
        true, true, true, false, true, true, true, false, false, true,
        40, 9, 2, 300, "MFM"
    };
    
    FormatConfig img_720 = {
        "PC/DOS", "IMG 720K", {"3.5\" DD"},
        true, true, true, false, true, true, true, false, false, true,
        80, 9, 2, 300, "MFM"
    };
    
    FormatConfig img_1440 = {
        "PC/DOS", "IMG 1.44M", {"3.5\" HD"},
        true, true, true, false, true, true, true, false, false, true,
        80, 18, 2, 300, "MFM"
    };
    
    FormatConfig imd = {
        "PC/DOS", "IMD", {"ImageDisk"},
        true, true, true, true, true, true, true, true, false, true,
        80, 18, 2, 300, "MFM"
    };
    
    m_formatDB["PC/DOS"] = {img_360, img_720, img_1440, imd};
    
    // ═══════════════════════════════════════════════════════════════════════
    // BBC MICRO FORMATS
    // ═══════════════════════════════════════════════════════════════════════
    
    FormatConfig ssd = {
        "BBC Micro", "SSD", {"Single Density"},
        true, true, true, false, true, true, true, false, false, true,
        40, 10, 1, 300, "FM"
    };
    
    FormatConfig dsd = {
        "BBC Micro", "DSD", {"Double Sided"},
        true, true, true, false, true, true, true, false, false, true,
        40, 10, 2, 300, "FM"
    };
    
    FormatConfig adfs = {
        "BBC Micro", "ADFS", {"S/M/L"},
        true, true, true, false, true, true, true, false, false, true,
        80, 16, 2, 300, "MFM"
    };
    
    m_formatDB["BBC Micro"] = {ssd, dsd, adfs};
    
    // ═══════════════════════════════════════════════════════════════════════
    // FLUX (RAW) FORMATS
    // ═══════════════════════════════════════════════════════════════════════
    
    FormatConfig scp = {
        "Flux (raw)", "SCP", {"v1", "v2", "v3"},
        true, true, false, false, true, true, false, false, false, false,
        84, 0, 2, 300, "RAW"
    };
    
    FormatConfig hfe = {
        "Flux (raw)", "HFE", {"v1", "v2", "v3"},
        true, true, false, false, true, true, false, false, false, false,
        84, 0, 2, 300, "RAW"
    };
    
    FormatConfig raw = {
        "Flux (raw)", "RAW", {"Greaseweazle", "KryoFlux"},
        true, true, false, false, true, false, false, false, false, false,
        84, 0, 2, 300, "RAW"
    };
    
    m_formatDB["Flux (raw)"] = {scp, hfe, raw};
}

void FormatTab::setupConnections()
{
    connect(ui->comboSystem, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onSystemChanged);
    connect(ui->comboFormat, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onFormatChanged);
    connect(ui->comboVersion, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatTab::onVersionChanged);
}

void FormatTab::onSystemChanged(int index)
{
    QString system = ui->comboSystem->currentText();
    qDebug() << "System changed to:" << system;
    
    updateFormatsForSystem(system);
    
    // Trigger format update
    onFormatChanged(0);
}

void FormatTab::onFormatChanged(int index)
{
    QString system = ui->comboSystem->currentText();
    QString format = ui->comboFormat->currentText();
    
    qDebug() << "Format changed to:" << format;
    
    // Find matching config
    if (m_formatDB.contains(system)) {
        for (const FormatConfig &cfg : m_formatDB[system]) {
            if (cfg.format == format) {
                m_currentConfig = cfg;
                updateVersionsForFormat(format);
                applyFormatConfig(cfg);
                emit formatChanged(system, format);
                emit configurationChanged();
                return;
            }
        }
    }
}

void FormatTab::onVersionChanged(int index)
{
    qDebug() << "Version changed to:" << ui->comboVersion->currentText();
    emit configurationChanged();
}

void FormatTab::updateFormatsForSystem(const QString &system)
{
    ui->comboFormat->blockSignals(true);
    ui->comboFormat->clear();
    
    if (m_formatDB.contains(system)) {
        for (const FormatConfig &cfg : m_formatDB[system]) {
            ui->comboFormat->addItem(cfg.format);
        }
    }
    
    ui->comboFormat->blockSignals(false);
}

void FormatTab::updateVersionsForFormat(const QString &format)
{
    ui->comboVersion->blockSignals(true);
    ui->comboVersion->clear();
    ui->comboVersion->addItems(m_currentConfig.versions);
    ui->comboVersion->blockSignals(false);
}

void FormatTab::applyFormatConfig(const FormatConfig &config)
{
    qDebug() << "Applying config for" << config.system << config.format;
    qDebug() << "  GCR:" << config.enableGCR << "MFM:" << config.enableMFM;
    qDebug() << "  Tracks:" << config.defaultTracks << "Sectors:" << config.defaultSectors;
    
    // ═══════════════════════════════════════════════════════════════════════
    // ENABLE/DISABLE UI GROUPS BASED ON FORMAT
    // ═══════════════════════════════════════════════════════════════════════
    
    // Flux settings (PLL, revolutions, etc.)
    setGroupEnabled(ui->groupFlux, config.enableFlux);
    
    // PLL settings
    if (ui->groupPLL) {
        setGroupEnabled(ui->groupPLL, config.enablePLL);
    }
    
    // Geometry settings (tracks, sectors, sides)
    if (ui->groupGeometry) {
        setGroupEnabled(ui->groupGeometry, config.enableGeometry);
        
        // Set default values
        if (ui->spinTracks) ui->spinTracks->setValue(config.defaultTracks);
        if (ui->spinSectors) ui->spinSectors->setValue(config.defaultSectors);
        if (ui->spinSides) ui->spinSides->setValue(config.defaultSides);
    }
    
    // Retry/Error handling
    if (ui->groupRetry) {
        setGroupEnabled(ui->groupRetry, config.enableRetry);
    }
    
    // Verify settings
    if (ui->groupVerify) {
        setGroupEnabled(ui->groupVerify, config.enableVerify);
    }
    
    // Nibble/raw data settings (for copy protection)
    if (ui->groupNibble) {
        setGroupEnabled(ui->groupNibble, config.enableNibble);
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // GCR vs MFM SPECIFIC OPTIONS
    // ═══════════════════════════════════════════════════════════════════════
    
    // GCR options (Commodore, Apple)
    if (ui->groupGCR) {
        setGroupEnabled(ui->groupGCR, config.enableGCR);
    }
    
    // MFM options (Amiga, PC, Atari, BBC)
    if (ui->groupMFM) {
        setGroupEnabled(ui->groupMFM, config.enableMFM);
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // UPDATE STATUS/INFO LABEL
    // ═══════════════════════════════════════════════════════════════════════
    
    QString info = QString("Format: %1 %2\nEncoding: %3\nGeometry: %4T/%5S/%6H @ %7 RPM")
        .arg(config.system)
        .arg(config.format)
        .arg(config.defaultEncoding)
        .arg(config.defaultTracks)
        .arg(config.defaultSectors)
        .arg(config.defaultSides)
        .arg(config.defaultRPM);
    
    if (ui->labelFormatInfo) {
        ui->labelFormatInfo->setText(info);
    }
}

void FormatTab::setGroupEnabled(QWidget *group, bool enabled)
{
    if (!group) return;
    
    group->setEnabled(enabled);
    
    // Visual feedback: gray out disabled groups
    if (enabled) {
        group->setStyleSheet("QGroupBox { font-weight: bold; }");
    } else {
        group->setStyleSheet("QGroupBox { font-weight: bold; color: #888; }");
    }
}

QString FormatTab::currentSystem() const
{
    return ui->comboSystem->currentText();
}

QString FormatTab::currentFormat() const
{
    return ui->comboFormat->currentText();
}

FormatConfig FormatTab::currentConfig() const
{
    return m_currentConfig;
}

void FormatTab::updateUIForCurrentFormat()
{
    applyFormatConfig(m_currentConfig);
}

void FormatTab::setFormat(const QString &system, const QString &format)
{
    qDebug() << "FormatTab::setFormat called with" << system << "/" << format;
    
    // Block signals to prevent triggering format change warning
    ui->comboSystem->blockSignals(true);
    ui->comboFormat->blockSignals(true);
    
    // Find and set system
    int systemIndex = ui->comboSystem->findText(system);
    if (systemIndex >= 0) {
        ui->comboSystem->setCurrentIndex(systemIndex);
        updateFormatsForSystem(system);
        
        // Find and set format
        int formatIndex = ui->comboFormat->findText(format);
        if (formatIndex >= 0) {
            ui->comboFormat->setCurrentIndex(formatIndex);
            
            // Find matching config
            if (m_formatDB.contains(system)) {
                for (const FormatConfig &cfg : m_formatDB[system]) {
                    if (cfg.format == format) {
                        m_currentConfig = cfg;
                        updateVersionsForFormat(format);
                        applyFormatConfig(cfg);
                        break;
                    }
                }
            }
        }
    }
    
    // Re-enable signals
    ui->comboSystem->blockSignals(false);
    ui->comboFormat->blockSignals(false);
    
    qDebug() << "Format set to" << system << "/" << format;
}

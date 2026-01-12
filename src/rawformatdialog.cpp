/**
 * @file rawformatdialog.cpp
 * @brief RAW File Format Configuration Dialog Implementation
 * 
 * @date 2026-01-12
 */

#include "rawformatdialog.h"
#include "ui_rawformatdialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

RawFormatDialog::RawFormatDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RawFormatDialog)
{
    ui->setupUi(this);
    
    setupTrackTypes();
    setupLayoutPresets();
    setupConnections();
    
    // Initial calculation
    updateCalculatedValues();
}

RawFormatDialog::~RawFormatDialog()
{
    delete ui;
}

void RawFormatDialog::setupTrackTypes()
{
    ui->comboTrackType->clear();
    ui->comboTrackType->addItem(tr("IBM FM"), "ibm_fm");
    ui->comboTrackType->addItem(tr("IBM MFM"), "ibm_mfm");
    ui->comboTrackType->addItem(tr("Amiga MFM"), "amiga_mfm");
    ui->comboTrackType->addItem(tr("Amiga MFM HD"), "amiga_mfm_hd");
    ui->comboTrackType->addItem(tr("Atari ST MFM"), "atari_mfm");
    ui->comboTrackType->addItem(tr("C64 GCR"), "c64_gcr");
    ui->comboTrackType->addItem(tr("Apple II GCR"), "apple_gcr");
    ui->comboTrackType->addItem(tr("Apple II GCR 6&2"), "apple_gcr_62");
    ui->comboTrackType->addItem(tr("Victor 9000 GCR"), "victor_gcr");
    ui->comboTrackType->addItem(tr("E-Emu"), "eemu");
    ui->comboTrackType->addItem(tr("AED 6200P"), "aed6200p");
    ui->comboTrackType->addItem(tr("TYCOM"), "tycom");
    ui->comboTrackType->addItem(tr("MEMBRAIN"), "membrain");
    ui->comboTrackType->addItem(tr("Arburg"), "arburg");
    
    // Default to IBM MFM
    ui->comboTrackType->setCurrentIndex(1);
}

void RawFormatDialog::setupLayoutPresets()
{
    ui->comboLayout->clear();
    ui->comboLayout->addItem(tr("Custom Disk Layout"), "custom");
    ui->comboLayout->addItem(tr("--- PC/DOS ---"), "");
    ui->comboLayout->addItem(tr("PC 360K (5.25\" DD)"), "pc_360k");
    ui->comboLayout->addItem(tr("PC 720K (3.5\" DD)"), "pc_720k");
    ui->comboLayout->addItem(tr("PC 1.2M (5.25\" HD)"), "pc_1200k");
    ui->comboLayout->addItem(tr("PC 1.44M (3.5\" HD)"), "pc_1440k");
    ui->comboLayout->addItem(tr("PC 2.88M (3.5\" ED)"), "pc_2880k");
    ui->comboLayout->addItem(tr("--- Amiga ---"), "");
    ui->comboLayout->addItem(tr("Amiga DD (880K)"), "amiga_dd");
    ui->comboLayout->addItem(tr("Amiga HD (1.76M)"), "amiga_hd");
    ui->comboLayout->addItem(tr("--- Atari ---"), "");
    ui->comboLayout->addItem(tr("Atari ST SS (360K)"), "atari_ss");
    ui->comboLayout->addItem(tr("Atari ST DS (720K)"), "atari_ds");
    ui->comboLayout->addItem(tr("--- Commodore ---"), "");
    ui->comboLayout->addItem(tr("C64 1541 (170K)"), "c64_1541");
    ui->comboLayout->addItem(tr("C64 1571 (340K)"), "c64_1571");
    ui->comboLayout->addItem(tr("C64 1581 (800K)"), "c64_1581");
    ui->comboLayout->addItem(tr("--- Apple ---"), "");
    ui->comboLayout->addItem(tr("Apple II DOS 3.3"), "apple_dos33");
    ui->comboLayout->addItem(tr("Apple II ProDOS"), "apple_prodos");
    ui->comboLayout->addItem(tr("--- Other ---"), "");
    ui->comboLayout->addItem(tr("ZX Spectrum +3"), "spectrum_p3");
    ui->comboLayout->addItem(tr("Amstrad CPC"), "amstrad_cpc");
    ui->comboLayout->addItem(tr("MSX"), "msx");
    ui->comboLayout->addItem(tr("BBC Micro"), "bbc");
    ui->comboLayout->addItem(tr("FM-77 / FM Towns"), "fm77");
    ui->comboLayout->addItem(tr("NEC PC-98"), "pc98");
    ui->comboLayout->addItem(tr("Sharp X68000"), "x68000");
}

void RawFormatDialog::setupConnections()
{
    // Track type
    connect(ui->comboTrackType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RawFormatDialog::onTrackTypeChanged);
    
    // Geometry changes
    connect(ui->spinTracks, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RawFormatDialog::onGeometryChanged);
    connect(ui->comboSides, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RawFormatDialog::onGeometryChanged);
    connect(ui->spinSectors, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RawFormatDialog::onGeometryChanged);
    connect(ui->comboSectorSize, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RawFormatDialog::onGeometryChanged);
    
    // Layout preset
    connect(ui->comboLayout, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RawFormatDialog::onLayoutPresetChanged);
    
    // Auto GAP3
    connect(ui->checkAutoGap3, &QCheckBox::toggled,
            this, &RawFormatDialog::onAutoGap3Toggled);
    
    // Buttons
    connect(ui->btnSaveConfig, &QPushButton::clicked,
            this, &RawFormatDialog::onSaveConfig);
    connect(ui->btnLoadConfig, &QPushButton::clicked,
            this, &RawFormatDialog::onLoadConfig);
    connect(ui->btnLoadRaw, &QPushButton::clicked,
            this, &RawFormatDialog::onLoadRawFile);
    connect(ui->btnCreateEmpty, &QPushButton::clicked,
            this, &RawFormatDialog::onCreateEmpty);
    connect(ui->btnClose, &QPushButton::clicked,
            this, &QDialog::accept);
}

void RawFormatDialog::onTrackTypeChanged(int index)
{
    Q_UNUSED(index);
    QString type = ui->comboTrackType->currentData().toString();
    
    // Set default bitrate for track type
    if (type == "ibm_fm") {
        ui->spinBitrate->setValue(125000);
    } else if (type == "ibm_mfm" || type == "atari_mfm") {
        ui->spinBitrate->setValue(250000);
    } else if (type == "amiga_mfm") {
        ui->spinBitrate->setValue(250000);
    } else if (type == "amiga_mfm_hd") {
        ui->spinBitrate->setValue(500000);
    } else if (type == "c64_gcr") {
        ui->spinBitrate->setValue(260000);  // Variable, but ~260K average
    } else if (type.startsWith("apple_gcr")) {
        ui->spinBitrate->setValue(250000);
    }
    
    updateCalculatedValues();
}

void RawFormatDialog::onGeometryChanged()
{
    updateCalculatedValues();
}

void RawFormatDialog::onLayoutPresetChanged(int index)
{
    Q_UNUSED(index);
    QString preset = ui->comboLayout->currentData().toString();
    
    if (preset.isEmpty() || preset == "custom") {
        return;  // Separator or custom
    }
    
    applyLayoutPreset(preset);
}

void RawFormatDialog::applyLayoutPreset(const QString& preset)
{
    // Block signals during update
    ui->spinTracks->blockSignals(true);
    ui->comboSides->blockSignals(true);
    ui->spinSectors->blockSignals(true);
    ui->comboSectorSize->blockSignals(true);
    ui->comboTrackType->blockSignals(true);
    ui->spinBitrate->blockSignals(true);
    
    if (preset == "pc_360k") {
        ui->comboTrackType->setCurrentIndex(1);  // IBM MFM
        ui->spinTracks->setValue(40);
        ui->comboSides->setCurrentIndex(1);  // 2 sides
        ui->spinSectors->setValue(9);
        ui->comboSectorSize->setCurrentIndex(2);  // 512
        ui->spinBitrate->setValue(250000);
    } else if (preset == "pc_720k") {
        ui->comboTrackType->setCurrentIndex(1);
        ui->spinTracks->setValue(80);
        ui->comboSides->setCurrentIndex(1);
        ui->spinSectors->setValue(9);
        ui->comboSectorSize->setCurrentIndex(2);
        ui->spinBitrate->setValue(250000);
    } else if (preset == "pc_1200k") {
        ui->comboTrackType->setCurrentIndex(1);
        ui->spinTracks->setValue(80);
        ui->comboSides->setCurrentIndex(1);
        ui->spinSectors->setValue(15);
        ui->comboSectorSize->setCurrentIndex(2);
        ui->spinBitrate->setValue(500000);
    } else if (preset == "pc_1440k") {
        ui->comboTrackType->setCurrentIndex(1);
        ui->spinTracks->setValue(80);
        ui->comboSides->setCurrentIndex(1);
        ui->spinSectors->setValue(18);
        ui->comboSectorSize->setCurrentIndex(2);
        ui->spinBitrate->setValue(500000);
    } else if (preset == "pc_2880k") {
        ui->comboTrackType->setCurrentIndex(1);
        ui->spinTracks->setValue(80);
        ui->comboSides->setCurrentIndex(1);
        ui->spinSectors->setValue(36);
        ui->comboSectorSize->setCurrentIndex(2);
        ui->spinBitrate->setValue(1000000);
    } else if (preset == "amiga_dd") {
        ui->comboTrackType->setCurrentIndex(2);  // Amiga MFM
        ui->spinTracks->setValue(80);
        ui->comboSides->setCurrentIndex(1);
        ui->spinSectors->setValue(11);
        ui->comboSectorSize->setCurrentIndex(2);
        ui->spinBitrate->setValue(250000);
    } else if (preset == "amiga_hd") {
        ui->comboTrackType->setCurrentIndex(3);  // Amiga MFM HD
        ui->spinTracks->setValue(80);
        ui->comboSides->setCurrentIndex(1);
        ui->spinSectors->setValue(22);
        ui->comboSectorSize->setCurrentIndex(2);
        ui->spinBitrate->setValue(500000);
    } else if (preset == "atari_ss") {
        ui->comboTrackType->setCurrentIndex(4);  // Atari ST
        ui->spinTracks->setValue(80);
        ui->comboSides->setCurrentIndex(0);  // 1 side
        ui->spinSectors->setValue(9);
        ui->comboSectorSize->setCurrentIndex(2);
        ui->spinBitrate->setValue(250000);
    } else if (preset == "atari_ds") {
        ui->comboTrackType->setCurrentIndex(4);
        ui->spinTracks->setValue(80);
        ui->comboSides->setCurrentIndex(1);
        ui->spinSectors->setValue(9);
        ui->comboSectorSize->setCurrentIndex(2);
        ui->spinBitrate->setValue(250000);
    } else if (preset == "c64_1541") {
        ui->comboTrackType->setCurrentIndex(5);  // C64 GCR
        ui->spinTracks->setValue(35);
        ui->comboSides->setCurrentIndex(0);
        ui->spinSectors->setValue(21);  // Variable, max 21
        ui->comboSectorSize->setCurrentIndex(1);  // 256
        ui->spinBitrate->setValue(260000);
    } else if (preset == "c64_1571") {
        ui->comboTrackType->setCurrentIndex(5);
        ui->spinTracks->setValue(35);
        ui->comboSides->setCurrentIndex(1);
        ui->spinSectors->setValue(21);
        ui->comboSectorSize->setCurrentIndex(1);
        ui->spinBitrate->setValue(260000);
    } else if (preset == "c64_1581") {
        ui->comboTrackType->setCurrentIndex(1);  // MFM for 1581
        ui->spinTracks->setValue(80);
        ui->comboSides->setCurrentIndex(1);
        ui->spinSectors->setValue(10);
        ui->comboSectorSize->setCurrentIndex(2);
        ui->spinBitrate->setValue(250000);
    } else if (preset == "apple_dos33") {
        ui->comboTrackType->setCurrentIndex(6);  // Apple GCR
        ui->spinTracks->setValue(35);
        ui->comboSides->setCurrentIndex(0);
        ui->spinSectors->setValue(16);
        ui->comboSectorSize->setCurrentIndex(1);  // 256
        ui->spinBitrate->setValue(250000);
    } else if (preset == "apple_prodos") {
        ui->comboTrackType->setCurrentIndex(7);  // Apple GCR 6&2
        ui->spinTracks->setValue(35);
        ui->comboSides->setCurrentIndex(0);
        ui->spinSectors->setValue(16);
        ui->comboSectorSize->setCurrentIndex(2);  // 512
        ui->spinBitrate->setValue(250000);
    } else if (preset == "spectrum_p3") {
        ui->comboTrackType->setCurrentIndex(1);  // IBM MFM
        ui->spinTracks->setValue(40);
        ui->comboSides->setCurrentIndex(0);
        ui->spinSectors->setValue(9);
        ui->comboSectorSize->setCurrentIndex(2);
        ui->spinBitrate->setValue(250000);
    } else if (preset == "amstrad_cpc") {
        ui->comboTrackType->setCurrentIndex(1);
        ui->spinTracks->setValue(40);
        ui->comboSides->setCurrentIndex(0);
        ui->spinSectors->setValue(9);
        ui->comboSectorSize->setCurrentIndex(2);
        ui->spinBitrate->setValue(250000);
    } else if (preset == "msx") {
        ui->comboTrackType->setCurrentIndex(1);
        ui->spinTracks->setValue(80);
        ui->comboSides->setCurrentIndex(1);
        ui->spinSectors->setValue(9);
        ui->comboSectorSize->setCurrentIndex(2);
        ui->spinBitrate->setValue(250000);
    } else if (preset == "pc98") {
        ui->comboTrackType->setCurrentIndex(1);
        ui->spinTracks->setValue(77);
        ui->comboSides->setCurrentIndex(1);
        ui->spinSectors->setValue(8);
        ui->comboSectorSize->setCurrentIndex(3);  // 1024
        ui->spinBitrate->setValue(500000);
    } else if (preset == "x68000") {
        ui->comboTrackType->setCurrentIndex(1);
        ui->spinTracks->setValue(77);
        ui->comboSides->setCurrentIndex(1);
        ui->spinSectors->setValue(8);
        ui->comboSectorSize->setCurrentIndex(3);
        ui->spinBitrate->setValue(500000);
    }
    
    // Restore signals
    ui->spinTracks->blockSignals(false);
    ui->comboSides->blockSignals(false);
    ui->spinSectors->blockSignals(false);
    ui->comboSectorSize->blockSignals(false);
    ui->comboTrackType->blockSignals(false);
    ui->spinBitrate->blockSignals(false);
    
    updateCalculatedValues();
}

void RawFormatDialog::onAutoGap3Toggled(bool checked)
{
    ui->spinGap3->setEnabled(!checked);
    
    if (checked) {
        // Calculate auto GAP3 based on track format
        int sectors = ui->spinSectors->value();
        // Rough calculation - actual depends on track type
        int gap3 = (6250 - (sectors * 640)) / sectors;
        if (gap3 < 1) gap3 = 1;
        if (gap3 > 255) gap3 = 255;
        ui->spinGap3->setValue(gap3);
    }
}

void RawFormatDialog::updateCalculatedValues()
{
    int totalSectors = calculateTotalSectors();
    int totalSize = calculateTotalSize();
    int formatValue = calculateFormatValue();
    
    ui->editTotalSectors->setText(QString::number(totalSectors));
    ui->editTotalSize->setText(QString::number(totalSize));
    ui->editFormatValue->setText(QString::number(formatValue));
}

int RawFormatDialog::calculateTotalSectors() const
{
    int tracks = ui->spinTracks->value();
    int sides = ui->comboSides->currentIndex() + 1;  // 1 or 2
    int sectors = ui->spinSectors->value();
    
    return tracks * sides * sectors;
}

int RawFormatDialog::calculateTotalSize() const
{
    int totalSectors = calculateTotalSectors();
    
    // Parse sector size from combo
    QString sizeText = ui->comboSectorSize->currentText();
    int sectorSize = sizeText.split(' ').first().toInt();
    
    return totalSectors * sectorSize;
}

int RawFormatDialog::calculateFormatValue() const
{
    // Format value is a packed representation used by some tools
    // Format: (tracks << 16) | (sides << 8) | sectors
    int tracks = ui->spinTracks->value();
    int sides = ui->comboSides->currentIndex() + 1;
    int sectors = ui->spinSectors->value();
    
    return (tracks << 16) | (sides << 8) | sectors;
}

RawFormatDialog::RawConfig RawFormatDialog::getConfig() const
{
    RawConfig cfg;
    
    cfg.trackType = ui->comboTrackType->currentText();
    cfg.tracks = ui->spinTracks->value();
    cfg.sides = ui->comboSides->currentIndex() + 1;
    cfg.sectorsPerTrack = ui->spinSectors->value();
    
    QString sizeText = ui->comboSectorSize->currentText();
    cfg.sectorSize = sizeText.split(' ').first().toInt();
    
    cfg.bitrate = ui->spinBitrate->value();
    cfg.rpm = ui->comboRPM->currentText().toInt();
    
    cfg.sectorIdStart = ui->spinSectorIdStart->value();
    cfg.interleave = ui->spinInterleave->value();
    cfg.skew = ui->spinSkew->value();
    cfg.interSideNumbering = ui->checkInterSideNumbering->isChecked();
    cfg.reverseSide = ui->checkReverseSide->isChecked();
    cfg.sidesGrouped = ui->checkSidesGrouped->isChecked();
    cfg.sideBasedSectorNum = ui->checkSideBased->isChecked();
    
    cfg.gap3Length = ui->spinGap3->value();
    cfg.preGapLength = ui->spinPreGap->value();
    cfg.autoGap3 = ui->checkAutoGap3->isChecked();
    
    cfg.totalSectors = calculateTotalSectors();
    cfg.totalSize = calculateTotalSize();
    cfg.formatValue = calculateFormatValue();
    
    cfg.layoutPreset = ui->comboLayout->currentData().toString();
    
    return cfg;
}

void RawFormatDialog::setConfig(const RawConfig& cfg)
{
    // Find and set track type
    int typeIdx = ui->comboTrackType->findText(cfg.trackType, Qt::MatchContains);
    if (typeIdx >= 0) ui->comboTrackType->setCurrentIndex(typeIdx);
    
    ui->spinTracks->setValue(cfg.tracks);
    ui->comboSides->setCurrentIndex(cfg.sides - 1);
    ui->spinSectors->setValue(cfg.sectorsPerTrack);
    
    // Find sector size
    QString sizeStr = QString::number(cfg.sectorSize) + " Bytes";
    int sizeIdx = ui->comboSectorSize->findText(sizeStr);
    if (sizeIdx >= 0) ui->comboSectorSize->setCurrentIndex(sizeIdx);
    
    ui->spinBitrate->setValue(cfg.bitrate);
    
    int rpmIdx = ui->comboRPM->findText(QString::number(cfg.rpm));
    if (rpmIdx >= 0) ui->comboRPM->setCurrentIndex(rpmIdx);
    
    ui->spinSectorIdStart->setValue(cfg.sectorIdStart);
    ui->spinInterleave->setValue(cfg.interleave);
    ui->spinSkew->setValue(cfg.skew);
    ui->checkInterSideNumbering->setChecked(cfg.interSideNumbering);
    ui->checkReverseSide->setChecked(cfg.reverseSide);
    ui->checkSidesGrouped->setChecked(cfg.sidesGrouped);
    ui->checkSideBased->setChecked(cfg.sideBasedSectorNum);
    
    ui->spinGap3->setValue(cfg.gap3Length);
    ui->spinPreGap->setValue(cfg.preGapLength);
    ui->checkAutoGap3->setChecked(cfg.autoGap3);
    
    updateCalculatedValues();
}

void RawFormatDialog::onSaveConfig()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Save RAW Configuration"), QString(),
        tr("RAW Config (*.rawcfg);;All Files (*.*)"));
    
    if (filename.isEmpty()) return;
    
    QSettings cfg(filename, QSettings::IniFormat);
    RawConfig c = getConfig();
    
    cfg.setValue("TrackType", c.trackType);
    cfg.setValue("Tracks", c.tracks);
    cfg.setValue("Sides", c.sides);
    cfg.setValue("SectorsPerTrack", c.sectorsPerTrack);
    cfg.setValue("SectorSize", c.sectorSize);
    cfg.setValue("Bitrate", c.bitrate);
    cfg.setValue("RPM", c.rpm);
    cfg.setValue("SectorIdStart", c.sectorIdStart);
    cfg.setValue("Interleave", c.interleave);
    cfg.setValue("Skew", c.skew);
    cfg.setValue("InterSideNumbering", c.interSideNumbering);
    cfg.setValue("ReverseSide", c.reverseSide);
    cfg.setValue("SidesGrouped", c.sidesGrouped);
    cfg.setValue("SideBased", c.sideBasedSectorNum);
    cfg.setValue("Gap3", c.gap3Length);
    cfg.setValue("PreGap", c.preGapLength);
    cfg.setValue("AutoGap3", c.autoGap3);
    
    cfg.sync();
    
    QMessageBox::information(this, tr("Saved"),
        tr("Configuration saved to:\n%1").arg(filename));
}

void RawFormatDialog::onLoadConfig()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Load RAW Configuration"), QString(),
        tr("RAW Config (*.rawcfg);;All Files (*.*)"));
    
    if (filename.isEmpty()) return;
    
    QSettings cfg(filename, QSettings::IniFormat);
    RawConfig c;
    
    c.trackType = cfg.value("TrackType", "IBM MFM").toString();
    c.tracks = cfg.value("Tracks", 80).toInt();
    c.sides = cfg.value("Sides", 2).toInt();
    c.sectorsPerTrack = cfg.value("SectorsPerTrack", 9).toInt();
    c.sectorSize = cfg.value("SectorSize", 512).toInt();
    c.bitrate = cfg.value("Bitrate", 250000).toInt();
    c.rpm = cfg.value("RPM", 300).toInt();
    c.sectorIdStart = cfg.value("SectorIdStart", 1).toInt();
    c.interleave = cfg.value("Interleave", 1).toInt();
    c.skew = cfg.value("Skew", 0).toInt();
    c.interSideNumbering = cfg.value("InterSideNumbering", false).toBool();
    c.reverseSide = cfg.value("ReverseSide", false).toBool();
    c.sidesGrouped = cfg.value("SidesGrouped", false).toBool();
    c.sideBasedSectorNum = cfg.value("SideBased", false).toBool();
    c.gap3Length = cfg.value("Gap3", 27).toInt();
    c.preGapLength = cfg.value("PreGap", 0).toInt();
    c.autoGap3 = cfg.value("AutoGap3", false).toBool();
    
    setConfig(c);
}

void RawFormatDialog::onLoadRawFile()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Load RAW File"), QString(),
        tr("RAW Files (*.raw *.bin *.img);;All Files (*.*)"));
    
    if (filename.isEmpty()) return;
    
    m_selectedFile = filename;
    emit loadRawFile(filename);
    emit configurationApplied(getConfig());
    accept();
}

void RawFormatDialog::onCreateEmpty()
{
    emit createEmptyFloppy(getConfig());
    emit configurationApplied(getConfig());
    accept();
}

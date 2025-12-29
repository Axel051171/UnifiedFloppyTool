/**
 * @file presetmanager.cpp
 * @brief Implementation of Preset Management System
 */

#include "presetmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileDialog>
#include <QMessageBox>

// ============================================================================
// FormatPreset Implementation
// ============================================================================

qint64 FormatPreset::calculateCapacity() const
{
    return static_cast<qint64>(tracks) * sides * sectorsPerTrack * sectorSize;
}

QJsonObject FormatPreset::toJson() const
{
    QJsonObject json;
    json["id"] = id;
    json["name"] = name;
    json["description"] = description;
    json["category"] = category;
    json["isBuiltIn"] = isBuiltIn;
    
    json["tracks"] = tracks;
    json["sides"] = sides;
    json["sectorsPerTrack"] = sectorsPerTrack;
    json["sectorSize"] = sectorSize;
    
    json["encoding"] = static_cast<int>(encoding);
    json["bitrate"] = bitrate;
    json["rpm"] = rpm;
    
    json["interleave"] = interleave;
    json["gap3Length"] = gap3Length;
    json["firstSectorID"] = firstSectorID;
    json["autoDetectVariants"] = autoDetectVariants;
    
    return json;
}

FormatPreset FormatPreset::fromJson(const QJsonObject& json)
{
    FormatPreset preset;
    preset.id = json["id"].toString();
    preset.name = json["name"].toString();
    preset.description = json["description"].toString();
    preset.category = json["category"].toString();
    preset.isBuiltIn = json["isBuiltIn"].toBool();
    
    preset.tracks = json["tracks"].toInt();
    preset.sides = json["sides"].toInt();
    preset.sectorsPerTrack = json["sectorsPerTrack"].toInt();
    preset.sectorSize = json["sectorSize"].toInt();
    
    preset.encoding = static_cast<Encoding>(json["encoding"].toInt());
    preset.bitrate = json["bitrate"].toInt();
    preset.rpm = json["rpm"].toInt();
    
    preset.interleave = json["interleave"].toInt();
    preset.gap3Length = json["gap3Length"].toInt();
    preset.firstSectorID = json["firstSectorID"].toInt();
    preset.autoDetectVariants = json["autoDetectVariants"].toBool();
    
    return preset;
}

bool FormatPreset::validate(QString* errorMsg) const
{
    if (tracks < 1 || tracks > 255) {
        if (errorMsg) *errorMsg = "Tracks must be 1-255";
        return false;
    }
    
    if (sides < 1 || sides > 2) {
        if (errorMsg) *errorMsg = "Sides must be 1 or 2";
        return false;
    }
    
    if (sectorsPerTrack < 1 || sectorsPerTrack > 255) {
        if (errorMsg) *errorMsg = "Sectors per track must be 1-255";
        return false;
    }
    
    if (sectorSize != 128 && sectorSize != 256 && sectorSize != 512 &&
        sectorSize != 1024 && sectorSize != 2048) {
        if (errorMsg) *errorMsg = "Sector size must be power of 2 (128-2048)";
        return false;
    }
    
    return true;
}

// ============================================================================
// PresetManager Implementation
// ============================================================================

PresetManager& PresetManager::instance()
{
    static PresetManager instance;
    return instance;
}

PresetManager::PresetManager()
{
    initializeBuiltInPresets();
    loadPresets();
}

void PresetManager::initializeBuiltInPresets()
{
    // PC 1.44MB
    {
        FormatPreset preset;
        preset.id = "pc_144mb";
        preset.name = "PC 1.44MB (3.5\" HD)";
        preset.description = "Standard PC 3.5\" High-Density floppy";
        preset.category = "PC";
        preset.isBuiltIn = true;
        preset.tracks = 80;
        preset.sides = 2;
        preset.sectorsPerTrack = 18;
        preset.sectorSize = 512;
        preset.encoding = FormatPreset::Encoding::MFM;
        preset.bitrate = 500000;
        preset.rpm = 300;
        preset.interleave = 1;
        preset.gap3Length = 27;
        preset.firstSectorID = 1;
        preset.autoDetectVariants = true;
        m_presets.push_back(preset);
    }
    
    // PC 720K
    {
        FormatPreset preset;
        preset.id = "pc_720k";
        preset.name = "PC 720K (3.5\" DD)";
        preset.description = "Standard PC 3.5\" Double-Density floppy";
        preset.category = "PC";
        preset.isBuiltIn = true;
        preset.tracks = 80;
        preset.sides = 2;
        preset.sectorsPerTrack = 9;
        preset.sectorSize = 512;
        preset.encoding = FormatPreset::Encoding::MFM;
        preset.bitrate = 250000;
        preset.rpm = 300;
        preset.interleave = 1;
        preset.gap3Length = 80;
        preset.firstSectorID = 1;
        preset.autoDetectVariants = true;
        m_presets.push_back(preset);
    }

    // PC 360K
    {
        FormatPreset preset;
        preset.id = "pc_360k";
        preset.name = "PC 360K (5.25\" DD)";
        preset.description = "IBM PC 5.25\" Double-Density 360K floppy";
        preset.category = "PC";
        preset.isBuiltIn = true;
        preset.tracks = 40;
        preset.sides = 2;
        preset.sectorsPerTrack = 9;
        preset.sectorSize = 512;
        preset.encoding = FormatPreset::Encoding::MFM;
        preset.bitrate = 250000;
        preset.rpm = 300;
        preset.interleave = 1;
        preset.gap3Length = 84;
        preset.firstSectorID = 1;
        preset.autoDetectVariants = true;
        m_presets.push_back(preset);
    }

    // PC 1.2M
    {
        FormatPreset preset;
        preset.id = "pc_12m";
        preset.name = "PC 1.2M (5.25\" HD)";
        preset.description = "IBM PC 5.25\" High-Density 1.2M floppy";
        preset.category = "PC";
        preset.isBuiltIn = true;
        preset.tracks = 80;
        preset.sides = 2;
        preset.sectorsPerTrack = 15;
        preset.sectorSize = 512;
        preset.encoding = FormatPreset::Encoding::MFM;
        preset.bitrate = 500000;
        preset.rpm = 360;
        preset.interleave = 1;
        preset.gap3Length = 84;
        preset.firstSectorID = 1;
        preset.autoDetectVariants = true;
        m_presets.push_back(preset);
    }

    // PC 2.88M
    {
        FormatPreset preset;
        preset.id = "pc_288m";
        preset.name = "PC 2.88M (3.5\" ED)";
        preset.description = "IBM PC 3.5\" Extended-Density 2.88M floppy";
        preset.category = "PC";
        preset.isBuiltIn = true;
        preset.tracks = 80;
        preset.sides = 2;
        preset.sectorsPerTrack = 36;
        preset.sectorSize = 512;
        preset.encoding = FormatPreset::Encoding::MFM;
        preset.bitrate = 1000000;
        preset.rpm = 300;
        preset.interleave = 1;
        preset.gap3Length = 80;
        preset.firstSectorID = 1;
        preset.autoDetectVariants = true;
        m_presets.push_back(preset);
    }
    
    // Amiga Standard
    {
        FormatPreset preset;
        preset.id = "amiga_std";
        preset.name = "Amiga Standard (880K)";
        preset.description = "Standard Amiga disk format";
        preset.category = "Amiga";
        preset.isBuiltIn = true;
        preset.tracks = 80;
        preset.sides = 2;
        preset.sectorsPerTrack = 11;
        preset.sectorSize = 512;
        preset.encoding = FormatPreset::Encoding::MFM;
        preset.bitrate = 250000;
        preset.rpm = 300;
        preset.interleave = 0;
        preset.gap3Length = 0;
        preset.firstSectorID = 0;
        preset.autoDetectVariants = true;
        m_presets.push_back(preset);
    }
    
    // C64 1541
    {
        FormatPreset preset;
        preset.id = "c64_1541";
        preset.name = "C64 1541 (170K)";
        preset.description = "Commodore 64 1541 disk drive";
        preset.category = "C64";
        preset.isBuiltIn = true;
        preset.tracks = 35;
        preset.sides = 1;
        preset.sectorsPerTrack = 21; // Average, varies by zone
        preset.sectorSize = 256;
        preset.encoding = FormatPreset::Encoding::GCR;
        preset.bitrate = 250000;
        preset.rpm = 300;
        preset.interleave = 10;
        preset.gap3Length = 0;
        preset.firstSectorID = 0;
        preset.autoDetectVariants = true;
        m_presets.push_back(preset);
    }

    // Apple II DOS 3.3
    {
        FormatPreset preset;
        preset.id = "apple2_dos33";
        preset.name = "Apple II DOS 3.3 (140K)";
        preset.description = "Apple II DOS 3.3 (35T/16S/256B)";
        preset.category = "Apple II";
        preset.isBuiltIn = true;
        preset.tracks = 35;
        preset.sides = 1;
        preset.sectorsPerTrack = 16;
        preset.sectorSize = 256;
        preset.encoding = FormatPreset::Encoding::GCR;
        preset.bitrate = 250000;
        preset.rpm = 300;
        preset.interleave = 0;
        preset.gap3Length = 0;
        preset.firstSectorID = 0;
        preset.autoDetectVariants = true;
        m_presets.push_back(preset);
    }
    
    // More presets can be added here...
}

const FormatPreset* PresetManager::getPreset(const QString& id) const
{
    for (const auto& preset : m_presets) {
        if (preset.id == id) {
            return &preset;
        }
    }
    return nullptr;
}

bool PresetManager::addPreset(const FormatPreset& preset)
{
    QString error;
    if (!preset.validate(&error)) {
        return false;
    }
    
    m_presets.push_back(preset);
    emit presetsChanged();
    emit presetAdded(preset.id);
    savePresets();
    
    return true;
}

bool PresetManager::updatePreset(const QString& id, const FormatPreset& preset)
{
    for (auto& p : m_presets) {
        if (p.id == id) {
            if (p.isBuiltIn) {
                return false; // Cannot modify built-in
            }
            
            QString error;
            if (!preset.validate(&error)) {
                return false;
            }
            
            p = preset;
            emit presetsChanged();
            savePresets();
            return true;
        }
    }
    return false;
}

bool PresetManager::deletePreset(const QString& id)
{
    for (auto it = m_presets.begin(); it != m_presets.end(); ++it) {
        if (it->id == id) {
            if (it->isBuiltIn) {
                return false; // Cannot delete built-in
            }
            
            m_presets.erase(it);
            emit presetsChanged();
            emit presetDeleted(id);
            savePresets();
            return true;
        }
    }
    return false;
}

bool PresetManager::importPreset(const QString& filepath)
{
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (doc.isNull() || !doc.isObject()) {
        return false;
    }
    
    FormatPreset preset = FormatPreset::fromJson(doc.object());
    return addPreset(preset);
}

bool PresetManager::exportPreset(const QString& id, const QString& filepath)
{
    const FormatPreset* preset = getPreset(id);
    if (!preset) {
        return false;
    }
    
    QJsonDocument doc(preset->toJson());
    
    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson());
    return true;
}

void PresetManager::loadPresets()
{
    QSettings settings("UnifiedFloppyTool", "Presets");
    int size = settings.beginReadArray("user_presets");
    
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        
        FormatPreset preset;
        preset.id = settings.value("id").toString();
        preset.name = settings.value("name").toString();
        preset.description = settings.value("description").toString();
        preset.category = settings.value("category").toString();
        preset.isBuiltIn = false;
        
        preset.tracks = settings.value("tracks").toInt();
        preset.sides = settings.value("sides").toInt();
        preset.sectorsPerTrack = settings.value("sectorsPerTrack").toInt();
        preset.sectorSize = settings.value("sectorSize").toInt();
        
        preset.encoding = static_cast<FormatPreset::Encoding>(
            settings.value("encoding").toInt());
        preset.bitrate = settings.value("bitrate").toInt();
        preset.rpm = settings.value("rpm").toInt();
        
        preset.interleave = settings.value("interleave").toInt();
        preset.gap3Length = settings.value("gap3Length").toInt();
        preset.firstSectorID = settings.value("firstSectorID").toInt();
        preset.autoDetectVariants = settings.value("autoDetectVariants").toBool();
        
        m_presets.push_back(preset);
    }
    
    settings.endArray();
}

void PresetManager::savePresets()
{
    QSettings settings("UnifiedFloppyTool", "Presets");
    settings.beginWriteArray("user_presets");
    
    int index = 0;
    for (const auto& preset : m_presets) {
        if (preset.isBuiltIn) continue; // Don't save built-ins
        
        settings.setArrayIndex(index++);
        settings.setValue("id", preset.id);
        settings.setValue("name", preset.name);
        settings.setValue("description", preset.description);
        settings.setValue("category", preset.category);
        
        settings.setValue("tracks", preset.tracks);
        settings.setValue("sides", preset.sides);
        settings.setValue("sectorsPerTrack", preset.sectorsPerTrack);
        settings.setValue("sectorSize", preset.sectorSize);
        
        settings.setValue("encoding", static_cast<int>(preset.encoding));
        settings.setValue("bitrate", preset.bitrate);
        settings.setValue("rpm", preset.rpm);
        
        settings.setValue("interleave", preset.interleave);
        settings.setValue("gap3Length", preset.gap3Length);
        settings.setValue("firstSectorID", preset.firstSectorID);
        settings.setValue("autoDetectVariants", preset.autoDetectVariants);
    }
    
    settings.endArray();
}

// ============================================================================
// PresetManagerDialog Implementation
// ============================================================================

PresetManagerDialog::PresetManagerDialog(QWidget *parent)
    : QDialog(parent)
    , m_modified(false)
{
    setWindowTitle("Preset Manager");
    resize(900, 700);
    
    createUI();
    populatePresetList();
}

void PresetManagerDialog::createUI()
{
    auto *mainLayout = new QHBoxLayout(this);
    
    // LEFT: Preset List
    auto *leftWidget = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftWidget);
    
    auto *listLabel = new QLabel("Presets:", leftWidget);
    leftLayout->addWidget(listLabel);
    
    m_presetList = new QListWidget(leftWidget);
    leftLayout->addWidget(m_presetList);
    
    // Buttons
    auto *buttonLayout = new QGridLayout();
    
    m_newButton = new QPushButton("+ New", leftWidget);
    m_copyButton = new QPushButton("Copy", leftWidget);
    m_deleteButton = new QPushButton("Delete", leftWidget);
    m_importButton = new QPushButton("Import", leftWidget);
    m_exportButton = new QPushButton("Export", leftWidget);
    
    buttonLayout->addWidget(m_newButton, 0, 0);
    buttonLayout->addWidget(m_copyButton, 0, 1);
    buttonLayout->addWidget(m_deleteButton, 1, 0);
    buttonLayout->addWidget(m_importButton, 1, 1);
    buttonLayout->addWidget(m_exportButton, 2, 0, 1, 2);
    
    leftLayout->addLayout(buttonLayout);
    mainLayout->addWidget(leftWidget, 1);
    
    // RIGHT: Preset Details
    auto *rightWidget = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightWidget);
    
    // Basic Info
    auto *infoGroup = new QGroupBox("Preset Information", rightWidget);
    auto *infoLayout = new QFormLayout(infoGroup);
    
    m_nameEdit = new QLineEdit(infoGroup);
    infoLayout->addRow("Name:", m_nameEdit);
    
    m_descriptionEdit = new QTextEdit(infoGroup);
    m_descriptionEdit->setMaximumHeight(80);
    infoLayout->addRow("Description:", m_descriptionEdit);
    
    m_categoryCombo = new QComboBox(infoGroup);
    m_categoryCombo->addItems({"PC", "Amiga", "C64", "Atari", "Apple", "Custom"});
    infoLayout->addRow("Category:", m_categoryCombo);
    
    rightLayout->addWidget(infoGroup);
    
    // Geometry
    auto *geomGroup = new QGroupBox("Geometry", rightWidget);
    auto *geomLayout = new QFormLayout(geomGroup);
    
    m_tracksSpinBox = new QSpinBox(geomGroup);
    m_tracksSpinBox->setRange(1, 255);
    geomLayout->addRow("Tracks:", m_tracksSpinBox);
    
    m_sidesSpinBox = new QSpinBox(geomGroup);
    m_sidesSpinBox->setRange(1, 2);
    geomLayout->addRow("Sides:", m_sidesSpinBox);
    
    m_sectorsPerTrackSpinBox = new QSpinBox(geomGroup);
    m_sectorsPerTrackSpinBox->setRange(1, 255);
    geomLayout->addRow("Sectors/Track:", m_sectorsPerTrackSpinBox);
    
    m_sectorSizeCombo = new QComboBox(geomGroup);
    m_sectorSizeCombo->addItems({"128", "256", "512", "1024", "2048"});
    m_sectorSizeCombo->setCurrentText("512");
    geomLayout->addRow("Sector Size:", m_sectorSizeCombo);
    
    m_capacityLabel = new QLabel("Capacity: 0 KB", geomGroup);
    m_capacityLabel->setStyleSheet("QLabel { font-weight: bold; color: #00AA00; }");
    geomLayout->addRow("", m_capacityLabel);
    
    rightLayout->addWidget(geomGroup);
    
    // Encoding
    auto *encGroup = new QGroupBox("Encoding", rightWidget);
    auto *encLayout = new QFormLayout(encGroup);
    
    m_encodingCombo = new QComboBox(encGroup);
    m_encodingCombo->addItems({"MFM", "FM", "GCR", "Auto"});
    encLayout->addRow("Type:", m_encodingCombo);
    
    m_bitrateSpinBox = new QSpinBox(encGroup);
    m_bitrateSpinBox->setRange(0, 1000000);
    m_bitrateSpinBox->setSingleStep(10000);
    m_bitrateSpinBox->setValue(250000);
    encLayout->addRow("Bitrate:", m_bitrateSpinBox);
    
    m_rpmCombo = new QComboBox(encGroup);
    m_rpmCombo->addItems({"288", "300", "360", "600"});
    m_rpmCombo->setCurrentText("300");
    encLayout->addRow("RPM:", m_rpmCombo);
    
    rightLayout->addWidget(encGroup);
    
    // Advanced
    auto *advGroup = new QGroupBox("Advanced", rightWidget);
    auto *advLayout = new QFormLayout(advGroup);
    
    m_interleaveSpinBox = new QSpinBox(advGroup);
    m_interleaveSpinBox->setRange(0, 255);
    m_interleaveSpinBox->setValue(1);
    advLayout->addRow("Interleave:", m_interleaveSpinBox);
    
    m_gap3Combo = new QComboBox(advGroup);
    m_gap3Combo->addItems({"Auto", "27", "54", "84"});
    advLayout->addRow("GAP3 Length:", m_gap3Combo);
    
    m_firstSectorSpinBox = new QSpinBox(advGroup);
    m_firstSectorSpinBox->setRange(0, 1);
    m_firstSectorSpinBox->setValue(1);
    advLayout->addRow("First Sector ID:", m_firstSectorSpinBox);
    
    m_autoDetectCheck = new QCheckBox("Auto-detect variants", advGroup);
    m_autoDetectCheck->setChecked(true);
    advLayout->addRow("", m_autoDetectCheck);
    
    rightLayout->addWidget(advGroup);
    
    // Spacer
    rightLayout->addStretch();
    
    // Bottom buttons
    auto *buttonRow = new QHBoxLayout();
    m_saveButton = new QPushButton("💾 Save", rightWidget);
    m_closeButton = new QPushButton("Close", rightWidget);
    buttonRow->addWidget(m_saveButton);
    buttonRow->addWidget(m_closeButton);
    rightLayout->addLayout(buttonRow);
    
    mainLayout->addWidget(rightWidget, 2);
    
    // Connect signals
    connect(m_presetList, &QListWidget::itemClicked, 
            this, &PresetManagerDialog::onPresetSelected);
    connect(m_newButton, &QPushButton::clicked, 
            this, &PresetManagerDialog::onNewClicked);
    connect(m_copyButton, &QPushButton::clicked, 
            this, &PresetManagerDialog::onCopyClicked);
    connect(m_deleteButton, &QPushButton::clicked, 
            this, &PresetManagerDialog::onDeleteClicked);
    connect(m_importButton, &QPushButton::clicked, 
            this, &PresetManagerDialog::onImportClicked);
    connect(m_exportButton, &QPushButton::clicked, 
            this, &PresetManagerDialog::onExportClicked);
    connect(m_saveButton, &QPushButton::clicked, 
            this, &PresetManagerDialog::onSaveClicked);
    connect(m_closeButton, &QPushButton::clicked, 
            this, &QDialog::accept);
    
    // Auto-update capacity
    connect(m_tracksSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PresetManagerDialog::onPresetChanged);
    connect(m_sidesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PresetManagerDialog::onPresetChanged);
    connect(m_sectorsPerTrackSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PresetManagerDialog::onPresetChanged);
    connect(m_sectorSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PresetManagerDialog::onPresetChanged);
}

void PresetManagerDialog::populatePresetList()
{
    m_presetList->clear();
    
    auto& manager = PresetManager::instance();
    for (const auto& preset : manager.getPresets()) {
        QString displayName = preset.name;
        if (preset.isBuiltIn) {
            displayName += " [Built-in]";
        }
        
        auto *item = new QListWidgetItem(displayName, m_presetList);
        item->setData(Qt::UserRole, preset.id);
    }
}

void PresetManagerDialog::loadPresetDetails(const FormatPreset* preset)
{
    if (!preset) return;
    
    m_nameEdit->setText(preset->name);
    m_descriptionEdit->setPlainText(preset->description);
    m_categoryCombo->setCurrentText(preset->category);
    
    m_tracksSpinBox->setValue(preset->tracks);
    m_sidesSpinBox->setValue(preset->sides);
    m_sectorsPerTrackSpinBox->setValue(preset->sectorsPerTrack);
    m_sectorSizeCombo->setCurrentText(QString::number(preset->sectorSize));
    
    m_encodingCombo->setCurrentIndex(static_cast<int>(preset->encoding));
    m_bitrateSpinBox->setValue(preset->bitrate);
    m_rpmCombo->setCurrentText(QString::number(preset->rpm));
    
    m_interleaveSpinBox->setValue(preset->interleave);
    m_gap3Combo->setCurrentText(preset->gap3Length == 0 ? "Auto" : 
                                 QString::number(preset->gap3Length));
    m_firstSectorSpinBox->setValue(preset->firstSectorID);
    m_autoDetectCheck->setChecked(preset->autoDetectVariants);
    
    m_currentPresetId = preset->id;
    m_modified = false;
    
    onPresetChanged(); // Update capacity
}

FormatPreset PresetManagerDialog::getCurrentPresetFromUI() const
{
    FormatPreset preset;
    preset.id = m_currentPresetId;
    preset.name = m_nameEdit->text();
    preset.description = m_descriptionEdit->toPlainText();
    preset.category = m_categoryCombo->currentText();
    preset.isBuiltIn = false;
    
    preset.tracks = m_tracksSpinBox->value();
    preset.sides = m_sidesSpinBox->value();
    preset.sectorsPerTrack = m_sectorsPerTrackSpinBox->value();
    preset.sectorSize = m_sectorSizeCombo->currentText().toInt();
    
    preset.encoding = static_cast<FormatPreset::Encoding>(
        m_encodingCombo->currentIndex());
    preset.bitrate = m_bitrateSpinBox->value();
    preset.rpm = m_rpmCombo->currentText().toInt();
    
    preset.interleave = m_interleaveSpinBox->value();
    preset.gap3Length = (m_gap3Combo->currentText() == "Auto") ? 0 :
                        m_gap3Combo->currentText().toInt();
    preset.firstSectorID = m_firstSectorSpinBox->value();
    preset.autoDetectVariants = m_autoDetectCheck->isChecked();
    
    return preset;
}

void PresetManagerDialog::onPresetSelected(QListWidgetItem* item)
{
    QString id = item->data(Qt::UserRole).toString();
    auto& manager = PresetManager::instance();
    const FormatPreset* preset = manager.getPreset(id);
    loadPresetDetails(preset);
}

void PresetManagerDialog::onNewClicked()
{
    FormatPreset preset;
    preset.id = QString("custom_%1").arg(QDateTime::currentSecsSinceEpoch());
    preset.name = "New Preset";
    preset.description = "";
    preset.category = "Custom";
    preset.isBuiltIn = false;
    preset.tracks = 80;
    preset.sides = 2;
    preset.sectorsPerTrack = 18;
    preset.sectorSize = 512;
    preset.encoding = FormatPreset::Encoding::MFM;
    preset.bitrate = 500000;
    preset.rpm = 300;
    preset.interleave = 1;
    preset.gap3Length = 27;
    preset.firstSectorID = 1;
    preset.autoDetectVariants = true;
    
    loadPresetDetails(&preset);
}

void PresetManagerDialog::onCopyClicked()
{
    FormatPreset preset = getCurrentPresetFromUI();
    preset.id = QString("custom_%1").arg(QDateTime::currentSecsSinceEpoch());
    preset.name += " (Copy)";
    preset.isBuiltIn = false;
    
    PresetManager::instance().addPreset(preset);
    populatePresetList();
}

void PresetManagerDialog::onDeleteClicked()
{
    if (m_currentPresetId.isEmpty()) return;
    
    auto reply = QMessageBox::question(this, "Delete Preset",
                                      "Are you sure you want to delete this preset?");
    if (reply == QMessageBox::Yes) {
        PresetManager::instance().deletePreset(m_currentPresetId);
        populatePresetList();
    }
}

void PresetManagerDialog::onImportClicked()
{
    QString filename = QFileDialog::getOpenFileName(this, "Import Preset",
                                                    "", "UFT Preset (*.uft-preset)");
    if (!filename.isEmpty()) {
        if (PresetManager::instance().importPreset(filename)) {
            populatePresetList();
            QMessageBox::information(this, "Success", "Preset imported successfully");
        } else {
            QMessageBox::warning(this, "Error", "Failed to import preset");
        }
    }
}

void PresetManagerDialog::onExportClicked()
{
    if (m_currentPresetId.isEmpty()) return;
    
    QString filename = QFileDialog::getSaveFileName(this, "Export Preset",
                                                    "", "UFT Preset (*.uft-preset)");
    if (!filename.isEmpty()) {
        if (PresetManager::instance().exportPreset(m_currentPresetId, filename)) {
            QMessageBox::information(this, "Success", "Preset exported successfully");
        } else {
            QMessageBox::warning(this, "Error", "Failed to export preset");
        }
    }
}

void PresetManagerDialog::onSaveClicked()
{
    FormatPreset preset = getCurrentPresetFromUI();
    
    QString error;
    if (!preset.validate(&error)) {
        QMessageBox::warning(this, "Validation Error", error);
        return;
    }
    
    if (preset.id.isEmpty()) {
        // New preset
        preset.id = QString("custom_%1").arg(QDateTime::currentSecsSinceEpoch());
        PresetManager::instance().addPreset(preset);
    } else {
        // Update existing
        PresetManager::instance().updatePreset(preset.id, preset);
    }
    
    populatePresetList();
    QMessageBox::information(this, "Success", "Preset saved successfully");
}

void PresetManagerDialog::onPresetChanged()
{
    // Update capacity label
    int tracks = m_tracksSpinBox->value();
    int sides = m_sidesSpinBox->value();
    int spt = m_sectorsPerTrackSpinBox->value();
    int sectorSize = m_sectorSizeCombo->currentText().toInt();
    
    qint64 capacity = static_cast<qint64>(tracks) * sides * spt * sectorSize;
    
    QString capacityStr;
    if (capacity < 1024) {
        capacityStr = QString("%1 Bytes").arg(capacity);
    } else if (capacity < 1024 * 1024) {
        capacityStr = QString("%1 KB").arg(capacity / 1024);
    } else {
        capacityStr = QString("%1 MB").arg(capacity / (1024.0 * 1024.0), 0, 'f', 2);
    }
    
    m_capacityLabel->setText("Capacity: " + capacityStr);
    m_modified = true;
}

const FormatPreset* PresetManagerDialog::getSelectedPreset() const
{
    if (m_currentPresetId.isEmpty()) return nullptr;
    return PresetManager::instance().getPreset(m_currentPresetId);
}

void PresetManagerDialog::selectPreset(const QString& id)
{
    m_currentPresetId = id;
    const FormatPreset* preset = PresetManager::instance().getPreset(id);
    loadPresetDetails(preset);
}

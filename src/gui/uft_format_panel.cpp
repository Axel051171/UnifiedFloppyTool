/**
 * @file uft_format_panel.cpp
 * @brief Format Settings Panel Implementation
 */

#include "uft_format_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QHeaderView>

UftFormatPanel::UftFormatPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    populateProfiles();
}

void UftFormatPanel::setupUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    /* Left side: Profile tree */
    createProfileTree();
    
    /* Right side: Settings */
    QWidget *settingsWidget = new QWidget();
    QVBoxLayout *settingsLayout = new QVBoxLayout(settingsWidget);
    
    createGeometryGroup();
    createEncodingGroup();
    createFilesystemGroup();
    createOutputGroup();
    
    settingsLayout->addWidget(m_geometryGroup);
    settingsLayout->addWidget(m_encodingGroup);
    settingsLayout->addWidget(m_filesystemGroup);
    settingsLayout->addWidget(m_outputGroup);
    settingsLayout->addStretch();
    
    /* Splitter */
    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(m_profileTree);
    splitter->addWidget(settingsWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    
    mainLayout->addWidget(splitter);
}

void UftFormatPanel::createProfileTree()
{
    m_profileTree = new QTreeWidget(this);
    m_profileTree->setHeaderLabel("Profiles");
    m_profileTree->setMinimumWidth(200);
    
    connect(m_profileTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        if (item->childCount() == 0) {
            setProfile(item->text(0));
            emit profileSelected(item->text(0));
        }
    });
}

void UftFormatPanel::populateProfiles()
{
    m_profileTree->clear();
    
    /* Group profiles by system */
    QMap<QString, QTreeWidgetItem*> systemItems;
    
    for (int i = 0; PRESET_PROFILES[i].name != nullptr; i++) {
        const FormatProfile &p = PRESET_PROFILES[i];
        QString system = p.system;
        
        if (!systemItems.contains(system)) {
            QTreeWidgetItem *sysItem = new QTreeWidgetItem(m_profileTree);
            sysItem->setText(0, system);
            sysItem->setExpanded(true);
            systemItems[system] = sysItem;
        }
        
        QTreeWidgetItem *item = new QTreeWidgetItem(systemItems[system]);
        item->setText(0, p.name);
        item->setData(0, Qt::UserRole, i);
    }
}

void UftFormatPanel::createGeometryGroup()
{
    m_geometryGroup = new QGroupBox("Disk Geometry", this);
    QGridLayout *layout = new QGridLayout(m_geometryGroup);
    
    /* Row 0 */
    layout->addWidget(new QLabel("Tracks:"), 0, 0);
    m_tracks = new QSpinBox();
    m_tracks->setRange(1, 255);
    m_tracks->setValue(80);
    layout->addWidget(m_tracks, 0, 1);
    
    layout->addWidget(new QLabel("Sides:"), 0, 2);
    m_sides = new QComboBox();
    m_sides->addItem("1 (Single)", 1);
    m_sides->addItem("2 (Double)", 2);
    m_sides->setCurrentIndex(1);
    layout->addWidget(m_sides, 0, 3);
    
    /* Row 1 */
    layout->addWidget(new QLabel("Sectors/Track:"), 1, 0);
    m_sectorsPerTrack = new QSpinBox();
    m_sectorsPerTrack->setRange(1, 100);
    m_sectorsPerTrack->setValue(18);
    layout->addWidget(m_sectorsPerTrack, 1, 1);
    
    layout->addWidget(new QLabel("Sector Size:"), 1, 2);
    m_sectorSize = new QComboBox();
    m_sectorSize->addItem("128 Bytes", 128);
    m_sectorSize->addItem("256 Bytes", 256);
    m_sectorSize->addItem("512 Bytes", 512);
    m_sectorSize->addItem("1024 Bytes", 1024);
    m_sectorSize->addItem("2048 Bytes", 2048);
    m_sectorSize->addItem("4096 Bytes", 4096);
    m_sectorSize->setCurrentIndex(2);
    layout->addWidget(m_sectorSize, 1, 3);
    
    /* Row 2 */
    layout->addWidget(new QLabel("Track Distance:"), 2, 0);
    m_trackDistance = new QComboBox();
    m_trackDistance->addItem("40 Tracks (96 TPI)", 40);
    m_trackDistance->addItem("80 Tracks (48 TPI)", 80);
    m_trackDistance->setCurrentIndex(1);
    layout->addWidget(m_trackDistance, 2, 1);
    
    layout->addWidget(new QLabel("RPM:"), 2, 2);
    m_rpm = new QDoubleSpinBox();
    m_rpm->setRange(100, 600);
    m_rpm->setValue(300);
    layout->addWidget(m_rpm, 2, 3);
    
    /* Row 3 */
    layout->addWidget(new QLabel("Total Sectors:"), 3, 0);
    m_totalSectors = new QLabel("2880");
    m_totalSectors->setStyleSheet("font-weight: bold;");
    layout->addWidget(m_totalSectors, 3, 1);
    
    /* Connect to update total */
    auto updateTotal = [this]() {
        int total = m_tracks->value() * m_sides->currentData().toInt() * m_sectorsPerTrack->value();
        int size = total * m_sectorSize->currentData().toInt();
        m_totalSectors->setText(QString("%1 (%2 KB)").arg(total).arg(size / 1024));
        emit paramsChanged();
    };
    
    connect(m_tracks, QOverload<int>::of(&QSpinBox::valueChanged), updateTotal);
    connect(m_sides, QOverload<int>::of(&QComboBox::currentIndexChanged), updateTotal);
    connect(m_sectorsPerTrack, QOverload<int>::of(&QSpinBox::valueChanged), updateTotal);
    connect(m_sectorSize, QOverload<int>::of(&QComboBox::currentIndexChanged), updateTotal);
}

void UftFormatPanel::createEncodingGroup()
{
    m_encodingGroup = new QGroupBox("Encoding", this);
    QGridLayout *layout = new QGridLayout(m_encodingGroup);
    
    /* Row 0 */
    layout->addWidget(new QLabel("Encoding:"), 0, 0);
    m_encoding = new QComboBox();
    m_encoding->addItem("MFM");
    m_encoding->addItem("FM");
    m_encoding->addItem("GCR (C64)");
    m_encoding->addItem("GCR (Apple)");
    m_encoding->addItem("RAW");
    layout->addWidget(m_encoding, 0, 1);
    
    layout->addWidget(new QLabel("Bitrate:"), 0, 2);
    m_bitrate = new QSpinBox();
    m_bitrate->setRange(100, 1000);
    m_bitrate->setValue(250);
    m_bitrate->setSuffix(" kbps");
    layout->addWidget(m_bitrate, 0, 3);
    
    /* Row 1 */
    layout->addWidget(new QLabel("Data Rate:"), 1, 0);
    m_dataRate = new QComboBox();
    m_dataRate->addItem("SD (125 kbps)", 125);
    m_dataRate->addItem("DD (250 kbps)", 250);
    m_dataRate->addItem("HD (500 kbps)", 500);
    m_dataRate->addItem("ED (1000 kbps)", 1000);
    m_dataRate->setCurrentIndex(1);
    layout->addWidget(m_dataRate, 1, 1);
    
    layout->addWidget(new QLabel("Gap 3:"), 1, 2);
    m_gap3 = new QSpinBox();
    m_gap3->setRange(0, 255);
    m_gap3->setValue(84);
    layout->addWidget(m_gap3, 1, 3);
    
    /* Row 2 */
    layout->addWidget(new QLabel("Pre-Gap:"), 2, 0);
    m_pregap = new QSpinBox();
    m_pregap->setRange(0, 255);
    m_pregap->setValue(0);
    layout->addWidget(m_pregap, 2, 1);
    
    layout->addWidget(new QLabel("Interleave:"), 2, 2);
    m_interleave = new QSpinBox();
    m_interleave->setRange(1, 20);
    m_interleave->setValue(1);
    layout->addWidget(m_interleave, 2, 3);
    
    /* Row 3 */
    layout->addWidget(new QLabel("Skew:"), 3, 0);
    m_skew = new QSpinBox();
    m_skew->setRange(0, 20);
    m_skew->setValue(0);
    layout->addWidget(m_skew, 3, 1);
    
    layout->addWidget(new QLabel("Sector ID Start:"), 3, 2);
    m_sectorIdStart = new QSpinBox();
    m_sectorIdStart->setRange(0, 255);
    m_sectorIdStart->setValue(1);
    layout->addWidget(m_sectorIdStart, 3, 3);
    
    connect(m_encoding, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftFormatPanel::paramsChanged);
}

void UftFormatPanel::createFilesystemGroup()
{
    m_filesystemGroup = new QGroupBox("Filesystem", this);
    QFormLayout *layout = new QFormLayout(m_filesystemGroup);
    
    m_filesystem = new QComboBox();
    m_filesystem->addItem("None");
    m_filesystem->addItem("FAT12");
    m_filesystem->addItem("FAT16");
    m_filesystem->addItem("OFS (Amiga)");
    m_filesystem->addItem("FFS (Amiga)");
    m_filesystem->addItem("CBM DOS");
    m_filesystem->addItem("DOS 3.3 (Apple)");
    m_filesystem->addItem("ProDOS (Apple)");
    m_filesystem->addItem("Atari DOS");
    m_filesystem->addItem("TR-DOS");
    m_filesystem->addItem("DFS (BBC)");
    layout->addRow("Filesystem:", m_filesystem);
    
    m_version = new QComboBox();
    m_version->addItem("Default");
    layout->addRow("Version:", m_version);
    
    m_bootable = new QCheckBox("Create bootable disk");
    layout->addRow(m_bootable);
    
    m_diskName = new QLineEdit();
    m_diskName->setMaxLength(30);
    m_diskName->setPlaceholderText("Disk name");
    layout->addRow("Disk Name:", m_diskName);
    
    /* Update version options based on filesystem */
    connect(m_filesystem, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_version->clear();
        switch (index) {
            case 3: /* OFS */
            case 4: /* FFS */
                m_version->addItem("DD (880K)");
                m_version->addItem("HD (1760K)");
                m_version->addItem("International");
                m_version->addItem("DirCache");
                break;
            case 5: /* CBM DOS */
                m_version->addItem("35 Track");
                m_version->addItem("40 Track");
                m_version->addItem("35 Track + Errors");
                m_version->addItem("40 Track + Errors");
                break;
            default:
                m_version->addItem("Default");
        }
        emit paramsChanged();
    });
}

void UftFormatPanel::createOutputGroup()
{
    m_outputGroup = new QGroupBox("Output Format", this);
    QFormLayout *layout = new QFormLayout(m_outputGroup);
    
    m_outputFormat = new QComboBox();
    m_outputFormat->addItem("D64 (Commodore 64)");
    m_outputFormat->addItem("D71 (Commodore 128)");
    m_outputFormat->addItem("D81 (Commodore 1581)");
    m_outputFormat->addItem("ADF (Amiga)");
    m_outputFormat->addItem("ADZ (Amiga gzip)");
    m_outputFormat->addItem("DMS (Amiga)");
    m_outputFormat->addItem("ATR (Atari)");
    m_outputFormat->addItem("ST (Atari ST)");
    m_outputFormat->addItem("MSA (Atari ST)");
    m_outputFormat->addItem("IMG (PC)");
    m_outputFormat->addItem("IMD (ImageDisk)");
    m_outputFormat->addItem("TD0 (Teledisk)");
    m_outputFormat->addItem("SCP (SuperCard Pro)");
    m_outputFormat->addItem("HFE (HxC)");
    m_outputFormat->addItem("G64 (GCR Nibble)");
    m_outputFormat->addItem("NIB (Nibble)");
    m_outputFormat->addItem("WOZ (Apple)");
    m_outputFormat->addItem("TRD (TR-DOS)");
    m_outputFormat->addItem("SSD (BBC)");
    m_outputFormat->addItem("DMK (TRS-80)");
    m_outputFormat->addItem("D88 (PC-98)");
    layout->addRow("Output Format:", m_outputFormat);
    
    m_extension = new QLineEdit();
    m_extension->setPlaceholderText("Auto");
    layout->addRow("Extension:", m_extension);
    
    m_useDefaults = new QCheckBox("Use format defaults");
    m_useDefaults->setChecked(true);
    layout->addRow(m_useDefaults);
    
    connect(m_outputFormat, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftFormatPanel::paramsChanged);
}

void UftFormatPanel::setProfile(const QString &profile)
{
    /* Find profile in presets */
    for (int i = 0; PRESET_PROFILES[i].name != nullptr; i++) {
        if (profile == PRESET_PROFILES[i].name) {
            const FormatProfile &p = PRESET_PROFILES[i];
            
            m_tracks->setValue(p.tracks);
            m_sides->setCurrentIndex(p.sides - 1);
            m_sectorsPerTrack->setValue(p.sectors);
            
            /* Find sector size in combo */
            for (int j = 0; j < m_sectorSize->count(); j++) {
                if (m_sectorSize->itemData(j).toInt() == p.sector_size) {
                    m_sectorSize->setCurrentIndex(j);
                    break;
                }
            }
            
            /* Encoding */
            QString enc = p.encoding;
            for (int j = 0; j < m_encoding->count(); j++) {
                if (m_encoding->itemText(j).startsWith(enc)) {
                    m_encoding->setCurrentIndex(j);
                    break;
                }
            }
            
            m_bitrate->setValue(p.bitrate);
            
            /* Filesystem */
            QString fs = p.filesystem;
            for (int j = 0; j < m_filesystem->count(); j++) {
                if (m_filesystem->itemText(j).contains(fs)) {
                    m_filesystem->setCurrentIndex(j);
                    break;
                }
            }
            
            break;
        }
    }
}

UftFormatPanel::FormatParams UftFormatPanel::getParams() const
{
    FormatParams params = {};
    
    params.tracks = m_tracks->value();
    params.sides = m_sides->currentData().toInt();
    params.sectors_per_track = m_sectorsPerTrack->value();
    params.sector_size = m_sectorSize->currentData().toInt();
    params.track_distance = m_trackDistance->currentData().toInt();
    params.rpm = m_rpm->value();
    
    params.encoding = m_encoding->currentText();
    params.bitrate = m_bitrate->value();
    params.data_rate = m_dataRate->currentText();
    params.gap3_length = m_gap3->value();
    params.pregap_length = m_pregap->value();
    params.interleave = m_interleave->value();
    params.skew = m_skew->value();
    params.sector_id_start = m_sectorIdStart->value();
    
    params.filesystem = m_filesystem->currentText();
    params.version = m_version->currentText();
    params.bootable = m_bootable->isChecked();
    
    params.output_format = m_outputFormat->currentText();
    params.extension = m_extension->text();
    
    return params;
}

void UftFormatPanel::setParams(const FormatParams &params)
{
    m_tracks->setValue(params.tracks);
    m_sectorsPerTrack->setValue(params.sectors_per_track);
    m_rpm->setValue(params.rpm);
    m_bitrate->setValue(params.bitrate);
    m_gap3->setValue(params.gap3_length);
    m_pregap->setValue(params.pregap_length);
    m_interleave->setValue(params.interleave);
    m_skew->setValue(params.skew);
    m_sectorIdStart->setValue(params.sector_id_start);
    m_bootable->setChecked(params.bootable);
    m_extension->setText(params.extension);
}

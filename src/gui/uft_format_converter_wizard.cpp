/**
 * @file uft_format_converter_wizard.cpp
 * @brief Format Converter Wizard Implementation (P2-GUI-008)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft_format_converter_wizard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QDateTime>
#include <QFileInfo>
#include <QStyle>
#include <QApplication>

/*===========================================================================
 * Format Data (matching uft_format_registry)
 *===========================================================================*/

struct FormatEntry {
    const char *id;
    const char *name;
    const char *description;
    const char *extension;
    const char *category;  /* sector, bitstream, flux */
    bool can_write;
};

static const FormatEntry FORMATS[] = {
    /* Amiga */
    { "ADF",  "Amiga Disk File",    "Standard Amiga sector image",     "adf",  "sector",    true  },
    { "ADZ",  "Compressed ADF",     "Gzip-compressed ADF",             "adz",  "sector",    true  },
    { "DMS",  "DiskMasher",         "Amiga DMS archive",               "dms",  "sector",    false },
    
    /* Commodore */
    { "D64",  "C64 1541 Disk",      "Standard C64 disk image",         "d64",  "sector",    true  },
    { "D71",  "C128 1571 Disk",     "Double-sided C64/C128 image",     "d71",  "sector",    true  },
    { "D81",  "C128 1581 Disk",     "3.5\" Commodore disk image",      "d81",  "sector",    true  },
    { "G64",  "C64 GCR Bitstream",  "GCR-level C64 image",             "g64",  "bitstream", true  },
    { "NIB",  "NIBTOOLS Format",    "Raw nibble data",                 "nib",  "bitstream", false },
    
    /* Atari */
    { "ATR",  "Atari 8-bit",        "Standard Atari disk image",       "atr",  "sector",    true  },
    { "ATX",  "Atari Extended",     "Protected Atari image",           "atx",  "bitstream", false },
    { "ST",   "Atari ST",           "Atari ST sector image",           "st",   "sector",    true  },
    { "STX",  "Atari ST Extended",  "Protected ST image",              "stx",  "bitstream", false },
    { "MSA",  "Magic Shadow",       "Compressed ST image",             "msa",  "sector",    true  },
    
    /* Apple */
    { "DSK",  "Apple II DOS",       "DOS 3.3 order image",             "dsk",  "sector",    true  },
    { "PO",   "Apple ProDOS",       "ProDOS order image",              "po",   "sector",    true  },
    { "2IMG", "2IMG Universal",     "Apple II universal format",       "2mg",  "sector",    true  },
    { "WOZ",  "WOZ Flux",           "Apple II flux image",             "woz",  "flux",      true  },
    { "A2R",  "Applesauce A2R",     "Multi-revolution flux",           "a2r",  "flux",      false },
    { "DC42", "DiskCopy 4.2",       "Macintosh disk image",            "dc42", "sector",    true  },
    
    /* PC/IBM */
    { "IMG",  "Raw Sector Image",   "Raw sector dump",                 "img",  "sector",    true  },
    { "IMA",  "DOS Floppy",         "DOS floppy image",                "ima",  "sector",    true  },
    { "IMD",  "ImageDisk",          "IMD with metadata",               "imd",  "sector",    true  },
    { "TD0",  "Teledisk",           "Teledisk archive",                "td0",  "sector",    false },
    { "D88",  "D88 Format",         "PC-98/X68000/FM-7 image",         "d88",  "sector",    true  },
    
    /* British */
    { "SSD",  "BBC Single-Sided",   "BBC Micro SS image",              "ssd",  "sector",    true  },
    { "DSD",  "BBC Double-Sided",   "BBC Micro DS image",              "dsd",  "sector",    true  },
    { "EDSK", "Extended DSK",       "Amstrad/Spectrum extended",       "dsk",  "bitstream", true  },
    { "TRD",  "TR-DOS",             "ZX Spectrum TR-DOS image",        "trd",  "sector",    true  },
    
    /* Flux */
    { "SCP",  "SuperCard Pro",      "Raw flux capture",                "scp",  "flux",      true  },
    { "KF",   "KryoFlux Stream",    "KryoFlux raw stream",             "raw",  "flux",      false },
    { "IPF",  "Interchangeable",    "CAPS/SPS format",                 "ipf",  "flux",      false },
    
    /* Bitstream */
    { "HFE",  "HxC Floppy",         "HxC emulator format",             "hfe",  "bitstream", true  },
    { "MFM",  "MFM Bitstream",      "Raw MFM bitstream",               "mfm",  "bitstream", true  },
    { "DMK",  "DMK Format",         "TRS-80 DMK format",               "dmk",  "bitstream", true  },
    
    /* UFT */
    { "UIR",  "UFT Intermediate",   "UFT universal format",            "uir",  "flux",      true  },
    
    { nullptr, nullptr, nullptr, nullptr, nullptr, false }
};

/*===========================================================================
 * UftSourcePage
 *===========================================================================*/

UftSourcePage::UftSourcePage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Source Image"));
    setSubTitle(tr("Select the disk image you want to convert."));
    setupUi();
}

void UftSourcePage::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    /* File selection */
    QHBoxLayout *fileLayout = new QHBoxLayout();
    m_sourcePath = new QLineEdit();
    m_sourcePath->setPlaceholderText(tr("Select source image file..."));
    m_browseButton = new QPushButton(tr("Browse..."));
    m_analyzeButton = new QPushButton(tr("Analyze"));
    m_analyzeButton->setEnabled(false);
    
    fileLayout->addWidget(m_sourcePath);
    fileLayout->addWidget(m_browseButton);
    fileLayout->addWidget(m_analyzeButton);
    layout->addLayout(fileLayout);
    
    /* Detection results */
    m_detectionGroup = new QGroupBox(tr("Detected Information"));
    QGridLayout *detGrid = new QGridLayout(m_detectionGroup);
    
    detGrid->addWidget(new QLabel(tr("Format:")), 0, 0);
    m_formatLabel = new QLabel(tr("-"));
    m_formatLabel->setStyleSheet("font-weight: bold;");
    detGrid->addWidget(m_formatLabel, 0, 1);
    
    detGrid->addWidget(new QLabel(tr("Size:")), 0, 2);
    m_sizeLabel = new QLabel(tr("-"));
    detGrid->addWidget(m_sizeLabel, 0, 3);
    
    detGrid->addWidget(new QLabel(tr("Tracks:")), 1, 0);
    m_tracksLabel = new QLabel(tr("-"));
    detGrid->addWidget(m_tracksLabel, 1, 1);
    
    detGrid->addWidget(new QLabel(tr("Encoding:")), 1, 2);
    m_encodingLabel = new QLabel(tr("-"));
    detGrid->addWidget(m_encodingLabel, 1, 3);
    
    detGrid->addWidget(new QLabel(tr("Quality:")), 2, 0);
    m_qualityLabel = new QLabel(tr("-"));
    detGrid->addWidget(m_qualityLabel, 2, 1);
    
    detGrid->addWidget(new QLabel(tr("Protection:")), 2, 2);
    m_protectionLabel = new QLabel(tr("-"));
    detGrid->addWidget(m_protectionLabel, 2, 3);
    
    layout->addWidget(m_detectionGroup);
    
    /* Content preview */
    QGroupBox *previewGroup = new QGroupBox(tr("Content Preview"));
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    m_contentTree = new QTreeWidget();
    m_contentTree->setHeaderLabels({tr("Name"), tr("Type"), tr("Size")});
    m_contentTree->setMaximumHeight(150);
    previewLayout->addWidget(m_contentTree);
    layout->addWidget(previewGroup);
    
    /* Connections */
    connect(m_browseButton, &QPushButton::clicked, this, &UftSourcePage::browseSource);
    connect(m_analyzeButton, &QPushButton::clicked, this, &UftSourcePage::analyzeSource);
    connect(m_sourcePath, &QLineEdit::textChanged, [this](const QString &text) {
        m_analyzeButton->setEnabled(!text.isEmpty());
        emit completeChanged();
    });
    
    /* Register field */
    registerField("sourcePath*", m_sourcePath);
}

void UftSourcePage::browseSource()
{
    QString filter = tr("All Disk Images (*.adf *.adz *.dms *.d64 *.d71 *.d81 *.g64 *.nib "
                        "*.atr *.atx *.st *.stx *.msa *.dsk *.do *.po *.2mg *.woz *.a2r "
                        "*.img *.ima *.imd *.td0 *.d88 *.ssd *.dsd *.trd *.scp *.raw *.ipf "
                        "*.hfe *.dmk *.uir);;"
                        "Amiga (*.adf *.adz *.dms);;"
                        "Commodore (*.d64 *.d71 *.d81 *.g64 *.nib);;"
                        "Atari (*.atr *.atx *.st *.stx *.msa);;"
                        "Apple (*.dsk *.do *.po *.2mg *.woz *.a2r);;"
                        "PC (*.img *.ima *.imd *.td0 *.d88);;"
                        "Flux (*.scp *.raw *.ipf *.woz *.a2r);;"
                        "All Files (*)");
    
    QString path = QFileDialog::getOpenFileName(this, tr("Select Source Image"),
                                                 QString(), filter);
    if (!path.isEmpty()) {
        m_sourcePath->setText(path);
        analyzeSource();
    }
}

void UftSourcePage::analyzeSource()
{
    QString path = m_sourcePath->text();
    if (path.isEmpty()) return;
    
    QFileInfo fi(path);
    if (!fi.exists()) {
        QMessageBox::warning(this, tr("File Not Found"),
                             tr("The selected file does not exist."));
        return;
    }
    
    /* Detect format by extension (simplified) */
    QString ext = fi.suffix().toLower();
    m_detectedFormat = ext.toUpper();
    
    /* Update labels */
    m_formatLabel->setText(m_detectedFormat);
    m_sizeLabel->setText(QString("%1 KB").arg(fi.size() / 1024));
    
    /* Estimate tracks based on size */
    qint64 size = fi.size();
    int tracks = 0;
    QString encoding = "-";
    
    if (ext == "adf" && size == 901120) {
        tracks = 160;
        encoding = "Amiga MFM";
    } else if (ext == "d64") {
        tracks = (size > 175000) ? 40 : 35;
        encoding = "C64 GCR";
    } else if (ext == "atr") {
        tracks = (int)(size / 128 / 18);
        encoding = "Atari FM/MFM";
    } else if (ext == "dsk" || ext == "do" || ext == "po") {
        tracks = 35;
        encoding = "Apple GCR";
    } else if (size == 737280 || size == 1474560) {
        tracks = (size == 737280) ? 160 : 160;
        encoding = "IBM MFM";
    }
    
    m_tracksLabel->setText(tracks > 0 ? QString::number(tracks) : "-");
    m_encodingLabel->setText(encoding);
    m_qualityLabel->setText(tr("Unknown"));
    m_protectionLabel->setText(tr("None detected"));
    
    emit completeChanged();
}

bool UftSourcePage::isComplete() const
{
    return !m_sourcePath->text().isEmpty() && 
           QFileInfo::exists(m_sourcePath->text());
}

bool UftSourcePage::validatePage()
{
    if (!QFileInfo::exists(m_sourcePath->text())) {
        QMessageBox::warning(const_cast<UftSourcePage*>(this), 
                             tr("File Not Found"),
                             tr("Please select a valid source file."));
        return false;
    }
    return true;
}

QString UftSourcePage::getSourcePath() const
{
    return m_sourcePath->text();
}

QString UftSourcePage::getDetectedFormat() const
{
    return m_detectedFormat;
}

/*===========================================================================
 * UftTargetPage
 *===========================================================================*/

UftTargetPage::UftTargetPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Target Format"));
    setSubTitle(tr("Select the output format and destination."));
    setupUi();
}

void UftTargetPage::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    /* Category filter */
    QHBoxLayout *catLayout = new QHBoxLayout();
    m_categoryGroup = new QButtonGroup(this);
    m_catAll = new QRadioButton(tr("All"));
    m_catSector = new QRadioButton(tr("Sector"));
    m_catBitstream = new QRadioButton(tr("Bitstream"));
    m_catFlux = new QRadioButton(tr("Flux"));
    m_catAll->setChecked(true);
    
    m_categoryGroup->addButton(m_catAll, 0);
    m_categoryGroup->addButton(m_catSector, 1);
    m_categoryGroup->addButton(m_catBitstream, 2);
    m_categoryGroup->addButton(m_catFlux, 3);
    
    catLayout->addWidget(new QLabel(tr("Category:")));
    catLayout->addWidget(m_catAll);
    catLayout->addWidget(m_catSector);
    catLayout->addWidget(m_catBitstream);
    catLayout->addWidget(m_catFlux);
    catLayout->addStretch();
    layout->addLayout(catLayout);
    
    /* Format list with filter */
    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel(tr("Filter:")));
    m_formatFilter = new QLineEdit();
    m_formatFilter->setPlaceholderText(tr("Type to filter formats..."));
    filterLayout->addWidget(m_formatFilter);
    layout->addLayout(filterLayout);
    
    QHBoxLayout *listLayout = new QHBoxLayout();
    
    m_formatList = new QListWidget();
    m_formatList->setMaximumWidth(200);
    listLayout->addWidget(m_formatList);
    
    /* Format description */
    QVBoxLayout *descLayout = new QVBoxLayout();
    m_formatDescription = new QLabel();
    m_formatDescription->setWordWrap(true);
    m_formatDescription->setMinimumHeight(60);
    m_formatDescription->setStyleSheet("background: palette(base); padding: 8px; "
                                        "border: 1px solid palette(mid);");
    descLayout->addWidget(m_formatDescription);
    
    m_compatibilityLabel = new QLabel();
    m_compatibilityLabel->setWordWrap(true);
    descLayout->addWidget(m_compatibilityLabel);
    descLayout->addStretch();
    
    listLayout->addLayout(descLayout);
    layout->addLayout(listLayout);
    
    /* Output path */
    QGroupBox *outputGroup = new QGroupBox(tr("Output"));
    QHBoxLayout *outputLayout = new QHBoxLayout(outputGroup);
    m_targetPath = new QLineEdit();
    m_targetPath->setPlaceholderText(tr("Select output file..."));
    m_browseButton = new QPushButton(tr("Browse..."));
    m_autoExtension = new QCheckBox(tr("Auto extension"));
    m_autoExtension->setChecked(true);
    
    outputLayout->addWidget(m_targetPath);
    outputLayout->addWidget(m_browseButton);
    outputLayout->addWidget(m_autoExtension);
    layout->addWidget(outputGroup);
    
    /* Populate and connect */
    populateFormats();
    
    connect(m_browseButton, &QPushButton::clicked, this, &UftTargetPage::browseTarget);
    connect(m_formatFilter, &QLineEdit::textChanged, this, &UftTargetPage::filterFormats);
    connect(m_formatList, &QListWidget::currentRowChanged, [this](int row) {
        if (row >= 0) {
            QString id = m_formatList->item(row)->data(Qt::UserRole).toString();
            for (const FormatEntry *f = FORMATS; f->id; f++) {
                if (id == f->id) {
                    m_formatDescription->setText(QString("<b>%1</b><br>%2")
                        .arg(f->name).arg(f->description));
                    m_compatibilityLabel->setText(
                        f->can_write ? tr("✓ Writing supported") : 
                                       tr("✗ Read-only format"));
                    if (m_autoExtension->isChecked()) {
                        updateExtension();
                    }
                    break;
                }
            }
        }
        emit completeChanged();
    });
    
    connect(m_categoryGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            [this](int) { filterFormats(m_formatFilter->text()); });
    
    registerField("targetPath*", m_targetPath);
}

void UftTargetPage::populateFormats()
{
    m_formatList->clear();
    
    for (const FormatEntry *f = FORMATS; f->id; f++) {
        if (!f->can_write) continue;  /* Only show writable formats */
        
        QListWidgetItem *item = new QListWidgetItem(
            QString("%1 - %2").arg(f->id).arg(f->name));
        item->setData(Qt::UserRole, QString(f->id));
        item->setData(Qt::UserRole + 1, QString(f->category));
        m_formatList->addItem(item);
    }
    
    if (m_formatList->count() > 0) {
        m_formatList->setCurrentRow(0);
    }
}

void UftTargetPage::filterFormats(const QString &text)
{
    QString filter = text.toLower();
    int catId = m_categoryGroup->checkedId();
    QString catFilter;
    if (catId == 1) catFilter = "sector";
    else if (catId == 2) catFilter = "bitstream";
    else if (catId == 3) catFilter = "flux";
    
    for (int i = 0; i < m_formatList->count(); i++) {
        QListWidgetItem *item = m_formatList->item(i);
        QString itemCat = item->data(Qt::UserRole + 1).toString();
        bool catMatch = catFilter.isEmpty() || itemCat == catFilter;
        bool textMatch = filter.isEmpty() || 
                         item->text().toLower().contains(filter);
        item->setHidden(!catMatch || !textMatch);
    }
}

void UftTargetPage::browseTarget()
{
    QString currentFormat = getTargetFormat();
    QString ext = "img";
    
    for (const FormatEntry *f = FORMATS; f->id; f++) {
        if (currentFormat == f->id) {
            ext = f->extension;
            break;
        }
    }
    
    QString filter = QString("%1 Files (*.%2);;All Files (*)").arg(currentFormat).arg(ext);
    QString path = QFileDialog::getSaveFileName(this, tr("Save As"), QString(), filter);
    
    if (!path.isEmpty()) {
        m_targetPath->setText(path);
    }
}

void UftTargetPage::updateExtension()
{
    QString path = m_targetPath->text();
    if (path.isEmpty()) return;
    
    QString format = getTargetFormat();
    QString newExt;
    
    for (const FormatEntry *f = FORMATS; f->id; f++) {
        if (format == f->id) {
            newExt = f->extension;
            break;
        }
    }
    
    if (!newExt.isEmpty()) {
        QFileInfo fi(path);
        QString newPath = fi.path() + "/" + fi.completeBaseName() + "." + newExt;
        m_targetPath->setText(newPath);
    }
}

bool UftTargetPage::isComplete() const
{
    return m_formatList->currentRow() >= 0 && !m_targetPath->text().isEmpty();
}

QString UftTargetPage::getTargetFormat() const
{
    if (m_formatList->currentRow() >= 0) {
        return m_formatList->currentItem()->data(Qt::UserRole).toString();
    }
    return QString();
}

QString UftTargetPage::getTargetPath() const
{
    return m_targetPath->text();
}

/*===========================================================================
 * UftOptionsPage
 *===========================================================================*/

UftOptionsPage::UftOptionsPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Conversion Options"));
    setSubTitle(tr("Configure how the conversion should be performed."));
    setupUi();
}

void UftOptionsPage::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    /* Presets */
    QHBoxLayout *presetLayout = new QHBoxLayout();
    presetLayout->addWidget(new QLabel(tr("Preset:")));
    m_presetCombo = new QComboBox();
    m_presetCombo->addItems({
        tr("Default"),
        tr("Preservation (Maximum)"),
        tr("Compatibility (Simple)"),
        tr("Recovery (Best Effort)")
    });
    presetLayout->addWidget(m_presetCombo);
    presetLayout->addStretch();
    layout->addLayout(presetLayout);
    
    /* Two-column layout */
    QHBoxLayout *colLayout = new QHBoxLayout();
    QVBoxLayout *leftCol = new QVBoxLayout();
    QVBoxLayout *rightCol = new QVBoxLayout();
    
    createPreservationGroup();
    createErrorGroup();
    createOutputGroup();
    createVerificationGroup();
    
    leftCol->addWidget(m_preserveGroup);
    leftCol->addWidget(m_errorGroup);
    rightCol->addWidget(m_outputGroup);
    rightCol->addWidget(m_verifyGroup);
    leftCol->addStretch();
    rightCol->addStretch();
    
    colLayout->addLayout(leftCol);
    colLayout->addLayout(rightCol);
    layout->addLayout(colLayout);
    
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftOptionsPage::loadPreset);
}

void UftOptionsPage::createPreservationGroup()
{
    m_preserveGroup = new QGroupBox(tr("Data Preservation"));
    QVBoxLayout *layout = new QVBoxLayout(m_preserveGroup);
    
    m_preserveWeak = new QCheckBox(tr("Preserve weak/fuzzy bits"));
    m_preserveTiming = new QCheckBox(tr("Preserve timing information"));
    m_preserveProtection = new QCheckBox(tr("Preserve copy protection"));
    m_multiRevolution = new QCheckBox(tr("Multi-revolution mode"));
    
    QHBoxLayout *revLayout = new QHBoxLayout();
    revLayout->addWidget(new QLabel(tr("   Preferred revolution:")));
    m_preferredRev = new QSpinBox();
    m_preferredRev->setRange(0, 10);
    m_preferredRev->setValue(0);
    m_preferredRev->setSpecialValueText(tr("Best"));
    revLayout->addWidget(m_preferredRev);
    revLayout->addStretch();
    
    layout->addWidget(m_preserveWeak);
    layout->addWidget(m_preserveTiming);
    layout->addWidget(m_preserveProtection);
    layout->addWidget(m_multiRevolution);
    layout->addLayout(revLayout);
}

void UftOptionsPage::createErrorGroup()
{
    m_errorGroup = new QGroupBox(tr("Error Handling"));
    QVBoxLayout *layout = new QVBoxLayout(m_errorGroup);
    
    m_retryBad = new QCheckBox(tr("Retry bad sectors"));
    QHBoxLayout *retryLayout = new QHBoxLayout();
    retryLayout->addWidget(new QLabel(tr("   Max retries:")));
    m_maxRetries = new QSpinBox();
    m_maxRetries->setRange(0, 20);
    m_maxRetries->setValue(3);
    retryLayout->addWidget(m_maxRetries);
    retryLayout->addStretch();
    
    m_ignoreCrc = new QCheckBox(tr("Include sectors with CRC errors"));
    m_fillBad = new QCheckBox(tr("Fill unreadable sectors with:"));
    
    QHBoxLayout *fillLayout = new QHBoxLayout();
    fillLayout->addWidget(new QLabel(tr("   Fill byte:")));
    m_fillByte = new QSpinBox();
    m_fillByte->setRange(0, 255);
    m_fillByte->setValue(0);
    m_fillByte->setDisplayIntegerBase(16);
    m_fillByte->setPrefix("0x");
    fillLayout->addWidget(m_fillByte);
    fillLayout->addStretch();
    
    layout->addWidget(m_retryBad);
    layout->addLayout(retryLayout);
    layout->addWidget(m_ignoreCrc);
    layout->addWidget(m_fillBad);
    layout->addLayout(fillLayout);
}

void UftOptionsPage::createOutputGroup()
{
    m_outputGroup = new QGroupBox(tr("Output Options"));
    QVBoxLayout *layout = new QVBoxLayout(m_outputGroup);
    
    m_compressOutput = new QCheckBox(tr("Compress output"));
    QHBoxLayout *compLayout = new QHBoxLayout();
    compLayout->addWidget(new QLabel(tr("   Level:")));
    m_compressionLevel = new QSpinBox();
    m_compressionLevel->setRange(1, 9);
    m_compressionLevel->setValue(6);
    compLayout->addWidget(m_compressionLevel);
    compLayout->addStretch();
    
    m_addMetadata = new QCheckBox(tr("Add metadata"));
    QHBoxLayout *metaLayout = new QHBoxLayout();
    metaLayout->addWidget(new QLabel(tr("   Comment:")));
    m_metadataComment = new QLineEdit();
    m_metadataComment->setPlaceholderText(tr("Optional comment..."));
    metaLayout->addWidget(m_metadataComment);
    
    layout->addWidget(m_compressOutput);
    layout->addLayout(compLayout);
    layout->addWidget(m_addMetadata);
    layout->addLayout(metaLayout);
}

void UftOptionsPage::createVerificationGroup()
{
    m_verifyGroup = new QGroupBox(tr("Verification"));
    QVBoxLayout *layout = new QVBoxLayout(m_verifyGroup);
    
    m_verifyAfter = new QCheckBox(tr("Verify after conversion"));
    m_verifyAfter->setChecked(true);
    m_generateReport = new QCheckBox(tr("Generate conversion report"));
    m_generateReport->setChecked(true);
    
    layout->addWidget(m_verifyAfter);
    layout->addWidget(m_generateReport);
}

void UftOptionsPage::loadPreset(int index)
{
    switch (index) {
    case 0: /* Default */
        m_preserveWeak->setChecked(true);
        m_preserveTiming->setChecked(false);
        m_preserveProtection->setChecked(false);
        m_retryBad->setChecked(true);
        m_maxRetries->setValue(3);
        m_ignoreCrc->setChecked(false);
        m_verifyAfter->setChecked(true);
        break;
        
    case 1: /* Preservation */
        m_preserveWeak->setChecked(true);
        m_preserveTiming->setChecked(true);
        m_preserveProtection->setChecked(true);
        m_multiRevolution->setChecked(true);
        m_retryBad->setChecked(true);
        m_maxRetries->setValue(10);
        m_ignoreCrc->setChecked(true);
        m_verifyAfter->setChecked(true);
        m_generateReport->setChecked(true);
        break;
        
    case 2: /* Compatibility */
        m_preserveWeak->setChecked(false);
        m_preserveTiming->setChecked(false);
        m_preserveProtection->setChecked(false);
        m_retryBad->setChecked(false);
        m_ignoreCrc->setChecked(false);
        m_fillBad->setChecked(true);
        m_compressOutput->setChecked(false);
        break;
        
    case 3: /* Recovery */
        m_retryBad->setChecked(true);
        m_maxRetries->setValue(20);
        m_ignoreCrc->setChecked(true);
        m_fillBad->setChecked(true);
        m_verifyAfter->setChecked(true);
        m_generateReport->setChecked(true);
        break;
    }
}

UftConversionOptions UftOptionsPage::getOptions() const
{
    UftConversionOptions opts;
    opts.preserve_weak_bits = m_preserveWeak->isChecked();
    opts.preserve_timing = m_preserveTiming->isChecked();
    opts.preserve_protection = m_preserveProtection->isChecked();
    opts.multi_revolution = m_multiRevolution->isChecked();
    opts.preferred_revolution = m_preferredRev->value();
    opts.retry_bad_sectors = m_retryBad->isChecked();
    opts.max_retries = m_maxRetries->value();
    opts.ignore_crc_errors = m_ignoreCrc->isChecked();
    opts.fill_bad_sectors = m_fillBad->isChecked();
    opts.fill_byte = (uint8_t)m_fillByte->value();
    opts.compress_output = m_compressOutput->isChecked();
    opts.compression_level = m_compressionLevel->value();
    opts.add_metadata = m_addMetadata->isChecked();
    opts.metadata_comment = m_metadataComment->text();
    opts.verify_after_convert = m_verifyAfter->isChecked();
    opts.generate_report = m_generateReport->isChecked();
    return opts;
}

/*===========================================================================
 * UftProgressPage
 *===========================================================================*/

UftProgressPage::UftProgressPage(QWidget *parent)
    : QWizardPage(parent)
    , m_worker(nullptr)
    , m_workerThread(nullptr)
    , m_isComplete(false)
    , m_success(false)
{
    setTitle(tr("Converting"));
    setSubTitle(tr("Please wait while the disk image is being converted."));
    setupUi();
}

void UftProgressPage::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    /* Progress bars */
    QFormLayout *progressLayout = new QFormLayout();
    m_overallProgress = new QProgressBar();
    m_trackProgress = new QProgressBar();
    progressLayout->addRow(tr("Overall:"), m_overallProgress);
    progressLayout->addRow(tr("Track:"), m_trackProgress);
    layout->addLayout(progressLayout);
    
    /* Status labels */
    m_statusLabel = new QLabel(tr("Initializing..."));
    m_trackLabel = new QLabel();
    m_timeLabel = new QLabel();
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_trackLabel);
    layout->addWidget(m_timeLabel);
    
    /* Log view */
    QGroupBox *logGroup = new QGroupBox(tr("Log"));
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    m_logView = new QTextEdit();
    m_logView->setReadOnly(true);
    m_logView->setMaximumHeight(120);
    m_logView->setStyleSheet("font-family: monospace;");
    logLayout->addWidget(m_logView);
    layout->addWidget(logGroup);
    
    /* Results (hidden initially) */
    m_resultsGroup = new QGroupBox(tr("Results"));
    m_resultsGroup->hide();
    QGridLayout *resLayout = new QGridLayout(m_resultsGroup);
    
    m_resultIcon = new QLabel();
    m_resultSummary = new QLabel();
    m_resultSummary->setStyleSheet("font-weight: bold; font-size: 14px;");
    resLayout->addWidget(m_resultIcon, 0, 0);
    resLayout->addWidget(m_resultSummary, 0, 1, 1, 3);
    
    m_tracksConverted = new QLabel();
    m_sectorsGood = new QLabel();
    m_sectorsBad = new QLabel();
    m_warnings = new QLabel();
    resLayout->addWidget(new QLabel(tr("Tracks:")), 1, 0);
    resLayout->addWidget(m_tracksConverted, 1, 1);
    resLayout->addWidget(new QLabel(tr("Good sectors:")), 1, 2);
    resLayout->addWidget(m_sectorsGood, 1, 3);
    resLayout->addWidget(new QLabel(tr("Bad sectors:")), 2, 0);
    resLayout->addWidget(m_sectorsBad, 2, 1);
    resLayout->addWidget(new QLabel(tr("Warnings:")), 2, 2);
    resLayout->addWidget(m_warnings, 2, 3);
    
    layout->addWidget(m_resultsGroup);
    
    /* Buttons */
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_cancelButton = new QPushButton(tr("Cancel"));
    m_openOutputButton = new QPushButton(tr("Open Output Folder"));
    m_openOutputButton->hide();
    btnLayout->addStretch();
    btnLayout->addWidget(m_cancelButton);
    btnLayout->addWidget(m_openOutputButton);
    layout->addLayout(btnLayout);
    
    connect(m_cancelButton, &QPushButton::clicked, this, &UftProgressPage::cancelConversion);
}

void UftProgressPage::initializePage()
{
    m_isComplete = false;
    m_success = false;
    m_overallProgress->setValue(0);
    m_trackProgress->setValue(0);
    m_logView->clear();
    m_resultsGroup->hide();
    m_openOutputButton->hide();
    m_cancelButton->show();
    
    /* Start conversion after a short delay */
    QTimer::singleShot(100, this, &UftProgressPage::startConversion);
}

void UftProgressPage::cleanupPage()
{
    cancelConversion();
}

bool UftProgressPage::isComplete() const
{
    return m_isComplete;
}

void UftProgressPage::startConversion()
{
    m_logView->append(tr("[%1] Starting conversion...")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    
    /* Simulate conversion (in real implementation, use worker thread) */
    int totalTracks = 80;
    for (int t = 0; t < totalTracks; t++) {
        onTrackProgress(t, totalTracks, QString("Track %1").arg(t));
        onProgress((t * 100) / totalTracks, QString("Converting track %1/%2").arg(t).arg(totalTracks));
        
        /* Process events to keep UI responsive */
        QApplication::processEvents();
        
        /* Simulate some work */
        QThread::msleep(20);
    }
    
    onComplete(true, tr("Conversion completed successfully"));
}

void UftProgressPage::cancelConversion()
{
    if (m_worker) {
        m_worker->cancel();
    }
    m_logView->append(tr("[%1] Conversion cancelled by user")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
}

void UftProgressPage::onProgress(int percent, const QString &status)
{
    m_overallProgress->setValue(percent);
    m_statusLabel->setText(status);
}

void UftProgressPage::onTrackProgress(int track, int total, const QString &info)
{
    m_trackProgress->setValue((track * 100) / total);
    m_trackLabel->setText(info);
}

void UftProgressPage::onWarning(const QString &message)
{
    m_logView->append(QString("<span style='color: orange;'>⚠ %1</span>").arg(message));
}

void UftProgressPage::onError(const QString &message)
{
    m_logView->append(QString("<span style='color: red;'>✗ %1</span>").arg(message));
}

void UftProgressPage::onComplete(bool success, const QString &summary)
{
    m_isComplete = true;
    m_success = success;
    
    m_cancelButton->hide();
    m_openOutputButton->show();
    m_resultsGroup->show();
    
    if (success) {
        m_resultIcon->setText("✓");
        m_resultIcon->setStyleSheet("color: green; font-size: 24px;");
        m_resultSummary->setText(tr("Conversion Successful"));
        m_resultSummary->setStyleSheet("color: green; font-weight: bold;");
    } else {
        m_resultIcon->setText("✗");
        m_resultIcon->setStyleSheet("color: red; font-size: 24px;");
        m_resultSummary->setText(tr("Conversion Failed"));
        m_resultSummary->setStyleSheet("color: red; font-weight: bold;");
    }
    
    /* Update stats (simulated) */
    m_tracksConverted->setText("80");
    m_sectorsGood->setText("1440");
    m_sectorsBad->setText("0");
    m_warnings->setText("0");
    
    m_logView->append(tr("[%1] %2")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(summary));
    
    emit completeChanged();
    emit conversionFinished(success);
}

/*===========================================================================
 * UftFormatConverterWizard
 *===========================================================================*/

UftFormatConverterWizard::UftFormatConverterWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle(tr("Format Converter"));
    setWizardStyle(QWizard::ModernStyle);
    setMinimumSize(700, 550);
    
    setupPages();
    applyStyle();
}

UftFormatConverterWizard::~UftFormatConverterWizard()
{
}

void UftFormatConverterWizard::setupPages()
{
    m_sourcePage = new UftSourcePage();
    m_targetPage = new UftTargetPage();
    m_optionsPage = new UftOptionsPage();
    m_progressPage = new UftProgressPage();
    
    setPage(Page_Source, m_sourcePage);
    setPage(Page_Target, m_targetPage);
    setPage(Page_Options, m_optionsPage);
    setPage(Page_Progress, m_progressPage);
    
    connect(this, &QWizard::currentIdChanged, this, &UftFormatConverterWizard::onPageChanged);
    connect(m_progressPage, &UftProgressPage::conversionFinished,
            this, &UftFormatConverterWizard::onConversionFinished);
}

void UftFormatConverterWizard::applyStyle()
{
    setStyleSheet(R"(
        QWizard {
            background: palette(window);
        }
        QWizardPage {
            background: palette(window);
        }
        QGroupBox {
            font-weight: bold;
            border: 1px solid palette(mid);
            border-radius: 4px;
            margin-top: 8px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
    )");
}

void UftFormatConverterWizard::setSourceFile(const QString &path)
{
    m_sourcePage->findChild<QLineEdit*>()->setText(path);
}

UftConversionOptions UftFormatConverterWizard::getOptions() const
{
    UftConversionOptions opts = m_optionsPage->getOptions();
    opts.source_path = m_sourcePage->getSourcePath();
    opts.source_format = m_sourcePage->getDetectedFormat();
    opts.target_path = m_targetPage->getTargetPath();
    opts.target_format = m_targetPage->getTargetFormat();
    return opts;
}

void UftFormatConverterWizard::onPageChanged(int id)
{
    if (id == Page_Target) {
        /* Auto-generate output path based on source */
        QString source = m_sourcePage->getSourcePath();
        if (!source.isEmpty()) {
            QFileInfo fi(source);
            QString basePath = fi.path() + "/" + fi.completeBaseName();
            m_targetPage->findChild<QLineEdit*>()->setText(basePath + ".converted");
        }
    }
}

void UftFormatConverterWizard::onConversionFinished(bool success)
{
    if (success) {
        emit conversionComplete(m_targetPage->getTargetPath());
    }
}

/*===========================================================================
 * UftConversionWorker
 *===========================================================================*/

UftConversionWorker::UftConversionWorker(QObject *parent)
    : QObject(parent)
    , m_cancelled(false)
{
}

void UftConversionWorker::setOptions(const UftConversionOptions &opts)
{
    m_options = opts;
}

void UftConversionWorker::process()
{
    /* TODO: Implement actual conversion logic */
    emit complete(true, "Conversion complete");
}

void UftConversionWorker::cancel()
{
    m_cancelled = true;
}

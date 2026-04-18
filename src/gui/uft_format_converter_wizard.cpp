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

extern "C" {
#include "uft/uft_format_convert.h"
#include "uft/uft_types.h"
#include "uft/core/uft_roundtrip.h"
}

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
    uft_format_id_t rt_id; /* Round-trip matrix ID (0 = unmapped) */
};

static const FormatEntry FORMATS[] = {
    /* Amiga */
    { "ADF",  "Amiga Disk File",    "Standard Amiga sector image",     "adf",  "sector",    true,  UFT_FORMAT_ADF  },
    { "ADZ",  "Compressed ADF",     "Gzip-compressed ADF",             "adz",  "sector",    true,  0               },
    { "DMS",  "DiskMasher",         "Amiga DMS archive",               "dms",  "sector",    false, 0               },

    /* Commodore */
    { "D64",  "C64 1541 Disk",      "Standard C64 disk image",         "d64",  "sector",    true,  UFT_FORMAT_D64  },
    { "D71",  "C128 1571 Disk",     "Double-sided C64/C128 image",     "d71",  "sector",    true,  UFT_FORMAT_D71  },
    { "D81",  "C128 1581 Disk",     "3.5\" Commodore disk image",      "d81",  "sector",    true,  UFT_FORMAT_D81  },
    { "G64",  "C64 GCR Bitstream",  "GCR-level C64 image",             "g64",  "bitstream", true,  UFT_FORMAT_G64  },
    { "NIB",  "NIBTOOLS Format",    "Raw nibble data",                 "nib",  "bitstream", false, UFT_FORMAT_NIB  },

    /* Atari */
    { "ATR",  "Atari 8-bit",        "Standard Atari disk image",       "atr",  "sector",    true,  UFT_FORMAT_ATR  },
    { "ATX",  "Atari Extended",     "Protected Atari image",           "atx",  "bitstream", false, 0               },
    { "ST",   "Atari ST",           "Atari ST sector image",           "st",   "sector",    true,  UFT_FORMAT_ST   },
    { "STX",  "Atari ST Extended",  "Protected ST image",              "stx",  "bitstream", false, UFT_FORMAT_STX  },
    { "MSA",  "Magic Shadow",       "Compressed ST image",             "msa",  "sector",    true,  UFT_FORMAT_MSA  },

    /* Apple */
    { "DSK",  "Apple II DOS",       "DOS 3.3 order image",             "dsk",  "sector",    true,  UFT_FORMAT_DSK  },
    { "PO",   "Apple ProDOS",       "ProDOS order image",              "po",   "sector",    true,  UFT_FORMAT_PO   },
    { "2IMG", "2IMG Universal",     "Apple II universal format",       "2mg",  "sector",    true,  0               },
    { "WOZ",  "WOZ Flux",           "Apple II flux image",             "woz",  "flux",      true,  UFT_FORMAT_WOZ  },
    { "A2R",  "Applesauce A2R",     "Multi-revolution flux",           "a2r",  "flux",      false, UFT_FORMAT_A2R  },
    { "DC42", "DiskCopy 4.2",       "Macintosh disk image",            "dc42", "sector",    true,  UFT_FORMAT_DC42 },

    /* PC/IBM */
    { "IMG",  "Raw Sector Image",   "Raw sector dump",                 "img",  "sector",    true,  UFT_FORMAT_IMG  },
    { "IMA",  "DOS Floppy",         "DOS floppy image",                "ima",  "sector",    true,  UFT_FORMAT_IMG  },
    { "IMD",  "ImageDisk",          "IMD with metadata",               "imd",  "sector",    true,  UFT_FORMAT_IMD  },
    { "TD0",  "Teledisk",           "Teledisk archive",                "td0",  "sector",    false, UFT_FORMAT_TD0  },
    { "D88",  "D88 Format",         "PC-98/X68000/FM-7 image",         "d88",  "sector",    true,  UFT_FORMAT_D88  },

    /* British */
    { "SSD",  "BBC Single-Sided",   "BBC Micro SS image",              "ssd",  "sector",    true,  UFT_FORMAT_SSD  },
    { "DSD",  "BBC Double-Sided",   "BBC Micro DS image",              "dsd",  "sector",    true,  UFT_FORMAT_DSD  },
    { "EDSK", "Extended DSK",       "Amstrad/Spectrum extended",       "dsk",  "bitstream", true,  UFT_FORMAT_EDSK },
    { "TRD",  "TR-DOS",             "ZX Spectrum TR-DOS image",        "trd",  "sector",    true,  UFT_FORMAT_TRD  },

    /* Flux */
    { "SCP",  "SuperCard Pro",      "Raw flux capture",                "scp",  "flux",      true,  UFT_FORMAT_SCP  },
    { "KF",   "KryoFlux Stream",    "KryoFlux raw stream",             "raw",  "flux",      false, UFT_FORMAT_KRYOFLUX },
    { "IPF",  "Interchangeable",    "CAPS/SPS format",                 "ipf",  "flux",      false, UFT_FORMAT_IPF  },

    /* Bitstream */
    { "HFE",  "HxC Floppy",         "HxC emulator format",             "hfe",  "bitstream", true,  UFT_FORMAT_HFE  },
    { "MFM",  "MFM Bitstream",      "Raw MFM bitstream",               "mfm",  "bitstream", true,  0               },
    { "DMK",  "DMK Format",         "TRS-80 DMK format",               "dmk",  "bitstream", true,  UFT_FORMAT_DMK  },

    /* UFT */
    { "UIR",  "UFT Intermediate",   "UFT universal format",            "uir",  "flux",      true,  0               },

    { nullptr, nullptr, nullptr, nullptr, nullptr, false, 0 }
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

    /* Conversion warning (same-format / lossy) */
    m_conversionWarning = new QLabel();
    m_conversionWarning->setWordWrap(true);
    m_conversionWarning->setVisible(false);
    descLayout->addWidget(m_conversionWarning);
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
                        f->can_write ? tr("\xe2\x9c\x93 Writing supported") :
                                       tr("\xe2\x9c\x97 Read-only format"));
                    if (m_autoExtension->isChecked()) {
                        updateExtension();
                    }
                    break;
                }
            }
        }
        updateConversionWarning();
        emit completeChanged();
    });
    
    // Qt6 removed the int overload of QButtonGroup::buttonClicked; the
    // integer-id equivalent is idClicked(int), added in Qt 5.15 for this
    // migration.
    connect(m_categoryGroup, &QButtonGroup::idClicked,
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
    if (m_formatList->currentRow() < 0 || m_targetPath->text().isEmpty())
        return false;

    /* Block if source and target format are identical */
    QString targetId = m_formatList->currentItem()->data(Qt::UserRole).toString();
    if (!m_sourceFormat.isEmpty()
        && targetId.compare(m_sourceFormat, Qt::CaseInsensitive) == 0)
        return false;

    return true;
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

void UftTargetPage::setSourceFormat(const QString &format)
{
    m_sourceFormat = format.toUpper();
    updateConversionWarning();
    emit completeChanged();
}

void UftTargetPage::updateConversionWarning()
{
    if (m_formatList->currentRow() < 0) {
        m_conversionWarning->setVisible(false);
        return;
    }

    QString targetId = m_formatList->currentItem()->data(Qt::UserRole).toString();
    QString targetCat = m_formatList->currentItem()->data(Qt::UserRole + 1).toString();

    /* Same format selected as source */
    if (!m_sourceFormat.isEmpty()
        && targetId.compare(m_sourceFormat, Qt::CaseInsensitive) == 0) {
        m_conversionWarning->setStyleSheet(
            "color: #d32f2f; font-weight: bold; padding: 4px;");
        m_conversionWarning->setText(
            tr("Source and target format are the same. "
               "Please select a different target format."));
        m_conversionWarning->setVisible(true);
        return;
    }

    /* Prinzip 5 §5.2: consult the authoritative round-trip matrix. */
    QString sourceCat;
    uft_format_id_t srcRt = 0;
    uft_format_id_t dstRt = 0;
    for (const FormatEntry *f = FORMATS; f->id; f++) {
        if (m_sourceFormat.compare(f->id, Qt::CaseInsensitive) == 0) {
            sourceCat = f->category;
            srcRt = f->rt_id;
        }
        if (targetId.compare(f->id, Qt::CaseInsensitive) == 0) {
            dstRt = f->rt_id;
        }
    }

    if (srcRt != 0 && dstRt != 0) {
        uft_roundtrip_status_t rt = uft_roundtrip_status(srcRt, dstRt);
        const char *rtNote = uft_roundtrip_note(srcRt, dstRt);

        auto show = [this](const QString &style, const QString &text) {
            m_conversionWarning->setStyleSheet(style);
            m_conversionWarning->setText(text);
            m_conversionWarning->setVisible(true);
        };

        switch (rt) {
        case UFT_RT_LOSSLESS:
            show(QStringLiteral("color: #2e7d32; font-weight: bold; padding: 4px;"),
                 tr("LOSSLESS round-trip: %1 → %2 is byte-identical%3")
                     .arg(m_sourceFormat, targetId,
                          *rtNote ? QString(" (%1).").arg(QString::fromUtf8(rtNote))
                                  : QStringLiteral(".")));
            return;
        case UFT_RT_LOSSY_DOCUMENTED:
            show(QStringLiteral("color: #e67e00; font-weight: bold; padding: 4px;"),
                 tr("LOSSY-DOCUMENTED: %1 → %2 loses %3. "
                    "A .loss.json sidecar will be written next to the target.")
                     .arg(m_sourceFormat, targetId,
                          *rtNote ? QString::fromUtf8(rtNote)
                                  : tr("information not yet enumerated")));
            return;
        case UFT_RT_IMPOSSIBLE:
            show(QStringLiteral("color: #b00020; font-weight: bold; padding: 4px;"),
                 tr("IMPOSSIBLE: %1 → %2 cannot be represented — %3")
                     .arg(m_sourceFormat, targetId,
                          *rtNote ? QString::fromUtf8(rtNote)
                                  : tr("the target format cannot carry the source's data.")));
            return;
        case UFT_RT_UNTESTED:
        default:
            show(QStringLiteral("color: #757575; font-style: italic; padding: 4px;"),
                 tr("UNTESTED: %1 → %2 is not in the round-trip matrix. "
                    "Conversion is not guaranteed to be reversible or complete. "
                    "See docs/DESIGN_PRINCIPLES.md §5.")
                     .arg(m_sourceFormat, targetId));
            return;
        }
    }

    /* Fallback heuristic when one of the formats isn't yet mapped to the
     * round-trip matrix — keeps the old category-based warning so the
     * wizard never looks blank. */
    QStringList lostData;
    if ((sourceCat == "flux" || sourceCat == "bitstream") && targetCat == "sector") {
        lostData << tr("timing information") << tr("weak/fuzzy bits");
        if (sourceCat == "flux")
            lostData << tr("multi-revolution data");
    } else if (sourceCat == "flux" && targetCat == "bitstream") {
        lostData << tr("multi-revolution data") << tr("raw flux timing");
    }

    if (!lostData.isEmpty()) {
        m_conversionWarning->setStyleSheet(
            "color: #e67e00; padding: 4px;");
        m_conversionWarning->setText(
            tr("Lossy conversion (heuristic): %1 will be lost.")
            .arg(lostData.join(", ")));
        m_conversionWarning->setVisible(true);
    } else {
        m_conversionWarning->setVisible(false);
    }
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
    m_trackProgress->setValue((total > 0) ? (track * 100) / total : 0);
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
        /* Pass detected source format to target page for conflict detection */
        QString detectedFormat = m_sourcePage->getDetectedFormat();
        m_targetPage->setSourceFormat(detectedFormat);

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
    emit progress(0, tr("Starting conversion..."));

    if (m_cancelled) {
        emit complete(false, tr("Cancelled"));
        return;
    }

    /* Validate paths */
    if (m_options.source_path.isEmpty() || m_options.target_path.isEmpty()) {
        emit error(tr("Source or target path is empty."));
        emit complete(false, tr("Conversion failed: missing paths"));
        return;
    }

    QFileInfo srcInfo(m_options.source_path);
    if (!srcInfo.exists()) {
        emit error(tr("Source file not found: %1").arg(m_options.source_path));
        emit complete(false, tr("Source file not found"));
        return;
    }

    emit progress(10, tr("Analyzing source: %1").arg(srcInfo.fileName()));

    /* Determine target format from extension or explicit format string */
    QString dstExt = m_options.target_format.toLower();
    if (dstExt.isEmpty()) {
        dstExt = QFileInfo(m_options.target_path).suffix().toLower();
    }

    uft_format_t dstFormat = UFT_FORMAT_UNKNOWN;
    if (dstExt == "adf") dstFormat = UFT_FORMAT_ADF;
    else if (dstExt == "d64") dstFormat = UFT_FORMAT_D64;
    else if (dstExt == "d71") dstFormat = UFT_FORMAT_D71;
    else if (dstExt == "d81") dstFormat = UFT_FORMAT_D81;
    else if (dstExt == "scp") dstFormat = UFT_FORMAT_SCP;
    else if (dstExt == "hfe") dstFormat = UFT_FORMAT_HFE;
    else if (dstExt == "img" || dstExt == "ima") dstFormat = UFT_FORMAT_IMG;
    else if (dstExt == "g64") dstFormat = UFT_FORMAT_G64;
    else if (dstExt == "st") dstFormat = UFT_FORMAT_ST;
    else if (dstExt == "atr") dstFormat = UFT_FORMAT_ATR;
    else if (dstExt == "dmk") dstFormat = UFT_FORMAT_DMK;
    else if (dstExt == "imd") dstFormat = UFT_FORMAT_IMD;
    else if (dstExt == "woz") dstFormat = UFT_FORMAT_WOZ;

    if (dstFormat == UFT_FORMAT_UNKNOWN) {
        emit error(tr("Unknown target format: %1").arg(dstExt));
        emit complete(false, tr("Unsupported target format"));
        return;
    }

    /* Set up conversion options.
     *
     * uft_convert_file() takes uft_convert_options_t (from uft_types.h), not
     * the _ext_t variant. The _t struct has no dedicated cancel pointer —
     * cancellation is signalled by the progress callback returning false.
     * We bridge the cancel flag through the progress context. */
    uft_convert_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.verify_after       = m_options.verify_after_convert;
    opts.preserve_errors    = false;
    opts.preserve_weak_bits = m_options.preserve_weak_bits;
    opts.decode_retries     = m_options.max_retries > 0 ? m_options.max_retries : 3;
    opts.use_multiple_revs  = m_options.multi_revolution;

    /* Progress callback bridging Qt + cancel flag. */
    struct ProgressCtx {
        UftConversionWorker *self;
        volatile bool       *cancel;
    };
    static auto progressCb = [](int cyl, int head, int percent,
                                 const char *stage, void *user) -> bool {
        (void)cyl;
        (void)head;
        auto *ctx = static_cast<ProgressCtx*>(user);
        QMetaObject::invokeMethod(ctx->self,
            [ctx, percent, msg = QString::fromUtf8(stage)]() {
                emit ctx->self->progress(10 + (percent * 80 / 100), msg);
            },
            Qt::QueuedConnection);
        /* Return false to request cancellation. */
        return !(ctx->cancel && *ctx->cancel);
    };
    ProgressCtx ctx{this, &m_cancelled};
    opts.progress      = progressCb;
    opts.progress_user = &ctx;

    uft_convert_result_t result;
    memset(&result, 0, sizeof(result));

    QByteArray srcBytes = m_options.source_path.toUtf8();
    QByteArray dstBytes = m_options.target_path.toUtf8();

    emit progress(20, tr("Converting..."));

    uft_error_t err = uft_convert_file(srcBytes.constData(),
                                        dstBytes.constData(),
                                        dstFormat, &opts, &result);

    if (m_cancelled) {
        emit complete(false, tr("Conversion cancelled by user"));
        return;
    }

    emit progress(95, tr("Finalizing..."));

    /* Report warnings */
    for (int i = 0; i < result.warning_count && i < 8; i++) {
        emit warning(QString::fromUtf8(result.warnings[i]));
    }

    if (err == UFT_OK && result.success) {
        QString summary = tr("Conversion complete: %1 tracks (%2 sectors), %3 bytes written")
            .arg(result.tracks_converted)
            .arg(result.sectors_converted)
            .arg(result.bytes_written);

        if (result.tracks_failed > 0) {
            summary += tr("\n%1 tracks failed").arg(result.tracks_failed);
        }

        emit progress(100, tr("Done"));
        emit complete(true, summary);
    } else {
        QString errMsg = tr("Conversion failed");
        if (result.tracks_failed > 0) {
            errMsg += tr(": %1 of %2 tracks failed")
                .arg(result.tracks_failed)
                .arg(result.tracks_converted + result.tracks_failed);
        }
        emit error(errMsg);
        emit complete(false, errMsg);
    }
}

void UftConversionWorker::cancel()
{
    m_cancelled = true;
}

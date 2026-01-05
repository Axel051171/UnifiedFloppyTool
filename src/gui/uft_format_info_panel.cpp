/**
 * @file uft_format_info_panel.cpp
 * @brief Format Info Panel Implementation (BONUS-GUI-003)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft_format_info_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QApplication>
#include <QDesktopServices>

/*===========================================================================
 * Format Documentation Database
 *===========================================================================*/

static const UftFormatDoc FORMAT_DOCS[] = {
    /* === Amiga === */
    {
        "ADF", "Amiga Disk File", "Sector", "Amiga",
        "The ADF format is a sector-by-sector dump of Amiga floppy disks. It contains "
        "880KB of raw sector data (1760 sectors × 512 bytes) without any metadata.",
        
        "Developed in the early 1990s as a simple way to archive Amiga floppy disks "
        "for use with emulators like UAE. The format became the de facto standard for "
        "Amiga disk preservation.",
        
        "<h3>Structure</h3>"
        "<ul>"
        "<li>Size: 901,120 bytes (880 KB)</li>"
        "<li>Tracks: 80 (0-79)</li>"
        "<li>Heads: 2 (upper/lower)</li>"
        "<li>Sectors per track: 11</li>"
        "<li>Bytes per sector: 512</li>"
        "</ul>"
        "<h3>Track Layout</h3>"
        "<p>Tracks are stored sequentially: Track 0 Head 0, Track 0 Head 1, "
        "Track 1 Head 0, Track 1 Head 1, etc.</p>",
        
        "Sector Size|512 bytes\n"
        "Total Sectors|1760\n"
        "Total Size|901,120 bytes\n"
        "Encoding|Amiga MFM\n"
        "Data Rate|250 kbit/s\n"
        "RPM|300",
        
        "https://en.wikipedia.org/wiki/Amiga_Disk_File\n"
        "http://lclevy.free.fr/adflib/adf_info.html"
    },
    
    /* === Commodore === */
    {
        "D64", "C64 1541 Disk Image", "Sector", "Commodore 64",
        "The D64 format stores the contents of a Commodore 1541 floppy disk. "
        "It contains 35 tracks with variable sectors per track, totaling 683 blocks.",
        
        "Created for C64 emulators, D64 became the standard format for preserving "
        "C64 software. The format can optionally include error bytes for each sector.",
        
        "<h3>Structure</h3>"
        "<ul>"
        "<li>Tracks: 35 (1-35)</li>"
        "<li>Sectors per track: 21/19/18/17 (varies)</li>"
        "<li>Bytes per sector: 256</li>"
        "<li>Total blocks: 683</li>"
        "</ul>"
        "<h3>Track Layout</h3>"
        "<table border='1'>"
        "<tr><th>Tracks</th><th>Sectors</th></tr>"
        "<tr><td>1-17</td><td>21</td></tr>"
        "<tr><td>18-24</td><td>19</td></tr>"
        "<tr><td>25-30</td><td>18</td></tr>"
        "<tr><td>31-35</td><td>17</td></tr>"
        "</table>"
        "<h3>BAM (Block Availability Map)</h3>"
        "<p>Located at Track 18, Sector 0. Contains disk name, ID, and free block bitmap.</p>",
        
        "Standard Size|174,848 bytes\n"
        "With Error Info|175,531 bytes\n"
        "Encoding|Commodore GCR\n"
        "Data Rate|~31.25 kbit/s\n"
        "RPM|300",
        
        "https://vice-emu.sourceforge.io/vice_17.html\n"
        "http://unusedino.de/ec64/technical/formats/d64.html"
    },
    
    {
        "G64", "C64 GCR Bitstream", "Bitstream", "Commodore 64",
        "G64 stores the raw GCR-encoded bitstream data of a 1541 disk, preserving "
        "timing variations and copy protection schemes that D64 cannot represent.",
        
        "Developed for the VICE emulator to enable accurate preservation of copy-protected "
        "software. Includes speed zone information and raw bitstream data.",
        
        "<h3>Header</h3>"
        "<ul>"
        "<li>Signature: 'GCR-1541' (8 bytes)</li>"
        "<li>Version: 0 (1 byte)</li>"
        "<li>Track count (1 byte)</li>"
        "<li>Max track size (2 bytes)</li>"
        "</ul>"
        "<h3>Track Data</h3>"
        "<p>Each track stored as:</p>"
        "<ul>"
        "<li>Track offset table (4 bytes × tracks)</li>"
        "<li>Speed zone table (4 bytes × tracks)</li>"
        "<li>Track data (variable, up to 7928 bytes)</li>"
        "</ul>",
        
        "Tracks|35-42\n"
        "Max Track Size|7928 bytes\n"
        "Encoding|Commodore GCR\n"
        "Speed Zones|4 (0-3)",
        
        "https://vice-emu.sourceforge.io/vice_17.html#SEC349\n"
        "http://www.unusedino.de/ec64/technical/formats/g64.html"
    },
    
    /* === Atari === */
    {
        "ATR", "Atari 8-bit Disk Image", "Sector", "Atari 8-bit",
        "ATR is the standard disk image format for Atari 8-bit computers. "
        "It includes a 16-byte header with disk geometry information.",
        
        "Created by Nick Kennedy for the SIO2PC interface. Supports various disk "
        "densities including single (90KB), enhanced (130KB), and double (180KB).",
        
        "<h3>Header (16 bytes)</h3>"
        "<ul>"
        "<li>Magic: 0x96 0x02 (2 bytes)</li>"
        "<li>Paragraphs (size/16): 2 bytes</li>"
        "<li>Sector size: 2 bytes</li>"
        "<li>High byte of paragraphs: 1 byte</li>"
        "<li>Reserved: 9 bytes</li>"
        "</ul>"
        "<h3>Sector Sizes</h3>"
        "<ul>"
        "<li>Single Density: 128 bytes</li>"
        "<li>Double Density: 256 bytes</li>"
        "</ul>",
        
        "Header Size|16 bytes\n"
        "SD Sectors|720 × 128 bytes\n"
        "DD Sectors|720 × 256 bytes\n"
        "Encoding|Atari FM/MFM",
        
        "https://atari8.co.uk/atr/\n"
        "https://www.atarimax.com/jindroush.atari.org/afmtatr.html"
    },
    
    /* === Apple === */
    {
        "DSK", "Apple II DOS Order", "Sector", "Apple II",
        "DSK/DO format stores Apple II disk images with sectors in DOS 3.3 order. "
        "The physical-to-logical sector mapping differs from ProDOS order.",
        
        "One of the earliest Apple II disk image formats. The 'DOS order' refers to "
        "the sector interleaving scheme used by DOS 3.3.",
        
        "<h3>Structure</h3>"
        "<ul>"
        "<li>Tracks: 35</li>"
        "<li>Sectors per track: 16</li>"
        "<li>Bytes per sector: 256</li>"
        "<li>Total size: 143,360 bytes</li>"
        "</ul>"
        "<h3>Sector Order</h3>"
        "<p>DOS 3.3 uses sector skewing for performance: "
        "0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15</p>",
        
        "Size|143,360 bytes\n"
        "Tracks|35\n"
        "Sectors|16/track\n"
        "Encoding|Apple GCR 6+2",
        
        "https://wiki.osdev.org/Apple_II#Disk_format\n"
        "https://retrocomputing.stackexchange.com/questions/166/what-are-the-differences-between-apple-ii-disk-image-formats"
    },
    
    {
        "WOZ", "WOZ Flux Image", "Flux", "Apple II",
        "WOZ is a flux-level disk image format designed to preserve copy-protected "
        "Apple II software. It stores raw bitstream data with timing information.",
        
        "Developed by John K. Morris for the Applesauce hardware. Named after Steve Wozniak. "
        "Supports weak bits, cross-track synchronization, and multiple revolutions.",
        
        "<h3>WOZ 2.0 Structure</h3>"
        "<ul>"
        "<li>Header: 'WOZ2' + CRC32 (12 bytes)</li>"
        "<li>INFO chunk: Disk metadata</li>"
        "<li>TMAP chunk: Track mapping</li>"
        "<li>TRKS chunk: Track data</li>"
        "<li>Optional: META, WRIT chunks</li>"
        "</ul>"
        "<h3>Features</h3>"
        "<ul>"
        "<li>Bit-accurate flux representation</li>"
        "<li>Weak/random bit support</li>"
        "<li>Quarter-track support</li>"
        "<li>Write splice markers</li>"
        "</ul>",
        
        "Version|1.0, 2.0\n"
        "Chunk Types|INFO, TMAP, TRKS, META\n"
        "Bit Timing|125ns resolution\n"
        "Max Track Size|Variable",
        
        "https://applesaucefdc.com/woz/reference2/\n"
        "https://github.com/a2-4am/wozardry"
    },
    
    /* === Flux === */
    {
        "SCP", "SuperCard Pro", "Flux", "Universal",
        "SCP format stores raw flux transitions captured by the SuperCard Pro hardware. "
        "Each transition is stored as a 16-bit timestamp relative to the previous transition.",
        
        "Developed by Jim Drew for the SuperCard Pro hardware. Supports multiple "
        "revolutions per track for enhanced data recovery and copy protection preservation.",
        
        "<h3>Header</h3>"
        "<ul>"
        "<li>Signature: 'SCP' (3 bytes)</li>"
        "<li>Version (1 byte)</li>"
        "<li>Disk type (1 byte)</li>"
        "<li>Revolution count (1 byte)</li>"
        "<li>Track range (2 bytes)</li>"
        "<li>Flags (1 byte)</li>"
        "<li>Cell width (1 byte)</li>"
        "<li>Heads (1 byte)</li>"
        "<li>Checksum (4 bytes)</li>"
        "</ul>"
        "<h3>Track Data</h3>"
        "<p>Each revolution stored as a series of 16-bit flux timing values. "
        "Timing resolution is 25ns (40MHz sample rate).</p>",
        
        "Timing Resolution|25ns\n"
        "Sample Rate|40 MHz\n"
        "Max Revolutions|5\n"
        "Max Tracks|168",
        
        "https://www.cbmstuff.com/downloads/scp/scp_image_specs.txt"
    },
    
    {
        "HFE", "HxC Floppy Emulator", "Bitstream", "Universal",
        "HFE is the native format for the HxC Floppy Emulator hardware. "
        "It stores MFM-encoded bitstream data organized by track and side.",
        
        "Created by Jean-François Del Nero for the HxC Floppy Emulator project. "
        "The format is designed for efficient streaming to floppy drive emulator hardware.",
        
        "<h3>Header (512 bytes)</h3>"
        "<ul>"
        "<li>Signature: 'HXCPICFE' (8 bytes)</li>"
        "<li>Format revision (1 byte)</li>"
        "<li>Track count (1 byte)</li>"
        "<li>Head count (1 byte)</li>"
        "<li>Track encoding (1 byte)</li>"
        "<li>Bit rate (2 bytes)</li>"
        "<li>RPM (2 bytes)</li>"
        "<li>Interface mode (1 byte)</li>"
        "</ul>"
        "<h3>Track Format</h3>"
        "<p>Tracks stored in 512-byte blocks with interleaved side 0/1 data.</p>",
        
        "Header Size|512 bytes\n"
        "Block Size|512 bytes\n"
        "Encoding|MFM/FM/GCR\n"
        "Max Tracks|256",
        
        "https://hxc2001.com/download/floppy_drive_emulator/HxCFloppyEmulator_file_format.pdf"
    },
    
    /* === End marker === */
    { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }
};

/*===========================================================================
 * Encoding Documentation Database
 *===========================================================================*/

static const UftEncodingDoc ENCODING_DOCS[] = {
    { "MFM", "Modified Frequency Modulation", 
      1000, 250, "0x4489 (A1 with clock)", "IBM PC, Amiga, Atari ST" },
    { "FM", "Frequency Modulation", 
      2000, 125, "0xF57E (FE with clock)", "IBM 3740, early 8-inch" },
    { "GCR-C64", "Group Coded Recording (Commodore)", 
      3200, 31, "0x52 (5 SYNC bytes)", "Commodore 64, 1541" },
    { "GCR-Apple", "Group Coded Recording (Apple)", 
      4000, 31, "D5 AA 96", "Apple II, Macintosh" },
    { "Amiga-MFM", "Amiga Modified MFM", 
      1000, 250, "0x4489 4489", "Amiga" },
    { nullptr, nullptr, 0, 0, nullptr, nullptr }
};

/*===========================================================================
 * UftFormatInfoPanel
 *===========================================================================*/

UftFormatInfoPanel::UftFormatInfoPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    populateFormats();
    populateEncodings();
}

void UftFormatInfoPanel::setupUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    /* Left panel: Format list */
    QWidget *leftWidget = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    m_filterEdit = new QLineEdit();
    m_filterEdit->setPlaceholderText(tr("Filter formats..."));
    leftLayout->addWidget(m_filterEdit);
    
    m_categoryCombo = new QComboBox();
    m_categoryCombo->addItems({tr("All"), tr("Sector"), tr("Bitstream"), tr("Flux")});
    leftLayout->addWidget(m_categoryCombo);
    
    m_formatTree = new QTreeWidget();
    m_formatTree->setHeaderLabels({tr("Format"), tr("Platform")});
    m_formatTree->setRootIsDecorated(true);
    m_formatTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    leftLayout->addWidget(m_formatTree);
    
    leftWidget->setMaximumWidth(250);
    mainLayout->addWidget(leftWidget);
    
    /* Right panel: Info tabs */
    m_infoTabs = new QTabWidget();
    
    m_overviewBrowser = new QTextBrowser();
    m_overviewBrowser->setOpenExternalLinks(true);
    m_infoTabs->addTab(m_overviewBrowser, tr("Overview"));
    
    m_structureBrowser = new QTextBrowser();
    m_infoTabs->addTab(m_structureBrowser, tr("Structure"));
    
    m_specsTable = new QTableWidget();
    m_specsTable->setColumnCount(2);
    m_specsTable->setHorizontalHeaderLabels({tr("Property"), tr("Value")});
    m_specsTable->horizontalHeader()->setStretchLastSection(true);
    m_specsTable->verticalHeader()->hide();
    m_infoTabs->addTab(m_specsTable, tr("Specifications"));
    
    m_referencesBrowser = new QTextBrowser();
    m_referencesBrowser->setOpenExternalLinks(true);
    m_infoTabs->addTab(m_referencesBrowser, tr("References"));
    
    /* Encoding tab */
    QWidget *encodingWidget = new QWidget();
    QHBoxLayout *encLayout = new QHBoxLayout(encodingWidget);
    m_encodingTree = new QTreeWidget();
    m_encodingTree->setHeaderLabels({tr("Encoding"), tr("Data Rate")});
    m_encodingTree->setMaximumWidth(200);
    encLayout->addWidget(m_encodingTree);
    m_encodingBrowser = new QTextBrowser();
    encLayout->addWidget(m_encodingBrowser);
    m_infoTabs->addTab(encodingWidget, tr("Encodings"));
    
    mainLayout->addWidget(m_infoTabs, 1);
    
    /* Connections */
    connect(m_formatTree, &QTreeWidget::itemClicked, 
            this, &UftFormatInfoPanel::onFormatSelected);
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &UftFormatInfoPanel::onFilterChanged);
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftFormatInfoPanel::onCategoryChanged);
    connect(m_overviewBrowser, &QTextBrowser::anchorClicked,
            this, &UftFormatInfoPanel::onLinkClicked);
    connect(m_encodingTree, &QTreeWidget::itemClicked, [this](QTreeWidgetItem *item, int) {
        showEncoding(item->text(0));
    });
}

void UftFormatInfoPanel::populateFormats()
{
    m_formatTree->clear();
    
    /* Create category items */
    QMap<QString, QTreeWidgetItem*> categories;
    categories["Sector"] = new QTreeWidgetItem(m_formatTree, {tr("Sector Images")});
    categories["Bitstream"] = new QTreeWidgetItem(m_formatTree, {tr("Bitstream Images")});
    categories["Flux"] = new QTreeWidgetItem(m_formatTree, {tr("Flux Captures")});
    
    for (auto *cat : categories) {
        cat->setExpanded(true);
        QFont font = cat->font(0);
        font.setBold(true);
        cat->setFont(0, font);
    }
    
    /* Add formats */
    for (const UftFormatDoc *doc = FORMAT_DOCS; doc->id; doc++) {
        QTreeWidgetItem *parent = categories.value(doc->category);
        if (!parent) parent = m_formatTree->invisibleRootItem();
        
        QTreeWidgetItem *item = new QTreeWidgetItem(parent);
        item->setText(0, doc->id);
        item->setText(1, doc->platform);
        item->setData(0, Qt::UserRole, QString(doc->id));
    }
}

void UftFormatInfoPanel::populateEncodings()
{
    m_encodingTree->clear();
    
    for (const UftEncodingDoc *doc = ENCODING_DOCS; doc->name; doc++) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_encodingTree);
        item->setText(0, doc->name);
        item->setText(1, QString("%1 kbit/s").arg(doc->data_rate_kbps));
    }
}

void UftFormatInfoPanel::onFormatSelected(QTreeWidgetItem *item, int)
{
    QString id = item->data(0, Qt::UserRole).toString();
    if (id.isEmpty()) return;
    
    showFormat(id);
    emit formatSelected(id);
}

void UftFormatInfoPanel::showFormat(const QString &formatId)
{
    for (const UftFormatDoc *doc = FORMAT_DOCS; doc->id; doc++) {
        if (QString(doc->id) == formatId) {
            showFormatDetails(doc);
            return;
        }
    }
}

void UftFormatInfoPanel::showFormatDetails(const UftFormatDoc *doc)
{
    /* Overview */
    QString html = QString("<h1>%1 - %2</h1>").arg(doc->id).arg(doc->name);
    html += QString("<p><b>Platform:</b> %1</p>").arg(doc->platform);
    html += QString("<p><b>Category:</b> %1</p>").arg(doc->category);
    html += QString("<hr><h2>Description</h2><p>%1</p>").arg(doc->description);
    html += QString("<h2>History</h2><p>%1</p>").arg(doc->history);
    m_overviewBrowser->setHtml(html);
    
    /* Structure */
    m_structureBrowser->setHtml(QString(doc->structure));
    
    /* Specifications */
    m_specsTable->setRowCount(0);
    QString specs = doc->specifications;
    QStringList lines = specs.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QStringList parts = line.split('|');
        if (parts.size() == 2) {
            int row = m_specsTable->rowCount();
            m_specsTable->insertRow(row);
            m_specsTable->setItem(row, 0, new QTableWidgetItem(parts[0].trimmed()));
            m_specsTable->setItem(row, 1, new QTableWidgetItem(parts[1].trimmed()));
        }
    }
    
    /* References */
    QString refs = doc->references;
    QString refHtml = "<h2>References</h2><ul>";
    QStringList urls = refs.split('\n', Qt::SkipEmptyParts);
    for (const QString &url : urls) {
        refHtml += QString("<li><a href='%1'>%1</a></li>").arg(url.trimmed());
    }
    refHtml += "</ul>";
    m_referencesBrowser->setHtml(refHtml);
    
    m_infoTabs->setCurrentIndex(0);
}

void UftFormatInfoPanel::showEncoding(const QString &encodingName)
{
    for (const UftEncodingDoc *doc = ENCODING_DOCS; doc->name; doc++) {
        if (QString(doc->name) == encodingName) {
            showEncodingDetails(doc);
            return;
        }
    }
}

void UftFormatInfoPanel::showEncodingDetails(const UftEncodingDoc *doc)
{
    QString html = QString("<h1>%1</h1>").arg(doc->name);
    html += QString("<p>%1</p>").arg(doc->description);
    html += "<table border='1' cellpadding='5'>";
    html += QString("<tr><th>Property</th><th>Value</th></tr>");
    html += QString("<tr><td>Bit Cell</td><td>%1 ns</td></tr>").arg(doc->bitcell_ns);
    html += QString("<tr><td>Data Rate</td><td>%1 kbit/s</td></tr>").arg(doc->data_rate_kbps);
    html += QString("<tr><td>Sync Pattern</td><td>%1</td></tr>").arg(doc->sync_pattern);
    html += QString("<tr><td>Platforms</td><td>%1</td></tr>").arg(doc->platforms);
    html += "</table>";
    m_encodingBrowser->setHtml(html);
}

void UftFormatInfoPanel::onFilterChanged(const QString &text)
{
    QString filter = text.toLower();
    
    for (int i = 0; i < m_formatTree->topLevelItemCount(); i++) {
        QTreeWidgetItem *cat = m_formatTree->topLevelItem(i);
        bool catVisible = false;
        
        for (int j = 0; j < cat->childCount(); j++) {
            QTreeWidgetItem *item = cat->child(j);
            bool match = filter.isEmpty() ||
                        item->text(0).toLower().contains(filter) ||
                        item->text(1).toLower().contains(filter);
            item->setHidden(!match);
            if (match) catVisible = true;
        }
        
        cat->setHidden(!catVisible);
    }
}

void UftFormatInfoPanel::onCategoryChanged(int index)
{
    QString catFilter;
    switch (index) {
        case 1: catFilter = "Sector"; break;
        case 2: catFilter = "Bitstream"; break;
        case 3: catFilter = "Flux"; break;
    }
    
    for (int i = 0; i < m_formatTree->topLevelItemCount(); i++) {
        QTreeWidgetItem *cat = m_formatTree->topLevelItem(i);
        bool visible = catFilter.isEmpty() || cat->text(0).contains(catFilter);
        cat->setHidden(!visible);
    }
}

void UftFormatInfoPanel::onLinkClicked(const QUrl &url)
{
    QDesktopServices::openUrl(url);
    emit linkClicked(url.toString());
}

/*===========================================================================
 * UftQuickReferenceDialog
 *===========================================================================*/

UftQuickReferenceDialog::UftQuickReferenceDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Quick Reference"));
    setMinimumSize(400, 300);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    m_browser = new QTextBrowser();
    m_browser->setOpenExternalLinks(true);
    layout->addWidget(m_browser);
}

void UftQuickReferenceDialog::setFormat(const QString &formatId)
{
    for (const UftFormatDoc *doc = FORMAT_DOCS; doc->id; doc++) {
        if (QString(doc->id) == formatId) {
            QString html = QString("<h2>%1 - %2</h2>").arg(doc->id).arg(doc->name);
            html += QString("<p><b>Platform:</b> %1 | <b>Type:</b> %2</p>")
                    .arg(doc->platform).arg(doc->category);
            html += QString("<p>%1</p>").arg(doc->description);
            html += doc->structure;
            m_browser->setHtml(html);
            return;
        }
    }
    m_browser->setHtml(tr("<p>Format not found.</p>"));
}

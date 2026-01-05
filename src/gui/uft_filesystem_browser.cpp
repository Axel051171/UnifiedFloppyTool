/**
 * @file uft_filesystem_browser.cpp
 * @brief Filesystem Browser Implementation (BONUS-GUI-001)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft_filesystem_browser.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QScrollBar>
#include <QCheckBox>
#include <QSpinBox>
#include <QApplication>

/*===========================================================================
 * C64 BASIC Tokens
 *===========================================================================*/

static const char* C64_BASIC_TOKENS[] = {
    "END", "FOR", "NEXT", "DATA", "INPUT#", "INPUT", "DIM", "READ",
    "LET", "GOTO", "RUN", "IF", "RESTORE", "GOSUB", "RETURN", "REM",
    "STOP", "ON", "WAIT", "LOAD", "SAVE", "VERIFY", "DEF", "POKE",
    "PRINT#", "PRINT", "CONT", "LIST", "CLR", "CMD", "SYS", "OPEN",
    "CLOSE", "GET", "NEW", "TAB(", "TO", "FN", "SPC(", "THEN",
    "NOT", "STEP", "+", "-", "*", "/", "^", "AND",
    "OR", ">", "=", "<", "SGN", "INT", "ABS", "USR",
    "FRE", "POS", "SQR", "RND", "LOG", "EXP", "COS", "SIN",
    "TAN", "ATN", "PEEK", "LEN", "STR$", "VAL", "ASC", "CHR$",
    "LEFT$", "RIGHT$", "MID$", "GO", nullptr
};

/*===========================================================================
 * PETSCII Conversion
 *===========================================================================*/

static QString petsciiToUnicode(uint8_t c)
{
    /* Convert PETSCII to printable character */
    if (c >= 0x41 && c <= 0x5A) return QString(QChar(c + 0x20));  /* A-Z -> a-z */
    if (c >= 0x61 && c <= 0x7A) return QString(QChar(c - 0x20));  /* a-z -> A-Z */
    if (c >= 0x20 && c <= 0x3F) return QString(QChar(c));         /* Space to ? */
    if (c == 0x0D) return "\n";                                    /* Return */
    if (c >= 0xC1 && c <= 0xDA) return QString(QChar(c - 0x80));  /* Shifted letters */
    return ".";  /* Non-printable */
}

/*===========================================================================
 * UftHexPreview
 *===========================================================================*/

UftHexPreview::UftHexPreview(QWidget *parent)
    : QWidget(parent)
    , m_offset(0)
    , m_bytesPerRow(16)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSpacing(2);
    layout->setContentsMargins(0, 0, 0, 0);
    
    m_hexView = new QTextEdit();
    m_hexView->setReadOnly(true);
    m_hexView->setStyleSheet("font-family: monospace; font-size: 11px;");
    m_hexView->setLineWrapMode(QTextEdit::NoWrap);
    
    m_asciiView = new QTextEdit();
    m_asciiView->setReadOnly(true);
    m_asciiView->setStyleSheet("font-family: monospace; font-size: 11px;");
    m_asciiView->setLineWrapMode(QTextEdit::NoWrap);
    m_asciiView->setMaximumWidth(180);
    
    /* Sync scrolling */
    connect(m_hexView->verticalScrollBar(), &QScrollBar::valueChanged,
            m_asciiView->verticalScrollBar(), &QScrollBar::setValue);
    connect(m_asciiView->verticalScrollBar(), &QScrollBar::valueChanged,
            m_hexView->verticalScrollBar(), &QScrollBar::setValue);
    
    layout->addWidget(m_hexView, 3);
    layout->addWidget(m_asciiView, 1);
}

void UftHexPreview::setData(const QByteArray &data)
{
    m_data = data;
    updateView();
}

void UftHexPreview::clear()
{
    m_data.clear();
    m_hexView->clear();
    m_asciiView->clear();
}

void UftHexPreview::setOffset(int offset)
{
    m_offset = offset;
    updateView();
}

int UftHexPreview::offset() const
{
    return m_offset;
}

void UftHexPreview::updateView()
{
    QString hexText, asciiText;
    
    for (int i = 0; i < m_data.size(); i += m_bytesPerRow) {
        /* Offset */
        hexText += QString("%1: ").arg(m_offset + i, 6, 16, QChar('0'));
        
        QString hexLine, asciiLine;
        for (int j = 0; j < m_bytesPerRow && i + j < m_data.size(); j++) {
            uint8_t b = (uint8_t)m_data[i + j];
            hexLine += QString("%1 ").arg(b, 2, 16, QChar('0'));
            asciiLine += (b >= 0x20 && b < 0x7F) ? QChar(b) : '.';
        }
        
        hexText += hexLine + "\n";
        asciiText += asciiLine + "\n";
    }
    
    m_hexView->setText(hexText);
    m_asciiView->setText(asciiText);
}

/*===========================================================================
 * UftTextPreview
 *===========================================================================*/

UftTextPreview::UftTextPreview(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel(tr("Encoding:")));
    m_encodingCombo = new QComboBox();
    m_encodingCombo->addItems({"Auto", "PETSCII", "ATASCII", "ASCII", "UTF-8"});
    topLayout->addWidget(m_encodingCombo);
    topLayout->addStretch();
    layout->addLayout(topLayout);
    
    m_textView = new QTextEdit();
    m_textView->setReadOnly(true);
    m_textView->setStyleSheet("font-family: monospace;");
    layout->addWidget(m_textView);
}

void UftTextPreview::setData(const QByteArray &data, const QString &format)
{
    QString text;
    
    /* Try to detect BASIC program */
    if (data.size() > 2) {
        uint16_t loadAddr = (uint8_t)data[0] | ((uint8_t)data[1] << 8);
        if (loadAddr == 0x0801 || loadAddr == 0x1C01) {
            /* Likely C64/C128 BASIC */
            text = tokenizeBASIC(data, "C64");
        } else if (format.contains("D64") || format.contains("C64")) {
            text = convertPETSCII(data);
        } else if (format.contains("ATR") || format.contains("Atari")) {
            text = convertATASCII(data);
        } else {
            /* Try ASCII */
            text = QString::fromLatin1(data);
        }
    } else {
        text = QString::fromLatin1(data);
    }
    
    m_textView->setText(text);
}

void UftTextPreview::clear()
{
    m_textView->clear();
}

QString UftTextPreview::convertPETSCII(const QByteArray &data)
{
    QString result;
    for (int i = 0; i < data.size(); i++) {
        result += petsciiToUnicode((uint8_t)data[i]);
    }
    return result;
}

QString UftTextPreview::convertATASCII(const QByteArray &data)
{
    QString result;
    for (int i = 0; i < data.size(); i++) {
        uint8_t c = (uint8_t)data[i];
        if (c == 0x9B) result += '\n';  /* EOL */
        else if (c >= 0x20 && c < 0x7F) result += QChar(c);
        else result += '.';
    }
    return result;
}

QString UftTextPreview::tokenizeBASIC(const QByteArray &data, const QString &format)
{
    QString result;
    
    if (data.size() < 4) return convertPETSCII(data);
    
    int pos = 2;  /* Skip load address */
    
    while (pos + 4 < data.size()) {
        /* Next line pointer */
        uint16_t nextLine = (uint8_t)data[pos] | ((uint8_t)data[pos + 1] << 8);
        if (nextLine == 0) break;  /* End of program */
        
        /* Line number */
        uint16_t lineNum = (uint8_t)data[pos + 2] | ((uint8_t)data[pos + 3] << 8);
        result += QString::number(lineNum) + " ";
        
        pos += 4;
        
        /* Tokenized line */
        while (pos < data.size() && data[pos] != 0) {
            uint8_t c = (uint8_t)data[pos];
            
            if (c >= 0x80 && c <= 0xCB && C64_BASIC_TOKENS[c - 0x80]) {
                result += C64_BASIC_TOKENS[c - 0x80];
            } else {
                result += petsciiToUnicode(c);
            }
            pos++;
        }
        
        result += "\n";
        pos++;  /* Skip null terminator */
    }
    
    return result;
}

/*===========================================================================
 * UftImagePreview
 *===========================================================================*/

UftImagePreview::UftImagePreview(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel(tr("Palette:")));
    m_paletteCombo = new QComboBox();
    m_paletteCombo->addItems({"C64", "VIC-20", "Atari", "CGA", "Grayscale"});
    topLayout->addWidget(m_paletteCombo);
    topLayout->addStretch();
    layout->addLayout(topLayout);
    
    m_imageLabel = new QLabel(tr("No image data"));
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(320, 200);
    m_imageLabel->setStyleSheet("background: #000;");
    layout->addWidget(m_imageLabel);
}

void UftImagePreview::setData(const QByteArray &data, const QString &type)
{
    /* TODO: Implement sprite/graphics rendering */
    m_imageLabel->setText(tr("Image preview: %1 bytes").arg(data.size()));
}

void UftImagePreview::clear()
{
    m_imageLabel->setText(tr("No image data"));
}

/*===========================================================================
 * UftFilesystemBrowser
 *===========================================================================*/

UftFilesystemBrowser::UftFilesystemBrowser(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void UftFilesystemBrowser::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    
    createToolbar();
    mainLayout->addWidget(m_toolbar);
    
    /* Main splitter */
    m_splitter = new QSplitter(Qt::Horizontal);
    
    /* Left: File list + Info */
    QWidget *leftWidget = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    createFileList();
    leftLayout->addWidget(m_fileTree);
    
    createInfoPanel();
    leftLayout->addWidget(m_infoGroup);
    
    m_splitter->addWidget(leftWidget);
    
    /* Right: Preview */
    createPreviewPanel();
    m_splitter->addWidget(m_previewTabs);
    
    m_splitter->setSizes({400, 400});
    mainLayout->addWidget(m_splitter);
}

void UftFilesystemBrowser::createToolbar()
{
    m_toolbar = new QToolBar();
    
    m_openButton = new QPushButton(tr("Open Disk..."));
    m_toolbar->addWidget(m_openButton);
    
    m_toolbar->addSeparator();
    
    m_refreshAction = m_toolbar->addAction(tr("Refresh"));
    m_extractAction = m_toolbar->addAction(tr("Extract"));
    m_extractAllAction = m_toolbar->addAction(tr("Extract All"));
    
    m_toolbar->addSeparator();
    
    m_toolbar->addWidget(new QLabel(tr("Filter:")));
    m_filterEdit = new QLineEdit();
    m_filterEdit->setMaximumWidth(150);
    m_filterEdit->setPlaceholderText(tr("*.prg"));
    m_toolbar->addWidget(m_filterEdit);
    
    m_showDeletedCheck = new QCheckBox(tr("Show Deleted"));
    m_toolbar->addWidget(m_showDeletedCheck);
    
    connect(m_openButton, &QPushButton::clicked, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Open Disk Image"),
            QString(), tr("Disk Images (*.d64 *.d71 *.d81 *.adf *.atr *.dsk *.img);;All Files (*)"));
        if (!path.isEmpty()) {
            loadDiskImage(path);
        }
    });
    
    connect(m_refreshAction, &QAction::triggered, this, &UftFilesystemBrowser::refresh);
    connect(m_extractAction, &QAction::triggered, this, &UftFilesystemBrowser::extractSelected);
    connect(m_extractAllAction, &QAction::triggered, this, &UftFilesystemBrowser::extractAll);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &UftFilesystemBrowser::onFilterChanged);
    connect(m_showDeletedCheck, &QCheckBox::toggled, this, &UftFilesystemBrowser::onShowDeleted);
}

void UftFilesystemBrowser::createFileList()
{
    m_fileTree = new QTreeWidget();
    m_fileTree->setHeaderLabels({tr("Name"), tr("Type"), tr("Size"), tr("Blocks"), tr("T/S")});
    m_fileTree->setRootIsDecorated(false);
    m_fileTree->setAlternatingRowColors(true);
    m_fileTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileTree->setContextMenuPolicy(Qt::CustomContextMenu);
    
    m_fileTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fileTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_fileTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    
    connect(m_fileTree, &QTreeWidget::itemClicked, this, &UftFilesystemBrowser::onFileSelected);
    connect(m_fileTree, &QTreeWidget::itemDoubleClicked, this, &UftFilesystemBrowser::onFileDoubleClicked);
    connect(m_fileTree, &QTreeWidget::customContextMenuRequested, this, &UftFilesystemBrowser::onContextMenu);
}

void UftFilesystemBrowser::createPreviewPanel()
{
    m_previewTabs = new QTabWidget();
    
    m_hexPreview = new UftHexPreview();
    m_textPreview = new UftTextPreview();
    m_imagePreview = new UftImagePreview();
    
    m_previewTabs->addTab(m_hexPreview, tr("Hex"));
    m_previewTabs->addTab(m_textPreview, tr("Text/BASIC"));
    m_previewTabs->addTab(m_imagePreview, tr("Graphics"));
}

void UftFilesystemBrowser::createInfoPanel()
{
    m_infoGroup = new QGroupBox(tr("Disk Information"));
    QGridLayout *grid = new QGridLayout(m_infoGroup);
    
    grid->addWidget(new QLabel(tr("Name:")), 0, 0);
    m_diskNameLabel = new QLabel("-");
    m_diskNameLabel->setStyleSheet("font-weight: bold;");
    grid->addWidget(m_diskNameLabel, 0, 1);
    
    grid->addWidget(new QLabel(tr("Format:")), 0, 2);
    m_formatLabel = new QLabel("-");
    grid->addWidget(m_formatLabel, 0, 3);
    
    grid->addWidget(new QLabel(tr("Files:")), 1, 0);
    m_filesLabel = new QLabel("-");
    grid->addWidget(m_filesLabel, 1, 1);
    
    grid->addWidget(new QLabel(tr("Free:")), 1, 2);
    m_freeLabel = new QLabel("-");
    grid->addWidget(m_freeLabel, 1, 3);
}

void UftFilesystemBrowser::loadDiskImage(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Error"), 
            tr("Cannot open file: %1").arg(path));
        return;
    }
    
    m_diskPath = path;
    m_diskData = file.readAll();
    file.close();
    
    parseFilesystem();
    populateFileList();
    updateInfo();
}

void UftFilesystemBrowser::parseFilesystem()
{
    m_entries.clear();
    m_fsInfo = UftFsInfo();
    
    QFileInfo fi(m_diskPath);
    QString ext = fi.suffix().toLower();
    
    if (ext == "d64" || ext == "d71" || ext == "d81") {
        parseD64();
    } else if (ext == "adf") {
        parseADF();
    } else if (ext == "atr") {
        parseATR();
    } else if (ext == "dsk" || ext == "do" || ext == "po") {
        parseDSK();
    } else {
        parseFAT12();
    }
}

void UftFilesystemBrowser::parseD64()
{
    /* D64: 35 tracks, 683 blocks (174848 bytes standard) */
    
    if (m_diskData.size() < 174848) {
        m_fsInfo.format = "Invalid D64";
        return;
    }
    
    m_fsInfo.format = "Commodore DOS 2.6";
    m_fsInfo.tracks = 35;
    
    /* Track 18, Sector 0 = Directory header (BAM) */
    int bamOffset = 0x16500;  /* Track 18, Sector 0 */
    
    /* Disk name at BAM + 0x90 (16 bytes, PETSCII) */
    QString diskName;
    for (int i = 0; i < 16; i++) {
        uint8_t c = (uint8_t)m_diskData[bamOffset + 0x90 + i];
        if (c == 0xA0) break;  /* Shifted space = end */
        diskName += petsciiToUnicode(c);
    }
    m_fsInfo.diskName = diskName.trimmed();
    
    /* Disk ID at BAM + 0xA2 (2 bytes) */
    m_fsInfo.diskId = QString("%1%2")
        .arg(petsciiToUnicode((uint8_t)m_diskData[bamOffset + 0xA2]))
        .arg(petsciiToUnicode((uint8_t)m_diskData[bamOffset + 0xA3]));
    
    /* Count free blocks from BAM */
    m_fsInfo.freeBlocks = 0;
    for (int t = 1; t <= 35; t++) {
        if (t == 18) continue;  /* Directory track */
        int bamEntry = bamOffset + 4 * t;
        m_fsInfo.freeBlocks += (uint8_t)m_diskData[bamEntry];
    }
    m_fsInfo.totalBlocks = 664;  /* Standard D64 */
    m_fsInfo.usedBlocks = m_fsInfo.totalBlocks - m_fsInfo.freeBlocks;
    
    /* Parse directory (Track 18, Sectors 1-18) */
    int dirTrack = 18;
    int dirSector = 1;
    
    while (dirTrack != 0 && dirSector != 255) {
        int dirOffset = getD64Offset(dirTrack, dirSector);
        if (dirOffset < 0 || dirOffset + 256 > m_diskData.size()) break;
        
        /* Next directory sector */
        dirTrack = (uint8_t)m_diskData[dirOffset];
        dirSector = (uint8_t)m_diskData[dirOffset + 1];
        
        /* 8 directory entries per sector */
        for (int e = 0; e < 8; e++) {
            int entryOffset = dirOffset + 0x02 + e * 0x20;
            
            uint8_t fileType = (uint8_t)m_diskData[entryOffset];
            if (fileType == 0x00) continue;  /* Empty slot */
            
            UftFsEntry entry;
            entry.isDeleted = (fileType & 0x80) == 0;
            
            /* File type */
            switch (fileType & 0x07) {
                case 0: entry.type = "DEL"; break;
                case 1: entry.type = "SEQ"; break;
                case 2: entry.type = "PRG"; break;
                case 3: entry.type = "USR"; break;
                case 4: entry.type = "REL"; break;
                default: entry.type = "???"; break;
            }
            
            entry.isLocked = (fileType & 0x40) != 0;
            
            /* Filename (16 bytes PETSCII) */
            QString name;
            for (int i = 0; i < 16; i++) {
                uint8_t c = (uint8_t)m_diskData[entryOffset + 0x03 + i];
                if (c == 0xA0) break;
                name += petsciiToUnicode(c);
            }
            entry.name = name.trimmed();
            
            /* Start track/sector */
            entry.startTrack = (uint8_t)m_diskData[entryOffset + 0x01];
            entry.startSector = (uint8_t)m_diskData[entryOffset + 0x02];
            
            /* Block count */
            entry.blocks = (uint8_t)m_diskData[entryOffset + 0x1C] |
                          ((uint8_t)m_diskData[entryOffset + 0x1D] << 8);
            entry.size = entry.blocks * 254;  /* 254 bytes per block (2 for chain) */
            
            m_entries.append(entry);
        }
    }
    
    m_fsInfo.totalFiles = m_entries.size();
}

int UftFilesystemBrowser::getD64Offset(int track, int sector)
{
    /* D64 track/sector to byte offset */
    static const int sectorsPerTrack[] = {
        0,  /* Track 0 doesn't exist */
        21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 1-17 */
        19, 19, 19, 19, 19, 19, 19,  /* 18-24 */
        18, 18, 18, 18, 18, 18,      /* 25-30 */
        17, 17, 17, 17, 17           /* 31-35 */
    };
    
    if (track < 1 || track > 35) return -1;
    if (sector < 0 || sector >= sectorsPerTrack[track]) return -1;
    
    int offset = 0;
    for (int t = 1; t < track; t++) {
        offset += sectorsPerTrack[t] * 256;
    }
    offset += sector * 256;
    
    return offset;
}

void UftFilesystemBrowser::parseADF()
{
    /* ADF: 880KB Amiga disk, 1760 sectors of 512 bytes */
    
    if (m_diskData.size() != 901120) {
        m_fsInfo.format = "Invalid ADF";
        return;
    }
    
    m_fsInfo.format = "Amiga OFS/FFS";
    m_fsInfo.tracks = 160;
    m_fsInfo.totalBlocks = 1760;
    
    /* Root block at sector 880 */
    int rootOffset = 880 * 512;
    
    /* Check block type (should be 2 = T_HEADER) */
    uint32_t blockType = qFromBigEndian<uint32_t>((const uchar*)m_diskData.data() + rootOffset);
    if (blockType != 2) {
        m_fsInfo.format = "ADF (no valid root)";
        return;
    }
    
    /* Disk name at root + 432 (30 bytes + length byte) */
    int nameLen = (uint8_t)m_diskData[rootOffset + 432];
    if (nameLen > 30) nameLen = 30;
    m_fsInfo.diskName = QString::fromLatin1(m_diskData.mid(rootOffset + 433, nameLen));
    
    /* Hash table at root + 24 (72 entries) */
    for (int i = 0; i < 72; i++) {
        int hashOffset = rootOffset + 24 + i * 4;
        uint32_t headerBlock = qFromBigEndian<uint32_t>((const uchar*)m_diskData.data() + hashOffset);
        
        if (headerBlock == 0) continue;
        
        /* Read file header */
        int headerOffset = headerBlock * 512;
        if (headerOffset + 512 > m_diskData.size()) continue;
        
        uint32_t hdrType = qFromBigEndian<uint32_t>((const uchar*)m_diskData.data() + headerOffset);
        if (hdrType != 2) continue;
        
        UftFsEntry entry;
        
        /* Secondary type at offset 508 */
        int32_t secType = qFromBigEndian<int32_t>((const uchar*)m_diskData.data() + headerOffset + 508);
        entry.isDirectory = (secType == 2);  /* ST_USERDIR */
        entry.type = entry.isDirectory ? "DIR" : "FILE";
        
        /* Filename at offset 432 */
        int fNameLen = (uint8_t)m_diskData[headerOffset + 432];
        if (fNameLen > 30) fNameLen = 30;
        entry.name = QString::fromLatin1(m_diskData.mid(headerOffset + 433, fNameLen));
        
        /* File size at offset 324 */
        entry.size = qFromBigEndian<uint32_t>((const uchar*)m_diskData.data() + headerOffset + 324);
        entry.blocks = (entry.size + 511) / 512;
        
        m_entries.append(entry);
    }
    
    m_fsInfo.totalFiles = m_entries.size();
}

void UftFilesystemBrowser::parseATR()
{
    /* ATR: Atari 8-bit disk with 16-byte header */
    
    if (m_diskData.size() < 16) {
        m_fsInfo.format = "Invalid ATR";
        return;
    }
    
    /* Check magic */
    if ((uint8_t)m_diskData[0] != 0x96 || (uint8_t)m_diskData[1] != 0x02) {
        m_fsInfo.format = "Invalid ATR magic";
        return;
    }
    
    m_fsInfo.format = "Atari DOS 2.x";
    
    /* Sector size */
    int sectorSize = (uint8_t)m_diskData[4] | ((uint8_t)m_diskData[5] << 8);
    if (sectorSize == 0) sectorSize = 128;
    
    /* Paragraph count */
    int paragraphs = (uint8_t)m_diskData[2] | ((uint8_t)m_diskData[3] << 8);
    int diskSize = paragraphs * 16;
    
    m_fsInfo.totalBlocks = diskSize / sectorSize;
    
    /* VTOC at sector 360 (offset 16 + 359 * 128 for SD) */
    int vtocOffset = 16 + 359 * 128;
    if (vtocOffset + 128 > m_diskData.size()) {
        m_fsInfo.format = "ATR (no VTOC)";
        return;
    }
    
    /* Free sectors at VTOC + 3-4 */
    m_fsInfo.freeBlocks = (uint8_t)m_diskData[vtocOffset + 3] | 
                          ((uint8_t)m_diskData[vtocOffset + 4] << 8);
    
    /* Directory at sectors 361-368 */
    for (int dirSec = 361; dirSec <= 368; dirSec++) {
        int dirOffset = 16 + (dirSec - 1) * 128;
        if (dirOffset + 128 > m_diskData.size()) break;
        
        /* 8 entries per sector, 16 bytes each */
        for (int e = 0; e < 8; e++) {
            int entryOffset = dirOffset + e * 16;
            
            uint8_t flags = (uint8_t)m_diskData[entryOffset];
            if (flags == 0x00) continue;  /* Unused */
            
            UftFsEntry entry;
            entry.isDeleted = (flags & 0x80) != 0;
            entry.isLocked = (flags & 0x20) != 0;
            
            /* Filename (8.3) */
            QString name = QString::fromLatin1(m_diskData.mid(entryOffset + 5, 8)).trimmed();
            QString ext = QString::fromLatin1(m_diskData.mid(entryOffset + 13, 3)).trimmed();
            entry.name = ext.isEmpty() ? name : name + "." + ext;
            
            entry.type = ext.isEmpty() ? "BIN" : ext.toUpper();
            
            /* Sector count */
            entry.blocks = (uint8_t)m_diskData[entryOffset + 1] |
                          ((uint8_t)m_diskData[entryOffset + 2] << 8);
            entry.size = entry.blocks * 125;  /* 125 data bytes per sector */
            
            /* Start sector */
            entry.startSector = (uint8_t)m_diskData[entryOffset + 3] |
                               ((uint8_t)m_diskData[entryOffset + 4] << 8);
            
            m_entries.append(entry);
        }
    }
    
    m_fsInfo.totalFiles = m_entries.size();
}

void UftFilesystemBrowser::parseDSK()
{
    /* Apple II DSK: 140KB, 35 tracks, 16 sectors */
    
    if (m_diskData.size() != 143360) {
        m_fsInfo.format = "Invalid DSK size";
        return;
    }
    
    m_fsInfo.format = "Apple DOS 3.3";
    m_fsInfo.tracks = 35;
    m_fsInfo.totalBlocks = 560;  /* 35 tracks * 16 sectors */
    
    /* VTOC at Track 17, Sector 0 */
    int vtocOffset = 17 * 16 * 256;
    
    /* Free sectors */
    m_fsInfo.freeBlocks = 0;
    for (int t = 0; t < 35; t++) {
        int mapOffset = vtocOffset + 0x38 + t * 4;
        uint32_t bitmap = (uint8_t)m_diskData[mapOffset] |
                         ((uint8_t)m_diskData[mapOffset + 1] << 8);
        while (bitmap) {
            m_fsInfo.freeBlocks += (bitmap & 1);
            bitmap >>= 1;
        }
    }
    
    /* Catalog at Track 17, Sectors 15-1 */
    int catTrack = 17;
    int catSector = 15;
    
    while (catTrack != 0) {
        int catOffset = catTrack * 16 * 256 + catSector * 256;
        if (catOffset + 256 > m_diskData.size()) break;
        
        /* Next catalog sector */
        catTrack = (uint8_t)m_diskData[catOffset + 1];
        catSector = (uint8_t)m_diskData[catOffset + 2];
        
        /* 7 file entries per sector */
        for (int e = 0; e < 7; e++) {
            int entryOffset = catOffset + 0x0B + e * 0x23;
            
            int firstTrack = (uint8_t)m_diskData[entryOffset];
            if (firstTrack == 0x00 || firstTrack == 0xFF) continue;
            
            UftFsEntry entry;
            entry.isDeleted = (firstTrack == 0xFF);
            
            uint8_t typeFlags = (uint8_t)m_diskData[entryOffset + 2];
            entry.isLocked = (typeFlags & 0x80) != 0;
            
            switch (typeFlags & 0x7F) {
                case 0x00: entry.type = "TXT"; break;
                case 0x01: entry.type = "INT"; break;
                case 0x02: entry.type = "APP"; break;
                case 0x04: entry.type = "BIN"; break;
                case 0x08: entry.type = "S"; break;
                case 0x10: entry.type = "REL"; break;
                case 0x20: entry.type = "A"; break;
                case 0x40: entry.type = "B"; break;
                default: entry.type = "???"; break;
            }
            
            /* Filename (30 bytes) */
            entry.name = QString::fromLatin1(m_diskData.mid(entryOffset + 3, 30)).trimmed();
            entry.name.replace(QChar(0xA0), ' ');
            
            /* Size */
            entry.blocks = (uint8_t)m_diskData[entryOffset + 0x21] |
                          ((uint8_t)m_diskData[entryOffset + 0x22] << 8);
            entry.size = entry.blocks * 256;
            
            entry.startTrack = (uint8_t)m_diskData[entryOffset];
            entry.startSector = (uint8_t)m_diskData[entryOffset + 1];
            
            m_entries.append(entry);
        }
    }
    
    m_fsInfo.totalFiles = m_entries.size();
}

void UftFilesystemBrowser::parseFAT12()
{
    /* Generic FAT12 parsing for PC/DOS disks */
    
    if (m_diskData.size() < 512) {
        m_fsInfo.format = "Unknown";
        return;
    }
    
    /* Check boot sector */
    if ((uint8_t)m_diskData[510] != 0x55 || (uint8_t)m_diskData[511] != 0xAA) {
        m_fsInfo.format = "No valid boot sector";
        return;
    }
    
    m_fsInfo.format = "FAT12";
    
    /* BPB */
    int bytesPerSector = (uint8_t)m_diskData[11] | ((uint8_t)m_diskData[12] << 8);
    int sectorsPerCluster = (uint8_t)m_diskData[13];
    int reservedSectors = (uint8_t)m_diskData[14] | ((uint8_t)m_diskData[15] << 8);
    int numFATs = (uint8_t)m_diskData[16];
    int rootEntries = (uint8_t)m_diskData[17] | ((uint8_t)m_diskData[18] << 8);
    int totalSectors = (uint8_t)m_diskData[19] | ((uint8_t)m_diskData[20] << 8);
    int sectorsPerFAT = (uint8_t)m_diskData[22] | ((uint8_t)m_diskData[23] << 8);
    
    if (bytesPerSector == 0) bytesPerSector = 512;
    
    m_fsInfo.totalBlocks = totalSectors;
    
    /* Volume label */
    m_fsInfo.diskName = QString::fromLatin1(m_diskData.mid(43, 11)).trimmed();
    
    /* Root directory */
    int rootDirOffset = (reservedSectors + numFATs * sectorsPerFAT) * bytesPerSector;
    int rootDirSize = rootEntries * 32;
    
    for (int i = 0; i < rootEntries; i++) {
        int entryOffset = rootDirOffset + i * 32;
        if (entryOffset + 32 > m_diskData.size()) break;
        
        uint8_t firstByte = (uint8_t)m_diskData[entryOffset];
        if (firstByte == 0x00) break;  /* End of directory */
        if (firstByte == 0xE5) continue;  /* Deleted */
        if (firstByte == 0x2E) continue;  /* . or .. */
        
        uint8_t attrs = (uint8_t)m_diskData[entryOffset + 11];
        if (attrs == 0x0F) continue;  /* LFN entry */
        
        UftFsEntry entry;
        entry.isDirectory = (attrs & 0x10) != 0;
        entry.isHidden = (attrs & 0x02) != 0;
        entry.isLocked = (attrs & 0x01) != 0;
        
        /* Filename (8.3) */
        QString name = QString::fromLatin1(m_diskData.mid(entryOffset, 8)).trimmed();
        QString ext = QString::fromLatin1(m_diskData.mid(entryOffset + 8, 3)).trimmed();
        entry.name = ext.isEmpty() ? name : name + "." + ext;
        entry.type = entry.isDirectory ? "DIR" : ext.toUpper();
        
        /* Size */
        entry.size = (uint8_t)m_diskData[entryOffset + 28] |
                    ((uint8_t)m_diskData[entryOffset + 29] << 8) |
                    ((uint8_t)m_diskData[entryOffset + 30] << 16) |
                    ((uint8_t)m_diskData[entryOffset + 31] << 24);
        entry.blocks = (entry.size + bytesPerSector * sectorsPerCluster - 1) / 
                       (bytesPerSector * sectorsPerCluster);
        
        m_entries.append(entry);
    }
    
    m_fsInfo.totalFiles = m_entries.size();
}

void UftFilesystemBrowser::populateFileList()
{
    m_fileTree->clear();
    
    bool showDeleted = m_showDeletedCheck->isChecked();
    QString filter = m_filterEdit->text();
    
    for (const auto &entry : m_entries) {
        if (!showDeleted && entry.isDeleted) continue;
        
        if (!filter.isEmpty()) {
            QRegularExpression rx(QRegularExpression::wildcardToRegularExpression(filter),
                                  QRegularExpression::CaseInsensitiveOption);
            if (!rx.match(entry.name).hasMatch()) continue;
        }
        
        QTreeWidgetItem *item = new QTreeWidgetItem();
        
        QString displayName = entry.name;
        if (entry.isDeleted) displayName = "×" + displayName;
        if (entry.isLocked) displayName += " 🔒";
        
        item->setText(0, displayName);
        item->setText(1, entry.type);
        item->setText(2, QString::number(entry.size));
        item->setText(3, QString::number(entry.blocks));
        item->setText(4, QString("%1/%2").arg(entry.startTrack).arg(entry.startSector));
        
        item->setData(0, Qt::UserRole, QVariant::fromValue(m_entries.indexOf(entry)));
        
        if (entry.isDeleted) {
            item->setForeground(0, Qt::gray);
        }
        if (entry.isDirectory) {
            item->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
        }
        
        m_fileTree->addTopLevelItem(item);
    }
}

void UftFilesystemBrowser::updateInfo()
{
    m_diskNameLabel->setText(m_fsInfo.diskName.isEmpty() ? "-" : m_fsInfo.diskName);
    m_formatLabel->setText(m_fsInfo.format);
    m_filesLabel->setText(QString::number(m_fsInfo.totalFiles));
    m_freeLabel->setText(QString("%1 blocks").arg(m_fsInfo.freeBlocks));
}

void UftFilesystemBrowser::onFileSelected(QTreeWidgetItem *item, int)
{
    int index = item->data(0, Qt::UserRole).toInt();
    if (index >= 0 && index < m_entries.size()) {
        m_selectedEntry = m_entries[index];
        updatePreview(m_selectedEntry);
        emit fileSelected(m_selectedEntry);
    }
}

void UftFilesystemBrowser::onFileDoubleClicked(QTreeWidgetItem *item, int)
{
    int index = item->data(0, Qt::UserRole).toInt();
    if (index >= 0 && index < m_entries.size()) {
        emit fileDoubleClicked(m_entries[index]);
    }
}

void UftFilesystemBrowser::updatePreview(const UftFsEntry &entry)
{
    QByteArray data = readFile(entry);
    
    m_hexPreview->setData(data);
    m_textPreview->setData(data, m_fsInfo.format);
    m_imagePreview->setData(data, entry.type);
}

QByteArray UftFilesystemBrowser::readFile(const UftFsEntry &entry)
{
    /* Simplified file reading - just read sequential sectors */
    QByteArray data;
    
    QFileInfo fi(m_diskPath);
    QString ext = fi.suffix().toLower();
    
    if (ext == "d64") {
        /* Follow sector chain */
        int track = entry.startTrack;
        int sector = entry.startSector;
        int maxBlocks = entry.blocks + 1;
        
        while (track != 0 && maxBlocks-- > 0) {
            int offset = getD64Offset(track, sector);
            if (offset < 0 || offset + 256 > m_diskData.size()) break;
            
            track = (uint8_t)m_diskData[offset];
            sector = (uint8_t)m_diskData[offset + 1];
            
            int dataLen = (track == 0) ? sector : 254;
            data.append(m_diskData.mid(offset + 2, dataLen));
        }
    } else {
        /* Simple: read entry.size bytes from start position */
        int offset = entry.startTrack * 256 * 16 + entry.startSector * 256;
        if (offset >= 0 && offset + entry.size <= (quint32)m_diskData.size()) {
            data = m_diskData.mid(offset, entry.size);
        }
    }
    
    return data;
}

void UftFilesystemBrowser::onContextMenu(const QPoint &pos)
{
    QMenu menu;
    menu.addAction(tr("View"), this, &UftFilesystemBrowser::viewSelected);
    menu.addAction(tr("Extract..."), this, &UftFilesystemBrowser::extractSelected);
    menu.addSeparator();
    menu.addAction(tr("Extract All..."), this, &UftFilesystemBrowser::extractAll);
    menu.exec(m_fileTree->mapToGlobal(pos));
}

void UftFilesystemBrowser::onFilterChanged(const QString &)
{
    populateFileList();
}

void UftFilesystemBrowser::onShowDeleted(bool)
{
    populateFileList();
}

void UftFilesystemBrowser::refresh()
{
    if (!m_diskPath.isEmpty()) {
        loadDiskImage(m_diskPath);
    }
}

void UftFilesystemBrowser::clear()
{
    m_diskPath.clear();
    m_diskData.clear();
    m_entries.clear();
    m_fileTree->clear();
    m_hexPreview->clear();
    m_textPreview->clear();
    updateInfo();
}

void UftFilesystemBrowser::viewSelected()
{
    /* Switch to hex tab */
    m_previewTabs->setCurrentWidget(m_hexPreview);
}

void UftFilesystemBrowser::extractSelected()
{
    QList<QTreeWidgetItem*> selected = m_fileTree->selectedItems();
    if (selected.isEmpty()) return;
    
    QString dir = QFileDialog::getExistingDirectory(this, tr("Extract To"));
    if (dir.isEmpty()) return;
    
    for (auto *item : selected) {
        int index = item->data(0, Qt::UserRole).toInt();
        if (index < 0 || index >= m_entries.size()) continue;
        
        const UftFsEntry &entry = m_entries[index];
        QByteArray data = readFile(entry);
        
        QString outPath = dir + "/" + entry.name;
        QFile outFile(outPath);
        if (outFile.open(QIODevice::WriteOnly)) {
            outFile.write(data);
            outFile.close();
        }
    }
    
    QMessageBox::information(this, tr("Extract"), 
        tr("Extracted %1 file(s)").arg(selected.size()));
}

void UftFilesystemBrowser::extractAll()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Extract All To"));
    if (dir.isEmpty()) return;
    
    int count = 0;
    for (const auto &entry : m_entries) {
        if (entry.isDeleted || entry.isDirectory) continue;
        
        QByteArray data = readFile(entry);
        
        QString outPath = dir + "/" + entry.name;
        QFile outFile(outPath);
        if (outFile.open(QIODevice::WriteOnly)) {
            outFile.write(data);
            outFile.close();
            count++;
        }
    }
    
    QMessageBox::information(this, tr("Extract All"), 
        tr("Extracted %1 file(s)").arg(count));
}

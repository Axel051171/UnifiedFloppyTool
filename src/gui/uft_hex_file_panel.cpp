/**
 * @file uft_hex_file_panel.cpp
 * @brief Hex Viewer and File Browser Panel Implementations
 */

#include "uft_hex_viewer_panel.h"
#include "uft_file_browser_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QHeaderView>
#include <QPainter>
#include <QScrollBar>
#include <QKeyEvent>

/* ============================================================================
 * Hex View Widget
 * ============================================================================ */

UftHexView::UftHexView(QWidget *parent)
    : QWidget(parent)
    , m_offset(0)
    , m_selectionStart(-1)
    , m_selectionEnd(-1)
    , m_cursorPos(0)
    , m_bytesPerLine(16)
    , m_readOnly(true)
{
    m_font = QFont("Consolas", 10);
    if (!m_font.exactMatch()) m_font = QFont("Courier New", 10);
    setFont(m_font);
    
    m_scrollBar = new QScrollBar(Qt::Vertical, this);
    connect(m_scrollBar, &QScrollBar::valueChanged, this, [this](int value) {
        m_offset = value * m_bytesPerLine;
        update();
    });
    
    setMinimumSize(400, 200);
    setFocusPolicy(Qt::StrongFocus);
}

void UftHexView::setData(const QByteArray &data)
{
    m_data = data;
    m_offset = 0;
    m_selectionStart = -1;
    m_selectionEnd = -1;
    updateScrollBar();
    update();
}

QByteArray UftHexView::getData() const { return m_data; }

void UftHexView::setOffset(qint64 offset) { m_offset = offset; update(); }
qint64 UftHexView::getOffset() const { return m_offset; }

void UftHexView::setBytesPerLine(int bytes) { m_bytesPerLine = bytes; updateScrollBar(); update(); }
int UftHexView::getBytesPerLine() const { return m_bytesPerLine; }

void UftHexView::setReadOnly(bool ro) { m_readOnly = ro; }
bool UftHexView::isReadOnly() const { return m_readOnly; }

void UftHexView::updateScrollBar()
{
    int lines = (m_data.size() + m_bytesPerLine - 1) / m_bytesPerLine;
    int visibleLines = height() / fontMetrics().height();
    m_scrollBar->setMaximum(qMax(0, lines - visibleLines));
    m_scrollBar->setPageStep(visibleLines);
}

void UftHexView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setFont(m_font);
    
    QFontMetrics fm = fontMetrics();
    int charW = fm.horizontalAdvance('0');
    int charH = fm.height();
    int addrW = charW * 10;
    int hexW = charW * (m_bytesPerLine * 3);
    
    p.fillRect(rect(), QColor(255, 255, 255));
    p.fillRect(0, 0, addrW, height(), QColor(245, 245, 245));
    
    int y = charH;
    qint64 offset = m_offset;
    
    while (y < height() && offset < m_data.size()) {
        /* Address */
        p.setPen(QColor(100, 100, 100));
        p.drawText(4, y, QString("%1").arg(offset, 8, 16, QChar('0')).toUpper());
        
        /* Hex bytes */
        int x = addrW + charW;
        for (int i = 0; i < m_bytesPerLine && offset + i < m_data.size(); i++) {
            uint8_t byte = m_data[offset + i];
            
            /* Selection highlight */
            if (offset + i >= m_selectionStart && offset + i <= m_selectionEnd) {
                p.fillRect(x - 2, y - charH + 4, charW * 2 + 2, charH, QColor(200, 220, 255));
            }
            
            p.setPen(QColor(0, 0, 0));
            p.drawText(x, y, QString("%1").arg(byte, 2, 16, QChar('0')).toUpper());
            x += charW * 3;
        }
        
        /* ASCII */
        x = addrW + hexW + charW * 2;
        p.setPen(QColor(100, 100, 100));
        for (int i = 0; i < m_bytesPerLine && offset + i < m_data.size(); i++) {
            char c = m_data[offset + i];
            if (c < 32 || c > 126) c = '.';
            p.drawText(x, y, QString(c));
            x += charW;
        }
        
        y += charH;
        offset += m_bytesPerLine;
    }
    
    /* Scrollbar */
    m_scrollBar->setGeometry(width() - 15, 0, 15, height());
}

void UftHexView::keyPressEvent(QKeyEvent *event) { QWidget::keyPressEvent(event); }
void UftHexView::mousePressEvent(QMouseEvent *event) { QWidget::mousePressEvent(event); }
void UftHexView::mouseMoveEvent(QMouseEvent *event) { QWidget::mouseMoveEvent(event); }
void UftHexView::wheelEvent(QWheelEvent *event)
{
    int delta = event->angleDelta().y() / 120;
    m_scrollBar->setValue(m_scrollBar->value() - delta * 3);
}
void UftHexView::resizeEvent(QResizeEvent *event) { updateScrollBar(); QWidget::resizeEvent(event); }

/* ============================================================================
 * Hex Viewer Panel
 * ============================================================================ */

UftHexViewerPanel::UftHexViewerPanel(QWidget *parent)
    : QWidget(parent), m_currentTrack(0), m_currentSide(0), m_currentSector(0)
{
    setupUi();
}

void UftHexViewerPanel::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    /* Top: Navigation + Search */
    QHBoxLayout *topLayout = new QHBoxLayout();
    createNavigationGroup();
    createSearchGroup();
    topLayout->addWidget(m_navigationGroup);
    topLayout->addWidget(m_searchGroup);
    mainLayout->addLayout(topLayout);
    
    /* Middle: Hex View */
    createHexView();
    mainLayout->addWidget(m_hexView, 1);
    
    /* Bottom: Info + Actions */
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    createInfoGroup();
    bottomLayout->addWidget(m_infoGroup);
    
    QHBoxLayout *actionLayout = new QHBoxLayout();
    m_exportButton = new QPushButton("Export");
    m_importButton = new QPushButton("Import");
    m_editButton = new QPushButton("Edit");
    m_saveButton = new QPushButton("Save");
    actionLayout->addWidget(m_exportButton);
    actionLayout->addWidget(m_importButton);
    actionLayout->addWidget(m_editButton);
    actionLayout->addWidget(m_saveButton);
    bottomLayout->addLayout(actionLayout);
    
    mainLayout->addLayout(bottomLayout);
    
    connect(m_exportButton, &QPushButton::clicked, this, &UftHexViewerPanel::onExportSelection);
    connect(m_importButton, &QPushButton::clicked, this, &UftHexViewerPanel::onImportData);
}

void UftHexViewerPanel::createNavigationGroup()
{
    m_navigationGroup = new QGroupBox("Navigation", this);
    QHBoxLayout *layout = new QHBoxLayout(m_navigationGroup);
    
    m_viewMode = new QComboBox();
    m_viewMode->addItem("Bootblock");
    m_viewMode->addItem("Sector");
    m_viewMode->addItem("Track");
    m_viewMode->addItem("Raw");
    layout->addWidget(new QLabel("View:"));
    layout->addWidget(m_viewMode);
    
    m_trackSelect = new QSpinBox();
    m_trackSelect->setRange(0, 255);
    layout->addWidget(new QLabel("Track:"));
    layout->addWidget(m_trackSelect);
    
    m_sideSelect = new QComboBox();
    m_sideSelect->addItem("0");
    m_sideSelect->addItem("1");
    layout->addWidget(new QLabel("Side:"));
    layout->addWidget(m_sideSelect);
    
    m_sectorSelect = new QSpinBox();
    m_sectorSelect->setRange(0, 255);
    layout->addWidget(new QLabel("Sector:"));
    layout->addWidget(m_sectorSelect);
    
    m_prevButton = new QPushButton("<");
    m_nextButton = new QPushButton(">");
    m_gotoButton = new QPushButton("Go");
    m_prevButton->setMaximumWidth(30);
    m_nextButton->setMaximumWidth(30);
    layout->addWidget(m_prevButton);
    layout->addWidget(m_nextButton);
    layout->addWidget(m_gotoButton);
    
    connect(m_gotoButton, &QPushButton::clicked, this, &UftHexViewerPanel::onGotoOffset);
}

void UftHexViewerPanel::createSearchGroup()
{
    m_searchGroup = new QGroupBox("Search", this);
    QHBoxLayout *layout = new QHBoxLayout(m_searchGroup);
    
    m_searchHex = new QLineEdit();
    m_searchHex->setPlaceholderText("Hex (e.g. 4F 53)");
    m_searchAscii = new QLineEdit();
    m_searchAscii->setPlaceholderText("ASCII");
    
    m_caseSensitive = new QCheckBox("Case");
    m_findNextButton = new QPushButton("Next");
    m_findPrevButton = new QPushButton("Prev");
    m_searchResult = new QLabel("");
    
    layout->addWidget(m_searchHex);
    layout->addWidget(m_searchAscii);
    layout->addWidget(m_caseSensitive);
    layout->addWidget(m_findPrevButton);
    layout->addWidget(m_findNextButton);
    layout->addWidget(m_searchResult);
    
    connect(m_findNextButton, &QPushButton::clicked, this, &UftHexViewerPanel::onSearch);
}

void UftHexViewerPanel::createInfoGroup()
{
    m_infoGroup = new QGroupBox("Info", this);
    QFormLayout *layout = new QFormLayout(m_infoGroup);
    
    m_offsetLabel = new QLabel("0x00000000");
    m_sizeLabel = new QLabel("0 bytes");
    m_selectionLabel = new QLabel("None");
    m_checksumLabel = new QLabel("-");
    
    layout->addRow("Offset:", m_offsetLabel);
    layout->addRow("Size:", m_sizeLabel);
    layout->addRow("Selection:", m_selectionLabel);
    layout->addRow("CRC:", m_checksumLabel);
}

void UftHexViewerPanel::createHexView()
{
    m_hexView = new UftHexView(this);
    
    QHBoxLayout *optLayout = new QHBoxLayout();
    m_bytesPerLine = new QComboBox();
    m_bytesPerLine->addItem("8", 8);
    m_bytesPerLine->addItem("16", 16);
    m_bytesPerLine->addItem("32", 32);
    m_bytesPerLine->setCurrentIndex(1);
    
    m_showAscii = new QCheckBox("ASCII");
    m_showAscii->setChecked(true);
    m_showOffset = new QCheckBox("Offset");
    m_showOffset->setChecked(true);
    
    connect(m_bytesPerLine, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_hexView->setBytesPerLine(m_bytesPerLine->itemData(idx).toInt());
    });
}

void UftHexViewerPanel::loadBootblock(const uint8_t *data, size_t size)
{
    m_currentData = QByteArray((const char*)data, size);
    m_hexView->setData(m_currentData);
    m_sizeLabel->setText(QString("%1 bytes").arg(size));
    updateInfo();
}

void UftHexViewerPanel::loadSector(int track, int side, int sector, const uint8_t *data, size_t size)
{
    m_currentTrack = track;
    m_currentSide = side;
    m_currentSector = sector;
    m_currentData = QByteArray((const char*)data, size);
    m_hexView->setData(m_currentData);
    m_trackSelect->setValue(track);
    m_sideSelect->setCurrentIndex(side);
    m_sectorSelect->setValue(sector);
    updateInfo();
}

void UftHexViewerPanel::loadTrack(int track, int side, const uint8_t *data, size_t size)
{
    m_currentTrack = track;
    m_currentSide = side;
    m_currentData = QByteArray((const char*)data, size);
    m_hexView->setData(m_currentData);
    updateInfo();
}

void UftHexViewerPanel::updateInfo()
{
    m_sizeLabel->setText(QString("%1 bytes").arg(m_currentData.size()));
    
    /* Calculate CRC32 */
    uint32_t crc = 0xFFFFFFFF;
    for (char c : m_currentData) {
        crc ^= (uint8_t)c;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    m_checksumLabel->setText(QString("%1").arg(~crc, 8, 16, QChar('0')).toUpper());
}

void UftHexViewerPanel::onSectorSelected(int track, int side, int sector)
{
    m_trackSelect->setValue(track);
    m_sideSelect->setCurrentIndex(side);
    m_sectorSelect->setValue(sector);
}

void UftHexViewerPanel::onTrackSelected(int track, int side)
{
    m_trackSelect->setValue(track);
    m_sideSelect->setCurrentIndex(side);
}

void UftHexViewerPanel::onSearch()
{
    QString hex = m_searchHex->text();
    if (!hex.isEmpty()) {
        m_searchResult->setText("Searching...");
        /* TODO: Implement search */
    }
}

void UftHexViewerPanel::onGotoOffset()
{
    /* TODO: Show goto dialog */
}

void UftHexViewerPanel::onExportSelection()
{
    QString path = QFileDialog::getSaveFileName(this, "Export Data");
    if (!path.isEmpty()) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(m_currentData);
            f.close();
        }
    }
}

void UftHexViewerPanel::onImportData()
{
    QString path = QFileDialog::getOpenFileName(this, "Import Data");
    if (!path.isEmpty()) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            m_currentData = f.readAll();
            m_hexView->setData(m_currentData);
            f.close();
            emit dataModified();
        }
    }
}

/* ============================================================================
 * File Browser Panel
 * ============================================================================ */

UftFileBrowserPanel::UftFileBrowserPanel(QWidget *parent) : QWidget(parent) { setupUi(); }

void UftFileBrowserPanel::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    createToolBar();
    createFileTable();
    createInfoPanel();
    createStatusBar();
    
    /* Layout */
    QHBoxLayout *toolLayout = new QHBoxLayout();
    toolLayout->addWidget(m_extractButton);
    toolLayout->addWidget(m_extractAllButton);
    toolLayout->addWidget(m_injectButton);
    toolLayout->addWidget(m_deleteButton);
    toolLayout->addWidget(m_refreshButton);
    toolLayout->addWidget(m_viewButton);
    toolLayout->addStretch();
    toolLayout->addWidget(new QLabel("Filter:"));
    toolLayout->addWidget(m_filterEdit);
    toolLayout->addWidget(new QLabel("Sort:"));
    toolLayout->addWidget(m_sortBy);
    mainLayout->addLayout(toolLayout);
    
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->addWidget(m_fileTable, 2);
    contentLayout->addWidget(m_infoGroup, 1);
    mainLayout->addLayout(contentLayout, 1);
    
    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addWidget(m_fileCountLabel);
    statusLayout->addWidget(m_operationProgress);
    mainLayout->addLayout(statusLayout);
}

void UftFileBrowserPanel::createToolBar()
{
    m_extractButton = new QPushButton("Extract");
    m_extractAllButton = new QPushButton("Extract All");
    m_injectButton = new QPushButton("Inject");
    m_deleteButton = new QPushButton("Delete");
    m_refreshButton = new QPushButton("Refresh");
    m_viewButton = new QPushButton("View");
    
    m_filterEdit = new QLineEdit();
    m_filterEdit->setPlaceholderText("Filter...");
    m_filterEdit->setMaximumWidth(150);
    
    m_sortBy = new QComboBox();
    m_sortBy->addItem("Name");
    m_sortBy->addItem("Size");
    m_sortBy->addItem("Type");
    
    connect(m_extractButton, &QPushButton::clicked, this, &UftFileBrowserPanel::onExtractSelected);
    connect(m_extractAllButton, &QPushButton::clicked, this, &UftFileBrowserPanel::onExtractAll);
    connect(m_injectButton, &QPushButton::clicked, this, &UftFileBrowserPanel::onInjectFile);
    connect(m_deleteButton, &QPushButton::clicked, this, &UftFileBrowserPanel::onDeleteSelected);
    connect(m_refreshButton, &QPushButton::clicked, this, &UftFileBrowserPanel::onRefresh);
    connect(m_viewButton, &QPushButton::clicked, this, &UftFileBrowserPanel::onViewFile);
}

void UftFileBrowserPanel::createFileTable()
{
    m_fileTable = new QTableWidget(this);
    m_fileTable->setColumnCount(7);
    m_fileTable->setHorizontalHeaderLabels({"Name", "Type", "Size", "Blocks", "Track", "Sector", "Flags"});
    m_fileTable->horizontalHeader()->setStretchLastSection(true);
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileTable->setAlternatingRowColors(true);
    
    connect(m_fileTable, &QTableWidget::itemDoubleClicked, this, &UftFileBrowserPanel::onFileDoubleClicked);
}

void UftFileBrowserPanel::createInfoPanel()
{
    m_infoGroup = new QGroupBox("Disk Info", this);
    QFormLayout *layout = new QFormLayout(m_infoGroup);
    
    m_diskNameLabel = new QLabel("-");
    m_diskIdLabel = new QLabel("-");
    m_filesystemLabel = new QLabel("-");
    m_totalBlocksLabel = new QLabel("-");
    m_freeBlocksLabel = new QLabel("-");
    m_usedBlocksLabel = new QLabel("-");
    m_usageBar = new QProgressBar();
    m_usageBar->setTextVisible(true);
    
    layout->addRow("Disk Name:", m_diskNameLabel);
    layout->addRow("Disk ID:", m_diskIdLabel);
    layout->addRow("Filesystem:", m_filesystemLabel);
    layout->addRow("Total:", m_totalBlocksLabel);
    layout->addRow("Free:", m_freeBlocksLabel);
    layout->addRow("Used:", m_usedBlocksLabel);
    layout->addRow("Usage:", m_usageBar);
}

void UftFileBrowserPanel::createStatusBar()
{
    m_statusLabel = new QLabel("Ready");
    m_fileCountLabel = new QLabel("0 files");
    m_operationProgress = new QProgressBar();
    m_operationProgress->setMaximumWidth(200);
    m_operationProgress->setVisible(false);
}

void UftFileBrowserPanel::loadDirectory(const QString &imagePath)
{
    m_currentImage = imagePath;
    m_fileTable->setRowCount(0);
    
    /* TODO: Call UFT core to list files */
    
    /* Simulate some files */
    QStringList names = {"LOADER", "GAME.PRG", "SPRITES.DAT", "MUSIC.SID"};
    for (int i = 0; i < names.size(); i++) {
        m_fileTable->insertRow(i);
        m_fileTable->setItem(i, 0, new QTableWidgetItem(names[i]));
        m_fileTable->setItem(i, 1, new QTableWidgetItem("PRG"));
        m_fileTable->setItem(i, 2, new QTableWidgetItem(QString::number(1024 * (i + 1))));
        m_fileTable->setItem(i, 3, new QTableWidgetItem(QString::number(4 * (i + 1))));
        m_fileTable->setItem(i, 4, new QTableWidgetItem("17"));
        m_fileTable->setItem(i, 5, new QTableWidgetItem(QString::number(i)));
        m_fileTable->setItem(i, 6, new QTableWidgetItem(""));
    }
    
    m_fileCountLabel->setText(QString("%1 files").arg(names.size()));
    updateDiskInfo();
}

void UftFileBrowserPanel::clear()
{
    m_fileTable->setRowCount(0);
    m_files.clear();
    m_currentImage.clear();
}

void UftFileBrowserPanel::updateDiskInfo()
{
    m_diskNameLabel->setText("MY DISK");
    m_diskIdLabel->setText("AB");
    m_filesystemLabel->setText("CBM DOS");
    m_totalBlocksLabel->setText("664");
    m_freeBlocksLabel->setText("600");
    m_usedBlocksLabel->setText("64");
    m_usageBar->setValue(10);
}

void UftFileBrowserPanel::onExtractSelected()
{
    QList<QTableWidgetItem*> items = m_fileTable->selectedItems();
    if (items.isEmpty()) return;
    
    QString dir = QFileDialog::getExistingDirectory(this, "Extract To");
    if (dir.isEmpty()) return;
    
    m_statusLabel->setText("Extracting...");
    /* TODO: Extract files */
    m_statusLabel->setText("Done");
}

void UftFileBrowserPanel::onExtractAll()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Extract All To");
    if (dir.isEmpty()) return;
    
    m_statusLabel->setText("Extracting all...");
    /* TODO: Extract all files */
    m_statusLabel->setText("Done");
}

void UftFileBrowserPanel::onInjectFile()
{
    QString path = QFileDialog::getOpenFileName(this, "Select File to Inject");
    if (path.isEmpty()) return;
    
    m_statusLabel->setText("Injecting...");
    /* TODO: Inject file */
    onRefresh();
    m_statusLabel->setText("Done");
}

void UftFileBrowserPanel::onDeleteSelected()
{
    QList<QTableWidgetItem*> items = m_fileTable->selectedItems();
    if (items.isEmpty()) return;
    
    /* TODO: Delete files */
    onRefresh();
}

void UftFileBrowserPanel::onRefresh()
{
    if (!m_currentImage.isEmpty()) {
        loadDirectory(m_currentImage);
    }
}

void UftFileBrowserPanel::onViewFile()
{
    /* TODO: Show file in hex viewer */
}

void UftFileBrowserPanel::onFileDoubleClicked(QTableWidgetItem *item)
{
    Q_UNUSED(item)
    onViewFile();
}

/**
 * @file uft_disk_compare_view.cpp
 * @brief Disk Compare View Implementation (P2-GUI-009)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft_disk_compare_view.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPainter>
#include <QMouseEvent>
#include <QCryptographicHash>
#include <QDateTime>
#include <QApplication>
#include <QScrollBar>

/*===========================================================================
 * UftCompareTrackGrid
 *===========================================================================*/

UftCompareTrackGrid::UftCompareTrackGrid(QWidget *parent)
    : QWidget(parent)
    , m_selectedCyl(-1)
    , m_selectedHead(-1)
    , m_cellSize(12)
    , m_cylinders(80)
    , m_heads(2)
{
    setMinimumSize(200, 80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void UftCompareTrackGrid::setCompareResults(const QList<UftTrackCompare> &results)
{
    m_results = results;
    
    /* Determine grid size */
    m_cylinders = 0;
    m_heads = 0;
    for (const auto &r : results) {
        if (r.cylinder + 1 > m_cylinders) m_cylinders = r.cylinder + 1;
        if (r.head + 1 > m_heads) m_heads = r.head + 1;
    }
    
    update();
}

void UftCompareTrackGrid::clear()
{
    m_results.clear();
    m_selectedCyl = -1;
    m_selectedHead = -1;
    update();
}

void UftCompareTrackGrid::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    if (m_cylinders == 0 || m_heads == 0) {
        p.drawText(rect(), Qt::AlignCenter, tr("No comparison data"));
        return;
    }
    
    /* Calculate cell size */
    int margin = 30;
    int availW = width() - margin * 2;
    int availH = height() - margin * 2;
    
    int cellW = availW / m_cylinders;
    int cellH = availH / m_heads;
    m_cellSize = qMin(cellW, cellH);
    m_cellSize = qMax(m_cellSize, 4);
    m_cellSize = qMin(m_cellSize, 20);
    
    /* Draw labels */
    p.setPen(Qt::gray);
    p.drawText(margin, 15, tr("Head 0"));
    if (m_heads > 1) {
        p.drawText(margin, 15 + m_cellSize + 5, tr("Head 1"));
    }
    
    /* Draw track every 10 */
    for (int c = 0; c < m_cylinders; c += 10) {
        int x = margin + c * m_cellSize;
        p.drawText(x, height() - 5, QString::number(c));
    }
    
    /* Draw grid */
    for (int h = 0; h < m_heads; h++) {
        for (int c = 0; c < m_cylinders; c++) {
            QRect cell(margin + c * m_cellSize, 
                       20 + h * (m_cellSize + 5),
                       m_cellSize - 1, m_cellSize - 1);
            
            /* Find result for this track */
            QColor color = Qt::lightGray;  /* No data */
            
            for (const auto &r : m_results) {
                if (r.cylinder == c && r.head == h) {
                    if (!r.left_present && !r.right_present) {
                        color = Qt::lightGray;
                    } else if (!r.left_present) {
                        color = QColor(255, 200, 100);  /* Right only */
                    } else if (!r.right_present) {
                        color = QColor(100, 200, 255);  /* Left only */
                    } else if (r.similarity >= 1.0f) {
                        color = QColor(100, 200, 100);  /* Match */
                    } else if (r.similarity >= 0.9f) {
                        color = QColor(200, 200, 100);  /* Minor diff */
                    } else {
                        color = QColor(255, 100, 100);  /* Major diff */
                    }
                    break;
                }
            }
            
            p.fillRect(cell, color);
            
            /* Selection highlight */
            if (c == m_selectedCyl && h == m_selectedHead) {
                p.setPen(QPen(Qt::blue, 2));
                p.drawRect(cell.adjusted(-1, -1, 1, 1));
                p.setPen(Qt::NoPen);
            }
        }
    }
    
    /* Legend */
    int legendY = height() - 20;
    int legendX = margin;
    
    auto drawLegend = [&](QColor c, const QString &text) {
        p.fillRect(legendX, legendY, 12, 12, c);
        p.setPen(Qt::black);
        p.drawText(legendX + 15, legendY + 10, text);
        legendX += 80;
    };
    
    drawLegend(QColor(100, 200, 100), tr("Match"));
    drawLegend(QColor(255, 100, 100), tr("Differ"));
    drawLegend(QColor(100, 200, 255), tr("Left only"));
    drawLegend(QColor(255, 200, 100), tr("Right only"));
}

void UftCompareTrackGrid::mousePressEvent(QMouseEvent *event)
{
    int cyl, head;
    if (cellToTrack(event->pos(), cyl, head)) {
        m_selectedCyl = cyl;
        m_selectedHead = head;
        update();
        emit trackSelected(cyl, head);
    }
}

void UftCompareTrackGrid::mouseDoubleClickEvent(QMouseEvent *event)
{
    int cyl, head;
    if (cellToTrack(event->pos(), cyl, head)) {
        emit trackDoubleClicked(cyl, head);
    }
}

void UftCompareTrackGrid::resizeEvent(QResizeEvent *)
{
    update();
}

QPoint UftCompareTrackGrid::trackToCell(int cyl, int head) const
{
    int margin = 30;
    return QPoint(margin + cyl * m_cellSize, 20 + head * (m_cellSize + 5));
}

bool UftCompareTrackGrid::cellToTrack(const QPoint &pos, int &cyl, int &head) const
{
    int margin = 30;
    cyl = (pos.x() - margin) / m_cellSize;
    head = (pos.y() - 20) / (m_cellSize + 5);
    
    return cyl >= 0 && cyl < m_cylinders && head >= 0 && head < m_heads;
}

/*===========================================================================
 * UftHexDiffView
 *===========================================================================*/

UftHexDiffView::UftHexDiffView(QWidget *parent)
    : QWidget(parent)
    , m_highlightDiffs(true)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSpacing(4);
    layout->setContentsMargins(0, 0, 0, 0);
    
    m_leftHex = new QTextEdit();
    m_rightHex = new QTextEdit();
    
    m_leftHex->setReadOnly(true);
    m_rightHex->setReadOnly(true);
    m_leftHex->setStyleSheet("font-family: monospace; font-size: 10px;");
    m_rightHex->setStyleSheet("font-family: monospace; font-size: 10px;");
    
    /* Sync scrolling */
    connect(m_leftHex->verticalScrollBar(), &QScrollBar::valueChanged,
            m_rightHex->verticalScrollBar(), &QScrollBar::setValue);
    connect(m_rightHex->verticalScrollBar(), &QScrollBar::valueChanged,
            m_leftHex->verticalScrollBar(), &QScrollBar::setValue);
    
    layout->addWidget(m_leftHex);
    layout->addWidget(m_rightHex);
}

void UftHexDiffView::setData(const QByteArray &left, const QByteArray &right)
{
    m_leftData = left;
    m_rightData = right;
    updateView();
}

void UftHexDiffView::setHighlightDiffs(bool highlight)
{
    m_highlightDiffs = highlight;
    updateView();
}

void UftHexDiffView::clear()
{
    m_leftData.clear();
    m_rightData.clear();
    m_leftHex->clear();
    m_rightHex->clear();
}

void UftHexDiffView::updateView()
{
    QString leftHtml, rightHtml;
    
    int maxLen = qMax(m_leftData.size(), m_rightData.size());
    int bytesPerRow = 16;
    
    leftHtml = "<pre>";
    rightHtml = "<pre>";
    
    for (int i = 0; i < maxLen; i += bytesPerRow) {
        /* Address */
        leftHtml += QString("%1: ").arg(i, 4, 16, QChar('0'));
        rightHtml += QString("%1: ").arg(i, 4, 16, QChar('0'));
        
        /* Hex bytes */
        for (int j = 0; j < bytesPerRow && i + j < maxLen; j++) {
            uint8_t leftByte = (i + j < m_leftData.size()) ? 
                               (uint8_t)m_leftData[i + j] : 0;
            uint8_t rightByte = (i + j < m_rightData.size()) ? 
                                (uint8_t)m_rightData[i + j] : 0;
            
            bool differ = m_highlightDiffs && leftByte != rightByte;
            
            QString leftStr = QString("%1 ").arg(leftByte, 2, 16, QChar('0'));
            QString rightStr = QString("%1 ").arg(rightByte, 2, 16, QChar('0'));
            
            if (differ) {
                leftHtml += QString("<span style='background:#ffcccc;'>%1</span>").arg(leftStr);
                rightHtml += QString("<span style='background:#ffcccc;'>%1</span>").arg(rightStr);
            } else {
                leftHtml += leftStr;
                rightHtml += rightStr;
            }
        }
        
        leftHtml += "\n";
        rightHtml += "\n";
    }
    
    leftHtml += "</pre>";
    rightHtml += "</pre>";
    
    m_leftHex->setHtml(leftHtml);
    m_rightHex->setHtml(rightHtml);
}

/*===========================================================================
 * UftDiskCompareView
 *===========================================================================*/

UftDiskCompareView::UftDiskCompareView(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void UftDiskCompareView::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    createToolbar();
    createInfoPanels();
    
    /* Tab widget for views */
    m_tabWidget = new QTabWidget();
    
    createTrackView();
    createSectorView();
    createHexView();
    createSummaryView();
    
    mainLayout->addWidget(m_tabWidget);
    
    /* Progress */
    QHBoxLayout *progressLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(tr("Ready"));
    m_progressBar = new QProgressBar();
    m_progressBar->setMaximumWidth(200);
    m_progressBar->hide();
    progressLayout->addWidget(m_statusLabel);
    progressLayout->addStretch();
    progressLayout->addWidget(m_progressBar);
    mainLayout->addLayout(progressLayout);
}

void UftDiskCompareView::createToolbar()
{
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    
    toolbarLayout->addWidget(new QLabel(tr("Mode:")));
    m_compareMode = new QComboBox();
    m_compareMode->addItems({
        tr("Sector by sector"),
        tr("Byte by byte"),
        tr("Track structure"),
        tr("Hash only")
    });
    toolbarLayout->addWidget(m_compareMode);
    
    m_showOnlyDiffs = new QCheckBox(tr("Show only differences"));
    toolbarLayout->addWidget(m_showOnlyDiffs);
    
    m_ignoreTimingDiffs = new QCheckBox(tr("Ignore timing"));
    toolbarLayout->addWidget(m_ignoreTimingDiffs);
    
    toolbarLayout->addWidget(new QLabel(tr("Tolerance:")));
    m_tolerance = new QSpinBox();
    m_tolerance->setRange(0, 100);
    m_tolerance->setValue(0);
    m_tolerance->setSuffix("%");
    toolbarLayout->addWidget(m_tolerance);
    
    toolbarLayout->addStretch();
    
    m_compareButton = new QPushButton(tr("Compare"));
    m_compareButton->setEnabled(false);
    toolbarLayout->addWidget(m_compareButton);
    
    m_exportButton = new QPushButton(tr("Export Report"));
    m_exportButton->setEnabled(false);
    toolbarLayout->addWidget(m_exportButton);
    
    ((QVBoxLayout*)layout())->addLayout(toolbarLayout);
    
    connect(m_compareButton, &QPushButton::clicked, this, &UftDiskCompareView::onCompareClicked);
    connect(m_exportButton, &QPushButton::clicked, this, &UftDiskCompareView::exportReport);
    connect(m_compareMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftDiskCompareView::updateCompareMode);
}

void UftDiskCompareView::createInfoPanels()
{
    QHBoxLayout *panelLayout = new QHBoxLayout();
    
    /* Left disk info */
    m_leftGroup = new QGroupBox(tr("Left Disk (A)"));
    QGridLayout *leftGrid = new QGridLayout(m_leftGroup);
    
    m_leftPath = new QLabel(tr("-"));
    m_leftPath->setWordWrap(true);
    m_browseLeft = new QPushButton(tr("Browse..."));
    leftGrid->addWidget(m_leftPath, 0, 0, 1, 2);
    leftGrid->addWidget(m_browseLeft, 0, 2);
    
    leftGrid->addWidget(new QLabel(tr("Format:")), 1, 0);
    m_leftFormat = new QLabel(tr("-"));
    leftGrid->addWidget(m_leftFormat, 1, 1, 1, 2);
    
    leftGrid->addWidget(new QLabel(tr("Size:")), 2, 0);
    m_leftSize = new QLabel(tr("-"));
    leftGrid->addWidget(m_leftSize, 2, 1, 1, 2);
    
    leftGrid->addWidget(new QLabel(tr("Tracks:")), 3, 0);
    m_leftTracks = new QLabel(tr("-"));
    leftGrid->addWidget(m_leftTracks, 3, 1, 1, 2);
    
    leftGrid->addWidget(new QLabel(tr("MD5:")), 4, 0);
    m_leftHash = new QLabel(tr("-"));
    m_leftHash->setStyleSheet("font-family: monospace; font-size: 10px;");
    leftGrid->addWidget(m_leftHash, 4, 1, 1, 2);
    
    /* Swap button */
    QVBoxLayout *swapLayout = new QVBoxLayout();
    swapLayout->addStretch();
    m_swapButton = new QPushButton("⇄");
    m_swapButton->setMaximumWidth(40);
    m_swapButton->setToolTip(tr("Swap left and right disks"));
    swapLayout->addWidget(m_swapButton);
    swapLayout->addStretch();
    
    /* Right disk info */
    m_rightGroup = new QGroupBox(tr("Right Disk (B)"));
    QGridLayout *rightGrid = new QGridLayout(m_rightGroup);
    
    m_rightPath = new QLabel(tr("-"));
    m_rightPath->setWordWrap(true);
    m_browseRight = new QPushButton(tr("Browse..."));
    rightGrid->addWidget(m_rightPath, 0, 0, 1, 2);
    rightGrid->addWidget(m_browseRight, 0, 2);
    
    rightGrid->addWidget(new QLabel(tr("Format:")), 1, 0);
    m_rightFormat = new QLabel(tr("-"));
    rightGrid->addWidget(m_rightFormat, 1, 1, 1, 2);
    
    rightGrid->addWidget(new QLabel(tr("Size:")), 2, 0);
    m_rightSize = new QLabel(tr("-"));
    rightGrid->addWidget(m_rightSize, 2, 1, 1, 2);
    
    rightGrid->addWidget(new QLabel(tr("Tracks:")), 3, 0);
    m_rightTracks = new QLabel(tr("-"));
    rightGrid->addWidget(m_rightTracks, 3, 1, 1, 2);
    
    rightGrid->addWidget(new QLabel(tr("MD5:")), 4, 0);
    m_rightHash = new QLabel(tr("-"));
    m_rightHash->setStyleSheet("font-family: monospace; font-size: 10px;");
    rightGrid->addWidget(m_rightHash, 4, 1, 1, 2);
    
    panelLayout->addWidget(m_leftGroup);
    panelLayout->addLayout(swapLayout);
    panelLayout->addWidget(m_rightGroup);
    
    ((QVBoxLayout*)layout())->addLayout(panelLayout);
    
    connect(m_browseLeft, &QPushButton::clicked, this, &UftDiskCompareView::browseLeft);
    connect(m_browseRight, &QPushButton::clicked, this, &UftDiskCompareView::browseRight);
    connect(m_swapButton, &QPushButton::clicked, this, &UftDiskCompareView::swapDisks);
}

void UftDiskCompareView::createTrackView()
{
    QWidget *trackTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(trackTab);
    
    m_trackGrid = new UftCompareTrackGrid();
    m_trackGrid->setMinimumHeight(100);
    layout->addWidget(m_trackGrid);
    
    connect(m_trackGrid, &UftCompareTrackGrid::trackSelected,
            this, &UftDiskCompareView::onTrackSelected);
    
    m_tabWidget->addTab(trackTab, tr("Track Grid"));
}

void UftDiskCompareView::createSectorView()
{
    QWidget *sectorTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(sectorTab);
    
    m_sectorTable = new QTableWidget();
    m_sectorTable->setColumnCount(9);
    m_sectorTable->setHorizontalHeaderLabels({
        tr("C"), tr("H"), tr("S"), tr("Left"), tr("Right"),
        tr("Match"), tr("CRC L"), tr("CRC R"), tr("Diffs")
    });
    m_sectorTable->horizontalHeader()->setStretchLastSection(true);
    m_sectorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sectorTable->setAlternatingRowColors(true);
    
    connect(m_sectorTable, &QTableWidget::cellClicked,
            this, &UftDiskCompareView::onSectorSelected);
    
    layout->addWidget(m_sectorTable);
    
    m_tabWidget->addTab(sectorTab, tr("Sector List"));
}

void UftDiskCompareView::createHexView()
{
    m_hexView = new UftHexDiffView();
    m_tabWidget->addTab(m_hexView, tr("Hex Diff"));
}

void UftDiskCompareView::createSummaryView()
{
    m_summaryView = new QTextEdit();
    m_summaryView->setReadOnly(true);
    m_tabWidget->addTab(m_summaryView, tr("Summary"));
}

void UftDiskCompareView::browseLeft()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Left Disk Image"));
    if (!path.isEmpty()) {
        setLeftDisk(path);
    }
}

void UftDiskCompareView::browseRight()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Right Disk Image"));
    if (!path.isEmpty()) {
        setRightDisk(path);
    }
}

void UftDiskCompareView::swapDisks()
{
    QString tmp = m_leftDiskPath;
    setLeftDisk(m_rightDiskPath);
    setRightDisk(tmp);
}

void UftDiskCompareView::setLeftDisk(const QString &path)
{
    m_leftDiskPath = path;
    loadDiskInfo(path, true);
    
    m_compareButton->setEnabled(!m_leftDiskPath.isEmpty() && 
                                 !m_rightDiskPath.isEmpty());
}

void UftDiskCompareView::setRightDisk(const QString &path)
{
    m_rightDiskPath = path;
    loadDiskInfo(path, false);
    
    m_compareButton->setEnabled(!m_leftDiskPath.isEmpty() && 
                                 !m_rightDiskPath.isEmpty());
}

void UftDiskCompareView::loadDiskInfo(const QString &path, bool isLeft)
{
    QFileInfo fi(path);
    
    QLabel *pathLabel = isLeft ? m_leftPath : m_rightPath;
    QLabel *formatLabel = isLeft ? m_leftFormat : m_rightFormat;
    QLabel *sizeLabel = isLeft ? m_leftSize : m_rightSize;
    QLabel *tracksLabel = isLeft ? m_leftTracks : m_rightTracks;
    QLabel *hashLabel = isLeft ? m_leftHash : m_rightHash;
    
    pathLabel->setText(fi.fileName());
    pathLabel->setToolTip(path);
    sizeLabel->setText(QString("%1 bytes").arg(fi.size()));
    
    /* Detect format */
    QString ext = fi.suffix().toLower();
    formatLabel->setText(ext.toUpper());
    
    /* Estimate tracks */
    qint64 size = fi.size();
    int tracks = 0;
    if (ext == "adf" && size == 901120) tracks = 160;
    else if (ext == "d64") tracks = 35;
    else if (size == 1474560) tracks = 160;
    else if (size == 737280) tracks = 160;
    tracksLabel->setText(tracks > 0 ? QString::number(tracks) : "-");
    
    /* Calculate hash */
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData(&file);
        hashLabel->setText(hash.result().toHex().left(16) + "...");
        hashLabel->setToolTip(hash.result().toHex());
    }
}

void UftDiskCompareView::onCompareClicked()
{
    startComparison();
}

void UftDiskCompareView::startComparison()
{
    emit comparisonStarted();
    m_progressBar->show();
    m_statusLabel->setText(tr("Comparing..."));
    
    performComparison();
    
    m_progressBar->hide();
    m_exportButton->setEnabled(true);
    emit comparisonComplete(m_summary);
}

void UftDiskCompareView::performComparison()
{
    /* Clear previous results */
    m_trackResults.clear();
    m_sectorResults.clear();
    
    /* Read files */
    QFile leftFile(m_leftDiskPath);
    QFile rightFile(m_rightDiskPath);
    
    if (!leftFile.open(QIODevice::ReadOnly) || !rightFile.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot open files for comparison"));
        return;
    }
    
    QByteArray leftData = leftFile.readAll();
    QByteArray rightData = rightFile.readAll();
    
    /* Calculate hashes */
    QCryptographicHash leftMd5(QCryptographicHash::Md5);
    QCryptographicHash rightMd5(QCryptographicHash::Md5);
    leftMd5.addData(leftData);
    rightMd5.addData(rightData);
    
    m_summary.left_hash_md5 = leftMd5.result().toHex();
    m_summary.right_hash_md5 = rightMd5.result().toHex();
    
    /* Simple sector comparison (assuming 512-byte sectors) */
    int sectorSize = 512;
    int leftSectors = leftData.size() / sectorSize;
    int rightSectors = rightData.size() / sectorSize;
    int maxSectors = qMax(leftSectors, rightSectors);
    
    m_summary.total_sectors = maxSectors;
    m_summary.matching_sectors = 0;
    m_summary.differing_sectors = 0;
    
    m_sectorTable->setRowCount(0);
    
    for (int s = 0; s < maxSectors; s++) {
        int cyl = s / 36;  /* Assuming 18 sectors × 2 heads */
        int head = (s / 18) % 2;
        int sector = s % 18 + 1;
        
        UftSectorCompare sc;
        sc.cylinder = cyl;
        sc.head = head;
        sc.sector = sector;
        sc.left_present = (s < leftSectors);
        sc.right_present = (s < rightSectors);
        
        if (sc.left_present && sc.right_present) {
            QByteArray leftSec = leftData.mid(s * sectorSize, sectorSize);
            QByteArray rightSec = rightData.mid(s * sectorSize, sectorSize);
            
            sc.data_match = (leftSec == rightSec);
            
            if (sc.data_match) {
                m_summary.matching_sectors++;
            } else {
                m_summary.differing_sectors++;
                
                /* Count differences */
                sc.diff_count = 0;
                sc.first_diff_offset = -1;
                for (int i = 0; i < sectorSize; i++) {
                    if (leftSec[i] != rightSec[i]) {
                        sc.diff_count++;
                        if (sc.first_diff_offset < 0) {
                            sc.first_diff_offset = i;
                        }
                    }
                }
            }
        }
        
        m_sectorResults.append(sc);
        
        /* Add to table if different or not filtering */
        if (!m_showOnlyDiffs->isChecked() || !sc.data_match) {
            int row = m_sectorTable->rowCount();
            m_sectorTable->insertRow(row);
            m_sectorTable->setItem(row, 0, new QTableWidgetItem(QString::number(cyl)));
            m_sectorTable->setItem(row, 1, new QTableWidgetItem(QString::number(head)));
            m_sectorTable->setItem(row, 2, new QTableWidgetItem(QString::number(sector)));
            m_sectorTable->setItem(row, 3, new QTableWidgetItem(sc.left_present ? "✓" : "-"));
            m_sectorTable->setItem(row, 4, new QTableWidgetItem(sc.right_present ? "✓" : "-"));
            
            QTableWidgetItem *matchItem = new QTableWidgetItem(sc.data_match ? "=" : "≠");
            if (!sc.data_match) {
                matchItem->setBackground(QColor(255, 200, 200));
            }
            m_sectorTable->setItem(row, 5, matchItem);
            
            m_sectorTable->setItem(row, 6, new QTableWidgetItem("-"));
            m_sectorTable->setItem(row, 7, new QTableWidgetItem("-"));
            m_sectorTable->setItem(row, 8, new QTableWidgetItem(
                sc.data_match ? "-" : QString::number(sc.diff_count)));
        }
        
        /* Update progress */
        if (s % 100 == 0) {
            m_progressBar->setValue((s * 100) / maxSectors);
            QApplication::processEvents();
        }
    }
    
    /* Build track results */
    for (int c = 0; c < 80; c++) {
        for (int h = 0; h < 2; h++) {
            UftTrackCompare tc;
            tc.cylinder = c;
            tc.head = h;
            tc.left_present = true;
            tc.right_present = true;
            tc.sectors_match = 0;
            tc.sectors_differ = 0;
            
            int trackStart = (c * 2 + h) * 18;
            for (int s = 0; s < 18 && trackStart + s < m_sectorResults.size(); s++) {
                const auto &sr = m_sectorResults[trackStart + s];
                if (sr.data_match) tc.sectors_match++;
                else tc.sectors_differ++;
            }
            
            tc.similarity = tc.sectors_match / 18.0f;
            m_trackResults.append(tc);
        }
    }
    
    m_trackGrid->setCompareResults(m_trackResults);
    
    /* Calculate overall similarity */
    m_summary.overall_similarity = (float)m_summary.matching_sectors / m_summary.total_sectors;
    
    updateSummary();
}

void UftDiskCompareView::updateSummary()
{
    QString html;
    html += "<h2>Disk Comparison Report</h2>";
    html += QString("<p><b>Date:</b> %1</p>").arg(QDateTime::currentDateTime().toString());
    
    html += "<h3>Files</h3>";
    html += QString("<p><b>Left:</b> %1</p>").arg(m_leftDiskPath);
    html += QString("<p><b>Right:</b> %1</p>").arg(m_rightDiskPath);
    
    html += "<h3>Hashes</h3>";
    html += QString("<p><b>Left MD5:</b> <code>%1</code></p>").arg(m_summary.left_hash_md5);
    html += QString("<p><b>Right MD5:</b> <code>%1</code></p>").arg(m_summary.right_hash_md5);
    
    bool identical = (m_summary.left_hash_md5 == m_summary.right_hash_md5);
    if (identical) {
        html += "<p style='color: green; font-weight: bold;'>✓ Files are identical</p>";
    } else {
        html += "<p style='color: red; font-weight: bold;'>✗ Files differ</p>";
    }
    
    html += "<h3>Statistics</h3>";
    html += QString("<p><b>Total sectors:</b> %1</p>").arg(m_summary.total_sectors);
    html += QString("<p><b>Matching:</b> %1</p>").arg(m_summary.matching_sectors);
    html += QString("<p><b>Different:</b> %1</p>").arg(m_summary.differing_sectors);
    html += QString("<p><b>Similarity:</b> %.1f%%</p>").arg(m_summary.overall_similarity * 100);
    
    m_summaryView->setHtml(html);
}

void UftDiskCompareView::onTrackSelected(int cyl, int head)
{
    m_statusLabel->setText(QString(tr("Track %1.%2 selected")).arg(cyl).arg(head));
    
    /* Filter sector table to show only this track */
    int trackStart = (cyl * 2 + head) * 18;
    for (int i = 0; i < m_sectorTable->rowCount(); i++) {
        int rowCyl = m_sectorTable->item(i, 0)->text().toInt();
        int rowHead = m_sectorTable->item(i, 1)->text().toInt();
        m_sectorTable->setRowHidden(i, rowCyl != cyl || rowHead != head);
    }
}

void UftDiskCompareView::onSectorSelected(int row, int)
{
    if (row < 0 || row >= m_sectorResults.size()) return;
    
    /* Load sector data into hex view */
    int sectorSize = 512;
    int sectorIndex = row;  /* Simplified - needs proper mapping */
    
    QFile leftFile(m_leftDiskPath);
    QFile rightFile(m_rightDiskPath);
    
    if (leftFile.open(QIODevice::ReadOnly) && rightFile.open(QIODevice::ReadOnly)) {
        leftFile.seek(sectorIndex * sectorSize);
        rightFile.seek(sectorIndex * sectorSize);
        
        QByteArray leftSec = leftFile.read(sectorSize);
        QByteArray rightSec = rightFile.read(sectorSize);
        
        m_hexView->setData(leftSec, rightSec);
        m_tabWidget->setCurrentWidget(m_hexView->parentWidget());
    }
}

void UftDiskCompareView::updateCompareMode()
{
    /* Update UI based on compare mode */
}

void UftDiskCompareView::clear()
{
    m_leftDiskPath.clear();
    m_rightDiskPath.clear();
    m_leftPath->setText("-");
    m_rightPath->setText("-");
    m_trackGrid->clear();
    m_sectorTable->setRowCount(0);
    m_hexView->clear();
    m_summaryView->clear();
    m_compareButton->setEnabled(false);
    m_exportButton->setEnabled(false);
}

void UftDiskCompareView::cancelComparison()
{
    /* TODO: Implement cancellation */
}

void UftDiskCompareView::exportReport()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Export Report"),
                                                 "comparison_report.html",
                                                 tr("HTML Files (*.html)"));
    if (path.isEmpty()) return;
    
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << m_summaryView->toHtml();
        file.close();
        QMessageBox::information(this, tr("Export Complete"),
                                  tr("Report saved to %1").arg(path));
    }
}

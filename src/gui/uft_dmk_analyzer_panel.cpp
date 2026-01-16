/**
 * @file uft_dmk_analyzer_panel.cpp
 * @brief Implementation of DMK Disk Image Analyzer Panel
 * 
 * Based on qbarnes/fgrdmk concept (finger DMK)
 */

#include "uft_dmk_analyzer_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QTabWidget>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QFont>
#include <QFile>
#include <QDataStream>

extern "C" {
#include "uft/formats/uft_dmk.h"
}

/*============================================================================
 * Worker Thread Implementation
 *============================================================================*/

UftDmkAnalyzerWorker::UftDmkAnalyzerWorker(QObject *parent)
    : QThread(parent)
    , m_stopRequested(false)
    , m_operation(OP_NONE)
    , m_fillByte(0xE5)
{
}

UftDmkAnalyzerWorker::~UftDmkAnalyzerWorker()
{
    requestStop();
    wait();
}

void UftDmkAnalyzerWorker::setFile(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    m_filePath = path;
}

void UftDmkAnalyzerWorker::setExportPath(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    m_exportPath = path;
}

void UftDmkAnalyzerWorker::setExportFillByte(uint8_t fill)
{
    QMutexLocker locker(&m_mutex);
    m_fillByte = fill;
}

void UftDmkAnalyzerWorker::analyzeFile()
{
    QMutexLocker locker(&m_mutex);
    m_operation = OP_ANALYZE;
    m_stopRequested = false;
    start();
}

void UftDmkAnalyzerWorker::exportToRaw()
{
    QMutexLocker locker(&m_mutex);
    m_operation = OP_EXPORT;
    m_stopRequested = false;
    start();
}

void UftDmkAnalyzerWorker::requestStop()
{
    QMutexLocker locker(&m_mutex);
    m_stopRequested = true;
}

void UftDmkAnalyzerWorker::run()
{
    Operation op;
    {
        QMutexLocker locker(&m_mutex);
        op = m_operation;
    }

    switch (op) {
    case OP_ANALYZE:
        {
            emit analysisStarted();
            DmkAnalysisResult result = performAnalysis();
            if (result.valid) {
                emit analysisComplete(result);
            } else {
                emit analysisError(result.errorMessage);
            }
        }
        break;
    case OP_EXPORT:
        if (performExport()) {
            QFileInfo fi(m_exportPath);
            emit exportComplete(m_exportPath, fi.size());
        }
        break;
    default:
        break;
    }
}

DmkAnalysisResult UftDmkAnalyzerWorker::performAnalysis()
{
    DmkAnalysisResult result;
    result.valid = false;
    result.filename = m_filePath;
    result.totalSectors = 0;
    result.errorSectors = 0;
    result.deletedSectors = 0;
    result.fmSectors = 0;
    result.mfmSectors = 0;

    // Read file
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errorMessage = QString("Cannot open file: %1").arg(file.errorString());
        return result;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.size() < UFT_DMK_HEADER_SIZE) {
        result.errorMessage = "File too small for DMK header";
        return result;
    }

    // Parse header
    const uint8_t *ptr = reinterpret_cast<const uint8_t*>(data.constData());
    const uft_dmk_header_t *header = reinterpret_cast<const uft_dmk_header_t*>(ptr);

    result.writeProtected = (header->write_protect != 0);
    result.tracks = header->tracks;
    result.trackLength = header->track_length;
    result.singleSided = (header->flags & UFT_DMK_FLAG_SS) != 0;
    result.singleDensity = (header->flags & UFT_DMK_FLAG_SD) != 0;
    result.nativeMode = (header->native_flag == UFT_DMK_NATIVE_SIG);
    result.heads = result.singleSided ? 1 : 2;

    // Validate geometry
    if (result.tracks == 0 || result.tracks > UFT_DMK_MAX_TRACKS) {
        result.errorMessage = QString("Invalid track count: %1").arg(result.tracks);
        return result;
    }

    if (result.trackLength < UFT_DMK_IDAM_TABLE_SIZE || result.trackLength > 0x4000) {
        result.errorMessage = QString("Invalid track length: %1").arg(result.trackLength);
        return result;
    }

    int expectedSize = UFT_DMK_HEADER_SIZE + result.tracks * result.heads * result.trackLength;
    if (data.size() < expectedSize) {
        result.errorMessage = QString("File truncated: expected %1 bytes, got %2")
                              .arg(expectedSize).arg(data.size());
        return result;
    }

    // Analyze each track
    int offset = UFT_DMK_HEADER_SIZE;
    int totalTracks = result.tracks * result.heads;
    
    for (int t = 0; t < result.tracks && !m_stopRequested; t++) {
        for (int h = 0; h < result.heads && !m_stopRequested; h++) {
            emit analysisProgress(t * result.heads + h, totalTracks);

            DmkTrackInfo trackInfo;
            trackInfo.cylinder = t;
            trackInfo.head = h;
            trackInfo.trackLength = result.trackLength;
            trackInfo.hasErrors = false;
            trackInfo.numIdams = 0;
            trackInfo.numSectors = 0;

            const uint8_t *trackData = ptr + offset;
            const uint16_t *idamTable = reinterpret_cast<const uint16_t*>(trackData);

            // Parse IDAM table
            QList<int> idamOffsets;
            for (int i = 0; i < UFT_DMK_MAX_IDAMS; i++) {
                uint16_t entry = idamTable[i];
                if (entry == 0) break;
                
                uint16_t idamOffset = entry & UFT_DMK_IDAM_MASK;
                bool isFm = (entry & UFT_DMK_IDAM_SD_FLAG) != 0;
                
                if (idamOffset >= UFT_DMK_IDAM_TABLE_SIZE && 
                    idamOffset < result.trackLength) {
                    idamOffsets.append(idamOffset);
                    trackInfo.numIdams++;
                }
            }

            // Parse sectors from IDAM pointers
            for (int idamOffset : idamOffsets) {
                if (m_stopRequested) break;

                DmkSectorInfo sector;
                
                // Check for MFM sync (A1 A1 A1)
                bool isMfm = false;
                if (idamOffset >= 3 && 
                    trackData[idamOffset - 3] == 0xA1 &&
                    trackData[idamOffset - 2] == 0xA1 &&
                    trackData[idamOffset - 1] == 0xA1) {
                    isMfm = true;
                }
                
                sector.fmEncoding = !isMfm;

                // Read sector ID field
                if (idamOffset + 6 < result.trackLength) {
                    uint8_t mark = trackData[idamOffset];
                    if (mark == UFT_DMK_MFM_IDAM || mark == UFT_DMK_FM_IDAM) {
                        sector.cylinder = trackData[idamOffset + 1];
                        sector.head = trackData[idamOffset + 2];
                        sector.sector = trackData[idamOffset + 3];
                        sector.sizeCode = trackData[idamOffset + 4];
                        
                        // Calculate data size
                        if (isMfm || (sector.sizeCode <= 3)) {
                            sector.dataSize = 128 << (sector.sizeCode & 0x3);
                        } else {
                            // WD1771 non-IBM mode
                            sector.dataSize = 16 * (sector.sizeCode ? sector.sizeCode : 256);
                        }

                        // CRC from ID field
                        uint16_t idCrc = (trackData[idamOffset + 5] << 8) | 
                                         trackData[idamOffset + 6];

                        // Look for Data Address Mark
                        int searchStart = idamOffset + 7;
                        int searchEnd = qMin(searchStart + 50, result.trackLength - 1);
                        sector.deleted = false;
                        sector.crcOk = false;
                        sector.dataOffset = 0;

                        for (int s = searchStart; s < searchEnd; s++) {
                            uint8_t b = trackData[s];
                            if (b == UFT_DMK_MFM_DAM || b == UFT_DMK_FM_DAM) {
                                sector.dataOffset = s + 1;
                                break;
                            } else if (b == UFT_DMK_MFM_DDAM || b == UFT_DMK_FM_DDAM) {
                                sector.dataOffset = s + 1;
                                sector.deleted = true;
                                break;
                            }
                        }

                        if (sector.dataOffset > 0 && 
                            sector.dataOffset + sector.dataSize + 2 <= result.trackLength) {
                            // Read sector data
                            sector.data = QByteArray(
                                reinterpret_cast<const char*>(trackData + sector.dataOffset),
                                sector.dataSize);

                            // Read CRC
                            int crcOff = sector.dataOffset + sector.dataSize;
                            sector.actualCrc = (trackData[crcOff] << 8) | trackData[crcOff + 1];

                            // Compute CRC (simplified - use initial value for MFM A1A1A1)
                            uint16_t crc = isMfm ? UFT_DMK_CRC_A1A1A1 : 0xFFFF;
                            // Add DAM byte to CRC
                            uint8_t damByte = sector.deleted ? UFT_DMK_MFM_DDAM : UFT_DMK_MFM_DAM;
                            crc = uft_dmk_crc16(&damByte, 1, crc);
                            crc = uft_dmk_crc16(
                                reinterpret_cast<const uint8_t*>(sector.data.constData()),
                                sector.dataSize, crc);
                            sector.computedCrc = crc;
                            sector.crcOk = (sector.actualCrc == sector.computedCrc);
                        }

                        // Update statistics
                        trackInfo.sectors.append(sector);
                        trackInfo.numSectors++;
                        result.totalSectors++;
                        
                        if (!sector.crcOk) {
                            result.errorSectors++;
                            trackInfo.hasErrors = true;
                        }
                        if (sector.deleted) result.deletedSectors++;
                        if (sector.fmEncoding) result.fmSectors++;
                        else result.mfmSectors++;
                    }
                }
            }

            result.trackList.append(trackInfo);
            offset += result.trackLength;
        }
    }

    result.valid = !m_stopRequested;
    return result;
}

bool UftDmkAnalyzerWorker::performExport()
{
    // First analyze to get sector data
    DmkAnalysisResult analysis = performAnalysis();
    if (!analysis.valid) {
        emit exportError(analysis.errorMessage);
        return false;
    }

    // Calculate output size based on geometry
    // Assume standard sector interleave
    int sectorsPerTrack = 0;
    int sectorSize = 0;
    
    if (!analysis.trackList.isEmpty() && !analysis.trackList[0].sectors.isEmpty()) {
        sectorsPerTrack = analysis.trackList[0].numSectors;
        sectorSize = analysis.trackList[0].sectors[0].dataSize;
    }
    
    if (sectorsPerTrack == 0 || sectorSize == 0) {
        emit exportError("Cannot determine disk geometry");
        return false;
    }

    qint64 totalSize = (qint64)analysis.tracks * analysis.heads * sectorsPerTrack * sectorSize;

    QFile outFile(m_exportPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        emit exportError(QString("Cannot create output file: %1").arg(outFile.errorString()));
        return false;
    }

    // Create fill buffer
    QByteArray fillSector(sectorSize, static_cast<char>(m_fillByte));

    for (const DmkTrackInfo &track : analysis.trackList) {
        if (m_stopRequested) {
            outFile.close();
            QFile::remove(m_exportPath);
            emit exportError("Export cancelled");
            return false;
        }

        // Write sectors in logical order
        for (int s = 1; s <= sectorsPerTrack; s++) {
            const DmkSectorInfo *found = nullptr;
            for (const DmkSectorInfo &sector : track.sectors) {
                if (sector.sector == s) {
                    found = &sector;
                    break;
                }
            }

            if (found && found->crcOk) {
                outFile.write(found->data);
            } else {
                outFile.write(fillSector);
            }
        }
    }

    outFile.close();
    return true;
}

/*============================================================================
 * Panel Implementation
 *============================================================================*/

UftDmkAnalyzerPanel::UftDmkAnalyzerPanel(QWidget *parent)
    : QWidget(parent)
    , m_worker(new UftDmkAnalyzerWorker(this))
{
    setupUi();

    // Connect worker signals
    connect(m_worker, &UftDmkAnalyzerWorker::analysisProgress,
            this, [this](int current, int total) {
                m_progressBar->setMaximum(total);
                m_progressBar->setValue(current);
            });
    connect(m_worker, &UftDmkAnalyzerWorker::analysisComplete,
            this, &UftDmkAnalyzerPanel::onAnalysisComplete);
    connect(m_worker, &UftDmkAnalyzerWorker::analysisError,
            this, &UftDmkAnalyzerPanel::onAnalysisError);
    connect(m_worker, &UftDmkAnalyzerWorker::exportComplete,
            this, &UftDmkAnalyzerPanel::onExportComplete);
    connect(m_worker, &UftDmkAnalyzerWorker::exportError,
            this, &UftDmkAnalyzerPanel::onExportError);
}

UftDmkAnalyzerPanel::~UftDmkAnalyzerPanel()
{
    m_worker->requestStop();
    m_worker->wait();
}

void UftDmkAnalyzerPanel::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // File selection group
    m_fileGroup = new QGroupBox(tr("DMK File"), this);
    QHBoxLayout *fileLayout = new QHBoxLayout(m_fileGroup);
    m_filePathEdit = new QLineEdit(this);
    m_filePathEdit->setPlaceholderText(tr("Select DMK file to analyze..."));
    m_browseBtn = new QPushButton(tr("Browse..."), this);
    m_analyzeBtn = new QPushButton(tr("Analyze"), this);
    m_analyzeBtn->setEnabled(false);
    fileLayout->addWidget(m_filePathEdit, 1);
    fileLayout->addWidget(m_browseBtn);
    fileLayout->addWidget(m_analyzeBtn);
    mainLayout->addWidget(m_fileGroup);

    connect(m_browseBtn, &QPushButton::clicked, this, &UftDmkAnalyzerPanel::openFile);
    connect(m_analyzeBtn, &QPushButton::clicked, this, &UftDmkAnalyzerPanel::analyzeFile);
    connect(m_filePathEdit, &QLineEdit::textChanged, [this](const QString &text) {
        m_analyzeBtn->setEnabled(!text.isEmpty());
    });

    // Main splitter
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Left panel: Overview + Track tree
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);

    // Overview group
    m_overviewGroup = new QGroupBox(tr("Overview"), this);
    QGridLayout *overviewLayout = new QGridLayout(m_overviewGroup);
    
    int row = 0;
    overviewLayout->addWidget(new QLabel(tr("Filename:"), this), row, 0);
    m_filenameLabel = new QLabel("-", this);
    m_filenameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    overviewLayout->addWidget(m_filenameLabel, row++, 1);

    overviewLayout->addWidget(new QLabel(tr("Tracks:"), this), row, 0);
    m_tracksLabel = new QLabel("-", this);
    overviewLayout->addWidget(m_tracksLabel, row++, 1);

    overviewLayout->addWidget(new QLabel(tr("Heads:"), this), row, 0);
    m_headsLabel = new QLabel("-", this);
    overviewLayout->addWidget(m_headsLabel, row++, 1);

    overviewLayout->addWidget(new QLabel(tr("Track Length:"), this), row, 0);
    m_trackLengthLabel = new QLabel("-", this);
    overviewLayout->addWidget(m_trackLengthLabel, row++, 1);

    overviewLayout->addWidget(new QLabel(tr("Density:"), this), row, 0);
    m_densityLabel = new QLabel("-", this);
    overviewLayout->addWidget(m_densityLabel, row++, 1);

    overviewLayout->addWidget(new QLabel(tr("Write Protect:"), this), row, 0);
    m_writeProtectLabel = new QLabel("-", this);
    overviewLayout->addWidget(m_writeProtectLabel, row++, 1);

    overviewLayout->addWidget(new QLabel(tr("Total Sectors:"), this), row, 0);
    m_totalSectorsLabel = new QLabel("-", this);
    overviewLayout->addWidget(m_totalSectorsLabel, row++, 1);

    overviewLayout->addWidget(new QLabel(tr("Error Sectors:"), this), row, 0);
    m_errorSectorsLabel = new QLabel("-", this);
    overviewLayout->addWidget(m_errorSectorsLabel, row++, 1);

    overviewLayout->addWidget(new QLabel(tr("Deleted Sectors:"), this), row, 0);
    m_deletedSectorsLabel = new QLabel("-", this);
    overviewLayout->addWidget(m_deletedSectorsLabel, row++, 1);

    overviewLayout->addWidget(new QLabel(tr("FM / MFM:"), this), row, 0);
    m_fmSectorsLabel = new QLabel("-", this);
    overviewLayout->addWidget(m_fmSectorsLabel, row++, 1);

    leftLayout->addWidget(m_overviewGroup);

    // Track tree
    m_trackTree = new QTreeWidget(this);
    m_trackTree->setHeaderLabels({tr("Track"), tr("Sectors"), tr("Errors")});
    m_trackTree->setColumnWidth(0, 100);
    m_trackTree->setColumnWidth(1, 60);
    m_trackTree->setColumnWidth(2, 60);
    leftLayout->addWidget(m_trackTree, 1);

    connect(m_trackTree, &QTreeWidget::itemClicked,
            this, &UftDmkAnalyzerPanel::onTrackSelected);

    m_mainSplitter->addWidget(leftPanel);

    // Right panel: Tabs for sectors and hex
    m_tabWidget = new QTabWidget(this);

    // Sector table tab
    QWidget *sectorTab = new QWidget(this);
    QVBoxLayout *sectorLayout = new QVBoxLayout(sectorTab);

    QHBoxLayout *filterLayout = new QHBoxLayout();
    m_showAllCheck = new QCheckBox(tr("Show all sectors"), this);
    m_showAllCheck->setChecked(true);
    m_showErrorsCheck = new QCheckBox(tr("Highlight errors"), this);
    m_showErrorsCheck->setChecked(true);
    filterLayout->addWidget(m_showAllCheck);
    filterLayout->addWidget(m_showErrorsCheck);
    filterLayout->addStretch();
    sectorLayout->addLayout(filterLayout);

    m_sectorTable = new QTableWidget(this);
    m_sectorTable->setColumnCount(9);
    m_sectorTable->setHorizontalHeaderLabels({
        tr("Cyl"), tr("Head"), tr("Sec"), tr("Size"), 
        tr("Encoding"), tr("Deleted"), tr("CRC"), tr("Actual"), tr("Computed")
    });
    m_sectorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sectorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sectorTable->horizontalHeader()->setStretchLastSection(true);
    sectorLayout->addWidget(m_sectorTable, 1);

    connect(m_sectorTable, &QTableWidget::cellDoubleClicked,
            this, &UftDmkAnalyzerPanel::onSectorDoubleClicked);

    m_tabWidget->addTab(sectorTab, tr("Sectors"));

    // Hex dump tab
    QWidget *hexTab = new QWidget(this);
    QVBoxLayout *hexLayout = new QVBoxLayout(hexTab);

    m_hexInfoLabel = new QLabel(tr("Select a sector to view data"), this);
    hexLayout->addWidget(m_hexInfoLabel);

    m_hexView = new QPlainTextEdit(this);
    m_hexView->setReadOnly(true);
    m_hexView->setFont(QFont("Courier New", 9));
    m_hexView->setLineWrapMode(QPlainTextEdit::NoWrap);
    hexLayout->addWidget(m_hexView, 1);

    QHBoxLayout *hexButtonLayout = new QHBoxLayout();
    m_copyHexBtn = new QPushButton(tr("Copy to Clipboard"), this);
    hexButtonLayout->addStretch();
    hexButtonLayout->addWidget(m_copyHexBtn);
    hexLayout->addLayout(hexButtonLayout);

    connect(m_copyHexBtn, &QPushButton::clicked, this, &UftDmkAnalyzerPanel::copyToClipboard);

    m_tabWidget->addTab(hexTab, tr("Hex Dump"));

    // Export tab
    QWidget *exportTab = new QWidget(this);
    QVBoxLayout *exportLayout = new QVBoxLayout(exportTab);

    m_exportGroup = new QGroupBox(tr("Export to Raw Binary"), this);
    QGridLayout *exportGridLayout = new QGridLayout(m_exportGroup);

    exportGridLayout->addWidget(new QLabel(tr("Output File:"), this), 0, 0);
    m_exportPathEdit = new QLineEdit(this);
    m_exportBrowseBtn = new QPushButton(tr("Browse..."), this);
    exportGridLayout->addWidget(m_exportPathEdit, 0, 1);
    exportGridLayout->addWidget(m_exportBrowseBtn, 0, 2);

    exportGridLayout->addWidget(new QLabel(tr("Fill Byte:"), this), 1, 0);
    m_fillByteSpin = new QSpinBox(this);
    m_fillByteSpin->setRange(0, 255);
    m_fillByteSpin->setValue(0xE5);
    m_fillByteSpin->setDisplayIntegerBase(16);
    m_fillByteSpin->setPrefix("0x");
    exportGridLayout->addWidget(m_fillByteSpin, 1, 1);

    m_exportBtn = new QPushButton(tr("Export"), this);
    m_exportBtn->setEnabled(false);
    exportGridLayout->addWidget(m_exportBtn, 2, 1);

    exportLayout->addWidget(m_exportGroup);
    exportLayout->addStretch();

    connect(m_exportBrowseBtn, &QPushButton::clicked, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("Export Raw Image"),
            QString(), tr("Raw Images (*.img *.bin);;All Files (*)"));
        if (!path.isEmpty()) {
            m_exportPathEdit->setText(path);
        }
    });
    connect(m_exportBtn, &QPushButton::clicked, this, &UftDmkAnalyzerPanel::exportToRaw);

    m_tabWidget->addTab(exportTab, tr("Export"));

    m_mainSplitter->addWidget(m_tabWidget);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 2);

    mainLayout->addWidget(m_mainSplitter, 1);

    // Progress and status
    QHBoxLayout *statusLayout = new QHBoxLayout();
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_statusLabel = new QLabel(tr("Ready"), this);
    statusLayout->addWidget(m_progressBar);
    statusLayout->addWidget(m_statusLabel, 1);
    mainLayout->addLayout(statusLayout);

    // Log
    m_logText = new QTextEdit(this);
    m_logText->setReadOnly(true);
    m_logText->setMaximumHeight(80);
    m_logText->setFont(QFont("Courier New", 8));
    mainLayout->addWidget(m_logText);
}

void UftDmkAnalyzerPanel::setFile(const QString &path)
{
    m_filePathEdit->setText(path);
    m_currentFile = path;
}

void UftDmkAnalyzerPanel::openFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open DMK File"),
        QString(), tr("DMK Files (*.dmk);;All Files (*)"));
    if (!path.isEmpty()) {
        setFile(path);
        analyzeFile();
    }
}

void UftDmkAnalyzerPanel::analyzeFile()
{
    QString path = m_filePathEdit->text();
    if (path.isEmpty()) return;

    m_currentFile = path;
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_statusLabel->setText(tr("Analyzing..."));
    m_analyzeBtn->setEnabled(false);

    m_worker->setFile(path);
    m_worker->analyzeFile();

    addLogMessage(QString("Analyzing: %1").arg(path));
}

void UftDmkAnalyzerPanel::exportToRaw()
{
    QString path = m_exportPathEdit->text();
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("Export"), tr("Please specify output file"));
        return;
    }

    m_worker->setExportPath(path);
    m_worker->setExportFillByte(static_cast<uint8_t>(m_fillByteSpin->value()));
    m_worker->exportToRaw();

    addLogMessage(QString("Exporting to: %1").arg(path));
}

void UftDmkAnalyzerPanel::copyToClipboard()
{
    QApplication::clipboard()->setText(m_hexView->toPlainText());
    m_statusLabel->setText(tr("Copied to clipboard"));
}

void UftDmkAnalyzerPanel::onAnalysisComplete(const DmkAnalysisResult &result)
{
    m_progressBar->setVisible(false);
    m_analyzeBtn->setEnabled(true);
    m_currentResult = result;

    updateDisplay(result);

    QString msg = QString("Analysis complete: %1 tracks, %2 sectors (%3 errors)")
                  .arg(result.tracks * result.heads)
                  .arg(result.totalSectors)
                  .arg(result.errorSectors);
    m_statusLabel->setText(msg);
    addLogMessage(msg);

    m_exportBtn->setEnabled(true);
    emit fileLoaded(result.filename);
}

void UftDmkAnalyzerPanel::onAnalysisError(const QString &error)
{
    m_progressBar->setVisible(false);
    m_analyzeBtn->setEnabled(true);
    m_statusLabel->setText(tr("Error"));
    addLogMessage(QString("ERROR: %1").arg(error), true);
    QMessageBox::warning(this, tr("Analysis Error"), error);
}

void UftDmkAnalyzerPanel::onExportComplete(const QString &path, qint64 size)
{
    QString msg = QString("Exported %1 bytes to %2").arg(size).arg(path);
    m_statusLabel->setText(msg);
    addLogMessage(msg);
}

void UftDmkAnalyzerPanel::onExportError(const QString &error)
{
    addLogMessage(QString("Export ERROR: %1").arg(error), true);
    QMessageBox::warning(this, tr("Export Error"), error);
}

void UftDmkAnalyzerPanel::updateDisplay(const DmkAnalysisResult &result)
{
    // Update overview
    QFileInfo fi(result.filename);
    m_filenameLabel->setText(fi.fileName());
    m_tracksLabel->setText(QString::number(result.tracks));
    m_headsLabel->setText(result.singleSided ? "1 (Single-sided)" : "2 (Double-sided)");
    m_trackLengthLabel->setText(QString("%1 bytes (0x%2)")
                                .arg(result.trackLength)
                                .arg(result.trackLength, 4, 16, QChar('0')));
    m_densityLabel->setText(result.singleDensity ? "Single (FM)" : "Double (MFM)");
    m_writeProtectLabel->setText(result.writeProtected ? "Yes" : "No");
    m_totalSectorsLabel->setText(QString::number(result.totalSectors));
    
    QString errorStyle = result.errorSectors > 0 ? "color: red; font-weight: bold;" : "";
    m_errorSectorsLabel->setText(QString::number(result.errorSectors));
    m_errorSectorsLabel->setStyleSheet(errorStyle);
    
    m_deletedSectorsLabel->setText(QString::number(result.deletedSectors));
    m_fmSectorsLabel->setText(QString("%1 / %2").arg(result.fmSectors).arg(result.mfmSectors));

    populateTrackTree(result);
}

void UftDmkAnalyzerPanel::populateTrackTree(const DmkAnalysisResult &result)
{
    m_trackTree->clear();

    for (const DmkTrackInfo &track : result.trackList) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, QString("T%1.%2").arg(track.cylinder).arg(track.head));
        item->setText(1, QString::number(track.numSectors));
        item->setText(2, track.hasErrors ? "Yes" : "");
        
        if (track.hasErrors) {
            item->setForeground(2, Qt::red);
            item->setBackground(0, QColor(255, 240, 240));
        }

        item->setData(0, Qt::UserRole, track.cylinder);
        item->setData(0, Qt::UserRole + 1, track.head);

        m_trackTree->addTopLevelItem(item);
    }
}

void UftDmkAnalyzerPanel::onTrackSelected(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    
    int cyl = item->data(0, Qt::UserRole).toInt();
    int head = item->data(0, Qt::UserRole + 1).toInt();

    // Find track in results
    for (const DmkTrackInfo &track : m_currentResult.trackList) {
        if (track.cylinder == cyl && track.head == head) {
            populateSectorTable(track);
            break;
        }
    }
}

void UftDmkAnalyzerPanel::populateSectorTable(const DmkTrackInfo &track)
{
    m_sectorTable->setRowCount(0);
    m_sectorTable->setRowCount(track.sectors.count());

    int row = 0;
    for (const DmkSectorInfo &sector : track.sectors) {
        m_sectorTable->setItem(row, 0, new QTableWidgetItem(QString::number(sector.cylinder)));
        m_sectorTable->setItem(row, 1, new QTableWidgetItem(QString::number(sector.head)));
        m_sectorTable->setItem(row, 2, new QTableWidgetItem(QString::number(sector.sector)));
        m_sectorTable->setItem(row, 3, new QTableWidgetItem(
            QString("%1 (%2)").arg(sector.sizeCode).arg(sector.dataSize)));
        m_sectorTable->setItem(row, 4, new QTableWidgetItem(
            sector.fmEncoding ? "FM" : "MFM"));
        m_sectorTable->setItem(row, 5, new QTableWidgetItem(
            sector.deleted ? "Yes" : ""));
        m_sectorTable->setItem(row, 6, new QTableWidgetItem(
            sector.crcOk ? "OK" : "ERROR"));
        m_sectorTable->setItem(row, 7, new QTableWidgetItem(
            QString("%1").arg(sector.actualCrc, 4, 16, QChar('0')).toUpper()));
        m_sectorTable->setItem(row, 8, new QTableWidgetItem(
            QString("%1").arg(sector.computedCrc, 4, 16, QChar('0')).toUpper()));

        // Highlight errors
        if (!sector.crcOk && m_showErrorsCheck->isChecked()) {
            for (int c = 0; c < m_sectorTable->columnCount(); c++) {
                QTableWidgetItem *item = m_sectorTable->item(row, c);
                if (item) {
                    item->setBackground(QColor(255, 200, 200));
                }
            }
        }

        // Store sector data for hex view
        m_sectorTable->item(row, 0)->setData(Qt::UserRole, 
            QVariant::fromValue(sector.data));

        row++;
    }

    m_sectorTable->resizeColumnsToContents();
}

void UftDmkAnalyzerPanel::onSectorDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    QTableWidgetItem *item = m_sectorTable->item(row, 0);
    if (!item) return;

    QByteArray data = item->data(Qt::UserRole).toByteArray();
    if (!data.isEmpty()) {
        int cyl = m_sectorTable->item(row, 0)->text().toInt();
        int head = m_sectorTable->item(row, 1)->text().toInt();
        int sec = m_sectorTable->item(row, 2)->text().toInt();

        m_hexInfoLabel->setText(QString("Sector C=%1 H=%2 S=%3 (%4 bytes)")
                                .arg(cyl).arg(head).arg(sec).arg(data.size()));
        showHexDump(data);
        m_tabWidget->setCurrentIndex(1); // Switch to hex tab
    }
}

void UftDmkAnalyzerPanel::showHexDump(const QByteArray &data)
{
    QString hex;
    const int bytesPerLine = 16;

    for (int i = 0; i < data.size(); i += bytesPerLine) {
        // Address
        hex += QString("%1  ").arg(i, 4, 16, QChar('0')).toUpper();

        // Hex bytes
        QString hexPart;
        QString asciiPart;
        for (int j = 0; j < bytesPerLine; j++) {
            if (i + j < data.size()) {
                uint8_t b = static_cast<uint8_t>(data[i + j]);
                hexPart += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
                asciiPart += (b >= 32 && b < 127) ? QChar(b) : '.';
            } else {
                hexPart += "   ";
                asciiPart += " ";
            }
            if (j == 7) hexPart += " ";
        }

        hex += hexPart + " " + asciiPart + "\n";
    }

    m_hexView->setPlainText(hex);
}

void UftDmkAnalyzerPanel::showSectorData(int track, int head, int sector)
{
    // Find sector in current result
    for (const DmkTrackInfo &t : m_currentResult.trackList) {
        if (t.cylinder == track && t.head == head) {
            for (const DmkSectorInfo &s : t.sectors) {
                if (s.sector == sector) {
                    m_hexInfoLabel->setText(QString("Sector C=%1 H=%2 S=%3 (%4 bytes)")
                                            .arg(track).arg(head).arg(sector).arg(s.data.size()));
                    showHexDump(s.data);
                    m_tabWidget->setCurrentIndex(1);
                    return;
                }
            }
        }
    }
}

void UftDmkAnalyzerPanel::onShowAllSectors(bool checked)
{
    Q_UNUSED(checked);
    // Refilter current track if any
}

void UftDmkAnalyzerPanel::onShowErrorsOnly(bool checked)
{
    Q_UNUSED(checked);
    // Refilter current track if any
}

void UftDmkAnalyzerPanel::addLogMessage(const QString &msg, bool isError)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString line = QString("[%1] %2").arg(timestamp).arg(msg);
    
    if (isError) {
        m_logText->append(QString("<span style='color:red;'>%1</span>").arg(line));
    } else {
        m_logText->append(line);
    }
}

/**
 * @file uft_xcopy_panel.cpp
 * @brief XCopy Panel Implementation
 */

#include "uft_xcopy_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QFileDialog>

UftXCopyPanel::UftXCopyPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void UftXCopyPanel::setupUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    /* Left column */
    QVBoxLayout *leftCol = new QVBoxLayout();
    createSourceGroup();
    createDestGroup();
    createRangeGroup();
    leftCol->addWidget(m_sourceGroup);
    leftCol->addWidget(m_destGroup);
    leftCol->addWidget(m_rangeGroup);
    leftCol->addStretch();
    
    /* Right column */
    QVBoxLayout *rightCol = new QVBoxLayout();
    createModeGroup();
    createAnalysisGroup();  /* NEW: Analysis integration */
    createErrorGroup();
    createSpeedGroup();
    createMultipleGroup();
    createProgressGroup();
    rightCol->addWidget(m_modeGroup);
    rightCol->addWidget(m_analysisGroup);  /* NEW */
    rightCol->addWidget(m_errorGroup);
    rightCol->addWidget(m_speedGroup);
    rightCol->addWidget(m_multipleGroup);
    rightCol->addWidget(m_progressGroup);
    rightCol->addStretch();
    
    mainLayout->addLayout(leftCol);
    mainLayout->addLayout(rightCol);
    
    /* Initialize analysis state */
    m_hasAnalysis = false;
    m_protectedTracks = 0;
    m_recommendedCopyMode = CopyModeRecommendation::Normal;
}

void UftXCopyPanel::createSourceGroup()
{
    m_sourceGroup = new QGroupBox("Source", this);
    QFormLayout *layout = new QFormLayout(m_sourceGroup);
    
    m_sourceIsDrive = new QRadioButton("Drive");
    m_sourceIsImage = new QRadioButton("Image File");
    m_sourceIsDrive->setChecked(true);
    
    QHBoxLayout *radioLayout = new QHBoxLayout();
    radioLayout->addWidget(m_sourceIsDrive);
    radioLayout->addWidget(m_sourceIsImage);
    layout->addRow(radioLayout);
    
    m_sourceDrive = new QComboBox();
    m_sourceDrive->addItem("Drive 0 (A:)");
    m_sourceDrive->addItem("Drive 1 (B:)");
    layout->addRow("Drive:", m_sourceDrive);
    
    m_sourcePath = new QLineEdit();
    m_browseSource = new QPushButton("...");
    m_browseSource->setMaximumWidth(30);
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathLayout->addWidget(m_sourcePath);
    pathLayout->addWidget(m_browseSource);
    layout->addRow("File:", pathLayout);
    
    connect(m_browseSource, &QPushButton::clicked, this, &UftXCopyPanel::selectSource);
}

void UftXCopyPanel::createDestGroup()
{
    m_destGroup = new QGroupBox("Destination", this);
    QFormLayout *layout = new QFormLayout(m_destGroup);
    
    m_destIsDrive = new QRadioButton("Drive");
    m_destIsImage = new QRadioButton("Image File");
    m_destIsImage->setChecked(true);
    
    QHBoxLayout *radioLayout = new QHBoxLayout();
    radioLayout->addWidget(m_destIsDrive);
    radioLayout->addWidget(m_destIsImage);
    layout->addRow(radioLayout);
    
    m_destDrive = new QComboBox();
    m_destDrive->addItem("Drive 0 (A:)");
    m_destDrive->addItem("Drive 1 (B:)");
    layout->addRow("Drive:", m_destDrive);
    
    m_destPath = new QLineEdit();
    m_browseDest = new QPushButton("...");
    m_browseDest->setMaximumWidth(30);
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathLayout->addWidget(m_destPath);
    pathLayout->addWidget(m_browseDest);
    layout->addRow("File:", pathLayout);
    
    connect(m_browseDest, &QPushButton::clicked, this, &UftXCopyPanel::selectDest);
}

void UftXCopyPanel::createRangeGroup()
{
    m_rangeGroup = new QGroupBox("Track Range", this);
    QFormLayout *layout = new QFormLayout(m_rangeGroup);
    
    m_startTrack = new QSpinBox();
    m_startTrack->setRange(0, 255);
    m_startTrack->setValue(0);
    layout->addRow("Start Track:", m_startTrack);
    
    m_endTrack = new QSpinBox();
    m_endTrack->setRange(0, 255);
    m_endTrack->setValue(79);
    layout->addRow("End Track:", m_endTrack);
    
    m_sides = new QComboBox();
    m_sides->addItem("Side 0 only", 0);
    m_sides->addItem("Side 1 only", 1);
    m_sides->addItem("Both sides", 2);
    m_sides->setCurrentIndex(2);
    layout->addRow("Sides:", m_sides);
    
    m_allTracks = new QCheckBox("All tracks");
    m_allTracks->setChecked(true);
    layout->addRow(m_allTracks);
}

void UftXCopyPanel::createModeGroup()
{
    m_modeGroup = new QGroupBox("Copy Mode", this);
    QFormLayout *layout = new QFormLayout(m_modeGroup);
    
    m_copyMode = new QComboBox();
    m_copyMode->addItem("Normal (Sector)", 0);
    m_copyMode->addItem("Track Copy", 1);
    m_copyMode->addItem("Flux Copy", 2);
    m_copyMode->addItem("Nibble Copy", 3);
    m_copyMode->addItem("Auto (from Analysis)", 4);   /* NEW */
    m_copyMode->addItem("Per-Track Mixed", 5);        /* NEW */
    layout->addRow("Mode:", m_copyMode);
    
    m_verifyWrite = new QCheckBox("Verify after write");
    m_verifyWrite->setChecked(true);
    layout->addRow(m_verifyWrite);
    
    m_verifyRetries = new QSpinBox();
    m_verifyRetries->setRange(0, 10);
    m_verifyRetries->setValue(3);
    layout->addRow("Verify retries:", m_verifyRetries);
    
    /* Connect mode change signal */
    connect(m_copyMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftXCopyPanel::onCopyModeChanged);
}

void UftXCopyPanel::createErrorGroup()
{
    m_errorGroup = new QGroupBox("Error Handling", this);
    QFormLayout *layout = new QFormLayout(m_errorGroup);
    
    m_ignoreErrors = new QCheckBox("Ignore errors");
    layout->addRow(m_ignoreErrors);
    
    m_retryErrors = new QCheckBox("Retry on error");
    m_retryErrors->setChecked(true);
    layout->addRow(m_retryErrors);
    
    m_maxRetries = new QSpinBox();
    m_maxRetries->setRange(0, 50);
    m_maxRetries->setValue(5);
    layout->addRow("Max retries:", m_maxRetries);
    
    m_skipBadSectors = new QCheckBox("Skip bad sectors");
    m_skipBadSectors->setChecked(true);
    layout->addRow(m_skipBadSectors);
    
    m_fillBadSectors = new QCheckBox("Fill bad sectors");
    layout->addRow(m_fillBadSectors);
    
    m_fillByte = new QSpinBox();
    m_fillByte->setRange(0, 255);
    m_fillByte->setValue(0);
    m_fillByte->setDisplayIntegerBase(16);
    m_fillByte->setPrefix("0x");
    layout->addRow("Fill byte:", m_fillByte);
}

void UftXCopyPanel::createSpeedGroup()
{
    m_speedGroup = new QGroupBox("Speed", this);
    QFormLayout *layout = new QFormLayout(m_speedGroup);
    
    m_readSpeed = new QComboBox();
    m_readSpeed->addItem("Normal");
    m_readSpeed->addItem("Fast");
    m_readSpeed->addItem("Maximum");
    layout->addRow("Read speed:", m_readSpeed);
    
    m_writeSpeed = new QComboBox();
    m_writeSpeed->addItem("Normal");
    m_writeSpeed->addItem("Fast");
    m_writeSpeed->addItem("Maximum");
    layout->addRow("Write speed:", m_writeSpeed);
    
    m_bufferDisk = new QCheckBox("Buffer entire disk");
    layout->addRow(m_bufferDisk);
}

void UftXCopyPanel::createMultipleGroup()
{
    m_multipleGroup = new QGroupBox("Multiple Copies", this);
    QFormLayout *layout = new QFormLayout(m_multipleGroup);
    
    m_numCopies = new QSpinBox();
    m_numCopies->setRange(1, 100);
    m_numCopies->setValue(1);
    layout->addRow("Number of copies:", m_numCopies);
    
    m_autoEject = new QCheckBox("Auto eject");
    layout->addRow(m_autoEject);
    
    m_waitForDisk = new QCheckBox("Wait for disk");
    m_waitForDisk->setChecked(true);
    layout->addRow(m_waitForDisk);
}

void UftXCopyPanel::createAnalysisGroup()
{
    m_analysisGroup = new QGroupBox("Track Analysis", this);
    QVBoxLayout *layout = new QVBoxLayout(m_analysisGroup);
    
    /* Analysis status */
    m_analysisStatus = new QLabel("Not analyzed");
    m_analysisStatus->setStyleSheet("font-style: italic; color: gray;");
    layout->addWidget(m_analysisStatus);
    
    /* Protection info */
    m_protectionInfo = new QLabel();
    m_protectionInfo->setWordWrap(true);
    m_protectionInfo->hide();
    layout->addWidget(m_protectionInfo);
    
    /* Recommended mode */
    m_recommendedMode = new QLabel();
    m_recommendedMode->setStyleSheet(
        "background: #E8F5E9; padding: 6px; border-radius: 4px; font-weight: bold;");
    m_recommendedMode->setAlignment(Qt::AlignCenter);
    m_recommendedMode->hide();
    layout->addWidget(m_recommendedMode);
    
    /* Mixed mode preview */
    m_mixedModePreview = new QLabel();
    m_mixedModePreview->setStyleSheet(
        "background: #FFF8E1; padding: 6px; border-radius: 4px; font-size: 9pt;");
    m_mixedModePreview->setWordWrap(true);
    m_mixedModePreview->hide();
    layout->addWidget(m_mixedModePreview);
    
    /* Buttons */
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_quickScanBtn = new QPushButton("🔍 Quick Scan");
    m_quickScanBtn->setToolTip("Analyze first tracks to detect platform and protection");
    m_analyzeBtn = new QPushButton("📊 Full Analysis");
    m_analyzeBtn->setToolTip("Analyze all tracks in detail");
    
    btnLayout->addWidget(m_quickScanBtn);
    btnLayout->addWidget(m_analyzeBtn);
    layout->addLayout(btnLayout);
    
    connect(m_quickScanBtn, &QPushButton::clicked, this, &UftXCopyPanel::requestQuickScan);
    connect(m_analyzeBtn, &QPushButton::clicked, this, &UftXCopyPanel::onAnalyzeClicked);
}

void UftXCopyPanel::onCopyModeChanged(int index)
{
    /* Show/hide mixed mode preview */
    bool isMixed = (index == 5);
    m_mixedModePreview->setVisible(isMixed && m_hasAnalysis);
    
    /* If Auto mode selected but no analysis, trigger quick scan */
    if (index == 4 && !m_hasAnalysis) {
        emit requestQuickScan();
    }
    
    emit paramsChanged();
}

void UftXCopyPanel::onAnalyzeClicked()
{
    emit requestAnalysis();
}

void UftXCopyPanel::applyAnalysisResults(CopyModeRecommendation mode,
                                         const QVector<CopyModeRecommendation> &trackModes)
{
    m_hasAnalysis = true;
    m_recommendedCopyMode = mode;
    m_trackModes.clear();
    
    /* Convert CopyModeRecommendation to int for storage */
    for (const auto &m : trackModes) {
        m_trackModes.append(static_cast<int>(m));
    }
    
    /* Update UI */
    updateAnalysisDisplay();
    
    /* Set copy mode based on recommendation */
    if (mode == CopyModeRecommendation::Mixed) {
        m_copyMode->setCurrentIndex(5);  /* Per-Track Mixed */
        updateMixedModePreview();
    } else {
        m_copyMode->setCurrentIndex(static_cast<int>(mode));
    }
    
    emit paramsChanged();
}

void UftXCopyPanel::setAnalysisInfo(int protectedTracks, const QString &protectionInfo)
{
    m_protectedTracks = protectedTracks;
    m_protectionInfoText = protectionInfo;
    updateAnalysisDisplay();
}

void UftXCopyPanel::clearAnalysis()
{
    m_hasAnalysis = false;
    m_protectedTracks = 0;
    m_protectionInfoText.clear();
    m_trackModes.clear();
    
    m_analysisStatus->setText("Not analyzed");
    m_analysisStatus->setStyleSheet("font-style: italic; color: gray;");
    m_protectionInfo->hide();
    m_recommendedMode->hide();
    m_mixedModePreview->hide();
}

void UftXCopyPanel::updateAnalysisDisplay()
{
    if (!m_hasAnalysis) {
        m_analysisStatus->setText("Not analyzed");
        m_analysisStatus->setStyleSheet("font-style: italic; color: gray;");
        m_protectionInfo->hide();
        m_recommendedMode->hide();
        return;
    }
    
    /* Update status */
    m_analysisStatus->setText("✓ Analysis complete");
    m_analysisStatus->setStyleSheet("font-weight: bold; color: #2E7D32;");
    
    /* Update protection info */
    if (m_protectedTracks > 0) {
        m_protectionInfo->setText(QString("⚠ %1 protected tracks detected\n%2")
            .arg(m_protectedTracks)
            .arg(m_protectionInfoText));
        m_protectionInfo->setStyleSheet("color: #C62828; font-weight: bold;");
        m_protectionInfo->show();
    } else {
        m_protectionInfo->setText("✓ No protection detected");
        m_protectionInfo->setStyleSheet("color: #2E7D32;");
        m_protectionInfo->show();
    }
    
    /* Update recommended mode */
    QString modeName;
    switch (m_recommendedCopyMode) {
        case CopyModeRecommendation::Normal:    modeName = "Normal (Sector)"; break;
        case CopyModeRecommendation::TrackCopy: modeName = "Track Copy"; break;
        case CopyModeRecommendation::FluxCopy:  modeName = "Flux Copy"; break;
        case CopyModeRecommendation::NibbleCopy: modeName = "Nibble Copy"; break;
        case CopyModeRecommendation::Mixed:     modeName = "Per-Track Mixed"; break;
        default: modeName = "Auto"; break;
    }
    
    m_recommendedMode->setText(QString("Recommended: %1").arg(modeName));
    
    if (m_protectedTracks > 0) {
        m_recommendedMode->setStyleSheet(
            "background: #FFEBEE; padding: 6px; border-radius: 4px; "
            "font-weight: bold; color: #C62828;");
    } else {
        m_recommendedMode->setStyleSheet(
            "background: #E8F5E9; padding: 6px; border-radius: 4px; "
            "font-weight: bold; color: #2E7D32;");
    }
    m_recommendedMode->show();
}

void UftXCopyPanel::updateMixedModePreview()
{
    if (m_trackModes.isEmpty()) {
        m_mixedModePreview->hide();
        return;
    }
    
    /* Group consecutive tracks with same mode */
    QString preview;
    int rangeStart = 0;
    int currentMode = m_trackModes[0];
    
    auto getModeShort = [](int m) -> QString {
        switch (m) {
            case 0: return "Normal";
            case 1: return "Track";
            case 2: return "Flux";
            case 3: return "Nibble";
            default: return "?";
        }
    };
    
    for (int i = 1; i <= m_trackModes.size(); i++) {
        if (i == m_trackModes.size() || m_trackModes[i] != currentMode) {
            if (i - 1 == rangeStart) {
                preview += QString("T%1: %2\n")
                    .arg(rangeStart)
                    .arg(getModeShort(currentMode));
            } else {
                preview += QString("T%1-%2: %3\n")
                    .arg(rangeStart)
                    .arg(i - 1)
                    .arg(getModeShort(currentMode));
            }
            
            if (i < m_trackModes.size()) {
                rangeStart = i;
                currentMode = m_trackModes[i];
            }
        }
    }
    
    m_mixedModePreview->setText(preview.trimmed());
    m_mixedModePreview->show();
}

int UftXCopyPanel::getTrackCopyMode(int track, int side) const
{
    int index = track * 2 + side;
    if (index < m_trackModes.size()) {
        return m_trackModes[index];
    }
    return 0;  /* Default: Normal */
}

void UftXCopyPanel::createProgressGroup()
{
    m_progressGroup = new QGroupBox("Progress", this);
    QVBoxLayout *layout = new QVBoxLayout(m_progressGroup);
    
    m_totalProgress = new QProgressBar();
    m_totalProgress->setTextVisible(true);
    layout->addWidget(new QLabel("Total:"));
    layout->addWidget(m_totalProgress);
    
    m_trackProgress = new QProgressBar();
    m_trackProgress->setTextVisible(true);
    layout->addWidget(new QLabel("Track:"));
    layout->addWidget(m_trackProgress);
    
    m_statusLabel = new QLabel("Ready");
    layout->addWidget(m_statusLabel);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("Start Copy");
    m_stopButton = new QPushButton("Stop");
    m_stopButton->setEnabled(false);
    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);
    layout->addLayout(buttonLayout);
    
    connect(m_startButton, &QPushButton::clicked, this, &UftXCopyPanel::startCopy);
    connect(m_stopButton, &QPushButton::clicked, this, &UftXCopyPanel::stopCopy);
}

void UftXCopyPanel::startCopy()
{
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_statusLabel->setText("Copying...");
    emit copyStarted();
}

void UftXCopyPanel::stopCopy()
{
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_statusLabel->setText("Stopped");
}

void UftXCopyPanel::selectSource()
{
    QString path = QFileDialog::getOpenFileName(this, "Select Source Image");
    if (!path.isEmpty()) {
        m_sourcePath->setText(path);
        m_sourceIsImage->setChecked(true);
    }
}

void UftXCopyPanel::selectDest()
{
    QString path = QFileDialog::getSaveFileName(this, "Select Destination");
    if (!path.isEmpty()) {
        m_destPath->setText(path);
        m_destIsImage->setChecked(true);
    }
}

UftXCopyPanel::XCopyParams UftXCopyPanel::getParams() const
{
    XCopyParams params;
    params.source_drive = m_sourceDrive->currentIndex();
    params.dest_drive = m_destDrive->currentIndex();
    params.source_is_image = m_sourceIsImage->isChecked();
    params.dest_is_image = m_destIsImage->isChecked();
    params.source_path = m_sourcePath->text();
    params.dest_path = m_destPath->text();
    params.start_track = m_startTrack->value();
    params.end_track = m_endTrack->value();
    params.sides = m_sides->currentData().toInt();
    params.copy_mode = m_copyMode->currentData().toInt();
    params.verify_after_write = m_verifyWrite->isChecked();
    params.verify_retries = m_verifyRetries->value();
    params.ignore_errors = m_ignoreErrors->isChecked();
    params.retry_on_error = m_retryErrors->isChecked();
    params.max_retries = m_maxRetries->value();
    params.skip_bad_sectors = m_skipBadSectors->isChecked();
    params.fill_bad_sectors = m_fillBadSectors->isChecked();
    params.fill_byte = m_fillByte->value();
    params.num_copies = m_numCopies->value();
    params.auto_eject = m_autoEject->isChecked();
    params.wait_for_disk = m_waitForDisk->isChecked();
    
    /* Analysis data (NEW) */
    params.analysis_available = m_hasAnalysis;
    params.protected_tracks = m_protectedTracks;
    params.protection_info = m_protectionInfoText;
    params.track_modes = m_trackModes;
    
    return params;
}

void UftXCopyPanel::setParams(const XCopyParams &params)
{
    m_sourcePath->setText(params.source_path);
    m_destPath->setText(params.dest_path);
    m_startTrack->setValue(params.start_track);
    m_endTrack->setValue(params.end_track);
    m_verifyWrite->setChecked(params.verify_after_write);
    m_verifyRetries->setValue(params.verify_retries);
    m_ignoreErrors->setChecked(params.ignore_errors);
    m_retryErrors->setChecked(params.retry_on_error);
    m_maxRetries->setValue(params.max_retries);
    m_skipBadSectors->setChecked(params.skip_bad_sectors);
    m_fillBadSectors->setChecked(params.fill_bad_sectors);
    m_fillByte->setValue(params.fill_byte);
    m_numCopies->setValue(params.num_copies);
    m_autoEject->setChecked(params.auto_eject);
    m_waitForDisk->setChecked(params.wait_for_disk);
    
    /* Set copy mode */
    for (int i = 0; i < m_copyMode->count(); i++) {
        if (m_copyMode->itemData(i).toInt() == params.copy_mode) {
            m_copyMode->setCurrentIndex(i);
            break;
        }
    }
    
    /* Restore analysis data (NEW) */
    if (params.analysis_available) {
        m_hasAnalysis = true;
        m_protectedTracks = params.protected_tracks;
        m_protectionInfoText = params.protection_info;
        m_trackModes = params.track_modes;
        updateAnalysisDisplay();
        if (params.copy_mode == 5) {
            updateMixedModePreview();
        }
    }
}

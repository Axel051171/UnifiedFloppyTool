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
    createErrorGroup();
    createSpeedGroup();
    createMultipleGroup();
    createProgressGroup();
    rightCol->addWidget(m_modeGroup);
    rightCol->addWidget(m_errorGroup);
    rightCol->addWidget(m_speedGroup);
    rightCol->addWidget(m_multipleGroup);
    rightCol->addWidget(m_progressGroup);
    rightCol->addStretch();
    
    mainLayout->addLayout(leftCol);
    mainLayout->addLayout(rightCol);
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
    layout->addRow("Mode:", m_copyMode);
    
    m_verifyWrite = new QCheckBox("Verify after write");
    m_verifyWrite->setChecked(true);
    layout->addRow(m_verifyWrite);
    
    m_verifyRetries = new QSpinBox();
    m_verifyRetries->setRange(0, 10);
    m_verifyRetries->setValue(3);
    layout->addRow("Verify retries:", m_verifyRetries);
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
}

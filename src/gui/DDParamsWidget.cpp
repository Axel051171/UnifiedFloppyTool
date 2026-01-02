// SPDX-License-Identifier: MIT
/*
 * DDParamsWidget.cpp - Qt Widget Implementation
 */

#include "DDParamsWidget.h"
#include <QScrollArea>
#include <QThread>

DDParamsWidget::DDParamsWidget(QWidget *parent)
    : QWidget(parent)
    , m_statusTimer(new QTimer(this))
{
    dd_config_init(&m_config);
    setupUI();
    updateWidgetsFromConfig();
    
    connect(m_statusTimer, &QTimer::timeout, this, &DDParamsWidget::updateStatus);
}

DDParamsWidget::~DDParamsWidget()
{
    m_statusTimer->stop();
}

void DDParamsWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Tab widget for parameter groups
    m_tabWidget = new QTabWidget();
    
    // I/O Tab
    QWidget *ioTab = new QWidget();
    QFormLayout *ioLayout = new QFormLayout(ioTab);
    createIOTab(ioLayout);
    m_tabWidget->addTab(ioTab, tr("Input/Output"));
    
    // Block Size Tab
    QWidget *blockTab = new QWidget();
    QFormLayout *blockLayout = new QFormLayout(blockTab);
    createBlockSizeTab(blockLayout);
    m_tabWidget->addTab(blockTab, tr("Block Size"));
    
    // Recovery Tab
    QWidget *recoveryTab = new QWidget();
    QFormLayout *recoveryLayout = new QFormLayout(recoveryTab);
    createRecoveryTab(recoveryLayout);
    m_tabWidget->addTab(recoveryTab, tr("Recovery"));
    
    // Hash Tab
    QWidget *hashTab = new QWidget();
    QFormLayout *hashLayout = new QFormLayout(hashTab);
    createHashTab(hashLayout);
    m_tabWidget->addTab(hashTab, tr("Hashing"));
    
    // Floppy Tab
    QWidget *floppyTab = new QWidget();
    QFormLayout *floppyLayout = new QFormLayout(floppyTab);
    createFloppyTab(floppyLayout);
    m_tabWidget->addTab(floppyTab, tr("Floppy Output"));
    
    mainLayout->addWidget(m_tabWidget);
    
    // Status group at bottom
    createStatusGroup();
    mainLayout->addWidget(m_statusGroup);
}

void DDParamsWidget::createIOTab(QFormLayout *layout)
{
    // Input file
    QHBoxLayout *inputLayout = new QHBoxLayout();
    m_inputFile = new QLineEdit();
    m_inputFile->setPlaceholderText(tr("Select input file or device..."));
    m_browseInput = new QPushButton(tr("Browse..."));
    connect(m_browseInput, &QPushButton::clicked, this, &DDParamsWidget::onBrowseInput);
    inputLayout->addWidget(m_inputFile);
    inputLayout->addWidget(m_browseInput);
    layout->addRow(tr("Input:"), inputLayout);
    
    // Output file
    QHBoxLayout *outputLayout = new QHBoxLayout();
    m_outputFile = new QLineEdit();
    m_outputFile->setPlaceholderText(tr("Select output file (or use Floppy tab)..."));
    m_browseOutput = new QPushButton(tr("Browse..."));
    connect(m_browseOutput, &QPushButton::clicked, this, &DDParamsWidget::onBrowseOutput);
    outputLayout->addWidget(m_outputFile);
    outputLayout->addWidget(m_browseOutput);
    layout->addRow(tr("Output:"), outputLayout);
    
    layout->addRow(new QLabel(tr("<i>Leave output empty if using Floppy Output</i>")));
    
    // Skip bytes
    m_skipBytes = new QSpinBox();
    m_skipBytes->setRange(0, INT_MAX);
    m_skipBytes->setSuffix(tr(" bytes"));
    m_skipBytes->setToolTip(tr("Skip this many bytes at the start of input"));
    connect(m_skipBytes, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DDParamsWidget::onValueChanged);
    layout->addRow(tr("Skip Input:"), m_skipBytes);
    
    // Seek bytes
    m_seekBytes = new QSpinBox();
    m_seekBytes->setRange(0, INT_MAX);
    m_seekBytes->setSuffix(tr(" bytes"));
    m_seekBytes->setToolTip(tr("Seek this many bytes at output before writing"));
    connect(m_seekBytes, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DDParamsWidget::onValueChanged);
    layout->addRow(tr("Seek Output:"), m_seekBytes);
    
    // Max bytes
    QHBoxLayout *maxLayout = new QHBoxLayout();
    m_maxBytes = new QSpinBox();
    m_maxBytes->setRange(0, INT_MAX);
    m_maxBytes->setSpecialValueText(tr("All"));
    m_maxBytes->setToolTip(tr("Maximum bytes to copy (0 = all)"));
    m_maxBytesUnit = new QComboBox();
    m_maxBytesUnit->addItems({"Bytes", "KB", "MB", "GB"});
    m_maxBytesUnit->setCurrentIndex(2);  // MB default
    maxLayout->addWidget(m_maxBytes);
    maxLayout->addWidget(m_maxBytesUnit);
    connect(m_maxBytes, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DDParamsWidget::onValueChanged);
    layout->addRow(tr("Max Size:"), maxLayout);
}

void DDParamsWidget::createBlockSizeTab(QFormLayout *layout)
{
    // Soft block size
    m_softBlockSize = new QComboBox();
    m_softBlockSize->addItem("512 B", 512);
    m_softBlockSize->addItem("1 KB", 1024);
    m_softBlockSize->addItem("4 KB", 4096);
    m_softBlockSize->addItem("8 KB", 8192);
    m_softBlockSize->addItem("16 KB", 16384);
    m_softBlockSize->addItem("32 KB", 32768);
    m_softBlockSize->addItem("64 KB", 65536);
    m_softBlockSize->addItem("128 KB", 131072);
    m_softBlockSize->addItem("256 KB", 262144);
    m_softBlockSize->addItem("512 KB", 524288);
    m_softBlockSize->addItem("1 MB", 1048576);
    m_softBlockSize->setCurrentIndex(7);  // 128KB default
    m_softBlockSize->setToolTip(tr("Normal read/write block size.\n"
                                    "Larger = faster, but less granular on errors."));
    connect(m_softBlockSize, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DDParamsWidget::onValueChanged);
    layout->addRow(tr("Block Size:"), m_softBlockSize);
    
    // Hard block size
    m_hardBlockSize = new QComboBox();
    m_hardBlockSize->addItem("512 B", 512);
    m_hardBlockSize->addItem("1 KB", 1024);
    m_hardBlockSize->addItem("2 KB", 2048);
    m_hardBlockSize->addItem("4 KB", 4096);
    m_hardBlockSize->setCurrentIndex(0);  // 512 default
    m_hardBlockSize->setToolTip(tr("Minimum block size on errors.\n"
                                    "Smaller = more data recovered from bad sectors."));
    connect(m_hardBlockSize, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DDParamsWidget::onValueChanged);
    layout->addRow(tr("Min Block (errors):"), m_hardBlockSize);
    
    // Auto adjust
    m_autoAdjust = new QCheckBox(tr("Auto-adjust on errors"));
    m_autoAdjust->setChecked(true);
    m_autoAdjust->setToolTip(tr("Automatically reduce block size when errors occur"));
    connect(m_autoAdjust, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_autoAdjust);
    
    layout->addRow(new QLabel(tr("<b>I/O Flags:</b>")));
    
    // Direct I/O
    m_directIO = new QCheckBox(tr("Direct I/O (O_DIRECT)"));
    m_directIO->setToolTip(tr("Bypass OS cache. Faster for large copies,\n"
                               "but requires aligned buffers."));
    connect(m_directIO, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_directIO);
    
    // Sync writes
    m_syncWrites = new QCheckBox(tr("Sync after each write"));
    m_syncWrites->setToolTip(tr("Force data to disk after each write.\n"
                                 "Slower but safer for removable media."));
    connect(m_syncWrites, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_syncWrites);
    
    // Sync frequency
    m_syncFrequency = new QSpinBox();
    m_syncFrequency->setRange(0, 10000);
    m_syncFrequency->setSpecialValueText(tr("Never"));
    m_syncFrequency->setSuffix(tr(" blocks"));
    m_syncFrequency->setToolTip(tr("Sync every N blocks (0 = disabled)"));
    connect(m_syncFrequency, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DDParamsWidget::onValueChanged);
    layout->addRow(tr("Sync Frequency:"), m_syncFrequency);
}

void DDParamsWidget::createRecoveryTab(QFormLayout *layout)
{
    // Recovery enabled
    m_recoveryEnabled = new QCheckBox(tr("Enable Recovery Mode"));
    m_recoveryEnabled->setChecked(true);
    m_recoveryEnabled->setToolTip(tr("Continue on errors, retry bad sectors"));
    connect(m_recoveryEnabled, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_recoveryEnabled);
    
    layout->addRow(new QLabel(tr("<b>Read Strategy:</b>")));
    
    // Reverse read
    m_reverseRead = new QCheckBox(tr("Read backwards (reverse)"));
    m_reverseRead->setToolTip(tr("Read from end to start.\n"
                                  "Useful for disks with head crashes at the start."));
    connect(m_reverseRead, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_reverseRead);
    
    // Sparse output
    m_sparseOutput = new QCheckBox(tr("Create sparse output file"));
    m_sparseOutput->setToolTip(tr("Don't write zero-filled blocks.\n"
                                   "Saves disk space for partial reads."));
    connect(m_sparseOutput, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_sparseOutput);
    
    layout->addRow(new QLabel(tr("<b>Error Handling:</b>")));
    
    // Continue on error
    m_continueOnError = new QCheckBox(tr("Continue on read errors (noerror)"));
    m_continueOnError->setChecked(true);
    m_continueOnError->setToolTip(tr("Don't stop when a read error occurs"));
    connect(m_continueOnError, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_continueOnError);
    
    // Fill on error
    QHBoxLayout *fillLayout = new QHBoxLayout();
    m_fillOnError = new QCheckBox(tr("Fill unreadable with:"));
    m_fillOnError->setChecked(true);
    m_fillPattern = new QSpinBox();
    m_fillPattern->setRange(0, 255);
    m_fillPattern->setDisplayIntegerBase(16);
    m_fillPattern->setPrefix("0x");
    m_fillPattern->setValue(0);
    m_fillPattern->setToolTip(tr("Byte pattern for unreadable sectors"));
    fillLayout->addWidget(m_fillOnError);
    fillLayout->addWidget(m_fillPattern);
    fillLayout->addStretch();
    connect(m_fillOnError, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    connect(m_fillPattern, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DDParamsWidget::onValueChanged);
    layout->addRow(fillLayout);
    
    // Max errors
    m_maxErrors = new QSpinBox();
    m_maxErrors->setRange(0, 100000);
    m_maxErrors->setSpecialValueText(tr("Unlimited"));
    m_maxErrors->setToolTip(tr("Stop after this many errors (0 = unlimited)"));
    connect(m_maxErrors, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DDParamsWidget::onValueChanged);
    layout->addRow(tr("Max Errors:"), m_maxErrors);
    
    // Retry count
    m_retryCount = new QSpinBox();
    m_retryCount->setRange(0, 100);
    m_retryCount->setValue(3);
    m_retryCount->setToolTip(tr("Number of retries for each bad sector"));
    connect(m_retryCount, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DDParamsWidget::onValueChanged);
    layout->addRow(tr("Retries:"), m_retryCount);
    
    // Retry delay
    m_retryDelay = new QSpinBox();
    m_retryDelay->setRange(0, 10000);
    m_retryDelay->setValue(100);
    m_retryDelay->setSuffix(tr(" ms"));
    m_retryDelay->setToolTip(tr("Delay between retry attempts"));
    connect(m_retryDelay, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DDParamsWidget::onValueChanged);
    layout->addRow(tr("Retry Delay:"), m_retryDelay);
}

void DDParamsWidget::createHashTab(QFormLayout *layout)
{
    layout->addRow(new QLabel(tr("<b>Hash Algorithms:</b>")));
    
    m_hashMD5 = new QCheckBox(tr("MD5"));
    m_hashMD5->setToolTip(tr("Calculate MD5 hash (fast, widely compatible)"));
    connect(m_hashMD5, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_hashMD5);
    
    m_hashSHA1 = new QCheckBox(tr("SHA-1"));
    m_hashSHA1->setToolTip(tr("Calculate SHA-1 hash"));
    connect(m_hashSHA1, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_hashSHA1);
    
    m_hashSHA256 = new QCheckBox(tr("SHA-256"));
    m_hashSHA256->setToolTip(tr("Calculate SHA-256 hash (recommended for forensics)"));
    connect(m_hashSHA256, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_hashSHA256);
    
    m_hashSHA512 = new QCheckBox(tr("SHA-512"));
    m_hashSHA512->setToolTip(tr("Calculate SHA-512 hash (most secure)"));
    connect(m_hashSHA512, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_hashSHA512);
    
    layout->addRow(new QLabel(tr("<b>Hash Targets:</b>")));
    
    m_hashInput = new QCheckBox(tr("Hash input data"));
    m_hashInput->setToolTip(tr("Calculate hash of data as read from source"));
    connect(m_hashInput, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_hashInput);
    
    m_hashOutput = new QCheckBox(tr("Hash output data"));
    m_hashOutput->setToolTip(tr("Calculate hash of data as written to destination"));
    connect(m_hashOutput, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_hashOutput);
    
    layout->addRow(new QLabel(tr("<b>Verification:</b>")));
    
    m_verifyAfter = new QCheckBox(tr("Verify by re-reading after write"));
    m_verifyAfter->setToolTip(tr("Read back and compare after copy completes"));
    connect(m_verifyAfter, &QCheckBox::toggled, this, &DDParamsWidget::onValueChanged);
    layout->addRow(m_verifyAfter);
    
    m_expectedHash = new QLineEdit();
    m_expectedHash->setPlaceholderText(tr("Enter expected hash to verify..."));
    m_expectedHash->setToolTip(tr("If provided, verify that final hash matches"));
    layout->addRow(tr("Expected Hash:"), m_expectedHash);
}

void DDParamsWidget::createFloppyTab(QFormLayout *layout)
{
    // Enable floppy output
    m_floppyEnabled = new QCheckBox(tr("Enable Floppy Output"));
    m_floppyEnabled->setToolTip(tr("Write directly to floppy disk instead of file"));
    connect(m_floppyEnabled, &QCheckBox::toggled, 
            this, &DDParamsWidget::onFloppyEnabledChanged);
    layout->addRow(m_floppyEnabled);
    
    // Device selection
    QHBoxLayout *deviceLayout = new QHBoxLayout();
    m_floppyDevice = new QComboBox();
    m_floppyDevice->setEditable(true);
    m_floppyDevice->setToolTip(tr("Floppy device path"));
#ifdef _WIN32
    m_floppyDevice->addItem("\\\\.\\A:");
    m_floppyDevice->addItem("\\\\.\\B:");
#else
    m_floppyDevice->addItem("/dev/fd0");
    m_floppyDevice->addItem("/dev/fd1");
#endif
    m_detectFloppy = new QPushButton(tr("Detect"));
    connect(m_detectFloppy, &QPushButton::clicked, 
            this, &DDParamsWidget::detectFloppyDevices);
    deviceLayout->addWidget(m_floppyDevice);
    deviceLayout->addWidget(m_detectFloppy);
    layout->addRow(tr("Device:"), deviceLayout);
    
    // Floppy type
    m_floppyType = new QComboBox();
    m_floppyType->addItem(tr("Auto-detect"), 0);
    m_floppyType->addItem(tr("DD 720K (80/2/9)"), 1);
    m_floppyType->addItem(tr("HD 1.44M (80/2/18)"), 2);
    m_floppyType->addItem(tr("DD 360K (40/2/9)"), 3);
    m_floppyType->addItem(tr("HD 1.2M (80/2/15)"), 4);
    m_floppyType->addItem(tr("Amiga DD (80/2/11)"), 5);
    m_floppyType->addItem(tr("Amiga HD (80/2/22)"), 6);
    m_floppyType->addItem(tr("Custom..."), 99);
    m_floppyType->setToolTip(tr("Disk format/geometry"));
    connect(m_floppyType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        bool custom = (m_floppyType->currentData().toInt() == 99);
        m_floppyTracks->setEnabled(custom);
        m_floppyHeads->setEnabled(custom);
        m_floppySPT->setEnabled(custom);
        
        // Set geometry from preset
        switch (m_floppyType->currentData().toInt()) {
            case 1: // DD 720K
                m_floppyTracks->setValue(80);
                m_floppyHeads->setValue(2);
                m_floppySPT->setValue(9);
                break;
            case 2: // HD 1.44M
                m_floppyTracks->setValue(80);
                m_floppyHeads->setValue(2);
                m_floppySPT->setValue(18);
                break;
            case 3: // DD 360K
                m_floppyTracks->setValue(40);
                m_floppyHeads->setValue(2);
                m_floppySPT->setValue(9);
                break;
            case 4: // HD 1.2M
                m_floppyTracks->setValue(80);
                m_floppyHeads->setValue(2);
                m_floppySPT->setValue(15);
                break;
            case 5: // Amiga DD
                m_floppyTracks->setValue(80);
                m_floppyHeads->setValue(2);
                m_floppySPT->setValue(11);
                break;
            case 6: // Amiga HD
                m_floppyTracks->setValue(80);
                m_floppyHeads->setValue(2);
                m_floppySPT->setValue(22);
                break;
        }
        onValueChanged();
    });
    layout->addRow(tr("Format:"), m_floppyType);
    
    layout->addRow(new QLabel(tr("<b>Geometry (for Custom):</b>")));
    
    // Geometry
    m_floppyTracks = new QSpinBox();
    m_floppyTracks->setRange(40, 85);
    m_floppyTracks->setValue(80);
    m_floppyTracks->setEnabled(false);
    layout->addRow(tr("Tracks:"), m_floppyTracks);
    
    m_floppyHeads = new QSpinBox();
    m_floppyHeads->setRange(1, 2);
    m_floppyHeads->setValue(2);
    m_floppyHeads->setEnabled(false);
    layout->addRow(tr("Heads:"), m_floppyHeads);
    
    m_floppySPT = new QSpinBox();
    m_floppySPT->setRange(1, 21);
    m_floppySPT->setValue(18);
    m_floppySPT->setEnabled(false);
    layout->addRow(tr("Sectors/Track:"), m_floppySPT);
    
    m_floppySectorSize = new QSpinBox();
    m_floppySectorSize->setRange(128, 1024);
    m_floppySectorSize->setSingleStep(128);
    m_floppySectorSize->setValue(512);
    m_floppySectorSize->setSuffix(tr(" bytes"));
    layout->addRow(tr("Sector Size:"), m_floppySectorSize);
    
    layout->addRow(new QLabel(tr("<b>Write Options:</b>")));
    
    m_floppyFormat = new QCheckBox(tr("Format disk before writing"));
    m_floppyFormat->setToolTip(tr("Low-level format the disk before writing image"));
    layout->addRow(m_floppyFormat);
    
    m_floppyVerify = new QCheckBox(tr("Verify each sector after write"));
    m_floppyVerify->setChecked(true);
    m_floppyVerify->setToolTip(tr("Read back and verify each sector"));
    layout->addRow(m_floppyVerify);
    
    m_floppyRetries = new QSpinBox();
    m_floppyRetries->setRange(0, 20);
    m_floppyRetries->setValue(3);
    layout->addRow(tr("Write Retries:"), m_floppyRetries);
    
    m_floppySkipBad = new QCheckBox(tr("Skip bad sectors (don't abort)"));
    m_floppySkipBad->setToolTip(tr("Continue writing even if some sectors fail"));
    layout->addRow(m_floppySkipBad);
    
    layout->addRow(new QLabel(tr("<b>Timing:</b>")));
    
    m_stepDelay = new QSpinBox();
    m_stepDelay->setRange(1, 50);
    m_stepDelay->setValue(3);
    m_stepDelay->setSuffix(tr(" ms"));
    m_stepDelay->setToolTip(tr("Head step delay between tracks"));
    layout->addRow(tr("Step Delay:"), m_stepDelay);
    
    m_settleDelay = new QSpinBox();
    m_settleDelay->setRange(5, 100);
    m_settleDelay->setValue(15);
    m_settleDelay->setSuffix(tr(" ms"));
    m_settleDelay->setToolTip(tr("Head settle delay after seek"));
    layout->addRow(tr("Settle Delay:"), m_settleDelay);
    
    m_motorDelay = new QSpinBox();
    m_motorDelay->setRange(100, 2000);
    m_motorDelay->setValue(500);
    m_motorDelay->setSuffix(tr(" ms"));
    m_motorDelay->setToolTip(tr("Motor spin-up delay"));
    layout->addRow(tr("Motor Delay:"), m_motorDelay);
    
    // Initially disable floppy controls
    onFloppyEnabledChanged(false);
}

void DDParamsWidget::createStatusGroup()
{
    m_statusGroup = new QGroupBox(tr("Status"));
    QVBoxLayout *statusLayout = new QVBoxLayout(m_statusGroup);
    
    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFormat(tr("%p% - %v MB"));
    statusLayout->addWidget(m_progressBar);
    
    // Status labels in grid
    QHBoxLayout *statsLayout = new QHBoxLayout();
    
    QVBoxLayout *leftStats = new QVBoxLayout();
    m_bytesRead = new QLabel(tr("Read: 0 B"));
    m_bytesWritten = new QLabel(tr("Written: 0 B"));
    m_errors = new QLabel(tr("Errors: 0"));
    leftStats->addWidget(m_bytesRead);
    leftStats->addWidget(m_bytesWritten);
    leftStats->addWidget(m_errors);
    statsLayout->addLayout(leftStats);
    
    QVBoxLayout *rightStats = new QVBoxLayout();
    m_speed = new QLabel(tr("Speed: -- MB/s"));
    m_eta = new QLabel(tr("ETA: --:--"));
    m_currentPosition = new QLabel(tr("Position: --"));
    rightStats->addWidget(m_speed);
    rightStats->addWidget(m_eta);
    rightStats->addWidget(m_currentPosition);
    statsLayout->addLayout(rightStats);
    
    statusLayout->addLayout(statsLayout);
    
    // Control buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton(tr("Start"));
    m_startButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    connect(m_startButton, &QPushButton::clicked, this, &DDParamsWidget::startOperation);
    
    m_pauseButton = new QPushButton(tr("Pause"));
    m_pauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    m_pauseButton->setEnabled(false);
    connect(m_pauseButton, &QPushButton::clicked, this, &DDParamsWidget::pauseOperation);
    
    m_cancelButton = new QPushButton(tr("Cancel"));
    m_cancelButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_cancelButton->setEnabled(false);
    connect(m_cancelButton, &QPushButton::clicked, this, &DDParamsWidget::cancelOperation);
    
    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_pauseButton);
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addStretch();
    
    statusLayout->addLayout(buttonLayout);
}

void DDParamsWidget::onFloppyEnabledChanged(bool enabled)
{
    m_floppyDevice->setEnabled(enabled);
    m_detectFloppy->setEnabled(enabled);
    m_floppyType->setEnabled(enabled);
    m_floppySectorSize->setEnabled(enabled);
    m_floppyFormat->setEnabled(enabled);
    m_floppyVerify->setEnabled(enabled);
    m_floppyRetries->setEnabled(enabled);
    m_floppySkipBad->setEnabled(enabled);
    m_stepDelay->setEnabled(enabled);
    m_settleDelay->setEnabled(enabled);
    m_motorDelay->setEnabled(enabled);
    
    // Disable regular output if floppy is enabled
    m_outputFile->setEnabled(!enabled);
    m_browseOutput->setEnabled(!enabled);
    
    onValueChanged();
}

void DDParamsWidget::detectFloppyDevices()
{
    char *devices[8] = {0};
    int count = dd_floppy_detect(devices, 8);
    
    m_floppyDevice->clear();
    
    if (count > 0) {
        for (int i = 0; i < count; i++) {
            if (devices[i]) {
                m_floppyDevice->addItem(QString::fromUtf8(devices[i]));
                free(devices[i]);
            }
        }
        QMessageBox::information(this, tr("Floppy Detection"),
            tr("Found %1 floppy device(s)").arg(count));
    } else {
        QMessageBox::warning(this, tr("Floppy Detection"),
            tr("No floppy devices found.\n"
               "Make sure the drive is connected and accessible."));
#ifdef _WIN32
        m_floppyDevice->addItem("\\\\.\\A:");
#else
        m_floppyDevice->addItem("/dev/fd0");
#endif
    }
}

void DDParamsWidget::updateConfigFromWidgets()
{
    // I/O
    // Note: input_file and output_file need to be allocated strings
    // For simplicity, we'll use static buffers in real implementation
    
    m_config.skip_bytes = m_skipBytes->value();
    m_config.seek_bytes = m_seekBytes->value();
    
    uint64_t maxBytes = m_maxBytes->value();
    switch (m_maxBytesUnit->currentIndex()) {
        case 1: maxBytes *= 1024; break;
        case 2: maxBytes *= 1024 * 1024; break;
        case 3: maxBytes *= 1024ULL * 1024 * 1024; break;
    }
    m_config.max_bytes = maxBytes;
    
    // Block sizes
    m_config.blocksize.soft_blocksize = m_softBlockSize->currentData().toInt();
    m_config.blocksize.hard_blocksize = m_hardBlockSize->currentData().toInt();
    m_config.blocksize.auto_adjust = m_autoAdjust->isChecked();
    
    m_config.output.direct_io = m_directIO->isChecked();
    m_config.output.sync_writes = m_syncWrites->isChecked();
    m_config.output.sync_frequency = m_syncFrequency->value();
    
    // Recovery
    m_config.recovery.enabled = m_recoveryEnabled->isChecked();
    m_config.recovery.reverse = m_reverseRead->isChecked();
    m_config.recovery.sparse = m_sparseOutput->isChecked();
    m_config.recovery.continue_on_error = m_continueOnError->isChecked();
    m_config.recovery.fill_on_error = m_fillOnError->isChecked();
    m_config.recovery.fill_pattern = m_fillPattern->value();
    m_config.recovery.max_errors = m_maxErrors->value();
    m_config.recovery.retry_count = m_retryCount->value();
    m_config.recovery.retry_delay_ms = m_retryDelay->value();
    
    // Hash
    m_config.hash.algorithms = 0;
    if (m_hashMD5->isChecked()) m_config.hash.algorithms |= HASH_MD5;
    if (m_hashSHA1->isChecked()) m_config.hash.algorithms |= HASH_SHA1;
    if (m_hashSHA256->isChecked()) m_config.hash.algorithms |= HASH_SHA256;
    if (m_hashSHA512->isChecked()) m_config.hash.algorithms |= HASH_SHA512;
    m_config.hash.hash_input = m_hashInput->isChecked();
    m_config.hash.hash_output = m_hashOutput->isChecked();
    m_config.hash.verify_after = m_verifyAfter->isChecked();
    
    // Floppy
    m_config.floppy.enabled = m_floppyEnabled->isChecked();
    m_config.floppy.tracks = m_floppyTracks->value();
    m_config.floppy.heads = m_floppyHeads->value();
    m_config.floppy.sectors_per_track = m_floppySPT->value();
    m_config.floppy.sector_size = m_floppySectorSize->value();
    m_config.floppy.format_before = m_floppyFormat->isChecked();
    m_config.floppy.verify_sectors = m_floppyVerify->isChecked();
    m_config.floppy.write_retries = m_floppyRetries->value();
    m_config.floppy.skip_bad_sectors = m_floppySkipBad->isChecked();
    m_config.floppy.step_delay_ms = m_stepDelay->value();
    m_config.floppy.settle_delay_ms = m_settleDelay->value();
    m_config.floppy.motor_delay_ms = m_motorDelay->value();
}

void DDParamsWidget::updateWidgetsFromConfig()
{
    blockSignals(true);
    
    // Block sizes
    for (int i = 0; i < m_softBlockSize->count(); i++) {
        if (m_softBlockSize->itemData(i).toInt() == (int)m_config.blocksize.soft_blocksize) {
            m_softBlockSize->setCurrentIndex(i);
            break;
        }
    }
    for (int i = 0; i < m_hardBlockSize->count(); i++) {
        if (m_hardBlockSize->itemData(i).toInt() == (int)m_config.blocksize.hard_blocksize) {
            m_hardBlockSize->setCurrentIndex(i);
            break;
        }
    }
    m_autoAdjust->setChecked(m_config.blocksize.auto_adjust);
    m_directIO->setChecked(m_config.output.direct_io);
    m_syncWrites->setChecked(m_config.output.sync_writes);
    m_syncFrequency->setValue(m_config.output.sync_frequency);
    
    // Recovery
    m_recoveryEnabled->setChecked(m_config.recovery.enabled);
    m_reverseRead->setChecked(m_config.recovery.reverse);
    m_sparseOutput->setChecked(m_config.recovery.sparse);
    m_continueOnError->setChecked(m_config.recovery.continue_on_error);
    m_fillOnError->setChecked(m_config.recovery.fill_on_error);
    m_fillPattern->setValue(m_config.recovery.fill_pattern);
    m_maxErrors->setValue(m_config.recovery.max_errors);
    m_retryCount->setValue(m_config.recovery.retry_count);
    m_retryDelay->setValue(m_config.recovery.retry_delay_ms);
    
    // Hash
    m_hashMD5->setChecked(m_config.hash.algorithms & HASH_MD5);
    m_hashSHA1->setChecked(m_config.hash.algorithms & HASH_SHA1);
    m_hashSHA256->setChecked(m_config.hash.algorithms & HASH_SHA256);
    m_hashSHA512->setChecked(m_config.hash.algorithms & HASH_SHA512);
    m_hashInput->setChecked(m_config.hash.hash_input);
    m_hashOutput->setChecked(m_config.hash.hash_output);
    m_verifyAfter->setChecked(m_config.hash.verify_after);
    
    // Floppy
    m_floppyEnabled->setChecked(m_config.floppy.enabled);
    m_floppyTracks->setValue(m_config.floppy.tracks);
    m_floppyHeads->setValue(m_config.floppy.heads);
    m_floppySPT->setValue(m_config.floppy.sectors_per_track);
    m_floppySectorSize->setValue(m_config.floppy.sector_size);
    m_floppyFormat->setChecked(m_config.floppy.format_before);
    m_floppyVerify->setChecked(m_config.floppy.verify_sectors);
    m_floppyRetries->setValue(m_config.floppy.write_retries);
    m_floppySkipBad->setChecked(m_config.floppy.skip_bad_sectors);
    m_stepDelay->setValue(m_config.floppy.step_delay_ms);
    m_settleDelay->setValue(m_config.floppy.settle_delay_ms);
    m_motorDelay->setValue(m_config.floppy.motor_delay_ms);
    
    blockSignals(false);
}

dd_config_t DDParamsWidget::getConfig() const
{
    return m_config;
}

void DDParamsWidget::setConfig(const dd_config_t &config)
{
    m_config = config;
    updateWidgetsFromConfig();
}

void DDParamsWidget::onValueChanged()
{
    updateConfigFromWidgets();
    emit configChanged();
}

void DDParamsWidget::onBrowseInput()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Select Input File"),
        QString(),
        tr("Disk Images (*.img *.ima *.adf *.d64 *.g64);;All Files (*)"));
    
    if (!fileName.isEmpty()) {
        m_inputFile->setText(fileName);
    }
}

void DDParamsWidget::onBrowseOutput()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Select Output File"),
        QString(),
        tr("Disk Images (*.img *.ima *.adf);;All Files (*)"));
    
    if (!fileName.isEmpty()) {
        m_outputFile->setText(fileName);
    }
}

void DDParamsWidget::startOperation()
{
    updateConfigFromWidgets();
    
    // Validate
    if (m_inputFile->text().isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Please select an input file."));
        return;
    }
    
    if (!m_config.floppy.enabled && m_outputFile->text().isEmpty()) {
        QMessageBox::warning(this, tr("Error"), 
            tr("Please select an output file or enable floppy output."));
        return;
    }
    
    // Update button states
    m_startButton->setEnabled(false);
    m_pauseButton->setEnabled(true);
    m_cancelButton->setEnabled(true);
    
    // Start status timer
    m_statusTimer->start(100);  // 10 Hz update
    
    emit operationStarted();
    
    // TODO: Start actual operation in thread
    // For now, just simulate
}

void DDParamsWidget::pauseOperation()
{
    if (dd_is_running()) {
        dd_pause();
        m_pauseButton->setText(tr("Resume"));
    } else {
        dd_resume();
        m_pauseButton->setText(tr("Pause"));
    }
}

void DDParamsWidget::cancelOperation()
{
    dd_cancel();
    
    m_statusTimer->stop();
    m_startButton->setEnabled(true);
    m_pauseButton->setEnabled(false);
    m_pauseButton->setText(tr("Pause"));
    m_cancelButton->setEnabled(false);
}

void DDParamsWidget::updateStatus()
{
    dd_status_t status;
    dd_get_status(&status);
    
    m_progressBar->setValue((int)status.percent_complete);
    m_bytesRead->setText(tr("Read: %1").arg(QString::fromUtf8(dd_format_size(status.bytes_read))));
    m_bytesWritten->setText(tr("Written: %1").arg(QString::fromUtf8(dd_format_size(status.bytes_written))));
    m_errors->setText(tr("Errors: %1 read, %2 write")
        .arg(status.errors_read)
        .arg(status.errors_write));
    
    if (status.bytes_per_second > 0) {
        m_speed->setText(tr("Speed: %1/s").arg(QString::fromUtf8(dd_format_size((uint64_t)status.bytes_per_second))));
    }
    
    if (status.eta_seconds > 0) {
        m_eta->setText(tr("ETA: %1").arg(QString::fromUtf8(dd_format_time(status.eta_seconds))));
    }
    
    if (m_config.floppy.enabled) {
        m_currentPosition->setText(tr("T:%1 H:%2 S:%3")
            .arg(status.current_track)
            .arg(status.current_head)
            .arg(status.current_sector));
    } else {
        m_currentPosition->setText(tr("Offset: %1").arg(QString::fromUtf8(dd_format_size(status.current_offset))));
    }
    
    emit progressUpdated(status.percent_complete);
    
    if (!status.is_running) {
        m_statusTimer->stop();
        m_startButton->setEnabled(true);
        m_pauseButton->setEnabled(false);
        m_cancelButton->setEnabled(false);
        emit operationFinished(status.has_error ? 1 : 0);
    }
}

void DDParamsWidget::resetToDefaults()
{
    dd_config_init(&m_config);
    updateWidgetsFromConfig();
    emit configChanged();
}

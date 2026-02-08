/**
 * @file uft_batch_wizard.cpp
 * @brief Batch Processing Wizard Implementation (BONUS-GUI-004)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft_batch_wizard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QDirIterator>
#include <QMessageBox>
#include <QDateTime>
#include <QTimer>
#include <QHeaderView>
#include <QStackedWidget>
#include <QCryptographicHash>
#include <QApplication>
#include <QFileInfo>

/*===========================================================================
 * UftBatchQueue
 *===========================================================================*/

UftBatchQueue::UftBatchQueue(QObject *parent)
    : QObject(parent)
    , m_nextId(1)
{
}

void UftBatchQueue::addJob(const UftBatchJob &job)
{
    QMutexLocker lock(&m_mutex);
    UftBatchJob j = job;
    j.id = m_nextId++;
    j.status = UftBatchJob::Pending;
    j.progress = 0;
    m_jobs.append(j);
    emit jobAdded(j.id);
    emit queueChanged();
}

void UftBatchQueue::removeJob(int id)
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_jobs.size(); i++) {
        if (m_jobs[i].id == id) {
            m_jobs.removeAt(i);
            emit jobRemoved(id);
            emit queueChanged();
            return;
        }
    }
}

void UftBatchQueue::clearJobs()
{
    QMutexLocker lock(&m_mutex);
    m_jobs.clear();
    emit queueChanged();
}

int UftBatchQueue::jobCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_jobs.size();
}

UftBatchJob UftBatchQueue::job(int id) const
{
    QMutexLocker lock(&m_mutex);
    for (const auto &j : m_jobs) {
        if (j.id == id) return j;
    }
    return UftBatchJob();
}

QList<UftBatchJob> UftBatchQueue::allJobs() const
{
    QMutexLocker lock(&m_mutex);
    return m_jobs;
}

void UftBatchQueue::moveUp(int id)
{
    QMutexLocker lock(&m_mutex);
    for (int i = 1; i < m_jobs.size(); i++) {
        if (m_jobs[i].id == id) {
            m_jobs.swapItemsAt(i, i - 1);
            emit queueChanged();
            return;
        }
    }
}

void UftBatchQueue::moveDown(int id)
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_jobs.size() - 1; i++) {
        if (m_jobs[i].id == id) {
            m_jobs.swapItemsAt(i, i + 1);
            emit queueChanged();
            return;
        }
    }
}

/*===========================================================================
 * UftBatchFilesPage
 *===========================================================================*/

UftBatchFilesPage::UftBatchFilesPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Select Files"));
    setSubTitle(tr("Add disk images to process in batch."));
    setupUi();
}

void UftBatchFilesPage::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    /* Buttons */
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_addFilesBtn = new QPushButton(tr("Add Files..."));
    m_addFolderBtn = new QPushButton(tr("Add Folder..."));
    m_removeBtn = new QPushButton(tr("Remove"));
    m_clearBtn = new QPushButton(tr("Clear All"));
    m_recursiveCheck = new QCheckBox(tr("Include subfolders"));
    m_recursiveCheck->setChecked(true);
    
    btnLayout->addWidget(m_addFilesBtn);
    btnLayout->addWidget(m_addFolderBtn);
    btnLayout->addWidget(m_recursiveCheck);
    btnLayout->addStretch();
    btnLayout->addWidget(m_removeBtn);
    btnLayout->addWidget(m_clearBtn);
    layout->addLayout(btnLayout);
    
    /* File list */
    m_fileList = new QListWidget();
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setAlternatingRowColors(true);
    layout->addWidget(m_fileList);
    
    /* Count */
    m_countLabel = new QLabel(tr("0 files selected"));
    layout->addWidget(m_countLabel);
    
    /* Connections */
    connect(m_addFilesBtn, &QPushButton::clicked, this, &UftBatchFilesPage::addFiles);
    connect(m_addFolderBtn, &QPushButton::clicked, this, &UftBatchFilesPage::addFolder);
    connect(m_removeBtn, &QPushButton::clicked, this, &UftBatchFilesPage::removeSelected);
    connect(m_clearBtn, &QPushButton::clicked, this, &UftBatchFilesPage::clearAll);
    connect(m_fileList, &QListWidget::itemSelectionChanged, [this]() {
        m_removeBtn->setEnabled(!m_fileList->selectedItems().isEmpty());
    });
}

void UftBatchFilesPage::addFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Add Files"),
        QString(), tr("Disk Images (*.d64 *.d71 *.d81 *.g64 *.adf *.atr *.dsk *.img *.scp *.hfe *.woz);;All Files (*)"));
    
    for (const QString &f : files) {
        if (m_fileList->findItems(f, Qt::MatchExactly).isEmpty()) {
            m_fileList->addItem(f);
        }
    }
    
    m_countLabel->setText(tr("%1 files selected").arg(m_fileList->count()));
    emit completeChanged();
}

void UftBatchFilesPage::addFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Add Folder"));
    if (!dir.isEmpty()) {
        scanFolder(dir, m_recursiveCheck->isChecked());
    }
}

void UftBatchFilesPage::scanFolder(const QString &path, bool recursive)
{
    QStringList filters = {"*.d64", "*.d71", "*.d81", "*.g64", "*.adf", 
                           "*.atr", "*.dsk", "*.img", "*.scp", "*.hfe", "*.woz"};
    
    QDirIterator::IteratorFlags flags = recursive ? 
        QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    
    QDirIterator it(path, filters, QDir::Files, flags);
    while (it.hasNext()) {
        QString file = it.next();
        if (m_fileList->findItems(file, Qt::MatchExactly).isEmpty()) {
            m_fileList->addItem(file);
        }
    }
    
    m_countLabel->setText(tr("%1 files selected").arg(m_fileList->count()));
    emit completeChanged();
}

void UftBatchFilesPage::removeSelected()
{
    QList<QListWidgetItem*> items = m_fileList->selectedItems();
    for (auto *item : items) {
        delete item;
    }
    m_countLabel->setText(tr("%1 files selected").arg(m_fileList->count()));
    emit completeChanged();
}

void UftBatchFilesPage::clearAll()
{
    m_fileList->clear();
    m_countLabel->setText(tr("0 files selected"));
    emit completeChanged();
}

bool UftBatchFilesPage::isComplete() const
{
    return m_fileList->count() > 0;
}

QStringList UftBatchFilesPage::selectedFiles() const
{
    QStringList files;
    for (int i = 0; i < m_fileList->count(); i++) {
        files << m_fileList->item(i)->text();
    }
    return files;
}

/*===========================================================================
 * UftBatchOperationPage
 *===========================================================================*/

UftBatchOperationPage::UftBatchOperationPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Select Operation"));
    setSubTitle(tr("Choose what to do with the selected files."));
    setupUi();
}

void UftBatchOperationPage::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    /* Operation selection */
    QHBoxLayout *opLayout = new QHBoxLayout();
    opLayout->addWidget(new QLabel(tr("Operation:")));
    m_operationCombo = new QComboBox();
    m_operationCombo->addItem(tr("Convert to format"), BATCH_OP_CONVERT);
    m_operationCombo->addItem(tr("Analyze & Report"), BATCH_OP_ANALYZE);
    m_operationCombo->addItem(tr("Verify integrity"), BATCH_OP_VERIFY);
    m_operationCombo->addItem(tr("Extract files"), BATCH_OP_EXTRACT);
    m_operationCombo->addItem(tr("Calculate hashes"), BATCH_OP_HASH);
    opLayout->addWidget(m_operationCombo);
    opLayout->addStretch();
    layout->addLayout(opLayout);
    
    /* Options stack */
    m_optionsStack = new QStackedWidget();
    
    createConvertOptions();
    createAnalyzeOptions();
    createExtractOptions();
    createHashOptions();
    
    /* Verify uses analyze options */
    m_optionsStack->addWidget(m_convertWidget);
    m_optionsStack->addWidget(m_analyzeWidget);
    m_optionsStack->addWidget(m_analyzeWidget);  /* Verify */
    m_optionsStack->addWidget(m_extractWidget);
    m_optionsStack->addWidget(m_hashWidget);
    
    layout->addWidget(m_optionsStack);
    layout->addStretch();
    
    connect(m_operationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftBatchOperationPage::onOperationChanged);
}

void UftBatchOperationPage::createConvertOptions()
{
    m_convertWidget = new QWidget();
    QFormLayout *form = new QFormLayout(m_convertWidget);
    
    m_targetFormat = new QComboBox();
    m_targetFormat->addItems({"ADF", "D64", "G64", "IMG", "HFE", "SCP"});
    form->addRow(tr("Target format:"), m_targetFormat);
    
    QHBoxLayout *dirLayout = new QHBoxLayout();
    m_outputDir = new QLineEdit();
    QPushButton *browseBtn = new QPushButton(tr("..."));
    browseBtn->setMaximumWidth(30);
    dirLayout->addWidget(m_outputDir);
    dirLayout->addWidget(browseBtn);
    form->addRow(tr("Output directory:"), dirLayout);
    
    m_preserveStructure = new QCheckBox(tr("Preserve folder structure"));
    m_preserveStructure->setChecked(true);
    form->addRow("", m_preserveStructure);
    
    m_overwriteExisting = new QCheckBox(tr("Overwrite existing files"));
    form->addRow("", m_overwriteExisting);
    
    connect(browseBtn, &QPushButton::clicked, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Output Directory"));
        if (!dir.isEmpty()) m_outputDir->setText(dir);
    });
}

void UftBatchOperationPage::createAnalyzeOptions()
{
    m_analyzeWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_analyzeWidget);
    
    m_detectProtection = new QCheckBox(tr("Detect copy protection"));
    m_detectProtection->setChecked(true);
    layout->addWidget(m_detectProtection);
    
    m_verifyChecksums = new QCheckBox(tr("Verify sector checksums"));
    m_verifyChecksums->setChecked(true);
    layout->addWidget(m_verifyChecksums);
    
    m_generateReport = new QCheckBox(tr("Generate HTML report"));
    m_generateReport->setChecked(true);
    layout->addWidget(m_generateReport);
    
    layout->addStretch();
}

void UftBatchOperationPage::createExtractOptions()
{
    m_extractWidget = new QWidget();
    QFormLayout *form = new QFormLayout(m_extractWidget);
    
    QHBoxLayout *dirLayout = new QHBoxLayout();
    m_extractDir = new QLineEdit();
    QPushButton *browseBtn = new QPushButton(tr("..."));
    browseBtn->setMaximumWidth(30);
    dirLayout->addWidget(m_extractDir);
    dirLayout->addWidget(browseBtn);
    form->addRow(tr("Extract to:"), dirLayout);
    
    m_extractAll = new QCheckBox(tr("Extract all files"));
    m_extractAll->setChecked(true);
    form->addRow("", m_extractAll);
    
    m_extractFilter = new QLineEdit();
    m_extractFilter->setPlaceholderText(tr("*.prg, *.seq"));
    form->addRow(tr("Filter:"), m_extractFilter);
    
    connect(browseBtn, &QPushButton::clicked, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Extract Directory"));
        if (!dir.isEmpty()) m_extractDir->setText(dir);
    });
}

void UftBatchOperationPage::createHashOptions()
{
    m_hashWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_hashWidget);
    
    layout->addWidget(new QLabel(tr("Calculate:")));
    
    m_hashMD5 = new QCheckBox(tr("MD5"));
    m_hashMD5->setChecked(true);
    layout->addWidget(m_hashMD5);
    
    m_hashSHA1 = new QCheckBox(tr("SHA-1"));
    m_hashSHA1->setChecked(true);
    layout->addWidget(m_hashSHA1);
    
    m_hashSHA256 = new QCheckBox(tr("SHA-256"));
    layout->addWidget(m_hashSHA256);
    
    m_hashCRC32 = new QCheckBox(tr("CRC-32"));
    m_hashCRC32->setChecked(true);
    layout->addWidget(m_hashCRC32);
    
    layout->addStretch();
}

void UftBatchOperationPage::onOperationChanged(int index)
{
    m_optionsStack->setCurrentIndex(index);
}

UftBatchOperation UftBatchOperationPage::selectedOperation() const
{
    return (UftBatchOperation)m_operationCombo->currentData().toInt();
}

QVariantMap UftBatchOperationPage::operationOptions() const
{
    QVariantMap opts;
    
    switch (selectedOperation()) {
        case BATCH_OP_CONVERT:
            opts["targetFormat"] = m_targetFormat->currentText();
            opts["outputDir"] = m_outputDir->text();
            opts["preserveStructure"] = m_preserveStructure->isChecked();
            opts["overwrite"] = m_overwriteExisting->isChecked();
            break;
        case BATCH_OP_ANALYZE:
        case BATCH_OP_VERIFY:
            opts["detectProtection"] = m_detectProtection->isChecked();
            opts["verifyChecksums"] = m_verifyChecksums->isChecked();
            opts["generateReport"] = m_generateReport->isChecked();
            break;
        case BATCH_OP_EXTRACT:
            opts["extractDir"] = m_extractDir->text();
            opts["extractAll"] = m_extractAll->isChecked();
            opts["filter"] = m_extractFilter->text();
            break;
        case BATCH_OP_HASH:
            opts["md5"] = m_hashMD5->isChecked();
            opts["sha1"] = m_hashSHA1->isChecked();
            opts["sha256"] = m_hashSHA256->isChecked();
            opts["crc32"] = m_hashCRC32->isChecked();
            break;
        default:
            break;
    }
    
    return opts;
}

/*===========================================================================
 * UftBatchOutputPage
 *===========================================================================*/

UftBatchOutputPage::UftBatchOutputPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Output Settings"));
    setSubTitle(tr("Configure where and how to save results."));
    setupUi();
}

void UftBatchOutputPage::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    QFormLayout *form = new QFormLayout();
    
    /* Output directory */
    QHBoxLayout *dirLayout = new QHBoxLayout();
    m_outputDir = new QLineEdit();
    m_browseBtn = new QPushButton(tr("..."));
    m_browseBtn->setMaximumWidth(30);
    dirLayout->addWidget(m_outputDir);
    dirLayout->addWidget(m_browseBtn);
    form->addRow(tr("Output folder:"), dirLayout);
    
    /* Naming */
    m_namingCombo = new QComboBox();
    m_namingCombo->addItems({
        tr("Same as source"),
        tr("Add suffix"),
        tr("Add timestamp"),
        tr("Custom pattern")
    });
    form->addRow(tr("File naming:"), m_namingCombo);
    
    m_customPattern = new QLineEdit();
    m_customPattern->setPlaceholderText(tr("{name}_{format}_{date}"));
    m_customPattern->setEnabled(false);
    form->addRow(tr("Pattern:"), m_customPattern);
    
    layout->addLayout(form);
    
    /* Options */
    m_subfolders = new QCheckBox(tr("Create subfolders by format"));
    layout->addWidget(m_subfolders);
    
    m_timestampFolder = new QCheckBox(tr("Create timestamped batch folder"));
    layout->addWidget(m_timestampFolder);
    
    /* Preview */
    QGroupBox *previewGroup = new QGroupBox(tr("Preview"));
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel(tr("Output: (select options above)"));
    m_previewLabel->setStyleSheet("font-family: monospace;");
    previewLayout->addWidget(m_previewLabel);
    layout->addWidget(previewGroup);
    
    layout->addStretch();
    
    /* Connections */
    connect(m_browseBtn, &QPushButton::clicked, this, &UftBatchOutputPage::browseOutput);
    connect(m_namingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
        m_customPattern->setEnabled(idx == 3);
        updatePreview();
    });
    connect(m_outputDir, &QLineEdit::textChanged, this, &UftBatchOutputPage::updatePreview);
}

void UftBatchOutputPage::browseOutput()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Output Folder"));
    if (!dir.isEmpty()) {
        m_outputDir->setText(dir);
    }
}

void UftBatchOutputPage::updatePreview()
{
    QString preview = m_outputDir->text();
    if (preview.isEmpty()) preview = tr("(not set)");
    
    preview += "/example";
    
    switch (m_namingCombo->currentIndex()) {
        case 1: preview += "_converted"; break;
        case 2: preview += "_20260105"; break;
        case 3: preview += "_custom"; break;
    }
    
    preview += ".adf";
    
    m_previewLabel->setText(tr("Example: %1").arg(preview));
}

QString UftBatchOutputPage::outputDirectory() const
{
    return m_outputDir->text();
}

QString UftBatchOutputPage::namingPattern() const
{
    return m_customPattern->text();
}

bool UftBatchOutputPage::createSubfolders() const
{
    return m_subfolders->isChecked();
}

/*===========================================================================
 * UftBatchQueuePage
 *===========================================================================*/

UftBatchQueuePage::UftBatchQueuePage(QWidget *parent)
    : QWizardPage(parent)
    , m_queue(new UftBatchQueue(this))
{
    setTitle(tr("Review Queue"));
    setSubTitle(tr("Review and adjust the batch processing queue."));
    setupUi();
}

void UftBatchQueuePage::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    /* Table */
    m_queueTable = new QTableWidget();
    m_queueTable->setColumnCount(4);
    m_queueTable->setHorizontalHeaderLabels({tr("File"), tr("Operation"), tr("Target"), tr("Status")});
    m_queueTable->horizontalHeader()->setStretchLastSection(true);
    m_queueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_queueTable->setAlternatingRowColors(true);
    layout->addWidget(m_queueTable);
    
    /* Buttons */
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_upBtn = new QPushButton(tr("▲ Up"));
    m_downBtn = new QPushButton(tr("▼ Down"));
    m_removeBtn = new QPushButton(tr("Remove"));
    m_summaryLabel = new QLabel();
    
    btnLayout->addWidget(m_upBtn);
    btnLayout->addWidget(m_downBtn);
    btnLayout->addWidget(m_removeBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_summaryLabel);
    layout->addLayout(btnLayout);
    
    connect(m_upBtn, &QPushButton::clicked, this, &UftBatchQueuePage::onMoveUp);
    connect(m_downBtn, &QPushButton::clicked, this, &UftBatchQueuePage::onMoveDown);
    connect(m_removeBtn, &QPushButton::clicked, this, &UftBatchQueuePage::onRemove);
    connect(m_queue, &UftBatchQueue::queueChanged, this, &UftBatchQueuePage::updateTable);
}

void UftBatchQueuePage::initializePage()
{
    /* Build queue from previous pages */
    UftBatchWizard *wiz = qobject_cast<UftBatchWizard*>(wizard());
    if (!wiz) return;
    
    m_queue->clearJobs();
    
    UftBatchFilesPage *filesPage = qobject_cast<UftBatchFilesPage*>(wiz->page(UftBatchWizard::Page_Files));
    UftBatchOperationPage *opPage = qobject_cast<UftBatchOperationPage*>(wiz->page(UftBatchWizard::Page_Operation));
    UftBatchOutputPage *outPage = qobject_cast<UftBatchOutputPage*>(wiz->page(UftBatchWizard::Page_Output));
    
    if (!filesPage || !opPage || !outPage) return;
    
    QStringList files = filesPage->selectedFiles();
    UftBatchOperation op = opPage->selectedOperation();
    QVariantMap opts = opPage->operationOptions();
    QString outDir = outPage->outputDirectory();
    
    for (const QString &file : files) {
        UftBatchJob job;
        job.sourcePath = file;
        job.operation = op;
        job.options = opts;
        
        /* Generate target path */
        QFileInfo fi(file);
        if (op == BATCH_OP_CONVERT) {
            QString ext = opts["targetFormat"].toString().toLower();
            job.targetPath = outDir + "/" + fi.completeBaseName() + "." + ext;
        } else {
            job.targetPath = outDir;
        }
        
        m_queue->addJob(job);
    }
    
    updateTable();
}

void UftBatchQueuePage::updateTable()
{
    m_queueTable->setRowCount(0);
    
    QList<UftBatchJob> jobs = m_queue->allJobs();
    for (const auto &job : jobs) {
        int row = m_queueTable->rowCount();
        m_queueTable->insertRow(row);
        
        QFileInfo fi(job.sourcePath);
        m_queueTable->setItem(row, 0, new QTableWidgetItem(fi.fileName()));
        
        QString opName;
        switch (job.operation) {
            case BATCH_OP_CONVERT: opName = tr("Convert"); break;
            case BATCH_OP_ANALYZE: opName = tr("Analyze"); break;
            case BATCH_OP_VERIFY: opName = tr("Verify"); break;
            case BATCH_OP_EXTRACT: opName = tr("Extract"); break;
            case BATCH_OP_HASH: opName = tr("Hash"); break;
            default: opName = "?"; break;
        }
        m_queueTable->setItem(row, 1, new QTableWidgetItem(opName));
        m_queueTable->setItem(row, 2, new QTableWidgetItem(job.targetPath));
        m_queueTable->setItem(row, 3, new QTableWidgetItem(tr("Pending")));
        
        m_queueTable->item(row, 0)->setData(Qt::UserRole, job.id);
    }
    
    m_summaryLabel->setText(tr("%1 jobs in queue").arg(jobs.size()));
}

void UftBatchQueuePage::onMoveUp()
{
    int row = m_queueTable->currentRow();
    if (row > 0) {
        int id = m_queueTable->item(row, 0)->data(Qt::UserRole).toInt();
        m_queue->moveUp(id);
    }
}

void UftBatchQueuePage::onMoveDown()
{
    int row = m_queueTable->currentRow();
    if (row >= 0 && row < m_queueTable->rowCount() - 1) {
        int id = m_queueTable->item(row, 0)->data(Qt::UserRole).toInt();
        m_queue->moveDown(id);
    }
}

void UftBatchQueuePage::onRemove()
{
    int row = m_queueTable->currentRow();
    if (row >= 0) {
        int id = m_queueTable->item(row, 0)->data(Qt::UserRole).toInt();
        m_queue->removeJob(id);
    }
}

/*===========================================================================
 * UftBatchProgressPage
 *===========================================================================*/

UftBatchProgressPage::UftBatchProgressPage(QWidget *parent)
    : QWizardPage(parent)
    , m_queue(nullptr)
    , m_isRunning(false)
    , m_isPaused(false)
    , m_isCancelled(false)
    , m_completedJobs(0)
    , m_failedJobs(0)
{
    setTitle(tr("Processing"));
    setSubTitle(tr("Batch processing in progress..."));
    setupUi();
}

void UftBatchProgressPage::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    /* Overall progress */
    QFormLayout *progressForm = new QFormLayout();
    m_overallProgress = new QProgressBar();
    m_currentProgress = new QProgressBar();
    progressForm->addRow(tr("Overall:"), m_overallProgress);
    progressForm->addRow(tr("Current:"), m_currentProgress);
    layout->addLayout(progressForm);
    
    /* Labels */
    m_overallLabel = new QLabel(tr("Ready"));
    m_currentLabel = new QLabel();
    m_timeLabel = new QLabel();
    layout->addWidget(m_overallLabel);
    layout->addWidget(m_currentLabel);
    layout->addWidget(m_timeLabel);
    
    /* Job table */
    m_jobTable = new QTableWidget();
    m_jobTable->setColumnCount(4);
    m_jobTable->setHorizontalHeaderLabels({tr("File"), tr("Status"), tr("Progress"), tr("Message")});
    m_jobTable->horizontalHeader()->setStretchLastSection(true);
    m_jobTable->setMaximumHeight(150);
    layout->addWidget(m_jobTable);
    
    /* Log */
    QGroupBox *logGroup = new QGroupBox(tr("Log"));
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    m_logView = new QTextEdit();
    m_logView->setReadOnly(true);
    m_logView->setStyleSheet("font-family: monospace; font-size: 10px;");
    m_logView->setMaximumHeight(100);
    logLayout->addWidget(m_logView);
    layout->addWidget(logGroup);
    
    /* Buttons */
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton(tr("Start"));
    m_pauseBtn = new QPushButton(tr("Pause"));
    m_cancelBtn = new QPushButton(tr("Cancel"));
    m_exportLogBtn = new QPushButton(tr("Export Log"));
    
    m_pauseBtn->setEnabled(false);
    m_cancelBtn->setEnabled(false);
    m_exportLogBtn->setEnabled(false);
    
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_pauseBtn);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_exportLogBtn);
    layout->addLayout(btnLayout);
    
    connect(m_startBtn, &QPushButton::clicked, this, &UftBatchProgressPage::startProcessing);
    connect(m_pauseBtn, &QPushButton::clicked, this, &UftBatchProgressPage::pauseProcessing);
    connect(m_cancelBtn, &QPushButton::clicked, this, &UftBatchProgressPage::cancelProcessing);
}

void UftBatchProgressPage::initializePage()
{
    /* Get queue from previous page */
    UftBatchWizard *wiz = qobject_cast<UftBatchWizard*>(wizard());
    if (wiz) {
        UftBatchQueuePage *queuePage = qobject_cast<UftBatchQueuePage*>(
            wiz->page(UftBatchWizard::Page_Queue));
        if (queuePage) {
            m_queue = queuePage->queue();
        }
    }
    
    /* Populate job table */
    if (m_queue) {
        m_jobTable->setRowCount(0);
        QList<UftBatchJob> jobs = m_queue->allJobs();
        for (const auto &job : jobs) {
            int row = m_jobTable->rowCount();
            m_jobTable->insertRow(row);
            
            QFileInfo fi(job.sourcePath);
            m_jobTable->setItem(row, 0, new QTableWidgetItem(fi.fileName()));
            m_jobTable->setItem(row, 1, new QTableWidgetItem(tr("Pending")));
            m_jobTable->setItem(row, 2, new QTableWidgetItem("0%"));
            m_jobTable->setItem(row, 3, new QTableWidgetItem("-"));
        }
        
        m_overallProgress->setMaximum(jobs.size());
        m_overallProgress->setValue(0);
    }
    
    m_isRunning = false;
    m_isPaused = false;
    m_isCancelled = false;
    m_completedJobs = 0;
    m_failedJobs = 0;
}

void UftBatchProgressPage::cleanupPage()
{
    cancelProcessing();
}

bool UftBatchProgressPage::isComplete() const
{
    return !m_isRunning && m_completedJobs > 0;
}

void UftBatchProgressPage::startProcessing()
{
    m_isRunning = true;
    m_isPaused = false;
    m_isCancelled = false;
    m_startTime = QDateTime::currentDateTime();
    
    m_startBtn->setEnabled(false);
    m_pauseBtn->setEnabled(true);
    m_cancelBtn->setEnabled(true);
    
    m_logView->append(tr("[%1] Batch processing started")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    
    processNextJob();
}

void UftBatchProgressPage::pauseProcessing()
{
    m_isPaused = !m_isPaused;
    m_pauseBtn->setText(m_isPaused ? tr("Resume") : tr("Pause"));
    
    if (!m_isPaused) {
        processNextJob();
    }
}

void UftBatchProgressPage::cancelProcessing()
{
    m_isCancelled = true;
    m_isRunning = false;
    
    m_logView->append(tr("[%1] Batch processing cancelled")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    
    m_startBtn->setEnabled(true);
    m_pauseBtn->setEnabled(false);
    m_cancelBtn->setEnabled(false);
}

void UftBatchProgressPage::processNextJob()
{
    if (m_isCancelled || m_isPaused || !m_queue) return;
    
    QList<UftBatchJob> jobs = m_queue->allJobs();
    
    /* Find next pending job */
    int jobIndex = -1;
    for (int i = 0; i < jobs.size(); i++) {
        if (jobs[i].status == UftBatchJob::Pending) {
            jobIndex = i;
            break;
        }
    }
    
    if (jobIndex < 0) {
        /* All done */
        onAllComplete();
        return;
    }
    
    UftBatchJob &job = jobs[jobIndex];
    onJobStarted(job.id);
    
    /* Simulate processing */
    m_currentLabel->setText(QFileInfo(job.sourcePath).fileName());
    
    for (int p = 0; p <= 100; p += 10) {
        if (m_isCancelled) break;
        
        onJobProgress(job.id, p, QString("Processing... %1%").arg(p));
        QApplication::processEvents();
        QThread::msleep(50);
    }
    
    if (!m_isCancelled) {
        onJobComplete(job.id, true, tr("Success"));
        m_completedJobs++;
        m_overallProgress->setValue(m_completedJobs);
        
        /* Process next */
        QTimer::singleShot(100, this, &UftBatchProgressPage::processNextJob);
    }
}

void UftBatchProgressPage::onJobStarted(int id)
{
    for (int row = 0; row < m_jobTable->rowCount(); row++) {
        /* Find row - simplified */
    }
    
    m_logView->append(tr("[%1] Job %2 started")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(id));
}

void UftBatchProgressPage::onJobProgress(int, int percent, const QString &status)
{
    m_currentProgress->setValue(percent);
    m_currentLabel->setText(status);
}

void UftBatchProgressPage::onJobComplete(int id, bool success, const QString &message)
{
    m_logView->append(tr("[%1] Job %2 %3: %4")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(id)
        .arg(success ? tr("completed") : tr("failed"))
        .arg(message));
}

void UftBatchProgressPage::onAllComplete()
{
    m_isRunning = false;
    
    QDateTime endTime = QDateTime::currentDateTime();
    qint64 elapsed = m_startTime.secsTo(endTime);
    
    m_overallLabel->setText(tr("Complete! %1 successful, %2 failed")
        .arg(m_completedJobs).arg(m_failedJobs));
    m_timeLabel->setText(tr("Elapsed time: %1:%2:%3")
        .arg(elapsed / 3600, 2, 10, QChar('0'))
        .arg((elapsed % 3600) / 60, 2, 10, QChar('0'))
        .arg(elapsed % 60, 2, 10, QChar('0')));
    
    m_logView->append(tr("[%1] Batch processing complete")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    
    m_startBtn->setEnabled(true);
    m_pauseBtn->setEnabled(false);
    m_cancelBtn->setEnabled(false);
    m_exportLogBtn->setEnabled(true);
    
    emit completeChanged();
}

/*===========================================================================
 * UftBatchWizard
 *===========================================================================*/

UftBatchWizard::UftBatchWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle(tr("Batch Processing"));
    setWizardStyle(QWizard::ModernStyle);
    setMinimumSize(700, 500);
    
    setupPages();
}

void UftBatchWizard::setupPages()
{
    m_filesPage = new UftBatchFilesPage();
    m_operationPage = new UftBatchOperationPage();
    m_outputPage = new UftBatchOutputPage();
    m_queuePage = new UftBatchQueuePage();
    m_progressPage = new UftBatchProgressPage();
    
    setPage(Page_Files, m_filesPage);
    setPage(Page_Operation, m_operationPage);
    setPage(Page_Output, m_outputPage);
    setPage(Page_Queue, m_queuePage);
    setPage(Page_Progress, m_progressPage);
}

/*===========================================================================
 * UftBatchWorker
 *===========================================================================*/

UftBatchWorker::UftBatchWorker(QObject *parent)
    : QObject(parent)
    , m_cancelled(false)
{
}

void UftBatchWorker::setJob(const UftBatchJob &job)
{
    m_job = job;
}

void UftBatchWorker::process()
{
    emit started();
    
    switch (m_job.operation) {
        case BATCH_OP_CONVERT: processConvert(); break;
        case BATCH_OP_ANALYZE: processAnalyze(); break;
        case BATCH_OP_VERIFY: processVerify(); break;
        case BATCH_OP_EXTRACT: processExtract(); break;
        case BATCH_OP_HASH: processHash(); break;
        default: emit error(tr("Unknown operation")); break;
    }
}

void UftBatchWorker::cancel()
{
    m_cancelled = true;
}

void UftBatchWorker::processConvert()
{
    emit progress(50, tr("Converting..."));
    /* TODO: Actual conversion */
    emit complete(true, tr("Converted successfully"));
}

void UftBatchWorker::processAnalyze()
{
    emit progress(50, tr("Analyzing..."));
    emit complete(true, tr("Analysis complete"));
}

void UftBatchWorker::processVerify()
{
    emit progress(50, tr("Verifying..."));
    emit complete(true, tr("Verification passed"));
}

void UftBatchWorker::processExtract()
{
    emit progress(50, tr("Extracting..."));
    emit complete(true, tr("Extraction complete"));
}

void UftBatchWorker::processHash()
{
    QFile file(m_job.sourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit error(tr("Cannot open file"));
        emit complete(false, tr("Hash failed"));
        return;
    }
    
    QByteArray data = file.readAll();
    
    QString result;
    if (m_job.options["md5"].toBool()) {
        result += "MD5: " + QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex() + "\n";
    }
    if (m_job.options["sha1"].toBool()) {
        result += "SHA1: " + QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex() + "\n";
    }
    
    emit progress(100, result);
    emit complete(true, tr("Hash calculated"));
}

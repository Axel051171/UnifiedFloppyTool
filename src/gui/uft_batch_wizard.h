/**
 * @file uft_batch_wizard.h
 * @brief Batch Processing Wizard (BONUS-GUI-004)
 * 
 * Process multiple disk images in batch mode.
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_BATCH_WIZARD_H
#define UFT_BATCH_WIZARD_H

#include <QWizard>
#include <QWizardPage>
#include <QListWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QProgressBar>
#include <QTextEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QThread>
#include <QQueue>
#include <QMutex>

/*===========================================================================
 * Batch Job Structures
 *===========================================================================*/

/**
 * @brief Batch operation types
 */
enum UftBatchOperation {
    BATCH_OP_CONVERT,
    BATCH_OP_ANALYZE,
    BATCH_OP_VERIFY,
    BATCH_OP_EXTRACT,
    BATCH_OP_HASH,
    BATCH_OP_COMPARE,
    BATCH_OP_REPAIR
};

/**
 * @brief Single job in batch queue
 */
struct UftBatchJob {
    int id;
    QString sourcePath;
    QString targetPath;
    UftBatchOperation operation;
    QVariantMap options;
    
    /* Status */
    enum Status { Pending, Running, Complete, Failed, Skipped };
    Status status;
    QString statusMessage;
    int progress;
    
    /* Results */
    bool success;
    QString resultSummary;
    QStringList warnings;
    QStringList errors;
};

/**
 * @brief Batch job queue
 */
class UftBatchQueue : public QObject
{
    Q_OBJECT
    
public:
    explicit UftBatchQueue(QObject *parent = nullptr);
    
    void addJob(const UftBatchJob &job);
    void removeJob(int id);
    void clearJobs();
    
    int jobCount() const;
    UftBatchJob job(int id) const;
    QList<UftBatchJob> allJobs() const;
    
    void moveUp(int id);
    void moveDown(int id);

signals:
    void jobAdded(int id);
    void jobRemoved(int id);
    void jobUpdated(int id);
    void queueChanged();

private:
    QList<UftBatchJob> m_jobs;
    int m_nextId;
    mutable QMutex m_mutex;
};

/*===========================================================================
 * Wizard Pages
 *===========================================================================*/

/**
 * @brief File selection page
 */
class UftBatchFilesPage : public QWizardPage
{
    Q_OBJECT
    
public:
    explicit UftBatchFilesPage(QWidget *parent = nullptr);
    
    bool isComplete() const override;
    QStringList selectedFiles() const;

private slots:
    void addFiles();
    void addFolder();
    void removeSelected();
    void clearAll();
    void scanFolder(const QString &path, bool recursive);

private:
    void setupUi();
    
    QListWidget *m_fileList;
    QPushButton *m_addFilesBtn;
    QPushButton *m_addFolderBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_clearBtn;
    QCheckBox *m_recursiveCheck;
    QLabel *m_countLabel;
};

/**
 * @brief Operation selection page
 */
class UftBatchOperationPage : public QWizardPage
{
    Q_OBJECT
    
public:
    explicit UftBatchOperationPage(QWidget *parent = nullptr);
    
    UftBatchOperation selectedOperation() const;
    QVariantMap operationOptions() const;

private slots:
    void onOperationChanged(int index);

private:
    void setupUi();
    void createConvertOptions();
    void createAnalyzeOptions();
    void createExtractOptions();
    void createHashOptions();
    
    QComboBox *m_operationCombo;
    QStackedWidget *m_optionsStack;
    
    /* Convert options */
    QWidget *m_convertWidget;
    QComboBox *m_targetFormat;
    QLineEdit *m_outputDir;
    QCheckBox *m_preserveStructure;
    QCheckBox *m_overwriteExisting;
    
    /* Analyze options */
    QWidget *m_analyzeWidget;
    QCheckBox *m_detectProtection;
    QCheckBox *m_verifyChecksums;
    QCheckBox *m_generateReport;
    
    /* Extract options */
    QWidget *m_extractWidget;
    QLineEdit *m_extractDir;
    QCheckBox *m_extractAll;
    QLineEdit *m_extractFilter;
    
    /* Hash options */
    QWidget *m_hashWidget;
    QCheckBox *m_hashMD5;
    QCheckBox *m_hashSHA1;
    QCheckBox *m_hashSHA256;
    QCheckBox *m_hashCRC32;
};

/**
 * @brief Output configuration page
 */
class UftBatchOutputPage : public QWizardPage
{
    Q_OBJECT
    
public:
    explicit UftBatchOutputPage(QWidget *parent = nullptr);
    
    QString outputDirectory() const;
    QString namingPattern() const;
    bool createSubfolders() const;

private slots:
    void browseOutput();
    void updatePreview();

private:
    void setupUi();
    
    QLineEdit *m_outputDir;
    QPushButton *m_browseBtn;
    QComboBox *m_namingCombo;
    QLineEdit *m_customPattern;
    QCheckBox *m_subfolders;
    QCheckBox *m_timestampFolder;
    QLabel *m_previewLabel;
};

/**
 * @brief Queue review page
 */
class UftBatchQueuePage : public QWizardPage
{
    Q_OBJECT
    
public:
    explicit UftBatchQueuePage(QWidget *parent = nullptr);
    
    void initializePage() override;
    UftBatchQueue* queue() { return m_queue; }

private slots:
    void onMoveUp();
    void onMoveDown();
    void onRemove();
    void updateTable();

private:
    void setupUi();
    
    QTableWidget *m_queueTable;
    QPushButton *m_upBtn;
    QPushButton *m_downBtn;
    QPushButton *m_removeBtn;
    QLabel *m_summaryLabel;
    
    UftBatchQueue *m_queue;
};

/**
 * @brief Progress and results page
 */
class UftBatchProgressPage : public QWizardPage
{
    Q_OBJECT
    
public:
    explicit UftBatchProgressPage(QWidget *parent = nullptr);
    
    bool isComplete() const override;
    void initializePage() override;
    void cleanupPage() override;

public slots:
    void startProcessing();
    void pauseProcessing();
    void cancelProcessing();

private slots:
    void onJobStarted(int id);
    void onJobProgress(int id, int percent, const QString &status);
    void onJobComplete(int id, bool success, const QString &message);
    void onAllComplete();

private:
    void setupUi();
    void processNextJob();
    
    /* Progress */
    QProgressBar *m_overallProgress;
    QProgressBar *m_currentProgress;
    QLabel *m_overallLabel;
    QLabel *m_currentLabel;
    QLabel *m_timeLabel;
    
    /* Job list */
    QTableWidget *m_jobTable;
    
    /* Log */
    QTextEdit *m_logView;
    
    /* Controls */
    QPushButton *m_startBtn;
    QPushButton *m_pauseBtn;
    QPushButton *m_cancelBtn;
    QPushButton *m_exportLogBtn;
    
    /* State */
    UftBatchQueue *m_queue;
    bool m_isRunning;
    bool m_isPaused;
    bool m_isCancelled;
    int m_completedJobs;
    int m_failedJobs;
    QDateTime m_startTime;
};

/*===========================================================================
 * Main Batch Wizard
 *===========================================================================*/

/**
 * @brief Batch Processing Wizard
 */
class UftBatchWizard : public QWizard
{
    Q_OBJECT
    
public:
    explicit UftBatchWizard(QWidget *parent = nullptr);
    
    enum PageId {
        Page_Files,
        Page_Operation,
        Page_Output,
        Page_Queue,
        Page_Progress
    };

signals:
    void batchComplete(int successful, int failed);

private:
    void setupPages();
    
    UftBatchFilesPage *m_filesPage;
    UftBatchOperationPage *m_operationPage;
    UftBatchOutputPage *m_outputPage;
    UftBatchQueuePage *m_queuePage;
    UftBatchProgressPage *m_progressPage;
};

/*===========================================================================
 * Batch Worker
 *===========================================================================*/

/**
 * @brief Background batch worker
 */
class UftBatchWorker : public QObject
{
    Q_OBJECT
    
public:
    explicit UftBatchWorker(QObject *parent = nullptr);
    
    void setJob(const UftBatchJob &job);

public slots:
    void process();
    void cancel();

signals:
    void started();
    void progress(int percent, const QString &status);
    void complete(bool success, const QString &message);
    void warning(const QString &message);
    void error(const QString &message);

private:
    void processConvert();
    void processAnalyze();
    void processVerify();
    void processExtract();
    void processHash();
    
    UftBatchJob m_job;
    bool m_cancelled;
};

#endif /* UFT_BATCH_WIZARD_H */

/**
 * @file uft_format_converter_wizard.h
 * @brief Format Converter Wizard (P2-GUI-008)
 * 
 * Step-by-step wizard for converting disk images between formats.
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_FORMAT_CONVERTER_WIZARD_H
#define UFT_FORMAT_CONVERTER_WIZARD_H

#include <QWizard>
#include <QWizardPage>
#include <QListWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QThread>

/* Forward declarations */
class UftConversionWorker;

/*===========================================================================
 * Conversion Options
 *===========================================================================*/

struct UftConversionOptions {
    /* Source */
    QString source_path;
    QString source_format;
    
    /* Target */
    QString target_path;
    QString target_format;
    
    /* Processing */
    bool preserve_weak_bits;
    bool preserve_timing;
    bool preserve_protection;
    bool multi_revolution;
    int  preferred_revolution;
    
    /* Error handling */
    bool retry_bad_sectors;
    int  max_retries;
    bool ignore_crc_errors;
    bool fill_bad_sectors;
    uint8_t fill_byte;
    
    /* Output */
    bool compress_output;
    int  compression_level;
    bool add_metadata;
    QString metadata_comment;
    
    /* Verification */
    bool verify_after_convert;
    bool generate_report;
};

/*===========================================================================
 * Wizard Pages
 *===========================================================================*/

/**
 * @brief Source selection page
 */
class UftSourcePage : public QWizardPage
{
    Q_OBJECT
    
public:
    explicit UftSourcePage(QWidget *parent = nullptr);
    
    bool isComplete() const override;
    bool validatePage() override;
    
    QString getSourcePath() const;
    QString getDetectedFormat() const;

private slots:
    void browseSource();
    void analyzeSource();

private:
    void setupUi();
    
    QLineEdit *m_sourcePath;
    QPushButton *m_browseButton;
    QPushButton *m_analyzeButton;
    
    /* Detection results */
    QGroupBox *m_detectionGroup;
    QLabel *m_formatLabel;
    QLabel *m_sizeLabel;
    QLabel *m_tracksLabel;
    QLabel *m_encodingLabel;
    QLabel *m_qualityLabel;
    QLabel *m_protectionLabel;
    
    /* Preview */
    QTreeWidget *m_contentTree;
    
    QString m_detectedFormat;
};

/**
 * @brief Target format selection page
 */
class UftTargetPage : public QWizardPage
{
    Q_OBJECT
    
public:
    explicit UftTargetPage(QWidget *parent = nullptr);
    
    bool isComplete() const override;
    
    QString getTargetFormat() const;
    QString getTargetPath() const;

private slots:
    void browseTarget();
    void updateExtension();
    void filterFormats(const QString &text);

private:
    void setupUi();
    void populateFormats();
    
    /* Format selection */
    QLineEdit *m_formatFilter;
    QListWidget *m_formatList;
    QLabel *m_formatDescription;
    QLabel *m_compatibilityLabel;
    
    /* Output path */
    QLineEdit *m_targetPath;
    QPushButton *m_browseButton;
    QCheckBox *m_autoExtension;
    
    /* Format categories */
    QButtonGroup *m_categoryGroup;
    QRadioButton *m_catAll;
    QRadioButton *m_catSector;
    QRadioButton *m_catBitstream;
    QRadioButton *m_catFlux;
};

/**
 * @brief Conversion options page
 */
class UftOptionsPage : public QWizardPage
{
    Q_OBJECT
    
public:
    explicit UftOptionsPage(QWidget *parent = nullptr);
    
    UftConversionOptions getOptions() const;
    void setOptions(const UftConversionOptions &opts);

private slots:
    void updatePresets();
    void loadPreset(int index);

private:
    void setupUi();
    void createPreservationGroup();
    void createErrorGroup();
    void createOutputGroup();
    void createVerificationGroup();
    
    /* Presets */
    QComboBox *m_presetCombo;
    
    /* Preservation */
    QGroupBox *m_preserveGroup;
    QCheckBox *m_preserveWeak;
    QCheckBox *m_preserveTiming;
    QCheckBox *m_preserveProtection;
    QCheckBox *m_multiRevolution;
    QSpinBox *m_preferredRev;
    
    /* Error handling */
    QGroupBox *m_errorGroup;
    QCheckBox *m_retryBad;
    QSpinBox *m_maxRetries;
    QCheckBox *m_ignoreCrc;
    QCheckBox *m_fillBad;
    QSpinBox *m_fillByte;
    
    /* Output */
    QGroupBox *m_outputGroup;
    QCheckBox *m_compressOutput;
    QSpinBox *m_compressionLevel;
    QCheckBox *m_addMetadata;
    QLineEdit *m_metadataComment;
    
    /* Verification */
    QGroupBox *m_verifyGroup;
    QCheckBox *m_verifyAfter;
    QCheckBox *m_generateReport;
};

/**
 * @brief Progress and results page
 */
class UftProgressPage : public QWizardPage
{
    Q_OBJECT
    
public:
    explicit UftProgressPage(QWidget *parent = nullptr);
    
    bool isComplete() const override;
    void initializePage() override;
    void cleanupPage() override;

signals:
    void conversionStarted();
    void conversionFinished(bool success);

public slots:
    void startConversion();
    void cancelConversion();

private slots:
    void onProgress(int percent, const QString &status);
    void onTrackProgress(int track, int total, const QString &info);
    void onWarning(const QString &message);
    void onError(const QString &message);
    void onComplete(bool success, const QString &summary);

private:
    void setupUi();
    
    /* Progress */
    QProgressBar *m_overallProgress;
    QProgressBar *m_trackProgress;
    QLabel *m_statusLabel;
    QLabel *m_trackLabel;
    QLabel *m_timeLabel;
    
    /* Log */
    QTextEdit *m_logView;
    
    /* Results */
    QGroupBox *m_resultsGroup;
    QLabel *m_resultIcon;
    QLabel *m_resultSummary;
    QLabel *m_tracksConverted;
    QLabel *m_sectorsGood;
    QLabel *m_sectorsBad;
    QLabel *m_warnings;
    
    /* Control */
    QPushButton *m_cancelButton;
    QPushButton *m_openOutputButton;
    
    /* Worker */
    UftConversionWorker *m_worker;
    QThread *m_workerThread;
    
    bool m_isComplete;
    bool m_success;
};

/*===========================================================================
 * Main Wizard
 *===========================================================================*/

/**
 * @brief Format Converter Wizard
 */
class UftFormatConverterWizard : public QWizard
{
    Q_OBJECT
    
public:
    explicit UftFormatConverterWizard(QWidget *parent = nullptr);
    ~UftFormatConverterWizard();
    
    enum PageId {
        Page_Source,
        Page_Target,
        Page_Options,
        Page_Progress
    };
    
    void setSourceFile(const QString &path);
    UftConversionOptions getOptions() const;

signals:
    void conversionComplete(const QString &outputPath);

private slots:
    void onPageChanged(int id);
    void onConversionFinished(bool success);

private:
    void setupPages();
    void applyStyle();
    
    UftSourcePage *m_sourcePage;
    UftTargetPage *m_targetPage;
    UftOptionsPage *m_optionsPage;
    UftProgressPage *m_progressPage;
};

/*===========================================================================
 * Conversion Worker
 *===========================================================================*/

/**
 * @brief Background conversion worker
 */
class UftConversionWorker : public QObject
{
    Q_OBJECT
    
public:
    explicit UftConversionWorker(QObject *parent = nullptr);
    
    void setOptions(const UftConversionOptions &opts);

public slots:
    void process();
    void cancel();

signals:
    void progress(int percent, const QString &status);
    void trackProgress(int track, int total, const QString &info);
    void warning(const QString &message);
    void error(const QString &message);
    void complete(bool success, const QString &summary);

private:
    UftConversionOptions m_options;
    bool m_cancelled;
};

#endif /* UFT_FORMAT_CONVERTER_WIZARD_H */

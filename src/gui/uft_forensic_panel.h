/**
 * @file uft_forensic_panel.h
 * @brief Forensic Panel - Checksums, Validation, Analysis, Reports
 */

#ifndef UFT_FORENSIC_PANEL_H
#define UFT_FORENSIC_PANEL_H

#include <QWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QProgressBar>

class UftForensicPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftForensicPanel(QWidget *parent = nullptr);

    struct ForensicParams {
        /* Checksums */
        bool calculate_md5;
        bool calculate_sha1;
        bool calculate_sha256;
        bool calculate_crc32;
        bool sector_checksums;
        bool track_checksums;
        
        /* Validation */
        bool validate_structure;
        bool validate_filesystem;
        bool validate_bootblock;
        bool validate_directory;
        bool validate_fat;
        bool validate_bam;
        
        /* Analysis */
        bool analyze_format;
        bool analyze_protection;
        bool analyze_duplicates;
        bool compare_revolutions;
        bool find_hidden_data;
        
        /* Report */
        bool generate_report;
        QString report_format;      /* HTML, JSON, XML, TXT */
        bool include_hex_dump;
        bool include_screenshots;
    };

    ForensicParams getParams() const;
    void setParams(const ForensicParams &params);

signals:
    void paramsChanged();
    void analysisStarted();
    void analysisFinished();

public slots:
    void runAnalysis();
    void generateReport();
    void compareImages();
    void exportResults();

private:
    void setupUi();
    void createChecksumGroup();
    void createValidationGroup();
    void createAnalysisGroup();
    void createReportGroup();
    void createResultsView();

    /* Checksums */
    QGroupBox *m_checksumGroup;
    QCheckBox *m_md5;
    QCheckBox *m_sha1;
    QCheckBox *m_sha256;
    QCheckBox *m_crc32;
    QCheckBox *m_sectorChecksums;
    QCheckBox *m_trackChecksums;
    QLineEdit *m_md5Result;
    QLineEdit *m_sha1Result;
    QLineEdit *m_sha256Result;
    QLineEdit *m_crc32Result;
    
    /* Validation */
    QGroupBox *m_validationGroup;
    QCheckBox *m_validateStructure;
    QCheckBox *m_validateFilesystem;
    QCheckBox *m_validateBootblock;
    QCheckBox *m_validateDirectory;
    QCheckBox *m_validateFat;
    QCheckBox *m_validateBam;
    
    /* Analysis */
    QGroupBox *m_analysisGroup;
    QCheckBox *m_analyzeFormat;
    QCheckBox *m_analyzeProtection;
    QCheckBox *m_analyzeDuplicates;
    QCheckBox *m_compareRevolutions;
    QCheckBox *m_findHiddenData;
    
    /* Report */
    QGroupBox *m_reportGroup;
    QCheckBox *m_generateReport;
    QComboBox *m_reportFormat;
    QCheckBox *m_includeHexDump;
    QCheckBox *m_includeScreenshots;
    QPushButton *m_exportButton;
    
    /* Results */
    QTableWidget *m_resultsTable;
    QPlainTextEdit *m_detailsView;
    QProgressBar *m_analysisProgress;
    QPushButton *m_runButton;
    QPushButton *m_compareButton;
};

#endif /* UFT_FORENSIC_PANEL_H */

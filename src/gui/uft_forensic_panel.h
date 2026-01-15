/**
 * @file uft_forensic_panel.h
 * @brief Forensic Analysis Panel
 */

#ifndef UFT_FORENSIC_PANEL_H
#define UFT_FORENSIC_PANEL_H

#include <QWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QProgressBar>

class UftForensicPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftForensicPanel(QWidget *parent = nullptr);

    struct ForensicParams {
        bool calculate_md5 = true;
        bool calculate_sha1 = true;
        bool calculate_sha256 = false;
        bool calculate_crc32 = true;
        bool validate_structure = true;
        bool validate_filesystem = false;
        bool analyze_format = true;
        bool analyze_protection = false;
        bool generate_report = false;
        QString report_format = "HTML";
    };

    ForensicParams getParams() const;
    void setParams(const ForensicParams &p);
    
    void setImagePath(const QString &path) { m_imagePath = path; }
    QString imagePath() const { return m_imagePath; }

signals:
    void analysisStarted();
    void analysisFinished();
    void analysisComplete(const QString &report);

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

    QString m_imagePath;
    
    QGroupBox *m_checksumGroup;
    QCheckBox *m_md5, *m_sha1, *m_sha256, *m_crc32;
    QLineEdit *m_md5Result, *m_sha1Result, *m_sha256Result, *m_crc32Result;
    QCheckBox *m_sectorChecksums, *m_trackChecksums;

    QGroupBox *m_validationGroup;
    QCheckBox *m_validateStructure, *m_validateFilesystem;

    QGroupBox *m_analysisGroup;
    QCheckBox *m_analyzeFormat, *m_analyzeProtection;
    QComboBox *m_analysisMode;

    QGroupBox *m_reportGroup;
    QCheckBox *m_generateReport;
    QComboBox *m_reportFormat;
    QPushButton *m_runButton, *m_compareButton, *m_exportButton;

    QTableWidget *m_resultsTable;
    QPlainTextEdit *m_detailsView;
    QProgressBar *m_analysisProgress;
};

#endif // UFT_FORENSIC_PANEL_H

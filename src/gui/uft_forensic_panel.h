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

class UftForensicPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftForensicPanel(QWidget *parent = nullptr);

    struct ForensicParams {
        bool md5;
        bool sha1;
        bool sha256;
        bool crc32;
        bool validateStructure;
        bool validateSectors;
        bool validateGaps;
        bool deepAnalysis;
        bool repairAttempt;
    };

    ForensicParams getParams() const;
    void setParams(const ForensicParams &p);

signals:
    void analysisComplete(const QString &report);

private slots:
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

    QGroupBox *m_checksumGroup;
    QCheckBox *m_md5, *m_sha1, *m_sha256, *m_crc32;
    QLineEdit *m_md5Result, *m_sha1Result, *m_sha256Result, *m_crc32Result;
    QCheckBox *m_sectorChecksums, *m_trackChecksums;

    QGroupBox *m_validationGroup;
    QCheckBox *m_validateStructure, *m_validateSectors, *m_validateGaps;

    QGroupBox *m_analysisGroup;
    QCheckBox *m_deepAnalysis, *m_repairAttempt;
    QComboBox *m_analysisMode;

    QGroupBox *m_reportGroup;
    QComboBox *m_reportFormat;
    QPushButton *m_runButton, *m_compareButton, *m_exportButton;

    QTableWidget *m_resultsTable;
    QPlainTextEdit *m_detailsView;
};

#endif // UFT_FORENSIC_PANEL_H

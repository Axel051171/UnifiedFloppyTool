/********************************************************************************
** Form generated from reading UI file 'tab_forensic.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TAB_FORENSIC_H
#define UI_TAB_FORENSIC_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TabForensic
{
public:
    QHBoxLayout *horizontalLayout_main;
    QVBoxLayout *verticalLayout_left;
    QGroupBox *groupChecksums;
    QVBoxLayout *verticalLayout_checksums;
    QHBoxLayout *hboxLayout;
    QCheckBox *checkMD5;
    QCheckBox *checkSHA1;
    QCheckBox *checkSHA256;
    QCheckBox *checkCRC32;
    QFormLayout *formLayout;
    QLabel *labelMD5Result;
    QLineEdit *editMD5;
    QLabel *labelSHA1Result;
    QLineEdit *editSHA1;
    QLabel *labelCRC32Result;
    QLineEdit *editCRC32;
    QCheckBox *checkSectorChecksums;
    QCheckBox *checkTrackChecksums;
    QGroupBox *groupValidation;
    QVBoxLayout *verticalLayout_validation;
    QCheckBox *checkValidateStructure;
    QCheckBox *checkValidateFilesystem;
    QCheckBox *checkValidateBootblock;
    QCheckBox *checkValidateDirectory;
    QCheckBox *checkValidateFAT;
    QGroupBox *groupAnalysis;
    QVBoxLayout *verticalLayout_analysis;
    QCheckBox *checkAnalyzeFormat;
    QCheckBox *checkAnalyzeProtection;
    QCheckBox *checkAnalyzeDuplicates;
    QCheckBox *checkCompareRevolutions;
    QCheckBox *checkFindHiddenData;
    QSpacerItem *spacerItem;
    QVBoxLayout *verticalLayout_right;
    QGroupBox *groupReport;
    QFormLayout *formLayout_report;
    QCheckBox *checkGenerateReport;
    QLabel *labelReportFormat;
    QComboBox *comboReportFormat;
    QCheckBox *checkIncludeHexDump;
    QCheckBox *checkIncludeScreenshots;
    QGroupBox *groupResults;
    QVBoxLayout *vboxLayout;
    QTableWidget *tableResults;
    QGroupBox *groupDetails;
    QVBoxLayout *vboxLayout1;
    QPlainTextEdit *textDetails;
    QHBoxLayout *hboxLayout1;
    QPushButton *btnRunAnalysis;
    QPushButton *btnCompare;
    QPushButton *btnExportReport;

    void setupUi(QWidget *TabForensic)
    {
        if (TabForensic->objectName().isEmpty())
            TabForensic->setObjectName("TabForensic");
        TabForensic->resize(900, 700);
        horizontalLayout_main = new QHBoxLayout(TabForensic);
        horizontalLayout_main->setObjectName("horizontalLayout_main");
        verticalLayout_left = new QVBoxLayout();
        verticalLayout_left->setObjectName("verticalLayout_left");
        groupChecksums = new QGroupBox(TabForensic);
        groupChecksums->setObjectName("groupChecksums");
        verticalLayout_checksums = new QVBoxLayout(groupChecksums);
        verticalLayout_checksums->setObjectName("verticalLayout_checksums");
        hboxLayout = new QHBoxLayout();
        hboxLayout->setObjectName("hboxLayout");
        checkMD5 = new QCheckBox(groupChecksums);
        checkMD5->setObjectName("checkMD5");
        checkMD5->setChecked(true);

        hboxLayout->addWidget(checkMD5);

        checkSHA1 = new QCheckBox(groupChecksums);
        checkSHA1->setObjectName("checkSHA1");
        checkSHA1->setChecked(true);

        hboxLayout->addWidget(checkSHA1);

        checkSHA256 = new QCheckBox(groupChecksums);
        checkSHA256->setObjectName("checkSHA256");

        hboxLayout->addWidget(checkSHA256);

        checkCRC32 = new QCheckBox(groupChecksums);
        checkCRC32->setObjectName("checkCRC32");
        checkCRC32->setChecked(true);

        hboxLayout->addWidget(checkCRC32);


        verticalLayout_checksums->addLayout(hboxLayout);

        formLayout = new QFormLayout();
        formLayout->setObjectName("formLayout");
        labelMD5Result = new QLabel(groupChecksums);
        labelMD5Result->setObjectName("labelMD5Result");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, labelMD5Result);

        editMD5 = new QLineEdit(groupChecksums);
        editMD5->setObjectName("editMD5");
        editMD5->setReadOnly(true);

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, editMD5);

        labelSHA1Result = new QLabel(groupChecksums);
        labelSHA1Result->setObjectName("labelSHA1Result");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, labelSHA1Result);

        editSHA1 = new QLineEdit(groupChecksums);
        editSHA1->setObjectName("editSHA1");
        editSHA1->setReadOnly(true);

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, editSHA1);

        labelCRC32Result = new QLabel(groupChecksums);
        labelCRC32Result->setObjectName("labelCRC32Result");

        formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, labelCRC32Result);

        editCRC32 = new QLineEdit(groupChecksums);
        editCRC32->setObjectName("editCRC32");
        editCRC32->setReadOnly(true);
        editCRC32->setMaximumWidth(150);

        formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, editCRC32);


        verticalLayout_checksums->addLayout(formLayout);

        checkSectorChecksums = new QCheckBox(groupChecksums);
        checkSectorChecksums->setObjectName("checkSectorChecksums");

        verticalLayout_checksums->addWidget(checkSectorChecksums);

        checkTrackChecksums = new QCheckBox(groupChecksums);
        checkTrackChecksums->setObjectName("checkTrackChecksums");

        verticalLayout_checksums->addWidget(checkTrackChecksums);


        verticalLayout_left->addWidget(groupChecksums);

        groupValidation = new QGroupBox(TabForensic);
        groupValidation->setObjectName("groupValidation");
        verticalLayout_validation = new QVBoxLayout(groupValidation);
        verticalLayout_validation->setObjectName("verticalLayout_validation");
        checkValidateStructure = new QCheckBox(groupValidation);
        checkValidateStructure->setObjectName("checkValidateStructure");
        checkValidateStructure->setChecked(true);

        verticalLayout_validation->addWidget(checkValidateStructure);

        checkValidateFilesystem = new QCheckBox(groupValidation);
        checkValidateFilesystem->setObjectName("checkValidateFilesystem");
        checkValidateFilesystem->setChecked(true);

        verticalLayout_validation->addWidget(checkValidateFilesystem);

        checkValidateBootblock = new QCheckBox(groupValidation);
        checkValidateBootblock->setObjectName("checkValidateBootblock");

        verticalLayout_validation->addWidget(checkValidateBootblock);

        checkValidateDirectory = new QCheckBox(groupValidation);
        checkValidateDirectory->setObjectName("checkValidateDirectory");

        verticalLayout_validation->addWidget(checkValidateDirectory);

        checkValidateFAT = new QCheckBox(groupValidation);
        checkValidateFAT->setObjectName("checkValidateFAT");

        verticalLayout_validation->addWidget(checkValidateFAT);


        verticalLayout_left->addWidget(groupValidation);

        groupAnalysis = new QGroupBox(TabForensic);
        groupAnalysis->setObjectName("groupAnalysis");
        verticalLayout_analysis = new QVBoxLayout(groupAnalysis);
        verticalLayout_analysis->setObjectName("verticalLayout_analysis");
        checkAnalyzeFormat = new QCheckBox(groupAnalysis);
        checkAnalyzeFormat->setObjectName("checkAnalyzeFormat");
        checkAnalyzeFormat->setChecked(true);

        verticalLayout_analysis->addWidget(checkAnalyzeFormat);

        checkAnalyzeProtection = new QCheckBox(groupAnalysis);
        checkAnalyzeProtection->setObjectName("checkAnalyzeProtection");
        checkAnalyzeProtection->setChecked(true);

        verticalLayout_analysis->addWidget(checkAnalyzeProtection);

        checkAnalyzeDuplicates = new QCheckBox(groupAnalysis);
        checkAnalyzeDuplicates->setObjectName("checkAnalyzeDuplicates");

        verticalLayout_analysis->addWidget(checkAnalyzeDuplicates);

        checkCompareRevolutions = new QCheckBox(groupAnalysis);
        checkCompareRevolutions->setObjectName("checkCompareRevolutions");

        verticalLayout_analysis->addWidget(checkCompareRevolutions);

        checkFindHiddenData = new QCheckBox(groupAnalysis);
        checkFindHiddenData->setObjectName("checkFindHiddenData");

        verticalLayout_analysis->addWidget(checkFindHiddenData);


        verticalLayout_left->addWidget(groupAnalysis);

        spacerItem = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_left->addItem(spacerItem);


        horizontalLayout_main->addLayout(verticalLayout_left);

        verticalLayout_right = new QVBoxLayout();
        verticalLayout_right->setObjectName("verticalLayout_right");
        groupReport = new QGroupBox(TabForensic);
        groupReport->setObjectName("groupReport");
        formLayout_report = new QFormLayout(groupReport);
        formLayout_report->setObjectName("formLayout_report");
        checkGenerateReport = new QCheckBox(groupReport);
        checkGenerateReport->setObjectName("checkGenerateReport");
        checkGenerateReport->setChecked(true);

        formLayout_report->setWidget(0, QFormLayout::ItemRole::SpanningRole, checkGenerateReport);

        labelReportFormat = new QLabel(groupReport);
        labelReportFormat->setObjectName("labelReportFormat");

        formLayout_report->setWidget(1, QFormLayout::ItemRole::LabelRole, labelReportFormat);

        comboReportFormat = new QComboBox(groupReport);
        comboReportFormat->addItem(QString());
        comboReportFormat->addItem(QString());
        comboReportFormat->addItem(QString());
        comboReportFormat->addItem(QString());
        comboReportFormat->setObjectName("comboReportFormat");
        comboReportFormat->setMaximumWidth(150);

        formLayout_report->setWidget(1, QFormLayout::ItemRole::FieldRole, comboReportFormat);

        checkIncludeHexDump = new QCheckBox(groupReport);
        checkIncludeHexDump->setObjectName("checkIncludeHexDump");

        formLayout_report->setWidget(2, QFormLayout::ItemRole::SpanningRole, checkIncludeHexDump);

        checkIncludeScreenshots = new QCheckBox(groupReport);
        checkIncludeScreenshots->setObjectName("checkIncludeScreenshots");

        formLayout_report->setWidget(3, QFormLayout::ItemRole::SpanningRole, checkIncludeScreenshots);


        verticalLayout_right->addWidget(groupReport);

        groupResults = new QGroupBox(TabForensic);
        groupResults->setObjectName("groupResults");
        vboxLayout = new QVBoxLayout(groupResults);
        vboxLayout->setObjectName("vboxLayout");
        tableResults = new QTableWidget(groupResults);
        if (tableResults->columnCount() < 3)
            tableResults->setColumnCount(3);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        tableResults->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        tableResults->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        tableResults->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        tableResults->setObjectName("tableResults");
        tableResults->setColumnCount(3);

        vboxLayout->addWidget(tableResults);


        verticalLayout_right->addWidget(groupResults);

        groupDetails = new QGroupBox(TabForensic);
        groupDetails->setObjectName("groupDetails");
        vboxLayout1 = new QVBoxLayout(groupDetails);
        vboxLayout1->setObjectName("vboxLayout1");
        textDetails = new QPlainTextEdit(groupDetails);
        textDetails->setObjectName("textDetails");
        textDetails->setReadOnly(true);
        textDetails->setMaximumHeight(120);

        vboxLayout1->addWidget(textDetails);


        verticalLayout_right->addWidget(groupDetails);

        hboxLayout1 = new QHBoxLayout();
        hboxLayout1->setObjectName("hboxLayout1");
        btnRunAnalysis = new QPushButton(TabForensic);
        btnRunAnalysis->setObjectName("btnRunAnalysis");

        hboxLayout1->addWidget(btnRunAnalysis);

        btnCompare = new QPushButton(TabForensic);
        btnCompare->setObjectName("btnCompare");

        hboxLayout1->addWidget(btnCompare);

        btnExportReport = new QPushButton(TabForensic);
        btnExportReport->setObjectName("btnExportReport");

        hboxLayout1->addWidget(btnExportReport);


        verticalLayout_right->addLayout(hboxLayout1);


        horizontalLayout_main->addLayout(verticalLayout_right);


        retranslateUi(TabForensic);

        QMetaObject::connectSlotsByName(TabForensic);
    } // setupUi

    void retranslateUi(QWidget *TabForensic)
    {
        groupChecksums->setTitle(QCoreApplication::translate("TabForensic", "Checksums", nullptr));
        checkMD5->setText(QCoreApplication::translate("TabForensic", "MD5", nullptr));
        checkSHA1->setText(QCoreApplication::translate("TabForensic", "SHA-1", nullptr));
        checkSHA256->setText(QCoreApplication::translate("TabForensic", "SHA-256", nullptr));
        checkCRC32->setText(QCoreApplication::translate("TabForensic", "CRC32", nullptr));
        labelMD5Result->setText(QCoreApplication::translate("TabForensic", "MD5:", nullptr));
        editMD5->setPlaceholderText(QCoreApplication::translate("TabForensic", "Not calculated", nullptr));
        labelSHA1Result->setText(QCoreApplication::translate("TabForensic", "SHA-1:", nullptr));
        editSHA1->setPlaceholderText(QCoreApplication::translate("TabForensic", "Not calculated", nullptr));
        labelCRC32Result->setText(QCoreApplication::translate("TabForensic", "CRC32:", nullptr));
        editCRC32->setPlaceholderText(QCoreApplication::translate("TabForensic", "Not calculated", nullptr));
        checkSectorChecksums->setText(QCoreApplication::translate("TabForensic", "Calculate per-sector checksums", nullptr));
        checkTrackChecksums->setText(QCoreApplication::translate("TabForensic", "Calculate per-track checksums", nullptr));
        groupValidation->setTitle(QCoreApplication::translate("TabForensic", "Validation", nullptr));
        checkValidateStructure->setText(QCoreApplication::translate("TabForensic", "Validate disk structure", nullptr));
        checkValidateFilesystem->setText(QCoreApplication::translate("TabForensic", "Validate filesystem", nullptr));
        checkValidateBootblock->setText(QCoreApplication::translate("TabForensic", "Validate boot block", nullptr));
        checkValidateDirectory->setText(QCoreApplication::translate("TabForensic", "Validate directory", nullptr));
        checkValidateFAT->setText(QCoreApplication::translate("TabForensic", "Validate FAT/BAM", nullptr));
        groupAnalysis->setTitle(QCoreApplication::translate("TabForensic", "Analysis", nullptr));
        checkAnalyzeFormat->setText(QCoreApplication::translate("TabForensic", "Analyze format", nullptr));
        checkAnalyzeProtection->setText(QCoreApplication::translate("TabForensic", "Analyze copy protection", nullptr));
        checkAnalyzeDuplicates->setText(QCoreApplication::translate("TabForensic", "Find duplicate sectors", nullptr));
        checkCompareRevolutions->setText(QCoreApplication::translate("TabForensic", "Compare revolutions", nullptr));
        checkFindHiddenData->setText(QCoreApplication::translate("TabForensic", "Find hidden data", nullptr));
        groupReport->setTitle(QCoreApplication::translate("TabForensic", "Report", nullptr));
        checkGenerateReport->setText(QCoreApplication::translate("TabForensic", "Generate report", nullptr));
        labelReportFormat->setText(QCoreApplication::translate("TabForensic", "Format:", nullptr));
        comboReportFormat->setItemText(0, QCoreApplication::translate("TabForensic", "HTML", nullptr));
        comboReportFormat->setItemText(1, QCoreApplication::translate("TabForensic", "JSON", nullptr));
        comboReportFormat->setItemText(2, QCoreApplication::translate("TabForensic", "XML", nullptr));
        comboReportFormat->setItemText(3, QCoreApplication::translate("TabForensic", "TXT", nullptr));

        checkIncludeHexDump->setText(QCoreApplication::translate("TabForensic", "Include hex dump", nullptr));
        checkIncludeScreenshots->setText(QCoreApplication::translate("TabForensic", "Include track screenshots", nullptr));
        groupResults->setTitle(QCoreApplication::translate("TabForensic", "Results", nullptr));
        QTableWidgetItem *___qtablewidgetitem = tableResults->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("TabForensic", "Check", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = tableResults->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("TabForensic", "Result", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = tableResults->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("TabForensic", "Details", nullptr));
        groupDetails->setTitle(QCoreApplication::translate("TabForensic", "Details", nullptr));
        btnRunAnalysis->setText(QCoreApplication::translate("TabForensic", "\342\226\266 Run Analysis", nullptr));
        btnRunAnalysis->setStyleSheet(QCoreApplication::translate("TabForensic", "background-color: #2196F3; color: white; font-weight: bold; padding: 10px;", nullptr));
        btnCompare->setText(QCoreApplication::translate("TabForensic", "Compare...", nullptr));
        btnExportReport->setText(QCoreApplication::translate("TabForensic", "Export Report", nullptr));
        (void)TabForensic;
    } // retranslateUi

};

namespace Ui {
    class TabForensic: public Ui_TabForensic {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TAB_FORENSIC_H

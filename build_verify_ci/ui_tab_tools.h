/********************************************************************************
** Form generated from reading UI file 'tab_tools.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TAB_TOOLS_H
#define UI_TAB_TOOLS_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TabTools
{
public:
    QVBoxLayout *verticalLayout_main;
    QScrollArea *scrollArea;
    QWidget *scrollContent;
    QHBoxLayout *mainHLayout;
    QVBoxLayout *col1;
    QGroupBox *groupConvert;
    QVBoxLayout *vboxLayout;
    QHBoxLayout *hboxLayout;
    QLabel *label;
    QLineEdit *editConvertSource;
    QPushButton *btnBrowseConvertSource;
    QHBoxLayout *hboxLayout1;
    QLabel *label1;
    QLineEdit *editConvertTarget;
    QPushButton *btnBrowseConvertTarget;
    QHBoxLayout *hboxLayout2;
    QLabel *label2;
    QComboBox *comboConvertFrom;
    QLabel *label3;
    QComboBox *comboConvertTo;
    QPushButton *btnConvert;
    QGroupBox *groupCompare;
    QVBoxLayout *vboxLayout1;
    QHBoxLayout *hboxLayout3;
    QLabel *label4;
    QLineEdit *editCompareA;
    QPushButton *btnBrowseCompareA;
    QHBoxLayout *hboxLayout4;
    QLabel *label5;
    QLineEdit *editCompareB;
    QPushButton *btnBrowseCompareB;
    QHBoxLayout *hboxLayout5;
    QLabel *label6;
    QComboBox *comboCompareMode;
    QHBoxLayout *hboxLayout6;
    QCheckBox *checkIgnoreHeaders;
    QCheckBox *checkShowHex;
    QPushButton *btnCompare;
    QSpacerItem *spacerItem;
    QVBoxLayout *col2;
    QGroupBox *groupRepair;
    QVBoxLayout *vboxLayout2;
    QHBoxLayout *hboxLayout7;
    QLabel *label7;
    QLineEdit *editRepairFile;
    QPushButton *btnBrowseRepair;
    QLabel *label8;
    QGridLayout *gridLayout;
    QCheckBox *checkFixChecksum;
    QCheckBox *checkFixBAM;
    QCheckBox *checkFixHeaders;
    QCheckBox *checkRecoverDeleted;
    QCheckBox *checkFillBadSectors;
    QCheckBox *checkBackup;
    QPushButton *btnRepair;
    QGroupBox *groupAnalyze;
    QVBoxLayout *vboxLayout3;
    QHBoxLayout *hboxLayout8;
    QLabel *label9;
    QLineEdit *editAnalyzeFile;
    QPushButton *btnBrowseAnalyze;
    QGridLayout *gridLayout1;
    QCheckBox *checkDetectFormat;
    QCheckBox *checkDetectProtection;
    QCheckBox *checkShowDirectory;
    QCheckBox *checkShowBadSectors;
    QCheckBox *checkCalcHashes;
    QCheckBox *checkHexDump;
    QPushButton *btnAnalyze;
    QSpacerItem *spacerItem1;
    QVBoxLayout *col3;
    QGroupBox *groupBatch;
    QVBoxLayout *vboxLayout4;
    QHBoxLayout *hboxLayout9;
    QLabel *label10;
    QLineEdit *editBatchFolder;
    QPushButton *btnBrowseBatch;
    QHBoxLayout *hboxLayout10;
    QLabel *label11;
    QLineEdit *editBatchFilter;
    QHBoxLayout *hboxLayout11;
    QLabel *label12;
    QComboBox *comboBatchAction;
    QHBoxLayout *hboxLayout12;
    QCheckBox *checkBatchSubfolders;
    QCheckBox *checkBatchLog;
    QHBoxLayout *hboxLayout13;
    QPushButton *btnBatchStart;
    QPushButton *btnBatchStop;
    QGroupBox *groupQuickTools;
    QGridLayout *gridLayout2;
    QPushButton *btnHexView;
    QPushButton *btnTrackView;
    QPushButton *btnFluxView;
    QPushButton *btnSectorEdit;
    QPushButton *btnCreateBlank;
    QPushButton *btnDiskInfo;
    QSpacerItem *spacerItem2;
    QGroupBox *groupOutput;
    QVBoxLayout *vboxLayout5;
    QPlainTextEdit *textOutput;
    QHBoxLayout *hboxLayout14;
    QProgressBar *progressBar;
    QPushButton *btnClearOutput;
    QPushButton *btnSaveOutput;

    void setupUi(QWidget *TabTools)
    {
        if (TabTools->objectName().isEmpty())
            TabTools->setObjectName("TabTools");
        TabTools->resize(900, 600);
        verticalLayout_main = new QVBoxLayout(TabTools);
        verticalLayout_main->setObjectName("verticalLayout_main");
        verticalLayout_main->setContentsMargins(4, 4, 4, 4);
        scrollArea = new QScrollArea(TabTools);
        scrollArea->setObjectName("scrollArea");
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollContent = new QWidget();
        scrollContent->setObjectName("scrollContent");
        mainHLayout = new QHBoxLayout(scrollContent);
        mainHLayout->setSpacing(8);
        mainHLayout->setObjectName("mainHLayout");
        col1 = new QVBoxLayout();
        col1->setSpacing(6);
        col1->setObjectName("col1");
        groupConvert = new QGroupBox(scrollContent);
        groupConvert->setObjectName("groupConvert");
        vboxLayout = new QVBoxLayout(groupConvert);
        vboxLayout->setSpacing(4);
        vboxLayout->setObjectName("vboxLayout");
        hboxLayout = new QHBoxLayout();
        hboxLayout->setObjectName("hboxLayout");
        label = new QLabel(groupConvert);
        label->setObjectName("label");

        hboxLayout->addWidget(label);

        editConvertSource = new QLineEdit(groupConvert);
        editConvertSource->setObjectName("editConvertSource");

        hboxLayout->addWidget(editConvertSource);

        btnBrowseConvertSource = new QPushButton(groupConvert);
        btnBrowseConvertSource->setObjectName("btnBrowseConvertSource");
        btnBrowseConvertSource->setMaximumWidth(30);

        hboxLayout->addWidget(btnBrowseConvertSource);


        vboxLayout->addLayout(hboxLayout);

        hboxLayout1 = new QHBoxLayout();
        hboxLayout1->setObjectName("hboxLayout1");
        label1 = new QLabel(groupConvert);
        label1->setObjectName("label1");

        hboxLayout1->addWidget(label1);

        editConvertTarget = new QLineEdit(groupConvert);
        editConvertTarget->setObjectName("editConvertTarget");

        hboxLayout1->addWidget(editConvertTarget);

        btnBrowseConvertTarget = new QPushButton(groupConvert);
        btnBrowseConvertTarget->setObjectName("btnBrowseConvertTarget");
        btnBrowseConvertTarget->setMaximumWidth(30);

        hboxLayout1->addWidget(btnBrowseConvertTarget);


        vboxLayout->addLayout(hboxLayout1);

        hboxLayout2 = new QHBoxLayout();
        hboxLayout2->setObjectName("hboxLayout2");
        label2 = new QLabel(groupConvert);
        label2->setObjectName("label2");

        hboxLayout2->addWidget(label2);

        comboConvertFrom = new QComboBox(groupConvert);
        comboConvertFrom->addItem(QString());
        comboConvertFrom->addItem(QString());
        comboConvertFrom->addItem(QString());
        comboConvertFrom->addItem(QString());
        comboConvertFrom->addItem(QString());
        comboConvertFrom->addItem(QString());
        comboConvertFrom->addItem(QString());
        comboConvertFrom->addItem(QString());
        comboConvertFrom->setObjectName("comboConvertFrom");

        hboxLayout2->addWidget(comboConvertFrom);

        label3 = new QLabel(groupConvert);
        label3->setObjectName("label3");

        hboxLayout2->addWidget(label3);

        comboConvertTo = new QComboBox(groupConvert);
        comboConvertTo->addItem(QString());
        comboConvertTo->addItem(QString());
        comboConvertTo->addItem(QString());
        comboConvertTo->addItem(QString());
        comboConvertTo->addItem(QString());
        comboConvertTo->addItem(QString());
        comboConvertTo->addItem(QString());
        comboConvertTo->setObjectName("comboConvertTo");

        hboxLayout2->addWidget(comboConvertTo);


        vboxLayout->addLayout(hboxLayout2);

        btnConvert = new QPushButton(groupConvert);
        btnConvert->setObjectName("btnConvert");

        vboxLayout->addWidget(btnConvert);


        col1->addWidget(groupConvert);

        groupCompare = new QGroupBox(scrollContent);
        groupCompare->setObjectName("groupCompare");
        vboxLayout1 = new QVBoxLayout(groupCompare);
        vboxLayout1->setSpacing(4);
        vboxLayout1->setObjectName("vboxLayout1");
        hboxLayout3 = new QHBoxLayout();
        hboxLayout3->setObjectName("hboxLayout3");
        label4 = new QLabel(groupCompare);
        label4->setObjectName("label4");

        hboxLayout3->addWidget(label4);

        editCompareA = new QLineEdit(groupCompare);
        editCompareA->setObjectName("editCompareA");

        hboxLayout3->addWidget(editCompareA);

        btnBrowseCompareA = new QPushButton(groupCompare);
        btnBrowseCompareA->setObjectName("btnBrowseCompareA");
        btnBrowseCompareA->setMaximumWidth(30);

        hboxLayout3->addWidget(btnBrowseCompareA);


        vboxLayout1->addLayout(hboxLayout3);

        hboxLayout4 = new QHBoxLayout();
        hboxLayout4->setObjectName("hboxLayout4");
        label5 = new QLabel(groupCompare);
        label5->setObjectName("label5");

        hboxLayout4->addWidget(label5);

        editCompareB = new QLineEdit(groupCompare);
        editCompareB->setObjectName("editCompareB");

        hboxLayout4->addWidget(editCompareB);

        btnBrowseCompareB = new QPushButton(groupCompare);
        btnBrowseCompareB->setObjectName("btnBrowseCompareB");
        btnBrowseCompareB->setMaximumWidth(30);

        hboxLayout4->addWidget(btnBrowseCompareB);


        vboxLayout1->addLayout(hboxLayout4);

        hboxLayout5 = new QHBoxLayout();
        hboxLayout5->setObjectName("hboxLayout5");
        label6 = new QLabel(groupCompare);
        label6->setObjectName("label6");

        hboxLayout5->addWidget(label6);

        comboCompareMode = new QComboBox(groupCompare);
        comboCompareMode->addItem(QString());
        comboCompareMode->addItem(QString());
        comboCompareMode->addItem(QString());
        comboCompareMode->addItem(QString());
        comboCompareMode->setObjectName("comboCompareMode");

        hboxLayout5->addWidget(comboCompareMode);


        vboxLayout1->addLayout(hboxLayout5);

        hboxLayout6 = new QHBoxLayout();
        hboxLayout6->setObjectName("hboxLayout6");
        checkIgnoreHeaders = new QCheckBox(groupCompare);
        checkIgnoreHeaders->setObjectName("checkIgnoreHeaders");

        hboxLayout6->addWidget(checkIgnoreHeaders);

        checkShowHex = new QCheckBox(groupCompare);
        checkShowHex->setObjectName("checkShowHex");

        hboxLayout6->addWidget(checkShowHex);


        vboxLayout1->addLayout(hboxLayout6);

        btnCompare = new QPushButton(groupCompare);
        btnCompare->setObjectName("btnCompare");

        vboxLayout1->addWidget(btnCompare);


        col1->addWidget(groupCompare);

        spacerItem = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        col1->addItem(spacerItem);


        mainHLayout->addLayout(col1);

        col2 = new QVBoxLayout();
        col2->setSpacing(6);
        col2->setObjectName("col2");
        groupRepair = new QGroupBox(scrollContent);
        groupRepair->setObjectName("groupRepair");
        vboxLayout2 = new QVBoxLayout(groupRepair);
        vboxLayout2->setSpacing(4);
        vboxLayout2->setObjectName("vboxLayout2");
        hboxLayout7 = new QHBoxLayout();
        hboxLayout7->setObjectName("hboxLayout7");
        label7 = new QLabel(groupRepair);
        label7->setObjectName("label7");

        hboxLayout7->addWidget(label7);

        editRepairFile = new QLineEdit(groupRepair);
        editRepairFile->setObjectName("editRepairFile");

        hboxLayout7->addWidget(editRepairFile);

        btnBrowseRepair = new QPushButton(groupRepair);
        btnBrowseRepair->setObjectName("btnBrowseRepair");
        btnBrowseRepair->setMaximumWidth(30);

        hboxLayout7->addWidget(btnBrowseRepair);


        vboxLayout2->addLayout(hboxLayout7);

        label8 = new QLabel(groupRepair);
        label8->setObjectName("label8");

        vboxLayout2->addWidget(label8);

        gridLayout = new QGridLayout();
        gridLayout->setObjectName("gridLayout");
        checkFixChecksum = new QCheckBox(groupRepair);
        checkFixChecksum->setObjectName("checkFixChecksum");
        checkFixChecksum->setChecked(true);

        gridLayout->addWidget(checkFixChecksum, 0, 0, 1, 1);

        checkFixBAM = new QCheckBox(groupRepair);
        checkFixBAM->setObjectName("checkFixBAM");
        checkFixBAM->setChecked(true);

        gridLayout->addWidget(checkFixBAM, 0, 1, 1, 1);

        checkFixHeaders = new QCheckBox(groupRepair);
        checkFixHeaders->setObjectName("checkFixHeaders");

        gridLayout->addWidget(checkFixHeaders, 1, 0, 1, 1);

        checkRecoverDeleted = new QCheckBox(groupRepair);
        checkRecoverDeleted->setObjectName("checkRecoverDeleted");

        gridLayout->addWidget(checkRecoverDeleted, 1, 1, 1, 1);

        checkFillBadSectors = new QCheckBox(groupRepair);
        checkFillBadSectors->setObjectName("checkFillBadSectors");

        gridLayout->addWidget(checkFillBadSectors, 2, 0, 1, 1);

        checkBackup = new QCheckBox(groupRepair);
        checkBackup->setObjectName("checkBackup");
        checkBackup->setChecked(true);

        gridLayout->addWidget(checkBackup, 2, 1, 1, 1);


        vboxLayout2->addLayout(gridLayout);

        btnRepair = new QPushButton(groupRepair);
        btnRepair->setObjectName("btnRepair");

        vboxLayout2->addWidget(btnRepair);


        col2->addWidget(groupRepair);

        groupAnalyze = new QGroupBox(scrollContent);
        groupAnalyze->setObjectName("groupAnalyze");
        vboxLayout3 = new QVBoxLayout(groupAnalyze);
        vboxLayout3->setSpacing(4);
        vboxLayout3->setObjectName("vboxLayout3");
        hboxLayout8 = new QHBoxLayout();
        hboxLayout8->setObjectName("hboxLayout8");
        label9 = new QLabel(groupAnalyze);
        label9->setObjectName("label9");

        hboxLayout8->addWidget(label9);

        editAnalyzeFile = new QLineEdit(groupAnalyze);
        editAnalyzeFile->setObjectName("editAnalyzeFile");

        hboxLayout8->addWidget(editAnalyzeFile);

        btnBrowseAnalyze = new QPushButton(groupAnalyze);
        btnBrowseAnalyze->setObjectName("btnBrowseAnalyze");
        btnBrowseAnalyze->setMaximumWidth(30);

        hboxLayout8->addWidget(btnBrowseAnalyze);


        vboxLayout3->addLayout(hboxLayout8);

        gridLayout1 = new QGridLayout();
        gridLayout1->setObjectName("gridLayout1");
        checkDetectFormat = new QCheckBox(groupAnalyze);
        checkDetectFormat->setObjectName("checkDetectFormat");
        checkDetectFormat->setChecked(true);

        gridLayout1->addWidget(checkDetectFormat, 0, 0, 1, 1);

        checkDetectProtection = new QCheckBox(groupAnalyze);
        checkDetectProtection->setObjectName("checkDetectProtection");
        checkDetectProtection->setChecked(true);

        gridLayout1->addWidget(checkDetectProtection, 0, 1, 1, 1);

        checkShowDirectory = new QCheckBox(groupAnalyze);
        checkShowDirectory->setObjectName("checkShowDirectory");
        checkShowDirectory->setChecked(true);

        gridLayout1->addWidget(checkShowDirectory, 1, 0, 1, 1);

        checkShowBadSectors = new QCheckBox(groupAnalyze);
        checkShowBadSectors->setObjectName("checkShowBadSectors");

        gridLayout1->addWidget(checkShowBadSectors, 1, 1, 1, 1);

        checkCalcHashes = new QCheckBox(groupAnalyze);
        checkCalcHashes->setObjectName("checkCalcHashes");

        gridLayout1->addWidget(checkCalcHashes, 2, 0, 1, 1);

        checkHexDump = new QCheckBox(groupAnalyze);
        checkHexDump->setObjectName("checkHexDump");

        gridLayout1->addWidget(checkHexDump, 2, 1, 1, 1);


        vboxLayout3->addLayout(gridLayout1);

        btnAnalyze = new QPushButton(groupAnalyze);
        btnAnalyze->setObjectName("btnAnalyze");

        vboxLayout3->addWidget(btnAnalyze);


        col2->addWidget(groupAnalyze);

        spacerItem1 = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        col2->addItem(spacerItem1);


        mainHLayout->addLayout(col2);

        col3 = new QVBoxLayout();
        col3->setSpacing(6);
        col3->setObjectName("col3");
        groupBatch = new QGroupBox(scrollContent);
        groupBatch->setObjectName("groupBatch");
        vboxLayout4 = new QVBoxLayout(groupBatch);
        vboxLayout4->setSpacing(4);
        vboxLayout4->setObjectName("vboxLayout4");
        hboxLayout9 = new QHBoxLayout();
        hboxLayout9->setObjectName("hboxLayout9");
        label10 = new QLabel(groupBatch);
        label10->setObjectName("label10");

        hboxLayout9->addWidget(label10);

        editBatchFolder = new QLineEdit(groupBatch);
        editBatchFolder->setObjectName("editBatchFolder");

        hboxLayout9->addWidget(editBatchFolder);

        btnBrowseBatch = new QPushButton(groupBatch);
        btnBrowseBatch->setObjectName("btnBrowseBatch");
        btnBrowseBatch->setMaximumWidth(30);

        hboxLayout9->addWidget(btnBrowseBatch);


        vboxLayout4->addLayout(hboxLayout9);

        hboxLayout10 = new QHBoxLayout();
        hboxLayout10->setObjectName("hboxLayout10");
        label11 = new QLabel(groupBatch);
        label11->setObjectName("label11");

        hboxLayout10->addWidget(label11);

        editBatchFilter = new QLineEdit(groupBatch);
        editBatchFilter->setObjectName("editBatchFilter");

        hboxLayout10->addWidget(editBatchFilter);


        vboxLayout4->addLayout(hboxLayout10);

        hboxLayout11 = new QHBoxLayout();
        hboxLayout11->setObjectName("hboxLayout11");
        label12 = new QLabel(groupBatch);
        label12->setObjectName("label12");

        hboxLayout11->addWidget(label12);

        comboBatchAction = new QComboBox(groupBatch);
        comboBatchAction->addItem(QString());
        comboBatchAction->addItem(QString());
        comboBatchAction->addItem(QString());
        comboBatchAction->addItem(QString());
        comboBatchAction->addItem(QString());
        comboBatchAction->setObjectName("comboBatchAction");

        hboxLayout11->addWidget(comboBatchAction);


        vboxLayout4->addLayout(hboxLayout11);

        hboxLayout12 = new QHBoxLayout();
        hboxLayout12->setObjectName("hboxLayout12");
        checkBatchSubfolders = new QCheckBox(groupBatch);
        checkBatchSubfolders->setObjectName("checkBatchSubfolders");

        hboxLayout12->addWidget(checkBatchSubfolders);

        checkBatchLog = new QCheckBox(groupBatch);
        checkBatchLog->setObjectName("checkBatchLog");
        checkBatchLog->setChecked(true);

        hboxLayout12->addWidget(checkBatchLog);


        vboxLayout4->addLayout(hboxLayout12);

        hboxLayout13 = new QHBoxLayout();
        hboxLayout13->setObjectName("hboxLayout13");
        btnBatchStart = new QPushButton(groupBatch);
        btnBatchStart->setObjectName("btnBatchStart");

        hboxLayout13->addWidget(btnBatchStart);

        btnBatchStop = new QPushButton(groupBatch);
        btnBatchStop->setObjectName("btnBatchStop");
        btnBatchStop->setEnabled(false);

        hboxLayout13->addWidget(btnBatchStop);


        vboxLayout4->addLayout(hboxLayout13);


        col3->addWidget(groupBatch);

        groupQuickTools = new QGroupBox(scrollContent);
        groupQuickTools->setObjectName("groupQuickTools");
        gridLayout2 = new QGridLayout(groupQuickTools);
        gridLayout2->setObjectName("gridLayout2");
        gridLayout2->setVerticalSpacing(4);
        btnHexView = new QPushButton(groupQuickTools);
        btnHexView->setObjectName("btnHexView");

        gridLayout2->addWidget(btnHexView, 0, 0, 1, 1);

        btnTrackView = new QPushButton(groupQuickTools);
        btnTrackView->setObjectName("btnTrackView");

        gridLayout2->addWidget(btnTrackView, 0, 1, 1, 1);

        btnFluxView = new QPushButton(groupQuickTools);
        btnFluxView->setObjectName("btnFluxView");

        gridLayout2->addWidget(btnFluxView, 1, 0, 1, 1);

        btnSectorEdit = new QPushButton(groupQuickTools);
        btnSectorEdit->setObjectName("btnSectorEdit");

        gridLayout2->addWidget(btnSectorEdit, 1, 1, 1, 1);

        btnCreateBlank = new QPushButton(groupQuickTools);
        btnCreateBlank->setObjectName("btnCreateBlank");

        gridLayout2->addWidget(btnCreateBlank, 2, 0, 1, 1);

        btnDiskInfo = new QPushButton(groupQuickTools);
        btnDiskInfo->setObjectName("btnDiskInfo");

        gridLayout2->addWidget(btnDiskInfo, 2, 1, 1, 1);


        col3->addWidget(groupQuickTools);

        spacerItem2 = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        col3->addItem(spacerItem2);


        mainHLayout->addLayout(col3);

        scrollArea->setWidget(scrollContent);

        verticalLayout_main->addWidget(scrollArea);

        groupOutput = new QGroupBox(TabTools);
        groupOutput->setObjectName("groupOutput");
        groupOutput->setMaximumHeight(150);
        vboxLayout5 = new QVBoxLayout(groupOutput);
        vboxLayout5->setObjectName("vboxLayout5");
        textOutput = new QPlainTextEdit(groupOutput);
        textOutput->setObjectName("textOutput");
        textOutput->setReadOnly(true);

        vboxLayout5->addWidget(textOutput);

        hboxLayout14 = new QHBoxLayout();
        hboxLayout14->setObjectName("hboxLayout14");
        progressBar = new QProgressBar(groupOutput);
        progressBar->setObjectName("progressBar");
        progressBar->setValue(0);

        hboxLayout14->addWidget(progressBar);

        btnClearOutput = new QPushButton(groupOutput);
        btnClearOutput->setObjectName("btnClearOutput");
        btnClearOutput->setMaximumWidth(60);

        hboxLayout14->addWidget(btnClearOutput);

        btnSaveOutput = new QPushButton(groupOutput);
        btnSaveOutput->setObjectName("btnSaveOutput");
        btnSaveOutput->setMaximumWidth(60);

        hboxLayout14->addWidget(btnSaveOutput);


        vboxLayout5->addLayout(hboxLayout14);


        verticalLayout_main->addWidget(groupOutput);


        retranslateUi(TabTools);

        QMetaObject::connectSlotsByName(TabTools);
    } // setupUi

    void retranslateUi(QWidget *TabTools)
    {
        groupConvert->setTitle(QCoreApplication::translate("TabTools", "Convert", nullptr));
        groupConvert->setStyleSheet(QCoreApplication::translate("TabTools", "QGroupBox { font-weight: bold; }", nullptr));
        label->setText(QCoreApplication::translate("TabTools", "Source:", nullptr));
        editConvertSource->setPlaceholderText(QCoreApplication::translate("TabTools", "Input file...", nullptr));
        btnBrowseConvertSource->setText(QCoreApplication::translate("TabTools", "...", nullptr));
        label1->setText(QCoreApplication::translate("TabTools", "Target:", nullptr));
        editConvertTarget->setPlaceholderText(QCoreApplication::translate("TabTools", "Output file...", nullptr));
        btnBrowseConvertTarget->setText(QCoreApplication::translate("TabTools", "...", nullptr));
        label2->setText(QCoreApplication::translate("TabTools", "From:", nullptr));
        comboConvertFrom->setItemText(0, QCoreApplication::translate("TabTools", "Auto-Detect", nullptr));
        comboConvertFrom->setItemText(1, QCoreApplication::translate("TabTools", "ADF", nullptr));
        comboConvertFrom->setItemText(2, QCoreApplication::translate("TabTools", "D64", nullptr));
        comboConvertFrom->setItemText(3, QCoreApplication::translate("TabTools", "G64", nullptr));
        comboConvertFrom->setItemText(4, QCoreApplication::translate("TabTools", "SCP", nullptr));
        comboConvertFrom->setItemText(5, QCoreApplication::translate("TabTools", "HFE", nullptr));
        comboConvertFrom->setItemText(6, QCoreApplication::translate("TabTools", "IMG", nullptr));
        comboConvertFrom->setItemText(7, QCoreApplication::translate("TabTools", "IMD", nullptr));

        label3->setText(QCoreApplication::translate("TabTools", "To:", nullptr));
        comboConvertTo->setItemText(0, QCoreApplication::translate("TabTools", "ADF", nullptr));
        comboConvertTo->setItemText(1, QCoreApplication::translate("TabTools", "D64", nullptr));
        comboConvertTo->setItemText(2, QCoreApplication::translate("TabTools", "G64", nullptr));
        comboConvertTo->setItemText(3, QCoreApplication::translate("TabTools", "SCP", nullptr));
        comboConvertTo->setItemText(4, QCoreApplication::translate("TabTools", "HFE", nullptr));
        comboConvertTo->setItemText(5, QCoreApplication::translate("TabTools", "IMG", nullptr));
        comboConvertTo->setItemText(6, QCoreApplication::translate("TabTools", "IMD", nullptr));

        btnConvert->setText(QCoreApplication::translate("TabTools", "\360\237\224\204 Convert", nullptr));
        groupCompare->setTitle(QCoreApplication::translate("TabTools", "Compare", nullptr));
        groupCompare->setStyleSheet(QCoreApplication::translate("TabTools", "QGroupBox { font-weight: bold; }", nullptr));
        label4->setText(QCoreApplication::translate("TabTools", "File A:", nullptr));
        editCompareA->setPlaceholderText(QCoreApplication::translate("TabTools", "First file...", nullptr));
        btnBrowseCompareA->setText(QCoreApplication::translate("TabTools", "...", nullptr));
        label5->setText(QCoreApplication::translate("TabTools", "File B:", nullptr));
        editCompareB->setPlaceholderText(QCoreApplication::translate("TabTools", "Second file...", nullptr));
        btnBrowseCompareB->setText(QCoreApplication::translate("TabTools", "...", nullptr));
        label6->setText(QCoreApplication::translate("TabTools", "Mode:", nullptr));
        comboCompareMode->setItemText(0, QCoreApplication::translate("TabTools", "Binary (byte-by-byte)", nullptr));
        comboCompareMode->setItemText(1, QCoreApplication::translate("TabTools", "Sector", nullptr));
        comboCompareMode->setItemText(2, QCoreApplication::translate("TabTools", "Track", nullptr));
        comboCompareMode->setItemText(3, QCoreApplication::translate("TabTools", "Flux timing", nullptr));

        checkIgnoreHeaders->setText(QCoreApplication::translate("TabTools", "Ignore headers", nullptr));
        checkShowHex->setText(QCoreApplication::translate("TabTools", "Show hex diff", nullptr));
        btnCompare->setText(QCoreApplication::translate("TabTools", "\360\237\224\215 Compare", nullptr));
        groupRepair->setTitle(QCoreApplication::translate("TabTools", "Repair", nullptr));
        groupRepair->setStyleSheet(QCoreApplication::translate("TabTools", "QGroupBox { font-weight: bold; }", nullptr));
        label7->setText(QCoreApplication::translate("TabTools", "File:", nullptr));
        editRepairFile->setPlaceholderText(QCoreApplication::translate("TabTools", "File to repair...", nullptr));
        btnBrowseRepair->setText(QCoreApplication::translate("TabTools", "...", nullptr));
        label8->setText(QCoreApplication::translate("TabTools", "Repairs:", nullptr));
        checkFixChecksum->setText(QCoreApplication::translate("TabTools", "Fix checksums", nullptr));
        checkFixBAM->setText(QCoreApplication::translate("TabTools", "Fix BAM/FAT", nullptr));
        checkFixHeaders->setText(QCoreApplication::translate("TabTools", "Fix headers", nullptr));
        checkRecoverDeleted->setText(QCoreApplication::translate("TabTools", "Recover deleted", nullptr));
        checkFillBadSectors->setText(QCoreApplication::translate("TabTools", "Fill bad sectors", nullptr));
        checkBackup->setText(QCoreApplication::translate("TabTools", "Create backup", nullptr));
        btnRepair->setText(QCoreApplication::translate("TabTools", "\360\237\224\247 Repair", nullptr));
        groupAnalyze->setTitle(QCoreApplication::translate("TabTools", "Analyze", nullptr));
        groupAnalyze->setStyleSheet(QCoreApplication::translate("TabTools", "QGroupBox { font-weight: bold; }", nullptr));
        label9->setText(QCoreApplication::translate("TabTools", "File:", nullptr));
        editAnalyzeFile->setPlaceholderText(QCoreApplication::translate("TabTools", "File to analyze...", nullptr));
        btnBrowseAnalyze->setText(QCoreApplication::translate("TabTools", "...", nullptr));
        checkDetectFormat->setText(QCoreApplication::translate("TabTools", "Detect format", nullptr));
        checkDetectProtection->setText(QCoreApplication::translate("TabTools", "Detect protection", nullptr));
        checkShowDirectory->setText(QCoreApplication::translate("TabTools", "Show directory", nullptr));
        checkShowBadSectors->setText(QCoreApplication::translate("TabTools", "Show bad sectors", nullptr));
        checkCalcHashes->setText(QCoreApplication::translate("TabTools", "Calculate hashes", nullptr));
        checkHexDump->setText(QCoreApplication::translate("TabTools", "Hex dump", nullptr));
        btnAnalyze->setText(QCoreApplication::translate("TabTools", "\360\237\223\212 Analyze", nullptr));
        groupBatch->setTitle(QCoreApplication::translate("TabTools", "Batch", nullptr));
        groupBatch->setStyleSheet(QCoreApplication::translate("TabTools", "QGroupBox { font-weight: bold; }", nullptr));
        label10->setText(QCoreApplication::translate("TabTools", "Folder:", nullptr));
        editBatchFolder->setPlaceholderText(QCoreApplication::translate("TabTools", "Input folder...", nullptr));
        btnBrowseBatch->setText(QCoreApplication::translate("TabTools", "...", nullptr));
        label11->setText(QCoreApplication::translate("TabTools", "Filter:", nullptr));
        editBatchFilter->setText(QCoreApplication::translate("TabTools", "*.d64;*.g64;*.adf", nullptr));
        label12->setText(QCoreApplication::translate("TabTools", "Action:", nullptr));
        comboBatchAction->setItemText(0, QCoreApplication::translate("TabTools", "Convert", nullptr));
        comboBatchAction->setItemText(1, QCoreApplication::translate("TabTools", "Analyze", nullptr));
        comboBatchAction->setItemText(2, QCoreApplication::translate("TabTools", "Validate", nullptr));
        comboBatchAction->setItemText(3, QCoreApplication::translate("TabTools", "Repair", nullptr));
        comboBatchAction->setItemText(4, QCoreApplication::translate("TabTools", "Calculate hashes", nullptr));

        checkBatchSubfolders->setText(QCoreApplication::translate("TabTools", "Include subfolders", nullptr));
        checkBatchLog->setText(QCoreApplication::translate("TabTools", "Create log", nullptr));
        btnBatchStart->setText(QCoreApplication::translate("TabTools", "\342\226\266 Start Batch", nullptr));
        btnBatchStop->setText(QCoreApplication::translate("TabTools", "\342\217\271 Stop", nullptr));
        groupQuickTools->setTitle(QCoreApplication::translate("TabTools", "Quick Tools", nullptr));
        groupQuickTools->setStyleSheet(QCoreApplication::translate("TabTools", "QGroupBox { font-weight: bold; }", nullptr));
        btnHexView->setText(QCoreApplication::translate("TabTools", "\360\237\224\242 Hex View", nullptr));
        btnTrackView->setText(QCoreApplication::translate("TabTools", "\360\237\223\200 Track View", nullptr));
        btnFluxView->setText(QCoreApplication::translate("TabTools", "\360\237\223\210 Flux View", nullptr));
        btnSectorEdit->setText(QCoreApplication::translate("TabTools", "\342\234\217\357\270\217 Sector Edit", nullptr));
        btnCreateBlank->setText(QCoreApplication::translate("TabTools", "\360\237\222\276 Create Blank", nullptr));
        btnDiskInfo->setText(QCoreApplication::translate("TabTools", "\342\204\271\357\270\217 Disk Info", nullptr));
        groupOutput->setTitle(QCoreApplication::translate("TabTools", "Output", nullptr));
        groupOutput->setStyleSheet(QCoreApplication::translate("TabTools", "QGroupBox { font-weight: bold; }", nullptr));
        textOutput->setPlainText(QCoreApplication::translate("TabTools", "Ready.", nullptr));
        btnClearOutput->setText(QCoreApplication::translate("TabTools", "Clear", nullptr));
        btnSaveOutput->setText(QCoreApplication::translate("TabTools", "Save...", nullptr));
        (void)TabTools;
    } // retranslateUi

};

namespace Ui {
    class TabTools: public Ui_TabTools {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TAB_TOOLS_H

/********************************************************************************
** Form generated from reading UI file 'tab_format.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TAB_FORMAT_H
#define UI_TAB_FORMAT_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TabFormat
{
public:
    QHBoxLayout *mainLayout;
    QWidget *col1Widget;
    QVBoxLayout *col1;
    QGroupBox *groupSystem;
    QFormLayout *formLayout;
    QLabel *label;
    QComboBox *comboSystem;
    QLabel *label1;
    QComboBox *comboFormat;
    QLabel *label2;
    QComboBox *comboVersion;
    QGroupBox *groupFormat;
    QGridLayout *gridLayout;
    QLabel *label3;
    QSpinBox *spinTracks;
    QLabel *label4;
    QSpinBox *spinSides;
    QLabel *label5;
    QSpinBox *spinSectors;
    QLabel *label6;
    QComboBox *comboSectorSize;
    QLabel *label7;
    QComboBox *comboEncoding;
    QLabel *label8;
    QComboBox *comboRPM;
    QGroupBox *groupXCopy;
    QVBoxLayout *vboxLayout;
    QHBoxLayout *hboxLayout;
    QLabel *label9;
    QSpinBox *spinStartTrack;
    QLabel *label10;
    QSpinBox *spinEndTrack;
    QHBoxLayout *hboxLayout1;
    QLabel *label11;
    QComboBox *comboXCopySides;
    QLabel *label12;
    QComboBox *comboXCopyMode;
    QHBoxLayout *hboxLayout2;
    QCheckBox *checkXCopyVerify;
    QCheckBox *checkAllTracks;
    QGroupBox *groupPresets;
    QVBoxLayout *vboxLayout1;
    QComboBox *comboPreset;
    QHBoxLayout *hboxLayout3;
    QPushButton *btnLoadPreset;
    QPushButton *btnSavePreset;
    QSpacerItem *col1Spacer;
    QWidget *col2Widget;
    QVBoxLayout *col2;
    QGroupBox *groupFlux;
    QVBoxLayout *vboxLayout2;
    QHBoxLayout *hboxLayout4;
    QLabel *label13;
    QComboBox *comboFluxSpeed;
    QLabel *label14;
    QSpinBox *spinRevolutions;
    QHBoxLayout *hboxLayout5;
    QLabel *label15;
    QComboBox *comboFluxErrors;
    QLabel *label16;
    QComboBox *comboFluxMerge;
    QHBoxLayout *hboxLayout6;
    QCheckBox *checkWeakBits;
    QCheckBox *checkNoFluxAreas;
    QPushButton *btnFluxAdvanced;
    QGroupBox *groupPLL;
    QVBoxLayout *vboxLayout3;
    QHBoxLayout *hboxLayout7;
    QLabel *label17;
    QComboBox *comboSampleRate;
    QHBoxLayout *hboxLayout8;
    QCheckBox *checkAdaptivePLL;
    QCheckBox *checkUseIndex;
    QPushButton *btnPLLAdvanced;
    QGroupBox *groupNibble;
    QVBoxLayout *vboxLayout4;
    QHBoxLayout *hboxLayout9;
    QLabel *label18;
    QComboBox *comboGCRType;
    QHBoxLayout *hboxLayout10;
    QCheckBox *checkNibbleMode;
    QCheckBox *checkDecodeGCR;
    QHBoxLayout *hboxLayout11;
    QCheckBox *checkHalfTracks;
    QPushButton *btnNibbleAdvanced;
    QGroupBox *groupWrite;
    QVBoxLayout *vboxLayout5;
    QCheckBox *checkWritePrecomp;
    QHBoxLayout *hboxLayout12;
    QLabel *label19;
    QComboBox *comboPlatform;
    QSpacerItem *col2Spacer;
    QWidget *col3Widget;
    QVBoxLayout *col3;
    QGroupBox *groupProtection;
    QVBoxLayout *vboxLayout6;
    QCheckBox *checkDetectAll;
    QGridLayout *gridLayout1;
    QCheckBox *checkDetectWeakBitsProt;
    QCheckBox *checkDetectLongTracks;
    QCheckBox *checkDetectHalfTracks;
    QCheckBox *checkDetectTiming;
    QCheckBox *checkDetectNoFlux;
    QCheckBox *checkDetectCustomSync;
    QHBoxLayout *hboxLayout13;
    QRadioButton *radioPreserve;
    QRadioButton *radioRemove;
    QGroupBox *groupForensic;
    QVBoxLayout *vboxLayout7;
    QCheckBox *checkGenerateHash;
    QCheckBox *checkGenerateReport;
    QCheckBox *checkStrictMode;
    QGroupBox *groupLogging;
    QVBoxLayout *vboxLayout8;
    QHBoxLayout *hboxLayout14;
    QLabel *label20;
    QComboBox *comboLogLevel;
    QCheckBox *checkLogToFile;
    QHBoxLayout *hboxLayout15;
    QLineEdit *editLogPath;
    QPushButton *btnBrowseLog;
    QHBoxLayout *hboxLayout16;
    QCheckBox *checkLogTimestamps;
    QCheckBox *checkVerboseLog;
    QGroupBox *groupGw2Dmk;
    QVBoxLayout *vboxLayout9;
    QHBoxLayout *hboxLayout17;
    QLabel *label21;
    QComboBox *comboGw2DmkPreset;
    QPushButton *btnGw2DmkOpen;
    QSpacerItem *col3Spacer;

    void setupUi(QWidget *TabFormat)
    {
        if (TabFormat->objectName().isEmpty())
            TabFormat->setObjectName("TabFormat");
        TabFormat->resize(1000, 700);
        mainLayout = new QHBoxLayout(TabFormat);
        mainLayout->setSpacing(8);
        mainLayout->setObjectName("mainLayout");
        mainLayout->setContentsMargins(4, 4, 4, 4);
        col1Widget = new QWidget(TabFormat);
        col1Widget->setObjectName("col1Widget");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(col1Widget->sizePolicy().hasHeightForWidth());
        col1Widget->setSizePolicy(sizePolicy);
        col1 = new QVBoxLayout(col1Widget);
        col1->setSpacing(4);
        col1->setObjectName("col1");
        col1->setContentsMargins(0, 0, 0, 0);
        groupSystem = new QGroupBox(col1Widget);
        groupSystem->setObjectName("groupSystem");
        groupSystem->setMinimumSize(QSize(0, 100));
        formLayout = new QFormLayout(groupSystem);
        formLayout->setObjectName("formLayout");
        label = new QLabel(groupSystem);
        label->setObjectName("label");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, label);

        comboSystem = new QComboBox(groupSystem);
        comboSystem->setObjectName("comboSystem");

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, comboSystem);

        label1 = new QLabel(groupSystem);
        label1->setObjectName("label1");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, label1);

        comboFormat = new QComboBox(groupSystem);
        comboFormat->setObjectName("comboFormat");

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, comboFormat);

        label2 = new QLabel(groupSystem);
        label2->setObjectName("label2");

        formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, label2);

        comboVersion = new QComboBox(groupSystem);
        comboVersion->setObjectName("comboVersion");

        formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, comboVersion);


        col1->addWidget(groupSystem);

        groupFormat = new QGroupBox(col1Widget);
        groupFormat->setObjectName("groupFormat");
        groupFormat->setMinimumSize(QSize(0, 100));
        gridLayout = new QGridLayout(groupFormat);
        gridLayout->setObjectName("gridLayout");
        label3 = new QLabel(groupFormat);
        label3->setObjectName("label3");

        gridLayout->addWidget(label3, 0, 0, 1, 1);

        spinTracks = new QSpinBox(groupFormat);
        spinTracks->setObjectName("spinTracks");
        spinTracks->setMaximum(255);
        spinTracks->setValue(80);

        gridLayout->addWidget(spinTracks, 0, 1, 1, 1);

        label4 = new QLabel(groupFormat);
        label4->setObjectName("label4");

        gridLayout->addWidget(label4, 0, 2, 1, 1);

        spinSides = new QSpinBox(groupFormat);
        spinSides->setObjectName("spinSides");
        spinSides->setMinimum(1);
        spinSides->setMaximum(2);
        spinSides->setValue(1);

        gridLayout->addWidget(spinSides, 0, 3, 1, 1);

        label5 = new QLabel(groupFormat);
        label5->setObjectName("label5");

        gridLayout->addWidget(label5, 1, 0, 1, 1);

        spinSectors = new QSpinBox(groupFormat);
        spinSectors->setObjectName("spinSectors");
        spinSectors->setMinimum(1);
        spinSectors->setMaximum(36);
        spinSectors->setValue(11);

        gridLayout->addWidget(spinSectors, 1, 1, 1, 1);

        label6 = new QLabel(groupFormat);
        label6->setObjectName("label6");

        gridLayout->addWidget(label6, 1, 2, 1, 1);

        comboSectorSize = new QComboBox(groupFormat);
        comboSectorSize->addItem(QString());
        comboSectorSize->addItem(QString());
        comboSectorSize->setObjectName("comboSectorSize");

        gridLayout->addWidget(comboSectorSize, 1, 3, 1, 1);

        label7 = new QLabel(groupFormat);
        label7->setObjectName("label7");

        gridLayout->addWidget(label7, 2, 0, 1, 1);

        comboEncoding = new QComboBox(groupFormat);
        comboEncoding->addItem(QString());
        comboEncoding->addItem(QString());
        comboEncoding->addItem(QString());
        comboEncoding->setObjectName("comboEncoding");

        gridLayout->addWidget(comboEncoding, 2, 1, 1, 1);

        label8 = new QLabel(groupFormat);
        label8->setObjectName("label8");

        gridLayout->addWidget(label8, 2, 2, 1, 1);

        comboRPM = new QComboBox(groupFormat);
        comboRPM->addItem(QString());
        comboRPM->addItem(QString());
        comboRPM->setObjectName("comboRPM");

        gridLayout->addWidget(comboRPM, 2, 3, 1, 1);


        col1->addWidget(groupFormat);

        groupXCopy = new QGroupBox(col1Widget);
        groupXCopy->setObjectName("groupXCopy");
        groupXCopy->setMinimumSize(QSize(0, 100));
        vboxLayout = new QVBoxLayout(groupXCopy);
        vboxLayout->setObjectName("vboxLayout");
        hboxLayout = new QHBoxLayout();
        hboxLayout->setObjectName("hboxLayout");
        label9 = new QLabel(groupXCopy);
        label9->setObjectName("label9");

        hboxLayout->addWidget(label9);

        spinStartTrack = new QSpinBox(groupXCopy);
        spinStartTrack->setObjectName("spinStartTrack");
        spinStartTrack->setMaximum(255);

        hboxLayout->addWidget(spinStartTrack);

        label10 = new QLabel(groupXCopy);
        label10->setObjectName("label10");

        hboxLayout->addWidget(label10);

        spinEndTrack = new QSpinBox(groupXCopy);
        spinEndTrack->setObjectName("spinEndTrack");
        spinEndTrack->setMaximum(255);
        spinEndTrack->setValue(79);

        hboxLayout->addWidget(spinEndTrack);


        vboxLayout->addLayout(hboxLayout);

        hboxLayout1 = new QHBoxLayout();
        hboxLayout1->setObjectName("hboxLayout1");
        label11 = new QLabel(groupXCopy);
        label11->setObjectName("label11");

        hboxLayout1->addWidget(label11);

        comboXCopySides = new QComboBox(groupXCopy);
        comboXCopySides->addItem(QString());
        comboXCopySides->addItem(QString());
        comboXCopySides->addItem(QString());
        comboXCopySides->setObjectName("comboXCopySides");

        hboxLayout1->addWidget(comboXCopySides);

        label12 = new QLabel(groupXCopy);
        label12->setObjectName("label12");

        hboxLayout1->addWidget(label12);

        comboXCopyMode = new QComboBox(groupXCopy);
        comboXCopyMode->addItem(QString());
        comboXCopyMode->addItem(QString());
        comboXCopyMode->addItem(QString());
        comboXCopyMode->setObjectName("comboXCopyMode");

        hboxLayout1->addWidget(comboXCopyMode);


        vboxLayout->addLayout(hboxLayout1);

        hboxLayout2 = new QHBoxLayout();
        hboxLayout2->setObjectName("hboxLayout2");
        checkXCopyVerify = new QCheckBox(groupXCopy);
        checkXCopyVerify->setObjectName("checkXCopyVerify");
        checkXCopyVerify->setChecked(true);

        hboxLayout2->addWidget(checkXCopyVerify);

        checkAllTracks = new QCheckBox(groupXCopy);
        checkAllTracks->setObjectName("checkAllTracks");
        checkAllTracks->setChecked(true);

        hboxLayout2->addWidget(checkAllTracks);


        vboxLayout->addLayout(hboxLayout2);


        col1->addWidget(groupXCopy);

        groupPresets = new QGroupBox(col1Widget);
        groupPresets->setObjectName("groupPresets");
        groupPresets->setMinimumSize(QSize(0, 80));
        vboxLayout1 = new QVBoxLayout(groupPresets);
        vboxLayout1->setObjectName("vboxLayout1");
        comboPreset = new QComboBox(groupPresets);
        comboPreset->setObjectName("comboPreset");

        vboxLayout1->addWidget(comboPreset);

        hboxLayout3 = new QHBoxLayout();
        hboxLayout3->setObjectName("hboxLayout3");
        btnLoadPreset = new QPushButton(groupPresets);
        btnLoadPreset->setObjectName("btnLoadPreset");

        hboxLayout3->addWidget(btnLoadPreset);

        btnSavePreset = new QPushButton(groupPresets);
        btnSavePreset->setObjectName("btnSavePreset");

        hboxLayout3->addWidget(btnSavePreset);


        vboxLayout1->addLayout(hboxLayout3);


        col1->addWidget(groupPresets);

        col1Spacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        col1->addItem(col1Spacer);


        mainLayout->addWidget(col1Widget);

        col2Widget = new QWidget(TabFormat);
        col2Widget->setObjectName("col2Widget");
        sizePolicy.setHeightForWidth(col2Widget->sizePolicy().hasHeightForWidth());
        col2Widget->setSizePolicy(sizePolicy);
        col2 = new QVBoxLayout(col2Widget);
        col2->setSpacing(4);
        col2->setObjectName("col2");
        col2->setContentsMargins(0, 0, 0, 0);
        groupFlux = new QGroupBox(col2Widget);
        groupFlux->setObjectName("groupFlux");
        groupFlux->setMinimumSize(QSize(0, 100));
        vboxLayout2 = new QVBoxLayout(groupFlux);
        vboxLayout2->setObjectName("vboxLayout2");
        hboxLayout4 = new QHBoxLayout();
        hboxLayout4->setObjectName("hboxLayout4");
        label13 = new QLabel(groupFlux);
        label13->setObjectName("label13");

        hboxLayout4->addWidget(label13);

        comboFluxSpeed = new QComboBox(groupFlux);
        comboFluxSpeed->addItem(QString());
        comboFluxSpeed->addItem(QString());
        comboFluxSpeed->addItem(QString());
        comboFluxSpeed->setObjectName("comboFluxSpeed");

        hboxLayout4->addWidget(comboFluxSpeed);

        label14 = new QLabel(groupFlux);
        label14->setObjectName("label14");

        hboxLayout4->addWidget(label14);

        spinRevolutions = new QSpinBox(groupFlux);
        spinRevolutions->setObjectName("spinRevolutions");
        spinRevolutions->setMinimum(1);
        spinRevolutions->setMaximum(10);
        spinRevolutions->setValue(3);

        hboxLayout4->addWidget(spinRevolutions);


        vboxLayout2->addLayout(hboxLayout4);

        hboxLayout5 = new QHBoxLayout();
        hboxLayout5->setObjectName("hboxLayout5");
        label15 = new QLabel(groupFlux);
        label15->setObjectName("label15");

        hboxLayout5->addWidget(label15);

        comboFluxErrors = new QComboBox(groupFlux);
        comboFluxErrors->addItem(QString());
        comboFluxErrors->addItem(QString());
        comboFluxErrors->addItem(QString());
        comboFluxErrors->setObjectName("comboFluxErrors");

        hboxLayout5->addWidget(comboFluxErrors);

        label16 = new QLabel(groupFlux);
        label16->setObjectName("label16");

        hboxLayout5->addWidget(label16);

        comboFluxMerge = new QComboBox(groupFlux);
        comboFluxMerge->addItem(QString());
        comboFluxMerge->addItem(QString());
        comboFluxMerge->addItem(QString());
        comboFluxMerge->setObjectName("comboFluxMerge");

        hboxLayout5->addWidget(comboFluxMerge);


        vboxLayout2->addLayout(hboxLayout5);

        hboxLayout6 = new QHBoxLayout();
        hboxLayout6->setObjectName("hboxLayout6");
        checkWeakBits = new QCheckBox(groupFlux);
        checkWeakBits->setObjectName("checkWeakBits");
        checkWeakBits->setChecked(true);

        hboxLayout6->addWidget(checkWeakBits);

        checkNoFluxAreas = new QCheckBox(groupFlux);
        checkNoFluxAreas->setObjectName("checkNoFluxAreas");
        checkNoFluxAreas->setChecked(true);

        hboxLayout6->addWidget(checkNoFluxAreas);

        btnFluxAdvanced = new QPushButton(groupFlux);
        btnFluxAdvanced->setObjectName("btnFluxAdvanced");

        hboxLayout6->addWidget(btnFluxAdvanced);


        vboxLayout2->addLayout(hboxLayout6);


        col2->addWidget(groupFlux);

        groupPLL = new QGroupBox(col2Widget);
        groupPLL->setObjectName("groupPLL");
        groupPLL->setMinimumSize(QSize(0, 80));
        vboxLayout3 = new QVBoxLayout(groupPLL);
        vboxLayout3->setObjectName("vboxLayout3");
        hboxLayout7 = new QHBoxLayout();
        hboxLayout7->setObjectName("hboxLayout7");
        label17 = new QLabel(groupPLL);
        label17->setObjectName("label17");

        hboxLayout7->addWidget(label17);

        comboSampleRate = new QComboBox(groupPLL);
        comboSampleRate->addItem(QString());
        comboSampleRate->addItem(QString());
        comboSampleRate->addItem(QString());
        comboSampleRate->addItem(QString());
        comboSampleRate->setObjectName("comboSampleRate");

        hboxLayout7->addWidget(comboSampleRate);


        vboxLayout3->addLayout(hboxLayout7);

        hboxLayout8 = new QHBoxLayout();
        hboxLayout8->setObjectName("hboxLayout8");
        checkAdaptivePLL = new QCheckBox(groupPLL);
        checkAdaptivePLL->setObjectName("checkAdaptivePLL");
        checkAdaptivePLL->setChecked(true);

        hboxLayout8->addWidget(checkAdaptivePLL);

        checkUseIndex = new QCheckBox(groupPLL);
        checkUseIndex->setObjectName("checkUseIndex");
        checkUseIndex->setChecked(true);

        hboxLayout8->addWidget(checkUseIndex);

        btnPLLAdvanced = new QPushButton(groupPLL);
        btnPLLAdvanced->setObjectName("btnPLLAdvanced");

        hboxLayout8->addWidget(btnPLLAdvanced);


        vboxLayout3->addLayout(hboxLayout8);


        col2->addWidget(groupPLL);

        groupNibble = new QGroupBox(col2Widget);
        groupNibble->setObjectName("groupNibble");
        groupNibble->setMinimumSize(QSize(0, 100));
        vboxLayout4 = new QVBoxLayout(groupNibble);
        vboxLayout4->setObjectName("vboxLayout4");
        hboxLayout9 = new QHBoxLayout();
        hboxLayout9->setObjectName("hboxLayout9");
        label18 = new QLabel(groupNibble);
        label18->setObjectName("label18");

        hboxLayout9->addWidget(label18);

        comboGCRType = new QComboBox(groupNibble);
        comboGCRType->addItem(QString());
        comboGCRType->addItem(QString());
        comboGCRType->addItem(QString());
        comboGCRType->setObjectName("comboGCRType");

        hboxLayout9->addWidget(comboGCRType);


        vboxLayout4->addLayout(hboxLayout9);

        hboxLayout10 = new QHBoxLayout();
        hboxLayout10->setObjectName("hboxLayout10");
        checkNibbleMode = new QCheckBox(groupNibble);
        checkNibbleMode->setObjectName("checkNibbleMode");

        hboxLayout10->addWidget(checkNibbleMode);

        checkDecodeGCR = new QCheckBox(groupNibble);
        checkDecodeGCR->setObjectName("checkDecodeGCR");
        checkDecodeGCR->setChecked(true);

        hboxLayout10->addWidget(checkDecodeGCR);


        vboxLayout4->addLayout(hboxLayout10);

        hboxLayout11 = new QHBoxLayout();
        hboxLayout11->setObjectName("hboxLayout11");
        checkHalfTracks = new QCheckBox(groupNibble);
        checkHalfTracks->setObjectName("checkHalfTracks");

        hboxLayout11->addWidget(checkHalfTracks);

        btnNibbleAdvanced = new QPushButton(groupNibble);
        btnNibbleAdvanced->setObjectName("btnNibbleAdvanced");

        hboxLayout11->addWidget(btnNibbleAdvanced);


        vboxLayout4->addLayout(hboxLayout11);


        col2->addWidget(groupNibble);

        groupWrite = new QGroupBox(col2Widget);
        groupWrite->setObjectName("groupWrite");
        groupWrite->setMinimumSize(QSize(0, 80));
        vboxLayout5 = new QVBoxLayout(groupWrite);
        vboxLayout5->setObjectName("vboxLayout5");
        checkWritePrecomp = new QCheckBox(groupWrite);
        checkWritePrecomp->setObjectName("checkWritePrecomp");

        vboxLayout5->addWidget(checkWritePrecomp);

        hboxLayout12 = new QHBoxLayout();
        hboxLayout12->setObjectName("hboxLayout12");
        label19 = new QLabel(groupWrite);
        label19->setObjectName("label19");

        hboxLayout12->addWidget(label19);

        comboPlatform = new QComboBox(groupWrite);
        comboPlatform->addItem(QString());
        comboPlatform->addItem(QString());
        comboPlatform->addItem(QString());
        comboPlatform->addItem(QString());
        comboPlatform->addItem(QString());
        comboPlatform->addItem(QString());
        comboPlatform->setObjectName("comboPlatform");

        hboxLayout12->addWidget(comboPlatform);


        vboxLayout5->addLayout(hboxLayout12);


        col2->addWidget(groupWrite);

        col2Spacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        col2->addItem(col2Spacer);


        mainLayout->addWidget(col2Widget);

        col3Widget = new QWidget(TabFormat);
        col3Widget->setObjectName("col3Widget");
        sizePolicy.setHeightForWidth(col3Widget->sizePolicy().hasHeightForWidth());
        col3Widget->setSizePolicy(sizePolicy);
        col3 = new QVBoxLayout(col3Widget);
        col3->setSpacing(4);
        col3->setObjectName("col3");
        col3->setContentsMargins(0, 0, 0, 0);
        groupProtection = new QGroupBox(col3Widget);
        groupProtection->setObjectName("groupProtection");
        groupProtection->setMinimumSize(QSize(0, 140));
        vboxLayout6 = new QVBoxLayout(groupProtection);
        vboxLayout6->setObjectName("vboxLayout6");
        checkDetectAll = new QCheckBox(groupProtection);
        checkDetectAll->setObjectName("checkDetectAll");
        checkDetectAll->setChecked(true);

        vboxLayout6->addWidget(checkDetectAll);

        gridLayout1 = new QGridLayout();
        gridLayout1->setObjectName("gridLayout1");
        checkDetectWeakBitsProt = new QCheckBox(groupProtection);
        checkDetectWeakBitsProt->setObjectName("checkDetectWeakBitsProt");
        checkDetectWeakBitsProt->setChecked(true);

        gridLayout1->addWidget(checkDetectWeakBitsProt, 0, 0, 1, 1);

        checkDetectLongTracks = new QCheckBox(groupProtection);
        checkDetectLongTracks->setObjectName("checkDetectLongTracks");
        checkDetectLongTracks->setChecked(true);

        gridLayout1->addWidget(checkDetectLongTracks, 0, 1, 1, 1);

        checkDetectHalfTracks = new QCheckBox(groupProtection);
        checkDetectHalfTracks->setObjectName("checkDetectHalfTracks");
        checkDetectHalfTracks->setChecked(true);

        gridLayout1->addWidget(checkDetectHalfTracks, 1, 0, 1, 1);

        checkDetectTiming = new QCheckBox(groupProtection);
        checkDetectTiming->setObjectName("checkDetectTiming");
        checkDetectTiming->setChecked(true);

        gridLayout1->addWidget(checkDetectTiming, 1, 1, 1, 1);

        checkDetectNoFlux = new QCheckBox(groupProtection);
        checkDetectNoFlux->setObjectName("checkDetectNoFlux");
        checkDetectNoFlux->setChecked(true);

        gridLayout1->addWidget(checkDetectNoFlux, 2, 0, 1, 1);

        checkDetectCustomSync = new QCheckBox(groupProtection);
        checkDetectCustomSync->setObjectName("checkDetectCustomSync");
        checkDetectCustomSync->setChecked(true);

        gridLayout1->addWidget(checkDetectCustomSync, 2, 1, 1, 1);


        vboxLayout6->addLayout(gridLayout1);

        hboxLayout13 = new QHBoxLayout();
        hboxLayout13->setObjectName("hboxLayout13");
        radioPreserve = new QRadioButton(groupProtection);
        radioPreserve->setObjectName("radioPreserve");
        radioPreserve->setChecked(true);

        hboxLayout13->addWidget(radioPreserve);

        radioRemove = new QRadioButton(groupProtection);
        radioRemove->setObjectName("radioRemove");

        hboxLayout13->addWidget(radioRemove);


        vboxLayout6->addLayout(hboxLayout13);


        col3->addWidget(groupProtection);

        groupForensic = new QGroupBox(col3Widget);
        groupForensic->setObjectName("groupForensic");
        groupForensic->setMinimumSize(QSize(0, 100));
        vboxLayout7 = new QVBoxLayout(groupForensic);
        vboxLayout7->setObjectName("vboxLayout7");
        checkGenerateHash = new QCheckBox(groupForensic);
        checkGenerateHash->setObjectName("checkGenerateHash");
        checkGenerateHash->setChecked(true);

        vboxLayout7->addWidget(checkGenerateHash);

        checkGenerateReport = new QCheckBox(groupForensic);
        checkGenerateReport->setObjectName("checkGenerateReport");

        vboxLayout7->addWidget(checkGenerateReport);

        checkStrictMode = new QCheckBox(groupForensic);
        checkStrictMode->setObjectName("checkStrictMode");

        vboxLayout7->addWidget(checkStrictMode);


        col3->addWidget(groupForensic);

        groupLogging = new QGroupBox(col3Widget);
        groupLogging->setObjectName("groupLogging");
        groupLogging->setMinimumSize(QSize(0, 130));
        vboxLayout8 = new QVBoxLayout(groupLogging);
        vboxLayout8->setObjectName("vboxLayout8");
        hboxLayout14 = new QHBoxLayout();
        hboxLayout14->setObjectName("hboxLayout14");
        label20 = new QLabel(groupLogging);
        label20->setObjectName("label20");

        hboxLayout14->addWidget(label20);

        comboLogLevel = new QComboBox(groupLogging);
        comboLogLevel->addItem(QString());
        comboLogLevel->addItem(QString());
        comboLogLevel->addItem(QString());
        comboLogLevel->addItem(QString());
        comboLogLevel->addItem(QString());
        comboLogLevel->setObjectName("comboLogLevel");

        hboxLayout14->addWidget(comboLogLevel);


        vboxLayout8->addLayout(hboxLayout14);

        checkLogToFile = new QCheckBox(groupLogging);
        checkLogToFile->setObjectName("checkLogToFile");

        vboxLayout8->addWidget(checkLogToFile);

        hboxLayout15 = new QHBoxLayout();
        hboxLayout15->setObjectName("hboxLayout15");
        editLogPath = new QLineEdit(groupLogging);
        editLogPath->setObjectName("editLogPath");
        editLogPath->setEnabled(false);

        hboxLayout15->addWidget(editLogPath);

        btnBrowseLog = new QPushButton(groupLogging);
        btnBrowseLog->setObjectName("btnBrowseLog");
        btnBrowseLog->setMaximumWidth(30);
        btnBrowseLog->setEnabled(false);

        hboxLayout15->addWidget(btnBrowseLog);


        vboxLayout8->addLayout(hboxLayout15);

        hboxLayout16 = new QHBoxLayout();
        hboxLayout16->setObjectName("hboxLayout16");
        checkLogTimestamps = new QCheckBox(groupLogging);
        checkLogTimestamps->setObjectName("checkLogTimestamps");
        checkLogTimestamps->setChecked(true);

        hboxLayout16->addWidget(checkLogTimestamps);

        checkVerboseLog = new QCheckBox(groupLogging);
        checkVerboseLog->setObjectName("checkVerboseLog");

        hboxLayout16->addWidget(checkVerboseLog);


        vboxLayout8->addLayout(hboxLayout16);


        col3->addWidget(groupLogging);

        groupGw2Dmk = new QGroupBox(col3Widget);
        groupGw2Dmk->setObjectName("groupGw2Dmk");
        groupGw2Dmk->setMinimumSize(QSize(0, 100));
        vboxLayout9 = new QVBoxLayout(groupGw2Dmk);
        vboxLayout9->setObjectName("vboxLayout9");
        hboxLayout17 = new QHBoxLayout();
        hboxLayout17->setObjectName("hboxLayout17");
        label21 = new QLabel(groupGw2Dmk);
        label21->setObjectName("label21");

        hboxLayout17->addWidget(label21);

        comboGw2DmkPreset = new QComboBox(groupGw2Dmk);
        comboGw2DmkPreset->addItem(QString());
        comboGw2DmkPreset->addItem(QString());
        comboGw2DmkPreset->addItem(QString());
        comboGw2DmkPreset->addItem(QString());
        comboGw2DmkPreset->setObjectName("comboGw2DmkPreset");

        hboxLayout17->addWidget(comboGw2DmkPreset);


        vboxLayout9->addLayout(hboxLayout17);

        btnGw2DmkOpen = new QPushButton(groupGw2Dmk);
        btnGw2DmkOpen->setObjectName("btnGw2DmkOpen");

        vboxLayout9->addWidget(btnGw2DmkOpen);


        col3->addWidget(groupGw2Dmk);

        col3Spacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        col3->addItem(col3Spacer);


        mainLayout->addWidget(col3Widget);


        retranslateUi(TabFormat);

        QMetaObject::connectSlotsByName(TabFormat);
    } // setupUi

    void retranslateUi(QWidget *TabFormat)
    {
        groupSystem->setTitle(QCoreApplication::translate("TabFormat", "System / Format", nullptr));
        label->setText(QCoreApplication::translate("TabFormat", "System:", nullptr));
        label1->setText(QCoreApplication::translate("TabFormat", "Format:", nullptr));
        label2->setText(QCoreApplication::translate("TabFormat", "Version:", nullptr));
        groupFormat->setTitle(QCoreApplication::translate("TabFormat", "Format", nullptr));
        label3->setText(QCoreApplication::translate("TabFormat", "Tracks:", nullptr));
        label4->setText(QCoreApplication::translate("TabFormat", "Sides:", nullptr));
        label5->setText(QCoreApplication::translate("TabFormat", "Sectors:", nullptr));
        label6->setText(QCoreApplication::translate("TabFormat", "Size:", nullptr));
        comboSectorSize->setItemText(0, QCoreApplication::translate("TabFormat", "256", nullptr));
        comboSectorSize->setItemText(1, QCoreApplication::translate("TabFormat", "512", nullptr));

        label7->setText(QCoreApplication::translate("TabFormat", "Encoding:", nullptr));
        comboEncoding->setItemText(0, QCoreApplication::translate("TabFormat", "MFM", nullptr));
        comboEncoding->setItemText(1, QCoreApplication::translate("TabFormat", "FM", nullptr));
        comboEncoding->setItemText(2, QCoreApplication::translate("TabFormat", "GCR", nullptr));

        label8->setText(QCoreApplication::translate("TabFormat", "RPM:", nullptr));
        comboRPM->setItemText(0, QCoreApplication::translate("TabFormat", "300", nullptr));
        comboRPM->setItemText(1, QCoreApplication::translate("TabFormat", "360", nullptr));

        groupXCopy->setTitle(QCoreApplication::translate("TabFormat", "XCopy", nullptr));
        label9->setText(QCoreApplication::translate("TabFormat", "Start:", nullptr));
        label10->setText(QCoreApplication::translate("TabFormat", "End:", nullptr));
        label11->setText(QCoreApplication::translate("TabFormat", "Sides:", nullptr));
        comboXCopySides->setItemText(0, QCoreApplication::translate("TabFormat", "Both", nullptr));
        comboXCopySides->setItemText(1, QCoreApplication::translate("TabFormat", "Side 0", nullptr));
        comboXCopySides->setItemText(2, QCoreApplication::translate("TabFormat", "Side 1", nullptr));

        label12->setText(QCoreApplication::translate("TabFormat", "Mode:", nullptr));
        comboXCopyMode->setItemText(0, QCoreApplication::translate("TabFormat", "Sector", nullptr));
        comboXCopyMode->setItemText(1, QCoreApplication::translate("TabFormat", "Track", nullptr));
        comboXCopyMode->setItemText(2, QCoreApplication::translate("TabFormat", "Index", nullptr));

        checkXCopyVerify->setText(QCoreApplication::translate("TabFormat", "Verify", nullptr));
        checkAllTracks->setText(QCoreApplication::translate("TabFormat", "All tracks", nullptr));
        groupPresets->setTitle(QCoreApplication::translate("TabFormat", "Presets", nullptr));
        btnLoadPreset->setText(QCoreApplication::translate("TabFormat", "Load", nullptr));
        btnSavePreset->setText(QCoreApplication::translate("TabFormat", "Save", nullptr));
        groupFlux->setTitle(QCoreApplication::translate("TabFormat", "Flux", nullptr));
        label13->setText(QCoreApplication::translate("TabFormat", "Speed:", nullptr));
        comboFluxSpeed->setItemText(0, QCoreApplication::translate("TabFormat", "Safe", nullptr));
        comboFluxSpeed->setItemText(1, QCoreApplication::translate("TabFormat", "Normal", nullptr));
        comboFluxSpeed->setItemText(2, QCoreApplication::translate("TabFormat", "Fast", nullptr));

        label14->setText(QCoreApplication::translate("TabFormat", "Revs:", nullptr));
        label15->setText(QCoreApplication::translate("TabFormat", "Errors:", nullptr));
        comboFluxErrors->setItemText(0, QCoreApplication::translate("TabFormat", "Strict", nullptr));
        comboFluxErrors->setItemText(1, QCoreApplication::translate("TabFormat", "Relaxed", nullptr));
        comboFluxErrors->setItemText(2, QCoreApplication::translate("TabFormat", "Ignore", nullptr));

        label16->setText(QCoreApplication::translate("TabFormat", "Merge:", nullptr));
        comboFluxMerge->setItemText(0, QCoreApplication::translate("TabFormat", "First", nullptr));
        comboFluxMerge->setItemText(1, QCoreApplication::translate("TabFormat", "Best", nullptr));
        comboFluxMerge->setItemText(2, QCoreApplication::translate("TabFormat", "All", nullptr));

        checkWeakBits->setText(QCoreApplication::translate("TabFormat", "Weak bits", nullptr));
        checkNoFluxAreas->setText(QCoreApplication::translate("TabFormat", "No-flux areas", nullptr));
        btnFluxAdvanced->setText(QCoreApplication::translate("TabFormat", "Advanced...", nullptr));
        groupPLL->setTitle(QCoreApplication::translate("TabFormat", "PLL", nullptr));
        label17->setText(QCoreApplication::translate("TabFormat", "Sample:", nullptr));
        comboSampleRate->setItemText(0, QCoreApplication::translate("TabFormat", "Auto", nullptr));
        comboSampleRate->setItemText(1, QCoreApplication::translate("TabFormat", "25 MHz", nullptr));
        comboSampleRate->setItemText(2, QCoreApplication::translate("TabFormat", "50 MHz", nullptr));
        comboSampleRate->setItemText(3, QCoreApplication::translate("TabFormat", "80 MHz", nullptr));

        checkAdaptivePLL->setText(QCoreApplication::translate("TabFormat", "Adaptive PLL", nullptr));
        checkUseIndex->setText(QCoreApplication::translate("TabFormat", "Use index", nullptr));
        btnPLLAdvanced->setText(QCoreApplication::translate("TabFormat", "Advanced...", nullptr));
        groupNibble->setTitle(QCoreApplication::translate("TabFormat", "GCR / Nibble", nullptr));
        label18->setText(QCoreApplication::translate("TabFormat", "Type:", nullptr));
        comboGCRType->setItemText(0, QCoreApplication::translate("TabFormat", "Standard GCR", nullptr));
        comboGCRType->setItemText(1, QCoreApplication::translate("TabFormat", "Apple II GCR", nullptr));
        comboGCRType->setItemText(2, QCoreApplication::translate("TabFormat", "C64 GCR", nullptr));

        checkNibbleMode->setText(QCoreApplication::translate("TabFormat", "Nibble mode (raw)", nullptr));
        checkDecodeGCR->setText(QCoreApplication::translate("TabFormat", "Decode GCR", nullptr));
        checkHalfTracks->setText(QCoreApplication::translate("TabFormat", "Half-tracks", nullptr));
        btnNibbleAdvanced->setText(QCoreApplication::translate("TabFormat", "Advanced...", nullptr));
        groupWrite->setTitle(QCoreApplication::translate("TabFormat", "Write", nullptr));
        checkWritePrecomp->setText(QCoreApplication::translate("TabFormat", "Write precompensation", nullptr));
        label19->setText(QCoreApplication::translate("TabFormat", "Platform:", nullptr));
        comboPlatform->setItemText(0, QCoreApplication::translate("TabFormat", "Auto-detect", nullptr));
        comboPlatform->setItemText(1, QCoreApplication::translate("TabFormat", "Commodore", nullptr));
        comboPlatform->setItemText(2, QCoreApplication::translate("TabFormat", "Amiga", nullptr));
        comboPlatform->setItemText(3, QCoreApplication::translate("TabFormat", "Atari", nullptr));
        comboPlatform->setItemText(4, QCoreApplication::translate("TabFormat", "Apple", nullptr));
        comboPlatform->setItemText(5, QCoreApplication::translate("TabFormat", "PC/DOS", nullptr));

        groupProtection->setTitle(QCoreApplication::translate("TabFormat", "Protection Detection", nullptr));
        checkDetectAll->setText(QCoreApplication::translate("TabFormat", "Detect all protections", nullptr));
        checkDetectWeakBitsProt->setText(QCoreApplication::translate("TabFormat", "Weak bits", nullptr));
        checkDetectLongTracks->setText(QCoreApplication::translate("TabFormat", "Long tracks", nullptr));
        checkDetectHalfTracks->setText(QCoreApplication::translate("TabFormat", "Half-tracks", nullptr));
        checkDetectTiming->setText(QCoreApplication::translate("TabFormat", "Timing var.", nullptr));
        checkDetectNoFlux->setText(QCoreApplication::translate("TabFormat", "No-flux areas", nullptr));
        checkDetectCustomSync->setText(QCoreApplication::translate("TabFormat", "Custom sync", nullptr));
        radioPreserve->setText(QCoreApplication::translate("TabFormat", "Preserve", nullptr));
        radioRemove->setText(QCoreApplication::translate("TabFormat", "Remove", nullptr));
        groupForensic->setTitle(QCoreApplication::translate("TabFormat", "Forensic", nullptr));
        checkGenerateHash->setText(QCoreApplication::translate("TabFormat", "Generate hash (SHA-256)", nullptr));
        checkGenerateReport->setText(QCoreApplication::translate("TabFormat", "Generate report", nullptr));
        checkStrictMode->setText(QCoreApplication::translate("TabFormat", "Strict mode (no guessing)", nullptr));
        groupLogging->setTitle(QCoreApplication::translate("TabFormat", "Logging", nullptr));
        label20->setText(QCoreApplication::translate("TabFormat", "Level:", nullptr));
        comboLogLevel->setItemText(0, QCoreApplication::translate("TabFormat", "Off", nullptr));
        comboLogLevel->setItemText(1, QCoreApplication::translate("TabFormat", "Error", nullptr));
        comboLogLevel->setItemText(2, QCoreApplication::translate("TabFormat", "Warning", nullptr));
        comboLogLevel->setItemText(3, QCoreApplication::translate("TabFormat", "Info", nullptr));
        comboLogLevel->setItemText(4, QCoreApplication::translate("TabFormat", "Debug", nullptr));

        checkLogToFile->setText(QCoreApplication::translate("TabFormat", "Log to file", nullptr));
        editLogPath->setPlaceholderText(QCoreApplication::translate("TabFormat", "Log file path...", nullptr));
        btnBrowseLog->setText(QCoreApplication::translate("TabFormat", "...", nullptr));
        checkLogTimestamps->setText(QCoreApplication::translate("TabFormat", "Timestamps", nullptr));
        checkVerboseLog->setText(QCoreApplication::translate("TabFormat", "Verbose", nullptr));
        groupGw2Dmk->setTitle(QCoreApplication::translate("TabFormat", "GW\342\206\222DMK Direct Read (TRS-80)", nullptr));
        label21->setText(QCoreApplication::translate("TabFormat", "Preset:", nullptr));
        comboGw2DmkPreset->setItemText(0, QCoreApplication::translate("TabFormat", "TRS-80 Model I/III SSSD", nullptr));
        comboGw2DmkPreset->setItemText(1, QCoreApplication::translate("TabFormat", "TRS-80 Model I/III SSDD", nullptr));
        comboGw2DmkPreset->setItemText(2, QCoreApplication::translate("TabFormat", "TRS-80 Model 4 DSDD", nullptr));
        comboGw2DmkPreset->setItemText(3, QCoreApplication::translate("TabFormat", "Custom", nullptr));

        btnGw2DmkOpen->setText(QCoreApplication::translate("TabFormat", "\360\237\224\247 Open GW\342\206\222DMK Panel...", nullptr));
#if QT_CONFIG(tooltip)
        btnGw2DmkOpen->setToolTip(QCoreApplication::translate("TabFormat", "Direct Greaseweazle to DMK imaging", nullptr));
#endif // QT_CONFIG(tooltip)
        (void)TabFormat;
    } // retranslateUi

};

namespace Ui {
    class TabFormat: public Ui_TabFormat {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TAB_FORMAT_H

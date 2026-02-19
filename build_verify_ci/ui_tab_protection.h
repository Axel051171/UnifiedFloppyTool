/********************************************************************************
** Form generated from reading UI file 'tab_protection.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TAB_PROTECTION_H
#define UI_TAB_PROTECTION_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
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
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TabProtection
{
public:
    QVBoxLayout *verticalLayout;
    QGroupBox *groupProfiles;
    QFormLayout *formLayout;
    QLabel *labelProfile;
    QComboBox *comboProfile;
    QLabel *labelProfileName;
    QLineEdit *lineProfileName;
    QHBoxLayout *horizontalLayout;
    QPushButton *btnSaveProfile;
    QPushButton *btnLoadProfile;
    QPushButton *btnDeleteProfile;
    QSpacerItem *horizontalSpacer;
    QGroupBox *groupDetection;
    QVBoxLayout *verticalLayout_2;
    QCheckBox *checkAutoDetect;
    QCheckBox *checkPreserveProtection;
    QCheckBox *checkReportProtection;
    QCheckBox *checkLogDetails;
    QTabWidget *tabProtectionTypes;
    QWidget *tabXCopy;
    QVBoxLayout *verticalLayout_3;
    QCheckBox *checkXCopyEnable;
    QLabel *labelXCopyInfo;
    QCheckBox *checkErr1;
    QCheckBox *checkErr2;
    QCheckBox *checkErr3;
    QCheckBox *checkErr4;
    QCheckBox *checkErr5;
    QCheckBox *checkErr6;
    QCheckBox *checkErr7;
    QCheckBox *checkErr8;
    QSpacerItem *verticalSpacer_2;
    QWidget *tabDiskDupe;
    QVBoxLayout *verticalLayout_4;
    QCheckBox *checkDDEnable;
    QLabel *labelDDInfo;
    QCheckBox *checkDD1;
    QCheckBox *checkDD2;
    QCheckBox *checkDD3;
    QCheckBox *checkDD4;
    QCheckBox *checkDD5;
    QCheckBox *checkDDExpertMode;
    QGroupBox *groupDDExpert;
    QFormLayout *formLayout_2;
    QLabel *labelHalfTrackThreshold;
    QSpinBox *spinHalfTrackThreshold;
    QLabel *labelSyncVariance;
    QSpinBox *spinSyncVariance;
    QLabel *labelTimingTolerance;
    QSpinBox *spinTimingTolerance;
    QSpacerItem *verticalSpacer_3;
    QWidget *tabC64Nibbler;
    QVBoxLayout *verticalLayout_5;
    QCheckBox *checkC64Enable;
    QLabel *labelC64Info;
    QGroupBox *groupGCR;
    QFormLayout *formLayout_3;
    QLabel *labelSpeedZones;
    QHBoxLayout *horizontalLayout_2;
    QRadioButton *radioSpeedAuto;
    QRadioButton *radioSpeedManual;
    QLabel *labelSync;
    QComboBox *comboSync;
    QLabel *labelGAP;
    QCheckBox *checkGAPAuto;
    QGroupBox *groupHalfTrack;
    QFormLayout *formLayout_4;
    QCheckBox *checkHalfTrack;
    QLabel *labelHalfTrackRange;
    QHBoxLayout *horizontalLayout_3;
    QDoubleSpinBox *spinHalfTrackStart;
    QLabel *labelTo;
    QDoubleSpinBox *spinHalfTrackEnd;
    QLabel *labelHalfTrackStep;
    QComboBox *comboHalfTrackStep;
    QGroupBox *groupC64Protection;
    QGridLayout *gridLayout;
    QCheckBox *checkC64WeakBits;
    QCheckBox *checkC64VarTiming;
    QCheckBox *checkC64Alignment;
    QCheckBox *checkC64SectorCount;
    QGroupBox *groupC64Output;
    QVBoxLayout *verticalLayout_6;
    QRadioButton *radioOutputG64;
    QRadioButton *radioOutputD64;
    QRadioButton *radioOutputNIB;
    QRadioButton *radioOutputFlux;
    QCheckBox *checkC64Expert;
    QGroupBox *groupC64ExpertParams;
    QFormLayout *formLayout_5;
    QLabel *labelGCRTolerance;
    QSpinBox *spinGCRTolerance;
    QLabel *labelBitSlip;
    QCheckBox *checkBitSlip;
    QLabel *labelRevAlignment;
    QCheckBox *checkRevAlignment;
    QLabel *labelTrackVariance;
    QSpinBox *spinTrackVariance;
    QWidget *tabFlags;
    QVBoxLayout *verticalLayout_7;
    QLabel *labelFlags;
    QGroupBox *groupFlags;
    QGridLayout *gridLayout_2;
    QCheckBox *checkWeakBits;
    QCheckBox *checkLongTrack;
    QCheckBox *checkShortTrack;
    QCheckBox *checkBadCRC;
    QCheckBox *checkDupIDs;
    QCheckBox *checkNonStdGAP;
    QCheckBox *checkSyncAnomaly;
    QCheckBox *checkCustom;
    QSpacerItem *verticalSpacer_4;

    void setupUi(QWidget *TabProtection)
    {
        if (TabProtection->objectName().isEmpty())
            TabProtection->setObjectName("TabProtection");
        TabProtection->resize(850, 650);
        verticalLayout = new QVBoxLayout(TabProtection);
        verticalLayout->setObjectName("verticalLayout");
        groupProfiles = new QGroupBox(TabProtection);
        groupProfiles->setObjectName("groupProfiles");
        formLayout = new QFormLayout(groupProfiles);
        formLayout->setObjectName("formLayout");
        labelProfile = new QLabel(groupProfiles);
        labelProfile->setObjectName("labelProfile");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, labelProfile);

        comboProfile = new QComboBox(groupProfiles);
        comboProfile->addItem(QString());
        comboProfile->addItem(QString());
        comboProfile->addItem(QString());
        comboProfile->addItem(QString());
        comboProfile->addItem(QString());
        comboProfile->addItem(QString());
        comboProfile->addItem(QString());
        comboProfile->addItem(QString());
        comboProfile->addItem(QString());
        comboProfile->addItem(QString());
        comboProfile->addItem(QString());
        comboProfile->setObjectName("comboProfile");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(comboProfile->sizePolicy().hasHeightForWidth());
        comboProfile->setSizePolicy(sizePolicy);
        comboProfile->setMaximumWidth(250);

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, comboProfile);

        labelProfileName = new QLabel(groupProfiles);
        labelProfileName->setObjectName("labelProfileName");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, labelProfileName);

        lineProfileName = new QLineEdit(groupProfiles);
        lineProfileName->setObjectName("lineProfileName");

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, lineProfileName);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        btnSaveProfile = new QPushButton(groupProfiles);
        btnSaveProfile->setObjectName("btnSaveProfile");

        horizontalLayout->addWidget(btnSaveProfile);

        btnLoadProfile = new QPushButton(groupProfiles);
        btnLoadProfile->setObjectName("btnLoadProfile");

        horizontalLayout->addWidget(btnLoadProfile);

        btnDeleteProfile = new QPushButton(groupProfiles);
        btnDeleteProfile->setObjectName("btnDeleteProfile");

        horizontalLayout->addWidget(btnDeleteProfile);

        horizontalSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);


        formLayout->setLayout(2, QFormLayout::ItemRole::SpanningRole, horizontalLayout);


        verticalLayout->addWidget(groupProfiles);

        groupDetection = new QGroupBox(TabProtection);
        groupDetection->setObjectName("groupDetection");
        verticalLayout_2 = new QVBoxLayout(groupDetection);
        verticalLayout_2->setObjectName("verticalLayout_2");
        checkAutoDetect = new QCheckBox(groupDetection);
        checkAutoDetect->setObjectName("checkAutoDetect");
        checkAutoDetect->setChecked(true);

        verticalLayout_2->addWidget(checkAutoDetect);

        checkPreserveProtection = new QCheckBox(groupDetection);
        checkPreserveProtection->setObjectName("checkPreserveProtection");
        checkPreserveProtection->setChecked(true);

        verticalLayout_2->addWidget(checkPreserveProtection);

        checkReportProtection = new QCheckBox(groupDetection);
        checkReportProtection->setObjectName("checkReportProtection");
        checkReportProtection->setChecked(true);

        verticalLayout_2->addWidget(checkReportProtection);

        checkLogDetails = new QCheckBox(groupDetection);
        checkLogDetails->setObjectName("checkLogDetails");

        verticalLayout_2->addWidget(checkLogDetails);


        verticalLayout->addWidget(groupDetection);

        tabProtectionTypes = new QTabWidget(TabProtection);
        tabProtectionTypes->setObjectName("tabProtectionTypes");
        tabXCopy = new QWidget();
        tabXCopy->setObjectName("tabXCopy");
        verticalLayout_3 = new QVBoxLayout(tabXCopy);
        verticalLayout_3->setObjectName("verticalLayout_3");
        checkXCopyEnable = new QCheckBox(tabXCopy);
        checkXCopyEnable->setObjectName("checkXCopyEnable");
        checkXCopyEnable->setChecked(true);

        verticalLayout_3->addWidget(checkXCopyEnable);

        labelXCopyInfo = new QLabel(tabXCopy);
        labelXCopyInfo->setObjectName("labelXCopyInfo");

        verticalLayout_3->addWidget(labelXCopyInfo);

        checkErr1 = new QCheckBox(tabXCopy);
        checkErr1->setObjectName("checkErr1");
        checkErr1->setEnabled(false);

        verticalLayout_3->addWidget(checkErr1);

        checkErr2 = new QCheckBox(tabXCopy);
        checkErr2->setObjectName("checkErr2");
        checkErr2->setEnabled(false);

        verticalLayout_3->addWidget(checkErr2);

        checkErr3 = new QCheckBox(tabXCopy);
        checkErr3->setObjectName("checkErr3");
        checkErr3->setEnabled(false);

        verticalLayout_3->addWidget(checkErr3);

        checkErr4 = new QCheckBox(tabXCopy);
        checkErr4->setObjectName("checkErr4");
        checkErr4->setEnabled(false);

        verticalLayout_3->addWidget(checkErr4);

        checkErr5 = new QCheckBox(tabXCopy);
        checkErr5->setObjectName("checkErr5");
        checkErr5->setEnabled(false);

        verticalLayout_3->addWidget(checkErr5);

        checkErr6 = new QCheckBox(tabXCopy);
        checkErr6->setObjectName("checkErr6");
        checkErr6->setEnabled(false);

        verticalLayout_3->addWidget(checkErr6);

        checkErr7 = new QCheckBox(tabXCopy);
        checkErr7->setObjectName("checkErr7");
        checkErr7->setEnabled(false);

        verticalLayout_3->addWidget(checkErr7);

        checkErr8 = new QCheckBox(tabXCopy);
        checkErr8->setObjectName("checkErr8");
        checkErr8->setEnabled(false);

        verticalLayout_3->addWidget(checkErr8);

        verticalSpacer_2 = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_3->addItem(verticalSpacer_2);

        tabProtectionTypes->addTab(tabXCopy, QString());
        tabDiskDupe = new QWidget();
        tabDiskDupe->setObjectName("tabDiskDupe");
        verticalLayout_4 = new QVBoxLayout(tabDiskDupe);
        verticalLayout_4->setObjectName("verticalLayout_4");
        checkDDEnable = new QCheckBox(tabDiskDupe);
        checkDDEnable->setObjectName("checkDDEnable");
        checkDDEnable->setChecked(true);

        verticalLayout_4->addWidget(checkDDEnable);

        labelDDInfo = new QLabel(tabDiskDupe);
        labelDDInfo->setObjectName("labelDDInfo");

        verticalLayout_4->addWidget(labelDDInfo);

        checkDD1 = new QCheckBox(tabDiskDupe);
        checkDD1->setObjectName("checkDD1");
        checkDD1->setEnabled(false);

        verticalLayout_4->addWidget(checkDD1);

        checkDD2 = new QCheckBox(tabDiskDupe);
        checkDD2->setObjectName("checkDD2");
        checkDD2->setEnabled(false);

        verticalLayout_4->addWidget(checkDD2);

        checkDD3 = new QCheckBox(tabDiskDupe);
        checkDD3->setObjectName("checkDD3");
        checkDD3->setEnabled(false);

        verticalLayout_4->addWidget(checkDD3);

        checkDD4 = new QCheckBox(tabDiskDupe);
        checkDD4->setObjectName("checkDD4");
        checkDD4->setEnabled(false);

        verticalLayout_4->addWidget(checkDD4);

        checkDD5 = new QCheckBox(tabDiskDupe);
        checkDD5->setObjectName("checkDD5");
        checkDD5->setEnabled(false);

        verticalLayout_4->addWidget(checkDD5);

        checkDDExpertMode = new QCheckBox(tabDiskDupe);
        checkDDExpertMode->setObjectName("checkDDExpertMode");

        verticalLayout_4->addWidget(checkDDExpertMode);

        groupDDExpert = new QGroupBox(tabDiskDupe);
        groupDDExpert->setObjectName("groupDDExpert");
        groupDDExpert->setEnabled(false);
        formLayout_2 = new QFormLayout(groupDDExpert);
        formLayout_2->setObjectName("formLayout_2");
        labelHalfTrackThreshold = new QLabel(groupDDExpert);
        labelHalfTrackThreshold->setObjectName("labelHalfTrackThreshold");

        formLayout_2->setWidget(0, QFormLayout::ItemRole::LabelRole, labelHalfTrackThreshold);

        spinHalfTrackThreshold = new QSpinBox(groupDDExpert);
        spinHalfTrackThreshold->setObjectName("spinHalfTrackThreshold");
        spinHalfTrackThreshold->setMaximum(100);
        spinHalfTrackThreshold->setValue(50);

        formLayout_2->setWidget(0, QFormLayout::ItemRole::FieldRole, spinHalfTrackThreshold);

        labelSyncVariance = new QLabel(groupDDExpert);
        labelSyncVariance->setObjectName("labelSyncVariance");

        formLayout_2->setWidget(1, QFormLayout::ItemRole::LabelRole, labelSyncVariance);

        spinSyncVariance = new QSpinBox(groupDDExpert);
        spinSyncVariance->setObjectName("spinSyncVariance");
        spinSyncVariance->setMaximum(1000);
        spinSyncVariance->setValue(100);

        formLayout_2->setWidget(1, QFormLayout::ItemRole::FieldRole, spinSyncVariance);

        labelTimingTolerance = new QLabel(groupDDExpert);
        labelTimingTolerance->setObjectName("labelTimingTolerance");

        formLayout_2->setWidget(2, QFormLayout::ItemRole::LabelRole, labelTimingTolerance);

        spinTimingTolerance = new QSpinBox(groupDDExpert);
        spinTimingTolerance->setObjectName("spinTimingTolerance");
        spinTimingTolerance->setMaximum(100);
        spinTimingTolerance->setValue(10);

        formLayout_2->setWidget(2, QFormLayout::ItemRole::FieldRole, spinTimingTolerance);


        verticalLayout_4->addWidget(groupDDExpert);

        verticalSpacer_3 = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_4->addItem(verticalSpacer_3);

        tabProtectionTypes->addTab(tabDiskDupe, QString());
        tabC64Nibbler = new QWidget();
        tabC64Nibbler->setObjectName("tabC64Nibbler");
        verticalLayout_5 = new QVBoxLayout(tabC64Nibbler);
        verticalLayout_5->setObjectName("verticalLayout_5");
        checkC64Enable = new QCheckBox(tabC64Nibbler);
        checkC64Enable->setObjectName("checkC64Enable");
        checkC64Enable->setChecked(true);

        verticalLayout_5->addWidget(checkC64Enable);

        labelC64Info = new QLabel(tabC64Nibbler);
        labelC64Info->setObjectName("labelC64Info");
        labelC64Info->setStyleSheet(QString::fromUtf8("color: rgb(255, 128, 0); font-weight: bold;"));

        verticalLayout_5->addWidget(labelC64Info);

        groupGCR = new QGroupBox(tabC64Nibbler);
        groupGCR->setObjectName("groupGCR");
        formLayout_3 = new QFormLayout(groupGCR);
        formLayout_3->setObjectName("formLayout_3");
        labelSpeedZones = new QLabel(groupGCR);
        labelSpeedZones->setObjectName("labelSpeedZones");

        formLayout_3->setWidget(0, QFormLayout::ItemRole::LabelRole, labelSpeedZones);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        radioSpeedAuto = new QRadioButton(groupGCR);
        radioSpeedAuto->setObjectName("radioSpeedAuto");
        radioSpeedAuto->setChecked(true);

        horizontalLayout_2->addWidget(radioSpeedAuto);

        radioSpeedManual = new QRadioButton(groupGCR);
        radioSpeedManual->setObjectName("radioSpeedManual");

        horizontalLayout_2->addWidget(radioSpeedManual);


        formLayout_3->setLayout(0, QFormLayout::ItemRole::FieldRole, horizontalLayout_2);

        labelSync = new QLabel(groupGCR);
        labelSync->setObjectName("labelSync");

        formLayout_3->setWidget(1, QFormLayout::ItemRole::LabelRole, labelSync);

        comboSync = new QComboBox(groupGCR);
        comboSync->addItem(QString());
        comboSync->addItem(QString());
        comboSync->addItem(QString());
        comboSync->setObjectName("comboSync");
        sizePolicy.setHeightForWidth(comboSync->sizePolicy().hasHeightForWidth());
        comboSync->setSizePolicy(sizePolicy);
        comboSync->setMaximumWidth(250);

        formLayout_3->setWidget(1, QFormLayout::ItemRole::FieldRole, comboSync);

        labelGAP = new QLabel(groupGCR);
        labelGAP->setObjectName("labelGAP");

        formLayout_3->setWidget(2, QFormLayout::ItemRole::LabelRole, labelGAP);

        checkGAPAuto = new QCheckBox(groupGCR);
        checkGAPAuto->setObjectName("checkGAPAuto");
        checkGAPAuto->setChecked(true);

        formLayout_3->setWidget(2, QFormLayout::ItemRole::FieldRole, checkGAPAuto);


        verticalLayout_5->addWidget(groupGCR);

        groupHalfTrack = new QGroupBox(tabC64Nibbler);
        groupHalfTrack->setObjectName("groupHalfTrack");
        formLayout_4 = new QFormLayout(groupHalfTrack);
        formLayout_4->setObjectName("formLayout_4");
        checkHalfTrack = new QCheckBox(groupHalfTrack);
        checkHalfTrack->setObjectName("checkHalfTrack");
        checkHalfTrack->setChecked(true);

        formLayout_4->setWidget(0, QFormLayout::ItemRole::SpanningRole, checkHalfTrack);

        labelHalfTrackRange = new QLabel(groupHalfTrack);
        labelHalfTrackRange->setObjectName("labelHalfTrackRange");

        formLayout_4->setWidget(1, QFormLayout::ItemRole::LabelRole, labelHalfTrackRange);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        spinHalfTrackStart = new QDoubleSpinBox(groupHalfTrack);
        spinHalfTrackStart->setObjectName("spinHalfTrackStart");
        spinHalfTrackStart->setMaximum(41.500000000000000);
        spinHalfTrackStart->setSingleStep(0.500000000000000);

        horizontalLayout_3->addWidget(spinHalfTrackStart);

        labelTo = new QLabel(groupHalfTrack);
        labelTo->setObjectName("labelTo");

        horizontalLayout_3->addWidget(labelTo);

        spinHalfTrackEnd = new QDoubleSpinBox(groupHalfTrack);
        spinHalfTrackEnd->setObjectName("spinHalfTrackEnd");
        spinHalfTrackEnd->setMaximum(41.500000000000000);
        spinHalfTrackEnd->setSingleStep(0.500000000000000);
        spinHalfTrackEnd->setValue(40.500000000000000);

        horizontalLayout_3->addWidget(spinHalfTrackEnd);


        formLayout_4->setLayout(1, QFormLayout::ItemRole::FieldRole, horizontalLayout_3);

        labelHalfTrackStep = new QLabel(groupHalfTrack);
        labelHalfTrackStep->setObjectName("labelHalfTrackStep");

        formLayout_4->setWidget(2, QFormLayout::ItemRole::LabelRole, labelHalfTrackStep);

        comboHalfTrackStep = new QComboBox(groupHalfTrack);
        comboHalfTrackStep->addItem(QString());
        comboHalfTrackStep->addItem(QString());
        comboHalfTrackStep->addItem(QString());
        comboHalfTrackStep->setObjectName("comboHalfTrackStep");
        sizePolicy.setHeightForWidth(comboHalfTrackStep->sizePolicy().hasHeightForWidth());
        comboHalfTrackStep->setSizePolicy(sizePolicy);
        comboHalfTrackStep->setMaximumWidth(250);

        formLayout_4->setWidget(2, QFormLayout::ItemRole::FieldRole, comboHalfTrackStep);


        verticalLayout_5->addWidget(groupHalfTrack);

        groupC64Protection = new QGroupBox(tabC64Nibbler);
        groupC64Protection->setObjectName("groupC64Protection");
        gridLayout = new QGridLayout(groupC64Protection);
        gridLayout->setObjectName("gridLayout");
        checkC64WeakBits = new QCheckBox(groupC64Protection);
        checkC64WeakBits->setObjectName("checkC64WeakBits");
        checkC64WeakBits->setChecked(true);

        gridLayout->addWidget(checkC64WeakBits, 0, 0, 1, 1);

        checkC64VarTiming = new QCheckBox(groupC64Protection);
        checkC64VarTiming->setObjectName("checkC64VarTiming");
        checkC64VarTiming->setChecked(true);

        gridLayout->addWidget(checkC64VarTiming, 0, 1, 1, 1);

        checkC64Alignment = new QCheckBox(groupC64Protection);
        checkC64Alignment->setObjectName("checkC64Alignment");
        checkC64Alignment->setChecked(true);

        gridLayout->addWidget(checkC64Alignment, 1, 0, 1, 1);

        checkC64SectorCount = new QCheckBox(groupC64Protection);
        checkC64SectorCount->setObjectName("checkC64SectorCount");
        checkC64SectorCount->setChecked(true);

        gridLayout->addWidget(checkC64SectorCount, 1, 1, 1, 1);


        verticalLayout_5->addWidget(groupC64Protection);

        groupC64Output = new QGroupBox(tabC64Nibbler);
        groupC64Output->setObjectName("groupC64Output");
        verticalLayout_6 = new QVBoxLayout(groupC64Output);
        verticalLayout_6->setObjectName("verticalLayout_6");
        radioOutputG64 = new QRadioButton(groupC64Output);
        radioOutputG64->setObjectName("radioOutputG64");
        radioOutputG64->setChecked(true);

        verticalLayout_6->addWidget(radioOutputG64);

        radioOutputD64 = new QRadioButton(groupC64Output);
        radioOutputD64->setObjectName("radioOutputD64");

        verticalLayout_6->addWidget(radioOutputD64);

        radioOutputNIB = new QRadioButton(groupC64Output);
        radioOutputNIB->setObjectName("radioOutputNIB");

        verticalLayout_6->addWidget(radioOutputNIB);

        radioOutputFlux = new QRadioButton(groupC64Output);
        radioOutputFlux->setObjectName("radioOutputFlux");

        verticalLayout_6->addWidget(radioOutputFlux);


        verticalLayout_5->addWidget(groupC64Output);

        checkC64Expert = new QCheckBox(tabC64Nibbler);
        checkC64Expert->setObjectName("checkC64Expert");

        verticalLayout_5->addWidget(checkC64Expert);

        groupC64ExpertParams = new QGroupBox(tabC64Nibbler);
        groupC64ExpertParams->setObjectName("groupC64ExpertParams");
        groupC64ExpertParams->setEnabled(false);
        formLayout_5 = new QFormLayout(groupC64ExpertParams);
        formLayout_5->setObjectName("formLayout_5");
        labelGCRTolerance = new QLabel(groupC64ExpertParams);
        labelGCRTolerance->setObjectName("labelGCRTolerance");

        formLayout_5->setWidget(0, QFormLayout::ItemRole::LabelRole, labelGCRTolerance);

        spinGCRTolerance = new QSpinBox(groupC64ExpertParams);
        spinGCRTolerance->setObjectName("spinGCRTolerance");
        spinGCRTolerance->setMaximum(100);
        spinGCRTolerance->setValue(10);

        formLayout_5->setWidget(0, QFormLayout::ItemRole::FieldRole, spinGCRTolerance);

        labelBitSlip = new QLabel(groupC64ExpertParams);
        labelBitSlip->setObjectName("labelBitSlip");

        formLayout_5->setWidget(1, QFormLayout::ItemRole::LabelRole, labelBitSlip);

        checkBitSlip = new QCheckBox(groupC64ExpertParams);
        checkBitSlip->setObjectName("checkBitSlip");
        checkBitSlip->setChecked(true);

        formLayout_5->setWidget(1, QFormLayout::ItemRole::FieldRole, checkBitSlip);

        labelRevAlignment = new QLabel(groupC64ExpertParams);
        labelRevAlignment->setObjectName("labelRevAlignment");

        formLayout_5->setWidget(2, QFormLayout::ItemRole::LabelRole, labelRevAlignment);

        checkRevAlignment = new QCheckBox(groupC64ExpertParams);
        checkRevAlignment->setObjectName("checkRevAlignment");
        checkRevAlignment->setChecked(true);

        formLayout_5->setWidget(2, QFormLayout::ItemRole::FieldRole, checkRevAlignment);

        labelTrackVariance = new QLabel(groupC64ExpertParams);
        labelTrackVariance->setObjectName("labelTrackVariance");

        formLayout_5->setWidget(3, QFormLayout::ItemRole::LabelRole, labelTrackVariance);

        spinTrackVariance = new QSpinBox(groupC64ExpertParams);
        spinTrackVariance->setObjectName("spinTrackVariance");
        spinTrackVariance->setMaximum(1000);
        spinTrackVariance->setValue(100);

        formLayout_5->setWidget(3, QFormLayout::ItemRole::FieldRole, spinTrackVariance);


        verticalLayout_5->addWidget(groupC64ExpertParams);

        tabProtectionTypes->addTab(tabC64Nibbler, QString());
        tabFlags = new QWidget();
        tabFlags->setObjectName("tabFlags");
        verticalLayout_7 = new QVBoxLayout(tabFlags);
        verticalLayout_7->setObjectName("verticalLayout_7");
        labelFlags = new QLabel(tabFlags);
        labelFlags->setObjectName("labelFlags");

        verticalLayout_7->addWidget(labelFlags);

        groupFlags = new QGroupBox(tabFlags);
        groupFlags->setObjectName("groupFlags");
        gridLayout_2 = new QGridLayout(groupFlags);
        gridLayout_2->setObjectName("gridLayout_2");
        checkWeakBits = new QCheckBox(groupFlags);
        checkWeakBits->setObjectName("checkWeakBits");
        checkWeakBits->setEnabled(false);

        gridLayout_2->addWidget(checkWeakBits, 0, 0, 1, 1);

        checkLongTrack = new QCheckBox(groupFlags);
        checkLongTrack->setObjectName("checkLongTrack");
        checkLongTrack->setEnabled(false);

        gridLayout_2->addWidget(checkLongTrack, 0, 1, 1, 1);

        checkShortTrack = new QCheckBox(groupFlags);
        checkShortTrack->setObjectName("checkShortTrack");
        checkShortTrack->setEnabled(false);

        gridLayout_2->addWidget(checkShortTrack, 1, 0, 1, 1);

        checkBadCRC = new QCheckBox(groupFlags);
        checkBadCRC->setObjectName("checkBadCRC");
        checkBadCRC->setEnabled(false);

        gridLayout_2->addWidget(checkBadCRC, 1, 1, 1, 1);

        checkDupIDs = new QCheckBox(groupFlags);
        checkDupIDs->setObjectName("checkDupIDs");
        checkDupIDs->setEnabled(false);

        gridLayout_2->addWidget(checkDupIDs, 2, 0, 1, 1);

        checkNonStdGAP = new QCheckBox(groupFlags);
        checkNonStdGAP->setObjectName("checkNonStdGAP");
        checkNonStdGAP->setEnabled(false);

        gridLayout_2->addWidget(checkNonStdGAP, 2, 1, 1, 1);

        checkSyncAnomaly = new QCheckBox(groupFlags);
        checkSyncAnomaly->setObjectName("checkSyncAnomaly");
        checkSyncAnomaly->setEnabled(false);

        gridLayout_2->addWidget(checkSyncAnomaly, 3, 0, 1, 1);

        checkCustom = new QCheckBox(groupFlags);
        checkCustom->setObjectName("checkCustom");
        checkCustom->setEnabled(false);

        gridLayout_2->addWidget(checkCustom, 3, 1, 1, 1);


        verticalLayout_7->addWidget(groupFlags);

        verticalSpacer_4 = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_7->addItem(verticalSpacer_4);

        tabProtectionTypes->addTab(tabFlags, QString());

        verticalLayout->addWidget(tabProtectionTypes);


        retranslateUi(TabProtection);
        QObject::connect(checkDDExpertMode, &QCheckBox::toggled, groupDDExpert, &QGroupBox::setEnabled);
        QObject::connect(checkC64Expert, &QCheckBox::toggled, groupC64ExpertParams, &QGroupBox::setEnabled);

        tabProtectionTypes->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(TabProtection);
    } // setupUi

    void retranslateUi(QWidget *TabProtection)
    {
        groupProfiles->setTitle(QCoreApplication::translate("TabProtection", "Protection Profiles (Auto-activates Sub-Tabs)", nullptr));
        labelProfile->setText(QCoreApplication::translate("TabProtection", "Active Profile:", nullptr));
        comboProfile->setItemText(0, QCoreApplication::translate("TabProtection", "Custom (User-Defined)", nullptr));
        comboProfile->setItemText(1, QCoreApplication::translate("TabProtection", "Amiga Standard (X-Copy + dd*)", nullptr));
        comboProfile->setItemText(2, QCoreApplication::translate("TabProtection", "Amiga Advanced (Rob Northen, NDOS, etc.)", nullptr));
        comboProfile->setItemText(3, QCoreApplication::translate("TabProtection", "C64 Standard (Weak Bits, GCR) \342\206\222 Activates C64 Tab!", nullptr));
        comboProfile->setItemText(4, QCoreApplication::translate("TabProtection", "C64 Advanced (Half-Tracks, Nibbler) \342\206\222 Activates C64 Tab!", nullptr));
        comboProfile->setItemText(5, QCoreApplication::translate("TabProtection", "Atari Standard (Bad Sectors, Phantom Sectors)", nullptr));
        comboProfile->setItemText(6, QCoreApplication::translate("TabProtection", "Atari Advanced (Happy, Speedy, Duplicator)", nullptr));
        comboProfile->setItemText(7, QCoreApplication::translate("TabProtection", "PC DOS (Weak Bits, Track Timing)", nullptr));
        comboProfile->setItemText(8, QCoreApplication::translate("TabProtection", "Apple II (Nibble Count, Self-Sync)", nullptr));
        comboProfile->setItemText(9, QCoreApplication::translate("TabProtection", "Archive Mode (Preserve EVERYTHING!)", nullptr));
        comboProfile->setItemText(10, QCoreApplication::translate("TabProtection", "Quick Mode (Ignore Protection, Fast Read)", nullptr));

        labelProfileName->setText(QCoreApplication::translate("TabProtection", "Profile Name:", nullptr));
        lineProfileName->setPlaceholderText(QCoreApplication::translate("TabProtection", "Enter custom profile name...", nullptr));
        btnSaveProfile->setText(QCoreApplication::translate("TabProtection", "Save Profile", nullptr));
        btnLoadProfile->setText(QCoreApplication::translate("TabProtection", "Load Profile", nullptr));
        btnDeleteProfile->setText(QCoreApplication::translate("TabProtection", "Delete Profile", nullptr));
        groupDetection->setTitle(QCoreApplication::translate("TabProtection", "General Protection Detection", nullptr));
        checkAutoDetect->setText(QCoreApplication::translate("TabProtection", "Auto-Detect Copy Protection", nullptr));
        checkPreserveProtection->setText(QCoreApplication::translate("TabProtection", "Preserve Protection Features", nullptr));
        checkReportProtection->setText(QCoreApplication::translate("TabProtection", "Report Protection Type", nullptr));
        checkLogDetails->setText(QCoreApplication::translate("TabProtection", "Log Protection Details", nullptr));
        checkXCopyEnable->setText(QCoreApplication::translate("TabProtection", "Enable X-Copy Analysis", nullptr));
        labelXCopyInfo->setText(QCoreApplication::translate("TabProtection", "X-Copy Errors (read-only, auto-detected):", nullptr));
        checkErr1->setText(QCoreApplication::translate("TabProtection", "Error 1: Sector Count != 11 (PROTECTION!)", nullptr));
        checkErr2->setText(QCoreApplication::translate("TabProtection", "Error 2: No Sync Found", nullptr));
        checkErr3->setText(QCoreApplication::translate("TabProtection", "Error 3: No Sync After GAP", nullptr));
        checkErr4->setText(QCoreApplication::translate("TabProtection", "Error 4: Header Checksum Error", nullptr));
        checkErr5->setText(QCoreApplication::translate("TabProtection", "Error 5: Header/Format Long Error", nullptr));
        checkErr6->setText(QCoreApplication::translate("TabProtection", "Error 6: Datablock Checksum Error", nullptr));
        checkErr7->setText(QCoreApplication::translate("TabProtection", "Error 7: Long Track (PROTECTION!)", nullptr));
        checkErr8->setText(QCoreApplication::translate("TabProtection", "Error 8: Verify Error", nullptr));
        tabProtectionTypes->setTabText(tabProtectionTypes->indexOf(tabXCopy), QCoreApplication::translate("TabProtection", "X-Copy (Amiga)", nullptr));
        checkDDEnable->setText(QCoreApplication::translate("TabProtection", "Enable Recovery (dd*) Analysis", nullptr));
        labelDDInfo->setText(QCoreApplication::translate("TabProtection", "Recovery Errors (read-only, auto-detected):", nullptr));
        checkDD1->setText(QCoreApplication::translate("TabProtection", "dd1: Half-Track Detection", nullptr));
        checkDD2->setText(QCoreApplication::translate("TabProtection", "dd2: Variable Sync Detection", nullptr));
        checkDD3->setText(QCoreApplication::translate("TabProtection", "dd3: Track Checksum Mismatch", nullptr));
        checkDD4->setText(QCoreApplication::translate("TabProtection", "dd4: Sector Timing Anomaly", nullptr));
        checkDD5->setText(QCoreApplication::translate("TabProtection", "dd5: Custom Sync Marks", nullptr));
        checkDDExpertMode->setText(QCoreApplication::translate("TabProtection", "Expert Mode (Show Advanced Parameters)", nullptr));
        groupDDExpert->setTitle(QCoreApplication::translate("TabProtection", "Recovery Expert Parameters", nullptr));
        labelHalfTrackThreshold->setText(QCoreApplication::translate("TabProtection", "Half-Track Threshold:", nullptr));
        labelSyncVariance->setText(QCoreApplication::translate("TabProtection", "Sync Variance (\302\265s):", nullptr));
        labelTimingTolerance->setText(QCoreApplication::translate("TabProtection", "Timing Tolerance (%):", nullptr));
        tabProtectionTypes->setTabText(tabProtectionTypes->indexOf(tabDiskDupe), QCoreApplication::translate("TabProtection", "Recovery", nullptr));
        checkC64Enable->setText(QCoreApplication::translate("TabProtection", "Enable C64 Nibbler Mode (GCR Format)", nullptr));
        labelC64Info->setText(QCoreApplication::translate("TabProtection", "\342\232\240\357\270\217 C64 Nibbler Mode replaces Format/Geometry tabs for this disk!", nullptr));
        groupGCR->setTitle(QCoreApplication::translate("TabProtection", "GCR Settings", nullptr));
        labelSpeedZones->setText(QCoreApplication::translate("TabProtection", "Speed Zones:", nullptr));
        radioSpeedAuto->setText(QCoreApplication::translate("TabProtection", "Auto (Standard 1541)", nullptr));
        radioSpeedManual->setText(QCoreApplication::translate("TabProtection", "Manual", nullptr));
        labelSync->setText(QCoreApplication::translate("TabProtection", "Sync Detection:", nullptr));
        comboSync->setItemText(0, QCoreApplication::translate("TabProtection", "Standard ($FF \303\227 10)", nullptr));
        comboSync->setItemText(1, QCoreApplication::translate("TabProtection", "Custom Sync (Protection)", nullptr));
        comboSync->setItemText(2, QCoreApplication::translate("TabProtection", "Variable Sync", nullptr));

        labelGAP->setText(QCoreApplication::translate("TabProtection", "GAP Detection:", nullptr));
        checkGAPAuto->setText(QCoreApplication::translate("TabProtection", "Auto-Detect GAP", nullptr));
        groupHalfTrack->setTitle(QCoreApplication::translate("TabProtection", "Half-Track Options", nullptr));
        checkHalfTrack->setText(QCoreApplication::translate("TabProtection", "Enable Half-Track Detection (0.5 steps)", nullptr));
        labelHalfTrackRange->setText(QCoreApplication::translate("TabProtection", "Track Range:", nullptr));
        labelTo->setText(QCoreApplication::translate("TabProtection", "to", nullptr));
        labelHalfTrackStep->setText(QCoreApplication::translate("TabProtection", "Step Size:", nullptr));
        comboHalfTrackStep->setItemText(0, QCoreApplication::translate("TabProtection", "0.5 (Standard)", nullptr));
        comboHalfTrackStep->setItemText(1, QCoreApplication::translate("TabProtection", "0.25 (High Precision)", nullptr));
        comboHalfTrackStep->setItemText(2, QCoreApplication::translate("TabProtection", "1.0 (Full Tracks Only)", nullptr));

        groupC64Protection->setTitle(QCoreApplication::translate("TabProtection", "C64 Protection Detection", nullptr));
        checkC64WeakBits->setText(QCoreApplication::translate("TabProtection", "Weak Bits", nullptr));
        checkC64VarTiming->setText(QCoreApplication::translate("TabProtection", "Variable Timing", nullptr));
        checkC64Alignment->setText(QCoreApplication::translate("TabProtection", "Track Alignment Issues", nullptr));
        checkC64SectorCount->setText(QCoreApplication::translate("TabProtection", "Sector Count Anomalies", nullptr));
        groupC64Output->setTitle(QCoreApplication::translate("TabProtection", "Output Format", nullptr));
        radioOutputG64->setText(QCoreApplication::translate("TabProtection", "G64 (GCR with Alignment) - Recommended for Protection!", nullptr));
        radioOutputD64->setText(QCoreApplication::translate("TabProtection", "D64 (Standard) - Loses Protection!", nullptr));
        radioOutputNIB->setText(QCoreApplication::translate("TabProtection", "NIB (Nibbler Format)", nullptr));
        radioOutputFlux->setText(QCoreApplication::translate("TabProtection", "Raw Flux (Complete)", nullptr));
        checkC64Expert->setText(QCoreApplication::translate("TabProtection", "Expert Mode (Advanced Parameters)", nullptr));
        groupC64ExpertParams->setTitle(QCoreApplication::translate("TabProtection", "C64 Expert Parameters", nullptr));
        labelGCRTolerance->setText(QCoreApplication::translate("TabProtection", "GCR Decode Tolerance (%):", nullptr));
        labelBitSlip->setText(QCoreApplication::translate("TabProtection", "Bit Slip Correction:", nullptr));
        checkBitSlip->setText(QCoreApplication::translate("TabProtection", "Enable", nullptr));
        labelRevAlignment->setText(QCoreApplication::translate("TabProtection", "Revolution Alignment:", nullptr));
        checkRevAlignment->setText(QCoreApplication::translate("TabProtection", "Enable", nullptr));
        labelTrackVariance->setText(QCoreApplication::translate("TabProtection", "Track Variance Threshold:", nullptr));
        tabProtectionTypes->setTabText(tabProtectionTypes->indexOf(tabC64Nibbler), QCoreApplication::translate("TabProtection", "C64 Nibbler ", nullptr));
        labelFlags->setText(QCoreApplication::translate("TabProtection", "Copy Protection Flags (UFM) - Auto-detected, read-only:", nullptr));
        groupFlags->setTitle(QCoreApplication::translate("TabProtection", "Detected Protection Features", nullptr));
        checkWeakBits->setText(QCoreApplication::translate("TabProtection", "Weak Bits", nullptr));
        checkLongTrack->setText(QCoreApplication::translate("TabProtection", "Long Track", nullptr));
        checkShortTrack->setText(QCoreApplication::translate("TabProtection", "Short Track", nullptr));
        checkBadCRC->setText(QCoreApplication::translate("TabProtection", "Bad CRC (intentional)", nullptr));
        checkDupIDs->setText(QCoreApplication::translate("TabProtection", "Duplicate IDs", nullptr));
        checkNonStdGAP->setText(QCoreApplication::translate("TabProtection", "Non-Standard GAP", nullptr));
        checkSyncAnomaly->setText(QCoreApplication::translate("TabProtection", "Sync Anomaly", nullptr));
        checkCustom->setText(QCoreApplication::translate("TabProtection", "Custom Protection", nullptr));
        tabProtectionTypes->setTabText(tabProtectionTypes->indexOf(tabFlags), QCoreApplication::translate("TabProtection", "Protection Flags", nullptr));
        (void)TabProtection;
    } // retranslateUi

};

namespace Ui {
    class TabProtection: public Ui_TabProtection {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TAB_PROTECTION_H

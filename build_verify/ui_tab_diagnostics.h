/********************************************************************************
** Form generated from reading UI file 'tab_diagnostics.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TAB_DIAGNOSTICS_H
#define UI_TAB_DIAGNOSTICS_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TabDiagnostics
{
public:
    QVBoxLayout *verticalLayout;
    QGroupBox *groupRPMMeasurement;
    QGridLayout *gridLayoutRPM;
    QLabel *labelMeasurementCycles;
    QSpinBox *spinMeasurementCycles;
    QLabel *labelWarmupRotations;
    QSpinBox *spinWarmupRotations;
    QLabel *labelSlidingWindow;
    QSpinBox *spinSlidingWindow;
    QLabel *labelTestCylinder;
    QSpinBox *spinTestCylinder;
    QPushButton *btnMeasureRPM;
    QGroupBox *groupRPMResult;
    QFormLayout *formLayoutRPMResult;
    QLabel *labelResultRotTime;
    QLineEdit *lineResultRotTime;
    QLabel *labelResultRPM;
    QLineEdit *lineResultRPM;
    QLabel *labelResultDeviation;
    QLineEdit *lineResultDeviation;
    QLabel *labelResultCapacity;
    QLineEdit *lineResultCapacity;
    QLabel *labelResultDataRate;
    QLineEdit *lineResultDataRate;
    QGroupBox *groupDriftJitter;
    QGridLayout *gridLayoutDrift;
    QLabel *labelRevsPerPass;
    QSpinBox *spinRevsPerPass;
    QLabel *labelPasses;
    QSpinBox *spinPasses;
    QPushButton *btnDriftTest;
    QGroupBox *groupDriftResult;
    QFormLayout *formLayoutDriftResult;
    QLabel *labelJitterPkPk;
    QLineEdit *lineJitterPkPk;
    QLabel *labelJitterStdev;
    QLineEdit *lineJitterStdev;
    QLabel *labelDriftTotal;
    QLineEdit *lineDriftTotal;
    QLabel *labelDriftAssessment;
    QLineEdit *lineDriftAssessment;
    QGroupBox *groupGWStats;
    QVBoxLayout *verticalLayoutGWStats;
    QHBoxLayout *horizontalLayoutGWControls;
    QPushButton *btnRefreshStats;
    QCheckBox *checkAutoRefresh;
    QDoubleSpinBox *spinRefreshInterval;
    QPushButton *btnResetStats;
    QSpacerItem *horizontalSpacer;
    QGroupBox *groupErrorStats;
    QGridLayout *gridLayoutErrorStats;
    QLabel *labelBadCmd;
    QLineEdit *lineBadCmd;
    QLabel *labelNoIndex;
    QLineEdit *lineNoIndex;
    QLabel *labelNoTrk0;
    QLineEdit *lineNoTrk0;
    QLabel *labelWrProt;
    QLineEdit *lineWrProt;
    QLabel *labelOverflow;
    QLineEdit *lineOverflow;
    QLabel *labelUnderflow;
    QLineEdit *lineUnderflow;
    QGroupBox *groupIndexFilter;
    QGridLayout *gridLayoutIndexFilter;
    QLabel *labelAccepted;
    QLineEdit *lineAccepted;
    QLabel *labelMasked;
    QLineEdit *lineMasked;
    QLabel *labelGlitch;
    QLineEdit *lineGlitch;
    QGroupBox *groupIndexWrite;
    QGridLayout *gridLayoutIndexWrite;
    QLabel *labelIdxStarts;
    QLineEdit *lineIdxStarts;
    QLabel *labelIdxMin;
    QLineEdit *lineIdxMin;
    QLabel *labelIdxMax;
    QLineEdit *lineIdxMax;
    QLabel *labelIdxAvg;
    QLineEdit *lineIdxAvg;
    QGroupBox *groupCMOSTable;
    QVBoxLayout *verticalLayoutCMOS;
    QTableWidget *tableCMOS;
    QSpacerItem *verticalSpacer;

    void setupUi(QWidget *TabDiagnostics)
    {
        if (TabDiagnostics->objectName().isEmpty())
            TabDiagnostics->setObjectName("TabDiagnostics");
        TabDiagnostics->resize(850, 700);
        verticalLayout = new QVBoxLayout(TabDiagnostics);
        verticalLayout->setObjectName("verticalLayout");
        groupRPMMeasurement = new QGroupBox(TabDiagnostics);
        groupRPMMeasurement->setObjectName("groupRPMMeasurement");
        gridLayoutRPM = new QGridLayout(groupRPMMeasurement);
        gridLayoutRPM->setObjectName("gridLayoutRPM");
        labelMeasurementCycles = new QLabel(groupRPMMeasurement);
        labelMeasurementCycles->setObjectName("labelMeasurementCycles");

        gridLayoutRPM->addWidget(labelMeasurementCycles, 0, 0, 1, 1);

        spinMeasurementCycles = new QSpinBox(groupRPMMeasurement);
        spinMeasurementCycles->setObjectName("spinMeasurementCycles");
        spinMeasurementCycles->setMinimum(100);
        spinMeasurementCycles->setMaximum(10000);
        spinMeasurementCycles->setValue(1000);

        gridLayoutRPM->addWidget(spinMeasurementCycles, 0, 1, 1, 1);

        labelWarmupRotations = new QLabel(groupRPMMeasurement);
        labelWarmupRotations->setObjectName("labelWarmupRotations");

        gridLayoutRPM->addWidget(labelWarmupRotations, 0, 2, 1, 1);

        spinWarmupRotations = new QSpinBox(groupRPMMeasurement);
        spinWarmupRotations->setObjectName("spinWarmupRotations");
        spinWarmupRotations->setMinimum(10);
        spinWarmupRotations->setMaximum(500);
        spinWarmupRotations->setValue(100);

        gridLayoutRPM->addWidget(spinWarmupRotations, 0, 3, 1, 1);

        labelSlidingWindow = new QLabel(groupRPMMeasurement);
        labelSlidingWindow->setObjectName("labelSlidingWindow");

        gridLayoutRPM->addWidget(labelSlidingWindow, 1, 0, 1, 1);

        spinSlidingWindow = new QSpinBox(groupRPMMeasurement);
        spinSlidingWindow->setObjectName("spinSlidingWindow");
        spinSlidingWindow->setMinimum(5);
        spinSlidingWindow->setMaximum(50);
        spinSlidingWindow->setValue(10);

        gridLayoutRPM->addWidget(spinSlidingWindow, 1, 1, 1, 1);

        labelTestCylinder = new QLabel(groupRPMMeasurement);
        labelTestCylinder->setObjectName("labelTestCylinder");

        gridLayoutRPM->addWidget(labelTestCylinder, 1, 2, 1, 1);

        spinTestCylinder = new QSpinBox(groupRPMMeasurement);
        spinTestCylinder->setObjectName("spinTestCylinder");
        spinTestCylinder->setMinimum(0);
        spinTestCylinder->setMaximum(84);
        spinTestCylinder->setValue(0);

        gridLayoutRPM->addWidget(spinTestCylinder, 1, 3, 1, 1);

        btnMeasureRPM = new QPushButton(groupRPMMeasurement);
        btnMeasureRPM->setObjectName("btnMeasureRPM");

        gridLayoutRPM->addWidget(btnMeasureRPM, 2, 0, 1, 4);

        groupRPMResult = new QGroupBox(groupRPMMeasurement);
        groupRPMResult->setObjectName("groupRPMResult");
        formLayoutRPMResult = new QFormLayout(groupRPMResult);
        formLayoutRPMResult->setObjectName("formLayoutRPMResult");
        labelResultRotTime = new QLabel(groupRPMResult);
        labelResultRotTime->setObjectName("labelResultRotTime");

        formLayoutRPMResult->setWidget(0, QFormLayout::ItemRole::LabelRole, labelResultRotTime);

        lineResultRotTime = new QLineEdit(groupRPMResult);
        lineResultRotTime->setObjectName("lineResultRotTime");
        lineResultRotTime->setReadOnly(true);

        formLayoutRPMResult->setWidget(0, QFormLayout::ItemRole::FieldRole, lineResultRotTime);

        labelResultRPM = new QLabel(groupRPMResult);
        labelResultRPM->setObjectName("labelResultRPM");

        formLayoutRPMResult->setWidget(1, QFormLayout::ItemRole::LabelRole, labelResultRPM);

        lineResultRPM = new QLineEdit(groupRPMResult);
        lineResultRPM->setObjectName("lineResultRPM");
        lineResultRPM->setReadOnly(true);

        formLayoutRPMResult->setWidget(1, QFormLayout::ItemRole::FieldRole, lineResultRPM);

        labelResultDeviation = new QLabel(groupRPMResult);
        labelResultDeviation->setObjectName("labelResultDeviation");

        formLayoutRPMResult->setWidget(2, QFormLayout::ItemRole::LabelRole, labelResultDeviation);

        lineResultDeviation = new QLineEdit(groupRPMResult);
        lineResultDeviation->setObjectName("lineResultDeviation");
        lineResultDeviation->setReadOnly(true);

        formLayoutRPMResult->setWidget(2, QFormLayout::ItemRole::FieldRole, lineResultDeviation);

        labelResultCapacity = new QLabel(groupRPMResult);
        labelResultCapacity->setObjectName("labelResultCapacity");

        formLayoutRPMResult->setWidget(3, QFormLayout::ItemRole::LabelRole, labelResultCapacity);

        lineResultCapacity = new QLineEdit(groupRPMResult);
        lineResultCapacity->setObjectName("lineResultCapacity");
        lineResultCapacity->setReadOnly(true);

        formLayoutRPMResult->setWidget(3, QFormLayout::ItemRole::FieldRole, lineResultCapacity);

        labelResultDataRate = new QLabel(groupRPMResult);
        labelResultDataRate->setObjectName("labelResultDataRate");

        formLayoutRPMResult->setWidget(4, QFormLayout::ItemRole::LabelRole, labelResultDataRate);

        lineResultDataRate = new QLineEdit(groupRPMResult);
        lineResultDataRate->setObjectName("lineResultDataRate");
        lineResultDataRate->setReadOnly(true);

        formLayoutRPMResult->setWidget(4, QFormLayout::ItemRole::FieldRole, lineResultDataRate);


        gridLayoutRPM->addWidget(groupRPMResult, 3, 0, 1, 4);


        verticalLayout->addWidget(groupRPMMeasurement);

        groupDriftJitter = new QGroupBox(TabDiagnostics);
        groupDriftJitter->setObjectName("groupDriftJitter");
        gridLayoutDrift = new QGridLayout(groupDriftJitter);
        gridLayoutDrift->setObjectName("gridLayoutDrift");
        labelRevsPerPass = new QLabel(groupDriftJitter);
        labelRevsPerPass->setObjectName("labelRevsPerPass");

        gridLayoutDrift->addWidget(labelRevsPerPass, 0, 0, 1, 1);

        spinRevsPerPass = new QSpinBox(groupDriftJitter);
        spinRevsPerPass->setObjectName("spinRevsPerPass");
        spinRevsPerPass->setMinimum(10);
        spinRevsPerPass->setMaximum(200);
        spinRevsPerPass->setValue(50);

        gridLayoutDrift->addWidget(spinRevsPerPass, 0, 1, 1, 1);

        labelPasses = new QLabel(groupDriftJitter);
        labelPasses->setObjectName("labelPasses");

        gridLayoutDrift->addWidget(labelPasses, 0, 2, 1, 1);

        spinPasses = new QSpinBox(groupDriftJitter);
        spinPasses->setObjectName("spinPasses");
        spinPasses->setMinimum(1);
        spinPasses->setMaximum(20);
        spinPasses->setValue(5);

        gridLayoutDrift->addWidget(spinPasses, 0, 3, 1, 1);

        btnDriftTest = new QPushButton(groupDriftJitter);
        btnDriftTest->setObjectName("btnDriftTest");

        gridLayoutDrift->addWidget(btnDriftTest, 1, 0, 1, 4);

        groupDriftResult = new QGroupBox(groupDriftJitter);
        groupDriftResult->setObjectName("groupDriftResult");
        formLayoutDriftResult = new QFormLayout(groupDriftResult);
        formLayoutDriftResult->setObjectName("formLayoutDriftResult");
        labelJitterPkPk = new QLabel(groupDriftResult);
        labelJitterPkPk->setObjectName("labelJitterPkPk");

        formLayoutDriftResult->setWidget(0, QFormLayout::ItemRole::LabelRole, labelJitterPkPk);

        lineJitterPkPk = new QLineEdit(groupDriftResult);
        lineJitterPkPk->setObjectName("lineJitterPkPk");
        lineJitterPkPk->setReadOnly(true);

        formLayoutDriftResult->setWidget(0, QFormLayout::ItemRole::FieldRole, lineJitterPkPk);

        labelJitterStdev = new QLabel(groupDriftResult);
        labelJitterStdev->setObjectName("labelJitterStdev");

        formLayoutDriftResult->setWidget(1, QFormLayout::ItemRole::LabelRole, labelJitterStdev);

        lineJitterStdev = new QLineEdit(groupDriftResult);
        lineJitterStdev->setObjectName("lineJitterStdev");
        lineJitterStdev->setReadOnly(true);

        formLayoutDriftResult->setWidget(1, QFormLayout::ItemRole::FieldRole, lineJitterStdev);

        labelDriftTotal = new QLabel(groupDriftResult);
        labelDriftTotal->setObjectName("labelDriftTotal");

        formLayoutDriftResult->setWidget(2, QFormLayout::ItemRole::LabelRole, labelDriftTotal);

        lineDriftTotal = new QLineEdit(groupDriftResult);
        lineDriftTotal->setObjectName("lineDriftTotal");
        lineDriftTotal->setReadOnly(true);

        formLayoutDriftResult->setWidget(2, QFormLayout::ItemRole::FieldRole, lineDriftTotal);

        labelDriftAssessment = new QLabel(groupDriftResult);
        labelDriftAssessment->setObjectName("labelDriftAssessment");

        formLayoutDriftResult->setWidget(3, QFormLayout::ItemRole::LabelRole, labelDriftAssessment);

        lineDriftAssessment = new QLineEdit(groupDriftResult);
        lineDriftAssessment->setObjectName("lineDriftAssessment");
        lineDriftAssessment->setReadOnly(true);

        formLayoutDriftResult->setWidget(3, QFormLayout::ItemRole::FieldRole, lineDriftAssessment);


        gridLayoutDrift->addWidget(groupDriftResult, 2, 0, 1, 4);


        verticalLayout->addWidget(groupDriftJitter);

        groupGWStats = new QGroupBox(TabDiagnostics);
        groupGWStats->setObjectName("groupGWStats");
        verticalLayoutGWStats = new QVBoxLayout(groupGWStats);
        verticalLayoutGWStats->setObjectName("verticalLayoutGWStats");
        horizontalLayoutGWControls = new QHBoxLayout();
        horizontalLayoutGWControls->setObjectName("horizontalLayoutGWControls");
        btnRefreshStats = new QPushButton(groupGWStats);
        btnRefreshStats->setObjectName("btnRefreshStats");

        horizontalLayoutGWControls->addWidget(btnRefreshStats);

        checkAutoRefresh = new QCheckBox(groupGWStats);
        checkAutoRefresh->setObjectName("checkAutoRefresh");

        horizontalLayoutGWControls->addWidget(checkAutoRefresh);

        spinRefreshInterval = new QDoubleSpinBox(groupGWStats);
        spinRefreshInterval->setObjectName("spinRefreshInterval");
        spinRefreshInterval->setMinimum(0.100000000000000);
        spinRefreshInterval->setMaximum(5.000000000000000);
        spinRefreshInterval->setSingleStep(0.100000000000000);
        spinRefreshInterval->setValue(0.500000000000000);

        horizontalLayoutGWControls->addWidget(spinRefreshInterval);

        btnResetStats = new QPushButton(groupGWStats);
        btnResetStats->setObjectName("btnResetStats");

        horizontalLayoutGWControls->addWidget(btnResetStats);

        horizontalSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayoutGWControls->addItem(horizontalSpacer);


        verticalLayoutGWStats->addLayout(horizontalLayoutGWControls);

        groupErrorStats = new QGroupBox(groupGWStats);
        groupErrorStats->setObjectName("groupErrorStats");
        gridLayoutErrorStats = new QGridLayout(groupErrorStats);
        gridLayoutErrorStats->setObjectName("gridLayoutErrorStats");
        labelBadCmd = new QLabel(groupErrorStats);
        labelBadCmd->setObjectName("labelBadCmd");

        gridLayoutErrorStats->addWidget(labelBadCmd, 0, 0, 1, 1);

        lineBadCmd = new QLineEdit(groupErrorStats);
        lineBadCmd->setObjectName("lineBadCmd");
        lineBadCmd->setReadOnly(true);
        lineBadCmd->setAlignment(Qt::AlignRight);

        gridLayoutErrorStats->addWidget(lineBadCmd, 0, 1, 1, 1);

        labelNoIndex = new QLabel(groupErrorStats);
        labelNoIndex->setObjectName("labelNoIndex");

        gridLayoutErrorStats->addWidget(labelNoIndex, 0, 2, 1, 1);

        lineNoIndex = new QLineEdit(groupErrorStats);
        lineNoIndex->setObjectName("lineNoIndex");
        lineNoIndex->setReadOnly(true);
        lineNoIndex->setAlignment(Qt::AlignRight);

        gridLayoutErrorStats->addWidget(lineNoIndex, 0, 3, 1, 1);

        labelNoTrk0 = new QLabel(groupErrorStats);
        labelNoTrk0->setObjectName("labelNoTrk0");

        gridLayoutErrorStats->addWidget(labelNoTrk0, 0, 4, 1, 1);

        lineNoTrk0 = new QLineEdit(groupErrorStats);
        lineNoTrk0->setObjectName("lineNoTrk0");
        lineNoTrk0->setReadOnly(true);
        lineNoTrk0->setAlignment(Qt::AlignRight);

        gridLayoutErrorStats->addWidget(lineNoTrk0, 0, 5, 1, 1);

        labelWrProt = new QLabel(groupErrorStats);
        labelWrProt->setObjectName("labelWrProt");

        gridLayoutErrorStats->addWidget(labelWrProt, 1, 0, 1, 1);

        lineWrProt = new QLineEdit(groupErrorStats);
        lineWrProt->setObjectName("lineWrProt");
        lineWrProt->setReadOnly(true);
        lineWrProt->setAlignment(Qt::AlignRight);

        gridLayoutErrorStats->addWidget(lineWrProt, 1, 1, 1, 1);

        labelOverflow = new QLabel(groupErrorStats);
        labelOverflow->setObjectName("labelOverflow");

        gridLayoutErrorStats->addWidget(labelOverflow, 1, 2, 1, 1);

        lineOverflow = new QLineEdit(groupErrorStats);
        lineOverflow->setObjectName("lineOverflow");
        lineOverflow->setReadOnly(true);
        lineOverflow->setAlignment(Qt::AlignRight);

        gridLayoutErrorStats->addWidget(lineOverflow, 1, 3, 1, 1);

        labelUnderflow = new QLabel(groupErrorStats);
        labelUnderflow->setObjectName("labelUnderflow");

        gridLayoutErrorStats->addWidget(labelUnderflow, 1, 4, 1, 1);

        lineUnderflow = new QLineEdit(groupErrorStats);
        lineUnderflow->setObjectName("lineUnderflow");
        lineUnderflow->setReadOnly(true);
        lineUnderflow->setAlignment(Qt::AlignRight);

        gridLayoutErrorStats->addWidget(lineUnderflow, 1, 5, 1, 1);


        verticalLayoutGWStats->addWidget(groupErrorStats);

        groupIndexFilter = new QGroupBox(groupGWStats);
        groupIndexFilter->setObjectName("groupIndexFilter");
        gridLayoutIndexFilter = new QGridLayout(groupIndexFilter);
        gridLayoutIndexFilter->setObjectName("gridLayoutIndexFilter");
        labelAccepted = new QLabel(groupIndexFilter);
        labelAccepted->setObjectName("labelAccepted");

        gridLayoutIndexFilter->addWidget(labelAccepted, 0, 0, 1, 1);

        lineAccepted = new QLineEdit(groupIndexFilter);
        lineAccepted->setObjectName("lineAccepted");
        lineAccepted->setReadOnly(true);
        lineAccepted->setAlignment(Qt::AlignRight);

        gridLayoutIndexFilter->addWidget(lineAccepted, 0, 1, 1, 1);

        labelMasked = new QLabel(groupIndexFilter);
        labelMasked->setObjectName("labelMasked");

        gridLayoutIndexFilter->addWidget(labelMasked, 0, 2, 1, 1);

        lineMasked = new QLineEdit(groupIndexFilter);
        lineMasked->setObjectName("lineMasked");
        lineMasked->setReadOnly(true);
        lineMasked->setAlignment(Qt::AlignRight);

        gridLayoutIndexFilter->addWidget(lineMasked, 0, 3, 1, 1);

        labelGlitch = new QLabel(groupIndexFilter);
        labelGlitch->setObjectName("labelGlitch");

        gridLayoutIndexFilter->addWidget(labelGlitch, 0, 4, 1, 1);

        lineGlitch = new QLineEdit(groupIndexFilter);
        lineGlitch->setObjectName("lineGlitch");
        lineGlitch->setReadOnly(true);
        lineGlitch->setAlignment(Qt::AlignRight);

        gridLayoutIndexFilter->addWidget(lineGlitch, 0, 5, 1, 1);


        verticalLayoutGWStats->addWidget(groupIndexFilter);

        groupIndexWrite = new QGroupBox(groupGWStats);
        groupIndexWrite->setObjectName("groupIndexWrite");
        gridLayoutIndexWrite = new QGridLayout(groupIndexWrite);
        gridLayoutIndexWrite->setObjectName("gridLayoutIndexWrite");
        labelIdxStarts = new QLabel(groupIndexWrite);
        labelIdxStarts->setObjectName("labelIdxStarts");

        gridLayoutIndexWrite->addWidget(labelIdxStarts, 0, 0, 1, 1);

        lineIdxStarts = new QLineEdit(groupIndexWrite);
        lineIdxStarts->setObjectName("lineIdxStarts");
        lineIdxStarts->setReadOnly(true);
        lineIdxStarts->setAlignment(Qt::AlignRight);

        gridLayoutIndexWrite->addWidget(lineIdxStarts, 0, 1, 1, 1);

        labelIdxMin = new QLabel(groupIndexWrite);
        labelIdxMin->setObjectName("labelIdxMin");

        gridLayoutIndexWrite->addWidget(labelIdxMin, 0, 2, 1, 1);

        lineIdxMin = new QLineEdit(groupIndexWrite);
        lineIdxMin->setObjectName("lineIdxMin");
        lineIdxMin->setReadOnly(true);
        lineIdxMin->setAlignment(Qt::AlignRight);

        gridLayoutIndexWrite->addWidget(lineIdxMin, 0, 3, 1, 1);

        labelIdxMax = new QLabel(groupIndexWrite);
        labelIdxMax->setObjectName("labelIdxMax");

        gridLayoutIndexWrite->addWidget(labelIdxMax, 0, 4, 1, 1);

        lineIdxMax = new QLineEdit(groupIndexWrite);
        lineIdxMax->setObjectName("lineIdxMax");
        lineIdxMax->setReadOnly(true);
        lineIdxMax->setAlignment(Qt::AlignRight);

        gridLayoutIndexWrite->addWidget(lineIdxMax, 0, 5, 1, 1);

        labelIdxAvg = new QLabel(groupIndexWrite);
        labelIdxAvg->setObjectName("labelIdxAvg");

        gridLayoutIndexWrite->addWidget(labelIdxAvg, 0, 6, 1, 1);

        lineIdxAvg = new QLineEdit(groupIndexWrite);
        lineIdxAvg->setObjectName("lineIdxAvg");
        lineIdxAvg->setReadOnly(true);
        lineIdxAvg->setAlignment(Qt::AlignRight);

        gridLayoutIndexWrite->addWidget(lineIdxAvg, 0, 7, 1, 1);


        verticalLayoutGWStats->addWidget(groupIndexWrite);


        verticalLayout->addWidget(groupGWStats);

        groupCMOSTable = new QGroupBox(TabDiagnostics);
        groupCMOSTable->setObjectName("groupCMOSTable");
        verticalLayoutCMOS = new QVBoxLayout(groupCMOSTable);
        verticalLayoutCMOS->setObjectName("verticalLayoutCMOS");
        tableCMOS = new QTableWidget(groupCMOSTable);
        if (tableCMOS->columnCount() < 5)
            tableCMOS->setColumnCount(5);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        tableCMOS->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        tableCMOS->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        tableCMOS->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        tableCMOS->setHorizontalHeaderItem(3, __qtablewidgetitem3);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        tableCMOS->setHorizontalHeaderItem(4, __qtablewidgetitem4);
        tableCMOS->setObjectName("tableCMOS");
        tableCMOS->setMaximumHeight(150);
        tableCMOS->setRowCount(6);
        tableCMOS->setColumnCount(5);

        verticalLayoutCMOS->addWidget(tableCMOS);


        verticalLayout->addWidget(groupCMOSTable);

        verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        retranslateUi(TabDiagnostics);

        QMetaObject::connectSlotsByName(TabDiagnostics);
    } // setupUi

    void retranslateUi(QWidget *TabDiagnostics)
    {
        groupRPMMeasurement->setTitle(QCoreApplication::translate("TabDiagnostics", "RPM-Messung (fdutils/floppymeter)", nullptr));
        labelMeasurementCycles->setText(QCoreApplication::translate("TabDiagnostics", "Messzyklen:", nullptr));
#if QT_CONFIG(tooltip)
        spinMeasurementCycles->setToolTip(QCoreApplication::translate("TabDiagnostics", "Anzahl Messzyklen f\303\274r RPM-Messung", nullptr));
#endif // QT_CONFIG(tooltip)
        labelWarmupRotations->setText(QCoreApplication::translate("TabDiagnostics", "Warmup-Rotationen:", nullptr));
        labelSlidingWindow->setText(QCoreApplication::translate("TabDiagnostics", "Sliding-Window:", nullptr));
        labelTestCylinder->setText(QCoreApplication::translate("TabDiagnostics", "Test-Zylinder:", nullptr));
        btnMeasureRPM->setText(QCoreApplication::translate("TabDiagnostics", "\360\237\224\254 RPM messen", nullptr));
        groupRPMResult->setTitle(QCoreApplication::translate("TabDiagnostics", "Messergebnis", nullptr));
        labelResultRotTime->setText(QCoreApplication::translate("TabDiagnostics", "Rotationszeit:", nullptr));
        lineResultRotTime->setPlaceholderText(QCoreApplication::translate("TabDiagnostics", "____ \302\265s (Soll: ____ \302\265s)", nullptr));
        labelResultRPM->setText(QCoreApplication::translate("TabDiagnostics", "RPM:", nullptr));
        lineResultRPM->setPlaceholderText(QCoreApplication::translate("TabDiagnostics", "____ (Soll: ____)", nullptr));
        labelResultDeviation->setText(QCoreApplication::translate("TabDiagnostics", "Abweichung:", nullptr));
        lineResultDeviation->setPlaceholderText(QCoreApplication::translate("TabDiagnostics", "____ PPM", nullptr));
        labelResultCapacity->setText(QCoreApplication::translate("TabDiagnostics", "Track-Kapazit\303\244t:", nullptr));
        lineResultCapacity->setPlaceholderText(QCoreApplication::translate("TabDiagnostics", "____ Half-Bits (Soll: ____)", nullptr));
        labelResultDataRate->setText(QCoreApplication::translate("TabDiagnostics", "Datenrate:", nullptr));
        lineResultDataRate->setPlaceholderText(QCoreApplication::translate("TabDiagnostics", "____ bit/s (Soll: ____)", nullptr));
        groupDriftJitter->setTitle(QCoreApplication::translate("TabDiagnostics", "Drift/Jitter-Test (gw_drift_jitter_test.py)", nullptr));
        labelRevsPerPass->setText(QCoreApplication::translate("TabDiagnostics", "Umdrehungen/Pass:", nullptr));
        labelPasses->setText(QCoreApplication::translate("TabDiagnostics", "Durchl\303\244ufe:", nullptr));
        btnDriftTest->setText(QCoreApplication::translate("TabDiagnostics", "\360\237\223\212 Drift-Test starten", nullptr));
        groupDriftResult->setTitle(QCoreApplication::translate("TabDiagnostics", "Testergebnis", nullptr));
        labelJitterPkPk->setText(QCoreApplication::translate("TabDiagnostics", "Jitter (pk-pk):", nullptr));
        lineJitterPkPk->setPlaceholderText(QCoreApplication::translate("TabDiagnostics", "____ Ticks", nullptr));
        labelJitterStdev->setText(QCoreApplication::translate("TabDiagnostics", "Jitter (stdev):", nullptr));
        lineJitterStdev->setPlaceholderText(QCoreApplication::translate("TabDiagnostics", "____ Ticks", nullptr));
        labelDriftTotal->setText(QCoreApplication::translate("TabDiagnostics", "Drift (gesamt):", nullptr));
        lineDriftTotal->setPlaceholderText(QCoreApplication::translate("TabDiagnostics", "____ Ticks \303\274ber ____ Passes", nullptr));
        labelDriftAssessment->setText(QCoreApplication::translate("TabDiagnostics", "Bewertung:", nullptr));
        lineDriftAssessment->setPlaceholderText(QCoreApplication::translate("TabDiagnostics", "[OK / WARNUNG / FEHLER]", nullptr));
        groupGWStats->setTitle(QCoreApplication::translate("TabDiagnostics", "Greaseweazle Live-Stats (gw_stats.py)", nullptr));
        btnRefreshStats->setText(QCoreApplication::translate("TabDiagnostics", "\360\237\224\204 Refresh", nullptr));
        checkAutoRefresh->setText(QCoreApplication::translate("TabDiagnostics", "Auto-Refresh", nullptr));
        spinRefreshInterval->setSuffix(QCoreApplication::translate("TabDiagnostics", " s", nullptr));
        btnResetStats->setText(QCoreApplication::translate("TabDiagnostics", "\360\237\227\221\357\270\217 Reset Stats", nullptr));
        groupErrorStats->setTitle(QCoreApplication::translate("TabDiagnostics", "Fehler-Statistik", nullptr));
        labelBadCmd->setText(QCoreApplication::translate("TabDiagnostics", "bad_cmd:", nullptr));
        lineBadCmd->setText(QCoreApplication::translate("TabDiagnostics", "0", nullptr));
        labelNoIndex->setText(QCoreApplication::translate("TabDiagnostics", "no_idx:", nullptr));
        lineNoIndex->setText(QCoreApplication::translate("TabDiagnostics", "0", nullptr));
        labelNoTrk0->setText(QCoreApplication::translate("TabDiagnostics", "no_trk0:", nullptr));
        lineNoTrk0->setText(QCoreApplication::translate("TabDiagnostics", "0", nullptr));
        labelWrProt->setText(QCoreApplication::translate("TabDiagnostics", "wrprot:", nullptr));
        lineWrProt->setText(QCoreApplication::translate("TabDiagnostics", "0", nullptr));
        labelOverflow->setText(QCoreApplication::translate("TabDiagnostics", "overflow:", nullptr));
        lineOverflow->setText(QCoreApplication::translate("TabDiagnostics", "0", nullptr));
        labelUnderflow->setText(QCoreApplication::translate("TabDiagnostics", "underflow:", nullptr));
        lineUnderflow->setText(QCoreApplication::translate("TabDiagnostics", "0", nullptr));
        groupIndexFilter->setTitle(QCoreApplication::translate("TabDiagnostics", "Index-Filter", nullptr));
        labelAccepted->setText(QCoreApplication::translate("TabDiagnostics", "accepted:", nullptr));
        lineAccepted->setText(QCoreApplication::translate("TabDiagnostics", "0", nullptr));
        labelMasked->setText(QCoreApplication::translate("TabDiagnostics", "masked:", nullptr));
        lineMasked->setText(QCoreApplication::translate("TabDiagnostics", "0", nullptr));
        labelGlitch->setText(QCoreApplication::translate("TabDiagnostics", "glitch:", nullptr));
        lineGlitch->setText(QCoreApplication::translate("TabDiagnostics", "0", nullptr));
        groupIndexWrite->setTitle(QCoreApplication::translate("TabDiagnostics", "Index-Write Timing", nullptr));
        labelIdxStarts->setText(QCoreApplication::translate("TabDiagnostics", "starts:", nullptr));
        lineIdxStarts->setText(QCoreApplication::translate("TabDiagnostics", "0", nullptr));
        labelIdxMin->setText(QCoreApplication::translate("TabDiagnostics", "min:", nullptr));
        lineIdxMin->setText(QCoreApplication::translate("TabDiagnostics", "0 \302\265s", nullptr));
        labelIdxMax->setText(QCoreApplication::translate("TabDiagnostics", "max:", nullptr));
        lineIdxMax->setText(QCoreApplication::translate("TabDiagnostics", "0 \302\265s", nullptr));
        labelIdxAvg->setText(QCoreApplication::translate("TabDiagnostics", "avg:", nullptr));
        lineIdxAvg->setText(QCoreApplication::translate("TabDiagnostics", "0 \302\265s", nullptr));
        groupCMOSTable->setTitle(QCoreApplication::translate("TabDiagnostics", "CMOS Drive-Type Referenz (fdutils)", nullptr));
        QTableWidgetItem *___qtablewidgetitem = tableCMOS->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("TabDiagnostics", "CMOS", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = tableCMOS->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("TabDiagnostics", "Typ", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = tableCMOS->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("TabDiagnostics", "Rotation (\302\265s)", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = tableCMOS->horizontalHeaderItem(3);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("TabDiagnostics", "RPM", nullptr));
        QTableWidgetItem *___qtablewidgetitem4 = tableCMOS->horizontalHeaderItem(4);
        ___qtablewidgetitem4->setText(QCoreApplication::translate("TabDiagnostics", "Datenrate", nullptr));
        (void)TabDiagnostics;
    } // retranslateUi

};

namespace Ui {
    class TabDiagnostics: public Ui_TabDiagnostics {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TAB_DIAGNOSTICS_H

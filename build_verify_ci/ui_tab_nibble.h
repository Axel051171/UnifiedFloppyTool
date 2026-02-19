/********************************************************************************
** Form generated from reading UI file 'tab_nibble.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TAB_NIBBLE_H
#define UI_TAB_NIBBLE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TabNibble
{
public:
    QHBoxLayout *horizontalLayout_main;
    QVBoxLayout *verticalLayout_left;
    QGroupBox *groupReadMode;
    QFormLayout *formLayout_read;
    QLabel *labelMode;
    QComboBox *comboReadMode;
    QLabel *labelRevolutions;
    QSpinBox *spinRevolutions;
    QCheckBox *checkReadBetweenIndex;
    QLabel *labelIndexToIndex;
    QDoubleSpinBox *spinIndexToIndex;
    QGroupBox *groupGCR;
    QVBoxLayout *verticalLayout_gcr;
    QCheckBox *checkGCRMode;
    QFormLayout *formLayout;
    QLabel *labelGCRType;
    QComboBox *comboGCRType;
    QCheckBox *checkDecodeGCR;
    QCheckBox *checkPreserveSync;
    QHBoxLayout *hboxLayout;
    QLabel *labelSyncLength;
    QSpinBox *spinSyncLength;
    QGroupBox *groupTiming;
    QVBoxLayout *verticalLayout_timing;
    QCheckBox *checkPreserveTiming;
    QFormLayout *formLayout1;
    QLabel *labelBitTolerance;
    QDoubleSpinBox *spinBitTolerance;
    QCheckBox *checkDetectWeakBits;
    QCheckBox *checkMarkWeakBits;
    QSpacerItem *spacerItem;
    QVBoxLayout *verticalLayout_right;
    QGroupBox *groupHalfTracks;
    QVBoxLayout *verticalLayout_half;
    QCheckBox *checkReadHalfTracks;
    QCheckBox *checkAnalyzeHalfTracks;
    QFormLayout *formLayout2;
    QLabel *labelHalfTrackOffset;
    QDoubleSpinBox *spinHalfTrackOffset;
    QGroupBox *groupDensity;
    QVBoxLayout *verticalLayout_density;
    QCheckBox *checkVariableDensity;
    QFormLayout *formLayout3;
    QLabel *labelDensityZones;
    QSpinBox *spinDensityZones;
    QCheckBox *checkAutoDetectDensity;
    QGroupBox *groupOutput;
    QVBoxLayout *verticalLayout_output;
    QCheckBox *checkCreateNIB;
    QCheckBox *checkCreateG64;
    QCheckBox *checkIncludeTiming;
    QCheckBox *checkIncludeRawFlux;
    QSpacerItem *spacerItem1;
    QGroupBox *groupAnalysis;
    QVBoxLayout *vboxLayout;
    QTableWidget *tableTrackAnalysis;

    void setupUi(QWidget *TabNibble)
    {
        if (TabNibble->objectName().isEmpty())
            TabNibble->setObjectName("TabNibble");
        TabNibble->resize(900, 700);
        horizontalLayout_main = new QHBoxLayout(TabNibble);
        horizontalLayout_main->setObjectName("horizontalLayout_main");
        verticalLayout_left = new QVBoxLayout();
        verticalLayout_left->setObjectName("verticalLayout_left");
        groupReadMode = new QGroupBox(TabNibble);
        groupReadMode->setObjectName("groupReadMode");
        formLayout_read = new QFormLayout(groupReadMode);
        formLayout_read->setObjectName("formLayout_read");
        labelMode = new QLabel(groupReadMode);
        labelMode->setObjectName("labelMode");

        formLayout_read->setWidget(0, QFormLayout::ItemRole::LabelRole, labelMode);

        comboReadMode = new QComboBox(groupReadMode);
        comboReadMode->addItem(QString());
        comboReadMode->addItem(QString());
        comboReadMode->addItem(QString());
        comboReadMode->setObjectName("comboReadMode");
        comboReadMode->setMaximumWidth(180);

        formLayout_read->setWidget(0, QFormLayout::ItemRole::FieldRole, comboReadMode);

        labelRevolutions = new QLabel(groupReadMode);
        labelRevolutions->setObjectName("labelRevolutions");

        formLayout_read->setWidget(1, QFormLayout::ItemRole::LabelRole, labelRevolutions);

        spinRevolutions = new QSpinBox(groupReadMode);
        spinRevolutions->setObjectName("spinRevolutions");
        spinRevolutions->setMinimum(1);
        spinRevolutions->setMaximum(10);
        spinRevolutions->setValue(3);
        spinRevolutions->setMaximumWidth(80);

        formLayout_read->setWidget(1, QFormLayout::ItemRole::FieldRole, spinRevolutions);

        checkReadBetweenIndex = new QCheckBox(groupReadMode);
        checkReadBetweenIndex->setObjectName("checkReadBetweenIndex");
        checkReadBetweenIndex->setChecked(true);

        formLayout_read->setWidget(2, QFormLayout::ItemRole::SpanningRole, checkReadBetweenIndex);

        labelIndexToIndex = new QLabel(groupReadMode);
        labelIndexToIndex->setObjectName("labelIndexToIndex");

        formLayout_read->setWidget(3, QFormLayout::ItemRole::LabelRole, labelIndexToIndex);

        spinIndexToIndex = new QDoubleSpinBox(groupReadMode);
        spinIndexToIndex->setObjectName("spinIndexToIndex");
        spinIndexToIndex->setMinimum(100);
        spinIndexToIndex->setMaximum(250);
        spinIndexToIndex->setValue(200);
        spinIndexToIndex->setMaximumWidth(100);

        formLayout_read->setWidget(3, QFormLayout::ItemRole::FieldRole, spinIndexToIndex);


        verticalLayout_left->addWidget(groupReadMode);

        groupGCR = new QGroupBox(TabNibble);
        groupGCR->setObjectName("groupGCR");
        verticalLayout_gcr = new QVBoxLayout(groupGCR);
        verticalLayout_gcr->setObjectName("verticalLayout_gcr");
        checkGCRMode = new QCheckBox(groupGCR);
        checkGCRMode->setObjectName("checkGCRMode");

        verticalLayout_gcr->addWidget(checkGCRMode);

        formLayout = new QFormLayout();
        formLayout->setObjectName("formLayout");
        labelGCRType = new QLabel(groupGCR);
        labelGCRType->setObjectName("labelGCRType");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, labelGCRType);

        comboGCRType = new QComboBox(groupGCR);
        comboGCRType->addItem(QString());
        comboGCRType->addItem(QString());
        comboGCRType->addItem(QString());
        comboGCRType->setObjectName("comboGCRType");
        comboGCRType->setMaximumWidth(180);

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, comboGCRType);


        verticalLayout_gcr->addLayout(formLayout);

        checkDecodeGCR = new QCheckBox(groupGCR);
        checkDecodeGCR->setObjectName("checkDecodeGCR");
        checkDecodeGCR->setChecked(true);

        verticalLayout_gcr->addWidget(checkDecodeGCR);

        checkPreserveSync = new QCheckBox(groupGCR);
        checkPreserveSync->setObjectName("checkPreserveSync");
        checkPreserveSync->setChecked(true);

        verticalLayout_gcr->addWidget(checkPreserveSync);

        hboxLayout = new QHBoxLayout();
        hboxLayout->setObjectName("hboxLayout");
        labelSyncLength = new QLabel(groupGCR);
        labelSyncLength->setObjectName("labelSyncLength");

        hboxLayout->addWidget(labelSyncLength);

        spinSyncLength = new QSpinBox(groupGCR);
        spinSyncLength->setObjectName("spinSyncLength");
        spinSyncLength->setMinimum(1);
        spinSyncLength->setMaximum(100);
        spinSyncLength->setValue(10);
        spinSyncLength->setMaximumWidth(80);

        hboxLayout->addWidget(spinSyncLength);


        verticalLayout_gcr->addLayout(hboxLayout);


        verticalLayout_left->addWidget(groupGCR);

        groupTiming = new QGroupBox(TabNibble);
        groupTiming->setObjectName("groupTiming");
        verticalLayout_timing = new QVBoxLayout(groupTiming);
        verticalLayout_timing->setObjectName("verticalLayout_timing");
        checkPreserveTiming = new QCheckBox(groupTiming);
        checkPreserveTiming->setObjectName("checkPreserveTiming");
        checkPreserveTiming->setChecked(true);

        verticalLayout_timing->addWidget(checkPreserveTiming);

        formLayout1 = new QFormLayout();
        formLayout1->setObjectName("formLayout1");
        labelBitTolerance = new QLabel(groupTiming);
        labelBitTolerance->setObjectName("labelBitTolerance");

        formLayout1->setWidget(0, QFormLayout::ItemRole::LabelRole, labelBitTolerance);

        spinBitTolerance = new QDoubleSpinBox(groupTiming);
        spinBitTolerance->setObjectName("spinBitTolerance");
        spinBitTolerance->setMinimum(1);
        spinBitTolerance->setMaximum(50);
        spinBitTolerance->setValue(10);
        spinBitTolerance->setMaximumWidth(100);

        formLayout1->setWidget(0, QFormLayout::ItemRole::FieldRole, spinBitTolerance);


        verticalLayout_timing->addLayout(formLayout1);

        checkDetectWeakBits = new QCheckBox(groupTiming);
        checkDetectWeakBits->setObjectName("checkDetectWeakBits");
        checkDetectWeakBits->setChecked(true);

        verticalLayout_timing->addWidget(checkDetectWeakBits);

        checkMarkWeakBits = new QCheckBox(groupTiming);
        checkMarkWeakBits->setObjectName("checkMarkWeakBits");

        verticalLayout_timing->addWidget(checkMarkWeakBits);


        verticalLayout_left->addWidget(groupTiming);

        spacerItem = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_left->addItem(spacerItem);


        horizontalLayout_main->addLayout(verticalLayout_left);

        verticalLayout_right = new QVBoxLayout();
        verticalLayout_right->setObjectName("verticalLayout_right");
        groupHalfTracks = new QGroupBox(TabNibble);
        groupHalfTracks->setObjectName("groupHalfTracks");
        verticalLayout_half = new QVBoxLayout(groupHalfTracks);
        verticalLayout_half->setObjectName("verticalLayout_half");
        checkReadHalfTracks = new QCheckBox(groupHalfTracks);
        checkReadHalfTracks->setObjectName("checkReadHalfTracks");

        verticalLayout_half->addWidget(checkReadHalfTracks);

        checkAnalyzeHalfTracks = new QCheckBox(groupHalfTracks);
        checkAnalyzeHalfTracks->setObjectName("checkAnalyzeHalfTracks");

        verticalLayout_half->addWidget(checkAnalyzeHalfTracks);

        formLayout2 = new QFormLayout();
        formLayout2->setObjectName("formLayout2");
        labelHalfTrackOffset = new QLabel(groupHalfTracks);
        labelHalfTrackOffset->setObjectName("labelHalfTrackOffset");

        formLayout2->setWidget(0, QFormLayout::ItemRole::LabelRole, labelHalfTrackOffset);

        spinHalfTrackOffset = new QDoubleSpinBox(groupHalfTracks);
        spinHalfTrackOffset->setObjectName("spinHalfTrackOffset");
        spinHalfTrackOffset->setMinimum(-1);
        spinHalfTrackOffset->setMaximum(1);
        spinHalfTrackOffset->setSingleStep(0);
        spinHalfTrackOffset->setValue(0);
        spinHalfTrackOffset->setMaximumWidth(100);

        formLayout2->setWidget(0, QFormLayout::ItemRole::FieldRole, spinHalfTrackOffset);


        verticalLayout_half->addLayout(formLayout2);


        verticalLayout_right->addWidget(groupHalfTracks);

        groupDensity = new QGroupBox(TabNibble);
        groupDensity->setObjectName("groupDensity");
        verticalLayout_density = new QVBoxLayout(groupDensity);
        verticalLayout_density->setObjectName("verticalLayout_density");
        checkVariableDensity = new QCheckBox(groupDensity);
        checkVariableDensity->setObjectName("checkVariableDensity");

        verticalLayout_density->addWidget(checkVariableDensity);

        formLayout3 = new QFormLayout();
        formLayout3->setObjectName("formLayout3");
        labelDensityZones = new QLabel(groupDensity);
        labelDensityZones->setObjectName("labelDensityZones");

        formLayout3->setWidget(0, QFormLayout::ItemRole::LabelRole, labelDensityZones);

        spinDensityZones = new QSpinBox(groupDensity);
        spinDensityZones->setObjectName("spinDensityZones");
        spinDensityZones->setMinimum(1);
        spinDensityZones->setMaximum(10);
        spinDensityZones->setValue(4);
        spinDensityZones->setMaximumWidth(80);

        formLayout3->setWidget(0, QFormLayout::ItemRole::FieldRole, spinDensityZones);


        verticalLayout_density->addLayout(formLayout3);

        checkAutoDetectDensity = new QCheckBox(groupDensity);
        checkAutoDetectDensity->setObjectName("checkAutoDetectDensity");
        checkAutoDetectDensity->setChecked(true);

        verticalLayout_density->addWidget(checkAutoDetectDensity);


        verticalLayout_right->addWidget(groupDensity);

        groupOutput = new QGroupBox(TabNibble);
        groupOutput->setObjectName("groupOutput");
        verticalLayout_output = new QVBoxLayout(groupOutput);
        verticalLayout_output->setObjectName("verticalLayout_output");
        checkCreateNIB = new QCheckBox(groupOutput);
        checkCreateNIB->setObjectName("checkCreateNIB");

        verticalLayout_output->addWidget(checkCreateNIB);

        checkCreateG64 = new QCheckBox(groupOutput);
        checkCreateG64->setObjectName("checkCreateG64");
        checkCreateG64->setChecked(true);

        verticalLayout_output->addWidget(checkCreateG64);

        checkIncludeTiming = new QCheckBox(groupOutput);
        checkIncludeTiming->setObjectName("checkIncludeTiming");
        checkIncludeTiming->setChecked(true);

        verticalLayout_output->addWidget(checkIncludeTiming);

        checkIncludeRawFlux = new QCheckBox(groupOutput);
        checkIncludeRawFlux->setObjectName("checkIncludeRawFlux");

        verticalLayout_output->addWidget(checkIncludeRawFlux);


        verticalLayout_right->addWidget(groupOutput);

        spacerItem1 = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_right->addItem(spacerItem1);

        groupAnalysis = new QGroupBox(TabNibble);
        groupAnalysis->setObjectName("groupAnalysis");
        vboxLayout = new QVBoxLayout(groupAnalysis);
        vboxLayout->setObjectName("vboxLayout");
        tableTrackAnalysis = new QTableWidget(groupAnalysis);
        if (tableTrackAnalysis->columnCount() < 5)
            tableTrackAnalysis->setColumnCount(5);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        tableTrackAnalysis->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        tableTrackAnalysis->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        tableTrackAnalysis->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        tableTrackAnalysis->setHorizontalHeaderItem(3, __qtablewidgetitem3);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        tableTrackAnalysis->setHorizontalHeaderItem(4, __qtablewidgetitem4);
        tableTrackAnalysis->setObjectName("tableTrackAnalysis");
        tableTrackAnalysis->setMaximumHeight(150);
        tableTrackAnalysis->setColumnCount(5);

        vboxLayout->addWidget(tableTrackAnalysis);


        verticalLayout_right->addWidget(groupAnalysis);


        horizontalLayout_main->addLayout(verticalLayout_right);


        retranslateUi(TabNibble);

        QMetaObject::connectSlotsByName(TabNibble);
    } // setupUi

    void retranslateUi(QWidget *TabNibble)
    {
        groupReadMode->setTitle(QCoreApplication::translate("TabNibble", "Read Mode", nullptr));
        labelMode->setText(QCoreApplication::translate("TabNibble", "Mode:", nullptr));
        comboReadMode->setItemText(0, QCoreApplication::translate("TabNibble", "Normal", nullptr));
        comboReadMode->setItemText(1, QCoreApplication::translate("TabNibble", "Raw", nullptr));
        comboReadMode->setItemText(2, QCoreApplication::translate("TabNibble", "Flux", nullptr));

        labelRevolutions->setText(QCoreApplication::translate("TabNibble", "Revolutions:", nullptr));
        checkReadBetweenIndex->setText(QCoreApplication::translate("TabNibble", "Read between index", nullptr));
        labelIndexToIndex->setText(QCoreApplication::translate("TabNibble", "Index to index:", nullptr));
        spinIndexToIndex->setSuffix(QCoreApplication::translate("TabNibble", " ms", nullptr));
        groupGCR->setTitle(QCoreApplication::translate("TabNibble", "GCR Settings (Commodore/Apple)", nullptr));
        checkGCRMode->setText(QCoreApplication::translate("TabNibble", "GCR Mode", nullptr));
        labelGCRType->setText(QCoreApplication::translate("TabNibble", "GCR Type:", nullptr));
        comboGCRType->setItemText(0, QCoreApplication::translate("TabNibble", "C64/1541", nullptr));
        comboGCRType->setItemText(1, QCoreApplication::translate("TabNibble", "Apple II 5.25\"", nullptr));
        comboGCRType->setItemText(2, QCoreApplication::translate("TabNibble", "Apple 3.5\"", nullptr));

        checkDecodeGCR->setText(QCoreApplication::translate("TabNibble", "Decode GCR", nullptr));
        checkPreserveSync->setText(QCoreApplication::translate("TabNibble", "Preserve sync marks", nullptr));
        labelSyncLength->setText(QCoreApplication::translate("TabNibble", "Min sync length:", nullptr));
        groupTiming->setTitle(QCoreApplication::translate("TabNibble", "Timing", nullptr));
        checkPreserveTiming->setText(QCoreApplication::translate("TabNibble", "Preserve original timing", nullptr));
        labelBitTolerance->setText(QCoreApplication::translate("TabNibble", "Bit time tolerance:", nullptr));
        spinBitTolerance->setSuffix(QCoreApplication::translate("TabNibble", " %", nullptr));
        checkDetectWeakBits->setText(QCoreApplication::translate("TabNibble", "Detect weak bits", nullptr));
        checkMarkWeakBits->setText(QCoreApplication::translate("TabNibble", "Mark weak bits in output", nullptr));
        groupHalfTracks->setTitle(QCoreApplication::translate("TabNibble", "Half Tracks", nullptr));
        checkReadHalfTracks->setText(QCoreApplication::translate("TabNibble", "Read half tracks", nullptr));
        checkAnalyzeHalfTracks->setText(QCoreApplication::translate("TabNibble", "Analyze half tracks", nullptr));
        labelHalfTrackOffset->setText(QCoreApplication::translate("TabNibble", "Half track offset:", nullptr));
        groupDensity->setTitle(QCoreApplication::translate("TabNibble", "Density", nullptr));
        checkVariableDensity->setText(QCoreApplication::translate("TabNibble", "Variable density", nullptr));
        labelDensityZones->setText(QCoreApplication::translate("TabNibble", "Density zones:", nullptr));
        checkAutoDetectDensity->setText(QCoreApplication::translate("TabNibble", "Auto-detect density", nullptr));
        groupOutput->setTitle(QCoreApplication::translate("TabNibble", "Output Format", nullptr));
        checkCreateNIB->setText(QCoreApplication::translate("TabNibble", "Create .NIB file", nullptr));
        checkCreateG64->setText(QCoreApplication::translate("TabNibble", "Create .G64 file", nullptr));
        checkIncludeTiming->setText(QCoreApplication::translate("TabNibble", "Include timing data", nullptr));
        checkIncludeRawFlux->setText(QCoreApplication::translate("TabNibble", "Include raw flux data", nullptr));
        groupAnalysis->setTitle(QCoreApplication::translate("TabNibble", "Track Analysis", nullptr));
        QTableWidgetItem *___qtablewidgetitem = tableTrackAnalysis->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("TabNibble", "Track", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = tableTrackAnalysis->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("TabNibble", "Side", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = tableTrackAnalysis->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("TabNibble", "Bits", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = tableTrackAnalysis->horizontalHeaderItem(3);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("TabNibble", "Sync", nullptr));
        QTableWidgetItem *___qtablewidgetitem4 = tableTrackAnalysis->horizontalHeaderItem(4);
        ___qtablewidgetitem4->setText(QCoreApplication::translate("TabNibble", "Status", nullptr));
        (void)TabNibble;
    } // retranslateUi

};

namespace Ui {
    class TabNibble: public Ui_TabNibble {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TAB_NIBBLE_H

/********************************************************************************
** Form generated from reading UI file 'diskanalyzer_window.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DISKANALYZER_WINDOW_H
#define UI_DISKANALYZER_WINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_DiskAnalyzerWindow
{
public:
    QVBoxLayout *verticalLayout_main;
    QHBoxLayout *mainContent;
    QVBoxLayout *diskLayout;
    QHBoxLayout *diskViewsLayout;
    QGroupBox *groupSide0;
    QVBoxLayout *vboxLayout;
    QLabel *labelSide0Info;
    QLabel *labelSide0Format;
    QFrame *frameDisk0;
    QGroupBox *groupSide1;
    QVBoxLayout *vboxLayout1;
    QLabel *labelSide1Info;
    QLabel *labelSide1Format;
    QFrame *frameDisk1;
    QGroupBox *groupViewControls;
    QHBoxLayout *hboxLayout;
    QLabel *label;
    QSlider *sliderOffset;
    QDoubleSpinBox *spinOffset;
    QLabel *label1;
    QSlider *sliderXScale;
    QLabel *label2;
    QSlider *sliderYScale;
    QRadioButton *radioTrackView;
    QRadioButton *radioDiskView;
    QVBoxLayout *rightPanel;
    QGroupBox *groupStatus;
    QVBoxLayout *vboxLayout2;
    QLabel *labelTrackInfo;
    QTextEdit *textSectorInfo;
    QGroupBox *groupHexDump;
    QVBoxLayout *vboxLayout3;
    QTextEdit *textHexDump;
    QGroupBox *groupTrackFormat;
    QGridLayout *gridLayout;
    QCheckBox *checkIsoMFM;
    QCheckBox *checkEEmu;
    QCheckBox *checkAED6200P;
    QCheckBox *checkIsoFM;
    QCheckBox *checkTYCOM;
    QCheckBox *checkMEMBRAIN;
    QCheckBox *checkAmigaMFM;
    QCheckBox *checkAppleII;
    QCheckBox *checkArburg;
    QCheckBox *checkC64GCR;
    QCheckBox *checkAtariFM;
    QCheckBox *checkUnknown;
    QGroupBox *groupSelection;
    QGridLayout *gridLayout1;
    QLabel *label3;
    QSpinBox *spinTrackNumber;
    QSlider *sliderTrack;
    QLabel *label4;
    QSpinBox *spinSideNumber;
    QSlider *sliderSide;
    QHBoxLayout *hboxLayout1;
    QPushButton *btnEditTools;
    QPushButton *btnExport;
    QPushButton *btnClose;
    QFrame *frameStatusBar;
    QHBoxLayout *hboxLayout2;
    QLabel *labelLibVersion;
    QSpacerItem *spacerItem;
    QLabel *labelCRC;

    void setupUi(QDialog *DiskAnalyzerWindow)
    {
        if (DiskAnalyzerWindow->objectName().isEmpty())
            DiskAnalyzerWindow->setObjectName("DiskAnalyzerWindow");
        DiskAnalyzerWindow->resize(1200, 750);
        DiskAnalyzerWindow->setMinimumSize(QSize(1000, 600));
        verticalLayout_main = new QVBoxLayout(DiskAnalyzerWindow);
        verticalLayout_main->setSpacing(4);
        verticalLayout_main->setObjectName("verticalLayout_main");
        verticalLayout_main->setContentsMargins(4, 4, 4, 4);
        mainContent = new QHBoxLayout();
        mainContent->setSpacing(6);
        mainContent->setObjectName("mainContent");
        diskLayout = new QVBoxLayout();
        diskLayout->setObjectName("diskLayout");
        diskViewsLayout = new QHBoxLayout();
        diskViewsLayout->setSpacing(8);
        diskViewsLayout->setObjectName("diskViewsLayout");
        groupSide0 = new QGroupBox(DiskAnalyzerWindow);
        groupSide0->setObjectName("groupSide0");
        vboxLayout = new QVBoxLayout(groupSide0);
        vboxLayout->setObjectName("vboxLayout");
        labelSide0Info = new QLabel(groupSide0);
        labelSide0Info->setObjectName("labelSide0Info");

        vboxLayout->addWidget(labelSide0Info);

        labelSide0Format = new QLabel(groupSide0);
        labelSide0Format->setObjectName("labelSide0Format");

        vboxLayout->addWidget(labelSide0Format);

        frameDisk0 = new QFrame(groupSide0);
        frameDisk0->setObjectName("frameDisk0");
        frameDisk0->setMinimumSize(QSize(350, 350));
        frameDisk0->setFrameShape(QFrame::Box);

        vboxLayout->addWidget(frameDisk0);


        diskViewsLayout->addWidget(groupSide0);

        groupSide1 = new QGroupBox(DiskAnalyzerWindow);
        groupSide1->setObjectName("groupSide1");
        vboxLayout1 = new QVBoxLayout(groupSide1);
        vboxLayout1->setObjectName("vboxLayout1");
        labelSide1Info = new QLabel(groupSide1);
        labelSide1Info->setObjectName("labelSide1Info");

        vboxLayout1->addWidget(labelSide1Info);

        labelSide1Format = new QLabel(groupSide1);
        labelSide1Format->setObjectName("labelSide1Format");

        vboxLayout1->addWidget(labelSide1Format);

        frameDisk1 = new QFrame(groupSide1);
        frameDisk1->setObjectName("frameDisk1");
        frameDisk1->setMinimumSize(QSize(350, 350));
        frameDisk1->setFrameShape(QFrame::Box);

        vboxLayout1->addWidget(frameDisk1);


        diskViewsLayout->addWidget(groupSide1);


        diskLayout->addLayout(diskViewsLayout);

        groupViewControls = new QGroupBox(DiskAnalyzerWindow);
        groupViewControls->setObjectName("groupViewControls");
        groupViewControls->setMaximumHeight(80);
        hboxLayout = new QHBoxLayout(groupViewControls);
        hboxLayout->setObjectName("hboxLayout");
        label = new QLabel(groupViewControls);
        label->setObjectName("label");

        hboxLayout->addWidget(label);

        sliderOffset = new QSlider(groupViewControls);
        sliderOffset->setObjectName("sliderOffset");
        sliderOffset->setOrientation(Qt::Horizontal);
        sliderOffset->setMaximum(100);

        hboxLayout->addWidget(sliderOffset);

        spinOffset = new QDoubleSpinBox(groupViewControls);
        spinOffset->setObjectName("spinOffset");
        spinOffset->setMaximum(100.000000000000000);
        spinOffset->setValue(85.000000000000000);

        hboxLayout->addWidget(spinOffset);

        label1 = new QLabel(groupViewControls);
        label1->setObjectName("label1");

        hboxLayout->addWidget(label1);

        sliderXScale = new QSlider(groupViewControls);
        sliderXScale->setObjectName("sliderXScale");
        sliderXScale->setOrientation(Qt::Horizontal);
        sliderXScale->setMaximum(100);
        sliderXScale->setValue(50);

        hboxLayout->addWidget(sliderXScale);

        label2 = new QLabel(groupViewControls);
        label2->setObjectName("label2");

        hboxLayout->addWidget(label2);

        sliderYScale = new QSlider(groupViewControls);
        sliderYScale->setObjectName("sliderYScale");
        sliderYScale->setOrientation(Qt::Horizontal);
        sliderYScale->setMaximum(100);
        sliderYScale->setValue(16);

        hboxLayout->addWidget(sliderYScale);

        radioTrackView = new QRadioButton(groupViewControls);
        radioTrackView->setObjectName("radioTrackView");

        hboxLayout->addWidget(radioTrackView);

        radioDiskView = new QRadioButton(groupViewControls);
        radioDiskView->setObjectName("radioDiskView");
        radioDiskView->setChecked(true);

        hboxLayout->addWidget(radioDiskView);


        diskLayout->addWidget(groupViewControls);


        mainContent->addLayout(diskLayout);

        rightPanel = new QVBoxLayout();
        rightPanel->setSpacing(6);
        rightPanel->setObjectName("rightPanel");
        groupStatus = new QGroupBox(DiskAnalyzerWindow);
        groupStatus->setObjectName("groupStatus");
        groupStatus->setMinimumWidth(280);
        vboxLayout2 = new QVBoxLayout(groupStatus);
        vboxLayout2->setObjectName("vboxLayout2");
        labelTrackInfo = new QLabel(groupStatus);
        labelTrackInfo->setObjectName("labelTrackInfo");

        vboxLayout2->addWidget(labelTrackInfo);

        textSectorInfo = new QTextEdit(groupStatus);
        textSectorInfo->setObjectName("textSectorInfo");
        textSectorInfo->setReadOnly(true);
        textSectorInfo->setMaximumHeight(120);

        vboxLayout2->addWidget(textSectorInfo);


        rightPanel->addWidget(groupStatus);

        groupHexDump = new QGroupBox(DiskAnalyzerWindow);
        groupHexDump->setObjectName("groupHexDump");
        vboxLayout3 = new QVBoxLayout(groupHexDump);
        vboxLayout3->setObjectName("vboxLayout3");
        textHexDump = new QTextEdit(groupHexDump);
        textHexDump->setObjectName("textHexDump");
        textHexDump->setReadOnly(true);

        vboxLayout3->addWidget(textHexDump);


        rightPanel->addWidget(groupHexDump);

        groupTrackFormat = new QGroupBox(DiskAnalyzerWindow);
        groupTrackFormat->setObjectName("groupTrackFormat");
        gridLayout = new QGridLayout(groupTrackFormat);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setVerticalSpacing(2);
        checkIsoMFM = new QCheckBox(groupTrackFormat);
        checkIsoMFM->setObjectName("checkIsoMFM");
        checkIsoMFM->setChecked(true);

        gridLayout->addWidget(checkIsoMFM, 0, 0, 1, 1);

        checkEEmu = new QCheckBox(groupTrackFormat);
        checkEEmu->setObjectName("checkEEmu");

        gridLayout->addWidget(checkEEmu, 0, 1, 1, 1);

        checkAED6200P = new QCheckBox(groupTrackFormat);
        checkAED6200P->setObjectName("checkAED6200P");

        gridLayout->addWidget(checkAED6200P, 0, 2, 1, 1);

        checkIsoFM = new QCheckBox(groupTrackFormat);
        checkIsoFM->setObjectName("checkIsoFM");

        gridLayout->addWidget(checkIsoFM, 1, 0, 1, 1);

        checkTYCOM = new QCheckBox(groupTrackFormat);
        checkTYCOM->setObjectName("checkTYCOM");

        gridLayout->addWidget(checkTYCOM, 1, 1, 1, 1);

        checkMEMBRAIN = new QCheckBox(groupTrackFormat);
        checkMEMBRAIN->setObjectName("checkMEMBRAIN");

        gridLayout->addWidget(checkMEMBRAIN, 1, 2, 1, 1);

        checkAmigaMFM = new QCheckBox(groupTrackFormat);
        checkAmigaMFM->setObjectName("checkAmigaMFM");
        checkAmigaMFM->setChecked(true);

        gridLayout->addWidget(checkAmigaMFM, 2, 0, 1, 1);

        checkAppleII = new QCheckBox(groupTrackFormat);
        checkAppleII->setObjectName("checkAppleII");

        gridLayout->addWidget(checkAppleII, 2, 1, 1, 1);

        checkArburg = new QCheckBox(groupTrackFormat);
        checkArburg->setObjectName("checkArburg");

        gridLayout->addWidget(checkArburg, 2, 2, 1, 1);

        checkC64GCR = new QCheckBox(groupTrackFormat);
        checkC64GCR->setObjectName("checkC64GCR");
        checkC64GCR->setChecked(true);

        gridLayout->addWidget(checkC64GCR, 3, 0, 1, 1);

        checkAtariFM = new QCheckBox(groupTrackFormat);
        checkAtariFM->setObjectName("checkAtariFM");

        gridLayout->addWidget(checkAtariFM, 3, 1, 1, 1);

        checkUnknown = new QCheckBox(groupTrackFormat);
        checkUnknown->setObjectName("checkUnknown");

        gridLayout->addWidget(checkUnknown, 3, 2, 1, 1);


        rightPanel->addWidget(groupTrackFormat);

        groupSelection = new QGroupBox(DiskAnalyzerWindow);
        groupSelection->setObjectName("groupSelection");
        gridLayout1 = new QGridLayout(groupSelection);
        gridLayout1->setObjectName("gridLayout1");
        label3 = new QLabel(groupSelection);
        label3->setObjectName("label3");

        gridLayout1->addWidget(label3, 0, 0, 1, 1);

        spinTrackNumber = new QSpinBox(groupSelection);
        spinTrackNumber->setObjectName("spinTrackNumber");
        spinTrackNumber->setMinimum(0);
        spinTrackNumber->setMaximum(85);
        spinTrackNumber->setValue(0);

        gridLayout1->addWidget(spinTrackNumber, 0, 1, 1, 1);

        sliderTrack = new QSlider(groupSelection);
        sliderTrack->setObjectName("sliderTrack");
        sliderTrack->setOrientation(Qt::Horizontal);
        sliderTrack->setMaximum(85);

        gridLayout1->addWidget(sliderTrack, 0, 2, 1, 1);

        label4 = new QLabel(groupSelection);
        label4->setObjectName("label4");

        gridLayout1->addWidget(label4, 1, 0, 1, 1);

        spinSideNumber = new QSpinBox(groupSelection);
        spinSideNumber->setObjectName("spinSideNumber");
        spinSideNumber->setMinimum(0);
        spinSideNumber->setMaximum(1);
        spinSideNumber->setValue(0);

        gridLayout1->addWidget(spinSideNumber, 1, 1, 1, 1);

        sliderSide = new QSlider(groupSelection);
        sliderSide->setObjectName("sliderSide");
        sliderSide->setOrientation(Qt::Horizontal);
        sliderSide->setMaximum(1);

        gridLayout1->addWidget(sliderSide, 1, 2, 1, 1);


        rightPanel->addWidget(groupSelection);

        hboxLayout1 = new QHBoxLayout();
        hboxLayout1->setObjectName("hboxLayout1");
        btnEditTools = new QPushButton(DiskAnalyzerWindow);
        btnEditTools->setObjectName("btnEditTools");

        hboxLayout1->addWidget(btnEditTools);

        btnExport = new QPushButton(DiskAnalyzerWindow);
        btnExport->setObjectName("btnExport");

        hboxLayout1->addWidget(btnExport);

        btnClose = new QPushButton(DiskAnalyzerWindow);
        btnClose->setObjectName("btnClose");

        hboxLayout1->addWidget(btnClose);


        rightPanel->addLayout(hboxLayout1);


        mainContent->addLayout(rightPanel);


        verticalLayout_main->addLayout(mainContent);

        frameStatusBar = new QFrame(DiskAnalyzerWindow);
        frameStatusBar->setObjectName("frameStatusBar");
        frameStatusBar->setMaximumHeight(25);
        frameStatusBar->setFrameShape(QFrame::StyledPanel);
        hboxLayout2 = new QHBoxLayout(frameStatusBar);
        hboxLayout2->setObjectName("hboxLayout2");
        hboxLayout2->setContentsMargins(4, 2, 4, 2);
        labelLibVersion = new QLabel(frameStatusBar);
        labelLibVersion->setObjectName("labelLibVersion");

        hboxLayout2->addWidget(labelLibVersion);

        spacerItem = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout2->addItem(spacerItem);

        labelCRC = new QLabel(frameStatusBar);
        labelCRC->setObjectName("labelCRC");

        hboxLayout2->addWidget(labelCRC);


        verticalLayout_main->addWidget(frameStatusBar);


        retranslateUi(DiskAnalyzerWindow);

        QMetaObject::connectSlotsByName(DiskAnalyzerWindow);
    } // setupUi

    void retranslateUi(QDialog *DiskAnalyzerWindow)
    {
        DiskAnalyzerWindow->setWindowTitle(QCoreApplication::translate("DiskAnalyzerWindow", "Disk Analyzer", nullptr));
        groupSide0->setTitle(QCoreApplication::translate("DiskAnalyzerWindow", "Side 0", nullptr));
        groupSide0->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "QGroupBox { font-weight: bold; }", nullptr));
        labelSide0Info->setText(QCoreApplication::translate("DiskAnalyzerWindow", "0 Tracks, 0 Sectors, 0 Bytes", nullptr));
        labelSide0Info->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #00AA00; font-family: monospace;", nullptr));
        labelSide0Format->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Format: Unknown", nullptr));
        labelSide0Format->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "font-family: monospace;", nullptr));
        frameDisk0->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "background-color: #1a1a1a;", nullptr));
        groupSide1->setTitle(QCoreApplication::translate("DiskAnalyzerWindow", "Side 1", nullptr));
        groupSide1->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "QGroupBox { font-weight: bold; }", nullptr));
        labelSide1Info->setText(QCoreApplication::translate("DiskAnalyzerWindow", "0 Tracks, 0 Sectors, 0 Bytes", nullptr));
        labelSide1Info->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #00AA00; font-family: monospace;", nullptr));
        labelSide1Format->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Format: Unknown", nullptr));
        labelSide1Format->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "font-family: monospace;", nullptr));
        frameDisk1->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "background-color: #1a1a1a;", nullptr));
        groupViewControls->setTitle(QCoreApplication::translate("DiskAnalyzerWindow", "View", nullptr));
        label->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Offset:", nullptr));
        spinOffset->setSuffix(QCoreApplication::translate("DiskAnalyzerWindow", " %", nullptr));
        label1->setText(QCoreApplication::translate("DiskAnalyzerWindow", "X Scale:", nullptr));
        label2->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Y Scale:", nullptr));
        radioTrackView->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Track view", nullptr));
        radioDiskView->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Disk view", nullptr));
        groupStatus->setTitle(QCoreApplication::translate("DiskAnalyzerWindow", "Status", nullptr));
        groupStatus->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "QGroupBox { font-weight: bold; }", nullptr));
        labelTrackInfo->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Track: - Side: -", nullptr));
        labelTrackInfo->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "font-family: monospace; font-weight: bold;", nullptr));
        textSectorInfo->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "font-family: monospace; font-size: 9pt;", nullptr));
        textSectorInfo->setPlainText(QCoreApplication::translate("DiskAnalyzerWindow", "MFM Sector\n"
"Sector ID: -\n"
"Track ID: - Side ID: -\n"
"Size: - (ID: -)\n"
"Data checksum: -\n"
"Head CRC: -\n"
"Data CRC: -", nullptr));
        groupHexDump->setTitle(QCoreApplication::translate("DiskAnalyzerWindow", "Hex Dump", nullptr));
        groupHexDump->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "QGroupBox { font-weight: bold; }", nullptr));
        textHexDump->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "font-family: monospace; font-size: 8pt; background-color: #1a1a1a; color: #00ff00;", nullptr));
        textHexDump->setPlainText(QCoreApplication::translate("DiskAnalyzerWindow", "00000  00 00 00 00 00 00 00 00  ........\n"
"00008  00 00 00 00 00 00 00 00  ........\n"
"00010  00 00 00 00 00 00 00 00  ........", nullptr));
        groupTrackFormat->setTitle(QCoreApplication::translate("DiskAnalyzerWindow", "Track analysis format", nullptr));
        groupTrackFormat->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "QGroupBox { font-weight: bold; }", nullptr));
        checkIsoMFM->setText(QCoreApplication::translate("DiskAnalyzerWindow", "ISO MFM", nullptr));
        checkIsoMFM->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #00ff00;", nullptr));
        checkEEmu->setText(QCoreApplication::translate("DiskAnalyzerWindow", "E-Emu", nullptr));
        checkEEmu->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #ffff00;", nullptr));
        checkAED6200P->setText(QCoreApplication::translate("DiskAnalyzerWindow", "AED 6200P", nullptr));
        checkAED6200P->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #ff00ff;", nullptr));
        checkIsoFM->setText(QCoreApplication::translate("DiskAnalyzerWindow", "ISO FM", nullptr));
        checkIsoFM->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #00ffff;", nullptr));
        checkTYCOM->setText(QCoreApplication::translate("DiskAnalyzerWindow", "TYCOM", nullptr));
        checkTYCOM->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #ff8800;", nullptr));
        checkMEMBRAIN->setText(QCoreApplication::translate("DiskAnalyzerWindow", "MEMBRAIN", nullptr));
        checkMEMBRAIN->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #8888ff;", nullptr));
        checkAmigaMFM->setText(QCoreApplication::translate("DiskAnalyzerWindow", "AMIGA MFM", nullptr));
        checkAmigaMFM->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #ff0000;", nullptr));
        checkAppleII->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Apple II", nullptr));
        checkAppleII->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #888888;", nullptr));
        checkArburg->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Arburg", nullptr));
        checkArburg->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #88ff88;", nullptr));
        checkC64GCR->setText(QCoreApplication::translate("DiskAnalyzerWindow", "C64 GCR", nullptr));
        checkC64GCR->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #4488ff;", nullptr));
        checkAtariFM->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Atari FM", nullptr));
        checkAtariFM->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #ff4488;", nullptr));
        checkUnknown->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Unknown", nullptr));
        checkUnknown->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "color: #666666;", nullptr));
        groupSelection->setTitle(QCoreApplication::translate("DiskAnalyzerWindow", "Track / Side selection", nullptr));
        groupSelection->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "QGroupBox { font-weight: bold; }", nullptr));
        label3->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Track:", nullptr));
        label4->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Side:", nullptr));
        btnEditTools->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Edit tools", nullptr));
        btnExport->setText(QCoreApplication::translate("DiskAnalyzerWindow", "Export...", nullptr));
        btnClose->setText(QCoreApplication::translate("DiskAnalyzerWindow", "OK", nullptr));
        labelLibVersion->setText(QCoreApplication::translate("DiskAnalyzerWindow", "libhxcfe v2.8.14.3", nullptr));
        labelLibVersion->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "font-family: monospace;", nullptr));
        labelCRC->setText(QCoreApplication::translate("DiskAnalyzerWindow", "CRC32: 0x00000000", nullptr));
        labelCRC->setStyleSheet(QCoreApplication::translate("DiskAnalyzerWindow", "font-family: monospace;", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DiskAnalyzerWindow: public Ui_DiskAnalyzerWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DISKANALYZER_WINDOW_H

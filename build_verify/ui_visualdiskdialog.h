/********************************************************************************
** Form generated from reading UI file 'visualdiskdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VISUALDISKDIALOG_H
#define UI_VISUALDISKDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
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

class Ui_VisualDiskDialog
{
public:
    QVBoxLayout *mainLayout;
    QHBoxLayout *hboxLayout;
    QLabel *labelSide0Info;
    QLabel *labelSide1Info;
    QHBoxLayout *hboxLayout1;
    QFrame *frameDiskView0;
    QFrame *frameDiskView1;
    QVBoxLayout *vboxLayout;
    QGroupBox *groupStatus;
    QVBoxLayout *vboxLayout1;
    QLabel *labelTrackSide;
    QTextEdit *textSectorInfo;
    QTextEdit *textHexDump;
    QGroupBox *groupFormats;
    QGridLayout *gridLayout;
    QCheckBox *checkIsoMfm;
    QCheckBox *checkEEmu;
    QCheckBox *checkAed6200p;
    QCheckBox *checkIsoFm;
    QCheckBox *checkTycom;
    QCheckBox *checkMembrain;
    QCheckBox *checkAmigaMfm;
    QCheckBox *checkAppleII;
    QCheckBox *checkArburg;
    QGroupBox *groupSelection;
    QFormLayout *formLayout;
    QLabel *label;
    QHBoxLayout *hboxLayout2;
    QSpinBox *spinTrack;
    QSlider *sliderTrack;
    QLabel *label1;
    QHBoxLayout *hboxLayout3;
    QSpinBox *spinSide;
    QSlider *sliderSide;
    QPushButton *btnEditTools;
    QHBoxLayout *hboxLayout4;
    QLabel *label2;
    QDoubleSpinBox *spinXOffset;
    QLabel *label3;
    QSlider *sliderXScale;
    QLabel *label4;
    QSpacerItem *spacerItem;
    QSlider *sliderYScale;
    QLabel *label5;
    QHBoxLayout *hboxLayout5;
    QSpacerItem *spacerItem1;
    QRadioButton *radioTrackView;
    QRadioButton *radioDiskView;
    QPushButton *btnOK;

    void setupUi(QDialog *VisualDiskDialog)
    {
        if (VisualDiskDialog->objectName().isEmpty())
            VisualDiskDialog->setObjectName("VisualDiskDialog");
        VisualDiskDialog->resize(1100, 700);
        mainLayout = new QVBoxLayout(VisualDiskDialog);
        mainLayout->setSpacing(4);
        mainLayout->setContentsMargins(4, 4, 4, 4);
        mainLayout->setObjectName("mainLayout");
        hboxLayout = new QHBoxLayout();
        hboxLayout->setObjectName("hboxLayout");
        labelSide0Info = new QLabel(VisualDiskDialog);
        labelSide0Info->setObjectName("labelSide0Info");

        hboxLayout->addWidget(labelSide0Info);

        labelSide1Info = new QLabel(VisualDiskDialog);
        labelSide1Info->setObjectName("labelSide1Info");

        hboxLayout->addWidget(labelSide1Info);


        mainLayout->addLayout(hboxLayout);

        hboxLayout1 = new QHBoxLayout();
        hboxLayout1->setObjectName("hboxLayout1");
        frameDiskView0 = new QFrame(VisualDiskDialog);
        frameDiskView0->setObjectName("frameDiskView0");
        frameDiskView0->setMinimumSize(QSize(350, 350));
        frameDiskView0->setFrameShape(QFrame::Box);

        hboxLayout1->addWidget(frameDiskView0);

        frameDiskView1 = new QFrame(VisualDiskDialog);
        frameDiskView1->setObjectName("frameDiskView1");
        frameDiskView1->setMinimumSize(QSize(350, 350));
        frameDiskView1->setFrameShape(QFrame::Box);

        hboxLayout1->addWidget(frameDiskView1);

        vboxLayout = new QVBoxLayout();
        vboxLayout->setObjectName("vboxLayout");
        groupStatus = new QGroupBox(VisualDiskDialog);
        groupStatus->setObjectName("groupStatus");
        vboxLayout1 = new QVBoxLayout(groupStatus);
        vboxLayout1->setObjectName("vboxLayout1");
        labelTrackSide = new QLabel(groupStatus);
        labelTrackSide->setObjectName("labelTrackSide");

        vboxLayout1->addWidget(labelTrackSide);

        textSectorInfo = new QTextEdit(groupStatus);
        textSectorInfo->setObjectName("textSectorInfo");
        textSectorInfo->setReadOnly(true);
        textSectorInfo->setMaximumHeight(180);

        vboxLayout1->addWidget(textSectorInfo);

        textHexDump = new QTextEdit(groupStatus);
        textHexDump->setObjectName("textHexDump");
        textHexDump->setReadOnly(true);
        textHexDump->setMaximumHeight(120);

        vboxLayout1->addWidget(textHexDump);


        vboxLayout->addWidget(groupStatus);

        groupFormats = new QGroupBox(VisualDiskDialog);
        groupFormats->setObjectName("groupFormats");
        gridLayout = new QGridLayout(groupFormats);
        gridLayout->setObjectName("gridLayout");
        checkIsoMfm = new QCheckBox(groupFormats);
        checkIsoMfm->setObjectName("checkIsoMfm");
        checkIsoMfm->setChecked(true);

        gridLayout->addWidget(checkIsoMfm, 0, 0, 1, 1);

        checkEEmu = new QCheckBox(groupFormats);
        checkEEmu->setObjectName("checkEEmu");

        gridLayout->addWidget(checkEEmu, 0, 1, 1, 1);

        checkAed6200p = new QCheckBox(groupFormats);
        checkAed6200p->setObjectName("checkAed6200p");

        gridLayout->addWidget(checkAed6200p, 0, 2, 1, 1);

        checkIsoFm = new QCheckBox(groupFormats);
        checkIsoFm->setObjectName("checkIsoFm");

        gridLayout->addWidget(checkIsoFm, 1, 0, 1, 1);

        checkTycom = new QCheckBox(groupFormats);
        checkTycom->setObjectName("checkTycom");

        gridLayout->addWidget(checkTycom, 1, 1, 1, 1);

        checkMembrain = new QCheckBox(groupFormats);
        checkMembrain->setObjectName("checkMembrain");

        gridLayout->addWidget(checkMembrain, 1, 2, 1, 1);

        checkAmigaMfm = new QCheckBox(groupFormats);
        checkAmigaMfm->setObjectName("checkAmigaMfm");

        gridLayout->addWidget(checkAmigaMfm, 2, 0, 1, 1);

        checkAppleII = new QCheckBox(groupFormats);
        checkAppleII->setObjectName("checkAppleII");

        gridLayout->addWidget(checkAppleII, 2, 1, 1, 1);

        checkArburg = new QCheckBox(groupFormats);
        checkArburg->setObjectName("checkArburg");

        gridLayout->addWidget(checkArburg, 2, 2, 1, 1);


        vboxLayout->addWidget(groupFormats);

        groupSelection = new QGroupBox(VisualDiskDialog);
        groupSelection->setObjectName("groupSelection");
        formLayout = new QFormLayout(groupSelection);
        formLayout->setObjectName("formLayout");
        label = new QLabel(groupSelection);
        label->setObjectName("label");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, label);

        hboxLayout2 = new QHBoxLayout();
        hboxLayout2->setObjectName("hboxLayout2");
        spinTrack = new QSpinBox(groupSelection);
        spinTrack->setObjectName("spinTrack");
        spinTrack->setMinimum(0);
        spinTrack->setMaximum(255);

        hboxLayout2->addWidget(spinTrack);

        sliderTrack = new QSlider(groupSelection);
        sliderTrack->setObjectName("sliderTrack");
        sliderTrack->setOrientation(Qt::Horizontal);
        sliderTrack->setMinimum(0);
        sliderTrack->setMaximum(79);

        hboxLayout2->addWidget(sliderTrack);


        formLayout->setLayout(0, QFormLayout::ItemRole::FieldRole, hboxLayout2);

        label1 = new QLabel(groupSelection);
        label1->setObjectName("label1");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, label1);

        hboxLayout3 = new QHBoxLayout();
        hboxLayout3->setObjectName("hboxLayout3");
        spinSide = new QSpinBox(groupSelection);
        spinSide->setObjectName("spinSide");
        spinSide->setMinimum(0);
        spinSide->setMaximum(1);

        hboxLayout3->addWidget(spinSide);

        sliderSide = new QSlider(groupSelection);
        sliderSide->setObjectName("sliderSide");
        sliderSide->setOrientation(Qt::Horizontal);
        sliderSide->setMinimum(0);
        sliderSide->setMaximum(1);

        hboxLayout3->addWidget(sliderSide);


        formLayout->setLayout(1, QFormLayout::ItemRole::FieldRole, hboxLayout3);


        vboxLayout->addWidget(groupSelection);

        btnEditTools = new QPushButton(VisualDiskDialog);
        btnEditTools->setObjectName("btnEditTools");

        vboxLayout->addWidget(btnEditTools);


        hboxLayout1->addLayout(vboxLayout);


        mainLayout->addLayout(hboxLayout1);

        hboxLayout4 = new QHBoxLayout();
        hboxLayout4->setObjectName("hboxLayout4");
        label2 = new QLabel(VisualDiskDialog);
        label2->setObjectName("label2");

        hboxLayout4->addWidget(label2);

        spinXOffset = new QDoubleSpinBox(VisualDiskDialog);
        spinXOffset->setObjectName("spinXOffset");
        spinXOffset->setMinimum(0);
        spinXOffset->setMaximum(100);
        spinXOffset->setValue(85);

        hboxLayout4->addWidget(spinXOffset);

        label3 = new QLabel(VisualDiskDialog);
        label3->setObjectName("label3");

        hboxLayout4->addWidget(label3);

        sliderXScale = new QSlider(VisualDiskDialog);
        sliderXScale->setObjectName("sliderXScale");
        sliderXScale->setOrientation(Qt::Horizontal);
        sliderXScale->setMinimum(1);
        sliderXScale->setMaximum(100);
        sliderXScale->setValue(50);

        hboxLayout4->addWidget(sliderXScale);

        label4 = new QLabel(VisualDiskDialog);
        label4->setObjectName("label4");

        hboxLayout4->addWidget(label4);

        spacerItem = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout4->addItem(spacerItem);

        sliderYScale = new QSlider(VisualDiskDialog);
        sliderYScale->setObjectName("sliderYScale");
        sliderYScale->setOrientation(Qt::Horizontal);
        sliderYScale->setMinimum(1);
        sliderYScale->setMaximum(100);
        sliderYScale->setValue(16);
        sliderYScale->setMaximumWidth(100);

        hboxLayout4->addWidget(sliderYScale);

        label5 = new QLabel(VisualDiskDialog);
        label5->setObjectName("label5");

        hboxLayout4->addWidget(label5);


        mainLayout->addLayout(hboxLayout4);

        hboxLayout5 = new QHBoxLayout();
        hboxLayout5->setObjectName("hboxLayout5");
        spacerItem1 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout5->addItem(spacerItem1);

        radioTrackView = new QRadioButton(VisualDiskDialog);
        radioTrackView->setObjectName("radioTrackView");

        hboxLayout5->addWidget(radioTrackView);

        radioDiskView = new QRadioButton(VisualDiskDialog);
        radioDiskView->setObjectName("radioDiskView");
        radioDiskView->setChecked(true);

        hboxLayout5->addWidget(radioDiskView);

        btnOK = new QPushButton(VisualDiskDialog);
        btnOK->setObjectName("btnOK");

        hboxLayout5->addWidget(btnOK);


        mainLayout->addLayout(hboxLayout5);


        retranslateUi(VisualDiskDialog);

        btnOK->setDefault(true);


        QMetaObject::connectSlotsByName(VisualDiskDialog);
    } // setupUi

    void retranslateUi(QDialog *VisualDiskDialog)
    {
        VisualDiskDialog->setWindowTitle(QCoreApplication::translate("VisualDiskDialog", "Visual Floppy Disk", nullptr));
        labelSide0Info->setText(QCoreApplication::translate("VisualDiskDialog", "Side 0, 80 Tracks | 1440 Sectors, 0 bad | 1474560 Bytes | ISO MFM", nullptr));
        labelSide0Info->setStyleSheet(QCoreApplication::translate("VisualDiskDialog", "background: #333; color: #0f0; padding: 4px;", nullptr));
        labelSide1Info->setText(QCoreApplication::translate("VisualDiskDialog", "Side 1, 80 Tracks | 1440 Sectors, 0 bad | 1474560 Bytes | ISO MFM", nullptr));
        labelSide1Info->setStyleSheet(QCoreApplication::translate("VisualDiskDialog", "background: #333; color: #0f0; padding: 4px;", nullptr));
        frameDiskView0->setStyleSheet(QCoreApplication::translate("VisualDiskDialog", "background: black;", nullptr));
        frameDiskView1->setStyleSheet(QCoreApplication::translate("VisualDiskDialog", "background: black;", nullptr));
        groupStatus->setTitle(QCoreApplication::translate("VisualDiskDialog", "Status", nullptr));
        labelTrackSide->setText(QCoreApplication::translate("VisualDiskDialog", "Track: 0 Side: 0", nullptr));
        textSectorInfo->setStyleSheet(QCoreApplication::translate("VisualDiskDialog", "font-family: monospace; font-size: 9pt;", nullptr));
        textHexDump->setStyleSheet(QCoreApplication::translate("VisualDiskDialog", "font-family: monospace; font-size: 8pt;", nullptr));
        groupFormats->setTitle(QCoreApplication::translate("VisualDiskDialog", "Track analysis format", nullptr));
        checkIsoMfm->setText(QCoreApplication::translate("VisualDiskDialog", "ISO MFM", nullptr));
        checkEEmu->setText(QCoreApplication::translate("VisualDiskDialog", "E-Emu", nullptr));
        checkAed6200p->setText(QCoreApplication::translate("VisualDiskDialog", "AED 6200P", nullptr));
        checkIsoFm->setText(QCoreApplication::translate("VisualDiskDialog", "ISO FM", nullptr));
        checkTycom->setText(QCoreApplication::translate("VisualDiskDialog", "TYCOM", nullptr));
        checkMembrain->setText(QCoreApplication::translate("VisualDiskDialog", "MEMBRAIN", nullptr));
        checkAmigaMfm->setText(QCoreApplication::translate("VisualDiskDialog", "AMIGA MFM", nullptr));
        checkAppleII->setText(QCoreApplication::translate("VisualDiskDialog", "Apple II", nullptr));
        checkArburg->setText(QCoreApplication::translate("VisualDiskDialog", "Arburg", nullptr));
        groupSelection->setTitle(QCoreApplication::translate("VisualDiskDialog", "Track / Side selection", nullptr));
        label->setText(QCoreApplication::translate("VisualDiskDialog", "Track number", nullptr));
        label1->setText(QCoreApplication::translate("VisualDiskDialog", "Side number", nullptr));
        btnEditTools->setText(QCoreApplication::translate("VisualDiskDialog", "Edit tools", nullptr));
        label2->setText(QCoreApplication::translate("VisualDiskDialog", "View", nullptr));
        label3->setText(QCoreApplication::translate("VisualDiskDialog", "x offset (% of the track len)", nullptr));
        label4->setText(QCoreApplication::translate("VisualDiskDialog", "full x time scale", nullptr));
        label5->setText(QCoreApplication::translate("VisualDiskDialog", "full y time scale (us)", nullptr));
        radioTrackView->setText(QCoreApplication::translate("VisualDiskDialog", "Track view mode", nullptr));
        radioDiskView->setText(QCoreApplication::translate("VisualDiskDialog", "Disk view mode", nullptr));
        btnOK->setText(QCoreApplication::translate("VisualDiskDialog", "OK", nullptr));
    } // retranslateUi

};

namespace Ui {
    class VisualDiskDialog: public Ui_VisualDiskDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VISUALDISKDIALOG_H

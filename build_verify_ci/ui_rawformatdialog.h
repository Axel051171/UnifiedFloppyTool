/********************************************************************************
** Form generated from reading UI file 'rawformatdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RAWFORMATDIALOG_H
#define UI_RAWFORMATDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_RawFormatDialog
{
public:
    QVBoxLayout *mainLayout;
    QHBoxLayout *hboxLayout;
    QFormLayout *formLayout;
    QLabel *label;
    QComboBox *comboTrackType;
    QFormLayout *formLayout1;
    QLabel *label1;
    QSpinBox *spinTracks;
    QFormLayout *formLayout2;
    QLabel *label2;
    QComboBox *comboSides;
    QCheckBox *checkSidesGrouped;
    QHBoxLayout *hboxLayout1;
    QFormLayout *formLayout3;
    QLabel *label3;
    QSpinBox *spinBitrate;
    QFormLayout *formLayout4;
    QLabel *label4;
    QSpinBox *spinSectors;
    QFormLayout *formLayout5;
    QLabel *label5;
    QComboBox *comboSectorSize;
    QCheckBox *checkReverseSide;
    QHBoxLayout *hboxLayout2;
    QFormLayout *formLayout6;
    QLabel *label6;
    QComboBox *comboRPM;
    QFormLayout *formLayout7;
    QLabel *label7;
    QSpinBox *spinSectorIdStart;
    QCheckBox *checkInterSideNumbering;
    QSpacerItem *spacerItem;
    QHBoxLayout *hboxLayout3;
    QFormLayout *formLayout8;
    QLabel *label8;
    QSpinBox *spinInterleave;
    QFormLayout *formLayout9;
    QLabel *label9;
    QSpinBox *spinSkew;
    QCheckBox *checkSideBased;
    QSpacerItem *spacerItem1;
    QHBoxLayout *hboxLayout4;
    QFormLayout *formLayout10;
    QLabel *label10;
    QLineEdit *editTotalSectors;
    QFormLayout *formLayout11;
    QLabel *label11;
    QLineEdit *editTotalSize;
    QFormLayout *formLayout12;
    QLabel *label12;
    QLineEdit *editFormatValue;
    QSpacerItem *spacerItem2;
    QHBoxLayout *hboxLayout5;
    QFormLayout *formLayout13;
    QLabel *label13;
    QSpinBox *spinGap3;
    QCheckBox *checkAutoGap3;
    QFormLayout *formLayout14;
    QLabel *label14;
    QSpinBox *spinPreGap;
    QSpacerItem *spacerItem3;
    QHBoxLayout *hboxLayout6;
    QLabel *label15;
    QComboBox *comboLayout;
    QSpacerItem *spacerItem4;
    QHBoxLayout *hboxLayout7;
    QPushButton *btnSaveConfig;
    QPushButton *btnLoadConfig;
    QSpacerItem *spacerItem5;
    QPushButton *btnLoadRaw;
    QPushButton *btnCreateEmpty;
    QPushButton *btnClose;
    QLabel *labelInfo;

    void setupUi(QDialog *RawFormatDialog)
    {
        if (RawFormatDialog->objectName().isEmpty())
            RawFormatDialog->setObjectName("RawFormatDialog");
        RawFormatDialog->resize(500, 420);
        mainLayout = new QVBoxLayout(RawFormatDialog);
        mainLayout->setSpacing(8);
        mainLayout->setContentsMargins(8, 8, 8, 8);
        mainLayout->setObjectName("mainLayout");
        hboxLayout = new QHBoxLayout();
        hboxLayout->setObjectName("hboxLayout");
        formLayout = new QFormLayout();
        formLayout->setObjectName("formLayout");
        label = new QLabel(RawFormatDialog);
        label->setObjectName("label");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, label);

        comboTrackType = new QComboBox(RawFormatDialog);
        comboTrackType->setObjectName("comboTrackType");
        comboTrackType->setMinimumWidth(120);

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, comboTrackType);


        hboxLayout->addLayout(formLayout);

        formLayout1 = new QFormLayout();
        formLayout1->setObjectName("formLayout1");
        label1 = new QLabel(RawFormatDialog);
        label1->setObjectName("label1");

        formLayout1->setWidget(0, QFormLayout::ItemRole::LabelRole, label1);

        spinTracks = new QSpinBox(RawFormatDialog);
        spinTracks->setObjectName("spinTracks");
        spinTracks->setMinimum(1);
        spinTracks->setMaximum(255);
        spinTracks->setValue(80);

        formLayout1->setWidget(0, QFormLayout::ItemRole::FieldRole, spinTracks);


        hboxLayout->addLayout(formLayout1);

        formLayout2 = new QFormLayout();
        formLayout2->setObjectName("formLayout2");
        label2 = new QLabel(RawFormatDialog);
        label2->setObjectName("label2");

        formLayout2->setWidget(0, QFormLayout::ItemRole::LabelRole, label2);

        comboSides = new QComboBox(RawFormatDialog);
        comboSides->addItem(QString());
        comboSides->addItem(QString());
        comboSides->setObjectName("comboSides");

        formLayout2->setWidget(0, QFormLayout::ItemRole::FieldRole, comboSides);


        hboxLayout->addLayout(formLayout2);

        checkSidesGrouped = new QCheckBox(RawFormatDialog);
        checkSidesGrouped->setObjectName("checkSidesGrouped");

        hboxLayout->addWidget(checkSidesGrouped);


        mainLayout->addLayout(hboxLayout);

        hboxLayout1 = new QHBoxLayout();
        hboxLayout1->setObjectName("hboxLayout1");
        formLayout3 = new QFormLayout();
        formLayout3->setObjectName("formLayout3");
        label3 = new QLabel(RawFormatDialog);
        label3->setObjectName("label3");

        formLayout3->setWidget(0, QFormLayout::ItemRole::LabelRole, label3);

        spinBitrate = new QSpinBox(RawFormatDialog);
        spinBitrate->setObjectName("spinBitrate");
        spinBitrate->setMinimum(100000);
        spinBitrate->setMaximum(1000000);
        spinBitrate->setValue(250000);
        spinBitrate->setSingleStep(1000);

        formLayout3->setWidget(0, QFormLayout::ItemRole::FieldRole, spinBitrate);


        hboxLayout1->addLayout(formLayout3);

        formLayout4 = new QFormLayout();
        formLayout4->setObjectName("formLayout4");
        label4 = new QLabel(RawFormatDialog);
        label4->setObjectName("label4");

        formLayout4->setWidget(0, QFormLayout::ItemRole::LabelRole, label4);

        spinSectors = new QSpinBox(RawFormatDialog);
        spinSectors->setObjectName("spinSectors");
        spinSectors->setMinimum(1);
        spinSectors->setMaximum(255);
        spinSectors->setValue(10);

        formLayout4->setWidget(0, QFormLayout::ItemRole::FieldRole, spinSectors);


        hboxLayout1->addLayout(formLayout4);

        formLayout5 = new QFormLayout();
        formLayout5->setObjectName("formLayout5");
        label5 = new QLabel(RawFormatDialog);
        label5->setObjectName("label5");

        formLayout5->setWidget(0, QFormLayout::ItemRole::LabelRole, label5);

        comboSectorSize = new QComboBox(RawFormatDialog);
        comboSectorSize->addItem(QString());
        comboSectorSize->addItem(QString());
        comboSectorSize->addItem(QString());
        comboSectorSize->addItem(QString());
        comboSectorSize->addItem(QString());
        comboSectorSize->addItem(QString());
        comboSectorSize->addItem(QString());
        comboSectorSize->setObjectName("comboSectorSize");

        formLayout5->setWidget(0, QFormLayout::ItemRole::FieldRole, comboSectorSize);


        hboxLayout1->addLayout(formLayout5);

        checkReverseSide = new QCheckBox(RawFormatDialog);
        checkReverseSide->setObjectName("checkReverseSide");

        hboxLayout1->addWidget(checkReverseSide);


        mainLayout->addLayout(hboxLayout1);

        hboxLayout2 = new QHBoxLayout();
        hboxLayout2->setObjectName("hboxLayout2");
        formLayout6 = new QFormLayout();
        formLayout6->setObjectName("formLayout6");
        label6 = new QLabel(RawFormatDialog);
        label6->setObjectName("label6");

        formLayout6->setWidget(0, QFormLayout::ItemRole::LabelRole, label6);

        comboRPM = new QComboBox(RawFormatDialog);
        comboRPM->addItem(QString());
        comboRPM->addItem(QString());
        comboRPM->setObjectName("comboRPM");

        formLayout6->setWidget(0, QFormLayout::ItemRole::FieldRole, comboRPM);


        hboxLayout2->addLayout(formLayout6);

        formLayout7 = new QFormLayout();
        formLayout7->setObjectName("formLayout7");
        label7 = new QLabel(RawFormatDialog);
        label7->setObjectName("label7");

        formLayout7->setWidget(0, QFormLayout::ItemRole::LabelRole, label7);

        spinSectorIdStart = new QSpinBox(RawFormatDialog);
        spinSectorIdStart->setObjectName("spinSectorIdStart");
        spinSectorIdStart->setMinimum(0);
        spinSectorIdStart->setMaximum(255);
        spinSectorIdStart->setValue(1);

        formLayout7->setWidget(0, QFormLayout::ItemRole::FieldRole, spinSectorIdStart);


        hboxLayout2->addLayout(formLayout7);

        checkInterSideNumbering = new QCheckBox(RawFormatDialog);
        checkInterSideNumbering->setObjectName("checkInterSideNumbering");

        hboxLayout2->addWidget(checkInterSideNumbering);

        spacerItem = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout2->addItem(spacerItem);


        mainLayout->addLayout(hboxLayout2);

        hboxLayout3 = new QHBoxLayout();
        hboxLayout3->setObjectName("hboxLayout3");
        formLayout8 = new QFormLayout();
        formLayout8->setObjectName("formLayout8");
        label8 = new QLabel(RawFormatDialog);
        label8->setObjectName("label8");

        formLayout8->setWidget(0, QFormLayout::ItemRole::LabelRole, label8);

        spinInterleave = new QSpinBox(RawFormatDialog);
        spinInterleave->setObjectName("spinInterleave");
        spinInterleave->setMinimum(1);
        spinInterleave->setMaximum(99);
        spinInterleave->setValue(1);

        formLayout8->setWidget(0, QFormLayout::ItemRole::FieldRole, spinInterleave);


        hboxLayout3->addLayout(formLayout8);

        formLayout9 = new QFormLayout();
        formLayout9->setObjectName("formLayout9");
        label9 = new QLabel(RawFormatDialog);
        label9->setObjectName("label9");

        formLayout9->setWidget(0, QFormLayout::ItemRole::LabelRole, label9);

        spinSkew = new QSpinBox(RawFormatDialog);
        spinSkew->setObjectName("spinSkew");
        spinSkew->setMinimum(0);
        spinSkew->setMaximum(99);
        spinSkew->setValue(0);

        formLayout9->setWidget(0, QFormLayout::ItemRole::FieldRole, spinSkew);


        hboxLayout3->addLayout(formLayout9);

        checkSideBased = new QCheckBox(RawFormatDialog);
        checkSideBased->setObjectName("checkSideBased");

        hboxLayout3->addWidget(checkSideBased);

        spacerItem1 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout3->addItem(spacerItem1);


        mainLayout->addLayout(hboxLayout3);

        hboxLayout4 = new QHBoxLayout();
        hboxLayout4->setObjectName("hboxLayout4");
        formLayout10 = new QFormLayout();
        formLayout10->setObjectName("formLayout10");
        label10 = new QLabel(RawFormatDialog);
        label10->setObjectName("label10");

        formLayout10->setWidget(0, QFormLayout::ItemRole::LabelRole, label10);

        editTotalSectors = new QLineEdit(RawFormatDialog);
        editTotalSectors->setObjectName("editTotalSectors");
        editTotalSectors->setReadOnly(true);
        editTotalSectors->setMaximumWidth(80);

        formLayout10->setWidget(0, QFormLayout::ItemRole::FieldRole, editTotalSectors);


        hboxLayout4->addLayout(formLayout10);

        formLayout11 = new QFormLayout();
        formLayout11->setObjectName("formLayout11");
        label11 = new QLabel(RawFormatDialog);
        label11->setObjectName("label11");

        formLayout11->setWidget(0, QFormLayout::ItemRole::LabelRole, label11);

        editTotalSize = new QLineEdit(RawFormatDialog);
        editTotalSize->setObjectName("editTotalSize");
        editTotalSize->setReadOnly(true);
        editTotalSize->setMaximumWidth(80);

        formLayout11->setWidget(0, QFormLayout::ItemRole::FieldRole, editTotalSize);


        hboxLayout4->addLayout(formLayout11);

        formLayout12 = new QFormLayout();
        formLayout12->setObjectName("formLayout12");
        label12 = new QLabel(RawFormatDialog);
        label12->setObjectName("label12");

        formLayout12->setWidget(0, QFormLayout::ItemRole::LabelRole, label12);

        editFormatValue = new QLineEdit(RawFormatDialog);
        editFormatValue->setObjectName("editFormatValue");
        editFormatValue->setReadOnly(true);
        editFormatValue->setMaximumWidth(80);

        formLayout12->setWidget(0, QFormLayout::ItemRole::FieldRole, editFormatValue);


        hboxLayout4->addLayout(formLayout12);

        spacerItem2 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout4->addItem(spacerItem2);


        mainLayout->addLayout(hboxLayout4);

        hboxLayout5 = new QHBoxLayout();
        hboxLayout5->setObjectName("hboxLayout5");
        formLayout13 = new QFormLayout();
        formLayout13->setObjectName("formLayout13");
        label13 = new QLabel(RawFormatDialog);
        label13->setObjectName("label13");

        formLayout13->setWidget(0, QFormLayout::ItemRole::LabelRole, label13);

        spinGap3 = new QSpinBox(RawFormatDialog);
        spinGap3->setObjectName("spinGap3");
        spinGap3->setMinimum(0);
        spinGap3->setMaximum(255);
        spinGap3->setValue(27);

        formLayout13->setWidget(0, QFormLayout::ItemRole::FieldRole, spinGap3);


        hboxLayout5->addLayout(formLayout13);

        checkAutoGap3 = new QCheckBox(RawFormatDialog);
        checkAutoGap3->setObjectName("checkAutoGap3");

        hboxLayout5->addWidget(checkAutoGap3);

        formLayout14 = new QFormLayout();
        formLayout14->setObjectName("formLayout14");
        label14 = new QLabel(RawFormatDialog);
        label14->setObjectName("label14");

        formLayout14->setWidget(0, QFormLayout::ItemRole::LabelRole, label14);

        spinPreGap = new QSpinBox(RawFormatDialog);
        spinPreGap->setObjectName("spinPreGap");
        spinPreGap->setMinimum(0);
        spinPreGap->setMaximum(255);
        spinPreGap->setValue(0);

        formLayout14->setWidget(0, QFormLayout::ItemRole::FieldRole, spinPreGap);


        hboxLayout5->addLayout(formLayout14);

        spacerItem3 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout5->addItem(spacerItem3);


        mainLayout->addLayout(hboxLayout5);

        hboxLayout6 = new QHBoxLayout();
        hboxLayout6->setObjectName("hboxLayout6");
        label15 = new QLabel(RawFormatDialog);
        label15->setObjectName("label15");

        hboxLayout6->addWidget(label15);

        comboLayout = new QComboBox(RawFormatDialog);
        comboLayout->setObjectName("comboLayout");
        comboLayout->setMinimumWidth(200);

        hboxLayout6->addWidget(comboLayout);

        spacerItem4 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout6->addItem(spacerItem4);


        mainLayout->addLayout(hboxLayout6);

        hboxLayout7 = new QHBoxLayout();
        hboxLayout7->setObjectName("hboxLayout7");
        btnSaveConfig = new QPushButton(RawFormatDialog);
        btnSaveConfig->setObjectName("btnSaveConfig");

        hboxLayout7->addWidget(btnSaveConfig);

        btnLoadConfig = new QPushButton(RawFormatDialog);
        btnLoadConfig->setObjectName("btnLoadConfig");

        hboxLayout7->addWidget(btnLoadConfig);

        spacerItem5 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout7->addItem(spacerItem5);

        btnLoadRaw = new QPushButton(RawFormatDialog);
        btnLoadRaw->setObjectName("btnLoadRaw");

        hboxLayout7->addWidget(btnLoadRaw);

        btnCreateEmpty = new QPushButton(RawFormatDialog);
        btnCreateEmpty->setObjectName("btnCreateEmpty");

        hboxLayout7->addWidget(btnCreateEmpty);

        btnClose = new QPushButton(RawFormatDialog);
        btnClose->setObjectName("btnClose");

        hboxLayout7->addWidget(btnClose);


        mainLayout->addLayout(hboxLayout7);

        labelInfo = new QLabel(RawFormatDialog);
        labelInfo->setObjectName("labelInfo");
        labelInfo->setWordWrap(true);

        mainLayout->addWidget(labelInfo);


        retranslateUi(RawFormatDialog);

        QMetaObject::connectSlotsByName(RawFormatDialog);
    } // setupUi

    void retranslateUi(QDialog *RawFormatDialog)
    {
        RawFormatDialog->setWindowTitle(QCoreApplication::translate("RawFormatDialog", "RAW File Format Configuration", nullptr));
        label->setText(QCoreApplication::translate("RawFormatDialog", "Track type:", nullptr));
        label1->setText(QCoreApplication::translate("RawFormatDialog", "Number of Track:", nullptr));
        label2->setText(QCoreApplication::translate("RawFormatDialog", "Number of side:", nullptr));
        comboSides->setItemText(0, QCoreApplication::translate("RawFormatDialog", "1 Side", nullptr));
        comboSides->setItemText(1, QCoreApplication::translate("RawFormatDialog", "2 Sides", nullptr));

        checkSidesGrouped->setText(QCoreApplication::translate("RawFormatDialog", "Tracks of a side\n"
"grouped in the file", nullptr));
        label3->setText(QCoreApplication::translate("RawFormatDialog", "Bitrate:", nullptr));
        label4->setText(QCoreApplication::translate("RawFormatDialog", "Sector per track:", nullptr));
        label5->setText(QCoreApplication::translate("RawFormatDialog", "Sector size:", nullptr));
        comboSectorSize->setItemText(0, QCoreApplication::translate("RawFormatDialog", "128 Bytes", nullptr));
        comboSectorSize->setItemText(1, QCoreApplication::translate("RawFormatDialog", "256 Bytes", nullptr));
        comboSectorSize->setItemText(2, QCoreApplication::translate("RawFormatDialog", "512 Bytes", nullptr));
        comboSectorSize->setItemText(3, QCoreApplication::translate("RawFormatDialog", "1024 Bytes", nullptr));
        comboSectorSize->setItemText(4, QCoreApplication::translate("RawFormatDialog", "2048 Bytes", nullptr));
        comboSectorSize->setItemText(5, QCoreApplication::translate("RawFormatDialog", "4096 Bytes", nullptr));
        comboSectorSize->setItemText(6, QCoreApplication::translate("RawFormatDialog", "8192 Bytes", nullptr));

        checkReverseSide->setText(QCoreApplication::translate("RawFormatDialog", "Reverse side", nullptr));
        label6->setText(QCoreApplication::translate("RawFormatDialog", "RPM:", nullptr));
        comboRPM->setItemText(0, QCoreApplication::translate("RawFormatDialog", "300", nullptr));
        comboRPM->setItemText(1, QCoreApplication::translate("RawFormatDialog", "360", nullptr));

        label7->setText(QCoreApplication::translate("RawFormatDialog", "Sector ID start:", nullptr));
        checkInterSideNumbering->setText(QCoreApplication::translate("RawFormatDialog", "Inter side sector\n"
"numbering", nullptr));
        label8->setText(QCoreApplication::translate("RawFormatDialog", "Interleave:", nullptr));
        label9->setText(QCoreApplication::translate("RawFormatDialog", "Skew:", nullptr));
        checkSideBased->setText(QCoreApplication::translate("RawFormatDialog", "Side based", nullptr));
        label10->setText(QCoreApplication::translate("RawFormatDialog", "Total Sector:", nullptr));
        editTotalSectors->setText(QCoreApplication::translate("RawFormatDialog", "1600", nullptr));
        label11->setText(QCoreApplication::translate("RawFormatDialog", "Total Size:", nullptr));
        editTotalSize->setText(QCoreApplication::translate("RawFormatDialog", "409600", nullptr));
        label12->setText(QCoreApplication::translate("RawFormatDialog", "Format value:", nullptr));
        label13->setText(QCoreApplication::translate("RawFormatDialog", "GAP3 length:", nullptr));
        checkAutoGap3->setText(QCoreApplication::translate("RawFormatDialog", "Auto GAP3", nullptr));
        label14->setText(QCoreApplication::translate("RawFormatDialog", "PRE-GAP length:", nullptr));
        label15->setText(QCoreApplication::translate("RawFormatDialog", "Predefined Disk Layout:", nullptr));
        btnSaveConfig->setText(QCoreApplication::translate("RawFormatDialog", "Save config", nullptr));
        btnLoadConfig->setText(QCoreApplication::translate("RawFormatDialog", "Load config", nullptr));
        btnLoadRaw->setText(QCoreApplication::translate("RawFormatDialog", "Load RAW file", nullptr));
        btnCreateEmpty->setText(QCoreApplication::translate("RawFormatDialog", "Create Empty\n"
"Floppy", nullptr));
        btnClose->setText(QCoreApplication::translate("RawFormatDialog", "Close", nullptr));
        labelInfo->setText(QCoreApplication::translate("RawFormatDialog", "To batch convert RAW files you can use the Batch Converter function and check the RAW files mode check box.", nullptr));
        labelInfo->setStyleSheet(QCoreApplication::translate("RawFormatDialog", "color: gray; font-size: 9pt;", nullptr));
    } // retranslateUi

};

namespace Ui {
    class RawFormatDialog: public Ui_RawFormatDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RAWFORMATDIALOG_H

/********************************************************************************
** Form generated from reading UI file 'tab_hardware.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TAB_HARDWARE_H
#define UI_TAB_HARDWARE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TabHardware
{
public:
    QVBoxLayout *verticalLayout_main;
    QScrollArea *scrollArea;
    QWidget *scrollContent;
    QHBoxLayout *mainHLayout;
    QVBoxLayout *col1;
    QGroupBox *groupController;
    QGridLayout *gridLayout;
    QLabel *label;
    QRadioButton *radioSource;
    QRadioButton *radioDestination;
    QLabel *label1;
    QComboBox *comboController;
    QLabel *label2;
    QLabel *labelControllerStatus;
    QGroupBox *groupConnection;
    QGridLayout *gridLayout1;
    QLabel *label3;
    QComboBox *comboPort;
    QPushButton *btnRefreshPorts;
    QLabel *label4;
    QComboBox *comboDriveSelect;
    QHBoxLayout *hboxLayout;
    QPushButton *btnDetect;
    QPushButton *btnConnect;
    QGroupBox *groupTest;
    QGridLayout *gridLayout2;
    QPushButton *btnCalibrate;
    QPushButton *btnSeekTest;
    QPushButton *btnReadTest;
    QPushButton *btnRPMTest;
    QSpacerItem *spacerItem;
    QVBoxLayout *col2;
    QGroupBox *groupDetection;
    QHBoxLayout *hboxLayout1;
    QRadioButton *radioAutoDetect;
    QRadioButton *radioManual;
    QGroupBox *groupDrive;
    QFormLayout *formLayout;
    QLabel *label5;
    QComboBox *comboDriveType;
    QLabel *label6;
    QComboBox *comboTracks;
    QLabel *label7;
    QComboBox *comboHeads;
    QLabel *label8;
    QComboBox *comboDensity;
    QLabel *label9;
    QComboBox *comboRPM;
    QGroupBox *groupMotor;
    QVBoxLayout *vboxLayout;
    QHBoxLayout *hboxLayout2;
    QPushButton *btnMotorOn;
    QPushButton *btnMotorOff;
    QCheckBox *checkAutoSpinDown;
    QSpacerItem *spacerItem1;
    QVBoxLayout *col3;
    QGroupBox *groupAdvanced;
    QVBoxLayout *vboxLayout1;
    QCheckBox *checkDoubleStep;
    QCheckBox *checkIgnoreIndex;
    QGroupBox *groupInfo;
    QFormLayout *formLayout1;
    QLabel *label10;
    QLabel *labelFirmware;
    QLabel *label11;
    QLabel *labelIndex;
    QGroupBox *groupStatus;
    QVBoxLayout *vboxLayout2;
    QTextEdit *textStatus;
    QSpacerItem *spacerItem2;

    void setupUi(QWidget *TabHardware)
    {
        if (TabHardware->objectName().isEmpty())
            TabHardware->setObjectName("TabHardware");
        TabHardware->resize(900, 600);
        verticalLayout_main = new QVBoxLayout(TabHardware);
        verticalLayout_main->setObjectName("verticalLayout_main");
        verticalLayout_main->setContentsMargins(4, 4, 4, 4);
        scrollArea = new QScrollArea(TabHardware);
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
        groupController = new QGroupBox(scrollContent);
        groupController->setObjectName("groupController");
        gridLayout = new QGridLayout(groupController);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setVerticalSpacing(4);
        label = new QLabel(groupController);
        label->setObjectName("label");

        gridLayout->addWidget(label, 0, 0, 1, 1);

        radioSource = new QRadioButton(groupController);
        radioSource->setObjectName("radioSource");
        radioSource->setChecked(true);

        gridLayout->addWidget(radioSource, 0, 1, 1, 1);

        radioDestination = new QRadioButton(groupController);
        radioDestination->setObjectName("radioDestination");

        gridLayout->addWidget(radioDestination, 0, 2, 1, 1);

        label1 = new QLabel(groupController);
        label1->setObjectName("label1");

        gridLayout->addWidget(label1, 1, 0, 1, 1);

        comboController = new QComboBox(groupController);
        comboController->setObjectName("comboController");
        comboController->setMinimumWidth(180);

        gridLayout->addWidget(comboController, 1, 1, 1, 2);

        label2 = new QLabel(groupController);
        label2->setObjectName("label2");

        gridLayout->addWidget(label2, 2, 0, 1, 1);

        labelControllerStatus = new QLabel(groupController);
        labelControllerStatus->setObjectName("labelControllerStatus");

        gridLayout->addWidget(labelControllerStatus, 2, 1, 1, 2);


        col1->addWidget(groupController);

        groupConnection = new QGroupBox(scrollContent);
        groupConnection->setObjectName("groupConnection");
        gridLayout1 = new QGridLayout(groupConnection);
        gridLayout1->setObjectName("gridLayout1");
        gridLayout1->setVerticalSpacing(4);
        label3 = new QLabel(groupConnection);
        label3->setObjectName("label3");

        gridLayout1->addWidget(label3, 0, 0, 1, 1);

        comboPort = new QComboBox(groupConnection);
        comboPort->setObjectName("comboPort");

        gridLayout1->addWidget(comboPort, 0, 1, 1, 1);

        btnRefreshPorts = new QPushButton(groupConnection);
        btnRefreshPorts->setObjectName("btnRefreshPorts");
        btnRefreshPorts->setMaximumWidth(30);

        gridLayout1->addWidget(btnRefreshPorts, 0, 2, 1, 1);

        label4 = new QLabel(groupConnection);
        label4->setObjectName("label4");

        gridLayout1->addWidget(label4, 1, 0, 1, 1);

        comboDriveSelect = new QComboBox(groupConnection);
        comboDriveSelect->addItem(QString());
        comboDriveSelect->addItem(QString());
        comboDriveSelect->setObjectName("comboDriveSelect");

        gridLayout1->addWidget(comboDriveSelect, 1, 1, 1, 2);

        hboxLayout = new QHBoxLayout();
        hboxLayout->setObjectName("hboxLayout");
        btnDetect = new QPushButton(groupConnection);
        btnDetect->setObjectName("btnDetect");

        hboxLayout->addWidget(btnDetect);

        btnConnect = new QPushButton(groupConnection);
        btnConnect->setObjectName("btnConnect");

        hboxLayout->addWidget(btnConnect);


        gridLayout1->addLayout(hboxLayout, 2, 0, 1, 3);


        col1->addWidget(groupConnection);

        groupTest = new QGroupBox(scrollContent);
        groupTest->setObjectName("groupTest");
        gridLayout2 = new QGridLayout(groupTest);
        gridLayout2->setObjectName("gridLayout2");
        gridLayout2->setVerticalSpacing(4);
        btnCalibrate = new QPushButton(groupTest);
        btnCalibrate->setObjectName("btnCalibrate");

        gridLayout2->addWidget(btnCalibrate, 0, 0, 1, 1);

        btnSeekTest = new QPushButton(groupTest);
        btnSeekTest->setObjectName("btnSeekTest");

        gridLayout2->addWidget(btnSeekTest, 0, 1, 1, 1);

        btnReadTest = new QPushButton(groupTest);
        btnReadTest->setObjectName("btnReadTest");

        gridLayout2->addWidget(btnReadTest, 1, 0, 1, 1);

        btnRPMTest = new QPushButton(groupTest);
        btnRPMTest->setObjectName("btnRPMTest");

        gridLayout2->addWidget(btnRPMTest, 1, 1, 1, 1);


        col1->addWidget(groupTest);

        spacerItem = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        col1->addItem(spacerItem);


        mainHLayout->addLayout(col1);

        col2 = new QVBoxLayout();
        col2->setSpacing(6);
        col2->setObjectName("col2");
        groupDetection = new QGroupBox(scrollContent);
        groupDetection->setObjectName("groupDetection");
        hboxLayout1 = new QHBoxLayout(groupDetection);
        hboxLayout1->setObjectName("hboxLayout1");
        radioAutoDetect = new QRadioButton(groupDetection);
        radioAutoDetect->setObjectName("radioAutoDetect");
        radioAutoDetect->setChecked(true);

        hboxLayout1->addWidget(radioAutoDetect);

        radioManual = new QRadioButton(groupDetection);
        radioManual->setObjectName("radioManual");

        hboxLayout1->addWidget(radioManual);


        col2->addWidget(groupDetection);

        groupDrive = new QGroupBox(scrollContent);
        groupDrive->setObjectName("groupDrive");
        formLayout = new QFormLayout(groupDrive);
        formLayout->setObjectName("formLayout");
        formLayout->setVerticalSpacing(4);
        label5 = new QLabel(groupDrive);
        label5->setObjectName("label5");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, label5);

        comboDriveType = new QComboBox(groupDrive);
        comboDriveType->addItem(QString());
        comboDriveType->addItem(QString());
        comboDriveType->addItem(QString());
        comboDriveType->addItem(QString());
        comboDriveType->setObjectName("comboDriveType");

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, comboDriveType);

        label6 = new QLabel(groupDrive);
        label6->setObjectName("label6");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, label6);

        comboTracks = new QComboBox(groupDrive);
        comboTracks->addItem(QString());
        comboTracks->addItem(QString());
        comboTracks->addItem(QString());
        comboTracks->addItem(QString());
        comboTracks->setObjectName("comboTracks");

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, comboTracks);

        label7 = new QLabel(groupDrive);
        label7->setObjectName("label7");

        formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, label7);

        comboHeads = new QComboBox(groupDrive);
        comboHeads->addItem(QString());
        comboHeads->addItem(QString());
        comboHeads->setObjectName("comboHeads");

        formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, comboHeads);

        label8 = new QLabel(groupDrive);
        label8->setObjectName("label8");

        formLayout->setWidget(3, QFormLayout::ItemRole::LabelRole, label8);

        comboDensity = new QComboBox(groupDrive);
        comboDensity->addItem(QString());
        comboDensity->addItem(QString());
        comboDensity->addItem(QString());
        comboDensity->setObjectName("comboDensity");

        formLayout->setWidget(3, QFormLayout::ItemRole::FieldRole, comboDensity);

        label9 = new QLabel(groupDrive);
        label9->setObjectName("label9");

        formLayout->setWidget(4, QFormLayout::ItemRole::LabelRole, label9);

        comboRPM = new QComboBox(groupDrive);
        comboRPM->addItem(QString());
        comboRPM->addItem(QString());
        comboRPM->setObjectName("comboRPM");

        formLayout->setWidget(4, QFormLayout::ItemRole::FieldRole, comboRPM);


        col2->addWidget(groupDrive);

        groupMotor = new QGroupBox(scrollContent);
        groupMotor->setObjectName("groupMotor");
        vboxLayout = new QVBoxLayout(groupMotor);
        vboxLayout->setObjectName("vboxLayout");
        hboxLayout2 = new QHBoxLayout();
        hboxLayout2->setObjectName("hboxLayout2");
        btnMotorOn = new QPushButton(groupMotor);
        btnMotorOn->setObjectName("btnMotorOn");

        hboxLayout2->addWidget(btnMotorOn);

        btnMotorOff = new QPushButton(groupMotor);
        btnMotorOff->setObjectName("btnMotorOff");

        hboxLayout2->addWidget(btnMotorOff);


        vboxLayout->addLayout(hboxLayout2);

        checkAutoSpinDown = new QCheckBox(groupMotor);
        checkAutoSpinDown->setObjectName("checkAutoSpinDown");
        checkAutoSpinDown->setChecked(true);

        vboxLayout->addWidget(checkAutoSpinDown);


        col2->addWidget(groupMotor);

        spacerItem1 = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        col2->addItem(spacerItem1);


        mainHLayout->addLayout(col2);

        col3 = new QVBoxLayout();
        col3->setSpacing(6);
        col3->setObjectName("col3");
        groupAdvanced = new QGroupBox(scrollContent);
        groupAdvanced->setObjectName("groupAdvanced");
        vboxLayout1 = new QVBoxLayout(groupAdvanced);
        vboxLayout1->setObjectName("vboxLayout1");
        checkDoubleStep = new QCheckBox(groupAdvanced);
        checkDoubleStep->setObjectName("checkDoubleStep");

        vboxLayout1->addWidget(checkDoubleStep);

        checkIgnoreIndex = new QCheckBox(groupAdvanced);
        checkIgnoreIndex->setObjectName("checkIgnoreIndex");

        vboxLayout1->addWidget(checkIgnoreIndex);


        col3->addWidget(groupAdvanced);

        groupInfo = new QGroupBox(scrollContent);
        groupInfo->setObjectName("groupInfo");
        formLayout1 = new QFormLayout(groupInfo);
        formLayout1->setObjectName("formLayout1");
        formLayout1->setVerticalSpacing(4);
        label10 = new QLabel(groupInfo);
        label10->setObjectName("label10");

        formLayout1->setWidget(0, QFormLayout::ItemRole::LabelRole, label10);

        labelFirmware = new QLabel(groupInfo);
        labelFirmware->setObjectName("labelFirmware");

        formLayout1->setWidget(0, QFormLayout::ItemRole::FieldRole, labelFirmware);

        label11 = new QLabel(groupInfo);
        label11->setObjectName("label11");

        formLayout1->setWidget(1, QFormLayout::ItemRole::LabelRole, label11);

        labelIndex = new QLabel(groupInfo);
        labelIndex->setObjectName("labelIndex");

        formLayout1->setWidget(1, QFormLayout::ItemRole::FieldRole, labelIndex);


        col3->addWidget(groupInfo);

        groupStatus = new QGroupBox(scrollContent);
        groupStatus->setObjectName("groupStatus");
        vboxLayout2 = new QVBoxLayout(groupStatus);
        vboxLayout2->setObjectName("vboxLayout2");
        textStatus = new QTextEdit(groupStatus);
        textStatus->setObjectName("textStatus");
        textStatus->setReadOnly(true);
        textStatus->setMaximumHeight(100);

        vboxLayout2->addWidget(textStatus);


        col3->addWidget(groupStatus);

        spacerItem2 = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        col3->addItem(spacerItem2);


        mainHLayout->addLayout(col3);

        scrollArea->setWidget(scrollContent);

        verticalLayout_main->addWidget(scrollArea);


        retranslateUi(TabHardware);

        QMetaObject::connectSlotsByName(TabHardware);
    } // setupUi

    void retranslateUi(QWidget *TabHardware)
    {
        groupController->setTitle(QCoreApplication::translate("TabHardware", "Controller", nullptr));
        groupController->setStyleSheet(QCoreApplication::translate("TabHardware", "QGroupBox { font-weight: bold; }", nullptr));
        label->setText(QCoreApplication::translate("TabHardware", "Role:", nullptr));
        radioSource->setText(QCoreApplication::translate("TabHardware", "Source", nullptr));
        radioDestination->setText(QCoreApplication::translate("TabHardware", "Destination", nullptr));
        label1->setText(QCoreApplication::translate("TabHardware", "Type:", nullptr));
        label2->setText(QCoreApplication::translate("TabHardware", "Status:", nullptr));
        labelControllerStatus->setText(QCoreApplication::translate("TabHardware", "Not connected", nullptr));
        labelControllerStatus->setStyleSheet(QCoreApplication::translate("TabHardware", "color: #888;", nullptr));
        groupConnection->setTitle(QCoreApplication::translate("TabHardware", "Connection", nullptr));
        groupConnection->setStyleSheet(QCoreApplication::translate("TabHardware", "QGroupBox { font-weight: bold; }", nullptr));
        label3->setText(QCoreApplication::translate("TabHardware", "Port:", nullptr));
        btnRefreshPorts->setText(QCoreApplication::translate("TabHardware", "\342\206\273", nullptr));
        label4->setText(QCoreApplication::translate("TabHardware", "Drive:", nullptr));
        comboDriveSelect->setItemText(0, QCoreApplication::translate("TabHardware", "A: (Drive 0)", nullptr));
        comboDriveSelect->setItemText(1, QCoreApplication::translate("TabHardware", "B: (Drive 1)", nullptr));

        btnDetect->setText(QCoreApplication::translate("TabHardware", "Detect", nullptr));
        btnConnect->setText(QCoreApplication::translate("TabHardware", "Connect", nullptr));
        groupTest->setTitle(QCoreApplication::translate("TabHardware", "Test", nullptr));
        groupTest->setStyleSheet(QCoreApplication::translate("TabHardware", "QGroupBox { font-weight: bold; }", nullptr));
        btnCalibrate->setText(QCoreApplication::translate("TabHardware", "Calibrate", nullptr));
        btnSeekTest->setText(QCoreApplication::translate("TabHardware", "Seek Test", nullptr));
        btnReadTest->setText(QCoreApplication::translate("TabHardware", "Read Test", nullptr));
        btnRPMTest->setText(QCoreApplication::translate("TabHardware", "RPM Test", nullptr));
        groupDetection->setTitle(QCoreApplication::translate("TabHardware", "Detection Mode", nullptr));
        groupDetection->setStyleSheet(QCoreApplication::translate("TabHardware", "QGroupBox { font-weight: bold; }", nullptr));
        radioAutoDetect->setText(QCoreApplication::translate("TabHardware", "Auto-Detect", nullptr));
        radioManual->setText(QCoreApplication::translate("TabHardware", "Manual", nullptr));
        groupDrive->setTitle(QCoreApplication::translate("TabHardware", "Drive", nullptr));
        groupDrive->setStyleSheet(QCoreApplication::translate("TabHardware", "QGroupBox { font-weight: bold; }", nullptr));
        label5->setText(QCoreApplication::translate("TabHardware", "Drive Type:", nullptr));
        comboDriveType->setItemText(0, QCoreApplication::translate("TabHardware", "3.5\" DD (720K)", nullptr));
        comboDriveType->setItemText(1, QCoreApplication::translate("TabHardware", "3.5\" HD (1.44M)", nullptr));
        comboDriveType->setItemText(2, QCoreApplication::translate("TabHardware", "5.25\" DD (360K)", nullptr));
        comboDriveType->setItemText(3, QCoreApplication::translate("TabHardware", "5.25\" HD (1.2M)", nullptr));

        label6->setText(QCoreApplication::translate("TabHardware", "Tracks:", nullptr));
        comboTracks->setItemText(0, QCoreApplication::translate("TabHardware", "40", nullptr));
        comboTracks->setItemText(1, QCoreApplication::translate("TabHardware", "80", nullptr));
        comboTracks->setItemText(2, QCoreApplication::translate("TabHardware", "82", nullptr));
        comboTracks->setItemText(3, QCoreApplication::translate("TabHardware", "84", nullptr));

        label7->setText(QCoreApplication::translate("TabHardware", "Heads:", nullptr));
        comboHeads->setItemText(0, QCoreApplication::translate("TabHardware", "1", nullptr));
        comboHeads->setItemText(1, QCoreApplication::translate("TabHardware", "2", nullptr));

        label8->setText(QCoreApplication::translate("TabHardware", "Density:", nullptr));
        comboDensity->setItemText(0, QCoreApplication::translate("TabHardware", "DD", nullptr));
        comboDensity->setItemText(1, QCoreApplication::translate("TabHardware", "HD", nullptr));
        comboDensity->setItemText(2, QCoreApplication::translate("TabHardware", "ED", nullptr));

        label9->setText(QCoreApplication::translate("TabHardware", "RPM:", nullptr));
        comboRPM->setItemText(0, QCoreApplication::translate("TabHardware", "300", nullptr));
        comboRPM->setItemText(1, QCoreApplication::translate("TabHardware", "360", nullptr));

        groupMotor->setTitle(QCoreApplication::translate("TabHardware", "Motor Control", nullptr));
        groupMotor->setStyleSheet(QCoreApplication::translate("TabHardware", "QGroupBox { font-weight: bold; }", nullptr));
        btnMotorOn->setText(QCoreApplication::translate("TabHardware", "Motor ON", nullptr));
        btnMotorOff->setText(QCoreApplication::translate("TabHardware", "Motor OFF", nullptr));
        checkAutoSpinDown->setText(QCoreApplication::translate("TabHardware", "Auto spin-down (10s)", nullptr));
        groupAdvanced->setTitle(QCoreApplication::translate("TabHardware", "Advanced", nullptr));
        groupAdvanced->setStyleSheet(QCoreApplication::translate("TabHardware", "QGroupBox { font-weight: bold; }", nullptr));
        checkDoubleStep->setText(QCoreApplication::translate("TabHardware", "Double step (40T in 80T drive)", nullptr));
        checkIgnoreIndex->setText(QCoreApplication::translate("TabHardware", "Ignore index hole", nullptr));
        groupInfo->setTitle(QCoreApplication::translate("TabHardware", "Detected Info", nullptr));
        groupInfo->setStyleSheet(QCoreApplication::translate("TabHardware", "QGroupBox { font-weight: bold; }", nullptr));
        label10->setText(QCoreApplication::translate("TabHardware", "Firmware:", nullptr));
        labelFirmware->setText(QCoreApplication::translate("TabHardware", "-", nullptr));
        label11->setText(QCoreApplication::translate("TabHardware", "Index:", nullptr));
        labelIndex->setText(QCoreApplication::translate("TabHardware", "-", nullptr));
        groupStatus->setTitle(QCoreApplication::translate("TabHardware", "Status", nullptr));
        groupStatus->setStyleSheet(QCoreApplication::translate("TabHardware", "QGroupBox { font-weight: bold; }", nullptr));
        (void)TabHardware;
    } // retranslateUi

};

namespace Ui {
    class TabHardware: public Ui_TabHardware {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TAB_HARDWARE_H

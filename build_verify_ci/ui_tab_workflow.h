/********************************************************************************
** Form generated from reading UI file 'tab_workflow.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TAB_WORKFLOW_H
#define UI_TAB_WORKFLOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TabWorkflow
{
public:
    QVBoxLayout *verticalLayout_main;
    QHBoxLayout *horizontalLayout_sourceDestination;
    QGroupBox *groupSource;
    QVBoxLayout *verticalLayout_source;
    QVBoxLayout *verticalLayout_sourceButtons;
    QPushButton *btnSourceFlux;
    QPushButton *btnSourceUSB;
    QPushButton *btnSourceFile;
    QGroupBox *groupSourceStatus;
    QVBoxLayout *verticalLayout_sourceStatus;
    QLabel *labelSourceStatus;
    QGroupBox *groupDestination;
    QVBoxLayout *verticalLayout_destination;
    QVBoxLayout *verticalLayout_destButtons;
    QPushButton *btnDestFlux;
    QPushButton *btnDestUSB;
    QPushButton *btnDestFile;
    QGroupBox *groupDestStatus;
    QVBoxLayout *verticalLayout_destStatus;
    QLabel *labelDestStatus;
    QGroupBox *groupOperation;
    QHBoxLayout *hboxLayout;
    QRadioButton *radioRead;
    QRadioButton *radioWrite;
    QRadioButton *radioVerify;
    QRadioButton *radioConvert;
    QPushButton *btnAnalyze;
    QSpacerItem *spacerItem;
    QLabel *labelOperationHint;
    QGroupBox *groupTracks;
    QHBoxLayout *hboxLayout1;
    QFrame *frameSide0;
    QVBoxLayout *vboxLayout;
    QLabel *labelSide0;
    QLabel *labelGridSide0;
    QFrame *frameSide1;
    QVBoxLayout *vboxLayout1;
    QLabel *labelSide1;
    QLabel *labelGridSide1;
    QVBoxLayout *vboxLayout2;
    QLabel *labelLegend;
    QFrame *line;
    QLabel *labelStats;
    QSpacerItem *spacerItem1;
    QGroupBox *groupProgress;
    QVBoxLayout *vboxLayout3;
    QProgressBar *progressBar;
    QHBoxLayout *hboxLayout2;
    QLabel *labelTime;
    QSpacerItem *spacerItem2;
    QLabel *labelSpeed;
    QHBoxLayout *hboxLayout3;
    QPushButton *btnStartAbort;
    QPushButton *btnPause;
    QSpacerItem *spacerItem3;
    QPushButton *btnLog;
    QPushButton *btnHistogram;

    void setupUi(QWidget *TabWorkflow)
    {
        if (TabWorkflow->objectName().isEmpty())
            TabWorkflow->setObjectName("TabWorkflow");
        TabWorkflow->resize(900, 700);
        verticalLayout_main = new QVBoxLayout(TabWorkflow);
        verticalLayout_main->setSpacing(8);
        verticalLayout_main->setObjectName("verticalLayout_main");
        horizontalLayout_sourceDestination = new QHBoxLayout();
        horizontalLayout_sourceDestination->setSpacing(8);
        horizontalLayout_sourceDestination->setObjectName("horizontalLayout_sourceDestination");
        groupSource = new QGroupBox(TabWorkflow);
        groupSource->setObjectName("groupSource");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(groupSource->sizePolicy().hasHeightForWidth());
        groupSource->setSizePolicy(sizePolicy);
        groupSource->setMinimumSize(QSize(400, 0));
        verticalLayout_source = new QVBoxLayout(groupSource);
        verticalLayout_source->setObjectName("verticalLayout_source");
        verticalLayout_sourceButtons = new QVBoxLayout();
        verticalLayout_sourceButtons->setObjectName("verticalLayout_sourceButtons");
        btnSourceFlux = new QPushButton(groupSource);
        btnSourceFlux->setObjectName("btnSourceFlux");
        btnSourceFlux->setCheckable(true);
        btnSourceFlux->setChecked(true);
        btnSourceFlux->setMinimumHeight(35);

        verticalLayout_sourceButtons->addWidget(btnSourceFlux);

        btnSourceUSB = new QPushButton(groupSource);
        btnSourceUSB->setObjectName("btnSourceUSB");
        btnSourceUSB->setCheckable(true);
        btnSourceUSB->setMinimumHeight(35);

        verticalLayout_sourceButtons->addWidget(btnSourceUSB);

        btnSourceFile = new QPushButton(groupSource);
        btnSourceFile->setObjectName("btnSourceFile");
        btnSourceFile->setCheckable(true);
        btnSourceFile->setMinimumHeight(35);

        verticalLayout_sourceButtons->addWidget(btnSourceFile);


        verticalLayout_source->addLayout(verticalLayout_sourceButtons);

        groupSourceStatus = new QGroupBox(groupSource);
        groupSourceStatus->setObjectName("groupSourceStatus");
        groupSourceStatus->setMinimumHeight(80);
        verticalLayout_sourceStatus = new QVBoxLayout(groupSourceStatus);
        verticalLayout_sourceStatus->setObjectName("verticalLayout_sourceStatus");
        labelSourceStatus = new QLabel(groupSourceStatus);
        labelSourceStatus->setObjectName("labelSourceStatus");
        labelSourceStatus->setWordWrap(true);

        verticalLayout_sourceStatus->addWidget(labelSourceStatus);


        verticalLayout_source->addWidget(groupSourceStatus);


        horizontalLayout_sourceDestination->addWidget(groupSource);

        groupDestination = new QGroupBox(TabWorkflow);
        groupDestination->setObjectName("groupDestination");
        sizePolicy.setHeightForWidth(groupDestination->sizePolicy().hasHeightForWidth());
        groupDestination->setSizePolicy(sizePolicy);
        groupDestination->setMinimumSize(QSize(400, 0));
        verticalLayout_destination = new QVBoxLayout(groupDestination);
        verticalLayout_destination->setObjectName("verticalLayout_destination");
        verticalLayout_destButtons = new QVBoxLayout();
        verticalLayout_destButtons->setObjectName("verticalLayout_destButtons");
        btnDestFlux = new QPushButton(groupDestination);
        btnDestFlux->setObjectName("btnDestFlux");
        btnDestFlux->setCheckable(true);
        btnDestFlux->setMinimumHeight(35);

        verticalLayout_destButtons->addWidget(btnDestFlux);

        btnDestUSB = new QPushButton(groupDestination);
        btnDestUSB->setObjectName("btnDestUSB");
        btnDestUSB->setCheckable(true);
        btnDestUSB->setMinimumHeight(35);

        verticalLayout_destButtons->addWidget(btnDestUSB);

        btnDestFile = new QPushButton(groupDestination);
        btnDestFile->setObjectName("btnDestFile");
        btnDestFile->setCheckable(true);
        btnDestFile->setChecked(true);
        btnDestFile->setMinimumHeight(35);

        verticalLayout_destButtons->addWidget(btnDestFile);


        verticalLayout_destination->addLayout(verticalLayout_destButtons);

        groupDestStatus = new QGroupBox(groupDestination);
        groupDestStatus->setObjectName("groupDestStatus");
        groupDestStatus->setMinimumHeight(80);
        verticalLayout_destStatus = new QVBoxLayout(groupDestStatus);
        verticalLayout_destStatus->setObjectName("verticalLayout_destStatus");
        labelDestStatus = new QLabel(groupDestStatus);
        labelDestStatus->setObjectName("labelDestStatus");
        labelDestStatus->setWordWrap(true);

        verticalLayout_destStatus->addWidget(labelDestStatus);


        verticalLayout_destination->addWidget(groupDestStatus);


        horizontalLayout_sourceDestination->addWidget(groupDestination);


        verticalLayout_main->addLayout(horizontalLayout_sourceDestination);

        groupOperation = new QGroupBox(TabWorkflow);
        groupOperation->setObjectName("groupOperation");
        groupOperation->setMaximumHeight(70);
        hboxLayout = new QHBoxLayout(groupOperation);
        hboxLayout->setObjectName("hboxLayout");
        radioRead = new QRadioButton(groupOperation);
        radioRead->setObjectName("radioRead");
        radioRead->setChecked(true);

        hboxLayout->addWidget(radioRead);

        radioWrite = new QRadioButton(groupOperation);
        radioWrite->setObjectName("radioWrite");

        hboxLayout->addWidget(radioWrite);

        radioVerify = new QRadioButton(groupOperation);
        radioVerify->setObjectName("radioVerify");

        hboxLayout->addWidget(radioVerify);

        radioConvert = new QRadioButton(groupOperation);
        radioConvert->setObjectName("radioConvert");

        hboxLayout->addWidget(radioConvert);

        btnAnalyze = new QPushButton(groupOperation);
        btnAnalyze->setObjectName("btnAnalyze");

        hboxLayout->addWidget(btnAnalyze);

        spacerItem = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout->addItem(spacerItem);

        labelOperationHint = new QLabel(groupOperation);
        labelOperationHint->setObjectName("labelOperationHint");

        hboxLayout->addWidget(labelOperationHint);


        verticalLayout_main->addWidget(groupOperation);

        groupTracks = new QGroupBox(TabWorkflow);
        groupTracks->setObjectName("groupTracks");
        hboxLayout1 = new QHBoxLayout(groupTracks);
        hboxLayout1->setObjectName("hboxLayout1");
        frameSide0 = new QFrame(groupTracks);
        frameSide0->setObjectName("frameSide0");
        frameSide0->setMinimumSize(QSize(240, 240));
        frameSide0->setMaximumSize(QSize(240, 240));
        frameSide0->setFrameShape(QFrame::Box);
        vboxLayout = new QVBoxLayout(frameSide0);
        vboxLayout->setObjectName("vboxLayout");
        labelSide0 = new QLabel(frameSide0);
        labelSide0->setObjectName("labelSide0");
        labelSide0->setAlignment(Qt::AlignCenter);
        QFont font;
        font.setPointSize(10);
        font.setBold(true);
        labelSide0->setFont(font);

        vboxLayout->addWidget(labelSide0);

        labelGridSide0 = new QLabel(frameSide0);
        labelGridSide0->setObjectName("labelGridSide0");
        labelGridSide0->setAlignment(Qt::AlignCenter);
        labelGridSide0->setMinimumSize(QSize(220, 200));

        vboxLayout->addWidget(labelGridSide0);


        hboxLayout1->addWidget(frameSide0);

        frameSide1 = new QFrame(groupTracks);
        frameSide1->setObjectName("frameSide1");
        frameSide1->setMinimumSize(QSize(240, 240));
        frameSide1->setMaximumSize(QSize(240, 240));
        frameSide1->setFrameShape(QFrame::Box);
        vboxLayout1 = new QVBoxLayout(frameSide1);
        vboxLayout1->setObjectName("vboxLayout1");
        labelSide1 = new QLabel(frameSide1);
        labelSide1->setObjectName("labelSide1");
        labelSide1->setAlignment(Qt::AlignCenter);
        labelSide1->setFont(font);

        vboxLayout1->addWidget(labelSide1);

        labelGridSide1 = new QLabel(frameSide1);
        labelGridSide1->setObjectName("labelGridSide1");
        labelGridSide1->setAlignment(Qt::AlignCenter);
        labelGridSide1->setMinimumSize(QSize(220, 200));

        vboxLayout1->addWidget(labelGridSide1);


        hboxLayout1->addWidget(frameSide1);

        vboxLayout2 = new QVBoxLayout();
        vboxLayout2->setObjectName("vboxLayout2");
        labelLegend = new QLabel(groupTracks);
        labelLegend->setObjectName("labelLegend");

        vboxLayout2->addWidget(labelLegend);

        line = new QFrame(groupTracks);
        line->setObjectName("line");
        line->setFrameShape(QFrame::Shape::HLine);
        line->setFrameShadow(QFrame::Shadow::Sunken);

        vboxLayout2->addWidget(line);

        labelStats = new QLabel(groupTracks);
        labelStats->setObjectName("labelStats");

        vboxLayout2->addWidget(labelStats);

        spacerItem1 = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        vboxLayout2->addItem(spacerItem1);


        hboxLayout1->addLayout(vboxLayout2);


        verticalLayout_main->addWidget(groupTracks);

        groupProgress = new QGroupBox(TabWorkflow);
        groupProgress->setObjectName("groupProgress");
        groupProgress->setMaximumHeight(80);
        vboxLayout3 = new QVBoxLayout(groupProgress);
        vboxLayout3->setObjectName("vboxLayout3");
        progressBar = new QProgressBar(groupProgress);
        progressBar->setObjectName("progressBar");
        progressBar->setValue(0);

        vboxLayout3->addWidget(progressBar);

        hboxLayout2 = new QHBoxLayout();
        hboxLayout2->setObjectName("hboxLayout2");
        labelTime = new QLabel(groupProgress);
        labelTime->setObjectName("labelTime");

        hboxLayout2->addWidget(labelTime);

        spacerItem2 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout2->addItem(spacerItem2);

        labelSpeed = new QLabel(groupProgress);
        labelSpeed->setObjectName("labelSpeed");

        hboxLayout2->addWidget(labelSpeed);


        vboxLayout3->addLayout(hboxLayout2);


        verticalLayout_main->addWidget(groupProgress);

        hboxLayout3 = new QHBoxLayout();
        hboxLayout3->setObjectName("hboxLayout3");
        btnStartAbort = new QPushButton(TabWorkflow);
        btnStartAbort->setObjectName("btnStartAbort");
        btnStartAbort->setMinimumHeight(40);

        hboxLayout3->addWidget(btnStartAbort);

        btnPause = new QPushButton(TabWorkflow);
        btnPause->setObjectName("btnPause");
        btnPause->setMinimumHeight(40);
        btnPause->setEnabled(false);

        hboxLayout3->addWidget(btnPause);

        spacerItem3 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout3->addItem(spacerItem3);

        btnLog = new QPushButton(TabWorkflow);
        btnLog->setObjectName("btnLog");
        btnLog->setMinimumHeight(40);

        hboxLayout3->addWidget(btnLog);

        btnHistogram = new QPushButton(TabWorkflow);
        btnHistogram->setObjectName("btnHistogram");
        btnHistogram->setMinimumHeight(40);

        hboxLayout3->addWidget(btnHistogram);


        verticalLayout_main->addLayout(hboxLayout3);


        retranslateUi(TabWorkflow);

        QMetaObject::connectSlotsByName(TabWorkflow);
    } // setupUi

    void retranslateUi(QWidget *TabWorkflow)
    {
        groupSource->setTitle(QCoreApplication::translate("TabWorkflow", "\360\237\223\202 SOURCE (Read From)", nullptr));
        btnSourceFlux->setText(QCoreApplication::translate("TabWorkflow", "\360\237\223\241 Flux Device", nullptr));
        btnSourceFlux->setStyleSheet(QCoreApplication::translate("TabWorkflow", "QPushButton:checked { background-color: #4CAF50; color: white; font-weight: bold; }", nullptr));
        btnSourceUSB->setText(QCoreApplication::translate("TabWorkflow", "\360\237\224\214 USB Device", nullptr));
        btnSourceUSB->setStyleSheet(QCoreApplication::translate("TabWorkflow", "QPushButton:checked { background-color: #4CAF50; color: white; font-weight: bold; }", nullptr));
        btnSourceFile->setText(QCoreApplication::translate("TabWorkflow", "\360\237\223\201 Image File", nullptr));
        btnSourceFile->setStyleSheet(QCoreApplication::translate("TabWorkflow", "QPushButton:checked { background-color: #4CAF50; color: white; font-weight: bold; }", nullptr));
        groupSourceStatus->setTitle(QCoreApplication::translate("TabWorkflow", "Status", nullptr));
        labelSourceStatus->setText(QCoreApplication::translate("TabWorkflow", "Mode: Flux Device\n"
"Device: (See Hardware Tab)\n"
"Drive: Drive 0 (3.5\")", nullptr));
        labelSourceStatus->setStyleSheet(QCoreApplication::translate("TabWorkflow", "color: green; font-weight: bold;", nullptr));
        groupDestination->setTitle(QCoreApplication::translate("TabWorkflow", "\360\237\222\276 DESTINATION (Write To)", nullptr));
        btnDestFlux->setText(QCoreApplication::translate("TabWorkflow", "\360\237\223\241 Flux Device", nullptr));
        btnDestFlux->setStyleSheet(QCoreApplication::translate("TabWorkflow", "QPushButton:checked { background-color: #2196F3; color: white; font-weight: bold; }", nullptr));
        btnDestUSB->setText(QCoreApplication::translate("TabWorkflow", "\360\237\224\214 USB Device", nullptr));
        btnDestUSB->setStyleSheet(QCoreApplication::translate("TabWorkflow", "QPushButton:checked { background-color: #2196F3; color: white; font-weight: bold; }", nullptr));
        btnDestFile->setText(QCoreApplication::translate("TabWorkflow", "\360\237\223\201 Image File", nullptr));
        btnDestFile->setStyleSheet(QCoreApplication::translate("TabWorkflow", "QPushButton:checked { background-color: #2196F3; color: white; font-weight: bold; }", nullptr));
        groupDestStatus->setTitle(QCoreApplication::translate("TabWorkflow", "Status", nullptr));
        labelDestStatus->setText(QCoreApplication::translate("TabWorkflow", "Mode: Image File\n"
"File: C:\\Images\\disk001.scp \342\234\223\n"
"Auto-increment: Enabled", nullptr));
        labelDestStatus->setStyleSheet(QCoreApplication::translate("TabWorkflow", "color: blue; font-weight: bold;", nullptr));
        groupOperation->setTitle(QCoreApplication::translate("TabWorkflow", "\342\232\231\357\270\217 OPERATION", nullptr));
        radioRead->setText(QCoreApplication::translate("TabWorkflow", "\360\237\223\226 Read", nullptr));
        radioWrite->setText(QCoreApplication::translate("TabWorkflow", "\360\237\223\235 Write", nullptr));
        radioVerify->setText(QCoreApplication::translate("TabWorkflow", "\342\234\223 Verify", nullptr));
        radioConvert->setText(QCoreApplication::translate("TabWorkflow", "\360\237\224\204 Convert", nullptr));
        btnAnalyze->setText(QCoreApplication::translate("TabWorkflow", "\360\237\223\212 Analyze...", nullptr));
        btnAnalyze->setStyleSheet(QCoreApplication::translate("TabWorkflow", "QPushButton { background-color: #9C27B0; color: white; font-weight: bold; padding: 5px 15px; }", nullptr));
        labelOperationHint->setText(QCoreApplication::translate("TabWorkflow", "\360\237\222\241 Hardware in Tab 2, Format in Tab 3", nullptr));
        labelOperationHint->setStyleSheet(QCoreApplication::translate("TabWorkflow", "color: #2196F3; font-style: italic;", nullptr));
        groupTracks->setTitle(QCoreApplication::translate("TabWorkflow", "\360\237\223\212 TRACK STATUS", nullptr));
        labelSide0->setText(QCoreApplication::translate("TabWorkflow", "Side 0", nullptr));
        labelGridSide0->setText(QCoreApplication::translate("TabWorkflow", "Track Grid\n"
"(10\303\2278 matrix)\n"
"\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234"
                        "\342\254\234", nullptr));
        labelSide1->setText(QCoreApplication::translate("TabWorkflow", "Side 1", nullptr));
        labelGridSide1->setText(QCoreApplication::translate("TabWorkflow", "Track Grid\n"
"(10\303\2278 matrix)\n"
"\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\n"
"\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234\342\254\234"
                        "\342\254\234", nullptr));
        labelLegend->setText(QCoreApplication::translate("TabWorkflow", "<b>Legend:</b>\n"
"\342\254\234 Not read\n"
"\360\237\237\246 Reading\n"
"\360\237\237\251 Good\n"
"\360\237\237\250 Warning\n"
"\360\237\237\245 Error\n"
"\360\237\237\247 Protected", nullptr));
        labelStats->setText(QCoreApplication::translate("TabWorkflow", "<b>Statistics:</b>\n"
"Tracks: 80\303\2272\n"
"Good: 0\n"
"Warnings: 0\n"
"Errors: 0", nullptr));
        groupProgress->setTitle(QCoreApplication::translate("TabWorkflow", "\360\237\223\210 PROGRESS", nullptr));
        progressBar->setFormat(QCoreApplication::translate("TabWorkflow", "%p% - Track 0/80", nullptr));
        labelTime->setText(QCoreApplication::translate("TabWorkflow", "Elapsed: 00:00:00 / Remaining: --:--:--", nullptr));
        labelSpeed->setText(QCoreApplication::translate("TabWorkflow", "300 RPM", nullptr));
        btnStartAbort->setText(QCoreApplication::translate("TabWorkflow", "\342\226\266 START", nullptr));
        btnStartAbort->setStyleSheet(QCoreApplication::translate("TabWorkflow", "QPushButton { background-color: #4CAF50; color: white; font-weight: bold; font-size: 12pt; } QPushButton:hover { background-color: #45a049; }", nullptr));
        btnPause->setText(QCoreApplication::translate("TabWorkflow", "\342\217\270 PAUSE", nullptr));
        btnPause->setStyleSheet(QCoreApplication::translate("TabWorkflow", "QPushButton { background-color: #FFC107; color: white; font-weight: bold; font-size: 12pt; }", nullptr));
        btnLog->setText(QCoreApplication::translate("TabWorkflow", "\360\237\223\213 VIEW LOG", nullptr));
        btnHistogram->setText(QCoreApplication::translate("TabWorkflow", "\360\237\223\212 Histogram", nullptr));
#if QT_CONFIG(tooltip)
        btnHistogram->setToolTip(QCoreApplication::translate("TabWorkflow", "Analyze flux timing histogram", nullptr));
#endif // QT_CONFIG(tooltip)
        (void)TabWorkflow;
    } // retranslateUi

};

namespace Ui {
    class TabWorkflow: public Ui_TabWorkflow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TAB_WORKFLOW_H

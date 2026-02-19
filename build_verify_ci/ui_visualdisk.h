/********************************************************************************
** Form generated from reading UI file 'visualdisk.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VISUALDISK_H
#define UI_VISUALDISK_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_VisualDiskWindow
{
public:
    QAction *actionDiskView;
    QAction *actionGridView;
    QAction *actionZoomIn;
    QAction *actionZoomOut;
    QAction *actionExport;
    QAction *actionRefresh;
    QVBoxLayout *verticalLayout;
    QToolBar *toolbar;
    QGroupBox *groupVisualization;
    QHBoxLayout *horizontalLayout;
    QWidget *widgetDiskView;
    QGroupBox *groupTrackDetails;
    QFormLayout *formLayout;
    QLabel *labelTrack;
    QLabel *labelTrackValue;
    QLabel *labelSide;
    QLabel *labelSideValue;
    QLabel *labelSectors;
    QLabel *labelSectorsValue;
    QLabel *labelGoodSectors;
    QLabel *labelGoodSectorsValue;
    QLabel *labelBadSectors;
    QLabel *labelBadSectorsValue;
    QLabel *labelRetries;
    QLabel *labelRetriesValue;
    QFrame *line;
    QLabel *labelFormat;
    QLabel *labelFormatValue;
    QLabel *labelRPM;
    QLabel *labelRPMValue;
    QLabel *labelEncoding;
    QLabel *labelEncodingValue;
    QGroupBox *groupLegend;
    QHBoxLayout *horizontalLayout_2;
    QLabel *labelGreenLegend;
    QLabel *labelYellowLegend;
    QLabel *labelRedLegend;
    QLabel *labelBlueLegend;
    QLabel *labelGrayLegend;
    QSpacerItem *horizontalSpacer;
    QHBoxLayout *horizontalLayout_3;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *btnClose;

    void setupUi(QDialog *VisualDiskWindow)
    {
        if (VisualDiskWindow->objectName().isEmpty())
            VisualDiskWindow->setObjectName("VisualDiskWindow");
        VisualDiskWindow->resize(1200, 700);
        actionDiskView = new QAction(VisualDiskWindow);
        actionDiskView->setObjectName("actionDiskView");
        actionDiskView->setCheckable(true);
        actionDiskView->setChecked(true);
        actionGridView = new QAction(VisualDiskWindow);
        actionGridView->setObjectName("actionGridView");
        actionGridView->setCheckable(true);
        actionZoomIn = new QAction(VisualDiskWindow);
        actionZoomIn->setObjectName("actionZoomIn");
        actionZoomOut = new QAction(VisualDiskWindow);
        actionZoomOut->setObjectName("actionZoomOut");
        actionExport = new QAction(VisualDiskWindow);
        actionExport->setObjectName("actionExport");
        actionRefresh = new QAction(VisualDiskWindow);
        actionRefresh->setObjectName("actionRefresh");
        verticalLayout = new QVBoxLayout(VisualDiskWindow);
        verticalLayout->setObjectName("verticalLayout");
        toolbar = new QToolBar(VisualDiskWindow);
        toolbar->setObjectName("toolbar");

        verticalLayout->addWidget(toolbar);

        groupVisualization = new QGroupBox(VisualDiskWindow);
        groupVisualization->setObjectName("groupVisualization");
        horizontalLayout = new QHBoxLayout(groupVisualization);
        horizontalLayout->setObjectName("horizontalLayout");
        widgetDiskView = new QWidget(groupVisualization);
        widgetDiskView->setObjectName("widgetDiskView");
        widgetDiskView->setMinimumSize(QSize(800, 500));
        widgetDiskView->setStyleSheet(QString::fromUtf8("background-color: rgb(0, 0, 0);"));

        horizontalLayout->addWidget(widgetDiskView);

        groupTrackDetails = new QGroupBox(groupVisualization);
        groupTrackDetails->setObjectName("groupTrackDetails");
        groupTrackDetails->setMinimumSize(QSize(300, 0));
        formLayout = new QFormLayout(groupTrackDetails);
        formLayout->setObjectName("formLayout");
        labelTrack = new QLabel(groupTrackDetails);
        labelTrack->setObjectName("labelTrack");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, labelTrack);

        labelTrackValue = new QLabel(groupTrackDetails);
        labelTrackValue->setObjectName("labelTrackValue");

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, labelTrackValue);

        labelSide = new QLabel(groupTrackDetails);
        labelSide->setObjectName("labelSide");

        formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, labelSide);

        labelSideValue = new QLabel(groupTrackDetails);
        labelSideValue->setObjectName("labelSideValue");

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, labelSideValue);

        labelSectors = new QLabel(groupTrackDetails);
        labelSectors->setObjectName("labelSectors");

        formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, labelSectors);

        labelSectorsValue = new QLabel(groupTrackDetails);
        labelSectorsValue->setObjectName("labelSectorsValue");

        formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, labelSectorsValue);

        labelGoodSectors = new QLabel(groupTrackDetails);
        labelGoodSectors->setObjectName("labelGoodSectors");

        formLayout->setWidget(3, QFormLayout::ItemRole::LabelRole, labelGoodSectors);

        labelGoodSectorsValue = new QLabel(groupTrackDetails);
        labelGoodSectorsValue->setObjectName("labelGoodSectorsValue");
        labelGoodSectorsValue->setStyleSheet(QString::fromUtf8("color: rgb(0, 255, 0);"));

        formLayout->setWidget(3, QFormLayout::ItemRole::FieldRole, labelGoodSectorsValue);

        labelBadSectors = new QLabel(groupTrackDetails);
        labelBadSectors->setObjectName("labelBadSectors");

        formLayout->setWidget(4, QFormLayout::ItemRole::LabelRole, labelBadSectors);

        labelBadSectorsValue = new QLabel(groupTrackDetails);
        labelBadSectorsValue->setObjectName("labelBadSectorsValue");
        labelBadSectorsValue->setStyleSheet(QString::fromUtf8("color: rgb(255, 0, 0);"));

        formLayout->setWidget(4, QFormLayout::ItemRole::FieldRole, labelBadSectorsValue);

        labelRetries = new QLabel(groupTrackDetails);
        labelRetries->setObjectName("labelRetries");

        formLayout->setWidget(5, QFormLayout::ItemRole::LabelRole, labelRetries);

        labelRetriesValue = new QLabel(groupTrackDetails);
        labelRetriesValue->setObjectName("labelRetriesValue");
        labelRetriesValue->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 0);"));

        formLayout->setWidget(5, QFormLayout::ItemRole::FieldRole, labelRetriesValue);

        line = new QFrame(groupTrackDetails);
        line->setObjectName("line");
        line->setFrameShape(QFrame::Shape::HLine);
        line->setFrameShadow(QFrame::Shadow::Sunken);

        formLayout->setWidget(6, QFormLayout::ItemRole::SpanningRole, line);

        labelFormat = new QLabel(groupTrackDetails);
        labelFormat->setObjectName("labelFormat");

        formLayout->setWidget(7, QFormLayout::ItemRole::LabelRole, labelFormat);

        labelFormatValue = new QLabel(groupTrackDetails);
        labelFormatValue->setObjectName("labelFormatValue");

        formLayout->setWidget(7, QFormLayout::ItemRole::FieldRole, labelFormatValue);

        labelRPM = new QLabel(groupTrackDetails);
        labelRPM->setObjectName("labelRPM");

        formLayout->setWidget(8, QFormLayout::ItemRole::LabelRole, labelRPM);

        labelRPMValue = new QLabel(groupTrackDetails);
        labelRPMValue->setObjectName("labelRPMValue");

        formLayout->setWidget(8, QFormLayout::ItemRole::FieldRole, labelRPMValue);

        labelEncoding = new QLabel(groupTrackDetails);
        labelEncoding->setObjectName("labelEncoding");

        formLayout->setWidget(9, QFormLayout::ItemRole::LabelRole, labelEncoding);

        labelEncodingValue = new QLabel(groupTrackDetails);
        labelEncodingValue->setObjectName("labelEncodingValue");

        formLayout->setWidget(9, QFormLayout::ItemRole::FieldRole, labelEncodingValue);


        horizontalLayout->addWidget(groupTrackDetails);


        verticalLayout->addWidget(groupVisualization);

        groupLegend = new QGroupBox(VisualDiskWindow);
        groupLegend->setObjectName("groupLegend");
        groupLegend->setMaximumSize(QSize(16777215, 80));
        horizontalLayout_2 = new QHBoxLayout(groupLegend);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        labelGreenLegend = new QLabel(groupLegend);
        labelGreenLegend->setObjectName("labelGreenLegend");
        labelGreenLegend->setStyleSheet(QString::fromUtf8("color: rgb(0, 255, 0);"));

        horizontalLayout_2->addWidget(labelGreenLegend);

        labelYellowLegend = new QLabel(groupLegend);
        labelYellowLegend->setObjectName("labelYellowLegend");
        labelYellowLegend->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 0);"));

        horizontalLayout_2->addWidget(labelYellowLegend);

        labelRedLegend = new QLabel(groupLegend);
        labelRedLegend->setObjectName("labelRedLegend");
        labelRedLegend->setStyleSheet(QString::fromUtf8("color: rgb(255, 0, 0);"));

        horizontalLayout_2->addWidget(labelRedLegend);

        labelBlueLegend = new QLabel(groupLegend);
        labelBlueLegend->setObjectName("labelBlueLegend");
        labelBlueLegend->setStyleSheet(QString::fromUtf8("color: rgb(85, 170, 255);"));

        horizontalLayout_2->addWidget(labelBlueLegend);

        labelGrayLegend = new QLabel(groupLegend);
        labelGrayLegend->setObjectName("labelGrayLegend");
        labelGrayLegend->setStyleSheet(QString::fromUtf8("color: rgb(128, 128, 128);"));

        horizontalLayout_2->addWidget(labelGrayLegend);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);


        verticalLayout->addWidget(groupLegend);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_2);

        btnClose = new QPushButton(VisualDiskWindow);
        btnClose->setObjectName("btnClose");

        horizontalLayout_3->addWidget(btnClose);


        verticalLayout->addLayout(horizontalLayout_3);


        toolbar->addAction(actionDiskView);
        toolbar->addAction(actionGridView);
        toolbar->addSeparator();
        toolbar->addAction(actionZoomIn);
        toolbar->addAction(actionZoomOut);
        toolbar->addSeparator();
        toolbar->addAction(actionExport);
        toolbar->addAction(actionRefresh);

        retranslateUi(VisualDiskWindow);
        QObject::connect(btnClose, &QPushButton::clicked, VisualDiskWindow, qOverload<>(&QDialog::close));

        QMetaObject::connectSlotsByName(VisualDiskWindow);
    } // setupUi

    void retranslateUi(QDialog *VisualDiskWindow)
    {
        VisualDiskWindow->setWindowTitle(QCoreApplication::translate("VisualDiskWindow", "Visual Disk - Track Visualization", nullptr));
        actionDiskView->setText(QCoreApplication::translate("VisualDiskWindow", "Disk View", nullptr));
        actionGridView->setText(QCoreApplication::translate("VisualDiskWindow", "Grid View", nullptr));
        actionZoomIn->setText(QCoreApplication::translate("VisualDiskWindow", "Zoom In", nullptr));
        actionZoomOut->setText(QCoreApplication::translate("VisualDiskWindow", "Zoom Out", nullptr));
        actionExport->setText(QCoreApplication::translate("VisualDiskWindow", "Export", nullptr));
        actionRefresh->setText(QCoreApplication::translate("VisualDiskWindow", "Refresh", nullptr));
        toolbar->setWindowTitle(QCoreApplication::translate("VisualDiskWindow", "toolBar", nullptr));
        groupVisualization->setTitle(QCoreApplication::translate("VisualDiskWindow", "Disk Visualization", nullptr));
        groupTrackDetails->setTitle(QCoreApplication::translate("VisualDiskWindow", "Track Details", nullptr));
        labelTrack->setText(QCoreApplication::translate("VisualDiskWindow", "Track:", nullptr));
        labelTrackValue->setText(QCoreApplication::translate("VisualDiskWindow", "-", nullptr));
        labelSide->setText(QCoreApplication::translate("VisualDiskWindow", "Side:", nullptr));
        labelSideValue->setText(QCoreApplication::translate("VisualDiskWindow", "-", nullptr));
        labelSectors->setText(QCoreApplication::translate("VisualDiskWindow", "Sectors:", nullptr));
        labelSectorsValue->setText(QCoreApplication::translate("VisualDiskWindow", "-", nullptr));
        labelGoodSectors->setText(QCoreApplication::translate("VisualDiskWindow", "Good:", nullptr));
        labelGoodSectorsValue->setText(QCoreApplication::translate("VisualDiskWindow", "-", nullptr));
        labelBadSectors->setText(QCoreApplication::translate("VisualDiskWindow", "Bad:", nullptr));
        labelBadSectorsValue->setText(QCoreApplication::translate("VisualDiskWindow", "-", nullptr));
        labelRetries->setText(QCoreApplication::translate("VisualDiskWindow", "Retries:", nullptr));
        labelRetriesValue->setText(QCoreApplication::translate("VisualDiskWindow", "-", nullptr));
        labelFormat->setText(QCoreApplication::translate("VisualDiskWindow", "Format:", nullptr));
        labelFormatValue->setText(QCoreApplication::translate("VisualDiskWindow", "-", nullptr));
        labelRPM->setText(QCoreApplication::translate("VisualDiskWindow", "RPM:", nullptr));
        labelRPMValue->setText(QCoreApplication::translate("VisualDiskWindow", "-", nullptr));
        labelEncoding->setText(QCoreApplication::translate("VisualDiskWindow", "Encoding:", nullptr));
        labelEncodingValue->setText(QCoreApplication::translate("VisualDiskWindow", "-", nullptr));
        groupLegend->setTitle(QCoreApplication::translate("VisualDiskWindow", "Legend", nullptr));
        labelGreenLegend->setText(QCoreApplication::translate("VisualDiskWindow", "\342\227\217 Green = Good", nullptr));
        labelYellowLegend->setText(QCoreApplication::translate("VisualDiskWindow", "\342\227\217 Yellow = Retry", nullptr));
        labelRedLegend->setText(QCoreApplication::translate("VisualDiskWindow", "\342\227\217 Red = Bad", nullptr));
        labelBlueLegend->setText(QCoreApplication::translate("VisualDiskWindow", "\342\227\217 Blue = Protection", nullptr));
        labelGrayLegend->setText(QCoreApplication::translate("VisualDiskWindow", "\342\227\217 Gray = Unread", nullptr));
        btnClose->setText(QCoreApplication::translate("VisualDiskWindow", "Close", nullptr));
    } // retranslateUi

};

namespace Ui {
    class VisualDiskWindow: public Ui_VisualDiskWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VISUALDISK_H

/********************************************************************************
** Form generated from reading UI file 'tab_status.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TAB_STATUS_H
#define UI_TAB_STATUS_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TabStatus
{
public:
    QVBoxLayout *verticalLayout_main;
    QGroupBox *groupStatus;
    QVBoxLayout *vboxLayout;
    QHBoxLayout *hboxLayout;
    QPushButton *btnLabelEditor;
    QPushButton *btnBAMViewer;
    QPushButton *btnBootblock;
    QPushButton *btnProtection;
    QPushButton *btnDmkAnalysis;
    QProgressBar *progressTrack;
    QProgressBar *progressTotal;
    QLabel *labelTrackSide;
    QTextEdit *textSectorInfo;
    QTextEdit *textHexDump;
    QHBoxLayout *hboxLayout1;
    QSlider *sliderHexScroll;

    void setupUi(QWidget *TabStatus)
    {
        if (TabStatus->objectName().isEmpty())
            TabStatus->setObjectName("TabStatus");
        TabStatus->resize(400, 500);
        verticalLayout_main = new QVBoxLayout(TabStatus);
        verticalLayout_main->setSpacing(4);
        verticalLayout_main->setObjectName("verticalLayout_main");
        verticalLayout_main->setContentsMargins(4, 4, 4, 4);
        groupStatus = new QGroupBox(TabStatus);
        groupStatus->setObjectName("groupStatus");
        vboxLayout = new QVBoxLayout(groupStatus);
        vboxLayout->setSpacing(4);
        vboxLayout->setObjectName("vboxLayout");
        hboxLayout = new QHBoxLayout();
        hboxLayout->setObjectName("hboxLayout");
        btnLabelEditor = new QPushButton(groupStatus);
        btnLabelEditor->setObjectName("btnLabelEditor");
        btnLabelEditor->setEnabled(false);

        hboxLayout->addWidget(btnLabelEditor);

        btnBAMViewer = new QPushButton(groupStatus);
        btnBAMViewer->setObjectName("btnBAMViewer");
        btnBAMViewer->setEnabled(false);

        hboxLayout->addWidget(btnBAMViewer);

        btnBootblock = new QPushButton(groupStatus);
        btnBootblock->setObjectName("btnBootblock");
        btnBootblock->setEnabled(false);

        hboxLayout->addWidget(btnBootblock);

        btnProtection = new QPushButton(groupStatus);
        btnProtection->setObjectName("btnProtection");
        btnProtection->setEnabled(false);

        hboxLayout->addWidget(btnProtection);

        btnDmkAnalysis = new QPushButton(groupStatus);
        btnDmkAnalysis->setObjectName("btnDmkAnalysis");

        hboxLayout->addWidget(btnDmkAnalysis);


        vboxLayout->addLayout(hboxLayout);

        progressTrack = new QProgressBar(groupStatus);
        progressTrack->setObjectName("progressTrack");
        progressTrack->setMaximum(100);
        progressTrack->setValue(0);
        progressTrack->setTextVisible(false);
        progressTrack->setMaximumHeight(8);

        vboxLayout->addWidget(progressTrack);

        progressTotal = new QProgressBar(groupStatus);
        progressTotal->setObjectName("progressTotal");
        progressTotal->setMaximum(100);
        progressTotal->setValue(0);
        progressTotal->setTextVisible(false);
        progressTotal->setMaximumHeight(8);

        vboxLayout->addWidget(progressTotal);

        labelTrackSide = new QLabel(groupStatus);
        labelTrackSide->setObjectName("labelTrackSide");

        vboxLayout->addWidget(labelTrackSide);

        textSectorInfo = new QTextEdit(groupStatus);
        textSectorInfo->setObjectName("textSectorInfo");
        textSectorInfo->setReadOnly(true);
        textSectorInfo->setMinimumHeight(160);

        vboxLayout->addWidget(textSectorInfo);

        textHexDump = new QTextEdit(groupStatus);
        textHexDump->setObjectName("textHexDump");
        textHexDump->setReadOnly(true);
        textHexDump->setMinimumHeight(180);

        vboxLayout->addWidget(textHexDump);

        hboxLayout1 = new QHBoxLayout();
        hboxLayout1->setObjectName("hboxLayout1");
        sliderHexScroll = new QSlider(groupStatus);
        sliderHexScroll->setObjectName("sliderHexScroll");
        sliderHexScroll->setOrientation(Qt::Horizontal);
        sliderHexScroll->setMinimum(0);
        sliderHexScroll->setMaximum(100);

        hboxLayout1->addWidget(sliderHexScroll);


        vboxLayout->addLayout(hboxLayout1);


        verticalLayout_main->addWidget(groupStatus);


        retranslateUi(TabStatus);

        QMetaObject::connectSlotsByName(TabStatus);
    } // setupUi

    void retranslateUi(QWidget *TabStatus)
    {
        groupStatus->setTitle(QCoreApplication::translate("TabStatus", "Status", nullptr));
        groupStatus->setStyleSheet(QCoreApplication::translate("TabStatus", "QGroupBox { font-weight: bold; }", nullptr));
        btnLabelEditor->setText(QCoreApplication::translate("TabStatus", "\360\237\217\267\357\270\217 Label Editor", nullptr));
#if QT_CONFIG(tooltip)
        btnLabelEditor->setToolTip(QCoreApplication::translate("TabStatus", "Edit disk name and ID", nullptr));
#endif // QT_CONFIG(tooltip)
        btnBAMViewer->setText(QCoreApplication::translate("TabStatus", "\360\237\223\212 BAM/FAT", nullptr));
#if QT_CONFIG(tooltip)
        btnBAMViewer->setToolTip(QCoreApplication::translate("TabStatus", "View Block Allocation Map / File Allocation Table", nullptr));
#endif // QT_CONFIG(tooltip)
        btnBootblock->setText(QCoreApplication::translate("TabStatus", "\360\237\245\276 Bootblock", nullptr));
#if QT_CONFIG(tooltip)
        btnBootblock->setToolTip(QCoreApplication::translate("TabStatus", "View and edit boot sector", nullptr));
#endif // QT_CONFIG(tooltip)
        btnProtection->setText(QCoreApplication::translate("TabStatus", "\360\237\233\241\357\270\217 Protection", nullptr));
#if QT_CONFIG(tooltip)
        btnProtection->setToolTip(QCoreApplication::translate("TabStatus", "Analyze copy protection", nullptr));
#endif // QT_CONFIG(tooltip)
        btnDmkAnalysis->setText(QCoreApplication::translate("TabStatus", "\360\237\224\216 DMK Analysis", nullptr));
#if QT_CONFIG(tooltip)
        btnDmkAnalysis->setToolTip(QCoreApplication::translate("TabStatus", "Analyze DMK disk image structure (TRS-80)", nullptr));
#endif // QT_CONFIG(tooltip)
        labelTrackSide->setText(QCoreApplication::translate("TabStatus", "Track : 0 Side : 0", nullptr));
        labelTrackSide->setStyleSheet(QCoreApplication::translate("TabStatus", "font-family: monospace; font-size: 10pt; background-color: #f0f0f0; padding: 4px; border: 1px solid #ccc;", nullptr));
        textSectorInfo->setStyleSheet(QCoreApplication::translate("TabStatus", "font-family: monospace; font-size: 9pt; background-color: #f8f8f8;", nullptr));
        textSectorInfo->setPlainText(QString());
        textHexDump->setStyleSheet(QCoreApplication::translate("TabStatus", "font-family: monospace; font-size: 8pt; background-color: #1a1a1a; color: #00ff00;", nullptr));
        textHexDump->setPlainText(QString());
        (void)TabStatus;
    } // retranslateUi

};

namespace Ui {
    class TabStatus: public Ui_TabStatus {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TAB_STATUS_H

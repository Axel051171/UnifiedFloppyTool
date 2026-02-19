/********************************************************************************
** Form generated from reading UI file 'tab_xcopy.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TAB_XCOPY_H
#define UI_TAB_XCOPY_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TabXCopy
{
public:
    QHBoxLayout *horizontalLayout_main;
    QVBoxLayout *verticalLayout_left;
    QGroupBox *groupSource;
    QFormLayout *formLayout_source;
    QLabel *labelSourceType;
    QComboBox *comboSourceType;
    QLabel *labelSourceDrive;
    QComboBox *comboSourceDrive;
    QLabel *labelSourceFile;
    QHBoxLayout *hboxLayout;
    QLineEdit *editSourceFile;
    QPushButton *btnBrowseSource;
    QGroupBox *groupDest;
    QFormLayout *formLayout_dest;
    QLabel *labelDestType;
    QComboBox *comboDestType;
    QLabel *labelDestDrive;
    QComboBox *comboDestDrive;
    QLabel *labelDestFile;
    QHBoxLayout *hboxLayout1;
    QLineEdit *editDestFile;
    QPushButton *btnBrowseDest;
    QGroupBox *groupRange;
    QFormLayout *formLayout_range;
    QLabel *labelStartTrack;
    QSpinBox *spinStartTrack;
    QLabel *labelEndTrack;
    QSpinBox *spinEndTrack;
    QLabel *labelSides;
    QComboBox *comboSides;
    QCheckBox *checkAllTracks;
    QSpacerItem *spacerItem;
    QVBoxLayout *verticalLayout_right;
    QGroupBox *groupMode;
    QFormLayout *formLayout_mode;
    QLabel *labelCopyMode;
    QComboBox *comboCopyMode;
    QCheckBox *checkVerify;
    QLabel *labelVerifyRetries;
    QSpinBox *spinVerifyRetries;
    QGroupBox *groupErrors;
    QVBoxLayout *verticalLayout_errors;
    QCheckBox *checkIgnoreErrors;
    QCheckBox *checkRetryErrors;
    QHBoxLayout *hboxLayout2;
    QLabel *labelMaxRetries;
    QSpinBox *spinMaxRetries;
    QCheckBox *checkSkipBad;
    QCheckBox *checkFillBad;
    QSpinBox *spinFillByte;
    QGroupBox *groupMultiple;
    QFormLayout *formLayout_multiple;
    QLabel *labelNumCopies;
    QSpinBox *spinNumCopies;
    QCheckBox *checkAutoEject;
    QCheckBox *checkWaitForDisk;
    QSpacerItem *spacerItem1;
    QHBoxLayout *hboxLayout3;
    QPushButton *btnStartCopy;
    QPushButton *btnStopCopy;

    void setupUi(QWidget *TabXCopy)
    {
        if (TabXCopy->objectName().isEmpty())
            TabXCopy->setObjectName("TabXCopy");
        TabXCopy->resize(900, 700);
        horizontalLayout_main = new QHBoxLayout(TabXCopy);
        horizontalLayout_main->setObjectName("horizontalLayout_main");
        verticalLayout_left = new QVBoxLayout();
        verticalLayout_left->setObjectName("verticalLayout_left");
        groupSource = new QGroupBox(TabXCopy);
        groupSource->setObjectName("groupSource");
        formLayout_source = new QFormLayout(groupSource);
        formLayout_source->setObjectName("formLayout_source");
        labelSourceType = new QLabel(groupSource);
        labelSourceType->setObjectName("labelSourceType");

        formLayout_source->setWidget(0, QFormLayout::ItemRole::LabelRole, labelSourceType);

        comboSourceType = new QComboBox(groupSource);
        comboSourceType->addItem(QString());
        comboSourceType->addItem(QString());
        comboSourceType->setObjectName("comboSourceType");
        comboSourceType->setMaximumWidth(200);

        formLayout_source->setWidget(0, QFormLayout::ItemRole::FieldRole, comboSourceType);

        labelSourceDrive = new QLabel(groupSource);
        labelSourceDrive->setObjectName("labelSourceDrive");

        formLayout_source->setWidget(1, QFormLayout::ItemRole::LabelRole, labelSourceDrive);

        comboSourceDrive = new QComboBox(groupSource);
        comboSourceDrive->addItem(QString());
        comboSourceDrive->addItem(QString());
        comboSourceDrive->setObjectName("comboSourceDrive");
        comboSourceDrive->setMaximumWidth(200);

        formLayout_source->setWidget(1, QFormLayout::ItemRole::FieldRole, comboSourceDrive);

        labelSourceFile = new QLabel(groupSource);
        labelSourceFile->setObjectName("labelSourceFile");

        formLayout_source->setWidget(2, QFormLayout::ItemRole::LabelRole, labelSourceFile);

        hboxLayout = new QHBoxLayout();
        hboxLayout->setObjectName("hboxLayout");
        editSourceFile = new QLineEdit(groupSource);
        editSourceFile->setObjectName("editSourceFile");

        hboxLayout->addWidget(editSourceFile);

        btnBrowseSource = new QPushButton(groupSource);
        btnBrowseSource->setObjectName("btnBrowseSource");
        btnBrowseSource->setMaximumWidth(30);

        hboxLayout->addWidget(btnBrowseSource);


        formLayout_source->setLayout(2, QFormLayout::ItemRole::FieldRole, hboxLayout);


        verticalLayout_left->addWidget(groupSource);

        groupDest = new QGroupBox(TabXCopy);
        groupDest->setObjectName("groupDest");
        formLayout_dest = new QFormLayout(groupDest);
        formLayout_dest->setObjectName("formLayout_dest");
        labelDestType = new QLabel(groupDest);
        labelDestType->setObjectName("labelDestType");

        formLayout_dest->setWidget(0, QFormLayout::ItemRole::LabelRole, labelDestType);

        comboDestType = new QComboBox(groupDest);
        comboDestType->addItem(QString());
        comboDestType->addItem(QString());
        comboDestType->setObjectName("comboDestType");
        comboDestType->setMaximumWidth(200);

        formLayout_dest->setWidget(0, QFormLayout::ItemRole::FieldRole, comboDestType);

        labelDestDrive = new QLabel(groupDest);
        labelDestDrive->setObjectName("labelDestDrive");

        formLayout_dest->setWidget(1, QFormLayout::ItemRole::LabelRole, labelDestDrive);

        comboDestDrive = new QComboBox(groupDest);
        comboDestDrive->addItem(QString());
        comboDestDrive->addItem(QString());
        comboDestDrive->setObjectName("comboDestDrive");
        comboDestDrive->setMaximumWidth(200);

        formLayout_dest->setWidget(1, QFormLayout::ItemRole::FieldRole, comboDestDrive);

        labelDestFile = new QLabel(groupDest);
        labelDestFile->setObjectName("labelDestFile");

        formLayout_dest->setWidget(2, QFormLayout::ItemRole::LabelRole, labelDestFile);

        hboxLayout1 = new QHBoxLayout();
        hboxLayout1->setObjectName("hboxLayout1");
        editDestFile = new QLineEdit(groupDest);
        editDestFile->setObjectName("editDestFile");

        hboxLayout1->addWidget(editDestFile);

        btnBrowseDest = new QPushButton(groupDest);
        btnBrowseDest->setObjectName("btnBrowseDest");
        btnBrowseDest->setMaximumWidth(30);

        hboxLayout1->addWidget(btnBrowseDest);


        formLayout_dest->setLayout(2, QFormLayout::ItemRole::FieldRole, hboxLayout1);


        verticalLayout_left->addWidget(groupDest);

        groupRange = new QGroupBox(TabXCopy);
        groupRange->setObjectName("groupRange");
        formLayout_range = new QFormLayout(groupRange);
        formLayout_range->setObjectName("formLayout_range");
        labelStartTrack = new QLabel(groupRange);
        labelStartTrack->setObjectName("labelStartTrack");

        formLayout_range->setWidget(0, QFormLayout::ItemRole::LabelRole, labelStartTrack);

        spinStartTrack = new QSpinBox(groupRange);
        spinStartTrack->setObjectName("spinStartTrack");
        spinStartTrack->setMaximum(255);
        spinStartTrack->setValue(0);
        spinStartTrack->setMaximumWidth(80);

        formLayout_range->setWidget(0, QFormLayout::ItemRole::FieldRole, spinStartTrack);

        labelEndTrack = new QLabel(groupRange);
        labelEndTrack->setObjectName("labelEndTrack");

        formLayout_range->setWidget(1, QFormLayout::ItemRole::LabelRole, labelEndTrack);

        spinEndTrack = new QSpinBox(groupRange);
        spinEndTrack->setObjectName("spinEndTrack");
        spinEndTrack->setMaximum(255);
        spinEndTrack->setValue(79);
        spinEndTrack->setMaximumWidth(80);

        formLayout_range->setWidget(1, QFormLayout::ItemRole::FieldRole, spinEndTrack);

        labelSides = new QLabel(groupRange);
        labelSides->setObjectName("labelSides");

        formLayout_range->setWidget(2, QFormLayout::ItemRole::LabelRole, labelSides);

        comboSides = new QComboBox(groupRange);
        comboSides->addItem(QString());
        comboSides->addItem(QString());
        comboSides->addItem(QString());
        comboSides->setObjectName("comboSides");
        comboSides->setMaximumWidth(150);

        formLayout_range->setWidget(2, QFormLayout::ItemRole::FieldRole, comboSides);

        checkAllTracks = new QCheckBox(groupRange);
        checkAllTracks->setObjectName("checkAllTracks");
        checkAllTracks->setChecked(true);

        formLayout_range->setWidget(3, QFormLayout::ItemRole::SpanningRole, checkAllTracks);


        verticalLayout_left->addWidget(groupRange);

        spacerItem = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_left->addItem(spacerItem);


        horizontalLayout_main->addLayout(verticalLayout_left);

        verticalLayout_right = new QVBoxLayout();
        verticalLayout_right->setObjectName("verticalLayout_right");
        groupMode = new QGroupBox(TabXCopy);
        groupMode->setObjectName("groupMode");
        formLayout_mode = new QFormLayout(groupMode);
        formLayout_mode->setObjectName("formLayout_mode");
        labelCopyMode = new QLabel(groupMode);
        labelCopyMode->setObjectName("labelCopyMode");

        formLayout_mode->setWidget(0, QFormLayout::ItemRole::LabelRole, labelCopyMode);

        comboCopyMode = new QComboBox(groupMode);
        comboCopyMode->addItem(QString());
        comboCopyMode->addItem(QString());
        comboCopyMode->addItem(QString());
        comboCopyMode->addItem(QString());
        comboCopyMode->setObjectName("comboCopyMode");
        comboCopyMode->setMaximumWidth(200);

        formLayout_mode->setWidget(0, QFormLayout::ItemRole::FieldRole, comboCopyMode);

        checkVerify = new QCheckBox(groupMode);
        checkVerify->setObjectName("checkVerify");
        checkVerify->setChecked(true);

        formLayout_mode->setWidget(1, QFormLayout::ItemRole::SpanningRole, checkVerify);

        labelVerifyRetries = new QLabel(groupMode);
        labelVerifyRetries->setObjectName("labelVerifyRetries");

        formLayout_mode->setWidget(2, QFormLayout::ItemRole::LabelRole, labelVerifyRetries);

        spinVerifyRetries = new QSpinBox(groupMode);
        spinVerifyRetries->setObjectName("spinVerifyRetries");
        spinVerifyRetries->setMaximum(10);
        spinVerifyRetries->setValue(3);
        spinVerifyRetries->setMaximumWidth(80);

        formLayout_mode->setWidget(2, QFormLayout::ItemRole::FieldRole, spinVerifyRetries);


        verticalLayout_right->addWidget(groupMode);

        groupErrors = new QGroupBox(TabXCopy);
        groupErrors->setObjectName("groupErrors");
        verticalLayout_errors = new QVBoxLayout(groupErrors);
        verticalLayout_errors->setObjectName("verticalLayout_errors");
        checkIgnoreErrors = new QCheckBox(groupErrors);
        checkIgnoreErrors->setObjectName("checkIgnoreErrors");

        verticalLayout_errors->addWidget(checkIgnoreErrors);

        checkRetryErrors = new QCheckBox(groupErrors);
        checkRetryErrors->setObjectName("checkRetryErrors");
        checkRetryErrors->setChecked(true);

        verticalLayout_errors->addWidget(checkRetryErrors);

        hboxLayout2 = new QHBoxLayout();
        hboxLayout2->setObjectName("hboxLayout2");
        labelMaxRetries = new QLabel(groupErrors);
        labelMaxRetries->setObjectName("labelMaxRetries");

        hboxLayout2->addWidget(labelMaxRetries);

        spinMaxRetries = new QSpinBox(groupErrors);
        spinMaxRetries->setObjectName("spinMaxRetries");
        spinMaxRetries->setMaximum(50);
        spinMaxRetries->setValue(5);
        spinMaxRetries->setMaximumWidth(80);

        hboxLayout2->addWidget(spinMaxRetries);


        verticalLayout_errors->addLayout(hboxLayout2);

        checkSkipBad = new QCheckBox(groupErrors);
        checkSkipBad->setObjectName("checkSkipBad");
        checkSkipBad->setChecked(true);

        verticalLayout_errors->addWidget(checkSkipBad);

        checkFillBad = new QCheckBox(groupErrors);
        checkFillBad->setObjectName("checkFillBad");

        verticalLayout_errors->addWidget(checkFillBad);

        spinFillByte = new QSpinBox(groupErrors);
        spinFillByte->setObjectName("spinFillByte");
        spinFillByte->setDisplayIntegerBase(16);
        spinFillByte->setMaximum(255);
        spinFillByte->setValue(0);
        spinFillByte->setMaximumWidth(80);

        verticalLayout_errors->addWidget(spinFillByte);


        verticalLayout_right->addWidget(groupErrors);

        groupMultiple = new QGroupBox(TabXCopy);
        groupMultiple->setObjectName("groupMultiple");
        formLayout_multiple = new QFormLayout(groupMultiple);
        formLayout_multiple->setObjectName("formLayout_multiple");
        labelNumCopies = new QLabel(groupMultiple);
        labelNumCopies->setObjectName("labelNumCopies");

        formLayout_multiple->setWidget(0, QFormLayout::ItemRole::LabelRole, labelNumCopies);

        spinNumCopies = new QSpinBox(groupMultiple);
        spinNumCopies->setObjectName("spinNumCopies");
        spinNumCopies->setMinimum(1);
        spinNumCopies->setMaximum(100);
        spinNumCopies->setValue(1);
        spinNumCopies->setMaximumWidth(80);

        formLayout_multiple->setWidget(0, QFormLayout::ItemRole::FieldRole, spinNumCopies);

        checkAutoEject = new QCheckBox(groupMultiple);
        checkAutoEject->setObjectName("checkAutoEject");

        formLayout_multiple->setWidget(1, QFormLayout::ItemRole::SpanningRole, checkAutoEject);

        checkWaitForDisk = new QCheckBox(groupMultiple);
        checkWaitForDisk->setObjectName("checkWaitForDisk");
        checkWaitForDisk->setChecked(true);

        formLayout_multiple->setWidget(2, QFormLayout::ItemRole::SpanningRole, checkWaitForDisk);


        verticalLayout_right->addWidget(groupMultiple);

        spacerItem1 = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_right->addItem(spacerItem1);

        hboxLayout3 = new QHBoxLayout();
        hboxLayout3->setObjectName("hboxLayout3");
        btnStartCopy = new QPushButton(TabXCopy);
        btnStartCopy->setObjectName("btnStartCopy");

        hboxLayout3->addWidget(btnStartCopy);

        btnStopCopy = new QPushButton(TabXCopy);
        btnStopCopy->setObjectName("btnStopCopy");
        btnStopCopy->setEnabled(false);

        hboxLayout3->addWidget(btnStopCopy);


        verticalLayout_right->addLayout(hboxLayout3);


        horizontalLayout_main->addLayout(verticalLayout_right);


        retranslateUi(TabXCopy);

        QMetaObject::connectSlotsByName(TabXCopy);
    } // setupUi

    void retranslateUi(QWidget *TabXCopy)
    {
        groupSource->setTitle(QCoreApplication::translate("TabXCopy", "Source", nullptr));
        labelSourceType->setText(QCoreApplication::translate("TabXCopy", "Type:", nullptr));
        comboSourceType->setItemText(0, QCoreApplication::translate("TabXCopy", "Drive", nullptr));
        comboSourceType->setItemText(1, QCoreApplication::translate("TabXCopy", "Image File", nullptr));

        labelSourceDrive->setText(QCoreApplication::translate("TabXCopy", "Drive:", nullptr));
        comboSourceDrive->setItemText(0, QCoreApplication::translate("TabXCopy", "A: (Drive 0)", nullptr));
        comboSourceDrive->setItemText(1, QCoreApplication::translate("TabXCopy", "B: (Drive 1)", nullptr));

        labelSourceFile->setText(QCoreApplication::translate("TabXCopy", "File:", nullptr));
        editSourceFile->setPlaceholderText(QCoreApplication::translate("TabXCopy", "Select source image...", nullptr));
        btnBrowseSource->setText(QCoreApplication::translate("TabXCopy", "...", nullptr));
        groupDest->setTitle(QCoreApplication::translate("TabXCopy", "Destination", nullptr));
        labelDestType->setText(QCoreApplication::translate("TabXCopy", "Type:", nullptr));
        comboDestType->setItemText(0, QCoreApplication::translate("TabXCopy", "Image File", nullptr));
        comboDestType->setItemText(1, QCoreApplication::translate("TabXCopy", "Drive", nullptr));

        labelDestDrive->setText(QCoreApplication::translate("TabXCopy", "Drive:", nullptr));
        comboDestDrive->setItemText(0, QCoreApplication::translate("TabXCopy", "A: (Drive 0)", nullptr));
        comboDestDrive->setItemText(1, QCoreApplication::translate("TabXCopy", "B: (Drive 1)", nullptr));

        labelDestFile->setText(QCoreApplication::translate("TabXCopy", "File:", nullptr));
        editDestFile->setPlaceholderText(QCoreApplication::translate("TabXCopy", "Select destination...", nullptr));
        btnBrowseDest->setText(QCoreApplication::translate("TabXCopy", "...", nullptr));
        groupRange->setTitle(QCoreApplication::translate("TabXCopy", "Track Range", nullptr));
        labelStartTrack->setText(QCoreApplication::translate("TabXCopy", "Start Track:", nullptr));
        labelEndTrack->setText(QCoreApplication::translate("TabXCopy", "End Track:", nullptr));
        labelSides->setText(QCoreApplication::translate("TabXCopy", "Sides:", nullptr));
        comboSides->setItemText(0, QCoreApplication::translate("TabXCopy", "Both (0+1)", nullptr));
        comboSides->setItemText(1, QCoreApplication::translate("TabXCopy", "Side 0 only", nullptr));
        comboSides->setItemText(2, QCoreApplication::translate("TabXCopy", "Side 1 only", nullptr));

        checkAllTracks->setText(QCoreApplication::translate("TabXCopy", "Copy all tracks", nullptr));
        groupMode->setTitle(QCoreApplication::translate("TabXCopy", "Copy Mode", nullptr));
        labelCopyMode->setText(QCoreApplication::translate("TabXCopy", "Mode:", nullptr));
        comboCopyMode->setItemText(0, QCoreApplication::translate("TabXCopy", "Sector Copy", nullptr));
        comboCopyMode->setItemText(1, QCoreApplication::translate("TabXCopy", "Track Copy", nullptr));
        comboCopyMode->setItemText(2, QCoreApplication::translate("TabXCopy", "Flux Copy", nullptr));
        comboCopyMode->setItemText(3, QCoreApplication::translate("TabXCopy", "Nibble Copy", nullptr));

        checkVerify->setText(QCoreApplication::translate("TabXCopy", "Verify after write", nullptr));
        labelVerifyRetries->setText(QCoreApplication::translate("TabXCopy", "Verify retries:", nullptr));
        groupErrors->setTitle(QCoreApplication::translate("TabXCopy", "Error Handling", nullptr));
        checkIgnoreErrors->setText(QCoreApplication::translate("TabXCopy", "Ignore errors", nullptr));
        checkRetryErrors->setText(QCoreApplication::translate("TabXCopy", "Retry on error", nullptr));
        labelMaxRetries->setText(QCoreApplication::translate("TabXCopy", "Max retries:", nullptr));
        checkSkipBad->setText(QCoreApplication::translate("TabXCopy", "Skip bad sectors", nullptr));
        checkFillBad->setText(QCoreApplication::translate("TabXCopy", "Fill bad sectors with:", nullptr));
        spinFillByte->setPrefix(QCoreApplication::translate("TabXCopy", "0x", nullptr));
        groupMultiple->setTitle(QCoreApplication::translate("TabXCopy", "Multiple Copies", nullptr));
        labelNumCopies->setText(QCoreApplication::translate("TabXCopy", "Number of copies:", nullptr));
        checkAutoEject->setText(QCoreApplication::translate("TabXCopy", "Auto-eject after copy", nullptr));
        checkWaitForDisk->setText(QCoreApplication::translate("TabXCopy", "Wait for next disk", nullptr));
        btnStartCopy->setText(QCoreApplication::translate("TabXCopy", "\342\226\266 Start Copy", nullptr));
        btnStartCopy->setStyleSheet(QCoreApplication::translate("TabXCopy", "background-color: #4CAF50; color: white; font-weight: bold; padding: 10px;", nullptr));
        btnStopCopy->setText(QCoreApplication::translate("TabXCopy", "\342\217\271 Stop", nullptr));
        (void)TabXCopy;
    } // retranslateUi

};

namespace Ui {
    class TabXCopy: public Ui_TabXCopy {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TAB_XCOPY_H

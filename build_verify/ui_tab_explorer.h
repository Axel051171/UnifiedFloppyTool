/********************************************************************************
** Form generated from reading UI file 'tab_explorer.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TAB_EXPLORER_H
#define UI_TAB_EXPLORER_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TabExplorer
{
public:
    QVBoxLayout *verticalLayout_main;
    QGroupBox *groupImage;
    QHBoxLayout *hboxLayout;
    QComboBox *comboRecentImages;
    QPushButton *btnBrowseImage;
    QPushButton *btnOpenImage;
    QPushButton *btnCloseImage;
    QSpacerItem *spacerItem;
    QLabel *labelImageInfo;
    QHBoxLayout *mainContent;
    QGroupBox *groupFiles;
    QVBoxLayout *vboxLayout;
    QHBoxLayout *hboxLayout1;
    QPushButton *btnUp;
    QLineEdit *editPath;
    QPushButton *btnRefresh;
    QTableWidget *tableFiles;
    QLabel *labelSelection;
    QVBoxLayout *actionsLayout;
    QGroupBox *groupExtract;
    QVBoxLayout *vboxLayout1;
    QPushButton *btnExtractSelected;
    QPushButton *btnExtractAll;
    QHBoxLayout *hboxLayout2;
    QLineEdit *editExtractPath;
    QPushButton *btnBrowseExtract;
    QGroupBox *groupImport;
    QVBoxLayout *vboxLayout2;
    QPushButton *btnImportFiles;
    QPushButton *btnImportFolder;
    QGroupBox *groupFileOps;
    QVBoxLayout *vboxLayout3;
    QPushButton *btnRename;
    QPushButton *btnDelete;
    QPushButton *btnNewFolder;
    QGroupBox *groupDiskOps;
    QVBoxLayout *vboxLayout4;
    QPushButton *btnNewDisk;
    QPushButton *btnValidate;
    QSpacerItem *spacerItem1;
    QGroupBox *groupStatus;
    QVBoxLayout *vboxLayout5;
    QProgressBar *progressBar;
    QLabel *labelStatus;

    void setupUi(QWidget *TabExplorer)
    {
        if (TabExplorer->objectName().isEmpty())
            TabExplorer->setObjectName("TabExplorer");
        TabExplorer->resize(900, 600);
        verticalLayout_main = new QVBoxLayout(TabExplorer);
        verticalLayout_main->setSpacing(6);
        verticalLayout_main->setObjectName("verticalLayout_main");
        verticalLayout_main->setContentsMargins(4, 4, 4, 4);
        groupImage = new QGroupBox(TabExplorer);
        groupImage->setObjectName("groupImage");
        groupImage->setMaximumHeight(70);
        hboxLayout = new QHBoxLayout(groupImage);
        hboxLayout->setObjectName("hboxLayout");
        comboRecentImages = new QComboBox(groupImage);
        comboRecentImages->addItem(QString());
        comboRecentImages->setObjectName("comboRecentImages");
        comboRecentImages->setMinimumWidth(300);
        comboRecentImages->setEditable(true);

        hboxLayout->addWidget(comboRecentImages);

        btnBrowseImage = new QPushButton(groupImage);
        btnBrowseImage->setObjectName("btnBrowseImage");
        btnBrowseImage->setMaximumWidth(30);

        hboxLayout->addWidget(btnBrowseImage);

        btnOpenImage = new QPushButton(groupImage);
        btnOpenImage->setObjectName("btnOpenImage");
        btnOpenImage->setMaximumWidth(70);

        hboxLayout->addWidget(btnOpenImage);

        btnCloseImage = new QPushButton(groupImage);
        btnCloseImage->setObjectName("btnCloseImage");
        btnCloseImage->setMaximumWidth(70);
        btnCloseImage->setEnabled(false);

        hboxLayout->addWidget(btnCloseImage);

        spacerItem = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout->addItem(spacerItem);

        labelImageInfo = new QLabel(groupImage);
        labelImageInfo->setObjectName("labelImageInfo");

        hboxLayout->addWidget(labelImageInfo);


        verticalLayout_main->addWidget(groupImage);

        mainContent = new QHBoxLayout();
        mainContent->setSpacing(6);
        mainContent->setObjectName("mainContent");
        groupFiles = new QGroupBox(TabExplorer);
        groupFiles->setObjectName("groupFiles");
        vboxLayout = new QVBoxLayout(groupFiles);
        vboxLayout->setSpacing(4);
        vboxLayout->setObjectName("vboxLayout");
        hboxLayout1 = new QHBoxLayout();
        hboxLayout1->setObjectName("hboxLayout1");
        btnUp = new QPushButton(groupFiles);
        btnUp->setObjectName("btnUp");
        btnUp->setMaximumWidth(30);
        btnUp->setEnabled(false);

        hboxLayout1->addWidget(btnUp);

        editPath = new QLineEdit(groupFiles);
        editPath->setObjectName("editPath");
        editPath->setReadOnly(true);

        hboxLayout1->addWidget(editPath);

        btnRefresh = new QPushButton(groupFiles);
        btnRefresh->setObjectName("btnRefresh");
        btnRefresh->setMaximumWidth(30);

        hboxLayout1->addWidget(btnRefresh);


        vboxLayout->addLayout(hboxLayout1);

        tableFiles = new QTableWidget(groupFiles);
        if (tableFiles->columnCount() < 4)
            tableFiles->setColumnCount(4);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        tableFiles->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        tableFiles->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        tableFiles->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        tableFiles->setHorizontalHeaderItem(3, __qtablewidgetitem3);
        tableFiles->setObjectName("tableFiles");
        tableFiles->setColumnCount(4);
        tableFiles->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
        tableFiles->setAlternatingRowColors(true);
        tableFiles->setSortingEnabled(true);

        vboxLayout->addWidget(tableFiles);

        labelSelection = new QLabel(groupFiles);
        labelSelection->setObjectName("labelSelection");

        vboxLayout->addWidget(labelSelection);


        mainContent->addWidget(groupFiles);

        actionsLayout = new QVBoxLayout();
        actionsLayout->setSpacing(6);
        actionsLayout->setObjectName("actionsLayout");
        groupExtract = new QGroupBox(TabExplorer);
        groupExtract->setObjectName("groupExtract");
        groupExtract->setMaximumWidth(180);
        vboxLayout1 = new QVBoxLayout(groupExtract);
        vboxLayout1->setSpacing(4);
        vboxLayout1->setObjectName("vboxLayout1");
        btnExtractSelected = new QPushButton(groupExtract);
        btnExtractSelected->setObjectName("btnExtractSelected");
        btnExtractSelected->setEnabled(false);

        vboxLayout1->addWidget(btnExtractSelected);

        btnExtractAll = new QPushButton(groupExtract);
        btnExtractAll->setObjectName("btnExtractAll");
        btnExtractAll->setEnabled(false);

        vboxLayout1->addWidget(btnExtractAll);

        hboxLayout2 = new QHBoxLayout();
        hboxLayout2->setObjectName("hboxLayout2");
        editExtractPath = new QLineEdit(groupExtract);
        editExtractPath->setObjectName("editExtractPath");

        hboxLayout2->addWidget(editExtractPath);

        btnBrowseExtract = new QPushButton(groupExtract);
        btnBrowseExtract->setObjectName("btnBrowseExtract");
        btnBrowseExtract->setMaximumWidth(30);

        hboxLayout2->addWidget(btnBrowseExtract);


        vboxLayout1->addLayout(hboxLayout2);


        actionsLayout->addWidget(groupExtract);

        groupImport = new QGroupBox(TabExplorer);
        groupImport->setObjectName("groupImport");
        groupImport->setMaximumWidth(180);
        vboxLayout2 = new QVBoxLayout(groupImport);
        vboxLayout2->setSpacing(4);
        vboxLayout2->setObjectName("vboxLayout2");
        btnImportFiles = new QPushButton(groupImport);
        btnImportFiles->setObjectName("btnImportFiles");
        btnImportFiles->setEnabled(false);

        vboxLayout2->addWidget(btnImportFiles);

        btnImportFolder = new QPushButton(groupImport);
        btnImportFolder->setObjectName("btnImportFolder");
        btnImportFolder->setEnabled(false);

        vboxLayout2->addWidget(btnImportFolder);


        actionsLayout->addWidget(groupImport);

        groupFileOps = new QGroupBox(TabExplorer);
        groupFileOps->setObjectName("groupFileOps");
        groupFileOps->setMaximumWidth(180);
        vboxLayout3 = new QVBoxLayout(groupFileOps);
        vboxLayout3->setSpacing(4);
        vboxLayout3->setObjectName("vboxLayout3");
        btnRename = new QPushButton(groupFileOps);
        btnRename->setObjectName("btnRename");
        btnRename->setEnabled(false);

        vboxLayout3->addWidget(btnRename);

        btnDelete = new QPushButton(groupFileOps);
        btnDelete->setObjectName("btnDelete");
        btnDelete->setEnabled(false);

        vboxLayout3->addWidget(btnDelete);

        btnNewFolder = new QPushButton(groupFileOps);
        btnNewFolder->setObjectName("btnNewFolder");
        btnNewFolder->setEnabled(false);

        vboxLayout3->addWidget(btnNewFolder);


        actionsLayout->addWidget(groupFileOps);

        groupDiskOps = new QGroupBox(TabExplorer);
        groupDiskOps->setObjectName("groupDiskOps");
        groupDiskOps->setMaximumWidth(180);
        vboxLayout4 = new QVBoxLayout(groupDiskOps);
        vboxLayout4->setSpacing(4);
        vboxLayout4->setObjectName("vboxLayout4");
        btnNewDisk = new QPushButton(groupDiskOps);
        btnNewDisk->setObjectName("btnNewDisk");

        vboxLayout4->addWidget(btnNewDisk);

        btnValidate = new QPushButton(groupDiskOps);
        btnValidate->setObjectName("btnValidate");
        btnValidate->setEnabled(false);

        vboxLayout4->addWidget(btnValidate);


        actionsLayout->addWidget(groupDiskOps);

        spacerItem1 = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        actionsLayout->addItem(spacerItem1);


        mainContent->addLayout(actionsLayout);


        verticalLayout_main->addLayout(mainContent);

        groupStatus = new QGroupBox(TabExplorer);
        groupStatus->setObjectName("groupStatus");
        groupStatus->setMaximumHeight(80);
        vboxLayout5 = new QVBoxLayout(groupStatus);
        vboxLayout5->setObjectName("vboxLayout5");
        progressBar = new QProgressBar(groupStatus);
        progressBar->setObjectName("progressBar");
        progressBar->setValue(0);
        progressBar->setMaximumHeight(20);

        vboxLayout5->addWidget(progressBar);

        labelStatus = new QLabel(groupStatus);
        labelStatus->setObjectName("labelStatus");

        vboxLayout5->addWidget(labelStatus);


        verticalLayout_main->addWidget(groupStatus);


        retranslateUi(TabExplorer);

        QMetaObject::connectSlotsByName(TabExplorer);
    } // setupUi

    void retranslateUi(QWidget *TabExplorer)
    {
        groupImage->setTitle(QCoreApplication::translate("TabExplorer", "Disk Image", nullptr));
        groupImage->setStyleSheet(QCoreApplication::translate("TabExplorer", "QGroupBox { font-weight: bold; }", nullptr));
        comboRecentImages->setItemText(0, QCoreApplication::translate("TabExplorer", "(Select or drop image...)", nullptr));

        btnBrowseImage->setText(QCoreApplication::translate("TabExplorer", "...", nullptr));
        btnOpenImage->setText(QCoreApplication::translate("TabExplorer", "Open", nullptr));
        btnCloseImage->setText(QCoreApplication::translate("TabExplorer", "Close", nullptr));
        labelImageInfo->setText(QCoreApplication::translate("TabExplorer", "No image loaded", nullptr));
        labelImageInfo->setStyleSheet(QCoreApplication::translate("TabExplorer", "color: #888;", nullptr));
        groupFiles->setTitle(QCoreApplication::translate("TabExplorer", "Directory", nullptr));
        groupFiles->setStyleSheet(QCoreApplication::translate("TabExplorer", "QGroupBox { font-weight: bold; }", nullptr));
        btnUp->setText(QCoreApplication::translate("TabExplorer", "\342\254\206", nullptr));
        editPath->setText(QCoreApplication::translate("TabExplorer", "/", nullptr));
        btnRefresh->setText(QCoreApplication::translate("TabExplorer", "\342\206\273", nullptr));
        QTableWidgetItem *___qtablewidgetitem = tableFiles->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("TabExplorer", "Name", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = tableFiles->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("TabExplorer", "Size", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = tableFiles->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("TabExplorer", "Type", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = tableFiles->horizontalHeaderItem(3);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("TabExplorer", "Date", nullptr));
        labelSelection->setText(QCoreApplication::translate("TabExplorer", "0 items selected", nullptr));
        labelSelection->setStyleSheet(QCoreApplication::translate("TabExplorer", "color: #666;", nullptr));
        groupExtract->setTitle(QCoreApplication::translate("TabExplorer", "Extract", nullptr));
        groupExtract->setStyleSheet(QCoreApplication::translate("TabExplorer", "QGroupBox { font-weight: bold; }", nullptr));
        btnExtractSelected->setText(QCoreApplication::translate("TabExplorer", "\360\237\223\244 Extract Selected", nullptr));
        btnExtractAll->setText(QCoreApplication::translate("TabExplorer", "\360\237\223\244 Extract All", nullptr));
        editExtractPath->setPlaceholderText(QCoreApplication::translate("TabExplorer", "Target folder...", nullptr));
        btnBrowseExtract->setText(QCoreApplication::translate("TabExplorer", "...", nullptr));
        groupImport->setTitle(QCoreApplication::translate("TabExplorer", "Import", nullptr));
        groupImport->setStyleSheet(QCoreApplication::translate("TabExplorer", "QGroupBox { font-weight: bold; }", nullptr));
        btnImportFiles->setText(QCoreApplication::translate("TabExplorer", "\360\237\223\245 Import Files...", nullptr));
        btnImportFolder->setText(QCoreApplication::translate("TabExplorer", "\360\237\223\201 Import Folder...", nullptr));
        groupFileOps->setTitle(QCoreApplication::translate("TabExplorer", "File Operations", nullptr));
        groupFileOps->setStyleSheet(QCoreApplication::translate("TabExplorer", "QGroupBox { font-weight: bold; }", nullptr));
        btnRename->setText(QCoreApplication::translate("TabExplorer", "\342\234\217\357\270\217 Rename", nullptr));
        btnDelete->setText(QCoreApplication::translate("TabExplorer", "\360\237\227\221\357\270\217 Delete", nullptr));
        btnNewFolder->setText(QCoreApplication::translate("TabExplorer", "\360\237\223\201 New Folder", nullptr));
        groupDiskOps->setTitle(QCoreApplication::translate("TabExplorer", "Disk", nullptr));
        groupDiskOps->setStyleSheet(QCoreApplication::translate("TabExplorer", "QGroupBox { font-weight: bold; }", nullptr));
        btnNewDisk->setText(QCoreApplication::translate("TabExplorer", "\360\237\222\276 New Disk...", nullptr));
        btnValidate->setText(QCoreApplication::translate("TabExplorer", "\342\234\223 Validate", nullptr));
        groupStatus->setTitle(QCoreApplication::translate("TabExplorer", "Status", nullptr));
        groupStatus->setStyleSheet(QCoreApplication::translate("TabExplorer", "QGroupBox { font-weight: bold; }", nullptr));
        labelStatus->setText(QCoreApplication::translate("TabExplorer", "Ready", nullptr));
        (void)TabExplorer;
    } // retranslateUi

};

namespace Ui {
    class TabExplorer: public Ui_TabExplorer {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TAB_EXPLORER_H

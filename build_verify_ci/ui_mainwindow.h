/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionOpen;
    QAction *actionSave;
    QAction *actionSaveAs;
    QAction *actionExit;
    QAction *actionConnect;
    QAction *actionDisconnect;
    QAction *actionReadDisk;
    QAction *actionWriteDisk;
    QAction *actionVerifyDisk;
    QAction *actionMotorOn;
    QAction *actionMotorOff;
    QAction *actionConvert;
    QAction *actionCompare;
    QAction *actionRepair;
    QAction *actionAnalyze;
    QAction *actionLabelEditor;
    QAction *actionBAMViewer;
    QAction *actionBootblockViewer;
    QAction *actionProtectionAnalyzer;
    QAction *actionChecksumDatabase;
    QAction *actionDarkMode;
    QAction *actionLangEnglish;
    QAction *actionLangGerman;
    QAction *actionLangFrench;
    QAction *actionLoadLanguage;
    QAction *actionPreferences;
    QAction *actionHelp;
    QAction *actionKeyboardShortcuts;
    QAction *actionCheckUpdates;
    QAction *actionAbout;
    QWidget *centralwidget;
    QVBoxLayout *vboxLayout;
    QFrame *frameStatusLED;
    QHBoxLayout *hboxLayout;
    QLabel *labelLED;
    QLabel *labelHWStatus;
    QSpacerItem *spacerItem;
    QLabel *labelImageInfo;
    QTabWidget *tabWidget;
    QWidget *tab_workflow;
    QWidget *tab_status;
    QWidget *tab_hardware;
    QWidget *tab_format;
    QWidget *tab_explorer;
    QWidget *tab_tools;
    QWidget *tab_signal_analysis;
    QMenuBar *menubar;
    QMenu *menuFile;
    QMenu *menuRecentFiles;
    QMenu *menuDrive;
    QMenu *menuTools;
    QMenu *menuSettings;
    QMenu *menuLanguage;
    QMenu *menuHelp;
    QStatusBar *statusbar;
    QToolBar *toolBar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1000, 780);
        MainWindow->setAcceptDrops(true);
        actionOpen = new QAction(MainWindow);
        actionOpen->setObjectName("actionOpen");
        actionSave = new QAction(MainWindow);
        actionSave->setObjectName("actionSave");
        actionSaveAs = new QAction(MainWindow);
        actionSaveAs->setObjectName("actionSaveAs");
        actionExit = new QAction(MainWindow);
        actionExit->setObjectName("actionExit");
        actionConnect = new QAction(MainWindow);
        actionConnect->setObjectName("actionConnect");
        actionDisconnect = new QAction(MainWindow);
        actionDisconnect->setObjectName("actionDisconnect");
        actionReadDisk = new QAction(MainWindow);
        actionReadDisk->setObjectName("actionReadDisk");
        actionWriteDisk = new QAction(MainWindow);
        actionWriteDisk->setObjectName("actionWriteDisk");
        actionVerifyDisk = new QAction(MainWindow);
        actionVerifyDisk->setObjectName("actionVerifyDisk");
        actionMotorOn = new QAction(MainWindow);
        actionMotorOn->setObjectName("actionMotorOn");
        actionMotorOff = new QAction(MainWindow);
        actionMotorOff->setObjectName("actionMotorOff");
        actionConvert = new QAction(MainWindow);
        actionConvert->setObjectName("actionConvert");
        actionCompare = new QAction(MainWindow);
        actionCompare->setObjectName("actionCompare");
        actionRepair = new QAction(MainWindow);
        actionRepair->setObjectName("actionRepair");
        actionAnalyze = new QAction(MainWindow);
        actionAnalyze->setObjectName("actionAnalyze");
        actionLabelEditor = new QAction(MainWindow);
        actionLabelEditor->setObjectName("actionLabelEditor");
        actionBAMViewer = new QAction(MainWindow);
        actionBAMViewer->setObjectName("actionBAMViewer");
        actionBootblockViewer = new QAction(MainWindow);
        actionBootblockViewer->setObjectName("actionBootblockViewer");
        actionProtectionAnalyzer = new QAction(MainWindow);
        actionProtectionAnalyzer->setObjectName("actionProtectionAnalyzer");
        actionChecksumDatabase = new QAction(MainWindow);
        actionChecksumDatabase->setObjectName("actionChecksumDatabase");
        actionDarkMode = new QAction(MainWindow);
        actionDarkMode->setObjectName("actionDarkMode");
        actionDarkMode->setCheckable(true);
        actionLangEnglish = new QAction(MainWindow);
        actionLangEnglish->setObjectName("actionLangEnglish");
        actionLangEnglish->setCheckable(true);
        actionLangEnglish->setChecked(true);
        actionLangGerman = new QAction(MainWindow);
        actionLangGerman->setObjectName("actionLangGerman");
        actionLangGerman->setCheckable(true);
        actionLangFrench = new QAction(MainWindow);
        actionLangFrench->setObjectName("actionLangFrench");
        actionLangFrench->setCheckable(true);
        actionLoadLanguage = new QAction(MainWindow);
        actionLoadLanguage->setObjectName("actionLoadLanguage");
        actionPreferences = new QAction(MainWindow);
        actionPreferences->setObjectName("actionPreferences");
        actionHelp = new QAction(MainWindow);
        actionHelp->setObjectName("actionHelp");
        actionKeyboardShortcuts = new QAction(MainWindow);
        actionKeyboardShortcuts->setObjectName("actionKeyboardShortcuts");
        actionCheckUpdates = new QAction(MainWindow);
        actionCheckUpdates->setObjectName("actionCheckUpdates");
        actionAbout = new QAction(MainWindow);
        actionAbout->setObjectName("actionAbout");
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        vboxLayout = new QVBoxLayout(centralwidget);
        vboxLayout->setSpacing(4);
        vboxLayout->setObjectName("vboxLayout");
        vboxLayout->setContentsMargins(4, 4, 4, 4);
        frameStatusLED = new QFrame(centralwidget);
        frameStatusLED->setObjectName("frameStatusLED");
        frameStatusLED->setMaximumHeight(30);
        frameStatusLED->setFrameShape(QFrame::StyledPanel);
        hboxLayout = new QHBoxLayout(frameStatusLED);
        hboxLayout->setObjectName("hboxLayout");
        hboxLayout->setContentsMargins(4, 2, 4, 2);
        labelLED = new QLabel(frameStatusLED);
        labelLED->setObjectName("labelLED");

        hboxLayout->addWidget(labelLED);

        labelHWStatus = new QLabel(frameStatusLED);
        labelHWStatus->setObjectName("labelHWStatus");

        hboxLayout->addWidget(labelHWStatus);

        spacerItem = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        hboxLayout->addItem(spacerItem);

        labelImageInfo = new QLabel(frameStatusLED);
        labelImageInfo->setObjectName("labelImageInfo");

        hboxLayout->addWidget(labelImageInfo);


        vboxLayout->addWidget(frameStatusLED);

        tabWidget = new QTabWidget(centralwidget);
        tabWidget->setObjectName("tabWidget");
        tab_workflow = new QWidget();
        tab_workflow->setObjectName("tab_workflow");
        tabWidget->addTab(tab_workflow, QString());
        tab_status = new QWidget();
        tab_status->setObjectName("tab_status");
        tabWidget->addTab(tab_status, QString());
        tab_hardware = new QWidget();
        tab_hardware->setObjectName("tab_hardware");
        tabWidget->addTab(tab_hardware, QString());
        tab_format = new QWidget();
        tab_format->setObjectName("tab_format");
        tabWidget->addTab(tab_format, QString());
        tab_explorer = new QWidget();
        tab_explorer->setObjectName("tab_explorer");
        tabWidget->addTab(tab_explorer, QString());
        tab_tools = new QWidget();
        tab_tools->setObjectName("tab_tools");
        tabWidget->addTab(tab_tools, QString());
        tab_signal_analysis = new QWidget();
        tab_signal_analysis->setObjectName("tab_signal_analysis");
        tabWidget->addTab(tab_signal_analysis, QString());

        vboxLayout->addWidget(tabWidget);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menuFile = new QMenu(menubar);
        menuFile->setObjectName("menuFile");
        menuRecentFiles = new QMenu(menuFile);
        menuRecentFiles->setObjectName("menuRecentFiles");
        menuDrive = new QMenu(menubar);
        menuDrive->setObjectName("menuDrive");
        menuTools = new QMenu(menubar);
        menuTools->setObjectName("menuTools");
        menuSettings = new QMenu(menubar);
        menuSettings->setObjectName("menuSettings");
        menuLanguage = new QMenu(menuSettings);
        menuLanguage->setObjectName("menuLanguage");
        menuHelp = new QMenu(menubar);
        menuHelp->setObjectName("menuHelp");
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);
        toolBar = new QToolBar(MainWindow);
        toolBar->setObjectName("toolBar");
        MainWindow->addToolBar(toolBar);

        menubar->addAction(menuFile->menuAction());
        menubar->addAction(menuDrive->menuAction());
        menubar->addAction(menuTools->menuAction());
        menubar->addAction(menuSettings->menuAction());
        menubar->addAction(menuHelp->menuAction());
        menuFile->addAction(actionOpen);
        menuFile->addAction(actionSave);
        menuFile->addAction(actionSaveAs);
        menuFile->addSeparator();
        menuFile->addAction(menuRecentFiles->menuAction());
        menuFile->addSeparator();
        menuFile->addAction(actionExit);
        menuDrive->addAction(actionConnect);
        menuDrive->addAction(actionDisconnect);
        menuDrive->addSeparator();
        menuDrive->addAction(actionReadDisk);
        menuDrive->addAction(actionWriteDisk);
        menuDrive->addAction(actionVerifyDisk);
        menuDrive->addSeparator();
        menuDrive->addAction(actionMotorOn);
        menuDrive->addAction(actionMotorOff);
        menuTools->addAction(actionConvert);
        menuTools->addAction(actionCompare);
        menuTools->addAction(actionRepair);
        menuTools->addAction(actionAnalyze);
        menuTools->addSeparator();
        menuTools->addAction(actionLabelEditor);
        menuTools->addAction(actionBAMViewer);
        menuTools->addAction(actionBootblockViewer);
        menuTools->addAction(actionProtectionAnalyzer);
        menuTools->addSeparator();
        menuTools->addAction(actionChecksumDatabase);
        menuSettings->addAction(actionDarkMode);
        menuSettings->addSeparator();
        menuSettings->addAction(menuLanguage->menuAction());
        menuSettings->addSeparator();
        menuSettings->addAction(actionPreferences);
        menuLanguage->addAction(actionLangEnglish);
        menuLanguage->addAction(actionLangGerman);
        menuLanguage->addAction(actionLangFrench);
        menuLanguage->addSeparator();
        menuLanguage->addAction(actionLoadLanguage);
        menuHelp->addAction(actionHelp);
        menuHelp->addAction(actionKeyboardShortcuts);
        menuHelp->addSeparator();
        menuHelp->addAction(actionCheckUpdates);
        menuHelp->addSeparator();
        menuHelp->addAction(actionAbout);
        toolBar->addAction(actionOpen);
        toolBar->addAction(actionSave);
        toolBar->addSeparator();
        toolBar->addAction(actionConnect);
        toolBar->addSeparator();
        toolBar->addAction(actionReadDisk);
        toolBar->addAction(actionWriteDisk);

        retranslateUi(MainWindow);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "UnifiedFloppyTool v4.1.0", nullptr));
        actionOpen->setText(QCoreApplication::translate("MainWindow", "&Open...", nullptr));
#if QT_CONFIG(shortcut)
        actionOpen->setShortcut(QCoreApplication::translate("MainWindow", "Ctrl+O", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        actionOpen->setToolTip(QCoreApplication::translate("MainWindow", "Open disk image", nullptr));
#endif // QT_CONFIG(tooltip)
        actionSave->setText(QCoreApplication::translate("MainWindow", "&Save", nullptr));
#if QT_CONFIG(shortcut)
        actionSave->setShortcut(QCoreApplication::translate("MainWindow", "Ctrl+S", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        actionSave->setToolTip(QCoreApplication::translate("MainWindow", "Save disk image", nullptr));
#endif // QT_CONFIG(tooltip)
        actionSaveAs->setText(QCoreApplication::translate("MainWindow", "Save &As...", nullptr));
#if QT_CONFIG(shortcut)
        actionSaveAs->setShortcut(QCoreApplication::translate("MainWindow", "Ctrl+Shift+S", nullptr));
#endif // QT_CONFIG(shortcut)
        actionExit->setText(QCoreApplication::translate("MainWindow", "E&xit", nullptr));
#if QT_CONFIG(shortcut)
        actionExit->setShortcut(QCoreApplication::translate("MainWindow", "Alt+F4", nullptr));
#endif // QT_CONFIG(shortcut)
        actionConnect->setText(QCoreApplication::translate("MainWindow", "&Connect", nullptr));
#if QT_CONFIG(shortcut)
        actionConnect->setShortcut(QCoreApplication::translate("MainWindow", "F2", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        actionConnect->setToolTip(QCoreApplication::translate("MainWindow", "Connect to hardware", nullptr));
#endif // QT_CONFIG(tooltip)
        actionDisconnect->setText(QCoreApplication::translate("MainWindow", "&Disconnect", nullptr));
        actionReadDisk->setText(QCoreApplication::translate("MainWindow", "&Read Disk", nullptr));
#if QT_CONFIG(shortcut)
        actionReadDisk->setShortcut(QCoreApplication::translate("MainWindow", "F5", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        actionReadDisk->setToolTip(QCoreApplication::translate("MainWindow", "Read disk to image", nullptr));
#endif // QT_CONFIG(tooltip)
        actionWriteDisk->setText(QCoreApplication::translate("MainWindow", "&Write Disk", nullptr));
#if QT_CONFIG(shortcut)
        actionWriteDisk->setShortcut(QCoreApplication::translate("MainWindow", "F6", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        actionWriteDisk->setToolTip(QCoreApplication::translate("MainWindow", "Write image to disk", nullptr));
#endif // QT_CONFIG(tooltip)
        actionVerifyDisk->setText(QCoreApplication::translate("MainWindow", "&Verify Disk", nullptr));
#if QT_CONFIG(shortcut)
        actionVerifyDisk->setShortcut(QCoreApplication::translate("MainWindow", "F7", nullptr));
#endif // QT_CONFIG(shortcut)
        actionMotorOn->setText(QCoreApplication::translate("MainWindow", "Motor &On", nullptr));
        actionMotorOff->setText(QCoreApplication::translate("MainWindow", "Motor O&ff", nullptr));
        actionConvert->setText(QCoreApplication::translate("MainWindow", "&Convert...", nullptr));
        actionCompare->setText(QCoreApplication::translate("MainWindow", "Co&mpare...", nullptr));
        actionRepair->setText(QCoreApplication::translate("MainWindow", "&Repair...", nullptr));
        actionAnalyze->setText(QCoreApplication::translate("MainWindow", "&Analyze...", nullptr));
#if QT_CONFIG(shortcut)
        actionAnalyze->setShortcut(QCoreApplication::translate("MainWindow", "F8", nullptr));
#endif // QT_CONFIG(shortcut)
        actionLabelEditor->setText(QCoreApplication::translate("MainWindow", "&Label Editor...", nullptr));
        actionBAMViewer->setText(QCoreApplication::translate("MainWindow", "&BAM/FAT Viewer...", nullptr));
        actionBootblockViewer->setText(QCoreApplication::translate("MainWindow", "Boot&block Viewer...", nullptr));
        actionProtectionAnalyzer->setText(QCoreApplication::translate("MainWindow", "&Protection Analyzer...", nullptr));
        actionChecksumDatabase->setText(QCoreApplication::translate("MainWindow", "&Checksum Database...", nullptr));
        actionDarkMode->setText(QCoreApplication::translate("MainWindow", "&Dark Mode", nullptr));
#if QT_CONFIG(shortcut)
        actionDarkMode->setShortcut(QCoreApplication::translate("MainWindow", "Ctrl+D", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        actionDarkMode->setToolTip(QCoreApplication::translate("MainWindow", "Toggle dark/light theme", nullptr));
#endif // QT_CONFIG(tooltip)
        actionLangEnglish->setText(QCoreApplication::translate("MainWindow", "English", nullptr));
        actionLangGerman->setText(QCoreApplication::translate("MainWindow", "Deutsch", nullptr));
        actionLangFrench->setText(QCoreApplication::translate("MainWindow", "Fran\303\247ais", nullptr));
        actionLoadLanguage->setText(QCoreApplication::translate("MainWindow", "Load Language File...", nullptr));
        actionPreferences->setText(QCoreApplication::translate("MainWindow", "&Preferences...", nullptr));
#if QT_CONFIG(shortcut)
        actionPreferences->setShortcut(QCoreApplication::translate("MainWindow", "Ctrl+,", nullptr));
#endif // QT_CONFIG(shortcut)
        actionHelp->setText(QCoreApplication::translate("MainWindow", "&Help", nullptr));
#if QT_CONFIG(shortcut)
        actionHelp->setShortcut(QCoreApplication::translate("MainWindow", "F1", nullptr));
#endif // QT_CONFIG(shortcut)
        actionKeyboardShortcuts->setText(QCoreApplication::translate("MainWindow", "&Keyboard Shortcuts...", nullptr));
        actionCheckUpdates->setText(QCoreApplication::translate("MainWindow", "Check for &Updates...", nullptr));
        actionAbout->setText(QCoreApplication::translate("MainWindow", "&About...", nullptr));
        labelLED->setText(QCoreApplication::translate("MainWindow", "\342\227\217", nullptr));
        labelLED->setStyleSheet(QCoreApplication::translate("MainWindow", "color: #888888; font-size: 16pt;", nullptr));
#if QT_CONFIG(tooltip)
        labelLED->setToolTip(QCoreApplication::translate("MainWindow", "Hardware connection status", nullptr));
#endif // QT_CONFIG(tooltip)
        labelHWStatus->setText(QCoreApplication::translate("MainWindow", "No hardware connected", nullptr));
        labelHWStatus->setStyleSheet(QCoreApplication::translate("MainWindow", "color: #666;", nullptr));
        labelImageInfo->setText(QCoreApplication::translate("MainWindow", "No image loaded", nullptr));
        labelImageInfo->setStyleSheet(QCoreApplication::translate("MainWindow", "color: #666;", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_workflow), QCoreApplication::translate("MainWindow", "Workflow", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_status), QCoreApplication::translate("MainWindow", "Status", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_hardware), QCoreApplication::translate("MainWindow", "Hardware", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_format), QCoreApplication::translate("MainWindow", "Settings", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_explorer), QCoreApplication::translate("MainWindow", "Explorer", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_tools), QCoreApplication::translate("MainWindow", "Tools", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_signal_analysis), QCoreApplication::translate("MainWindow", "Signal Analysis", nullptr));
        menuFile->setTitle(QCoreApplication::translate("MainWindow", "&File", nullptr));
        menuRecentFiles->setTitle(QCoreApplication::translate("MainWindow", "Recent Files", nullptr));
        menuDrive->setTitle(QCoreApplication::translate("MainWindow", "&Drive", nullptr));
        menuTools->setTitle(QCoreApplication::translate("MainWindow", "&Tools", nullptr));
        menuSettings->setTitle(QCoreApplication::translate("MainWindow", "&Settings", nullptr));
        menuLanguage->setTitle(QCoreApplication::translate("MainWindow", "Language", nullptr));
        menuHelp->setTitle(QCoreApplication::translate("MainWindow", "&Help", nullptr));
        toolBar->setWindowTitle(QCoreApplication::translate("MainWindow", "Main Toolbar", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H

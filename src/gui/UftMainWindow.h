/**
 * @file UftMainWindow.h
 * @brief Consolidated Main Window
 * 
 * P2-003: GUI Konsolidierung
 * 
 * Integriert alle GUI-Komponenten in einem einheitlichen Fenster:
 * - Tab-basierte Oberfläche
 * - Statusbar mit Progress
 * - Toolbar für schnelle Aktionen
 * - Dock Widgets für Tools
 */

#ifndef UFT_MAIN_WINDOW_H
#define UFT_MAIN_WINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QProgressBar>
#include <QDockWidget>
#include <QAction>

/* Forward declarations */
class UftMainController;
class UftFormatDetectionWidget;
class UftWidgetBinder;
class QSpinBox;
class QComboBox;
class QLineEdit;
class QCheckBox;

/**
 * @brief Main application window
 */
class UftMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit UftMainWindow(QWidget *parent = nullptr);
    ~UftMainWindow();
    
protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    /* File menu */
    void onOpen();
    void onSave();
    void onSaveAs();
    void onOpenRecent();
    void onClose();
    void onQuit();
    
    /* Edit menu */
    void onUndo();
    void onRedo();
    void onPreferences();
    
    /* Disk menu */
    void onReadDisk();
    void onWriteDisk();
    void onVerifyDisk();
    void onFormatDisk();
    void onAnalyzeDisk();
    
    /* Tools menu */
    void onConvert();
    void onCompare();
    void onBatchProcess();
    
    /* Help menu */
    void onAbout();
    void onHelp();
    
    /* Controller signals */
    void onBusyChanged(bool busy);
    void onProgressChanged(int progress);
    void onStatusChanged(const QString &message);
    void onErrorOccurred(const QString &error);
    void onOperationCompleted(int op, bool success);

private:
    void setupUi();
    void setupMenus();
    void setupToolbar();
    void setupStatusbar();
    void setupTabs();
    void setupDocks();
    void setupConnections();
    void bindWidgets();
    void updateRecentFilesMenu();
    void updateWindowTitle();
    
    /* Controller */
    UftMainController *m_controller = nullptr;
    
    /* Central widget */
    QTabWidget *m_tabWidget = nullptr;
    
    /* Tabs */
    QWidget *m_tabFormat = nullptr;
    QWidget *m_tabHardware = nullptr;
    QWidget *m_tabRecovery = nullptr;
    QWidget *m_tabProtection = nullptr;
    QWidget *m_tabDiagnostics = nullptr;
    
    /* Toolbar */
    QToolBar *m_mainToolbar = nullptr;
    
    /* Status bar widgets */
    QLabel *m_statusLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_formatLabel = nullptr;
    
    /* Dock widgets */
    QDockWidget *m_formatDetectionDock = nullptr;
    UftFormatDetectionWidget *m_formatDetectionWidget = nullptr;
    
    /* Actions */
    QAction *m_actOpen = nullptr;
    QAction *m_actSave = nullptr;
    QAction *m_actClose = nullptr;
    QAction *m_actRead = nullptr;
    QAction *m_actWrite = nullptr;
    QAction *m_actVerify = nullptr;
    QAction *m_actCancel = nullptr;
    QAction *m_actUndo = nullptr;
    QAction *m_actRedo = nullptr;
    QAction *m_actDarkMode = nullptr;
    
    /* Recent files menu */
    QMenu *m_recentFilesMenu = nullptr;
    
    /* Format tab widgets (for binding) */
    QComboBox *m_formatCombo = nullptr;
    QSpinBox *m_cylindersSpin = nullptr;
    QSpinBox *m_headsSpin = nullptr;
    QSpinBox *m_sectorsSpin = nullptr;
    QComboBox *m_encodingCombo = nullptr;
    
    /* Hardware tab widgets */
    QComboBox *m_hardwareCombo = nullptr;
    QLineEdit *m_deviceEdit = nullptr;
    QSpinBox *m_driveNumberSpin = nullptr;
    
    /* Recovery tab widgets */
    QSpinBox *m_retriesSpin = nullptr;
    QSpinBox *m_revolutionsSpin = nullptr;
    QCheckBox *m_weakBitsCheck = nullptr;
    /* Write widgets */
    QCheckBox *m_verifyAfterWriteCheck = nullptr;
    QSpinBox *m_writeRetriesSpin = nullptr;
    
    /* Path widgets */
    QLineEdit *m_inputPathEdit = nullptr;
    QLineEdit *m_outputPathEdit = nullptr;
};

#endif /* UFT_MAIN_WINDOW_H */

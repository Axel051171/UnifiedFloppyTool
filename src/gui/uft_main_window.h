/**
 * @file uft_main_window.h
 * @brief UFT Main Window - Qt 6 GUI
 * @version 5.33.0
 */

#ifndef UFT_MAIN_WINDOW_H
#define UFT_MAIN_WINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QProgressBar>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class UftMainWindow; }
QT_END_NAMESPACE

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

class UftFluxPanel;
class UftFormatPanel;
class UftXCopyPanel;
class UftNibblePanel;
class UftRecoveryPanel;
class UftForensicPanel;
class UftProtectionPanel;
class UftHexViewerPanel;
class UftFileBrowserPanel;
class UftHardwarePanel;
class UftTrackGridWidget;
class UftDmkAnalyzerPanel;
class UftGw2DmkPanel;
class AnalyzerToolbar;
class TrackAnalyzerWidget;
struct ToolbarAnalysisResult;
enum class CopyMode;

/* ============================================================================
 * Main Window
 * ============================================================================ */

class UftMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit UftMainWindow(QWidget *parent = nullptr);
    ~UftMainWindow();

    /* File Operations */
    bool openImage(const QString &path);
    bool saveImage(const QString &path);
    bool convertImage(const QString &srcPath, const QString &dstPath);

public slots:
    /* Menu Actions */
    void onOpenFile();
    void onSaveFile();
    void onSaveFileAs();
    void onExportFiles();
    void onImportFiles();
    void onExit();

    /* Disk Operations */
    void onReadDisk();
    void onWriteDisk();
    void onVerifyDisk();
    void onFormatDisk();

    /* Tools */
    void onConvert();
    void onAnalyze();
    void onRepair();
    void onCompare();

    /* Hardware */
    void onDetectHardware();
    void onHardwareSettings();

    /* Help */
    void onAbout();
    void onHelp();

signals:
    void imageLoaded(const QString &path);
    void imageSaved(const QString &path);
    void operationStarted(const QString &operation);
    void operationProgress(int percent, const QString &status);
    void operationFinished(bool success, const QString &message);

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupAnalyzerToolbar();  /* P2-12: AnalyzerToolbar */
    void setupStatusBar();
    void setupCentralWidget();
    void setupConnections();
    void setupAnalyzerConnections();  /* P2-13: Analyzer ↔ XCopy */
    void loadSettings();
    void saveSettings();

    /* UI Components */
    Ui::UftMainWindow *ui;
    QTabWidget *m_mainTabs;
    
    /* Analyzer Toolbar - P2-12 */
    AnalyzerToolbar *m_analyzerToolbar;
    TrackAnalyzerWidget *m_trackAnalyzer;
    
    /* Panels */
    UftFluxPanel       *m_fluxPanel;
    UftFormatPanel     *m_formatPanel;
    UftXCopyPanel      *m_xcopyPanel;
    UftNibblePanel     *m_nibblePanel;
    UftRecoveryPanel   *m_recoveryPanel;
    UftForensicPanel   *m_forensicPanel;
    UftProtectionPanel *m_protectionPanel;
    UftHexViewerPanel  *m_hexViewerPanel;
    UftFileBrowserPanel *m_fileBrowserPanel;
    UftHardwarePanel   *m_hardwarePanel;
    UftDmkAnalyzerPanel *m_dmkAnalyzerPanel;
    UftGw2DmkPanel     *m_gw2DmkPanel;
    
    /* Track Visualization */
    UftTrackGridWidget *m_trackGrid;
    
    /* Status Bar */
    QLabel *m_statusLabel;
    QLabel *m_formatLabel;
    QLabel *m_hardwareLabel;
    QProgressBar *m_progressBar;
    
    /* Current State */
    QString m_currentFile;
    QString m_currentFormat;
    bool m_modified;

private slots:
    /* Analyzer Integration - P2-12/P2-13 */
    void onAnalyzerQuickScan();
    void onAnalyzerFullAnalysis();
    void onAnalyzerApply(CopyMode mode);
    void onQuickScanComplete(const ToolbarAnalysisResult &result);
    void onImageLoadedForAnalysis(const QString &path);
};

#endif /* UFT_MAIN_WINDOW_H */

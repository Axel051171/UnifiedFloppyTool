/**
 * @file uft_dmk_analyzer_panel.h
 * @brief GUI Panel for DMK Disk Image Analysis ("Finger" Tool)
 * 
 * Provides detailed analysis of DMK disk images:
 * - Header information (tracks, sides, density)
 * - Track-by-track sector listing
 * - CRC validation and error detection
 * - Sector data hex dump
 * - Export to raw binary
 * 
 * Based on qbarnes/fgrdmk concept (finger DMK)
 * 
 * @version 1.0
 * @date 2026-01-16
 */

#ifndef UFT_DMK_ANALYZER_PANEL_H
#define UFT_DMK_ANALYZER_PANEL_H

#include <QWidget>
#include <QThread>
#include <QMutex>
#include <memory>

class QLabel;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QProgressBar;
class QGroupBox;
class QTextEdit;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QTabWidget;
class QSplitter;
class QLineEdit;
class QPlainTextEdit;

// Forward declarations
struct uft_dmk_image_t;
struct uft_dmk_track_t;
struct uft_dmk_sector_t;

/**
 * @brief DMK sector analysis result
 */
struct DmkSectorInfo {
    int cylinder;
    int head;
    int sector;
    int sizeCode;
    int dataSize;
    bool fmEncoding;
    bool deleted;
    bool crcOk;
    uint16_t actualCrc;
    uint16_t computedCrc;
    int dataOffset;
    QByteArray data;
};

/**
 * @brief DMK track analysis result
 */
struct DmkTrackInfo {
    int cylinder;
    int head;
    int trackLength;
    int numIdams;
    int numSectors;
    bool hasErrors;
    QList<DmkSectorInfo> sectors;
};

/**
 * @brief DMK image analysis result
 */
struct DmkAnalysisResult {
    QString filename;
    bool valid;
    QString errorMessage;
    
    // Header info
    int tracks;
    int heads;
    int trackLength;
    bool singleSided;
    bool singleDensity;
    bool writeProtected;
    bool nativeMode;
    
    // Statistics
    int totalSectors;
    int errorSectors;
    int deletedSectors;
    int fmSectors;
    int mfmSectors;
    
    // Tracks
    QList<DmkTrackInfo> trackList;
};

/**
 * @brief Worker thread for DMK analysis
 */
class UftDmkAnalyzerWorker : public QThread
{
    Q_OBJECT

public:
    explicit UftDmkAnalyzerWorker(QObject *parent = nullptr);
    ~UftDmkAnalyzerWorker();

    void setFile(const QString &path);
    void setExportPath(const QString &path);
    void setExportFillByte(uint8_t fill);
    
    void analyzeFile();
    void exportToRaw();
    void requestStop();

signals:
    void analysisStarted();
    void analysisProgress(int track, int total);
    void analysisComplete(const DmkAnalysisResult &result);
    void analysisError(const QString &error);
    void exportComplete(const QString &path, qint64 size);
    void exportError(const QString &error);

protected:
    void run() override;

private:
    enum Operation { OP_NONE, OP_ANALYZE, OP_EXPORT };
    
    DmkAnalysisResult performAnalysis();
    bool performExport();
    
    QMutex m_mutex;
    volatile bool m_stopRequested;
    Operation m_operation;
    QString m_filePath;
    QString m_exportPath;
    uint8_t m_fillByte;
};

/**
 * @brief Main GUI panel for DMK Analysis
 */
class UftDmkAnalyzerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftDmkAnalyzerPanel(QWidget *parent = nullptr);
    ~UftDmkAnalyzerPanel();

    void setFile(const QString &path);
    QString currentFile() const { return m_currentFile; }

public slots:
    void openFile();
    void analyzeFile();
    void exportToRaw();
    void copyToClipboard();
    void showSectorData(int track, int head, int sector);

private slots:
    void onAnalysisComplete(const DmkAnalysisResult &result);
    void onAnalysisError(const QString &error);
    void onExportComplete(const QString &path, qint64 size);
    void onExportError(const QString &error);
    void onTrackSelected(QTreeWidgetItem *item, int column);
    void onSectorDoubleClicked(int row, int column);
    void onShowAllSectors(bool checked);
    void onShowErrorsOnly(bool checked);

signals:
    void fileLoaded(const QString &filename);
    void analysisRequested(const QString &filename);
    void sectorSelected(int track, int head, int sector);

private:
    void setupUi();
    void updateDisplay(const DmkAnalysisResult &result);
    void populateTrackTree(const DmkAnalysisResult &result);
    void populateSectorTable(const DmkTrackInfo &track);
    void showHexDump(const QByteArray &data);
    void addLogMessage(const QString &msg, bool isError = false);
    QString formatSectorSize(int sizeCode, bool fm);
    QString formatEncoding(bool fm);
    QString formatCrcStatus(bool ok, uint16_t actual, uint16_t computed);

    // Worker
    UftDmkAnalyzerWorker *m_worker;
    QString m_currentFile;
    DmkAnalysisResult m_currentResult;

    // File selection
    QGroupBox *m_fileGroup;
    QLineEdit *m_filePathEdit;
    QPushButton *m_browseBtn;
    QPushButton *m_analyzeBtn;

    // Overview panel
    QGroupBox *m_overviewGroup;
    QLabel *m_filenameLabel;
    QLabel *m_tracksLabel;
    QLabel *m_headsLabel;
    QLabel *m_trackLengthLabel;
    QLabel *m_densityLabel;
    QLabel *m_writeProtectLabel;
    QLabel *m_totalSectorsLabel;
    QLabel *m_errorSectorsLabel;
    QLabel *m_deletedSectorsLabel;
    QLabel *m_fmSectorsLabel;
    QLabel *m_mfmSectorsLabel;

    // Main content area
    QSplitter *m_mainSplitter;
    QTabWidget *m_tabWidget;

    // Track tree view
    QTreeWidget *m_trackTree;

    // Sector table
    QTableWidget *m_sectorTable;
    QCheckBox *m_showAllCheck;
    QCheckBox *m_showErrorsCheck;

    // Hex view
    QPlainTextEdit *m_hexView;
    QLabel *m_hexInfoLabel;
    QPushButton *m_copyHexBtn;

    // Export options
    QGroupBox *m_exportGroup;
    QLineEdit *m_exportPathEdit;
    QPushButton *m_exportBrowseBtn;
    QPushButton *m_exportBtn;
    QSpinBox *m_fillByteSpin;

    // Status
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;

    // Log
    QTextEdit *m_logText;
};

#endif /* UFT_DMK_ANALYZER_PANEL_H */

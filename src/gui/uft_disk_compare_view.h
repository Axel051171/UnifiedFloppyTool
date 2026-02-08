/**
 * @file uft_disk_compare_view.h
 * @brief Disk Compare View (P2-GUI-009)
 * 
 * Side-by-side comparison of two disk images with diff highlighting.
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_DISK_COMPARE_VIEW_H
#define UFT_DISK_COMPARE_VIEW_H

#include <QWidget>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTextEdit>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QProgressBar>
#include <QGroupBox>
#include <QSpinBox>
#include <QTabWidget>

/*===========================================================================
 * Comparison Result Structures
 *===========================================================================*/

/**
 * @brief Sector comparison result
 */
struct UftSectorCompare {
    int cylinder;
    int head;
    int sector;
    
    bool left_present;
    bool right_present;
    bool data_match;
    bool crc_left_ok;
    bool crc_right_ok;
    
    int first_diff_offset;
    int diff_count;
    
    QString left_hash;
    QString right_hash;
};

/**
 * @brief Track comparison result
 */
struct UftTrackCompare {
    int cylinder;
    int head;
    
    bool left_present;
    bool right_present;
    
    int sectors_left;
    int sectors_right;
    int sectors_match;
    int sectors_differ;
    int sectors_missing;
    
    float similarity;  /* 0.0 - 1.0 */
};

/**
 * @brief Disk comparison summary
 */
struct UftDiskCompareSummary {
    /* Basic info */
    QString left_path;
    QString right_path;
    QString left_format;
    QString right_format;
    
    /* Statistics */
    int total_tracks;
    int matching_tracks;
    int differing_tracks;
    int left_only_tracks;
    int right_only_tracks;
    
    int total_sectors;
    int matching_sectors;
    int differing_sectors;
    int left_only_sectors;
    int right_only_sectors;
    
    float overall_similarity;  /* 0.0 - 1.0 */
    
    /* Hashes */
    QString left_hash_md5;
    QString right_hash_md5;
    QString left_hash_sha1;
    QString right_hash_sha1;
};

/*===========================================================================
 * Comparison View Components
 *===========================================================================*/

/**
 * @brief Track grid with diff highlighting
 */
class UftCompareTrackGrid : public QWidget
{
    Q_OBJECT
    
public:
    explicit UftCompareTrackGrid(QWidget *parent = nullptr);
    
    void setCompareResults(const QList<UftTrackCompare> &results);
    void clear();

signals:
    void trackSelected(int cylinder, int head);
    void trackDoubleClicked(int cylinder, int head);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QList<UftTrackCompare> m_results;
    int m_selectedCyl;
    int m_selectedHead;
    int m_cellSize;
    int m_cylinders;
    int m_heads;
    
    QPoint trackToCell(int cyl, int head) const;
    bool cellToTrack(const QPoint &pos, int &cyl, int &head) const;
};

/**
 * @brief Hex diff view
 */
class UftHexDiffView : public QWidget
{
    Q_OBJECT
    
public:
    explicit UftHexDiffView(QWidget *parent = nullptr);
    
    void setData(const QByteArray &left, const QByteArray &right);
    void setHighlightDiffs(bool highlight);
    void clear();

private:
    void updateView();
    
    QTextEdit *m_leftHex;
    QTextEdit *m_rightHex;
    QByteArray m_leftData;
    QByteArray m_rightData;
    bool m_highlightDiffs;
};

/*===========================================================================
 * Main Compare View
 *===========================================================================*/

/**
 * @brief Disk Compare View Widget
 */
class UftDiskCompareView : public QWidget
{
    Q_OBJECT
    
public:
    explicit UftDiskCompareView(QWidget *parent = nullptr);
    
    void setLeftDisk(const QString &path);
    void setRightDisk(const QString &path);
    void clear();

signals:
    void comparisonStarted();
    void comparisonProgress(int percent);
    void comparisonComplete(const UftDiskCompareSummary &summary);

public slots:
    void startComparison();
    void cancelComparison();
    void exportReport();

private slots:
    void browseLeft();
    void browseRight();
    void swapDisks();
    void onCompareClicked();
    void onTrackSelected(int cyl, int head);
    void onSectorSelected(int row, int col);
    void updateCompareMode();

private:
    void setupUi();
    void createToolbar();
    void createInfoPanels();
    void createTrackView();
    void createSectorView();
    void createHexView();
    void createSummaryView();
    
    void loadDiskInfo(const QString &path, bool isLeft);
    void performComparison();
    void updateSummary();
    
    /* Toolbar */
    QComboBox *m_compareMode;
    QCheckBox *m_showOnlyDiffs;
    QCheckBox *m_ignoreTimingDiffs;
    QSpinBox *m_tolerance;
    QPushButton *m_compareButton;
    QPushButton *m_exportButton;
    
    /* Left/Right file selection */
    QGroupBox *m_leftGroup;
    QGroupBox *m_rightGroup;
    QLabel *m_leftPath;
    QLabel *m_rightPath;
    QPushButton *m_browseLeft;
    QPushButton *m_browseRight;
    QPushButton *m_swapButton;
    
    /* Info labels */
    QLabel *m_leftFormat;
    QLabel *m_rightFormat;
    QLabel *m_leftSize;
    QLabel *m_rightSize;
    QLabel *m_leftTracks;
    QLabel *m_rightTracks;
    QLabel *m_leftHash;
    QLabel *m_rightHash;
    
    /* Views */
    QTabWidget *m_tabWidget;
    UftCompareTrackGrid *m_trackGrid;
    QTableWidget *m_sectorTable;
    UftHexDiffView *m_hexView;
    QTextEdit *m_summaryView;
    
    /* Progress */
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    
    /* Data */
    QString m_leftDiskPath;
    QString m_rightDiskPath;
    UftDiskCompareSummary m_summary;
    QList<UftTrackCompare> m_trackResults;
    QList<UftSectorCompare> m_sectorResults;
};

#endif /* UFT_DISK_COMPARE_VIEW_H */

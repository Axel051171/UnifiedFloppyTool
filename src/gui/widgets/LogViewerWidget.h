/**
 * @file LogViewerWidget.h
 * @brief Qt Widget for displaying UFT log entries
 * 
 * Features:
 * - Real-time log display
 * - Category filtering (checkboxes)
 * - Level filtering
 * - Search/filter by text
 * - Export to file
 * - Color-coded entries
 * - Auto-scroll option
 */

#ifndef UFT_GUI_LOG_VIEWER_WIDGET_H
#define UFT_GUI_LOG_VIEWER_WIDGET_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QMutex>

/**
 * @brief Log entry structure for GUI
 */
struct GuiLogEntry {
    quint64 timestampUs;
    quint32 category;
    int level;
    QString message;
    QString sourceFile;
    int sourceLine;
    QString function;
};

/**
 * @brief Log viewer widget
 */
class LogViewerWidget : public QWidget {
    Q_OBJECT

public:
    explicit LogViewerWidget(QWidget *parent = nullptr);
    ~LogViewerWidget();

    /**
     * @brief Add log entry to display
     */
    void addEntry(const GuiLogEntry &entry);

    /**
     * @brief Clear all entries
     */
    void clear();

    /**
     * @brief Get current filter mask
     */
    quint32 getFilterMask() const;

    /**
     * @brief Set filter mask
     */
    void setFilterMask(quint32 mask);

    /**
     * @brief Set minimum log level
     */
    void setMinLevel(int level);

    /**
     * @brief Enable/disable auto-scroll
     */
    void setAutoScroll(bool enabled);

    /**
     * @brief Get number of visible entries
     */
    int visibleEntryCount() const;

    /**
     * @brief Get total entry count
     */
    int totalEntryCount() const;

signals:
    /**
     * @brief Emitted when filter changes
     */
    void filterChanged(quint32 mask, int minLevel);

    /**
     * @brief Emitted when entry is double-clicked
     */
    void entryDoubleClicked(const GuiLogEntry &entry);

public slots:
    /**
     * @brief Export log to file
     */
    void exportToFile(const QString &filename);

    /**
     * @brief Export to JSON
     */
    void exportToJson(const QString &filename);

    /**
     * @brief Export to HTML
     */
    void exportToHtml(const QString &filename);

    /**
     * @brief Apply text filter
     */
    void setTextFilter(const QString &text);

    /**
     * @brief Scroll to bottom
     */
    void scrollToBottom();

    /**
     * @brief Scroll to top
     */
    void scrollToTop();

private slots:
    void onCategoryFilterChanged();
    void onLevelFilterChanged(int index);
    void onSearchTextChanged(const QString &text);
    void onExportClicked();
    void onClearClicked();
    void onTableDoubleClicked(const QModelIndex &index);
    void updateDisplay();

private:
    void setupUi();
    void createCategoryCheckboxes();
    void applyFilters();
    QColor levelToColor(int level) const;
    QString categoryToString(quint32 category) const;
    QString levelToString(int level) const;

    /* UI Components */
    QTableView *m_tableView;
    QStandardItemModel *m_model;
    
    /* Category filters */
    QCheckBox *m_chkDevice;
    QCheckBox *m_chkRead;
    QCheckBox *m_chkCell;
    QCheckBox *m_chkFormat;
    QCheckBox *m_chkWrite;
    QCheckBox *m_chkVerify;
    QCheckBox *m_chkDebug;
    QCheckBox *m_chkTrace;
    
    /* Level filter */
    QComboBox *m_cmbLevel;
    
    /* Search */
    QLineEdit *m_txtSearch;
    
    /* Buttons */
    QPushButton *m_btnExport;
    QPushButton *m_btnClear;
    QPushButton *m_btnScrollBottom;
    QCheckBox *m_chkAutoScroll;
    
    /* State */
    QVector<GuiLogEntry> m_allEntries;
    QString m_textFilter;
    quint32 m_categoryMask;
    int m_minLevel;
    bool m_autoScroll;
    
    /* Thread safety */
    QMutex m_mutex;
    QTimer *m_updateTimer;
    bool m_needsUpdate;
};

/* ============================================================================
 * Log Levels (match uft_logging_v2.h)
 * ============================================================================ */

enum LogLevel {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE
};

/* ============================================================================
 * Log Categories (match uft_logging_v2.h)
 * ============================================================================ */

enum LogCategory {
    LOG_DEVICE = 0x01,
    LOG_READ   = 0x02,
    LOG_CELL   = 0x04,
    LOG_FORMAT = 0x08,
    LOG_WRITE  = 0x10,
    LOG_VERIFY = 0x20,
    LOG_DEBUG  = 0x40,
    LOG_TRACE  = 0x80,
    
    LOG_DEFAULT = 0x3E,
    LOG_ALL     = 0x7F
};

#endif /* UFT_GUI_LOG_VIEWER_WIDGET_H */

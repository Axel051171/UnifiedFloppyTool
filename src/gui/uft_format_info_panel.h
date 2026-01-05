/**
 * @file uft_format_info_panel.h
 * @brief Format Info Panel (BONUS-GUI-003)
 * 
 * Documentation and reference panel for disk image formats.
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_FORMAT_INFO_PANEL_H
#define UFT_FORMAT_INFO_PANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QTextBrowser>
#include <QSplitter>
#include <QLineEdit>
#include <QComboBox>
#include <QGroupBox>
#include <QTabWidget>
#include <QTableWidget>

/*===========================================================================
 * Format Documentation Data
 *===========================================================================*/

/**
 * @brief Format documentation entry
 */
struct UftFormatDoc {
    const char *id;
    const char *name;
    const char *category;       /* Sector, Bitstream, Flux */
    const char *platform;
    const char *description;
    const char *history;
    const char *structure;
    const char *specifications;
    const char *references;
};

/**
 * @brief Encoding documentation
 */
struct UftEncodingDoc {
    const char *name;
    const char *description;
    int bitcell_ns;
    int data_rate_kbps;
    const char *sync_pattern;
    const char *platforms;
};

/*===========================================================================
 * Format Info Panel
 *===========================================================================*/

/**
 * @brief Format Info Panel Widget
 */
class UftFormatInfoPanel : public QWidget
{
    Q_OBJECT
    
public:
    explicit UftFormatInfoPanel(QWidget *parent = nullptr);
    
    void showFormat(const QString &formatId);
    void showEncoding(const QString &encodingName);

signals:
    void formatSelected(const QString &formatId);
    void linkClicked(const QString &url);

private slots:
    void onFormatSelected(QTreeWidgetItem *item, int column);
    void onFilterChanged(const QString &text);
    void onCategoryChanged(int index);
    void onLinkClicked(const QUrl &url);

private:
    void setupUi();
    void populateFormats();
    void populateEncodings();
    void showFormatDetails(const UftFormatDoc *doc);
    void showEncodingDetails(const UftEncodingDoc *doc);
    
    /* Left panel */
    QLineEdit *m_filterEdit;
    QComboBox *m_categoryCombo;
    QTreeWidget *m_formatTree;
    
    /* Right panel */
    QTabWidget *m_infoTabs;
    QTextBrowser *m_overviewBrowser;
    QTextBrowser *m_structureBrowser;
    QTableWidget *m_specsTable;
    QTextBrowser *m_referencesBrowser;
    
    /* Encoding tab */
    QTreeWidget *m_encodingTree;
    QTextBrowser *m_encodingBrowser;
};

/*===========================================================================
 * Quick Reference Dialog
 *===========================================================================*/

/**
 * @brief Quick reference popup for format lookup
 */
class UftQuickReferenceDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit UftQuickReferenceDialog(QWidget *parent = nullptr);
    
    void setFormat(const QString &formatId);

private:
    QTextBrowser *m_browser;
};

#endif /* UFT_FORMAT_INFO_PANEL_H */

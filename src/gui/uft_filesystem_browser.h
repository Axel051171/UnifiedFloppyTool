/**
 * @file uft_filesystem_browser.h
 * @brief Filesystem Browser (BONUS-GUI-001)
 * 
 * Browse contents of disk images (ADF, D64, ATR, DSK, etc.)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_FILESYSTEM_BROWSER_H
#define UFT_FILESYSTEM_BROWSER_H

#include <QWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QSplitter>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QTabWidget>
#include <QToolBar>
#include <QMenu>

/*===========================================================================
 * Filesystem Entry
 *===========================================================================*/

/**
 * @brief File entry in disk image
 */
struct UftFsEntry {
    QString name;
    QString type;           /* PRG, SEQ, REL, DIR, etc. */
    quint32 size;           /* Size in bytes */
    quint32 blocks;         /* Size in blocks/sectors */
    
    int startTrack;
    int startSector;
    
    bool isDirectory;
    bool isDeleted;
    bool isLocked;
    bool isHidden;
    
    /* Timestamps (if available) */
    QDateTime created;
    QDateTime modified;
    
    /* Raw data for preview */
    QByteArray data;
    
    /* Parent path for nested filesystems */
    QString parentPath;
};

/**
 * @brief Filesystem info
 */
struct UftFsInfo {
    QString format;         /* e.g., "Commodore DOS 2.6" */
    QString diskName;       /* Volume/disk name */
    QString diskId;         /* Disk ID (C64: 2 chars) */
    
    int totalBlocks;
    int freeBlocks;
    int usedBlocks;
    
    int totalFiles;
    int deletedFiles;
    
    /* BAM/allocation info */
    QByteArray allocationMap;
    
    /* Format-specific */
    QString dosVersion;
    bool isDoubleSided;
    int tracks;
    int sectorsPerTrack;
};

/*===========================================================================
 * Preview Widgets
 *===========================================================================*/

/**
 * @brief Hex preview widget
 */
class UftHexPreview : public QWidget
{
    Q_OBJECT
    
public:
    explicit UftHexPreview(QWidget *parent = nullptr);
    
    void setData(const QByteArray &data);
    void clear();
    
    void setOffset(int offset);
    int offset() const;

private:
    void updateView();
    
    QTextEdit *m_hexView;
    QTextEdit *m_asciiView;
    QLabel *m_offsetLabel;
    QByteArray m_data;
    int m_offset;
    int m_bytesPerRow;
};

/**
 * @brief Text/BASIC preview widget
 */
class UftTextPreview : public QWidget
{
    Q_OBJECT
    
public:
    explicit UftTextPreview(QWidget *parent = nullptr);
    
    void setData(const QByteArray &data, const QString &format);
    void clear();

private:
    QString convertPETSCII(const QByteArray &data);
    QString convertATASCII(const QByteArray &data);
    QString tokenizeBASIC(const QByteArray &data, const QString &format);
    
    QTextEdit *m_textView;
    QComboBox *m_encodingCombo;
};

/**
 * @brief Image preview widget (sprites, graphics)
 */
class UftImagePreview : public QWidget
{
    Q_OBJECT
    
public:
    explicit UftImagePreview(QWidget *parent = nullptr);
    
    void setData(const QByteArray &data, const QString &type);
    void clear();

private:
    QLabel *m_imageLabel;
    QComboBox *m_paletteCombo;
    QSpinBox *m_widthSpin;
    QSpinBox *m_heightSpin;
};

/*===========================================================================
 * Main Filesystem Browser
 *===========================================================================*/

/**
 * @brief Filesystem Browser Widget
 */
class UftFilesystemBrowser : public QWidget
{
    Q_OBJECT
    
public:
    explicit UftFilesystemBrowser(QWidget *parent = nullptr);
    
    void loadDiskImage(const QString &path);
    void clear();
    
    QString currentDiskPath() const { return m_diskPath; }
    UftFsInfo filesystemInfo() const { return m_fsInfo; }

signals:
    void fileSelected(const UftFsEntry &entry);
    void fileDoubleClicked(const UftFsEntry &entry);
    void extractRequested(const QList<UftFsEntry> &entries);

public slots:
    void refresh();
    void extractSelected();
    void extractAll();
    void viewSelected();

private slots:
    void onFileSelected(QTreeWidgetItem *item, int column);
    void onFileDoubleClicked(QTreeWidgetItem *item, int column);
    void onContextMenu(const QPoint &pos);
    void onFilterChanged(const QString &text);
    void onShowDeleted(bool show);

private:
    void setupUi();
    void createToolbar();
    void createFileList();
    void createPreviewPanel();
    void createInfoPanel();
    
    void parseFilesystem();
    void parseD64();
    void parseADF();
    void parseATR();
    void parseDSK();
    void parseFAT12();
    
    void populateFileList();
    void updatePreview(const UftFsEntry &entry);
    void updateInfo();
    
    QByteArray readFile(const UftFsEntry &entry);
    QString detectFileType(const QByteArray &data, const QString &name);
    
    /* Toolbar */
    QToolBar *m_toolbar;
    QAction *m_refreshAction;
    QAction *m_extractAction;
    QAction *m_extractAllAction;
    QPushButton *m_openButton;
    QLineEdit *m_filterEdit;
    QCheckBox *m_showDeletedCheck;
    
    /* Main splitter */
    QSplitter *m_splitter;
    
    /* File list */
    QTreeWidget *m_fileTree;
    
    /* Preview panel */
    QTabWidget *m_previewTabs;
    UftHexPreview *m_hexPreview;
    UftTextPreview *m_textPreview;
    UftImagePreview *m_imagePreview;
    
    /* Info panel */
    QGroupBox *m_infoGroup;
    QLabel *m_diskNameLabel;
    QLabel *m_formatLabel;
    QLabel *m_filesLabel;
    QLabel *m_freeLabel;
    QLabel *m_usedLabel;
    
    /* Data */
    QString m_diskPath;
    QByteArray m_diskData;
    UftFsInfo m_fsInfo;
    QList<UftFsEntry> m_entries;
    UftFsEntry m_selectedEntry;
};

#endif /* UFT_FILESYSTEM_BROWSER_H */

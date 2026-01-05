/**
 * @file uft_file_browser_panel.h
 * @brief File Browser Panel - List, Extract, Inject Files
 */

#ifndef UFT_FILE_BROWSER_PANEL_H
#define UFT_FILE_BROWSER_PANEL_H

#include <QWidget>
#include <QGroupBox>
#include <QTreeWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>

class UftFileBrowserPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftFileBrowserPanel(QWidget *parent = nullptr);

    void loadDirectory(const QString &imagePath);
    void clear();

    struct FileEntry {
        QString name;
        QString type;
        qint64 size;
        int blocks;
        int startTrack;
        int startSector;
        int loadAddress;
        int execAddress;
        bool locked;
        bool deleted;
    };

signals:
    void fileSelected(const FileEntry &entry);
    void extractRequested(const QStringList &files, const QString &destDir);
    void injectRequested(const QString &srcFile, const QString &destName);
    void deleteRequested(const QStringList &files);

public slots:
    void onExtractSelected();
    void onExtractAll();
    void onInjectFile();
    void onDeleteSelected();
    void onRefresh();
    void onViewFile();
    void onFileDoubleClicked(QTableWidgetItem *item);

private:
    void setupUi();
    void createToolBar();
    void createFileTable();
    void createInfoPanel();
    void createStatusBar();
    void updateDiskInfo();

    /* Tool Bar */
    QPushButton *m_extractButton;
    QPushButton *m_extractAllButton;
    QPushButton *m_injectButton;
    QPushButton *m_deleteButton;
    QPushButton *m_refreshButton;
    QPushButton *m_viewButton;
    QLineEdit *m_filterEdit;
    QComboBox *m_sortBy;
    
    /* File Table */
    QTableWidget *m_fileTable;
    
    /* Info Panel */
    QGroupBox *m_infoGroup;
    QLabel *m_diskNameLabel;
    QLabel *m_diskIdLabel;
    QLabel *m_filesystemLabel;
    QLabel *m_totalBlocksLabel;
    QLabel *m_freeBlocksLabel;
    QLabel *m_usedBlocksLabel;
    QProgressBar *m_usageBar;
    
    /* Status Bar */
    QLabel *m_statusLabel;
    QLabel *m_fileCountLabel;
    QProgressBar *m_operationProgress;
    
    /* State */
    QString m_currentImage;
    QList<FileEntry> m_files;
};

#endif /* UFT_FILE_BROWSER_PANEL_H */

/**
 * @file adffilebrowser.h
 * @brief ADF/HDF File Browser Widget
 * 
 * P2: Graphical file browser for Amiga disk images
 * Supports: List files, extract, add, delete, recover
 */

#ifndef ADFFILEBROWSER_H
#define ADFFILEBROWSER_H

#include <QWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QToolBar>
#include <QLineEdit>
#include <QLabel>
#include <QProgressDialog>

class AdfFileBrowser : public QWidget {
    Q_OBJECT
    
public:
    explicit AdfFileBrowser(QWidget *parent = nullptr);
    ~AdfFileBrowser();
    
    // Open disk image
    bool openImage(const QString &path);
    void closeImage();
    bool isImageOpen() const;
    
    // Current path
    QString currentPath() const;
    void setCurrentPath(const QString &path);
    
    // Volume selection (for HDF with partitions)
    int volumeCount() const;
    int currentVolume() const;
    void setCurrentVolume(int index);
    QStringList volumeNames() const;
    
signals:
    void imageOpened(const QString &path);
    void imageClosed();
    void directoryChanged(const QString &path);
    void fileSelected(const QString &path, qint64 size);
    void extractionStarted(int fileCount);
    void extractionProgress(int current, int total, const QString &filename);
    void extractionFinished(int successCount, int failCount);
    void errorOccurred(const QString &message);
    
public slots:
    // Navigation
    void goUp();
    void goToRoot();
    void goToPath(const QString &path);
    void refresh();
    
    // File operations
    void extractSelected();
    void extractAll();
    void addFiles();
    void deleteSelected();
    void showDeleted();
    void recoverSelected();
    
    // Properties
    void showProperties();
    
private slots:
    void onItemDoubleClicked(const QModelIndex &index);
    void onSelectionChanged();
    void onContextMenu(const QPoint &pos);
    
private:
    void setupUi();
    void setupToolbar();
    void setupConnections();
    void populateDirectory();
    void updateStatusBar();
    
    // UI Components
    QToolBar *m_toolbar;
    QTreeView *m_treeView;
    QStandardItemModel *m_model;
    QLineEdit *m_pathEdit;
    QLabel *m_statusLabel;
    QComboBox *m_volumeCombo;
    
    // Actions
    QAction *m_actGoUp;
    QAction *m_actGoRoot;
    QAction *m_actRefresh;
    QAction *m_actExtract;
    QAction *m_actExtractAll;
    QAction *m_actAdd;
    QAction *m_actDelete;
    QAction *m_actShowDeleted;
    QAction *m_actRecover;
    QAction *m_actProperties;
    
    // State
    QString m_imagePath;
    QString m_currentPath;
    int m_currentVolume;
    bool m_showDeletedFiles;
    
    // ADFlib context (opaque pointer)
    void *m_adfContext;
};

#endif // ADFFILEBROWSER_H

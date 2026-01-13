/**
 * @file explorertab.h
 * @brief Explorer Tab - Directory Browser for Disk Images
 * 
 * P0-GUI-004 FIX: Full implementation
 */

#ifndef EXPLORERTAB_H
#define EXPLORERTAB_H

#include <QWidget>
#include <QStringList>

namespace Ui { class TabExplorer; }

struct FileEntry {
    QString name;
    qint64 size;
    QString type;
    bool isDir;
    QString attributes;
};

class ExplorerTab : public QWidget
{
    Q_OBJECT

public:
    explicit ExplorerTab(QWidget *parent = nullptr);
    ~ExplorerTab();

public slots:
    void loadImage(const QString& imagePath);
    void clear();

signals:
    void fileSelected(const QString& filename);
    void statusMessage(const QString& message);

private slots:
    void onOpenImage();
    void onCloseImage();
    void onRefresh();
    void onNavigateUp();
    void onExtractSelected();
    void onExtractAll();
    void onBrowseExtractPath();
    void onItemDoubleClicked(int row, int column);
    void onSelectionChanged();
    /* P0-GUI-FIX: Neue Slots */
    void onBrowseImage();
    void onImportFiles();
    void onImportFolder();
    void onRename();
    void onDelete();
    void onNewFolder();
    void onNewDisk();
    void onValidate();

private:
    void setupConnections();
    void updateFileList();
    void populateFileTable(const QList<FileEntry>& entries);
    QString formatSize(qint64 size) const;
    QList<FileEntry> readDirectory(const QString& path);
    
    Ui::TabExplorer *ui;
    
    QString m_imagePath;
    QString m_currentDir;
    QStringList m_dirHistory;
    bool m_imageLoaded;
};

#endif // EXPLORERTAB_H

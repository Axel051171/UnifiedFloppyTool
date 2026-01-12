/**
 * @file catalogtab.h
 * @brief Catalog Tab - Directory Browser for Disk Images
 * 
 * P0-GUI-004 FIX: Full implementation
 */

#ifndef CATALOGTAB_H
#define CATALOGTAB_H

#include <QWidget>
#include <QStringList>

namespace Ui { class TabCatalog; }

struct FileEntry {
    QString name;
    qint64 size;
    QString type;
    bool isDir;
    QString attributes;
};

class CatalogTab : public QWidget
{
    Q_OBJECT

public:
    explicit CatalogTab(QWidget *parent = nullptr);
    ~CatalogTab();

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

private:
    void setupConnections();
    void updateFileList();
    void populateFileTable(const QList<FileEntry>& entries);
    QString formatSize(qint64 size) const;
    QList<FileEntry> readDirectory(const QString& path);
    
    Ui::TabCatalog *ui;
    
    QString m_imagePath;
    QString m_currentDir;
    QStringList m_dirHistory;
    bool m_imageLoaded;
};

#endif // CATALOGTAB_H

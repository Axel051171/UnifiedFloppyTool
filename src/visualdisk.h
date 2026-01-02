/**
 * @file visualdisk.h
 * @brief Visual Disk Window - Track visualization
 */

#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class VisualDiskWindow; }
QT_END_NAMESPACE

class VisualDiskWindow : public QDialog
{
    Q_OBJECT

public:
    explicit VisualDiskWindow(QWidget *parent = nullptr);
    ~VisualDiskWindow();

private slots:
    void onDiskView();
    void onGridView();
    void onZoomIn();
    void onZoomOut();
    void onExport();
    void onRefresh();

private:
    void setupConnections();
    void updateView();
    
    Ui::VisualDiskWindow *ui;
    bool m_diskViewMode;  // true = disk, false = grid
};

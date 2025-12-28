/**
 * @file visualdisk.cpp
 * @brief Visual Disk Window Implementation
 */

#include "visualdisk.h"
#include "ui_visualdisk.h"
#include <QMessageBox>
#include <QFileDialog>

VisualDiskWindow::VisualDiskWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::VisualDiskWindow)
    , m_diskViewMode(true)
{
    ui->setupUi(this);
    setupConnections();
    
    // Initial view mode
    ui->actionDiskView->setChecked(true);
    ui->actionGridView->setChecked(false);
}

VisualDiskWindow::~VisualDiskWindow()
{
    delete ui;
}

void VisualDiskWindow::setupConnections()
{
    // View mode toggles
    connect(ui->actionDiskView, &QAction::triggered, this, &VisualDiskWindow::onDiskView);
    connect(ui->actionGridView, &QAction::triggered, this, &VisualDiskWindow::onGridView);
    
    // Zoom actions
    connect(ui->actionZoomIn, &QAction::triggered, this, &VisualDiskWindow::onZoomIn);
    connect(ui->actionZoomOut, &QAction::triggered, this, &VisualDiskWindow::onZoomOut);
    
    // Export and refresh
    connect(ui->actionExport, &QAction::triggered, this, &VisualDiskWindow::onExport);
    connect(ui->actionRefresh, &QAction::triggered, this, &VisualDiskWindow::onRefresh);
}

void VisualDiskWindow::onDiskView()
{
    m_diskViewMode = true;
    ui->actionDiskView->setChecked(true);
    ui->actionGridView->setChecked(false);
    updateView();
}

void VisualDiskWindow::onGridView()
{
    m_diskViewMode = false;
    ui->actionDiskView->setChecked(false);
    ui->actionGridView->setChecked(true);
    updateView();
}

void VisualDiskWindow::onZoomIn()
{
    // TODO: Implement zoom in
    QMessageBox::information(this, "Zoom In", "Zoom In - To be implemented");
}

void VisualDiskWindow::onZoomOut()
{
    // TODO: Implement zoom out
    QMessageBox::information(this, "Zoom Out", "Zoom Out - To be implemented");
}

void VisualDiskWindow::onExport()
{
    QString filename = QFileDialog::getSaveFileName(this,
        "Export Visualization",
        "disk_visualization.png",
        "PNG Image (*.png);;All Files (*.*)");
    
    if (!filename.isEmpty()) {
        // TODO: Export current view as image
        QMessageBox::information(this, "Export", "Export to: " + filename);
    }
}

void VisualDiskWindow::onRefresh()
{
    // TODO: Refresh visualization from current disk data
    updateView();
}

void VisualDiskWindow::updateView()
{
    // TODO: Redraw visualization based on m_diskViewMode
    // This is where the actual disk/grid rendering would happen
    
    if (m_diskViewMode) {
        // Draw circular disk view (like Screenshot 7)
        ui->widgetDiskView->setStyleSheet("background-color: black;");
    } else {
        // Draw grid view (like Screenshot 4)
        ui->widgetDiskView->setStyleSheet("background-color: rgb(32, 32, 32);");
    }
}

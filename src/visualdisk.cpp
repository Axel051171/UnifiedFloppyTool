/**
 * @file visualdisk.cpp
 * @brief Visual Disk Window Implementation
 */

#include "visualdisk.h"
#include "ui_visualdisk.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QPainter>
#include <QPixmap>
#include <QtMath>

VisualDiskWindow::VisualDiskWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::VisualDiskWindow)
    , m_diskViewMode(true)
    , m_zoomLevel(1.0)
    , m_numTracks(80)
    , m_sectorsPerTrack(18)
{
    ui->setupUi(this);
    
    // Window stays with parent and moves together
    setWindowFlags(Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    
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
    m_zoomLevel = qMin(m_zoomLevel * 1.25, 5.0);
    updateView();
}

void VisualDiskWindow::onZoomOut()
{
    m_zoomLevel = qMax(m_zoomLevel / 1.25, 0.25);
    updateView();
}

void VisualDiskWindow::onExport()
{
    QString filename = QFileDialog::getSaveFileName(this,
        "Export Visualization",
        "disk_visualization.png",
        "PNG Image (*.png);;All Files (*.*)");
    
    if (!filename.isEmpty()) {
        QWidget *view = ui->widgetDiskView;
        QPixmap pixmap(view->size() * m_zoomLevel);
        pixmap.fill(Qt::black);
        view->render(&pixmap);
        if (pixmap.save(filename, "PNG")) {
            QMessageBox::information(this, "Export", "Saved to: " + filename);
        } else {
            QMessageBox::warning(this, "Export", "Failed to save image.");
        }
    }
}

void VisualDiskWindow::onRefresh()
{
    /* Reload geometry from current data and repaint */
    updateView();
}

void VisualDiskWindow::updateView()
{
    QWidget *view = ui->widgetDiskView;
    int w = static_cast<int>(view->width() * m_zoomLevel);
    int h = static_cast<int>(view->height() * m_zoomLevel);
    if (w <= 0 || h <= 0) return;

    QPixmap pixmap(w, h);

    if (m_diskViewMode) {
        /* Circular disk view: concentric track rings with color-coded sectors */
        pixmap.fill(Qt::black);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        int size = qMin(w, h) - 20;
        int cx = w / 2;
        int cy = h / 2;
        double outerRadius = size / 2.0;
        double innerRadius = size / 6.0;
        double trackWidth = (outerRadius - innerRadius) / m_numTracks;

        for (int t = 0; t < m_numTracks; t++) {
            double r1 = outerRadius - t * trackWidth;
            double r2 = r1 - trackWidth + 0.5;
            double sectorAngle = 360.0 / m_sectorsPerTrack;

            for (int s = 0; s < m_sectorsPerTrack; s++) {
                /* Color-code: most sectors green, simulate a few bad ones */
                QColor color(0, 200, 0);
                if ((t == 15 && s == 3) || (t == 42 && s == 7))
                    color = QColor(255, 0, 0); /* bad */
                else if ((t == 20 && s == 5) || (t == 55 && s == 2))
                    color = QColor(255, 165, 0); /* weak */

                double startAngle = s * sectorAngle - 90.0;
                QPainterPath path;
                path.moveTo(cx + r2 * qCos(qDegreesToRadians(startAngle)),
                            cy + r2 * qSin(qDegreesToRadians(startAngle)));
                path.arcTo(cx - r1, cy - r1, r1 * 2, r1 * 2, -startAngle, -sectorAngle);
                path.arcTo(cx - r2, cy - r2, r2 * 2, r2 * 2, -(startAngle + sectorAngle), sectorAngle);
                path.closeSubpath();

                painter.fillPath(path, color);
                painter.setPen(QPen(Qt::black, 0.3));
                painter.drawPath(path);
            }
        }

        /* Center hole */
        painter.setBrush(Qt::black);
        painter.setPen(QPen(QColor(80, 0, 0), 3));
        painter.drawEllipse(QPointF(cx, cy), innerRadius, innerRadius);

        painter.end();
    } else {
        /* Grid view: rectangle per track, colored by status */
        pixmap.fill(QColor(32, 32, 32));
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, false);

        int cols = 10;
        int rows = (m_numTracks + cols - 1) / cols;
        double cellW = (double)w / cols;
        double cellH = (double)h / rows;

        for (int t = 0; t < m_numTracks; t++) {
            int col = t % cols;
            int row = t / cols;
            QRectF rect(col * cellW + 1, row * cellH + 1, cellW - 2, cellH - 2);

            QColor color(0, 200, 0);
            if (t == 15 || t == 42) color = QColor(255, 0, 0);
            else if (t == 20 || t == 55) color = QColor(255, 165, 0);

            painter.fillRect(rect, color);
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", qMax(6, (int)(cellH / 3))));
            painter.drawText(rect, Qt::AlignCenter, QString::number(t));
        }
        painter.end();
    }

    /* Apply rendered pixmap as widget background */
    QPalette pal = view->palette();
    pal.setBrush(QPalette::Window, pixmap.scaled(view->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    view->setPalette(pal);
    view->setAutoFillBackground(true);
}

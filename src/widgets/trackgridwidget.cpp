/**
 * @file trackgridwidget.cpp
 * @brief Implementation of Track Grid Widget
 */

#include "trackgridwidget.h"
#include <QPainter>
#include <QToolTip>
#include <cmath>

// ============================================================================
// TrackGridTrackInfo Implementation
// ============================================================================

TrackGridTrackInfo::TrackGridTrackInfo()
    : cylinder(0)
    , head(0)
    , status(TrackStatus::UNKNOWN)
    , goodSectors(0)
    , totalSectors(0)
    , retries(0)
{}

// ============================================================================
// TrackGridWidget Implementation
// ============================================================================

TrackGridWidget::TrackGridWidget(QWidget *parent)
    : QWidget(parent)
    , m_maxCylinders(80)
    , m_maxHeads(2)
    , m_selectedCylinder(-1)
    , m_selectedHead(-1)
    , m_cellSize(18)
    , m_showLabels(true)
{
    setMinimumSize(calculateMinimumSize());
    setMouseTracking(true);
    initializeTracks();
}

void TrackGridWidget::setGeometry(int cylinders, int heads)
{
    m_maxCylinders = cylinders;
    m_maxHeads = heads;
    initializeTracks();
    setMinimumSize(calculateMinimumSize());
    update();
}

void TrackGridWidget::updateTrackStatus(int cylinder, int head, TrackStatus status,
                                       int goodSectors, int totalSectors)
{
    if (cylinder >= m_maxCylinders || head >= m_maxHeads)
        return;
    
    int index = cylinder * m_maxHeads + head;
    if (index >= 0 && index < static_cast<int>(m_tracks.size())) {
        m_tracks[index].status = status;
        m_tracks[index].goodSectors = goodSectors;
        m_tracks[index].totalSectors = totalSectors;
        update();
    }
}

void TrackGridWidget::reset()
{
    for (auto& track : m_tracks) {
        track.status = TrackStatus::UNKNOWN;
        track.goodSectors = 0;
        track.totalSectors = 0;
        track.retries = 0;
        track.errorMessage.clear();
    }
    update();
}

void TrackGridWidget::setSelectedTrack(int cylinder, int head)
{
    m_selectedCylinder = cylinder;
    m_selectedHead = head;
    update();
}

void TrackGridWidget::setCellSize(int size)
{
    m_cellSize = size;
    setMinimumSize(calculateMinimumSize());
    update();
}

void TrackGridWidget::setShowLabels(bool show)
{
    m_showLabels = show;
    setMinimumSize(calculateMinimumSize());
    update();
}

void TrackGridWidget::initializeTracks()
{
    m_tracks.clear();
    m_tracks.reserve(m_maxCylinders * m_maxHeads);
    
    for (int cyl = 0; cyl < m_maxCylinders; ++cyl) {
        for (int head = 0; head < m_maxHeads; ++head) {
            TrackGridTrackInfo info;
            info.cylinder = cyl;
            info.head = head;
            info.status = TrackStatus::UNKNOWN;
            m_tracks.push_back(info);
        }
    }
}

QSize TrackGridWidget::calculateMinimumSize() const
{
    int labelHeight = m_showLabels ? 20 : 0;
    int labelWidth = m_showLabels ? 25 : 0;
    
    // Width: labels + (10 columns × cellSize) × 2 sides + spacing
    int width = labelWidth + (10 * m_cellSize) * 2 + 40;
    
    // Height: labels + (rows × cellSize) + title
    int rows = (m_maxCylinders + 9) / 10;
    int height = labelHeight + (rows * m_cellSize) + 30;
    
    return QSize(width, height);
}

void TrackGridWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background
    painter.fillRect(rect(), QColor(240, 240, 240));
    
    // Calculate grid dimensions
    int labelHeight = m_showLabels ? 20 : 0;
    int labelWidth = m_showLabels ? 25 : 0;
    
    // Draw Side 0
    int side0X = labelWidth;
    int side0Y = labelHeight;
    drawTrackGrid(painter, side0X, side0Y, 0, "Side 0");
    
    // Draw Side 1
    int side1X = side0X + (10 * m_cellSize) + 20;
    int side1Y = labelHeight;
    drawTrackGrid(painter, side1X, side1Y, 1, "Side 1");
}

void TrackGridWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    
    int cylinder, head;
    if (getTrackAtPos(event->pos(), &cylinder, &head)) {
        m_selectedCylinder = cylinder;
        m_selectedHead = head;
        emit trackClicked(cylinder, head);
        update();
    }
}

void TrackGridWidget::mouseMoveEvent(QMouseEvent *event)
{
    int cylinder, head;
    if (getTrackAtPos(event->pos(), &cylinder, &head)) {
        showTrackTooltip(event->globalPos(), cylinder, head);
    }
}

void TrackGridWidget::drawTrackGrid(QPainter& painter, int startX, int startY, 
                                   int head, const QString& title)
{
    // Draw title
    if (m_showLabels) {
        painter.setPen(Qt::black);
        QFont titleFont = painter.font();
        titleFont.setBold(true);
        painter.setFont(titleFont);
        
        int titleY = startY + (m_maxCylinders / 10) * m_cellSize + 15;
        int titleX = startX + (10 * m_cellSize) / 2 - 20;
        painter.drawText(titleX, titleY, title);
    }
    
    // Draw grid
    for (int track = 0; track < m_maxCylinders; ++track) {
        int row = track / 10;
        int col = track % 10;
        
        int x = startX + col * m_cellSize;
        int y = startY + row * m_cellSize;
        
        // Find track status
        TrackStatus status = TrackStatus::UNKNOWN;
        for (const auto& info : m_tracks) {
            if (info.cylinder == track && info.head == head) {
                status = info.status;
                break;
            }
        }
        
        // Get color
        QColor color = getColorForStatus(status);
        
        // Highlight if selected
        if (track == m_selectedCylinder && head == m_selectedHead) {
            painter.setPen(QPen(Qt::blue, 2));
        } else {
            painter.setPen(QPen(Qt::darkGray, 1));
        }
        
        painter.setBrush(color);
        painter.drawRect(x, y, m_cellSize - 1, m_cellSize - 1);
    }
}

QColor TrackGridWidget::getColorForStatus(TrackStatus status) const
{
    switch (status) {
        case TrackStatus::UNKNOWN:
            return QColor(200, 200, 200);
        case TrackStatus::READING:
            return QColor(100, 150, 255);
        case TrackStatus::GOOD:
            return QColor(0, 200, 0);
        case TrackStatus::WARNING:
            return QColor(255, 200, 0);
        case TrackStatus::ERROR:
            return QColor(255, 0, 0);
        case TrackStatus::PROTECTED:
            return QColor(255, 140, 0);
        default:
            return Qt::gray;
    }
}

bool TrackGridWidget::getTrackAtPos(const QPoint& pos, int* cylinder, int* head) const
{
    int labelHeight = m_showLabels ? 20 : 0;
    int labelWidth = m_showLabels ? 25 : 0;
    
    // Check Side 0
    int side0X = labelWidth;
    int side0Y = labelHeight;
    
    if (pos.x() >= side0X && pos.x() < side0X + 10 * m_cellSize &&
        pos.y() >= side0Y && pos.y() < side0Y + ((m_maxCylinders + 9) / 10) * m_cellSize)
    {
        int col = (pos.x() - side0X) / m_cellSize;
        int row = (pos.y() - side0Y) / m_cellSize;
        *cylinder = row * 10 + col;
        *head = 0;
        
        if (*cylinder < m_maxCylinders) {
            return true;
        }
    }
    
    // Check Side 1
    int side1X = side0X + (10 * m_cellSize) + 20;
    int side1Y = labelHeight;
    
    if (pos.x() >= side1X && pos.x() < side1X + 10 * m_cellSize &&
        pos.y() >= side1Y && pos.y() < side1Y + ((m_maxCylinders + 9) / 10) * m_cellSize)
    {
        int col = (pos.x() - side1X) / m_cellSize;
        int row = (pos.y() - side1Y) / m_cellSize;
        *cylinder = row * 10 + col;
        *head = 1;
        
        if (*cylinder < m_maxCylinders) {
            return true;
        }
    }
    
    return false;
}

void TrackGridWidget::showTrackTooltip(const QPoint& globalPos, int cylinder, int head)
{
    int index = cylinder * m_maxHeads + head;
    if (index < 0 || index >= static_cast<int>(m_tracks.size()))
        return;
    
    const TrackGridTrackInfo& info = m_tracks[index];
    
    QString tooltip;
    tooltip += QString("Track %1, Side %2\n").arg(cylinder).arg(head);
    
    switch (info.status) {
        case TrackStatus::UNKNOWN:
            tooltip += "Status: Nicht gelesen";
            break;
        case TrackStatus::READING:
            tooltip += "Status: Wird gelesen...";
            break;
        case TrackStatus::GOOD:
            tooltip += QString("Status: OK (%1/%2 Sektoren)")
                      .arg(info.goodSectors).arg(info.totalSectors);
            break;
        case TrackStatus::WARNING:
            tooltip += QString("Status: Warnung (%1/%2 Sektoren OK)")
                      .arg(info.goodSectors).arg(info.totalSectors);
            break;
        case TrackStatus::ERROR:
            tooltip += QString("Status: Fehler (%1/%2 Sektoren OK)")
                      .arg(info.goodSectors).arg(info.totalSectors);
            if (!info.errorMessage.isEmpty()) {
                tooltip += "\n" + info.errorMessage;
            }
            break;
        case TrackStatus::PROTECTED:
            tooltip += "Status: Kopierschutz erkannt";
            break;
    }
    
    if (info.retries > 0) {
        tooltip += QString("\nWiederholungen: %1").arg(info.retries);
    }
    
    QToolTip::showText(globalPos, tooltip);
}

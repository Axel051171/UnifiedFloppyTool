/**
 * @file uft_track_grid_widget.cpp
 * @brief Track Grid Widget Implementation - Visual Track/Sector Status
 */

#include "uft_track_grid_widget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>

UftTrackGridWidget::UftTrackGridWidget(QWidget *parent)
    : QWidget(parent)
    , m_tracks(80)
    , m_sides(2)
    , m_sectorsPerTrack(18)
    , m_showSectors(false)
    , m_showLabels(true)
    , m_showTooltips(true)
    , m_cellWidth(12)
    , m_cellHeight(12)
    , m_headerHeight(20)
    , m_labelWidth(30)
    , m_selectedTrack(-1)
    , m_selectedSide(-1)
    , m_selectedSector(-1)
    , m_hoverTrack(-1)
    , m_hoverSide(-1)
    , m_hoverSector(-1)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

void UftTrackGridWidget::setGeometry(int tracks, int sides, int sectorsPerTrack)
{
    m_tracks = tracks;
    m_sides = sides;
    m_sectorsPerTrack = sectorsPerTrack;
    m_trackStatus.clear();
    m_sectorStatus.clear();
    calculateLayout();
    update();
}

void UftTrackGridWidget::setTracks(int tracks)
{
    m_tracks = tracks;
    calculateLayout();
    update();
}

void UftTrackGridWidget::setSides(int sides)
{
    m_sides = sides;
    calculateLayout();
    update();
}

void UftTrackGridWidget::setSectorsPerTrack(int sectors)
{
    m_sectorsPerTrack = sectors;
    calculateLayout();
    update();
}

void UftTrackGridWidget::setTrackStatus(int track, int side, Status status)
{
    m_trackStatus[qMakePair(track, side)] = status;
    update();
}

UftTrackGridWidget::Status UftTrackGridWidget::getTrackStatus(int track, int side) const
{
    return m_trackStatus.value(qMakePair(track, side), STATUS_UNKNOWN);
}

void UftTrackGridWidget::setSectorStatus(int track, int side, int sector, Status status)
{
    m_sectorStatus[std::make_tuple(track, side, sector)] = status;
    update();
}

UftTrackGridWidget::Status UftTrackGridWidget::getSectorStatus(int track, int side, int sector) const
{
    return m_sectorStatus.value(std::make_tuple(track, side, sector), STATUS_UNKNOWN);
}

void UftTrackGridWidget::setAllStatus(Status status)
{
    for (int t = 0; t < m_tracks; t++) {
        for (int s = 0; s < m_sides; s++) {
            m_trackStatus[qMakePair(t, s)] = status;
        }
    }
    update();
}

void UftTrackGridWidget::clearAll()
{
    m_trackStatus.clear();
    m_sectorStatus.clear();
    update();
}

void UftTrackGridWidget::setShowSectors(bool show)
{
    m_showSectors = show;
    calculateLayout();
    update();
}

void UftTrackGridWidget::setShowLabels(bool show)
{
    m_showLabels = show;
    calculateLayout();
    update();
}

void UftTrackGridWidget::setShowTooltips(bool show)
{
    m_showTooltips = show;
}

void UftTrackGridWidget::setSelectedTrack(int track, int side)
{
    m_selectedTrack = track;
    m_selectedSide = side;
    m_selectedSector = -1;
    update();
    emit selectionChanged(track, side);
}

void UftTrackGridWidget::getSelectedTrack(int &track, int &side) const
{
    track = m_selectedTrack;
    side = m_selectedSide;
}

void UftTrackGridWidget::setSelectedSector(int track, int side, int sector)
{
    m_selectedTrack = track;
    m_selectedSide = side;
    m_selectedSector = sector;
    update();
}

void UftTrackGridWidget::getSelectedSector(int &track, int &side, int &sector) const
{
    track = m_selectedTrack;
    side = m_selectedSide;
    sector = m_selectedSector;
}

void UftTrackGridWidget::calculateLayout()
{
    if (m_showSectors) {
        m_cellWidth = 8;
        m_cellHeight = 8;
    } else {
        m_cellWidth = 12;
        m_cellHeight = 12;
    }
    
    updateGeometry();
}

QSize UftTrackGridWidget::sizeHint() const
{
    int cols = m_showSectors ? m_sectorsPerTrack : 10;
    int width = m_labelWidth + (cols * m_cellWidth + 20) * m_sides + 20;
    
    int rows = m_showSectors ? m_tracks : (m_tracks + 9) / 10;
    int height = m_headerHeight + rows * m_cellHeight + 20;
    
    return QSize(width, height);
}

QSize UftTrackGridWidget::minimumSizeHint() const
{
    return sizeHint();
}

void UftTrackGridWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    /* Background */
    painter.fillRect(rect(), QColor(250, 250, 250));
    
    /* Draw for each side */
    int cols = m_showSectors ? m_sectorsPerTrack : 10;
    
    for (int side = 0; side < m_sides; side++) {
        int xOffset = m_labelWidth + side * (cols * m_cellWidth + 40);
        
        /* Side header */
        painter.setPen(QColor(25, 118, 210));
        painter.setFont(QFont("Arial", 10, QFont::Bold));
        painter.drawText(xOffset, 2, cols * m_cellWidth, m_headerHeight - 4,
                         Qt::AlignCenter, QString("Side %1").arg(side));
        
        /* Column headers */
        painter.setFont(QFont("Arial", 7));
        painter.setPen(QColor(100, 100, 100));
        for (int c = 0; c < cols; c++) {
            painter.drawText(xOffset + c * m_cellWidth, m_headerHeight - 12,
                             m_cellWidth, 10, Qt::AlignCenter, QString::number(c));
        }
        
        /* Draw cells */
        if (m_showSectors) {
            /* Sector view */
            for (int t = 0; t < m_tracks; t++) {
                /* Row label */
                if (m_showLabels) {
                    painter.drawText(2, m_headerHeight + t * m_cellHeight,
                                     m_labelWidth - 4, m_cellHeight,
                                     Qt::AlignRight | Qt::AlignVCenter,
                                     QString::number(t));
                }
                
                for (int s = 0; s < m_sectorsPerTrack; s++) {
                    int x = xOffset + s * m_cellWidth;
                    int y = m_headerHeight + t * m_cellHeight;
                    QRect cellRect(x, y, m_cellWidth - 1, m_cellHeight - 1);
                    
                    Status status = getSectorStatus(t, side, s);
                    QColor color = statusColor(status);
                    
                    /* Highlight selection/hover */
                    if (t == m_selectedTrack && side == m_selectedSide && s == m_selectedSector) {
                        painter.setPen(QPen(QColor(25, 118, 210), 2));
                    } else if (t == m_hoverTrack && side == m_hoverSide && s == m_hoverSector) {
                        painter.setPen(QPen(QColor(100, 150, 200), 1));
                    } else {
                        painter.setPen(QPen(QColor(180, 180, 180), 1));
                    }
                    
                    painter.setBrush(color);
                    painter.drawRect(cellRect);
                }
            }
        } else {
            /* Track view (10 columns) */
            int rows = (m_tracks + 9) / 10;
            
            for (int row = 0; row < rows; row++) {
                /* Row label */
                if (m_showLabels) {
                    painter.drawText(2, m_headerHeight + row * m_cellHeight,
                                     m_labelWidth - 4, m_cellHeight,
                                     Qt::AlignRight | Qt::AlignVCenter,
                                     QString::number(row));
                }
                
                for (int col = 0; col < 10; col++) {
                    int track = row * 10 + col;
                    if (track >= m_tracks) continue;
                    
                    int x = xOffset + col * m_cellWidth;
                    int y = m_headerHeight + row * m_cellHeight;
                    QRect cellRect(x, y, m_cellWidth - 1, m_cellHeight - 1);
                    
                    Status status = getTrackStatus(track, side);
                    QColor color = statusColor(status);
                    
                    /* Highlight selection/hover */
                    if (track == m_selectedTrack && side == m_selectedSide) {
                        painter.setPen(QPen(QColor(25, 118, 210), 2));
                    } else if (track == m_hoverTrack && side == m_hoverSide) {
                        painter.setPen(QPen(QColor(100, 150, 200), 1));
                    } else {
                        painter.setPen(QPen(QColor(180, 180, 180), 1));
                    }
                    
                    painter.setBrush(color);
                    painter.drawRect(cellRect);
                }
            }
        }
    }
    
    /* Legend */
    int legendY = height() - 20;
    int legendX = 10;
    painter.setFont(QFont("Arial", 8));
    
    QList<QPair<Status, QString>> legend = {
        {STATUS_GOOD, "Good"},
        {STATUS_BAD, "Bad"},
        {STATUS_WARNING, "Warning"},
        {STATUS_WEAK, "Weak"},
        {STATUS_UNKNOWN, "Unknown"}
    };
    
    for (const auto &item : legend) {
        painter.setBrush(statusColor(item.first));
        painter.setPen(QColor(150, 150, 150));
        painter.drawRect(legendX, legendY, 10, 10);
        painter.setPen(QColor(50, 50, 50));
        painter.drawText(legendX + 14, legendY, 50, 12, Qt::AlignLeft | Qt::AlignVCenter, item.second);
        legendX += 70;
    }
}

UftTrackGridWidget::CellInfo UftTrackGridWidget::cellAtPosition(const QPoint &pos) const
{
    CellInfo info = {-1, -1, -1, QRect()};
    
    int cols = m_showSectors ? m_sectorsPerTrack : 10;
    
    for (int side = 0; side < m_sides; side++) {
        int xOffset = m_labelWidth + side * (cols * m_cellWidth + 40);
        
        if (pos.x() >= xOffset && pos.x() < xOffset + cols * m_cellWidth) {
            int col = (pos.x() - xOffset) / m_cellWidth;
            int row = (pos.y() - m_headerHeight) / m_cellHeight;
            
            if (row >= 0 && col >= 0 && col < cols) {
                if (m_showSectors) {
                    if (row < m_tracks) {
                        info.track = row;
                        info.side = side;
                        info.sector = col;
                        info.rect = QRect(xOffset + col * m_cellWidth,
                                          m_headerHeight + row * m_cellHeight,
                                          m_cellWidth - 1, m_cellHeight - 1);
                    }
                } else {
                    int track = row * 10 + col;
                    if (track < m_tracks) {
                        info.track = track;
                        info.side = side;
                        info.sector = -1;
                        info.rect = QRect(xOffset + col * m_cellWidth,
                                          m_headerHeight + row * m_cellHeight,
                                          m_cellWidth - 1, m_cellHeight - 1);
                    }
                }
            }
            break;
        }
    }
    
    return info;
}

QString UftTrackGridWidget::statusTooltip(int track, int side, int sector) const
{
    QString text = QString("Track %1, Side %2").arg(track).arg(side);
    if (sector >= 0) {
        text += QString(", Sector %1").arg(sector);
    }
    
    Status status = (sector >= 0) ? getSectorStatus(track, side, sector) 
                                   : getTrackStatus(track, side);
    
    QString statusText;
    switch (status) {
        case STATUS_GOOD:       statusText = "Good"; break;
        case STATUS_BAD:        statusText = "Bad"; break;
        case STATUS_WARNING:    statusText = "Warning"; break;
        case STATUS_READING:    statusText = "Reading..."; break;
        case STATUS_WRITING:    statusText = "Writing..."; break;
        case STATUS_WEAK:       statusText = "Weak bits detected"; break;
        case STATUS_EMPTY:      statusText = "Empty"; break;
        case STATUS_PROTECTED:  statusText = "Protected"; break;
        case STATUS_MODIFIED:   statusText = "Modified"; break;
        default:                statusText = "Unknown"; break;
    }
    
    text += "\nStatus: " + statusText;
    return text;
}

void UftTrackGridWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        CellInfo cell = cellAtPosition(event->pos());
        if (cell.track >= 0) {
            m_selectedTrack = cell.track;
            m_selectedSide = cell.side;
            m_selectedSector = cell.sector;
            update();
            
            if (cell.sector >= 0) {
                emit sectorClicked(cell.track, cell.side, cell.sector);
            } else {
                emit trackClicked(cell.track, cell.side);
            }
        }
    }
    
    QWidget::mousePressEvent(event);
}

void UftTrackGridWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        CellInfo cell = cellAtPosition(event->pos());
        if (cell.track >= 0) {
            if (cell.sector >= 0) {
                emit sectorDoubleClicked(cell.track, cell.side, cell.sector);
            } else {
                emit trackDoubleClicked(cell.track, cell.side);
            }
        }
    }
    
    QWidget::mouseDoubleClickEvent(event);
}

void UftTrackGridWidget::mouseMoveEvent(QMouseEvent *event)
{
    CellInfo cell = cellAtPosition(event->pos());
    
    bool needUpdate = (cell.track != m_hoverTrack || 
                       cell.side != m_hoverSide || 
                       cell.sector != m_hoverSector);
    
    m_hoverTrack = cell.track;
    m_hoverSide = cell.side;
    m_hoverSector = cell.sector;
    
    if (needUpdate) {
        update();
    }
    
    if (m_showTooltips && cell.track >= 0) {
        QToolTip::showText(event->globalPosition().toPoint(),
                           statusTooltip(cell.track, cell.side, cell.sector),
                           this);
    }
    
    QWidget::mouseMoveEvent(event);
}

void UftTrackGridWidget::leaveEvent(QEvent *event)
{
    m_hoverTrack = -1;
    m_hoverSide = -1;
    m_hoverSector = -1;
    update();
    
    QWidget::leaveEvent(event);
}

void UftTrackGridWidget::wheelEvent(QWheelEvent *event)
{
    /* Toggle sector view on Ctrl+Wheel */
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            setShowSectors(true);
        } else {
            setShowSectors(false);
        }
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void UftTrackGridWidget::resizeEvent(QResizeEvent *event)
{
    calculateLayout();
    QWidget::resizeEvent(event);
}

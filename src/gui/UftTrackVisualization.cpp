/**
 * @file UftTrackVisualization.cpp
 * @brief Track Visualization Implementation
 */

#include "UftTrackVisualization.h"
#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>

UftTrackVisualization::UftTrackVisualization(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

UftTrackVisualization::~UftTrackVisualization()
{
}

void UftTrackVisualization::setTracks(int tracks)
{
    if (m_tracks != tracks) {
        m_tracks = tracks;
        update();
        emit layoutChanged();
    }
}

void UftTrackVisualization::setMaxSectors(int sectors)
{
    if (m_maxSectors != sectors) {
        m_maxSectors = sectors;
        update();
        emit layoutChanged();
    }
}

void UftTrackVisualization::setHeads(int heads)
{
    if (m_heads != heads) {
        m_heads = heads;
        update();
        emit layoutChanged();
    }
}

int UftTrackVisualization::sectorsForTrack(int track) const
{
    return m_sectorsPerTrack.value(track, m_maxSectors);
}

void UftTrackVisualization::setSectorsPerTrack(int track, int sectors)
{
    m_sectorsPerTrack[track] = sectors;
    update();
}

void UftTrackVisualization::setSectorStatus(int head, int track, int sector, 
                                             UftSectorStatus status)
{
    SectorKey key = {head, track, sector};
    m_status[key] = status;
    updateStatistics();
    update();
}

UftSectorStatus UftTrackVisualization::sectorStatus(int head, int track, int sector) const
{
    SectorKey key = {head, track, sector};
    return m_status.value(key, UftSectorStatus::Unknown);
}

void UftTrackVisualization::clearAll()
{
    m_status.clear();
    m_currentHead = m_currentTrack = m_currentSector = -1;
    updateStatistics();
    update();
}

void UftTrackVisualization::reset()
{
    m_status.clear();
    m_sectorsPerTrack.clear();
    m_currentHead = m_currentTrack = m_currentSector = -1;
    updateStatistics();
    update();
}

void UftTrackVisualization::setCurrentPosition(int head, int track, int sector)
{
    m_currentHead = head;
    m_currentTrack = track;
    m_currentSector = sector;
    update();
}

void UftTrackVisualization::clearProgress()
{
    m_currentHead = m_currentTrack = m_currentSector = -1;
    update();
}

void UftTrackVisualization::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

void UftTrackVisualization::setShowLabels(bool show)
{
    m_showLabels = show;
    update();
}

void UftTrackVisualization::setColorScheme(bool dark)
{
    m_darkMode = dark;
    update();
}

QSize UftTrackVisualization::sizeHint() const
{
    int w = m_labelWidth + m_maxSectors * m_cellWidth * m_heads + 
            (m_heads - 1) * m_headGap + 20;
    int h = m_tracks * m_cellHeight + 40;
    return QSize(w, h);
}

QSize UftTrackVisualization::minimumSizeHint() const
{
    return QSize(200, 100);
}

void UftTrackVisualization::updateStatistics()
{
    m_goodCount = m_weakCount = m_badCount = m_totalCount = 0;
    
    for (auto it = m_status.begin(); it != m_status.end(); ++it) {
        m_totalCount++;
        switch (it.value()) {
            case UftSectorStatus::Good:
                m_goodCount++;
                break;
            case UftSectorStatus::Weak:
            case UftSectorStatus::CrcError:
            case UftSectorStatus::Protected:
                m_weakCount++;
                break;
            case UftSectorStatus::Bad:
                m_badCount++;
                break;
            default:
                break;
        }
    }
    
    emit statisticsChanged();
}

QColor UftTrackVisualization::statusColor(UftSectorStatus status) const
{
    switch (status) {
        case UftSectorStatus::Unknown:
            return m_darkMode ? QColor(60, 60, 60) : QColor(220, 220, 220);
        case UftSectorStatus::Good:
            return QColor(76, 175, 80);    // Green
        case UftSectorStatus::Weak:
            return QColor(255, 193, 7);    // Amber
        case UftSectorStatus::CrcError:
            return QColor(255, 152, 0);    // Orange
        case UftSectorStatus::Bad:
            return QColor(244, 67, 54);    // Red
        case UftSectorStatus::Protected:
            return QColor(156, 39, 176);   // Purple
        case UftSectorStatus::Progress:
            return QColor(33, 150, 243);   // Blue
        default:
            return Qt::gray;
    }
}

QRect UftTrackVisualization::sectorRect(int head, int track, int sector) const
{
    int x = m_labelWidth + head * (m_maxSectors * m_cellWidth + m_headGap) +
            sector * m_cellWidth;
    int y = track * m_cellHeight;
    return QRect(x, y, m_cellWidth - 1, m_cellHeight - 1);
}

bool UftTrackVisualization::hitTest(const QPoint &pos, int &head, int &track, int &sector) const
{
    for (int h = 0; h < m_heads; h++) {
        for (int t = 0; t < m_tracks; t++) {
            int sectors = sectorsForTrack(t);
            for (int s = 0; s < sectors; s++) {
                if (sectorRect(h, t, s).contains(pos)) {
                    head = h;
                    track = t;
                    sector = s;
                    return true;
                }
            }
        }
    }
    return false;
}

void UftTrackVisualization::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    
    /* Background */
    painter.fillRect(rect(), m_darkMode ? QColor(30, 30, 30) : Qt::white);
    
    /* Draw sectors */
    for (int h = 0; h < m_heads; h++) {
        for (int t = 0; t < m_tracks; t++) {
            int sectors = sectorsForTrack(t);
            for (int s = 0; s < sectors; s++) {
                QRect r = sectorRect(h, t, s);
                UftSectorStatus st = sectorStatus(h, t, s);
                
                /* Current position highlight */
                if (h == m_currentHead && t == m_currentTrack && s == m_currentSector) {
                    st = UftSectorStatus::Progress;
                }
                
                painter.fillRect(r, statusColor(st));
                
                /* Grid */
                if (m_showGrid) {
                    painter.setPen(m_darkMode ? QColor(50, 50, 50) : QColor(200, 200, 200));
                    painter.drawRect(r);
                }
                
                /* Hover highlight */
                if (h == m_hoverHead && t == m_hoverTrack && s == m_hoverSector) {
                    painter.setPen(QPen(Qt::white, 2));
                    painter.drawRect(r);
                }
            }
        }
    }
    
    /* Track labels */
    if (m_showLabels) {
        painter.setPen(m_darkMode ? Qt::lightGray : Qt::black);
        QFont font = painter.font();
        font.setPointSize(7);
        painter.setFont(font);
        
        for (int t = 0; t < m_tracks; t += 10) {
            int y = t * m_cellHeight + m_cellHeight / 2 + 3;
            painter.drawText(2, y, QString::number(t));
        }
        
        /* Head labels */
        for (int h = 0; h < m_heads; h++) {
            int x = m_labelWidth + h * (m_maxSectors * m_cellWidth + m_headGap) +
                    (m_maxSectors * m_cellWidth) / 2 - 20;
            painter.drawText(x, m_tracks * m_cellHeight + 12, 
                            QString("Head %1").arg(h));
        }
    }
}

void UftTrackVisualization::mousePressEvent(QMouseEvent *event)
{
    int h, t, s;
    if (hitTest(event->pos(), h, t, s)) {
        emit sectorClicked(h, t, s);
    }
}

void UftTrackVisualization::mouseMoveEvent(QMouseEvent *event)
{
    int h, t, s;
    if (hitTest(event->pos(), h, t, s)) {
        if (h != m_hoverHead || t != m_hoverTrack || s != m_hoverSector) {
            m_hoverHead = h;
            m_hoverTrack = t;
            m_hoverSector = s;
            update();
            
            QString tip = QString("Head %1, Track %2, Sector %3\nStatus: %4")
                .arg(h).arg(t).arg(s)
                .arg(static_cast<int>(sectorStatus(h, t, s)));
            QToolTip::showText(event->globalPos(), tip, this);
        }
    } else {
        if (m_hoverHead >= 0) {
            m_hoverHead = m_hoverTrack = m_hoverSector = -1;
            update();
        }
    }
}

void UftTrackVisualization::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    /* Recalculate cell sizes based on available space */
    int availW = width() - m_labelWidth - (m_heads - 1) * m_headGap - 20;
    int availH = height() - 40;
    
    m_cellWidth = qMax(2, availW / (m_maxSectors * m_heads));
    m_cellHeight = qMax(2, availH / m_tracks);
}

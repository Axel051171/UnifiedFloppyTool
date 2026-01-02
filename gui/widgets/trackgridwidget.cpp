/**
 * @file trackgridwidget.cpp
 * @brief Track Grid Widget Implementation
 */

#include "trackgridwidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QToolTip>
#include <QtMath>

TrackGridWidget::TrackGridWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    
    // Catppuccin Mocha palette colors (X-Copy Style)
    m_colorEmpty = QColor(0x31, 0x32, 0x44);      // Surface0 - Pending
    m_colorOk = QColor(0xa6, 0xe3, 0xa1);         // Green - OK (O)
    m_colorHeaderBad = QColor(0xfa, 0xb3, 0x87);  // Peach - Header Error (H)
    m_colorDataBad = QColor(0xf3, 0x8b, 0xa8);    // Red - Data Error (E)
    m_colorWeak = QColor(0xf9, 0xe2, 0xaf);       // Yellow - Weak (W)
    m_colorProtected = QColor(0xcb, 0xa6, 0xf7);  // Mauve - Protected (P)
    m_colorDeleted = QColor(0x74, 0xc7, 0xec);    // Sapphire - Deleted (D)
    m_colorWriting = QColor(0xf9, 0xe2, 0xaf);    // Yellow - Writing
    m_colorVerifying = QColor(0x94, 0xe2, 0xd5);  // Teal - Verifying
    m_colorHover = QColor(0x89, 0xb4, 0xfa);      // Blue - Hover
    m_colorGrid = QColor(0x45, 0x47, 0x5a);       // Surface1
    
    // X-Copy Style: Big Track Number Font (48pt)
    m_bigTrackFont = QFont("JetBrains Mono", 48, QFont::Bold);
    if (!QFontInfo(m_bigTrackFont).exactMatch()) {
        m_bigTrackFont = QFont("Consolas", 48, QFont::Bold);
    }
    
    // Timer Font (24pt)
    m_timerFont = QFont("JetBrains Mono", 24);
    if (!QFontInfo(m_timerFont).exactMatch()) {
        m_timerFont = QFont("Consolas", 24);
    }
    
    initializeData();
}

TrackGridWidget::~TrackGridWidget() = default;

void TrackGridWidget::setTracks(int tracks)
{
    if (m_tracks != tracks) {
        m_tracks = tracks;
        initializeData();
        calculateGeometry();
        update();
    }
}

void TrackGridWidget::setHeads(int heads)
{
    if (m_heads != heads) {
        m_heads = heads;
        initializeData();
        calculateGeometry();
        update();
    }
}

void TrackGridWidget::setSectorsPerTrack(int spt)
{
    if (m_sectorsPerTrack != spt) {
        m_sectorsPerTrack = spt;
        initializeData();
        calculateGeometry();
        update();
    }
}

void TrackGridWidget::updateSector(int track, int head, int sector, int status)
{
    if (track < 0 || track >= m_tracks) return;
    if (head < 0 || head >= m_heads) return;
    if (sector < 0 || sector >= m_sectorsPerTrack) return;
    
    m_data[track][head][sector] = static_cast<SectorStatus>(status);
    
    // Only repaint the affected cell region
    update(sectorRect(track, head, sector).adjusted(-2, -2, 2, 2));
    updateStatistics();
}

void TrackGridWidget::updateTrack(int track, int head, int goodSectors, int badSectors)
{
    Q_UNUSED(goodSectors)
    Q_UNUSED(badSectors)
    
    // Repaint entire track row
    if (track >= 0 && track < m_tracks && head >= 0 && head < m_heads) {
        int y = m_offsetY + (track * m_heads + head) * m_cellHeight;
        update(QRect(0, y - 1, width(), m_cellHeight + 2));
    }
}

void TrackGridWidget::clear()
{
    for (auto& track : m_data) {
        for (auto& head : track) {
            std::fill(head.begin(), head.end(), SectorStatus::Empty);
        }
    }
    update();
    updateStatistics();
}

int TrackGridWidget::totalSectors() const
{
    return m_tracks * m_heads * m_sectorsPerTrack;
}

int TrackGridWidget::goodSectors() const
{
    int count = 0;
    for (const auto& track : m_data) {
        for (const auto& head : track) {
            for (SectorStatus status : head) {
                if (status == SectorStatus::Ok) ++count;
            }
        }
    }
    return count;
}

int TrackGridWidget::badSectors() const
{
    int count = 0;
    for (const auto& track : m_data) {
        for (const auto& head : track) {
            for (SectorStatus status : head) {
                if (status == SectorStatus::HeaderBad || 
                    status == SectorStatus::DataBad) {
                    ++count;
                }
            }
        }
    }
    return count;
}

QSize TrackGridWidget::minimumSizeHint() const
{
    return QSize(200, 200);
}

QSize TrackGridWidget::sizeHint() const
{
    int cellW = 12;
    int cellH = 6;
    int labelWidth = 50;
    
    int w = labelWidth + m_sectorsPerTrack * cellW + 20;
    int h = m_tracks * m_heads * cellH + 40;
    
    return QSize(w, h);
}

void TrackGridWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Background
    painter.fillRect(rect(), QColor(0x1e, 0x1e, 0x2e));
    
    if (m_cellWidth <= 0 || m_cellHeight <= 0) {
        calculateGeometry();
    }
    
    // Track/Head labels font
    QFont labelFont = font();
    labelFont.setPointSize(8);
    painter.setFont(labelFont);
    painter.setPen(QColor(0x6c, 0x70, 0x86));
    
    // Draw grid
    for (int track = 0; track < m_tracks; ++track) {
        for (int head = 0; head < m_heads; ++head) {
            int row = track * m_heads + head;
            int y = m_offsetY + row * m_cellHeight;
            
            // Row label
            QString label = QString("%1.%2").arg(track, 2, 10, QChar('0')).arg(head);
            painter.drawText(4, y, m_offsetX - 8, m_cellHeight, 
                           Qt::AlignRight | Qt::AlignVCenter, label);
            
            // Sector cells
            for (int sector = 0; sector < m_sectorsPerTrack; ++sector) {
                QRect cellRect = sectorRect(track, head, sector);
                
                SectorStatus status = m_data[track][head][sector];
                QColor color = colorForStatus(status);
                
                // Highlight current track being processed
                bool isCurrentTrack = (track == m_currentTrack && head == m_currentHead);
                
                // Hover highlight
                bool isHovered = (track == m_hoverTrack && 
                                 head == m_hoverHead && 
                                 sector == m_hoverSector);
                if (isHovered) {
                    color = m_colorHover;
                }
                
                // Cell background with rounded corners
                QPainterPath path;
                path.addRoundedRect(cellRect.adjusted(1, 1, -1, -1), 2, 2);
                painter.fillPath(path, color);
                
                // Cell border (brighter for current track)
                if (isCurrentTrack) {
                    painter.setPen(QPen(m_colorHover, 1.5));
                } else {
                    painter.setPen(QPen(m_colorGrid, 0.5));
                }
                painter.drawPath(path);
            }
        }
    }
    
    // Column headers (sector numbers)
    painter.setPen(QColor(0x89, 0xb4, 0xfa));
    for (int sector = 0; sector < m_sectorsPerTrack; ++sector) {
        int x = m_offsetX + sector * m_cellWidth;
        painter.drawText(x, 2, m_cellWidth, m_offsetY - 4,
                        Qt::AlignCenter, QString::number(sector));
    }
    
    // === X-COPY STYLE: Big Track Number Display ===
    if (m_showBigTrackNumber && m_currentTrack >= 0) {
        // Position: Right side of widget, centered vertically
        int bigNumX = width() - 100;
        int bigNumY = height() / 2 - 40;
        
        // Shadow effect
        painter.setFont(m_bigTrackFont);
        painter.setPen(QColor(0x11, 0x11, 0x1b, 180));
        painter.drawText(bigNumX + 2, bigNumY + 2, 90, 80, 
                        Qt::AlignCenter, QString::number(m_currentTrack));
        
        // Main number (green if processing)
        painter.setPen(m_colorOk);
        painter.drawText(bigNumX, bigNumY, 90, 80, 
                        Qt::AlignCenter, QString::number(m_currentTrack));
    }
    
    // === X-COPY STYLE: Timer Display ===
    if (m_showTimer && !m_timerText.isEmpty()) {
        // Position: Below big track number or bottom right
        int timerX = width() - 150;
        int timerY = m_showBigTrackNumber ? height() / 2 + 50 : height() - 50;
        
        painter.setFont(m_timerFont);
        painter.setPen(QColor(0x89, 0xb4, 0xfa));  // Blue
        painter.drawText(timerX, timerY, 140, 30, 
                        Qt::AlignCenter, m_timerText);
    }
    
    // Legend at bottom
    int legendY = height() - 20;
    int legendX = 10;
    int legendSize = 10;
    int spacing = 70;
    
    auto drawLegendItem = [&](const QColor& color, const QString& text) {
        QPainterPath path;
        path.addRoundedRect(QRectF(legendX, legendY, legendSize, legendSize), 2, 2);
        painter.fillPath(path, color);
        painter.setPen(QColor(0x6c, 0x70, 0x86));
        QFont smallFont = font();
        smallFont.setPointSize(8);
        painter.setFont(smallFont);
        painter.drawText(legendX + legendSize + 4, legendY, 60, legendSize + 2,
                        Qt::AlignLeft | Qt::AlignVCenter, text);
        legendX += spacing;
    };
    
    drawLegendItem(m_colorOk, "OK");
    drawLegendItem(m_colorDataBad, "Bad");
    drawLegendItem(m_colorHeaderBad, "Header");
    drawLegendItem(m_colorWeak, "Weak");
    drawLegendItem(m_colorEmpty, "Empty");
}

void TrackGridWidget::mouseMoveEvent(QMouseEvent* event)
{
    int track, head, sector;
    sectorAt(event->pos(), track, head, sector);
    
    bool changed = (track != m_hoverTrack || 
                   head != m_hoverHead || 
                   sector != m_hoverSector);
    
    if (changed) {
        // Clear old hover
        if (m_hoverTrack >= 0) {
            update(sectorRect(m_hoverTrack, m_hoverHead, m_hoverSector).adjusted(-2, -2, 2, 2));
        }
        
        m_hoverTrack = track;
        m_hoverHead = head;
        m_hoverSector = sector;
        
        // Update new hover
        if (track >= 0) {
            update(sectorRect(track, head, sector).adjusted(-2, -2, 2, 2));
            emit sectorHovered(track, head, sector);
            
            // Tooltip
            SectorStatus status = m_data[track][head][sector];
            QString statusStr;
            switch (status) {
                case SectorStatus::Empty: statusStr = "Empty"; break;
                case SectorStatus::Ok: statusStr = "OK"; break;
                case SectorStatus::HeaderBad: statusStr = "Header CRC Error"; break;
                case SectorStatus::DataBad: statusStr = "Data CRC Error"; break;
                case SectorStatus::Weak: statusStr = "Weak/Unstable"; break;
                case SectorStatus::Protected: statusStr = "Copy Protected"; break;
                case SectorStatus::Deleted: statusStr = "Deleted"; break;
            }
            
            QToolTip::showText(event->globalPos(),
                QString("Track %1, Head %2, Sector %3\nStatus: %4")
                    .arg(track).arg(head).arg(sector).arg(statusStr));
        }
    }
}

void TrackGridWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        int track, head, sector;
        sectorAt(event->pos(), track, head, sector);
        
        if (track >= 0) {
            emit sectorClicked(track, head, sector);
        }
    }
}

void TrackGridWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
    
    if (m_hoverTrack >= 0) {
        int oldTrack = m_hoverTrack;
        int oldHead = m_hoverHead;
        int oldSector = m_hoverSector;
        
        m_hoverTrack = -1;
        m_hoverHead = -1;
        m_hoverSector = -1;
        
        update(sectorRect(oldTrack, oldHead, oldSector).adjusted(-2, -2, 2, 2));
    }
}

void TrackGridWidget::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event)
    calculateGeometry();
}

void TrackGridWidget::initializeData()
{
    m_data.resize(m_tracks);
    for (int t = 0; t < m_tracks; ++t) {
        m_data[t].resize(m_heads);
        for (int h = 0; h < m_heads; ++h) {
            m_data[t][h].resize(m_sectorsPerTrack);
            std::fill(m_data[t][h].begin(), m_data[t][h].end(), SectorStatus::Empty);
        }
    }
}

void TrackGridWidget::calculateGeometry()
{
    int totalRows = m_tracks * m_heads;
    
    m_offsetX = 45;  // Space for row labels
    m_offsetY = 18;  // Space for column headers
    
    int availableWidth = width() - m_offsetX - 10;
    int availableHeight = height() - m_offsetY - 30; // Space for legend
    
    m_cellWidth = availableWidth / qMax(1, m_sectorsPerTrack);
    m_cellHeight = availableHeight / qMax(1, totalRows);
    
    // Clamp cell sizes
    m_cellWidth = qBound(8, m_cellWidth, 30);
    m_cellHeight = qBound(4, m_cellHeight, 15);
}

QColor TrackGridWidget::colorForStatus(SectorStatus status) const
{
    switch (status) {
        case SectorStatus::Empty: return m_colorEmpty;
        case SectorStatus::Ok: return m_colorOk;
        case SectorStatus::HeaderBad: return m_colorHeaderBad;
        case SectorStatus::DataBad: return m_colorDataBad;
        case SectorStatus::Weak: return m_colorWeak;
        case SectorStatus::Protected: return m_colorProtected;
        case SectorStatus::Deleted: return m_colorDeleted;
        case SectorStatus::Writing: return m_colorWriting;
        case SectorStatus::Verifying: return m_colorVerifying;
        default: return m_colorEmpty;
    }
}

// X-Copy Style Slot Implementations

void TrackGridWidget::setCurrentTrack(int track, int head)
{
    if (m_currentTrack != track || m_currentHead != head) {
        m_currentTrack = track;
        m_currentHead = head;
        emit currentTrackChanged(track, head);
        update();  // Redraw to show big track number
    }
}

void TrackGridWidget::setShowBigTrackNumber(bool show)
{
    if (m_showBigTrackNumber != show) {
        m_showBigTrackNumber = show;
        update();
    }
}

void TrackGridWidget::setTimerText(const QString& text)
{
    if (m_timerText != text) {
        m_timerText = text;
        update();
    }
}

void TrackGridWidget::setShowTimer(bool show)
{
    if (m_showTimer != show) {
        m_showTimer = show;
        update();
    }
}

void TrackGridWidget::sectorAt(const QPoint& pos, int& track, int& head, int& sector) const
{
    track = -1;
    head = -1;
    sector = -1;
    
    if (pos.x() < m_offsetX || pos.y() < m_offsetY) return;
    
    int col = (pos.x() - m_offsetX) / m_cellWidth;
    int row = (pos.y() - m_offsetY) / m_cellHeight;
    
    if (col < 0 || col >= m_sectorsPerTrack) return;
    
    int totalRows = m_tracks * m_heads;
    if (row < 0 || row >= totalRows) return;
    
    track = row / m_heads;
    head = row % m_heads;
    sector = col;
}

QRect TrackGridWidget::sectorRect(int track, int head, int sector) const
{
    int row = track * m_heads + head;
    int x = m_offsetX + sector * m_cellWidth;
    int y = m_offsetY + row * m_cellHeight;
    
    return QRect(x, y, m_cellWidth, m_cellHeight);
}

void TrackGridWidget::updateStatistics()
{
    emit statisticsChanged(goodSectors(), badSectors(), totalSectors());
}

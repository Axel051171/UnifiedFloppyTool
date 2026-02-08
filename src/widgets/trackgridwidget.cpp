/**
 * @file trackgridwidget.cpp
 * @brief Track Grid Widget with Heatmap Visualization - Full Implementation
 * @version 4.0.0
 * 
 * Professional track grid with multiple visualization modes:
 * - Status: Traditional color-coded status display
 * - Confidence: Gradient heatmap 0-100%
 * - Timing: Variance visualization
 * - Protection: Copy protection highlighting
 * - Retries: Retry count visualization
 * - Sectors: Sector success ratio
 */

#include "trackgridwidget.h"
#include <QPainter>
#include <QToolTip>
#include <QContextMenuEvent>
#include <QLinearGradient>
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
    , recoveredSectors(0)
    , retries(0)
    , passCount(1)
    , confidence(0)
    , timingVariance(0.0f)
    , protection(ProtectionType::NONE)
    , weakBitCount(0)
    , hasWeakBits(false)
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
    , m_hoveredCylinder(-1)
    , m_hoveredHead(-1)
    , m_cellSize(16)
    , m_showLabels(true)
    , m_showLegend(true)
    , m_showConfidenceText(false)
    , m_compactMode(false)
    , m_heatmapMode(HeatmapMode::STATUS)
{
    setMinimumSize(calculateMinimumSize());
    setMouseTracking(true);
    initializeTracks();
}

void TrackGridWidget::setDiskGeometry(int cylinders, int heads)
{
    m_maxCylinders = cylinders;
    m_maxHeads = heads;
    initializeTracks();
    setMinimumSize(calculateMinimumSize());
    update();
}

void TrackGridWidget::reset()
{
    for (auto& track : m_tracks) {
        track.status = TrackStatus::UNKNOWN;
        track.goodSectors = 0;
        track.totalSectors = 0;
        track.recoveredSectors = 0;
        track.retries = 0;
        track.passCount = 1;
        track.confidence = 0;
        track.timingVariance = 0.0f;
        track.protection = ProtectionType::NONE;
        track.errorMessage.clear();
        track.protectionName.clear();
        track.weakBitCount = 0;
        track.hasWeakBits = false;
    }
    m_selectedCylinder = -1;
    m_selectedHead = -1;
    update();
}

// ============================================================================
// Track Update Methods
// ============================================================================

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
        
        // Auto-calculate confidence if not set
        if (totalSectors > 0 && m_tracks[index].confidence == 0) {
            m_tracks[index].confidence = (goodSectors * 100) / totalSectors;
        }
        
        update();
    }
}

void TrackGridWidget::updateTrackConfidence(int cylinder, int head, int confidence)
{
    if (cylinder >= m_maxCylinders || head >= m_maxHeads)
        return;
    
    int index = cylinder * m_maxHeads + head;
    if (index >= 0 && index < static_cast<int>(m_tracks.size())) {
        m_tracks[index].confidence = qBound(0, confidence, 100);
        update();
    }
}

void TrackGridWidget::updateTrackTiming(int cylinder, int head, float variance)
{
    if (cylinder >= m_maxCylinders || head >= m_maxHeads)
        return;
    
    int index = cylinder * m_maxHeads + head;
    if (index >= 0 && index < static_cast<int>(m_tracks.size())) {
        m_tracks[index].timingVariance = variance;
        update();
    }
}

void TrackGridWidget::updateTrackProtection(int cylinder, int head, ProtectionType type,
                                            const QString& name)
{
    if (cylinder >= m_maxCylinders || head >= m_maxHeads)
        return;
    
    int index = cylinder * m_maxHeads + head;
    if (index >= 0 && index < static_cast<int>(m_tracks.size())) {
        m_tracks[index].protection = type;
        m_tracks[index].protectionName = name;
        if (type != ProtectionType::NONE) {
            m_tracks[index].status = TrackStatus::PROTECTED;
        }
        update();
    }
}

void TrackGridWidget::updateTrackRetries(int cylinder, int head, int retries, int passes)
{
    if (cylinder >= m_maxCylinders || head >= m_maxHeads)
        return;
    
    int index = cylinder * m_maxHeads + head;
    if (index >= 0 && index < static_cast<int>(m_tracks.size())) {
        m_tracks[index].retries = retries;
        m_tracks[index].passCount = passes;
        update();
    }
}

void TrackGridWidget::updateTrackWeakBits(int cylinder, int head, int count)
{
    if (cylinder >= m_maxCylinders || head >= m_maxHeads)
        return;
    
    int index = cylinder * m_maxHeads + head;
    if (index >= 0 && index < static_cast<int>(m_tracks.size())) {
        m_tracks[index].weakBitCount = count;
        m_tracks[index].hasWeakBits = (count > 0);
        if (count > 0 && m_tracks[index].protection == ProtectionType::NONE) {
            m_tracks[index].protection = ProtectionType::WEAK_BITS;
        }
        update();
    }
}

void TrackGridWidget::updateTrackRecovered(int cylinder, int head, int recoveredSectors)
{
    if (cylinder >= m_maxCylinders || head >= m_maxHeads)
        return;
    
    int index = cylinder * m_maxHeads + head;
    if (index >= 0 && index < static_cast<int>(m_tracks.size())) {
        m_tracks[index].recoveredSectors = recoveredSectors;
        update();
    }
}

void TrackGridWidget::setTrackInfo(int cylinder, int head, const TrackGridTrackInfo& info)
{
    if (cylinder >= m_maxCylinders || head >= m_maxHeads)
        return;
    
    int index = cylinder * m_maxHeads + head;
    if (index >= 0 && index < static_cast<int>(m_tracks.size())) {
        m_tracks[index] = info;
        m_tracks[index].cylinder = cylinder;
        m_tracks[index].head = head;
        update();
    }
}

const TrackGridTrackInfo* TrackGridWidget::getTrackInfo(int cylinder, int head) const
{
    if (cylinder >= m_maxCylinders || head >= m_maxHeads)
        return nullptr;
    
    int index = cylinder * m_maxHeads + head;
    if (index >= 0 && index < static_cast<int>(m_tracks.size())) {
        return &m_tracks[index];
    }
    return nullptr;
}

// ============================================================================
// Selection
// ============================================================================

void TrackGridWidget::setSelectedTrack(int cylinder, int head)
{
    if (m_selectedCylinder != cylinder || m_selectedHead != head) {
        m_selectedCylinder = cylinder;
        m_selectedHead = head;
        emit selectionChanged(cylinder, head);
        update();
    }
}

void TrackGridWidget::clearSelection()
{
    m_selectedCylinder = -1;
    m_selectedHead = -1;
    update();
}

// ============================================================================
// View Mode
// ============================================================================

void TrackGridWidget::setHeatmapMode(HeatmapMode mode)
{
    if (m_heatmapMode != mode) {
        m_heatmapMode = mode;
        update();
    }
}

// ============================================================================
// Display Options
// ============================================================================

void TrackGridWidget::setCellSize(int size)
{
    m_cellSize = qBound(8, size, 32);
    setMinimumSize(calculateMinimumSize());
    update();
}

void TrackGridWidget::setShowLabels(bool show)
{
    m_showLabels = show;
    setMinimumSize(calculateMinimumSize());
    update();
}

void TrackGridWidget::setShowLegend(bool show)
{
    m_showLegend = show;
    setMinimumSize(calculateMinimumSize());
    update();
}

void TrackGridWidget::setShowConfidenceText(bool show)
{
    m_showConfidenceText = show;
    update();
}

void TrackGridWidget::setCompactMode(bool compact)
{
    m_compactMode = compact;
    if (compact) {
        m_cellSize = 10;
        m_showLabels = false;
        m_showLegend = false;
    }
    setMinimumSize(calculateMinimumSize());
    update();
}

// ============================================================================
// Statistics
// ============================================================================

int TrackGridWidget::goodTracks() const
{
    int count = 0;
    for (const auto& track : m_tracks) {
        if (track.status == TrackStatus::GOOD || track.status == TrackStatus::VERIFIED) {
            count++;
        }
    }
    return count;
}

int TrackGridWidget::warningTracks() const
{
    int count = 0;
    for (const auto& track : m_tracks) {
        if (track.status == TrackStatus::WARNING || track.status == TrackStatus::PARTIAL) {
            count++;
        }
    }
    return count;
}

int TrackGridWidget::errorTracks() const
{
    int count = 0;
    for (const auto& track : m_tracks) {
        if (track.status == TrackStatus::ERROR) {
            count++;
        }
    }
    return count;
}

int TrackGridWidget::protectedTracks() const
{
    int count = 0;
    for (const auto& track : m_tracks) {
        if (track.status == TrackStatus::PROTECTED || track.protection != ProtectionType::NONE) {
            count++;
        }
    }
    return count;
}

float TrackGridWidget::averageConfidence() const
{
    if (m_tracks.empty()) return 0.0f;
    
    int total = 0;
    int count = 0;
    for (const auto& track : m_tracks) {
        if (track.status != TrackStatus::UNKNOWN) {
            total += track.confidence;
            count++;
        }
    }
    
    return count > 0 ? static_cast<float>(total) / count : 0.0f;
}

// ============================================================================
// Initialization
// ============================================================================

void TrackGridWidget::initializeTracks()
{
    m_tracks.clear();
    m_tracks.reserve(m_maxCylinders * m_maxHeads);
    
    for (int cyl = 0; cyl < m_maxCylinders; ++cyl) {
        for (int head = 0; head < m_maxHeads; ++head) {
            TrackGridTrackInfo info;
            info.cylinder = cyl;
            info.head = head;
            m_tracks.push_back(info);
        }
    }
}

QSize TrackGridWidget::calculateMinimumSize() const
{
    int labelHeight = m_showLabels ? 25 : 0;
    int labelWidth = m_showLabels ? 30 : 0;
    int legendHeight = m_showLegend ? 40 : 0;
    
    int cols = 10;
    int rows = (m_maxCylinders + cols - 1) / cols;
    
    int gridWidth = cols * m_cellSize;
    int width = labelWidth + gridWidth * m_maxHeads + (m_maxHeads > 1 ? 30 : 0) + 20;
    int height = 25 + labelHeight + (rows * m_cellSize) + legendHeight + 10;
    
    return QSize(width, height);
}

// ============================================================================
// Color Functions
// ============================================================================

QColor TrackGridWidget::getColorForStatus(TrackStatus status) const
{
    switch (status) {
        case TrackStatus::UNKNOWN:   return QColor(180, 180, 180);
        case TrackStatus::READING:   return QColor(100, 150, 255);
        case TrackStatus::GOOD:      return QColor(0, 200, 0);
        case TrackStatus::WARNING:   return QColor(255, 200, 0);
        case TrackStatus::PARTIAL:   return QColor(255, 165, 0);
        case TrackStatus::ERROR:     return QColor(220, 0, 0);
        case TrackStatus::PROTECTED: return QColor(255, 100, 0);
        case TrackStatus::WRITING:   return QColor(180, 100, 255);
        case TrackStatus::VERIFIED:  return QColor(0, 255, 100);
        default:                     return QColor(128, 128, 128);
    }
}

QColor TrackGridWidget::getColorForConfidence(int confidence) const
{
    if (confidence <= 50) {
        float t = confidence / 50.0f;
        return interpolateColor(QColor(220, 0, 0), QColor(255, 200, 0), t);
    } else {
        float t = (confidence - 50) / 50.0f;
        return interpolateColor(QColor(255, 200, 0), QColor(0, 200, 0), t);
    }
}

QColor TrackGridWidget::getColorForTiming(float variance) const
{
    float normalized = qBound(0.0f, variance * 2.0f, 1.0f);
    return interpolateColor(QColor(0, 200, 0), QColor(220, 0, 0), normalized);
}

QColor TrackGridWidget::getColorForProtection(ProtectionType type) const
{
    switch (type) {
        case ProtectionType::NONE:           return QColor(180, 180, 180);
        case ProtectionType::WEAK_BITS:      return QColor(255, 200, 100);
        case ProtectionType::FUZZY_BITS:     return QColor(255, 180, 80);
        case ProtectionType::LONG_TRACK:     return QColor(255, 140, 0);
        case ProtectionType::EXTRA_SECTORS:  return QColor(255, 100, 100);
        case ProtectionType::MISSING_SECTORS:return QColor(200, 100, 100);
        case ProtectionType::BAD_CRC:        return QColor(220, 80, 80);
        case ProtectionType::VMAX:           return QColor(200, 0, 200);
        case ProtectionType::RAPIDLOK:       return QColor(150, 0, 200);
        case ProtectionType::COPYLOCK:       return QColor(100, 0, 200);
        case ProtectionType::SPEEDLOCK:      return QColor(50, 0, 200);
        case ProtectionType::CUSTOM:         return QColor(200, 100, 200);
        default:                             return QColor(128, 128, 128);
    }
}

QColor TrackGridWidget::getColorForRetries(int retries) const
{
    if (retries == 0) return QColor(0, 200, 0);
    if (retries <= 2) return QColor(150, 200, 0);
    if (retries <= 5) return QColor(255, 200, 0);
    if (retries <= 10) return QColor(255, 140, 0);
    return QColor(220, 0, 0);
}

QColor TrackGridWidget::getColorForSectors(int good, int total) const
{
    if (total == 0) return QColor(180, 180, 180);
    return getColorForConfidence((good * 100) / total);
}

QColor TrackGridWidget::getHeatmapColor(const TrackGridTrackInfo& info) const
{
    switch (m_heatmapMode) {
        case HeatmapMode::STATUS:
            return getColorForStatus(info.status);
        case HeatmapMode::CONFIDENCE:
            if (info.status == TrackStatus::UNKNOWN) return QColor(180, 180, 180);
            return getColorForConfidence(info.confidence);
        case HeatmapMode::TIMING:
            if (info.status == TrackStatus::UNKNOWN) return QColor(180, 180, 180);
            return getColorForTiming(info.timingVariance);
        case HeatmapMode::PROTECTION:
            return getColorForProtection(info.protection);
        case HeatmapMode::RETRIES:
            if (info.status == TrackStatus::UNKNOWN) return QColor(180, 180, 180);
            return getColorForRetries(info.retries);
        case HeatmapMode::SECTORS:
            return getColorForSectors(info.goodSectors, info.totalSectors);
        default:
            return getColorForStatus(info.status);
    }
}

QColor TrackGridWidget::interpolateColor(const QColor& from, const QColor& to, float t) const
{
    t = qBound(0.0f, t, 1.0f);
    return QColor(
        from.red() + (to.red() - from.red()) * t,
        from.green() + (to.green() - from.green()) * t,
        from.blue() + (to.blue() - from.blue()) * t
    );
}

// ============================================================================
// Paint Event
// ============================================================================

void TrackGridWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    painter.fillRect(rect(), QColor(245, 245, 245));
    
    int labelHeight = m_showLabels ? 25 : 0;
    int labelWidth = m_showLabels ? 30 : 0;
    int titleY = 5;
    
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSize(9);
    painter.setFont(titleFont);
    painter.setPen(Qt::darkGray);
    
    QString modeText;
    switch (m_heatmapMode) {
        case HeatmapMode::STATUS:     modeText = "Status View"; break;
        case HeatmapMode::CONFIDENCE: modeText = "Confidence Heatmap"; break;
        case HeatmapMode::TIMING:     modeText = "Timing Variance"; break;
        case HeatmapMode::PROTECTION: modeText = "Protection View"; break;
        case HeatmapMode::RETRIES:    modeText = "Retry Count"; break;
        case HeatmapMode::SECTORS:    modeText = "Sector Success"; break;
    }
    painter.drawText(5, titleY + 12, modeText);
    
    int gridStartY = titleY + 20;
    
    for (int head = 0; head < m_maxHeads; ++head) {
        int gridX = labelWidth + head * (10 * m_cellSize + 25);
        drawTrackGrid(painter, gridX, gridStartY + labelHeight, head,
                     QString("Side %1").arg(head));
    }
    
    if (m_showLegend) {
        int legendY = gridStartY + labelHeight + ((m_maxCylinders + 9) / 10) * m_cellSize + 25;
        if (m_heatmapMode == HeatmapMode::CONFIDENCE || 
            m_heatmapMode == HeatmapMode::TIMING ||
            m_heatmapMode == HeatmapMode::SECTORS) {
            drawHeatmapLegend(painter, labelWidth, legendY);
        } else {
            drawLegend(painter, labelWidth, legendY);
        }
    }
}

void TrackGridWidget::drawTrackGrid(QPainter& painter, int startX, int startY,
                                    int head, const QString& title)
{
    if (m_showLabels) {
        QFont labelFont = painter.font();
        labelFont.setBold(true);
        labelFont.setPointSize(8);
        painter.setFont(labelFont);
        painter.setPen(Qt::black);
        
        int rows = (m_maxCylinders + 9) / 10;
        int titleX = startX + (10 * m_cellSize) / 2 - 20;
        int titleY = startY + rows * m_cellSize + 15;
        painter.drawText(titleX, titleY, title);
        
        labelFont.setPointSize(7);
        painter.setFont(labelFont);
        painter.setPen(Qt::darkGray);
        
        for (int row = 0; row < rows; ++row) {
            int trackNum = row * 10;
            painter.drawText(startX - 25, startY + row * m_cellSize + m_cellSize - 3,
                            QString("%1").arg(trackNum, 2, 10, QChar('0')));
        }
    }
    
    for (int track = 0; track < m_maxCylinders; ++track) {
        int row = track / 10;
        int col = track % 10;
        
        int x = startX + col * m_cellSize;
        int y = startY + row * m_cellSize;
        
        int index = track * m_maxHeads + head;
        if (index >= static_cast<int>(m_tracks.size())) continue;
        
        const TrackGridTrackInfo& info = m_tracks[index];
        QColor cellColor = getHeatmapColor(info);
        
        bool isSelected = (track == m_selectedCylinder && head == m_selectedHead);
        bool isHovered = (track == m_hoveredCylinder && head == m_hoveredHead);
        
        if (isSelected) {
            painter.setPen(QPen(QColor(0, 100, 255), 2));
        } else if (isHovered) {
            painter.setPen(QPen(QColor(100, 100, 255), 1));
        } else {
            painter.setPen(QPen(QColor(100, 100, 100), 1));
        }
        
        painter.setBrush(cellColor);
        painter.drawRect(x, y, m_cellSize - 1, m_cellSize - 1);
        
        if (m_showConfidenceText && m_cellSize >= 16 && 
            info.status != TrackStatus::UNKNOWN) {
            QFont smallFont = painter.font();
            smallFont.setPointSize(6);
            painter.setFont(smallFont);
            painter.setPen(cellColor.lightness() > 128 ? Qt::black : Qt::white);
            painter.drawText(x + 2, y + m_cellSize - 3, QString::number(info.confidence));
        }
        
        if (info.protection != ProtectionType::NONE && m_heatmapMode != HeatmapMode::PROTECTION) {
            QPolygon triangle;
            triangle << QPoint(x + m_cellSize - 6, y + 1)
                    << QPoint(x + m_cellSize - 1, y + 1)
                    << QPoint(x + m_cellSize - 1, y + 6);
            painter.setBrush(QColor(255, 100, 0));
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(triangle);
        }
        
        if (info.hasWeakBits && m_heatmapMode != HeatmapMode::PROTECTION) {
            painter.setBrush(QColor(255, 255, 0));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(x + 2, y + 2, 4, 4);
        }
    }
}

void TrackGridWidget::drawLegend(QPainter& painter, int x, int y)
{
    QFont legendFont = painter.font();
    legendFont.setPointSize(7);
    painter.setFont(legendFont);
    
    struct LegendItem { QColor color; QString text; };
    std::vector<LegendItem> items;
    
    if (m_heatmapMode == HeatmapMode::STATUS) {
        items = {
            {getColorForStatus(TrackStatus::GOOD), "Good"},
            {getColorForStatus(TrackStatus::WARNING), "Warning"},
            {getColorForStatus(TrackStatus::ERROR), "Error"},
            {getColorForStatus(TrackStatus::PROTECTED), "Protected"},
            {getColorForStatus(TrackStatus::UNKNOWN), "Unknown"}
        };
    } else if (m_heatmapMode == HeatmapMode::PROTECTION) {
        items = {
            {getColorForProtection(ProtectionType::NONE), "None"},
            {getColorForProtection(ProtectionType::WEAK_BITS), "Weak"},
            {getColorForProtection(ProtectionType::COPYLOCK), "CopyLock"},
            {getColorForProtection(ProtectionType::VMAX), "V-MAX"},
            {getColorForProtection(ProtectionType::RAPIDLOK), "RapidLok"}
        };
    } else if (m_heatmapMode == HeatmapMode::RETRIES) {
        items = {
            {getColorForRetries(0), "0"},
            {getColorForRetries(2), "1-2"},
            {getColorForRetries(5), "3-5"},
            {getColorForRetries(10), "6-10"},
            {getColorForRetries(20), "10+"}
        };
    }
    
    int itemX = x;
    for (const auto& item : items) {
        painter.setBrush(item.color);
        painter.setPen(Qt::darkGray);
        painter.drawRect(itemX, y, 12, 12);
        painter.setPen(Qt::black);
        painter.drawText(itemX + 15, y + 10, item.text);
        itemX += 55;
    }
}

void TrackGridWidget::drawHeatmapLegend(QPainter& painter, int x, int y)
{
    int barWidth = 200;
    int barHeight = 12;
    
    QLinearGradient gradient(x, y, x + barWidth, y);
    gradient.setColorAt(0.0, QColor(220, 0, 0));
    gradient.setColorAt(0.5, QColor(255, 200, 0));
    gradient.setColorAt(1.0, QColor(0, 200, 0));
    
    painter.fillRect(x, y, barWidth, barHeight, gradient);
    painter.setPen(Qt::darkGray);
    painter.drawRect(x, y, barWidth, barHeight);
    
    QFont legendFont = painter.font();
    legendFont.setPointSize(7);
    painter.setFont(legendFont);
    painter.setPen(Qt::black);
    
    if (m_heatmapMode == HeatmapMode::CONFIDENCE || m_heatmapMode == HeatmapMode::SECTORS) {
        painter.drawText(x - 5, y + barHeight + 12, "0%");
        painter.drawText(x + barWidth / 2 - 10, y + barHeight + 12, "50%");
        painter.drawText(x + barWidth - 15, y + barHeight + 12, "100%");
    } else if (m_heatmapMode == HeatmapMode::TIMING) {
        painter.drawText(x - 5, y + barHeight + 12, "Low");
        painter.drawText(x + barWidth - 20, y + barHeight + 12, "High");
    }
}

// ============================================================================
// Mouse Events
// ============================================================================

void TrackGridWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int cylinder, head;
        if (getTrackAtPos(event->pos(), &cylinder, &head)) {
            setSelectedTrack(cylinder, head);
            emit trackClicked(cylinder, head);
        }
    }
}

void TrackGridWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int cylinder, head;
        if (getTrackAtPos(event->pos(), &cylinder, &head)) {
            emit trackDoubleClicked(cylinder, head);
        }
    }
}

void TrackGridWidget::mouseMoveEvent(QMouseEvent *event)
{
    int cylinder, head;
    if (getTrackAtPos(event->pos(), &cylinder, &head)) {
        if (m_hoveredCylinder != cylinder || m_hoveredHead != head) {
            m_hoveredCylinder = cylinder;
            m_hoveredHead = head;
            update();
        }
        showTrackTooltip(event->globalPosition().toPoint(), cylinder, head);
    } else {
        if (m_hoveredCylinder != -1) {
            m_hoveredCylinder = -1;
            m_hoveredHead = -1;
            update();
        }
        QToolTip::hideText();
    }
}

void TrackGridWidget::contextMenuEvent(QContextMenuEvent *event)
{
    int cylinder, head;
    if (getTrackAtPos(event->pos(), &cylinder, &head)) {
        emit trackContextMenu(cylinder, head, event->globalPos());
    }
}

void TrackGridWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_hoveredCylinder = -1;
    m_hoveredHead = -1;
    update();
}

// ============================================================================
// Hit Testing
// ============================================================================

bool TrackGridWidget::getTrackAtPos(const QPoint& pos, int* cylinder, int* head) const
{
    int labelHeight = m_showLabels ? 25 : 0;
    int labelWidth = m_showLabels ? 30 : 0;
    int gridStartY = 25 + labelHeight;
    
    for (int h = 0; h < m_maxHeads; ++h) {
        int gridX = labelWidth + h * (10 * m_cellSize + 25);
        int gridY = gridStartY;
        int rows = (m_maxCylinders + 9) / 10;
        
        if (pos.x() >= gridX && pos.x() < gridX + 10 * m_cellSize &&
            pos.y() >= gridY && pos.y() < gridY + rows * m_cellSize)
        {
            int col = (pos.x() - gridX) / m_cellSize;
            int row = (pos.y() - gridY) / m_cellSize;
            int track = row * 10 + col;
            
            if (track < m_maxCylinders) {
                *cylinder = track;
                *head = h;
                return true;
            }
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
    QToolTip::showText(globalPos, formatTrackTooltip(info));
}

QString TrackGridWidget::formatTrackTooltip(const TrackGridTrackInfo& info) const
{
    QString tooltip;
    tooltip += QString("<b>Track %1, Side %2</b><br>").arg(info.cylinder).arg(info.head);
    tooltip += "<hr>";
    
    QString statusText;
    switch (info.status) {
        case TrackStatus::UNKNOWN:   statusText = "Not read"; break;
        case TrackStatus::READING:   statusText = "<font color='blue'>Reading...</font>"; break;
        case TrackStatus::GOOD:      statusText = "<font color='green'>Good</font>"; break;
        case TrackStatus::WARNING:   statusText = "<font color='orange'>Warning</font>"; break;
        case TrackStatus::PARTIAL:   statusText = "<font color='darkorange'>Partial</font>"; break;
        case TrackStatus::ERROR:     statusText = "<font color='red'>Error</font>"; break;
        case TrackStatus::PROTECTED: statusText = "<font color='darkorange'>Protected</font>"; break;
        case TrackStatus::WRITING:   statusText = "<font color='purple'>Writing...</font>"; break;
        case TrackStatus::VERIFIED:  statusText = "<font color='green'>Verified</font>"; break;
    }
    tooltip += QString("Status: %1<br>").arg(statusText);
    
    if (info.totalSectors > 0) {
        tooltip += QString("Sectors: %1/%2").arg(info.goodSectors).arg(info.totalSectors);
        if (info.recoveredSectors > 0) {
            tooltip += QString(" (+%1 recovered)").arg(info.recoveredSectors);
        }
        tooltip += "<br>";
    }
    
    if (info.status != TrackStatus::UNKNOWN) {
        QString confColor = info.confidence >= 90 ? "green" : 
                           (info.confidence >= 70 ? "orange" : "red");
        tooltip += QString("Confidence: <font color='%1'>%2%</font><br>")
                  .arg(confColor).arg(info.confidence);
    }
    
    if (info.retries > 0) {
        tooltip += QString("Retries: %1").arg(info.retries);
        if (info.passCount > 1) {
            tooltip += QString(" (%1 passes)").arg(info.passCount);
        }
        tooltip += "<br>";
    }
    
    if (info.protection != ProtectionType::NONE) {
        tooltip += QString("Protection: <font color='darkorange'>%1</font><br>")
                  .arg(info.protectionName.isEmpty() ? "Detected" : info.protectionName);
    }
    
    if (info.hasWeakBits) {
        tooltip += QString("Weak bits: %1<br>").arg(info.weakBitCount);
    }
    
    if (info.timingVariance > 0.01f) {
        tooltip += QString("Timing variance: %1<br>").arg(info.timingVariance, 0, 'f', 3);
    }
    
    if (!info.errorMessage.isEmpty()) {
        tooltip += QString("<font color='red'>%1</font>").arg(info.errorMessage);
    }
    
    return tooltip;
}

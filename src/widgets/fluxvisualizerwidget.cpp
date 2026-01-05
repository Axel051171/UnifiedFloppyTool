/**
 * @file fluxvisualizerwidget.cpp
 * @brief Flux Timing Waveform Visualization - Full Implementation
 * @version 4.0.0
 */

#include "fluxvisualizerwidget.h"
#include <QPainter>
#include <QLinearGradient>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QBuffer>
#include <cmath>
#include <algorithm>
#include <numeric>

// ============================================================================
// Constants
// ============================================================================

static const int RULER_HEIGHT = 30;
static const int STATS_HEIGHT = 60;
static const int MARGIN_LEFT = 50;
static const int MARGIN_RIGHT = 20;
static const int MARGIN_TOP = 10;
static const int MARGIN_BOTTOM = 10;

// Standard MFM cell times (nanoseconds at 300 RPM, 250kbps)
static const double MFM_CELL_2US = 2000.0;  // Short cell
static const double MFM_CELL_3US = 3000.0;  // Medium cell
static const double MFM_CELL_4US = 4000.0;  // Long cell

// ============================================================================
// Constructor / Destructor
// ============================================================================

FluxVisualizerWidget::FluxVisualizerWidget(QWidget *parent)
    : QWidget(parent)
    , m_viewMode(FluxViewMode::WAVEFORM)
    , m_encoding(FluxEncoding::AUTO)
    , m_zoom(1.0)
    , m_offset(0.0)
    , m_cellTime(MFM_CELL_2US)
    , m_syncPattern(0x4489)  // Amiga/IBM MFM sync
    , m_showGrid(true)
    , m_showCellBoundaries(true)
    , m_showSyncPatterns(true)
    , m_showWeakBits(true)
    , m_showStatistics(true)
    , m_isDragging(false)
    , m_markerPosition(0)
    , m_selectionStart(0)
    , m_selectionEnd(0)
    , m_isSelecting(false)
    , m_totalTime(0.0)
    , m_visibleTime(0.0)
    , m_rulerHeight(RULER_HEIGHT)
    , m_statsHeight(STATS_HEIGHT)
{
    setMinimumSize(400, 200);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    memset(&m_stats, 0, sizeof(m_stats));
}

FluxVisualizerWidget::~FluxVisualizerWidget()
{
}

// ============================================================================
// Data Loading
// ============================================================================

void FluxVisualizerWidget::setFluxData(const std::vector<uint32_t>& timings, int revolution)
{
    m_revolutions.clear();
    
    std::vector<FluxSample> samples;
    samples.reserve(timings.size());
    
    for (size_t i = 0; i < timings.size(); ++i) {
        FluxSample sample;
        sample.time_ns = timings[i];
        sample.revolution = revolution;
        sample.is_weak = false;
        sample.is_sync = false;
        samples.push_back(sample);
    }
    
    m_revolutions.push_back(samples);
    
    updateStatistics();
    detectSyncPatterns();
    detectWeakBits();
    
    zoomToFit();
    update();
}

void FluxVisualizerWidget::addRevolution(const std::vector<uint32_t>& timings)
{
    std::vector<FluxSample> samples;
    samples.reserve(timings.size());
    
    int revolution = m_revolutions.size();
    
    for (size_t i = 0; i < timings.size(); ++i) {
        FluxSample sample;
        sample.time_ns = timings[i];
        sample.revolution = revolution;
        sample.is_weak = false;
        sample.is_sync = false;
        samples.push_back(sample);
    }
    
    m_revolutions.push_back(samples);
    
    updateStatistics();
    if (m_revolutions.size() > 1) {
        detectWeakBits();
    }
    
    update();
}

void FluxVisualizerWidget::clearData()
{
    m_revolutions.clear();
    m_regions.clear();
    memset(&m_stats, 0, sizeof(m_stats));
    m_totalTime = 0;
    update();
}

// ============================================================================
// View Settings
// ============================================================================

void FluxVisualizerWidget::setViewMode(FluxViewMode mode)
{
    if (m_viewMode != mode) {
        m_viewMode = mode;
        emit viewModeChanged(mode);
        update();
    }
}

void FluxVisualizerWidget::setEncoding(FluxEncoding encoding)
{
    if (m_encoding != encoding) {
        m_encoding = encoding;
        
        // Set appropriate cell time
        switch (encoding) {
            case FluxEncoding::MFM:
            case FluxEncoding::AMIGA_MFM:
                m_cellTime = MFM_CELL_2US;
                m_syncPattern = 0x4489;
                break;
            case FluxEncoding::FM:
                m_cellTime = 4000.0;
                break;
            case FluxEncoding::GCR:
            case FluxEncoding::APPLE_GCR:
                m_cellTime = 2000.0;
                m_syncPattern = 0xD5AA;
                break;
            default:
                break;
        }
        
        detectSyncPatterns();
        update();
    }
}

// ============================================================================
// Navigation
// ============================================================================

void FluxVisualizerWidget::setZoom(double zoom)
{
    zoom = qBound(0.1, zoom, 100.0);
    if (m_zoom != zoom) {
        m_zoom = zoom;
        emit zoomChanged(zoom);
        update();
    }
}

void FluxVisualizerWidget::setOffset(double offset)
{
    offset = qBound(0.0, offset, m_totalTime);
    if (m_offset != offset) {
        m_offset = offset;
        update();
    }
}

void FluxVisualizerWidget::zoomIn()
{
    setZoom(m_zoom * 1.5);
}

void FluxVisualizerWidget::zoomOut()
{
    setZoom(m_zoom / 1.5);
}

void FluxVisualizerWidget::zoomToFit()
{
    if (m_totalTime > 0) {
        int plotWidth = width() - MARGIN_LEFT - MARGIN_RIGHT;
        m_zoom = plotWidth / m_totalTime * 1000.0;  // pixels per microsecond
        m_offset = 0;
        emit zoomChanged(m_zoom);
        update();
    }
}

void FluxVisualizerWidget::scrollTo(size_t sampleIndex)
{
    if (m_revolutions.empty() || m_revolutions[0].empty())
        return;
    
    // Calculate time offset to sample
    double time = 0;
    const auto& samples = m_revolutions[0];
    for (size_t i = 0; i < sampleIndex && i < samples.size(); ++i) {
        time += samples[i].time_ns;
    }
    
    // Center the sample in view
    int plotWidth = width() - MARGIN_LEFT - MARGIN_RIGHT;
    double visibleTime = plotWidth / m_zoom * 1000.0;
    
    setOffset(qMax(0.0, time - visibleTime / 2));
}

// ============================================================================
// Display Options
// ============================================================================

void FluxVisualizerWidget::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

void FluxVisualizerWidget::setShowCellBoundaries(bool show)
{
    m_showCellBoundaries = show;
    update();
}

void FluxVisualizerWidget::setShowSyncPatterns(bool show)
{
    m_showSyncPatterns = show;
    update();
}

void FluxVisualizerWidget::setShowWeakBits(bool show)
{
    m_showWeakBits = show;
    update();
}

void FluxVisualizerWidget::setShowStatistics(bool show)
{
    m_showStatistics = show;
    m_statsHeight = show ? STATS_HEIGHT : 0;
    update();
}

void FluxVisualizerWidget::setCellTime(double time_ns)
{
    m_cellTime = time_ns;
    update();
}

void FluxVisualizerWidget::setSyncPattern(uint16_t pattern)
{
    m_syncPattern = pattern;
    detectSyncPatterns();
    update();
}

// ============================================================================
// Markers and Regions
// ============================================================================

void FluxVisualizerWidget::addRegion(const FluxRegion& region)
{
    m_regions.push_back(region);
    update();
}

void FluxVisualizerWidget::clearRegions()
{
    m_regions.clear();
    update();
}

void FluxVisualizerWidget::setMarkerPosition(size_t index)
{
    m_markerPosition = index;
    update();
}

// ============================================================================
// Statistics
// ============================================================================

FluxStatistics FluxVisualizerWidget::getStatistics() const
{
    return m_stats;
}

void FluxVisualizerWidget::updateStatistics()
{
    memset(&m_stats, 0, sizeof(m_stats));
    
    if (m_revolutions.empty() || m_revolutions[0].empty())
        return;
    
    const auto& samples = m_revolutions[0];
    
    // Calculate basic statistics
    double sum = 0;
    double sum_sq = 0;
    double min_val = samples[0].time_ns;
    double max_val = samples[0].time_ns;
    
    for (const auto& sample : samples) {
        double t = sample.time_ns;
        sum += t;
        sum_sq += t * t;
        min_val = std::min(min_val, t);
        max_val = std::max(max_val, t);
    }
    
    m_stats.sample_count = samples.size();
    m_stats.mean_time = sum / samples.size();
    m_stats.std_dev = std::sqrt(sum_sq / samples.size() - 
                                m_stats.mean_time * m_stats.mean_time);
    m_stats.min_time = min_val;
    m_stats.max_time = max_val;
    
    // Calculate total time
    m_totalTime = sum;
    
    // Calculate RPM (assuming one revolution)
    // 1 revolution = totalTime nanoseconds
    // RPM = 60 * 1e9 / totalTime
    if (m_totalTime > 0) {
        m_stats.rpm = 60.0 * 1e9 / m_totalTime;
    }
    
    // Count weak and sync
    for (const auto& sample : samples) {
        if (sample.is_weak) m_stats.weak_count++;
        if (sample.is_sync) m_stats.sync_count++;
    }
}

void FluxVisualizerWidget::detectSyncPatterns()
{
    if (m_revolutions.empty())
        return;
    
    // Simple sync detection based on timing patterns
    // MFM sync 0x4489 has specific timing sequence
    
    for (auto& samples : m_revolutions) {
        for (size_t i = 0; i + 4 < samples.size(); ++i) {
            // Look for characteristic sync timing pattern
            // 4489 in MFM: 01000100 10001001
            // This creates specific flux timing sequences
            
            double t1 = samples[i].time_ns;
            double t2 = samples[i+1].time_ns;
            
            // Check for 4us-4us-2us pattern (simplified)
            if (std::abs(t1 - 4000) < 500 && std::abs(t2 - 4000) < 500) {
                samples[i].is_sync = true;
                samples[i+1].is_sync = true;
            }
        }
    }
}

void FluxVisualizerWidget::detectWeakBits()
{
    if (m_revolutions.size() < 2)
        return;
    
    // Compare revolutions to find inconsistent bits
    const auto& ref = m_revolutions[0];
    
    for (size_t i = 0; i < ref.size(); ++i) {
        double ref_time = ref[i].time_ns;
        bool is_weak = false;
        
        for (size_t r = 1; r < m_revolutions.size(); ++r) {
            if (i < m_revolutions[r].size()) {
                double diff = std::abs(m_revolutions[r][i].time_ns - ref_time);
                // More than 20% deviation = weak
                if (diff / ref_time > 0.2) {
                    is_weak = true;
                    break;
                }
            }
        }
        
        for (auto& rev : m_revolutions) {
            if (i < rev.size()) {
                rev[i].is_weak = is_weak;
            }
        }
    }
}

// ============================================================================
// Color Functions
// ============================================================================

QColor FluxVisualizerWidget::getTimingColor(uint32_t time_ns) const
{
    // Color based on cell timing
    // Short (2us) = Green, Medium (3us) = Yellow, Long (4us) = Red
    
    double ratio = time_ns / m_cellTime;
    
    if (ratio < 1.25) {
        return QColor(0, 200, 0);       // Short - Green
    } else if (ratio < 1.75) {
        return QColor(255, 200, 0);     // Medium - Yellow
    } else if (ratio < 2.25) {
        return QColor(255, 100, 0);     // Long - Orange
    } else {
        return QColor(200, 0, 0);       // Very long - Red
    }
}

// ============================================================================
// Coordinate Conversion
// ============================================================================

int FluxVisualizerWidget::timeToX(double time_ns) const
{
    return MARGIN_LEFT + (time_ns - m_offset) * m_zoom / 1000.0;
}

double FluxVisualizerWidget::xToTime(int x) const
{
    return m_offset + (x - MARGIN_LEFT) * 1000.0 / m_zoom;
}

size_t FluxVisualizerWidget::xToSampleIndex(int x) const
{
    if (m_revolutions.empty() || m_revolutions[0].empty())
        return 0;
    
    double targetTime = xToTime(x);
    double cumTime = 0;
    
    const auto& samples = m_revolutions[0];
    for (size_t i = 0; i < samples.size(); ++i) {
        cumTime += samples[i].time_ns;
        if (cumTime >= targetTime) {
            return i;
        }
    }
    
    return samples.size() - 1;
}

// ============================================================================
// Paint Event
// ============================================================================

void FluxVisualizerWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background
    painter.fillRect(rect(), QColor(30, 30, 35));
    
    if (m_revolutions.empty()) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, "No flux data loaded");
        return;
    }
    
    // Draw components based on view mode
    if (m_showGrid) {
        drawGrid(painter);
    }
    
    switch (m_viewMode) {
        case FluxViewMode::WAVEFORM:
            drawWaveform(painter);
            break;
        case FluxViewMode::HISTOGRAM:
            drawHistogram(painter);
            break;
        case FluxViewMode::SPECTROGRAM:
            drawSpectrogram(painter);
            break;
        case FluxViewMode::CELL_VIEW:
            drawCellView(painter);
            break;
        case FluxViewMode::COMPARISON:
            drawComparison(painter);
            break;
    }
    
    if (m_showSyncPatterns) {
        drawSyncPatterns(painter);
    }
    
    if (m_showWeakBits) {
        drawWeakBits(painter);
    }
    
    drawRegions(painter);
    drawRuler(painter);
    drawMarker(painter);
    
    if (m_showStatistics) {
        drawStatistics(painter);
    }
}

void FluxVisualizerWidget::drawWaveform(QPainter& painter)
{
    if (m_revolutions.empty())
        return;
    
    int plotHeight = height() - m_rulerHeight - m_statsHeight - MARGIN_TOP - MARGIN_BOTTOM;
    int baseY = MARGIN_TOP + plotHeight / 2;
    int amplitude = plotHeight / 3;
    
    for (size_t r = 0; r < m_revolutions.size(); ++r) {
        const auto& samples = m_revolutions[r];
        
        // Color for this revolution
        QColor lineColor = r == 0 ? QColor(0, 200, 255) : 
                          QColor(100 + r * 30, 200 - r * 20, 255 - r * 30);
        lineColor.setAlpha(r == 0 ? 255 : 150);
        painter.setPen(QPen(lineColor, r == 0 ? 2 : 1));
        
        double cumTime = 0;
        bool high = true;
        
        QPolygon polygon;
        
        for (size_t i = 0; i < samples.size(); ++i) {
            int x1 = timeToX(cumTime);
            cumTime += samples[i].time_ns;
            int x2 = timeToX(cumTime);
            
            // Skip if completely outside view
            if (x2 < MARGIN_LEFT || x1 > width() - MARGIN_RIGHT)
                continue;
            
            int y = baseY + (high ? -amplitude : amplitude);
            
            if (polygon.isEmpty()) {
                polygon << QPoint(x1, y);
            }
            polygon << QPoint(x2, y);
            
            high = !high;
            
            // Transition
            int nextY = baseY + (high ? -amplitude : amplitude);
            polygon << QPoint(x2, nextY);
        }
        
        if (!polygon.isEmpty()) {
            painter.drawPolyline(polygon);
        }
    }
}

void FluxVisualizerWidget::drawHistogram(QPainter& painter)
{
    if (m_revolutions.empty() || m_revolutions[0].empty())
        return;
    
    const auto& samples = m_revolutions[0];
    
    // Build histogram
    const int NUM_BINS = 100;
    std::vector<int> bins(NUM_BINS, 0);
    
    double binWidth = (m_stats.max_time - m_stats.min_time) / NUM_BINS;
    if (binWidth <= 0) binWidth = 100;
    
    int maxCount = 0;
    for (const auto& sample : samples) {
        int bin = (sample.time_ns - m_stats.min_time) / binWidth;
        bin = qBound(0, bin, NUM_BINS - 1);
        bins[bin]++;
        maxCount = std::max(maxCount, bins[bin]);
    }
    
    // Draw histogram
    int plotHeight = height() - m_rulerHeight - m_statsHeight - MARGIN_TOP - MARGIN_BOTTOM;
    int plotWidth = width() - MARGIN_LEFT - MARGIN_RIGHT;
    int barWidth = plotWidth / NUM_BINS;
    
    for (int i = 0; i < NUM_BINS; ++i) {
        int barHeight = bins[i] * plotHeight / (maxCount + 1);
        int x = MARGIN_LEFT + i * barWidth;
        int y = MARGIN_TOP + plotHeight - barHeight;
        
        double time = m_stats.min_time + i * binWidth;
        QColor color = getTimingColor(time);
        
        painter.fillRect(x, y, barWidth - 1, barHeight, color);
    }
    
    // Draw expected cell markers
    painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
    for (double cell = m_cellTime; cell < m_stats.max_time; cell += m_cellTime) {
        int bin = (cell - m_stats.min_time) / binWidth;
        if (bin >= 0 && bin < NUM_BINS) {
            int x = MARGIN_LEFT + bin * barWidth;
            painter.drawLine(x, MARGIN_TOP, x, MARGIN_TOP + plotHeight);
        }
    }
}

void FluxVisualizerWidget::drawSpectrogram(QPainter& painter)
{
    // Time-frequency display (simplified)
    if (m_revolutions.empty() || m_revolutions[0].empty())
        return;
    
    const auto& samples = m_revolutions[0];
    
    int plotHeight = height() - m_rulerHeight - m_statsHeight - MARGIN_TOP - MARGIN_BOTTOM;
    int plotWidth = width() - MARGIN_LEFT - MARGIN_RIGHT;
    
    // Each column represents a window of samples
    int windowSize = 16;
    int numCols = samples.size() / windowSize;
    if (numCols == 0) numCols = 1;
    
    double colWidth = static_cast<double>(plotWidth) / numCols;
    
    for (int col = 0; col < numCols; ++col) {
        // Analyze window
        double mean = 0;
        for (int i = 0; i < windowSize && col * windowSize + i < (int)samples.size(); ++i) {
            mean += samples[col * windowSize + i].time_ns;
        }
        mean /= windowSize;
        
        // Map to color (frequency indicator)
        QColor color = getTimingColor(mean);
        
        int x = MARGIN_LEFT + col * colWidth;
        painter.fillRect(x, MARGIN_TOP, colWidth + 1, plotHeight, color);
    }
}

void FluxVisualizerWidget::drawCellView(QPainter& painter)
{
    if (m_revolutions.empty() || m_revolutions[0].empty())
        return;
    
    const auto& samples = m_revolutions[0];
    
    int plotHeight = height() - m_rulerHeight - m_statsHeight - MARGIN_TOP - MARGIN_BOTTOM;
    int cellHeight = plotHeight / 4;
    int baseY = MARGIN_TOP + plotHeight / 2;
    
    double cumTime = 0;
    
    for (size_t i = 0; i < samples.size(); ++i) {
        int x1 = timeToX(cumTime);
        cumTime += samples[i].time_ns;
        int x2 = timeToX(cumTime);
        
        if (x2 < MARGIN_LEFT || x1 > width() - MARGIN_RIGHT)
            continue;
        
        // Determine cell type based on timing
        int cellType = std::round(samples[i].time_ns / m_cellTime);
        cellType = qBound(1, cellType, 4);
        
        QColor color = getTimingColor(samples[i].time_ns);
        
        // Draw cell rectangle
        int y = baseY - cellHeight / 2;
        painter.fillRect(x1, y, x2 - x1 - 1, cellHeight, color);
        
        // Draw cell type label
        if (x2 - x1 > 15) {
            painter.setPen(Qt::white);
            QFont smallFont = painter.font();
            smallFont.setPointSize(7);
            painter.setFont(smallFont);
            painter.drawText(x1 + 2, y + cellHeight - 3, QString::number(cellType));
        }
    }
}

void FluxVisualizerWidget::drawComparison(QPainter& painter)
{
    if (m_revolutions.size() < 2)
        return;
    
    int plotHeight = height() - m_rulerHeight - m_statsHeight - MARGIN_TOP - MARGIN_BOTTOM;
    int revHeight = plotHeight / m_revolutions.size();
    
    for (size_t r = 0; r < m_revolutions.size(); ++r) {
        const auto& samples = m_revolutions[r];
        int baseY = MARGIN_TOP + r * revHeight + revHeight / 2;
        int amplitude = revHeight / 3;
        
        QColor color(100 + r * 50, 200 - r * 30, 255 - r * 40);
        painter.setPen(QPen(color, 1));
        
        double cumTime = 0;
        bool high = true;
        
        for (size_t i = 0; i < samples.size(); ++i) {
            int x1 = timeToX(cumTime);
            cumTime += samples[i].time_ns;
            int x2 = timeToX(cumTime);
            
            if (x2 < MARGIN_LEFT || x1 > width() - MARGIN_RIGHT)
                continue;
            
            int y = baseY + (high ? -amplitude : amplitude);
            
            if (samples[i].is_weak) {
                painter.fillRect(x1, baseY - amplitude, x2 - x1, amplitude * 2,
                               QColor(255, 255, 0, 50));
            }
            
            painter.drawLine(x1, y, x2, y);
            
            high = !high;
            int nextY = baseY + (high ? -amplitude : amplitude);
            painter.drawLine(x2, y, x2, nextY);
        }
        
        // Revolution label
        painter.setPen(Qt::white);
        painter.drawText(5, baseY, QString("Rev %1").arg(r));
    }
}

void FluxVisualizerWidget::drawGrid(QPainter& painter)
{
    int plotHeight = height() - m_rulerHeight - m_statsHeight - MARGIN_TOP - MARGIN_BOTTOM;
    int plotWidth = width() - MARGIN_LEFT - MARGIN_RIGHT;
    
    painter.setPen(QPen(QColor(60, 60, 70), 1));
    
    // Horizontal lines
    for (int i = 1; i < 4; ++i) {
        int y = MARGIN_TOP + i * plotHeight / 4;
        painter.drawLine(MARGIN_LEFT, y, MARGIN_LEFT + plotWidth, y);
    }
    
    // Vertical lines (time based)
    double timeStep = 10000.0;  // 10 microseconds
    if (m_zoom > 10) timeStep = 1000.0;
    if (m_zoom > 50) timeStep = 500.0;
    
    for (double t = 0; t < m_totalTime; t += timeStep) {
        int x = timeToX(t);
        if (x >= MARGIN_LEFT && x <= MARGIN_LEFT + plotWidth) {
            painter.drawLine(x, MARGIN_TOP, x, MARGIN_TOP + plotHeight);
        }
    }
}

void FluxVisualizerWidget::drawSyncPatterns(QPainter& painter)
{
    if (m_revolutions.empty())
        return;
    
    const auto& samples = m_revolutions[0];
    int plotHeight = height() - m_rulerHeight - m_statsHeight - MARGIN_TOP - MARGIN_BOTTOM;
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 255, 0, 40));
    
    double cumTime = 0;
    bool inSync = false;
    double syncStart = 0;
    
    for (size_t i = 0; i < samples.size(); ++i) {
        if (samples[i].is_sync && !inSync) {
            inSync = true;
            syncStart = cumTime;
        } else if (!samples[i].is_sync && inSync) {
            inSync = false;
            int x1 = timeToX(syncStart);
            int x2 = timeToX(cumTime);
            painter.drawRect(x1, MARGIN_TOP, x2 - x1, plotHeight);
        }
        cumTime += samples[i].time_ns;
    }
}

void FluxVisualizerWidget::drawWeakBits(QPainter& painter)
{
    if (m_revolutions.empty())
        return;
    
    const auto& samples = m_revolutions[0];
    int plotHeight = height() - m_rulerHeight - m_statsHeight - MARGIN_TOP - MARGIN_BOTTOM;
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 0, 60));
    
    double cumTime = 0;
    
    for (size_t i = 0; i < samples.size(); ++i) {
        if (samples[i].is_weak) {
            int x = timeToX(cumTime);
            painter.drawRect(x - 2, MARGIN_TOP, 4, plotHeight);
        }
        cumTime += samples[i].time_ns;
    }
}

void FluxVisualizerWidget::drawRegions(QPainter& painter)
{
    if (m_revolutions.empty())
        return;
    
    const auto& samples = m_revolutions[0];
    int plotHeight = height() - m_rulerHeight - m_statsHeight - MARGIN_TOP - MARGIN_BOTTOM;
    
    for (const auto& region : m_regions) {
        double startTime = 0, endTime = 0;
        
        for (size_t i = 0; i < samples.size(); ++i) {
            if (i == region.start_index) startTime = endTime;
            endTime += samples[i].time_ns;
            if (i == region.end_index) break;
        }
        
        int x1 = timeToX(startTime);
        int x2 = timeToX(endTime);
        
        QColor fillColor = region.color;
        fillColor.setAlpha(50);
        painter.fillRect(x1, MARGIN_TOP, x2 - x1, plotHeight, fillColor);
        
        painter.setPen(region.color);
        painter.drawRect(x1, MARGIN_TOP, x2 - x1, plotHeight);
        
        if (!region.label.isEmpty()) {
            painter.drawText(x1 + 2, MARGIN_TOP + 12, region.label);
        }
    }
}

void FluxVisualizerWidget::drawRuler(QPainter& painter)
{
    int rulerY = height() - m_statsHeight - m_rulerHeight;
    int plotWidth = width() - MARGIN_LEFT - MARGIN_RIGHT;
    
    painter.fillRect(MARGIN_LEFT, rulerY, plotWidth, m_rulerHeight, QColor(40, 40, 45));
    
    painter.setPen(Qt::lightGray);
    
    double timeStep = 10000.0;  // 10 microseconds
    if (m_zoom > 10) timeStep = 1000.0;
    if (m_zoom > 50) timeStep = 500.0;
    
    QFont smallFont = painter.font();
    smallFont.setPointSize(8);
    painter.setFont(smallFont);
    
    for (double t = 0; t < m_totalTime; t += timeStep) {
        int x = timeToX(t);
        if (x >= MARGIN_LEFT && x <= MARGIN_LEFT + plotWidth) {
            painter.drawLine(x, rulerY, x, rulerY + 8);
            painter.drawText(x - 15, rulerY + 20, QString("%1µs").arg(t / 1000.0, 0, 'f', 1));
        }
    }
}

void FluxVisualizerWidget::drawMarker(QPainter& painter)
{
    if (m_revolutions.empty() || m_markerPosition >= m_revolutions[0].size())
        return;
    
    double time = 0;
    for (size_t i = 0; i < m_markerPosition; ++i) {
        time += m_revolutions[0][i].time_ns;
    }
    
    int x = timeToX(time);
    int plotHeight = height() - m_rulerHeight - m_statsHeight - MARGIN_TOP - MARGIN_BOTTOM;
    
    painter.setPen(QPen(Qt::cyan, 2));
    painter.drawLine(x, MARGIN_TOP, x, MARGIN_TOP + plotHeight);
}

void FluxVisualizerWidget::drawStatistics(QPainter& painter)
{
    if (!m_showStatistics)
        return;
    
    int statsY = height() - m_statsHeight;
    int statsWidth = width();
    
    painter.fillRect(0, statsY, statsWidth, m_statsHeight, QColor(35, 35, 40));
    
    painter.setPen(Qt::lightGray);
    QFont statsFont = painter.font();
    statsFont.setPointSize(9);
    painter.setFont(statsFont);
    
    int col1 = 10, col2 = 180, col3 = 350, col4 = 520;
    int row1 = statsY + 18, row2 = statsY + 38;
    
    painter.drawText(col1, row1, QString("Samples: %1").arg(m_stats.sample_count));
    painter.drawText(col1, row2, QString("RPM: %1").arg(m_stats.rpm, 0, 'f', 1));
    
    painter.drawText(col2, row1, QString("Mean: %1 ns").arg(m_stats.mean_time, 0, 'f', 0));
    painter.drawText(col2, row2, QString("StdDev: %1 ns").arg(m_stats.std_dev, 0, 'f', 0));
    
    painter.drawText(col3, row1, QString("Min: %1 ns").arg(m_stats.min_time, 0, 'f', 0));
    painter.drawText(col3, row2, QString("Max: %1 ns").arg(m_stats.max_time, 0, 'f', 0));
    
    painter.setPen(QColor(255, 255, 0));
    painter.drawText(col4, row1, QString("Weak: %1").arg(m_stats.weak_count));
    painter.setPen(QColor(0, 255, 0));
    painter.drawText(col4, row2, QString("Sync: %1").arg(m_stats.sync_count));
}

// ============================================================================
// Mouse Events
// ============================================================================

void FluxVisualizerWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_lastMousePos = event->pos();
        
        size_t index = xToSampleIndex(event->pos().x());
        if (!m_revolutions.empty() && index < m_revolutions[0].size()) {
            m_markerPosition = index;
            emit sampleClicked(index, m_revolutions[0][index].time_ns);
            update();
        }
    } else if (event->button() == Qt::RightButton) {
        m_isSelecting = true;
        m_selectionStart = xToSampleIndex(event->pos().x());
        m_selectionEnd = m_selectionStart;
    }
}

void FluxVisualizerWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    } else if (event->button() == Qt::RightButton) {
        m_isSelecting = false;
        if (m_selectionStart != m_selectionEnd) {
            emit regionSelected(std::min(m_selectionStart, m_selectionEnd),
                              std::max(m_selectionStart, m_selectionEnd));
        }
    }
}

void FluxVisualizerWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        int dx = event->pos().x() - m_lastMousePos.x();
        double dTime = -dx * 1000.0 / m_zoom;
        setOffset(m_offset + dTime);
        m_lastMousePos = event->pos();
    } else if (m_isSelecting) {
        m_selectionEnd = xToSampleIndex(event->pos().x());
        update();
    }
}

void FluxVisualizerWidget::wheelEvent(QWheelEvent *event)
{
    double factor = event->angleDelta().y() > 0 ? 1.2 : 0.8;
    
    // Zoom centered on mouse position
    double mouseTime = xToTime(event->position().x());
    setZoom(m_zoom * factor);
    
    // Adjust offset to keep mouse position stable
    double newMouseTime = xToTime(event->position().x());
    setOffset(m_offset + (mouseTime - newMouseTime));
}

void FluxVisualizerWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    update();
}

void FluxVisualizerWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomIn();
            break;
        case Qt::Key_Minus:
            zoomOut();
            break;
        case Qt::Key_0:
            zoomToFit();
            break;
        case Qt::Key_Left:
            setOffset(m_offset - 10000);
            break;
        case Qt::Key_Right:
            setOffset(m_offset + 10000);
            break;
        case Qt::Key_1:
            setViewMode(FluxViewMode::WAVEFORM);
            break;
        case Qt::Key_2:
            setViewMode(FluxViewMode::HISTOGRAM);
            break;
        case Qt::Key_3:
            setViewMode(FluxViewMode::CELL_VIEW);
            break;
        case Qt::Key_4:
            setViewMode(FluxViewMode::COMPARISON);
            break;
        default:
            QWidget::keyPressEvent(event);
    }
}

// ============================================================================
// Export
// ============================================================================

QImage FluxVisualizerWidget::exportToImage(int w, int h)
{
    if (w <= 0) w = width();
    if (h <= 0) h = height();
    
    QImage image(w, h, QImage::Format_RGB32);
    QPainter painter(&image);
    
    // Temporarily resize for export
    QSize oldSize = size();
    resize(w, h);
    render(&painter);
    resize(oldSize);
    
    return image;
}

QString FluxVisualizerWidget::exportToCSV() const
{
    QString csv;
    csv += "Index,Time_ns,Revolution,Is_Weak,Is_Sync\n";
    
    for (size_t r = 0; r < m_revolutions.size(); ++r) {
        const auto& samples = m_revolutions[r];
        for (size_t i = 0; i < samples.size(); ++i) {
            csv += QString("%1,%2,%3,%4,%5\n")
                   .arg(i)
                   .arg(samples[i].time_ns)
                   .arg(r)
                   .arg(samples[i].is_weak ? 1 : 0)
                   .arg(samples[i].is_sync ? 1 : 0);
        }
    }
    
    return csv;
}

/**
 * @file uft_flux_view_widget.cpp
 * @brief Flux Data Visualization Widget Implementation
 * @version 3.3.2
 */

#include "uft_flux_view_widget.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QToolTip>
#include <QFileDialog>
#include <cmath>
#include <algorithm>

/*============================================================================
 * CONSTRUCTOR / DESTRUCTOR
 *============================================================================*/

UftFluxViewWidget::UftFluxViewWidget(QWidget* parent)
    : QWidget(parent)
    , m_totalTime(0)
    , m_viewMode(FluxViewMode::Timeline)
    , m_zoomLevel(1.0)
    , m_viewPosition(0)
    , m_nominalBitCell(2000.0)  // 2µs default (MFM DD)
    , m_selecting(false)
    , m_selectionStart(0)
    , m_selectionEnd(0)
    , m_rubberBand(nullptr)
    , m_dragging(false)
    , m_dragStartPos(0)
    , m_histogramBins(100)
    , m_histogramMax(0)
    , m_showGrid(true)
    , m_showWeakBits(true)
    , m_showSectors(true)
    , m_plotLeft(60)
    , m_plotTop(20)
    , m_plotWidth(0)
    , m_plotHeight(0)
    , m_hoverIndex(-1)
    , m_hoverTime(0)
{
    // Default colors (dark theme)
    m_fluxColor = QColor(0, 200, 100);       // Green
    m_gridColor = QColor(60, 60, 60);        // Dark gray
    m_weakBitColor = QColor(255, 100, 100);  // Red
    m_backgroundColor = QColor(30, 30, 30);  // Near black
    m_sectorColor = QColor(100, 100, 255);   // Blue
    m_selectionColor = QColor(100, 150, 200, 100);  // Semi-transparent blue

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
}

UftFluxViewWidget::~UftFluxViewWidget()
{
    delete m_rubberBand;
}

/*============================================================================
 * DATA
 *============================================================================*/

void UftFluxViewWidget::setFluxData(const QVector<uint32_t>& fluxTimes)
{
    m_fluxTimes = fluxTimes;
    m_multiRevData.clear();
    m_multiRevData.append(fluxTimes);

    // Calculate total time
    m_totalTime = 0;
    for (uint32_t t : fluxTimes) {
        m_totalTime += t;
    }

    calculateHistogram();
    zoomToFit();
    update();
}

void UftFluxViewWidget::setMultiRevFluxData(const QVector<QVector<uint32_t>>& revolutions)
{
    m_multiRevData = revolutions;

    // Use first revolution as primary
    if (!revolutions.isEmpty()) {
        m_fluxTimes = revolutions[0];
        m_totalTime = 0;
        for (uint32_t t : m_fluxTimes) {
            m_totalTime += t;
        }
    } else {
        m_fluxTimes.clear();
        m_totalTime = 0;
    }

    calculateHistogram();
    zoomToFit();
    update();
}

void UftFluxViewWidget::clearFluxData()
{
    m_fluxTimes.clear();
    m_multiRevData.clear();
    m_totalTime = 0;
    m_histogram.clear();
    m_histogramMax = 0;
    m_markers.clear();
    m_sectorBoundaries.clear();
    update();
}

/*============================================================================
 * VIEW SETTINGS
 *============================================================================*/

void UftFluxViewWidget::setViewMode(FluxViewMode mode)
{
    if (m_viewMode != mode) {
        m_viewMode = mode;
        update();
    }
}

void UftFluxViewWidget::setZoomLevel(double zoom)
{
    zoom = qBound(0.01, zoom, 1000.0);
    if (!qFuzzyCompare(m_zoomLevel, zoom)) {
        m_zoomLevel = zoom;
        updateScrollBar();
        emit zoomChanged(zoom);
        emit viewChanged(m_viewPosition, m_zoomLevel);
        update();
    }
}

void UftFluxViewWidget::setViewPosition(int64_t pos)
{
    pos = qBound(int64_t(0), pos, int64_t(m_fluxTimes.size()));
    if (m_viewPosition != pos) {
        m_viewPosition = pos;
        emit viewChanged(m_viewPosition, m_zoomLevel);
        update();
    }
}

void UftFluxViewWidget::setNominalBitCell(double ns)
{
    m_nominalBitCell = ns;
    calculateHistogram();
    update();
}

/*============================================================================
 * MARKERS
 *============================================================================*/

void UftFluxViewWidget::addMarker(const FluxMarker& marker)
{
    m_markers.append(marker);
    update();
}

void UftFluxViewWidget::clearMarkers()
{
    m_markers.clear();
    update();
}

void UftFluxViewWidget::setSectorBoundaries(const QVector<int64_t>& boundaries)
{
    m_sectorBoundaries = boundaries;
    update();
}

/*============================================================================
 * DISPLAY OPTIONS
 *============================================================================*/

void UftFluxViewWidget::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

void UftFluxViewWidget::setShowWeakBits(bool show)
{
    m_showWeakBits = show;
    update();
}

void UftFluxViewWidget::setShowSectors(bool show)
{
    m_showSectors = show;
    update();
}

void UftFluxViewWidget::setHistogramBins(int bins)
{
    m_histogramBins = qBound(10, bins, 1000);
    calculateHistogram();
    update();
}

/*============================================================================
 * GEOMETRY
 *============================================================================*/

QSize UftFluxViewWidget::minimumSizeHint() const
{
    return QSize(400, 200);
}

QSize UftFluxViewWidget::sizeHint() const
{
    return QSize(800, 300);
}

/*============================================================================
 * SLOTS
 *============================================================================*/

void UftFluxViewWidget::zoomToFit()
{
    m_zoomLevel = 1.0;
    m_viewPosition = 0;
    emit zoomChanged(m_zoomLevel);
    emit viewChanged(m_viewPosition, m_zoomLevel);
    update();
}

void UftFluxViewWidget::zoomIn(double factor)
{
    // Zoom centered on current view center
    int64_t centerIndex = m_viewPosition + (m_fluxTimes.size() / m_zoomLevel) / 2;
    setZoomLevel(m_zoomLevel * factor);

    // Adjust position to keep center
    int64_t visibleFlux = m_fluxTimes.size() / m_zoomLevel;
    setViewPosition(centerIndex - visibleFlux / 2);
}

void UftFluxViewWidget::zoomOut(double factor)
{
    zoomIn(1.0 / factor);
}

void UftFluxViewWidget::scrollToFlux(int64_t index)
{
    setViewPosition(index);
}

void UftFluxViewWidget::scrollToTime(int64_t timeNs)
{
    // Find flux index at time
    int64_t accum = 0;
    for (int i = 0; i < m_fluxTimes.size(); ++i) {
        accum += m_fluxTimes[i];
        if (accum >= timeNs) {
            scrollToFlux(i);
            return;
        }
    }
}

void UftFluxViewWidget::scrollToSector(int sector)
{
    if (sector >= 0 && sector < m_sectorBoundaries.size()) {
        scrollToFlux(m_sectorBoundaries[sector]);
    }
}

void UftFluxViewWidget::exportImage(const QString& path)
{
    QPixmap pixmap(size());
    render(&pixmap);
    pixmap.save(path);
}

/*============================================================================
 * PAINT EVENT
 *============================================================================*/

void UftFluxViewWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Calculate plot area
    m_plotLeft = 60;
    m_plotTop = 20;
    m_plotWidth = width() - m_plotLeft - 20;
    m_plotHeight = height() - m_plotTop - 40;

    // Background
    painter.fillRect(rect(), m_backgroundColor);

    if (m_fluxTimes.isEmpty()) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, tr("No flux data loaded"));
        return;
    }

    // Draw grid
    if (m_showGrid) {
        paintGrid(painter);
    }

    // Draw main content based on view mode
    switch (m_viewMode) {
        case FluxViewMode::Timeline:
            paintTimeline(painter);
            break;
        case FluxViewMode::Histogram:
            paintHistogram(painter);
            break;
        case FluxViewMode::Overlay:
            paintOverlay(painter);
            break;
        case FluxViewMode::Difference:
            paintDifference(painter);
            break;
    }

    // Draw markers
    paintMarkers(painter);

    // Draw info overlay
    paintInfoOverlay(painter);
}

void UftFluxViewWidget::paintTimeline(QPainter& painter)
{
    if (m_plotWidth <= 0 || m_fluxTimes.isEmpty()) {
        return;
    }

    // Calculate visible range
    int64_t visibleFlux = static_cast<int64_t>(m_fluxTimes.size() / m_zoomLevel);
    int64_t startIdx = m_viewPosition;
    int64_t endIdx = qMin(startIdx + visibleFlux, int64_t(m_fluxTimes.size()));

    if (endIdx <= startIdx) {
        return;
    }

    // Draw flux transitions as vertical lines
    painter.setPen(QPen(m_fluxColor, 1));

    double pixelsPerFlux = static_cast<double>(m_plotWidth) / visibleFlux;
    int lastX = -1;

    for (int64_t i = startIdx; i < endIdx; ++i) {
        int x = m_plotLeft + static_cast<int>((i - startIdx) * pixelsPerFlux);

        // Skip if same x as last (for performance)
        if (x == lastX && pixelsPerFlux < 1.0) {
            continue;
        }

        // Normalize flux time to plot height
        double normalized = m_fluxTimes[i] / (m_nominalBitCell * 4.0);
        normalized = qBound(0.0, normalized, 1.0);

        int y1 = m_plotTop + m_plotHeight;
        int y2 = m_plotTop + static_cast<int>(m_plotHeight * (1.0 - normalized));

        painter.drawLine(x, y1, x, y2);

        lastX = x;
    }

    // Draw weak bits if enabled
    if (m_showWeakBits && m_multiRevData.size() > 1) {
        painter.setPen(QPen(m_weakBitColor, 2));

        // Find positions with high variance across revolutions
        for (int64_t i = startIdx; i < endIdx; ++i) {
            double sum = 0;
            double sumSq = 0;
            int count = 0;

            for (const auto& rev : m_multiRevData) {
                if (i < rev.size()) {
                    double val = rev[i];
                    sum += val;
                    sumSq += val * val;
                    count++;
                }
            }

            if (count > 1) {
                double mean = sum / count;
                double variance = (sumSq / count) - (mean * mean);
                double cv = (mean > 0) ? sqrt(variance) / mean : 0;

                if (cv > 0.15) {  // High variance = weak bit
                    int x = m_plotLeft + static_cast<int>((i - startIdx) * pixelsPerFlux);
                    painter.drawLine(x, m_plotTop, x, m_plotTop + m_plotHeight);
                }
            }
        }
    }

    // Draw sector boundaries
    if (m_showSectors) {
        painter.setPen(QPen(m_sectorColor, 1, Qt::DashLine));

        for (int64_t boundary : m_sectorBoundaries) {
            if (boundary >= startIdx && boundary < endIdx) {
                int x = m_plotLeft + static_cast<int>((boundary - startIdx) * pixelsPerFlux);
                painter.drawLine(x, m_plotTop, x, m_plotTop + m_plotHeight);
            }
        }
    }
}

void UftFluxViewWidget::paintHistogram(QPainter& painter)
{
    if (m_histogram.isEmpty() || m_histogramMax == 0) {
        return;
    }

    int barWidth = m_plotWidth / m_histogram.size();
    if (barWidth < 1) barWidth = 1;

    painter.setPen(Qt::NoPen);

    for (int i = 0; i < m_histogram.size(); ++i) {
        double normalized = static_cast<double>(m_histogram[i]) / m_histogramMax;
        int barHeight = static_cast<int>(normalized * m_plotHeight);

        int x = m_plotLeft + i * barWidth;
        int y = m_plotTop + m_plotHeight - barHeight;

        // Color based on bin position relative to nominal
        double binCenter = (i + 0.5) * (m_nominalBitCell * 4.0) / m_histogram.size();
        double ratio = binCenter / m_nominalBitCell;

        QColor barColor;
        if (ratio >= 0.8 && ratio <= 1.2) {
            barColor = QColor(0, 200, 100);  // Normal - green
        } else if (ratio >= 0.5 && ratio <= 1.5) {
            barColor = QColor(255, 200, 0);  // Marginal - yellow
        } else {
            barColor = QColor(255, 100, 100);  // Abnormal - red
        }

        painter.setBrush(barColor);
        painter.drawRect(x, y, barWidth - 1, barHeight);
    }

    // Draw nominal bit cell markers
    painter.setPen(QPen(Qt::white, 1, Qt::DashLine));

    for (int mult = 1; mult <= 3; ++mult) {
        double time = m_nominalBitCell * mult;
        int x = m_plotLeft + static_cast<int>(time / (m_nominalBitCell * 4.0) * m_plotWidth);

        if (x > m_plotLeft && x < m_plotLeft + m_plotWidth) {
            painter.drawLine(x, m_plotTop, x, m_plotTop + m_plotHeight);

            QString label = QString("%1T").arg(mult);
            painter.drawText(x - 10, m_plotTop - 5, label);
        }
    }
}

void UftFluxViewWidget::paintOverlay(QPainter& painter)
{
    if (m_multiRevData.isEmpty() || m_plotWidth <= 0) {
        return;
    }

    // Draw each revolution in different color
    QVector<QColor> revColors = {
        QColor(0, 200, 100),     // Green
        QColor(100, 150, 255),   // Blue
        QColor(255, 150, 100),   // Orange
        QColor(200, 100, 200),   // Purple
        QColor(255, 255, 100)    // Yellow
    };

    int64_t visibleFlux = static_cast<int64_t>(m_fluxTimes.size() / m_zoomLevel);
    int64_t startIdx = m_viewPosition;
    int64_t endIdx = qMin(startIdx + visibleFlux, int64_t(m_fluxTimes.size()));

    double pixelsPerFlux = static_cast<double>(m_plotWidth) / visibleFlux;

    for (int rev = 0; rev < m_multiRevData.size(); ++rev) {
        const auto& revData = m_multiRevData[rev];
        QColor color = revColors[rev % revColors.size()];
        color.setAlpha(150);

        painter.setPen(QPen(color, 1));

        for (int64_t i = startIdx; i < endIdx && i < revData.size(); ++i) {
            int x = m_plotLeft + static_cast<int>((i - startIdx) * pixelsPerFlux);

            double normalized = revData[i] / (m_nominalBitCell * 4.0);
            normalized = qBound(0.0, normalized, 1.0);

            int y1 = m_plotTop + m_plotHeight;
            int y2 = m_plotTop + static_cast<int>(m_plotHeight * (1.0 - normalized));

            painter.drawLine(x, y1, x, y2);
        }
    }
}

void UftFluxViewWidget::paintDifference(QPainter& painter)
{
    if (m_multiRevData.size() < 2 || m_plotWidth <= 0) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, tr("Need multiple revolutions"));
        return;
    }

    int64_t visibleFlux = static_cast<int64_t>(m_fluxTimes.size() / m_zoomLevel);
    int64_t startIdx = m_viewPosition;
    int64_t endIdx = qMin(startIdx + visibleFlux, int64_t(m_fluxTimes.size()));

    double pixelsPerFlux = static_cast<double>(m_plotWidth) / visibleFlux;

    // Draw difference between first two revolutions
    const auto& rev0 = m_multiRevData[0];
    const auto& rev1 = m_multiRevData[1];

    int centerY = m_plotTop + m_plotHeight / 2;

    painter.setPen(QPen(m_fluxColor, 1));

    for (int64_t i = startIdx; i < endIdx; ++i) {
        if (i >= rev0.size() || i >= rev1.size()) {
            break;
        }

        int x = m_plotLeft + static_cast<int>((i - startIdx) * pixelsPerFlux);

        double diff = static_cast<double>(rev1[i]) - static_cast<double>(rev0[i]);
        double normalizedDiff = diff / m_nominalBitCell;
        normalizedDiff = qBound(-1.0, normalizedDiff, 1.0);

        int y = centerY - static_cast<int>(normalizedDiff * m_plotHeight / 2);

        painter.drawLine(x, centerY, x, y);
    }

    // Draw zero line
    painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
    painter.drawLine(m_plotLeft, centerY, m_plotLeft + m_plotWidth, centerY);
}

void UftFluxViewWidget::paintMarkers(QPainter& painter)
{
    int64_t visibleFlux = static_cast<int64_t>(m_fluxTimes.size() / m_zoomLevel);
    int64_t startIdx = m_viewPosition;
    int64_t endIdx = startIdx + visibleFlux;

    double pixelsPerFlux = static_cast<double>(m_plotWidth) / visibleFlux;

    for (const auto& marker : m_markers) {
        if (marker.position >= startIdx && marker.position < endIdx) {
            int x = m_plotLeft + static_cast<int>((marker.position - startIdx) * pixelsPerFlux);

            painter.setPen(QPen(marker.color, 2));
            painter.drawLine(x, m_plotTop, x, m_plotTop + m_plotHeight);

            if (!marker.label.isEmpty()) {
                painter.drawText(x + 2, m_plotTop + 12, marker.label);
            }
        }
    }
}

void UftFluxViewWidget::paintGrid(QPainter& painter)
{
    painter.setPen(QPen(m_gridColor, 1));

    // Horizontal grid lines (5 divisions)
    for (int i = 0; i <= 4; ++i) {
        int y = m_plotTop + (m_plotHeight * i) / 4;
        painter.drawLine(m_plotLeft, y, m_plotLeft + m_plotWidth, y);

        // Y-axis labels
        double time = m_nominalBitCell * (4 - i);  // 4T to 0
        QString label = QString("%1 µs").arg(time / 1000.0, 0, 'f', 1);
        painter.drawText(5, y + 4, label);
    }

    // Vertical time axis
    painter.drawLine(m_plotLeft, m_plotTop, m_plotLeft, m_plotTop + m_plotHeight);
    painter.drawLine(m_plotLeft, m_plotTop + m_plotHeight,
                     m_plotLeft + m_plotWidth, m_plotTop + m_plotHeight);
}

void UftFluxViewWidget::paintInfoOverlay(QPainter& painter)
{
    // Info box in top-right corner
    painter.setPen(Qt::white);
    painter.setFont(QFont("Monospace", 9));

    QStringList info;
    info << QString("Flux: %1").arg(m_fluxTimes.size());
    info << QString("Time: %1 ms").arg(m_totalTime / 1000000.0, 0, 'f', 2);
    info << QString("Zoom: %1x").arg(m_zoomLevel, 0, 'f', 1);

    if (m_hoverIndex >= 0 && m_hoverIndex < m_fluxTimes.size()) {
        info << QString("Hover: %1 ns").arg(m_fluxTimes[m_hoverIndex]);
    }

    int x = m_plotLeft + m_plotWidth - 100;
    int y = m_plotTop + 15;

    for (const QString& line : info) {
        painter.drawText(x, y, line);
        y += 14;
    }

    // View mode indicator
    QString modeStr;
    switch (m_viewMode) {
        case FluxViewMode::Timeline:   modeStr = "Timeline"; break;
        case FluxViewMode::Histogram:  modeStr = "Histogram"; break;
        case FluxViewMode::Overlay:    modeStr = "Overlay"; break;
        case FluxViewMode::Difference: modeStr = "Difference"; break;
    }
    painter.drawText(m_plotLeft + 5, m_plotTop + 15, modeStr);
}

/*============================================================================
 * MOUSE EVENTS
 *============================================================================*/

void UftFluxViewWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() & Qt::ControlModifier) {
            // Selection mode
            m_selecting = true;
            m_selectionStart = fluxIndexAt(event->pos());
            m_selectionEnd = m_selectionStart;
            m_rubberBand->setGeometry(QRect(event->pos(), QSize()));
            m_rubberBand->show();
        } else {
            // Drag mode
            m_dragging = true;
            m_dragStart = event->pos();
            m_dragStartPos = m_viewPosition;
            setCursor(Qt::ClosedHandCursor);
        }
    } else if (event->button() == Qt::RightButton) {
        // Click to get info
        int64_t idx = fluxIndexAt(event->pos());
        int64_t time = timeAt(event->pos());
        emit fluxClicked(idx, time);
    }
}

void UftFluxViewWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_selecting) {
        m_selectionEnd = fluxIndexAt(event->pos());
        m_rubberBand->setGeometry(QRect(m_dragStart, event->pos()).normalized());
    } else if (m_dragging) {
        // Pan view
        int dx = event->pos().x() - m_dragStart.x();
        int64_t visibleFlux = static_cast<int64_t>(m_fluxTimes.size() / m_zoomLevel);
        double pixelsPerFlux = static_cast<double>(m_plotWidth) / visibleFlux;

        int64_t deltaFlux = static_cast<int64_t>(-dx / pixelsPerFlux);
        setViewPosition(m_dragStartPos + deltaFlux);
    } else {
        // Hover
        m_hoverIndex = fluxIndexAt(event->pos());
        m_hoverTime = timeAt(event->pos());

        if (m_hoverIndex >= 0 && m_hoverIndex < m_fluxTimes.size()) {
            QString tip = QString("Index: %1\nTime: %2 ns")
                          .arg(m_hoverIndex)
                          .arg(m_fluxTimes[m_hoverIndex]);
            QToolTip::showText(event->globalPos(), tip, this);
            emit fluxHovered(m_hoverIndex, m_hoverTime);
        }

        update();
    }
}

void UftFluxViewWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_selecting) {
            m_selecting = false;
            m_rubberBand->hide();
            emit selectionChanged(m_selectionStart, m_selectionEnd);
        } else if (m_dragging) {
            m_dragging = false;
            setCursor(Qt::ArrowCursor);
        }
    }
}

void UftFluxViewWidget::wheelEvent(QWheelEvent* event)
{
    double delta = event->angleDelta().y();

    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom with Ctrl+Wheel
        if (delta > 0) {
            zoomIn(1.2);
        } else {
            zoomOut(1.2);
        }
    } else {
        // Scroll
        int64_t visibleFlux = static_cast<int64_t>(m_fluxTimes.size() / m_zoomLevel);
        int64_t scroll = visibleFlux / 10;

        if (delta > 0) {
            setViewPosition(m_viewPosition - scroll);
        } else {
            setViewPosition(m_viewPosition + scroll);
        }
    }

    event->accept();
}

void UftFluxViewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateScrollBar();
}

void UftFluxViewWidget::keyPressEvent(QKeyEvent* event)
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
        case Qt::Key_1:
            setViewMode(FluxViewMode::Timeline);
            break;
        case Qt::Key_2:
            setViewMode(FluxViewMode::Histogram);
            break;
        case Qt::Key_3:
            setViewMode(FluxViewMode::Overlay);
            break;
        case Qt::Key_4:
            setViewMode(FluxViewMode::Difference);
            break;
        case Qt::Key_G:
            setShowGrid(!m_showGrid);
            break;
        case Qt::Key_W:
            setShowWeakBits(!m_showWeakBits);
            break;
        case Qt::Key_S:
            setShowSectors(!m_showSectors);
            break;
        default:
            QWidget::keyPressEvent(event);
    }
}

/*============================================================================
 * HELPER METHODS
 *============================================================================*/

void UftFluxViewWidget::calculateHistogram()
{
    m_histogram.clear();
    m_histogram.resize(m_histogramBins, 0);
    m_histogramMax = 0;

    if (m_fluxTimes.isEmpty()) {
        return;
    }

    double maxTime = m_nominalBitCell * 4.0;  // 4T max

    for (uint32_t t : m_fluxTimes) {
        int bin = static_cast<int>(t / maxTime * m_histogramBins);
        bin = qBound(0, bin, m_histogramBins - 1);
        m_histogram[bin]++;
    }

    m_histogramMax = *std::max_element(m_histogram.begin(), m_histogram.end());
}

void UftFluxViewWidget::updateScrollBar()
{
    // Could update external scroll bar here
}

int64_t UftFluxViewWidget::fluxIndexAt(const QPoint& pos) const
{
    if (m_fluxTimes.isEmpty() || m_plotWidth <= 0) {
        return -1;
    }

    int x = pos.x() - m_plotLeft;
    if (x < 0 || x > m_plotWidth) {
        return -1;
    }

    int64_t visibleFlux = static_cast<int64_t>(m_fluxTimes.size() / m_zoomLevel);
    double pixelsPerFlux = static_cast<double>(m_plotWidth) / visibleFlux;

    int64_t index = m_viewPosition + static_cast<int64_t>(x / pixelsPerFlux);
    return qBound(int64_t(0), index, int64_t(m_fluxTimes.size() - 1));
}

int64_t UftFluxViewWidget::timeAt(const QPoint& pos) const
{
    int64_t idx = fluxIndexAt(pos);
    if (idx < 0) {
        return 0;
    }

    int64_t time = 0;
    for (int64_t i = 0; i < idx && i < m_fluxTimes.size(); ++i) {
        time += m_fluxTimes[i];
    }
    return time;
}

int UftFluxViewWidget::xForFluxIndex(int64_t index) const
{
    int64_t visibleFlux = static_cast<int64_t>(m_fluxTimes.size() / m_zoomLevel);
    double pixelsPerFlux = static_cast<double>(m_plotWidth) / visibleFlux;
    return m_plotLeft + static_cast<int>((index - m_viewPosition) * pixelsPerFlux);
}

int UftFluxViewWidget::xForTime(int64_t timeNs) const
{
    // Find flux index at time
    int64_t accum = 0;
    for (int i = 0; i < m_fluxTimes.size(); ++i) {
        accum += m_fluxTimes[i];
        if (accum >= timeNs) {
            return xForFluxIndex(i);
        }
    }
    return m_plotLeft + m_plotWidth;
}

/**
 * @file uft_flux_view_widget.h
 * @brief Flux Data Visualization Widget for UnifiedFloppyTool
 * @version 3.3.2
 * @date 2025-01-03
 *
 * Features:
 * - Real-time flux timing visualization
 * - Zoom and pan navigation
 * - Sector boundary markers
 * - Weak bit highlighting
 * - Histogram view
 * - Multi-revolution overlay
 */

#ifndef UFT_FLUX_VIEW_WIDGET_H
#define UFT_FLUX_VIEW_WIDGET_H

#include <QWidget>
#include <QVector>
#include <QColor>
#include <QTimer>
#include <QRubberBand>
#include <QScrollBar>

/**
 * @brief View mode for flux display
 */
enum class FluxViewMode {
    Timeline = 0,   // Time-domain waveform
    Histogram,      // Flux timing distribution
    Overlay,        // Multi-revolution overlay
    Difference      // Revolution difference view
};

/**
 * @brief Marker type for annotations
 */
enum class FluxMarkerType {
    SectorStart = 0,
    SectorEnd,
    SyncPattern,
    WeakBit,
    CRCError,
    Custom
};

/**
 * @brief Marker annotation
 */
struct FluxMarker {
    FluxMarkerType type;
    int64_t        position;    // Flux index
    QString        label;
    QColor         color;
};

/**
 * @brief Flux View Widget
 *
 * Provides interactive visualization of raw flux timing data
 * with zoom, pan, and annotation support.
 */
class UftFluxViewWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(FluxViewMode viewMode READ viewMode WRITE setViewMode)
    Q_PROPERTY(double zoomLevel READ zoomLevel WRITE setZoomLevel)
    Q_PROPERTY(int64_t viewPosition READ viewPosition WRITE setViewPosition)

public:
    explicit UftFluxViewWidget(QWidget* parent = nullptr);
    ~UftFluxViewWidget() override;

    // --- Data ---

    /**
     * @brief Set flux timing data (single revolution)
     * @param fluxTimes Vector of flux transition times (in nanoseconds)
     */
    void setFluxData(const QVector<uint32_t>& fluxTimes);

    /**
     * @brief Set flux timing data (multiple revolutions)
     * @param revolutions Vector of flux data per revolution
     */
    void setMultiRevFluxData(const QVector<QVector<uint32_t>>& revolutions);

    /**
     * @brief Clear all flux data
     */
    void clearFluxData();

    /**
     * @brief Get flux count
     */
    int fluxCount() const { return m_fluxTimes.size(); }

    /**
     * @brief Get total track time (nanoseconds)
     */
    int64_t totalTime() const { return m_totalTime; }

    // --- View Settings ---

    /**
     * @brief Get current view mode
     */
    FluxViewMode viewMode() const { return m_viewMode; }
    void setViewMode(FluxViewMode mode);

    /**
     * @brief Get zoom level (1.0 = fit all)
     */
    double zoomLevel() const { return m_zoomLevel; }
    void setZoomLevel(double zoom);

    /**
     * @brief Get view position (flux index)
     */
    int64_t viewPosition() const { return m_viewPosition; }
    void setViewPosition(int64_t pos);

    /**
     * @brief Get nominal bit cell time (ns)
     */
    double nominalBitCell() const { return m_nominalBitCell; }
    void setNominalBitCell(double ns);

    // --- Markers ---

    /**
     * @brief Add annotation marker
     */
    void addMarker(const FluxMarker& marker);

    /**
     * @brief Clear all markers
     */
    void clearMarkers();

    /**
     * @brief Set sector boundaries
     * @param boundaries Vector of flux indices where sectors start
     */
    void setSectorBoundaries(const QVector<int64_t>& boundaries);

    // --- Display Options ---

    /**
     * @brief Show/hide grid
     */
    void setShowGrid(bool show);
    bool showGrid() const { return m_showGrid; }

    /**
     * @brief Show/hide weak bits
     */
    void setShowWeakBits(bool show);
    bool showWeakBits() const { return m_showWeakBits; }

    /**
     * @brief Show/hide sector markers
     */
    void setShowSectors(bool show);
    bool showSectors() const { return m_showSectors; }

    /**
     * @brief Set histogram bin count
     */
    void setHistogramBins(int bins);
    int histogramBins() const { return m_histogramBins; }

    /**
     * @brief Set colors
     */
    void setFluxColor(const QColor& color) { m_fluxColor = color; update(); }
    void setGridColor(const QColor& color) { m_gridColor = color; update(); }
    void setWeakBitColor(const QColor& color) { m_weakBitColor = color; update(); }

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    /**
     * @brief Emitted when user clicks on flux position
     */
    void fluxClicked(int64_t fluxIndex, int64_t timeNs);

    /**
     * @brief Emitted when user hovers over flux position
     */
    void fluxHovered(int64_t fluxIndex, int64_t timeNs);

    /**
     * @brief Emitted when view changes
     */
    void viewChanged(int64_t position, double zoom);

    /**
     * @brief Emitted when zoom level changes
     */
    void zoomChanged(double zoom);

    /**
     * @brief Emitted when selection changes
     */
    void selectionChanged(int64_t start, int64_t end);

public slots:
    /**
     * @brief Zoom to fit all data
     */
    void zoomToFit();

    /**
     * @brief Zoom in by factor
     */
    void zoomIn(double factor = 2.0);

    /**
     * @brief Zoom out by factor
     */
    void zoomOut(double factor = 2.0);

    /**
     * @brief Scroll to flux index
     */
    void scrollToFlux(int64_t index);

    /**
     * @brief Scroll to time position
     */
    void scrollToTime(int64_t timeNs);

    /**
     * @brief Scroll to sector
     */
    void scrollToSector(int sector);

    /**
     * @brief Export view as image
     */
    void exportImage(const QString& path);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void paintTimeline(QPainter& painter);
    void paintHistogram(QPainter& painter);
    void paintOverlay(QPainter& painter);
    void paintDifference(QPainter& painter);
    void paintMarkers(QPainter& painter);
    void paintGrid(QPainter& painter);
    void paintInfoOverlay(QPainter& painter);

    void calculateHistogram();
    void updateScrollBar();
    int64_t fluxIndexAt(const QPoint& pos) const;
    int64_t timeAt(const QPoint& pos) const;
    int xForFluxIndex(int64_t index) const;
    int xForTime(int64_t timeNs) const;

    // --- Flux Data ---
    QVector<uint32_t>           m_fluxTimes;        // Single revolution
    QVector<QVector<uint32_t>>  m_multiRevData;     // Multi-revolution
    int64_t                     m_totalTime;        // Total track time (ns)

    // --- View State ---
    FluxViewMode m_viewMode;
    double       m_zoomLevel;       // 1.0 = fit all
    int64_t      m_viewPosition;    // Start position (flux index)
    double       m_nominalBitCell;  // Expected bit cell time (ns)

    // --- Selection ---
    bool    m_selecting;
    int64_t m_selectionStart;
    int64_t m_selectionEnd;
    QRubberBand* m_rubberBand;

    // --- Dragging ---
    bool   m_dragging;
    QPoint m_dragStart;
    int64_t m_dragStartPos;

    // --- Markers ---
    QVector<FluxMarker>  m_markers;
    QVector<int64_t>     m_sectorBoundaries;

    // --- Histogram ---
    QVector<int>  m_histogram;
    int           m_histogramBins;
    int           m_histogramMax;

    // --- Display Options ---
    bool   m_showGrid;
    bool   m_showWeakBits;
    bool   m_showSectors;

    // --- Colors ---
    QColor m_fluxColor;
    QColor m_gridColor;
    QColor m_weakBitColor;
    QColor m_backgroundColor;
    QColor m_sectorColor;
    QColor m_selectionColor;

    // --- Cached Geometry ---
    int m_plotLeft;
    int m_plotTop;
    int m_plotWidth;
    int m_plotHeight;

    // --- Hover State ---
    int64_t m_hoverIndex;
    int64_t m_hoverTime;
};

#endif // UFT_FLUX_VIEW_WIDGET_H

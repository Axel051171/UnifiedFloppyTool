/**
 * @file fluxvisualizerwidget.h
 * @brief Flux Timing Waveform Visualization Widget
 * @version 4.0.0
 * 
 * FEATURES:
 * - Real-time flux timing waveform display
 * - Multiple view modes: Histogram, Waveform, Spectrogram
 * - Zoom and pan navigation
 * - Sync pattern highlighting
 * - Weak bit visualization
 * - MFM/GCR cell boundary markers
 * - Multi-revolution overlay
 * - Export to image/data
 */

#pragma once

#include <QWidget>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>
#include <vector>
#include <cstdint>

/**
 * @enum FluxViewMode
 * @brief Visualization mode for flux data
 */
enum class FluxViewMode
{
    WAVEFORM,       // Traditional timing waveform
    HISTOGRAM,      // Timing distribution histogram
    SPECTROGRAM,    // Time-frequency spectrogram
    CELL_VIEW,      // Decoded cell boundaries
    COMPARISON      // Multi-revolution comparison
};

/**
 * @enum FluxEncoding
 * @brief Expected flux encoding
 */
enum class FluxEncoding
{
    AUTO,           // Auto-detect
    MFM,            // Modified Frequency Modulation
    FM,             // Frequency Modulation
    GCR,            // Group Coded Recording
    APPLE_GCR,      // Apple-specific GCR
    AMIGA_MFM       // Amiga MFM with sync
};

/**
 * @struct FluxSample
 * @brief Single flux timing sample
 */
struct FluxSample
{
    uint32_t time_ns;       // Time since last transition (nanoseconds)
    uint8_t  revolution;    // Revolution number
    bool     is_weak;       // Identified as weak bit
    bool     is_sync;       // Part of sync pattern
};

/**
 * @struct FluxRegion
 * @brief Marked region in flux data
 */
struct FluxRegion
{
    size_t start_index;
    size_t end_index;
    QString label;
    QColor  color;
};

/**
 * @struct FluxStatistics
 * @brief Statistics for displayed flux data
 */
struct FluxStatistics
{
    double mean_time;       // Mean transition time
    double std_dev;         // Standard deviation
    double min_time;        // Minimum time
    double max_time;        // Maximum time
    int    sample_count;    // Number of samples
    int    weak_count;      // Weak bits detected
    int    sync_count;      // Sync patterns found
    double rpm;             // Calculated RPM
};

/**
 * @class FluxVisualizerWidget
 * @brief Professional flux timing visualization widget
 */
class FluxVisualizerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FluxVisualizerWidget(QWidget *parent = nullptr);
    virtual ~FluxVisualizerWidget();
    
    // Data loading
    void setFluxData(const std::vector<uint32_t>& timings, int revolution = 0);
    void addRevolution(const std::vector<uint32_t>& timings);
    void clearData();
    
    // View settings
    void setViewMode(FluxViewMode mode);
    FluxViewMode viewMode() const { return m_viewMode; }
    
    void setEncoding(FluxEncoding encoding);
    FluxEncoding encoding() const { return m_encoding; }
    
    // Navigation
    void setZoom(double zoom);
    double zoom() const { return m_zoom; }
    void setOffset(double offset);
    double offset() const { return m_offset; }
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void scrollTo(size_t sampleIndex);
    
    // Display options
    void setShowGrid(bool show);
    void setShowCellBoundaries(bool show);
    void setShowSyncPatterns(bool show);
    void setShowWeakBits(bool show);
    void setShowStatistics(bool show);
    void setCellTime(double time_ns);
    void setSyncPattern(uint16_t pattern);
    
    // Markers
    void addRegion(const FluxRegion& region);
    void clearRegions();
    void setMarkerPosition(size_t index);
    
    // Statistics
    FluxStatistics getStatistics() const;
    
    // Export
    QImage exportToImage(int width = 0, int height = 0);
    QString exportToCSV() const;
    
signals:
    void sampleClicked(size_t index, uint32_t time_ns);
    void regionSelected(size_t startIndex, size_t endIndex);
    void zoomChanged(double zoom);
    void viewModeChanged(FluxViewMode mode);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    // Drawing functions
    void drawWaveform(QPainter& painter);
    void drawHistogram(QPainter& painter);
    void drawSpectrogram(QPainter& painter);
    void drawCellView(QPainter& painter);
    void drawComparison(QPainter& painter);
    void drawGrid(QPainter& painter);
    void drawSyncPatterns(QPainter& painter);
    void drawWeakBits(QPainter& painter);
    void drawRegions(QPainter& painter);
    void drawStatistics(QPainter& painter);
    void drawRuler(QPainter& painter);
    void drawMarker(QPainter& painter);
    
    // Helper functions
    int timeToX(double time_ns) const;
    double xToTime(int x) const;
    size_t xToSampleIndex(int x) const;
    void updateStatistics();
    void detectSyncPatterns();
    void detectWeakBits();
    QColor getTimingColor(uint32_t time_ns) const;
    
    // Data
    std::vector<std::vector<FluxSample>> m_revolutions;
    std::vector<FluxRegion> m_regions;
    FluxStatistics m_stats;
    
    // View state
    FluxViewMode m_viewMode;
    FluxEncoding m_encoding;
    double m_zoom;
    double m_offset;
    double m_cellTime;
    uint16_t m_syncPattern;
    
    // Display options
    bool m_showGrid;
    bool m_showCellBoundaries;
    bool m_showSyncPatterns;
    bool m_showWeakBits;
    bool m_showStatistics;
    
    // Interaction state
    bool m_isDragging;
    QPoint m_lastMousePos;
    size_t m_markerPosition;
    size_t m_selectionStart;
    size_t m_selectionEnd;
    bool m_isSelecting;
    
    // Cached values
    double m_totalTime;
    double m_visibleTime;
    int m_rulerHeight;
    int m_statsHeight;
};

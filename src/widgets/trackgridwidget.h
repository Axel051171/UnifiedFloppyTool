/**
 * @file trackgridwidget.h
 * @brief Track Status Grid Widget with Heatmap Support
 * @version 4.0.0
 * 
 * FEATURES:
 * - Compact track grid (Side 0 + Side 1)
 * - Multiple view modes: Status, Confidence, Timing, Protection
 * - Heatmap color gradients
 * - Real-time updates during read/write
 * - Clickable tracks for selection
 * - Context menu for track-local actions
 * - Tooltips with detailed track info
 * - Legend display
 */

#pragma once

#include <QWidget>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QMenu>
#include <vector>
#include <cstdint>

/**
 * @enum TrackStatus
 * @brief Status of a single track
 */
enum class TrackStatus
{
    UNKNOWN,        // Gray - not yet read
    READING,        // Blue - currently reading
    GOOD,           // Green - read successfully
    WARNING,        // Yellow - some errors, but readable
    PARTIAL,        // Orange-Yellow - partial recovery
    ERROR,          // Red - unreadable
    PROTECTED,      // Orange - copy protection detected
    WRITING,        // Purple - currently writing
    VERIFIED        // Bright Green - verified after write
};

/**
 * @enum HeatmapMode
 * @brief View mode for the track grid
 */
enum class HeatmapMode
{
    STATUS,         // Default: color by read status
    CONFIDENCE,     // Color by confidence percentage (0-100)
    TIMING,         // Color by timing variance
    PROTECTION,     // Color by protection type
    RETRIES,        // Color by retry count
    SECTORS         // Color by sector success ratio
};

/**
 * @enum ProtectionType
 * @brief Detected copy protection type
 */
enum class ProtectionType
{
    NONE = 0,
    WEAK_BITS,
    FUZZY_BITS,
    LONG_TRACK,
    EXTRA_SECTORS,
    MISSING_SECTORS,
    BAD_CRC,
    VMAX,
    RAPIDLOK,
    COPYLOCK,
    SPEEDLOCK,
    CUSTOM
};

/**
 * @struct TrackGridTrackInfo
 * @brief Extended information about a single track
 */
struct TrackGridTrackInfo
{
    // Basic info
    int cylinder;
    int head;
    TrackStatus status;
    
    // Sector info
    int goodSectors;
    int totalSectors;
    int recoveredSectors;
    
    // Recovery info
    int retries;
    int passCount;          // Multi-pass read count
    
    // Heatmap data
    int confidence;         // 0-100%
    float timingVariance;   // Timing variance (0.0 = perfect)
    ProtectionType protection;
    
    // Messages
    QString errorMessage;
    QString protectionName;
    
    // Weak bits
    int weakBitCount;
    bool hasWeakBits;
    
    TrackGridTrackInfo();
};

/**
 * @class TrackGridWidget
 * @brief Professional track grid widget with heatmap visualization
 */
class TrackGridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrackGridWidget(QWidget *parent = nullptr);
    
    // Geometry
    void setDiskGeometry(int cylinders, int heads);
    void reset();
    
    // Track updates
    void updateTrackStatus(int cylinder, int head, TrackStatus status,
                          int goodSectors = 0, int totalSectors = 0);
    void updateTrackConfidence(int cylinder, int head, int confidence);
    void updateTrackTiming(int cylinder, int head, float variance);
    void updateTrackProtection(int cylinder, int head, ProtectionType type,
                               const QString& name = QString());
    void updateTrackRetries(int cylinder, int head, int retries, int passes = 1);
    void updateTrackWeakBits(int cylinder, int head, int count);
    void updateTrackRecovered(int cylinder, int head, int recoveredSectors);
    
    // Bulk update
    void setTrackInfo(int cylinder, int head, const TrackGridTrackInfo& info);
    const TrackGridTrackInfo* getTrackInfo(int cylinder, int head) const;
    
    // Selection
    void setSelectedTrack(int cylinder, int head);
    void clearSelection();
    int selectedCylinder() const { return m_selectedCylinder; }
    int selectedHead() const { return m_selectedHead; }
    
    // View mode
    void setHeatmapMode(HeatmapMode mode);
    HeatmapMode heatmapMode() const { return m_heatmapMode; }
    
    // Display options
    void setCellSize(int size);
    void setShowLabels(bool show);
    void setShowLegend(bool show);
    void setShowConfidenceText(bool show);
    void setCompactMode(bool compact);
    
    // Statistics
    int totalTracks() const { return m_maxCylinders * m_maxHeads; }
    int goodTracks() const;
    int warningTracks() const;
    int errorTracks() const;
    int protectedTracks() const;
    float averageConfidence() const;

signals:
    void trackClicked(int cylinder, int head);
    void trackDoubleClicked(int cylinder, int head);
    void trackContextMenu(int cylinder, int head, const QPoint& globalPos);
    void selectionChanged(int cylinder, int head);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void initializeTracks();
    QSize calculateMinimumSize() const;
    void drawTrackGrid(QPainter& painter, int startX, int startY, 
                       int head, const QString& title);
    void drawLegend(QPainter& painter, int x, int y);
    void drawHeatmapLegend(QPainter& painter, int x, int y);
    
    // Color functions
    QColor getColorForStatus(TrackStatus status) const;
    QColor getColorForConfidence(int confidence) const;
    QColor getColorForTiming(float variance) const;
    QColor getColorForProtection(ProtectionType type) const;
    QColor getColorForRetries(int retries) const;
    QColor getColorForSectors(int good, int total) const;
    QColor getHeatmapColor(const TrackGridTrackInfo& info) const;
    QColor interpolateColor(const QColor& from, const QColor& to, float t) const;
    
    // Hit testing
    bool getTrackAtPos(const QPoint& pos, int* cylinder, int* head) const;
    void showTrackTooltip(const QPoint& globalPos, int cylinder, int head);
    QString formatTrackTooltip(const TrackGridTrackInfo& info) const;
    
    // Data
    int m_maxCylinders;
    int m_maxHeads;
    int m_selectedCylinder;
    int m_selectedHead;
    int m_hoveredCylinder;
    int m_hoveredHead;
    
    // Display settings
    int m_cellSize;
    bool m_showLabels;
    bool m_showLegend;
    bool m_showConfidenceText;
    bool m_compactMode;
    HeatmapMode m_heatmapMode;
    
    // Track data
    std::vector<TrackGridTrackInfo> m_tracks;
};

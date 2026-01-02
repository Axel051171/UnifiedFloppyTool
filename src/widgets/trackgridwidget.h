/**
 * @file trackgridwidget.h
 * @brief Track Status Grid Widget (für Hauptfenster-Integration)
 * @version 3.0.0
 * 
 * FEATURES:
 * - Compact track grid (Side 0 + Side 1)
 * - Color-coded status (Green/Yellow/Red/Gray)
 * - Real-time updates during read/write
 * - Clickable tracks for selection
 * - Tooltips with track details
 * - Fits in sidebar or bottom panel
 */

#pragma once

#include <QWidget>
#include <QPaintEvent>
#include <QMouseEvent>
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
    ERROR,          // Red - unreadable
    PROTECTED       // Orange - copy protection detected
};

/**
 * @struct TrackInfo
 * @brief Information about a single track
 */
struct TrackGridTrackInfo
{
    int cylinder;
    int head;
    TrackStatus status;
    int goodSectors;
    int totalSectors;
    int retries;
    QString errorMessage;
    
    TrackGridTrackInfo();
};

/**
 * @class TrackGridWidget
 * @brief Compact track grid for main window
 */
class TrackGridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrackGridWidget(QWidget *parent = nullptr);
    
    void setGeometry(int cylinders, int heads);
    void updateTrackStatus(int cylinder, int head, TrackStatus status,
                          int goodSectors = 0, int totalSectors = 0);
    void reset();
    void setSelectedTrack(int cylinder, int head);
    void setCellSize(int size);
    void setShowLabels(bool show);

signals:
    void trackClicked(int cylinder, int head);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void initializeTracks();
    QSize calculateMinimumSize() const;
    void drawTrackGrid(QPainter& painter, int startX, int startY, 
                       int head, const QString& title);
    QColor getColorForStatus(TrackStatus status) const;
    bool getTrackAtPos(const QPoint& pos, int* cylinder, int* head) const;
    void showTrackTooltip(const QPoint& globalPos, int cylinder, int head);
    
    int m_maxCylinders;
    int m_maxHeads;
    int m_selectedCylinder;
    int m_selectedHead;
    int m_cellSize;
    bool m_showLabels;
    
    std::vector<TrackGridTrackInfo> m_tracks;
};

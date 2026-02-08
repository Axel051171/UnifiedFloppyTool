/**
 * @file uft_track_grid_widget.h
 * @brief Track Grid Widget - Visual Track/Sector Status Display
 * 
 * Similar to UFT HFE Format track visualization
 */

#ifndef UFT_TRACK_GRID_WIDGET_H
#define UFT_TRACK_GRID_WIDGET_H

#include <QWidget>
#include <QMap>
#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>

class UftTrackGridWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int tracks READ tracks WRITE setTracks)
    Q_PROPERTY(int sides READ sides WRITE setSides)
    Q_PROPERTY(int sectorsPerTrack READ sectorsPerTrack WRITE setSectorsPerTrack)

public:
    explicit UftTrackGridWidget(QWidget *parent = nullptr);

    /* Geometry */
    int tracks() const { return m_tracks; }
    void setTracks(int tracks);
    
    int sides() const { return m_sides; }
    void setSides(int sides);
    
    int sectorsPerTrack() const { return m_sectorsPerTrack; }
    void setSectorsPerTrack(int sectors);
    
    void setGeometry(int tracks, int sides, int sectorsPerTrack);

    /* Track/Sector Status */
    enum Status {
        STATUS_UNKNOWN = 0,     /* Gray */
        STATUS_GOOD,            /* Green */
        STATUS_BAD,             /* Red */
        STATUS_WARNING,         /* Yellow */
        STATUS_READING,         /* Blue */
        STATUS_WRITING,         /* Cyan */
        STATUS_WEAK,            /* Orange */
        STATUS_EMPTY,           /* White */
        STATUS_PROTECTED,       /* Purple */
        STATUS_MODIFIED         /* Pink */
    };

    void setTrackStatus(int track, int side, Status status);
    Status getTrackStatus(int track, int side) const;
    
    void setSectorStatus(int track, int side, int sector, Status status);
    Status getSectorStatus(int track, int side, int sector) const;
    
    void setAllStatus(Status status);
    void clearAll();

    /* Display Options */
    void setShowSectors(bool show);
    bool showSectors() const { return m_showSectors; }
    
    void setShowLabels(bool show);
    bool showLabels() const { return m_showLabels; }
    
    void setShowTooltips(bool show);
    bool showTooltips() const { return m_showTooltips; }

    /* Selection */
    void setSelectedTrack(int track, int side);
    void getSelectedTrack(int &track, int &side) const;
    
    void setSelectedSector(int track, int side, int sector);
    void getSelectedSector(int &track, int &side, int &sector) const;

signals:
    void trackClicked(int track, int side);
    void trackDoubleClicked(int track, int side);
    void sectorClicked(int track, int side, int sector);
    void sectorDoubleClicked(int track, int side, int sector);
    void selectionChanged(int track, int side);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    struct CellInfo {
        int track;
        int side;
        int sector;     /* -1 for track-level */
        QRect rect;
    };

    void calculateLayout();
    CellInfo cellAtPosition(const QPoint &pos) const;
    QColor statusColor(Status status) const;
    QString statusTooltip(int track, int side, int sector = -1) const;

    /* Geometry */
    int m_tracks;
    int m_sides;
    int m_sectorsPerTrack;
    
    /* Status Maps */
    QMap<QPair<int,int>, Status> m_trackStatus;
    QMap<std::tuple<int,int,int>, Status> m_sectorStatus;
    
    /* Display */
    bool m_showSectors;
    bool m_showLabels;
    bool m_showTooltips;
    int m_cellWidth;
    int m_cellHeight;
    int m_headerHeight;
    int m_labelWidth;
    
    /* Selection */
    int m_selectedTrack;
    int m_selectedSide;
    int m_selectedSector;
    int m_hoverTrack;
    int m_hoverSide;
    int m_hoverSector;
};

/* ============================================================================
 * Status Colors
 * ============================================================================ */

inline QColor UftTrackGridWidget::statusColor(Status status) const
{
    switch (status) {
        case STATUS_UNKNOWN:    return QColor(200, 200, 200);   /* Gray */
        case STATUS_GOOD:       return QColor(100, 200, 100);   /* Green */
        case STATUS_BAD:        return QColor(200, 80, 80);     /* Red */
        case STATUS_WARNING:    return QColor(230, 200, 80);    /* Yellow */
        case STATUS_READING:    return QColor(100, 150, 230);   /* Blue */
        case STATUS_WRITING:    return QColor(100, 200, 230);   /* Cyan */
        case STATUS_WEAK:       return QColor(230, 150, 80);    /* Orange */
        case STATUS_EMPTY:      return QColor(250, 250, 250);   /* White */
        case STATUS_PROTECTED:  return QColor(180, 100, 200);   /* Purple */
        case STATUS_MODIFIED:   return QColor(230, 150, 200);   /* Pink */
        default:                return QColor(200, 200, 200);
    }
}

#endif /* UFT_TRACK_GRID_WIDGET_H */

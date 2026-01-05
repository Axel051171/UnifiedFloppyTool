/**
 * @file trackgridwidget.h
 * @brief Custom Widget für Track/Sektor Grid Visualisierung
 * 
 * Features:
 * - Color-coded sector status
 * - Interactive hover/click
 * - Smooth animations
 * - Efficient QPainter rendering
 */

#ifndef TRACKGRIDWIDGET_H
#define TRACKGRIDWIDGET_H

#include <QWidget>
#include <QVector>
#include <QColor>
#include <QTimer>
#include <QPropertyAnimation>

/**
 * @brief Sector Status für Grid-Darstellung (X-Copy Style)
 */
enum class SectorStatus {
    Empty = 0,      // Not yet read (pending)
    HeaderBad,      // Header CRC error (H)
    DataBad,        // Data CRC error (E)  
    Ok,             // Good sector (O)
    Deleted,        // Deleted data mark (D)
    Weak,           // Weak/unstable (W)
    Protected,      // Copy protection detected (P)
    Writing,        // Currently being processed
    Verifying       // Being verified
};

/**
 * @brief Track Grid Widget
 * 
 * Zeigt eine 2D-Grid-Visualisierung aller Tracks/Sektoren.
 * Farbkodierung nach Status, mit Hover-Effekten und Tooltips.
 */
class TrackGridWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int tracks READ tracks WRITE setTracks)
    Q_PROPERTY(int heads READ heads WRITE setHeads)
    Q_PROPERTY(int sectorsPerTrack READ sectorsPerTrack WRITE setSectorsPerTrack)

public:
    explicit TrackGridWidget(QWidget* parent = nullptr);
    ~TrackGridWidget() override;

    // Geometry
    int tracks() const { return m_tracks; }
    void setTracks(int tracks);
    
    int heads() const { return m_heads; }
    void setHeads(int heads);
    
    int sectorsPerTrack() const { return m_sectorsPerTrack; }
    void setSectorsPerTrack(int spt);

    // Data
    void updateSector(int track, int head, int sector, int status);
    void updateTrack(int track, int head, int goodSectors, int badSectors);
    void clear();
    
    // Statistics
    int totalSectors() const;
    int goodSectors() const;
    int badSectors() const;

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    void sectorClicked(int track, int head, int sector);
    void sectorHovered(int track, int head, int sector);
    void statisticsChanged(int good, int bad, int total);
    void currentTrackChanged(int track, int head);

public slots:
    /**
     * @brief Setzt den aktuellen Track (für große Anzeige)
     */
    void setCurrentTrack(int track, int head = 0);
    
    /**
     * @brief Aktiviert/Deaktiviert große Track-Nummer-Anzeige
     */
    void setShowBigTrackNumber(bool show);
    
    /**
     * @brief Setzt Timer-Anzeige (Format: "TIME: MM:SS")
     */
    void setTimerText(const QString& text);
    
    /**
     * @brief Aktiviert/Deaktiviert Timer-Anzeige
     */
    void setShowTimer(bool show);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    int m_tracks = 80;
    int m_heads = 2;
    int m_sectorsPerTrack = 11;
    
    // Grid data: [track][head][sector] -> status
    QVector<QVector<QVector<SectorStatus>>> m_data;
    
    // Hover state
    int m_hoverTrack = -1;
    int m_hoverHead = -1;
    int m_hoverSector = -1;
    
    // Colors
    QColor m_colorEmpty;
    QColor m_colorOk;
    QColor m_colorHeaderBad;
    QColor m_colorDataBad;
    QColor m_colorWeak;
    QColor m_colorProtected;
    QColor m_colorDeleted;
    QColor m_colorWriting;
    QColor m_colorVerifying;
    QColor m_colorHover;
    QColor m_colorGrid;
    
    // X-Copy Style Features
    int m_currentTrack = -1;
    int m_currentHead = -1;
    bool m_showBigTrackNumber = true;
    bool m_showTimer = true;
    QString m_timerText = "TIME: 00:00";
    QFont m_bigTrackFont;    // 48pt font for track number
    QFont m_timerFont;       // 24pt font for timer
    
    // Cached geometry
    int m_cellWidth = 0;
    int m_cellHeight = 0;
    int m_offsetX = 0;
    int m_offsetY = 0;
    
    void initializeData();
    void calculateGeometry();
    QColor colorForStatus(SectorStatus status) const;
    void sectorAt(const QPoint& pos, int& track, int& head, int& sector) const;
    QRect sectorRect(int track, int head, int sector) const;
    void updateStatistics();
};

#endif // TRACKGRIDWIDGET_H

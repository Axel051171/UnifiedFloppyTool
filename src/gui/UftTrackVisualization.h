/**
 * @file UftTrackVisualization.h
 * @brief Track/Sector Visualization Widget
 * 
 * P3-006: GUI Visualization
 * 
 * Visualizes:
 * - Disk surface map (tracks x sectors)
 * - Sector status (good/weak/bad)
 * - Read progress
 * - Timing variations
 */

#ifndef UFT_TRACK_VISUALIZATION_H
#define UFT_TRACK_VISUALIZATION_H

#include <QWidget>
#include <QColor>
#include <QMap>
#include <QPair>

/**
 * @brief Sector status for visualization
 */
enum class UftSectorStatus {
    Unknown,        // Not yet read
    Good,           // Successfully read
    Weak,           // Read with weak bits
    CrcError,       // CRC error but data recovered
    Bad,            // Unrecoverable
    Protected,      // Copy protection detected
    Progress        // Currently reading
};

/**
 * @brief Track visualization widget
 */
class UftTrackVisualization : public QWidget
{
    Q_OBJECT
    
    Q_PROPERTY(int tracks READ tracks WRITE setTracks NOTIFY layoutChanged)
    Q_PROPERTY(int maxSectors READ maxSectors WRITE setMaxSectors NOTIFY layoutChanged)
    Q_PROPERTY(int heads READ heads WRITE setHeads NOTIFY layoutChanged)

public:
    explicit UftTrackVisualization(QWidget *parent = nullptr);
    ~UftTrackVisualization();
    
    /* Geometry */
    int tracks() const { return m_tracks; }
    void setTracks(int tracks);
    
    int maxSectors() const { return m_maxSectors; }
    void setMaxSectors(int sectors);
    
    int heads() const { return m_heads; }
    void setHeads(int heads);
    
    int sectorsForTrack(int track) const;
    void setSectorsPerTrack(int track, int sectors);
    
    /* Status */
    void setSectorStatus(int head, int track, int sector, UftSectorStatus status);
    UftSectorStatus sectorStatus(int head, int track, int sector) const;
    void clearAll();
    void reset();
    
    /* Progress */
    void setCurrentPosition(int head, int track, int sector);
    void clearProgress();
    
    /* Display options */
    void setShowGrid(bool show);
    void setShowLabels(bool show);
    void setColorScheme(bool dark);
    
    /* Statistics */
    int goodCount() const { return m_goodCount; }
    int weakCount() const { return m_weakCount; }
    int badCount() const { return m_badCount; }
    int totalCount() const { return m_totalCount; }
    
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void layoutChanged();
    void sectorClicked(int head, int track, int sector);
    void statisticsChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct SectorKey {
        int head, track, sector;
        bool operator<(const SectorKey &o) const {
            if (head != o.head) return head < o.head;
            if (track != o.track) return track < o.track;
            return sector < o.sector;
        }
    };
    
    void updateStatistics();
    QRect sectorRect(int head, int track, int sector) const;
    QColor statusColor(UftSectorStatus status) const;
    bool hitTest(const QPoint &pos, int &head, int &track, int &sector) const;
    
    int m_tracks = 80;
    int m_maxSectors = 18;
    int m_heads = 2;
    QMap<int, int> m_sectorsPerTrack;
    QMap<SectorKey, UftSectorStatus> m_status;
    
    int m_currentHead = -1;
    int m_currentTrack = -1;
    int m_currentSector = -1;
    
    bool m_showGrid = true;
    bool m_showLabels = true;
    bool m_darkMode = false;
    
    int m_goodCount = 0;
    int m_weakCount = 0;
    int m_badCount = 0;
    int m_totalCount = 0;
    
    int m_hoverHead = -1;
    int m_hoverTrack = -1;
    int m_hoverSector = -1;
    
    /* Layout cache */
    int m_cellWidth = 4;
    int m_cellHeight = 6;
    int m_labelWidth = 30;
    int m_headGap = 10;
};

#endif /* UFT_TRACK_VISUALIZATION_H */

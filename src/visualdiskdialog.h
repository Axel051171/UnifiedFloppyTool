#pragma once
/**
 * @file visualdiskdialog.h
 * @brief Visual Floppy Disk Viewer Dialog
 * 
 * Based on HxCFloppyEmulator's Visual Floppy Disk view.
 * Shows dual-side polar visualization with sector status.
 * 
 * Features:
 * - Dual-side disk visualization (Side 0 / Side 1)
 * - Color-coded sector status (good/bad/weak/missing)
 * - Track/sector information panel
 * - Hex dump of selected sector
 * - Format analysis with multiple encoding detection
 * - Track/Side selection controls
 * 
 * @date 2026-01-12
 */

#ifndef VISUALDISKDIALOG_H
#define VISUALDISKDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QImage>
#include <QTimer>
#include <QMap>
#include <QPair>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QColor>
#include <QPoint>

namespace Ui {
class VisualDiskDialog;
}

class VisualDiskWidget;

class VisualDiskDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VisualDiskDialog(QWidget *parent = nullptr);
    ~VisualDiskDialog();
    
    /**
     * @brief Sector status enumeration
     */
    enum SectorStatus {
        SectorGood = 0,      // Green - valid CRC
        SectorBad,           // Red - CRC error
        SectorWeak,          // Orange - weak bits detected
        SectorMissing,       // Gray - sector not found
        SectorNoData,        // Blue - ID found but no data
        SectorAlternate,     // Yellow - alternate data
        SectorUnknown        // Dark gray - not analyzed
    };
    
    /**
     * @brief Track format types for analysis
     */
    enum TrackFormat {
        FormatUnknown = 0,
        FormatIsoMfm,
        FormatIsoFm,
        FormatAmigaMfm,
        FormatAppleGcr,
        FormatC64Gcr,
        FormatEEmu,
        FormatAed6200p,
        FormatTycom,
        FormatMembrain,
        FormatArburg
    };
    
    /**
     * @brief Sector information structure
     */
    struct SectorInfo {
        int track;
        int side;
        int sectorId;
        int size;
        SectorStatus status;
        uint16_t headerCrc;
        uint16_t dataCrc;
        bool headerCrcOk;
        bool dataCrcOk;
        int startCell;
        int endCell;
        int cellCount;
        QByteArray data;
    };
    
    /**
     * @brief Track information structure
     */
    struct TrackInfo {
        int trackNum;
        int side;
        int sectorCount;
        int goodSectors;
        int badSectors;
        int missingSectors;
        int weakSectors;
        int totalBytes;
        TrackFormat format;
        QString formatName;
        QList<SectorInfo> sectors;
    };
    
    void loadDiskImage(const QString& path);
    void setTrackData(int track, int side, const TrackInfo& info);
    void setDiskGeometry(int tracks, int sides);

signals:
    void sectorSelected(int track, int side, int sector);
    void trackSelected(int track, int side);

private slots:
    void onTrackChanged(int track);
    void onSideChanged(int side);
    void onSectorClicked(int track, int side, int sector);
    void onFormatCheckChanged();
    void onViewModeChanged();
    void onEditTools();

private:
    Ui::VisualDiskDialog *ui;
    VisualDiskWidget *m_diskWidget0;  // Side 0
    VisualDiskWidget *m_diskWidget1;  // Side 1
    
    QString m_imagePath;
    int m_totalTracks;
    int m_totalSides;
    int m_currentTrack;
    int m_currentSide;
    
    QMap<QPair<int,int>, TrackInfo> m_trackData;  // (track, side) -> info
    
    void setupConnections();
    void setupFormatCheckboxes();
    void updateStatusPanel(const SectorInfo& sector);
    void updateHexDump(const QByteArray& data);
    void updateTrackInfo(const TrackInfo& info);
    void analyzeWithFormats();
    void generateSampleData();
    void updateInfoLabels();
};

// ============================================================================
// Visual Disk Widget - Polar visualization
// ============================================================================

class VisualDiskWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit VisualDiskWidget(QWidget *parent = nullptr);
    
    void setSide(int side) { m_side = side; update(); }
    void setTracks(int tracks) { m_tracks = tracks; update(); }
    void setTrackData(int track, const VisualDiskDialog::TrackInfo& info);
    void setCurrentTrack(int track) { m_currentTrack = track; update(); }
    void clear();
    
signals:
    void sectorClicked(int track, int side, int sector);
    void trackClicked(int track, int side);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    
private:
    int m_side;
    int m_tracks;
    int m_currentTrack;
    QMap<int, VisualDiskDialog::TrackInfo> m_trackInfo;  // track -> info
    
    QColor statusColor(VisualDiskDialog::SectorStatus status) const;
    QPair<int, int> hitTest(const QPoint& pos) const;  // returns (track, sector)
};

#endif // VISUALDISKDIALOG_H

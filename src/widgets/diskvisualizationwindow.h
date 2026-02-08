/**
 * @file diskvisualizationwindow.h
 * @brief Separate Forensic Disk Visualization Window (HxC-Style)
 * @version 3.0.0
 * 
 * FEATURES:
 * - Dual-Disk circular visualization (Side 0 + Side 1)
 * - Color-coded track quality (Green=Good, Orange=Warning, Red=Error)
 * - Hex dump viewer
 * - Track/Sector analysis
 * - Format detection per track
 * - Real-time update during read
 * 
 * Disk visualization's Visual Floppy Disk
 */

#pragma once

#include <QDialog>
#include <QWidget>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QGroupBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <vector>
#include <cstdint>

/**
 * @struct TrackInfo
 * @brief Information about a single track
 */
struct TrackInfo
{
    int cylinder;
    int head;
    int quality;           // 0-100
    int errorCount;        // Number of bad sectors
    int goodSectors;
    int totalSectors;
    
    enum class Format {
        UNKNOWN,
        ISO_MFM,
        ISO_FM,
        AMIGA_MFM,
        APPLE_GCR,
        C64_GCR
    } format;
    
    QColor getColor() const;
};

/**
 * @class DualDiskWidget
 * @brief Custom widget for dual circular disk visualization
 */
class DualDiskWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DualDiskWidget(QWidget *parent = nullptr);
    
    void setTrackData(const std::vector<TrackInfo>& tracks);
    void setSelectedTrack(int track, int side);
    void setMaxTracks(int maxTracks);

signals:
    void trackClicked(int cylinder, int head);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void drawDisk(QPainter& painter, int centerX, int centerY, 
                  int radius, int side);
    
    int m_maxTracks;
    int m_selectedTrack;
    int m_selectedSide;
    std::vector<TrackInfo> m_tracks;
};

/**
 * @class DiskVisualizationWindow
 * @brief Main window for forensic disk visualization
 */
class DiskVisualizationWindow : public QDialog
{
    Q_OBJECT

public:
    explicit DiskVisualizationWindow(QWidget *parent = nullptr);
    ~DiskVisualizationWindow() override = default;

    void loadDisk(const QString& diskPath);
    void updateTrackQuality(int cylinder, int head, int quality, int errorCount);
    void setSelectedTrack(int cylinder, int head);

signals:
    void trackSelected(int cylinder, int head);
    void sectorSelected(int cylinder, int head, int sector);

private slots:
    void onTrackSpinBoxChanged(int value);
    void onSideSpinBoxChanged(int value);
    void onFormatComboChanged(int index);
    void onDiskViewModeToggled(bool checked);
    
private:
    void createUI();
    void setupConnections();
    
    // UI Components
    QWidget*      m_visualizationWidget;
    QWidget*      m_infoPanel;
    DualDiskWidget* m_diskWidget;
    
    QGroupBox*    m_statusGroup;
    QLabel*       m_diskInfoLabel;
    QLabel*       m_trackInfoLabel;
    QTextEdit*    m_hexDumpEdit;
    
    QGroupBox*    m_formatGroup;
    QCheckBox*    m_isoMFMCheck;
    QCheckBox*    m_isoFMCheck;
    QCheckBox*    m_amigaMFMCheck;
    QCheckBox*    m_eEmuCheck;
    QCheckBox*    m_aed6200pCheck;
    QCheckBox*    m_membrainCheck;
    QCheckBox*    m_appleIICheck;
    
    QGroupBox*    m_selectionGroup;
    QSpinBox*     m_trackSpinBox;
    QSpinBox*     m_sideSpinBox;
    
    QRadioButton* m_trackViewRadio;
    QRadioButton* m_diskViewRadio;
    
    int           m_currentTrack;
    int           m_currentSide;
    QString       m_currentDiskPath;
    
    std::vector<TrackInfo> m_trackData;
};

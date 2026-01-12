/**
 * @file rawformatdialog.h
 * @brief RAW File Format Configuration Dialog
 * 
 * Based on HxCFloppyEmulator's RAW format configuration.
 * Allows analysis and configuration of unknown disk formats.
 * 
 * Features:
 * - Track type selection (IBM FM, IBM MFM, Amiga MFM, etc.)
 * - Geometry configuration (tracks, sides, sectors)
 * - Timing parameters (bitrate, RPM, GAP lengths)
 * - Sector interleave and skew
 * - Predefined disk layouts
 * - Save/Load configurations
 * 
 * @date 2026-01-12
 */

#ifndef RAWFORMATDIALOG_H
#define RAWFORMATDIALOG_H

#include <QDialog>
#include <QSettings>

namespace Ui {
class RawFormatDialog;
}

class RawFormatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RawFormatDialog(QWidget *parent = nullptr);
    ~RawFormatDialog();
    
    /**
     * @brief RAW format configuration structure
     */
    struct RawConfig {
        // Track type
        QString trackType;      // "IBM FM", "IBM MFM", "Amiga MFM", "Apple II", "C64 GCR"
        
        // Geometry
        int tracks;             // Number of tracks (35-84)
        int sides;              // Number of sides (1-2)
        int sectorsPerTrack;    // Sectors per track
        int sectorSize;         // Sector size in bytes (128-8192)
        
        // Timing
        int bitrate;            // Bitrate in bps (125000-500000)
        int rpm;                // RPM (300, 360)
        
        // Sector layout
        int sectorIdStart;      // First sector ID (0 or 1)
        int interleave;         // Sector interleave (1-n)
        int skew;               // Head/track skew
        bool interSideNumbering; // Continue sector numbering across sides
        bool reverseSide;       // Reverse side 1
        bool sidesGrouped;      // Tracks of a side grouped in file
        
        // GAP configuration
        int gap3Length;         // GAP3 length
        int preGapLength;       // PRE-GAP length
        bool autoGap3;          // Auto-calculate GAP3
        
        // Calculated values (read-only display)
        int totalSectors;
        int totalSize;
        int formatValue;
        
        // Layout preset
        QString layoutPreset;
        bool sideBasedSectorNum; // Side-based sector numbering
    };
    
    RawConfig getConfig() const;
    void setConfig(const RawConfig& config);
    
    QString getSelectedFile() const { return m_selectedFile; }

signals:
    void configurationApplied(const RawConfig& config);
    void loadRawFile(const QString& path);
    void createEmptyFloppy(const RawConfig& config);

private slots:
    void onTrackTypeChanged(int index);
    void onGeometryChanged();
    void onLayoutPresetChanged(int index);
    void onAutoGap3Toggled(bool checked);
    
    void onSaveConfig();
    void onLoadConfig();
    void onLoadRawFile();
    void onCreateEmpty();
    
    void updateCalculatedValues();

private:
    Ui::RawFormatDialog *ui;
    QString m_selectedFile;
    
    void setupConnections();
    void setupTrackTypes();
    void setupLayoutPresets();
    void applyLayoutPreset(const QString& preset);
    int calculateTotalSectors() const;
    int calculateTotalSize() const;
    int calculateFormatValue() const;
};

#endif // RAWFORMATDIALOG_H

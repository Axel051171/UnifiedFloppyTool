/**
 * @file statustab.h
 * @brief Status Tab - Real-time Decode Progress Display
 * 
 * P1-2: Connected to DecodeJob signals for live updates
 * 
 * Features:
 * - Current track/side being processed
 * - Progress bars (track and total)
 * - Sector status grid (OK/BAD/MISSING)
 * - Hex dump of current sector
 * - Tool buttons: Label Editor, BAM/FAT, Bootblock, Protection
 * 
 * @date 2026-01-12
 */

#ifndef STATUSTAB_H
#define STATUSTAB_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include "decodejob.h"

namespace Ui {
class TabStatus;
}

/**
 * @brief Status display for decode operations
 */
class StatusTab : public QWidget
{
    Q_OBJECT

public:
    explicit StatusTab(QWidget *parent = nullptr);
    ~StatusTab();
    
    // ========================================================================
    // Manual Update Methods
    // ========================================================================
    
    void setTrackSide(int track, int side);
    void setProgress(int trackProgress, int totalProgress);
    void setSectorInfo(const QString &info);
    void setHexDump(const QByteArray &data, int offset = 0);
    void appendHexLine(int address, const QByteArray &bytes, const QString &ascii);
    void clear();
    
    // ========================================================================
    // DecodeJob Connection
    // ========================================================================
    
    void connectToDecodeJob(DecodeJob* job);
    void disconnectFromDecodeJob();

public slots:
    // DecodeJob Signals
    void onProgress(int percentage);
    void onStageChanged(const QString& stage);
    void onSectorUpdate(int track, int sector, const QString& status);
    void onImageInfo(const DecodeResult& info);
    void onDecodeFinished(const QString& message);
    void onDecodeError(const QString& error);

private slots:
    // Tool Button Handlers
    void onLabelEditorClicked();
    void onBAMViewerClicked();
    void onBootblockClicked();
    void onProtectionClicked();

signals:
    void decodeCompleted(bool success);
    void requestLabelEditor();
    void requestBAMViewer();
    void requestBootblock();
    void requestProtectionAnalysis();

private:
    Ui::TabStatus *ui;
    
    // Current state
    int m_currentTrack = 0;
    int m_currentSide = 0;
    int m_totalTracks = 0;
    int m_sectorsPerTrack = 0;
    
    // Current image info
    DecodeResult m_currentImage;
    bool m_hasImage = false;
    
    // Sector status tracking
    struct SectorStatus {
        int track;
        int sector;
        QString status;
    };
    QVector<SectorStatus> m_sectorHistory;
    QMap<QString, int> m_statusCounts;
    
    // Connected job
    DecodeJob* m_connectedJob = nullptr;
    
    // Helpers
    void setupConnections();
    QString formatHexDump(const QByteArray &data, int startAddress);
    void updateSectorGrid();
    void updateStatusCounts();
    void appendLog(const QString& message, const QString& level = "INFO");
    QString statusToIcon(const QString& status);
    
    // Enable/disable tool buttons based on loaded format
    void updateToolButtons();
};

#endif // STATUSTAB_H

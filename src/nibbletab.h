#pragma once
/**
 * @file nibbletab.h
 * @brief Nibble Tab - Low-Level Track Editor (GCR/MFM)
 * 
 * UI Dependencies:
 * - checkGCRMode → GCR-specific options enabled
 * - comboGCRType → C64/Apple/Victor GCR variants
 * - checkReadHalfTracks → spinHalfTrackOffset enabled
 * - checkVariableDensity → spinDensityZones enabled
 * - comboReadMode → timing/flux options
 * 
 * @date 2026-01-12
 */

#ifndef NIBBLETAB_H
#define NIBBLETAB_H

#include <QWidget>

class QPushButton;
class QSpinBox;
class QPlainTextEdit;

namespace Ui { class TabNibble; }

class NibbleTab : public QWidget
{
    Q_OBJECT

public:
    explicit NibbleTab(QWidget *parent = nullptr);
    ~NibbleTab();

public slots:
    void loadTrack(const QString& imagePath, int track, int head);

signals:
    void trackModified(int track, int head);
    void statusMessage(const QString& message);

private slots:
    // Track operations
    void onReadTrack();
    void onWriteTrack();
    void onAnalyzeGCR();
    void onDecodeGCR();
    void onDetectWeakBits();
    void onExportNIB();
    void onExportG64();
    void onTrackChanged(int track);
    void onHeadChanged(int head);
    
    // UI Dependencies
    void onGCRModeToggled(bool checked);
    void onGCRTypeChanged(int index);
    void onReadModeChanged(int index);
    void onReadHalfTracksToggled(bool checked);
    void onVariableDensityToggled(bool checked);
    void onPreserveTimingToggled(bool checked);
    void onAutoDetectDensityToggled(bool checked);

private:
    void createWidgets();
    void setupConnections();
    void setupDependencies();
    void updateDisplay();
    void displayHexDump(const QByteArray& data);
    void displayTimingHistogram(const QByteArray& data);
    void analyzeSync(const QByteArray& data);
    
    // Dependency updates
    void updateGCROptions(bool enabled);
    void updateHalfTrackOptions(bool enabled);
    void updateTimingOptions(bool enabled);
    void updateDensityOptions(bool enabled);
    void updateReadModeOptions(const QString& mode);
    
    Ui::TabNibble *ui;
    
    // Toolbar buttons (created programmatically)
    QPushButton* m_btnReadTrack;
    QPushButton* m_btnWriteTrack;
    QPushButton* m_btnAnalyzeGCR;
    QPushButton* m_btnDecodeGCR;
    QPushButton* m_btnDetectWeakBits;
    QPushButton* m_btnExportNIB;
    QPushButton* m_btnExportG64;
    
    // Controls (created programmatically)
    QSpinBox* m_spinTrack;
    QSpinBox* m_spinHead;
    QPlainTextEdit* m_textHexDump;
    QPlainTextEdit* m_textAnalysis;
    
    QString m_imagePath;
    int m_currentTrack;
    int m_currentHead;
    QByteArray m_trackData;
    bool m_modified;
};

#endif // NIBBLETAB_H

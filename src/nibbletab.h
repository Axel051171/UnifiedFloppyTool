/**
 * @file nibbletab.h
 * @brief Nibble Tab - Low-Level Track Editor (GCR/MFM)
 * 
 * P0-GUI-007 FIX: Full implementation
 */

#ifndef NIBBLETAB_H
#define NIBBLETAB_H

#include <QWidget>

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
    void onReadTrack();
    void onWriteTrack();
    void onAnalyzeGCR();
    void onDecodeGCR();
    void onDetectWeakBits();
    void onExportNIB();
    void onExportG64();
    void onTrackChanged(int track);
    void onHeadChanged(int head);

private:
    void setupConnections();
    void updateDisplay();
    void displayHexDump(const QByteArray& data);
    void displayTimingHistogram(const QByteArray& data);
    void analyzeSync(const QByteArray& data);
    
    Ui::TabNibble *ui;
    
    QString m_imagePath;
    int m_currentTrack;
    int m_currentHead;
    QByteArray m_trackData;
    bool m_modified;
};

#endif // NIBBLETAB_H

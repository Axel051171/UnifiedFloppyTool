/**
 * @file nibbletab.h
 * @brief Nibble Tab - Low-Level Track Editor (GCR/MFM)
 * 
 * P0-GUI-007 FIX: Full implementation with programmatic UI
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
    void createWidgets();
    void setupConnections();
    void updateDisplay();
    void displayHexDump(const QByteArray& data);
    void displayTimingHistogram(const QByteArray& data);
    void analyzeSync(const QByteArray& data);
    
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

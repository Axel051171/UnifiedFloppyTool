#ifndef DISKANALYZERWINDOW_H
#define DISKANALYZERWINDOW_H

#include <QDialog>

namespace Ui {
class DiskAnalyzerWindow;
}

class DiskAnalyzerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit DiskAnalyzerWindow(QWidget *parent = nullptr);
    ~DiskAnalyzerWindow();
    
    // Load image for analysis
    void loadImage(const QString &filename);

private slots:
    void onTrackChanged(int track);
    void onSideChanged(int side);
    void onViewModeChanged();
    void onExportClicked();
    void onEditToolsClicked();

private:
    Ui::DiskAnalyzerWindow *ui;
    
    void updateDiskView();
    void updateSectorInfo(int track, int side, int sector);
    void updateHexDump(const QByteArray &data);
    
    QString m_currentFile;
    int m_currentTrack;
    int m_currentSide;
};

#endif // DISKANALYZERWINDOW_H

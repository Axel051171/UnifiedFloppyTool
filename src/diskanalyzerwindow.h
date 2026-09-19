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

    /** MF-1273: der Bericht des Zentrums (`uft_disk2`) ueber das
     *  geoeffnete Abbild — was der Traeger traegt, gemessen. */
    QString traegerBericht(struct uft_disk *disk);

    /**
     * @brief Kodierung und Variante, wie das PLUGIN sie meldet (MF-662).
     *
     * Liefert "nicht ermittelt", wenn das Plugin schweigt — nie einen
     * Rateschluss aus Dateigröße oder Endung. Bis MF-662 stand hier
     * fest "ISO MFM", für jedes Format.
     */
    QString formatBeschreibung(const struct uft_disk *disk);

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

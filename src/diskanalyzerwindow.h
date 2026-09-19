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
     *  geoeffnete Abbild — was der Traeger traegt, gemessen.
     *
     *  Seit MF-1275 BEHAELT das Fenster das Modell (`m_traeger`), statt
     *  es am Ende wegzuwerfen: `traegerSichern()` schreibt genau dieses
     *  Modell als UFTD. Ein zweites Lesen der Diskette waere eine
     *  zweite MESSUNG, und zwei Messungen koennen auseinandergehen. */
    QString traegerBericht(struct uft_disk *disk);

    /**
     * MF-1275: das eingespeiste Modell als UFTD sichern — der erste
     * Weg, auf dem ein Abzug diesen Baum OHNE Verlust verlaesst.
     *
     * @param pfad    Zieldatei.
     * @param fehler  optional; bekommt bei false den Grund im Klartext.
     * @return false, wenn kein Abbild geladen ist oder das Schreiben
     *         scheitert — nie ein stilles Nichts.
     */
    bool traegerSichern(const QString &pfad, QString *fehler = nullptr);

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

    /** MF-1275: das Modell des zuletzt geladenen Abbilds. Gehoert diesem
     *  Fenster, wird beim naechsten Laden ersetzt und im Destruktor
     *  freigegeben. NULL heisst: nichts geladen. */
    struct uft_disk2 *m_traeger = nullptr;
};

#endif // DISKANALYZERWINDOW_H

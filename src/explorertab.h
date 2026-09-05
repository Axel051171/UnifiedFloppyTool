/**
 * @file explorertab.h
 * @brief Explorer Tab - Directory Browser for Disk Images
 * 
 * P0-GUI-004 FIX: Full implementation
 */

#ifndef EXPLORERTAB_H
#define EXPLORERTAB_H

#include <QWidget>
#include <QStringList>
#include <QMenu>

namespace Ui { class TabExplorer; }

struct FileEntry {
    QString name;
    qint64 size;
    QString type;
    bool isDir;
    QString attributes;
};

class ExplorerTab : public QWidget
{
    Q_OBJECT

public:
    explicit ExplorerTab(QWidget *parent = nullptr);
    ~ExplorerTab();

public slots:
    void loadImage(const QString& imagePath);

public:
    /**
     * Liest die genannten Dateien aus dem geladenen Abbild und schreibt sie
     * nach @p destDir. Gibt zurueck, wie viele WIRKLICH geschrieben wurden.
     *
     * MF-889: das ist die Arbeit, getrennt von ihrer Meldung. Bis dahin
     * standen beide in `onExtractSelected()`, und die Meldung zaehlte eine
     * Schleife hoch, die nichts tat. Wer die Zahl im Dialog aendern will,
     * muss jetzt hier etwas schreiben.
     *
     * @param fehler  optional; je nicht lesbarer Datei eine Zeile.
     */
    int extractFilesTo(const QStringList& fileNames,
                       const QString& destDir,
                       QStringList* fehler = nullptr);
    void clear();

signals:
    void fileSelected(const QString& filename);
    void statusMessage(const QString& message);

private slots:
    void onOpenImage();
    void onCloseImage();
    void onRefresh();
    void onNavigateUp();
    void onExtractSelected();
    void onExtractAll();
    void onBrowseExtractPath();
    void onItemDoubleClicked(int row, int column);
    void onSelectionChanged();
    /* P0-GUI-FIX: Neue Slots */
    void onBrowseImage();
    void onImportFiles();
    void onImportFolder();
    void onRename();
    void onDelete();
    void onNewFolder();
    void onNewDisk();
    void onValidate();
    /* Context Menu */
    void showContextMenu(const QPoint& pos);
    void onViewHex();
    void onViewText();
    void onViewProperties();
    void onCopyToClipboard();

private:
    void setupConnections();
    void setupContextMenu();
    void updateFileList();
    void populateFileTable(const QList<FileEntry>& entries);
    QString formatSize(qint64 size) const;
    QList<FileEntry> readDirectory(const QString& path);

    /** Rohbytes einer Datei aus dem Abbild. MF-889: einmal statt zweimal —
     *  derselbe Block stand wortgleich in `onViewHex()` und `onViewText()`. */
    bool readFileBytes(const QString& fileName, QByteArray* out,
                       QString* fehler = nullptr, QString* warnung = nullptr);

    /** Meldet das Ergebnis einer Extraktion — die Zahl kommt aus
     *  `extractFilesTo()`, nicht aus der Auswahl (MF-889). */
    void meldeExtraktion(int geschrieben, int angefordert,
                         const QString& ziel, const QStringList& fehler);

    /** MF-573: Schreib-Sicherheitstor vor jeder Aenderung am Abbild.
     *  false heisst abbrechen — der Benutzer wurde bereits informiert. */
    bool gateBeforeModify(const QString &what, QString *snapOut = nullptr);
    
    Ui::TabExplorer *ui;
    QMenu *m_contextMenu;
    
    QString m_imagePath;
    QString m_currentDir;
    QStringList m_dirHistory;
    bool m_imageLoaded;
};

#endif // EXPLORERTAB_H

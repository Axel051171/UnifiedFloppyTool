#ifndef STATUSTAB_H
#define STATUSTAB_H

#include <QWidget>

namespace Ui {
class TabStatus;
}

class StatusTab : public QWidget
{
    Q_OBJECT

public:
    explicit StatusTab(QWidget *parent = nullptr);
    ~StatusTab();
    
    // Update methods for real-time display
    void setTrackSide(int track, int side);
    void setProgress(int trackProgress, int totalProgress);
    void setSectorInfo(const QString &info);
    void setHexDump(const QByteArray &data, int offset = 0);
    void appendHexLine(int address, const QByteArray &bytes, const QString &ascii);
    void clear();

private:
    Ui::TabStatus *ui;
    
    QString formatHexDump(const QByteArray &data, int startAddress);
};

#endif // STATUSTAB_H

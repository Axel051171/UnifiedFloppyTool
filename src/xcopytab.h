/**
 * @file xcopytab.h
 * @brief XCopy Tab - Disk Copy Operations
 * 
 * P0-GUI-008 FIX: Full implementation
 */

#ifndef XCOPYTAB_H
#define XCOPYTAB_H

#include <QWidget>
#include <QThread>

namespace Ui { class TabXCopy; }

class CopyWorker;

class XCopyTab : public QWidget
{
    Q_OBJECT

public:
    explicit XCopyTab(QWidget *parent = nullptr);
    ~XCopyTab();

signals:
    void copyProgress(int track, int total);
    void copyComplete(bool success, const QString& message);
    void statusMessage(const QString& message);

private slots:
    void onBrowseSource();
    void onBrowseDest();
    void onStartCopy();
    void onStopCopy();
    void onCopyProgress(int track, int total);
    void onCopyFinished(bool success, const QString& message);

private:
    void setupConnections();
    void updateUIState(bool copying);
    bool validatePaths();
    void performCopy();
    
    Ui::TabXCopy *ui;
    
    bool m_copying;
    QThread *m_copyThread;
    CopyWorker *m_copyWorker;
};

#endif // XCOPYTAB_H

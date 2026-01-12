/**
 * @file xcopytab.h
 * @brief XCopy Tab - Disk Copy Operations
 * 
 * UI Dependencies:
 * - comboCopyMode → track/flux options enable/disable
 * - comboSourceType → source file/drive selection
 * - comboDestType → dest file/drive selection
 * - checkRetryErrors → spinMaxRetries
 * - checkVerify → spinVerifyRetries
 * - checkFillBad → spinFillByte
 * 
 * @date 2026-01-12
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
    // Browse buttons
    void onBrowseSource();
    void onBrowseDest();
    
    // Copy control
    void onStartCopy();
    void onStopCopy();
    void onCopyProgress(int track, int total);
    void onCopyFinished(bool success, const QString& message);
    
    // UI Dependencies
    void onCopyModeChanged(int index);
    void onSourceTypeChanged(int index);
    void onDestTypeChanged(int index);
    void onRetryErrorsToggled(bool checked);
    void onVerifyToggled(bool checked);
    void onFillBadToggled(bool checked);
    void onSidesChanged(int index);

private:
    void setupConnections();
    void setupDependencies();
    void updateUIState(bool copying);
    bool validatePaths();
    void performCopy();
    
    // Dependency updates
    void updateCopyModeOptions(const QString& mode);
    void updateSourceOptions(const QString& type);
    void updateDestOptions(const QString& type);
    void updateTrackRange();
    
    Ui::TabXCopy *ui;
    
    // Programmatic widgets (not in .ui file)
    class QProgressBar *m_progressBar;
    class QLabel *m_labelStatus;
    class QGroupBox *m_groupOptions;
    
    bool m_copying;
    QThread *m_copyThread;
    CopyWorker *m_copyWorker;
    
    void createExtraWidgets();
};

#endif // XCOPYTAB_H

#ifndef WORKFLOWTAB_H
#define WORKFLOWTAB_H

#include <QWidget>

class QThread;
class DecodeJob;

namespace Ui {
class TabWorkflow;
}

class WorkflowTab : public QWidget
{
    Q_OBJECT

public:
    explicit WorkflowTab(QWidget *parent = nullptr);
    ~WorkflowTab();

private slots:
    void onSourceFluxClicked();
    void onSourceUSBClicked();
    void onSourceFileClicked();
    void onDestFluxClicked();
    void onDestUSBClicked();
    void onDestFileClicked();
    void onStartAbortClicked();

private:
    void connectSignals();
    void updateSourceStatus();
    void updateDestinationStatus();

    Ui::TabWorkflow *ui;
    bool m_isRunning;
    QThread *m_workerThread;
    DecodeJob *m_decodeJob;
};

#endif // WORKFLOWTAB_H

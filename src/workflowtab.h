#pragma once
/**
 * @file workflowtab.h
 * @brief Workflow Tab - Source/Destination Selection with Combination Validation
 * 
 * Valid Combinations:
 * ┌────────────┬────────────┬─────────────────────────────────┬────────┐
 * │ SOURCE     │ DEST       │ Description                     │ Status │
 * ├────────────┼────────────┼─────────────────────────────────┼────────┤
 * │ Flux       │ File       │ Read hardware → Save file       │ ✓      │
 * │ Flux       │ Flux       │ Disk-to-Disk copy (2 drives)    │ ✓      │
 * │ USB        │ File       │ Read USB floppy → Save file     │ ✓      │
 * │ USB        │ USB        │ USB-to-USB copy (2 drives)      │ ⚠      │
 * │ File       │ Flux       │ Write file → Hardware           │ ✓      │
 * │ File       │ USB        │ Write file → USB floppy         │ ✓      │
 * │ File       │ File       │ Convert format                  │ ✓      │
 * │ Flux       │ USB        │ Mixed hardware (unusual)        │ ⚠      │
 * │ USB        │ Flux       │ Mixed hardware (unusual)        │ ⚠      │
 * └────────────┴────────────┴─────────────────────────────────┴────────┘
 * 
 * @date 2026-01-12
 */

#ifndef WORKFLOWTAB_H
#define WORKFLOWTAB_H

#include <QWidget>
#include <QButtonGroup>

namespace Ui {
class TabWorkflow;
}

class DecodeJob;
class QThread;

class WorkflowTab : public QWidget
{
    Q_OBJECT

public:
    explicit WorkflowTab(QWidget *parent = nullptr);
    ~WorkflowTab();
    
    enum Mode {
        Flux = 0,   // Flux Device (Greaseweazle, SCP, KryoFlux)
        USB  = 1,   // USB Floppy Drive
        File = 2    // Image File
    };
    
    enum OperationMode {
        OpRead = 0,    // Read from source
        OpWrite = 1,   // Write to destination
        OpVerify = 2,  // Verify disk
        OpConvert = 3  // Convert format
    };
    
    /**
     * @brief Combination validity result
     */
    struct CombinationInfo {
        bool isValid;
        bool needsWarning;
        QString description;
        QString warningMessage;
    };

signals:
    void operationStarted();
    void operationFinished(bool success);
    void progressChanged(int percentage);
    void hardwareModeChanged(bool sourceIsHardware, bool destIsHardware);

private slots:
    void onSourceModeChanged(int id);
    void onDestModeChanged(int id);
    void onSourceFileClicked();
    void onDestFileClicked();
    void onStartAbortClicked();
    void onHistogramClicked();
    void onPauseClicked();
    void onLogClicked();
    void onAnalyzeClicked();

private:
    Ui::TabWorkflow *ui;
    
    QButtonGroup* m_sourceGroup;
    QButtonGroup* m_destGroup;
    
    Mode m_sourceMode;
    Mode m_destMode;
    OperationMode m_operationMode;
    
    QString m_sourceFile;
    QString m_destFile;
    QString m_logBuffer;
    
    bool m_isRunning;
    bool m_isPaused;
    QThread* m_workerThread;
    DecodeJob* m_decodeJob;
    
    void setupButtonGroups();
    void connectSignals();
    void updateSourceStatus();
    void updateDestinationStatus();
    void resetUI();
    void updateOperationModeUI();
    
    // Combination validation
    CombinationInfo validateCombination() const;
    void updateCombinationUI();
    void updateDestinationOptions();
    QString getModeIcon(Mode mode) const;
    QString getModeString(Mode mode) const;
};

#endif // WORKFLOWTAB_H

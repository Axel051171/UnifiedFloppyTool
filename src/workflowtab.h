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
class FluxCaptureJob;
class FluxWriteJob;
class QThread;
/* MF-200 (P1.20): WorkflowTab now holds a non-owning GreaseweazleProviderV2
 * pointer (owned by HardwareTab) instead of a raw uft_gw_device_t* void*. */
namespace uft::hal { class GreaseweazleProviderV2; }

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

public slots:
    void onDeviceInfoChanged(const QString& deviceName, const QString& firmware);

    // MF-110 / MF-200 / MF-201 — MainWindow forwards HardwareTab's
    // non-owning GreaseweazleProviderV2* every time the connection state
    // flips. Pass nullptr when disconnected. Both FluxCaptureJob (P1.20)
    // and FluxWriteJob (P1.21) drive it via the V2 outcome surface; the
    // raw_handle()/gwDevice() escape hatch was removed in P1.22 (MF-202).
    void setHardwareDevice(::uft::hal::GreaseweazleProviderV2 *provider,
                           int cylinders, int sides);

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
    FluxCaptureJob* m_captureJob;
    FluxWriteJob* m_writeJob;

    // MF-110 / MF-200 — cached GreaseweazleProviderV2 from HardwareTab.
    // Non-owning: HardwareTab's m_gwProviderV2 owns the instance + handle.
    ::uft::hal::GreaseweazleProviderV2* m_gwProvider;
    int m_hwCylinders;
    int m_hwSides;
    
    // Device info from HardwareTab
    QString m_deviceName;
    QString m_deviceFirmware;
    
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

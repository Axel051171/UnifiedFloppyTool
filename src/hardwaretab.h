#pragma once
/**
 * @file hardwaretab.h
 * @brief Hardware Tab - Floppy Controller Management
 * 
 * Source/Destination Mode:
 * - Source: Flux devices only (Greaseweazle, SCP, KryoFlux)
 * - Destination: Flux devices + USB Floppy Drive
 * 
 * Activation:
 * - Buttons only active when Workflow Tab has Flux/USB selected
 * - Disabled when Workflow Tab has "Image File" selected
 */

#ifndef HARDWARETAB_H
#define HARDWARETAB_H

#include <QWidget>
#include <QTimer>
#include <QButtonGroup>

namespace Ui { class TabHardware; }

class HardwareTab : public QWidget
{
    Q_OBJECT

public:
    explicit HardwareTab(QWidget *parent = nullptr);
    ~HardwareTab();
    
    enum ControllerRole {
        RoleSource = 0,
        RoleDestination = 1
    };
    
    bool isConnected() const { return m_connected; }
    QString currentController() const { return m_controllerType; }
    ControllerRole currentRole() const { return m_controllerRole; }

signals:
    void connectionChanged(bool connected);
    void statusMessage(const QString& message);

public slots:
    /**
     * @brief Enable/disable based on Workflow Tab selection
     * @param sourceMode true if Workflow source is Flux/USB, false if File
     * @param destMode true if Workflow dest is Flux/USB, false if File
     */
    void setWorkflowModes(bool sourceIsHardware, bool destIsHardware);

private slots:
    // Connection
    void onRefreshPorts();
    void onConnect();
    void onDisconnect();
    void onControllerChanged(int index);
    
    // Role selection (Source/Destination)
    void onRoleChanged(int roleId);
    
    // Detection mode
    void onDetectionModeChanged();
    void onDetectDrive();
    
    // Motor control
    void onMotorOn();
    void onMotorOff();
    void onAutoSpinDownChanged(bool enabled);
    
    // Drive settings (Manual mode)
    void onDriveTypeChanged(int index);
    void onTracksChanged(int index);
    void onHeadsChanged(int index);
    void onDensityChanged(int index);
    void onRPMChanged(int index);
    
    // Advanced settings
    void onDoubleStepChanged(bool enabled);
    void onIgnoreIndexChanged(bool enabled);
    void onStepDelayChanged(int value);
    void onSettleTimeChanged(int value);
    
    // Tests
    void onSeekTest();
    void onReadTest();
    void onRPMTest();
    void onCalibrate();

private:
    void setupConnections();
    void setupButtonGroups();
    void detectSerialPorts();
    
    // Controller list management
    void populateControllerList();
    void updateControllerListForRole();
    
    // UI State Management
    void updateUIState();
    void setConnectionState(bool connected);
    void setDetectionMode(bool autoDetect);
    void updateDriveSettingsEnabled();
    void updateMotorControlsEnabled();
    void updateAdvancedEnabled();
    void updateTestButtonsEnabled();
    void updateRoleButtonsEnabled();
    
    // Status updates
    void updateStatus(const QString& status, bool isError = false);
    void clearDetectedInfo();
    void setDetectedInfo(const QString& model, const QString& firmware, 
                         const QString& rpm, const QString& index);
    
    // Auto-detection
    void autoDetectDrive();
    void applyDetectedSettings(const QString& driveType, int tracks, 
                               int heads, const QString& density, int rpm);
    
    Ui::TabHardware *ui;
    QButtonGroup *m_detectionModeGroup;
    QButtonGroup *m_roleGroup;
    
    // State
    bool m_connected;
    bool m_autoDetect;
    bool m_motorRunning;
    ControllerRole m_controllerRole;
    
    // Workflow state (from WorkflowTab)
    bool m_sourceIsHardware;
    bool m_destIsHardware;
    
    // Hardware info
    QString m_controllerType;
    QString m_portName;
    QString m_firmwareVersion;
    int m_hwModel;              // Hardware model (e.g., F1=1, F7=7)
    void *m_gwDevice;           // HAL device handle (uft_gw_device_t*)
    
    // Detected drive info
    QString m_detectedModel;
    int m_detectedTracks;
    int m_detectedHeads;
    QString m_detectedDensity;
    int m_detectedRPM;
    
    // Timers
    QTimer *m_motorTimer;
    QTimer *m_statusTimer;
};

#endif // HARDWARETAB_H

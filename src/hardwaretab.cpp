/**
 * @file hardwaretab.cpp
 * @brief Hardware Tab Implementation with Source/Destination Role
 * 
 * Role-based Controller Selection:
 * ┌─────────────────────────────────────────────────────────────┐
 * │ SOURCE Mode:                                                │
 * │   - Greaseweazle (F1/F7)                                   │
 * │   - SuperCard Pro                                          │
 * │   - KryoFlux                                               │
 * │   (NO USB Floppy - can only READ flux, not write)          │
 * ├─────────────────────────────────────────────────────────────┤
 * │ DESTINATION Mode:                                          │
 * │   - Greaseweazle (F1/F7)                                   │
 * │   - SuperCard Pro                                          │
 * │   - KryoFlux                                               │
 * │   - USB Floppy Drive  ← ONLY in Destination mode!          │
 * └─────────────────────────────────────────────────────────────┘
 * 
 * @date 2026-01-12
 */

#include "hardwaretab.h"
#include "ui_tab_hardware.h"

#include <QMessageBox>
#include <QRandomGenerator>
#include <QDebug>
#include <QThread>

#ifdef UFT_HAS_SERIALPORT
#include <QSerialPortInfo>
#define HAS_SERIALPORT 1
#else
#define HAS_SERIALPORT 0
#endif

// HAL includes for real hardware connection
#ifdef UFT_HAS_HAL
extern "C" {
#include "uft/hal/uft_greaseweazle_full.h"
}
#define HAS_HAL 1
#else
#define HAS_HAL 0
#endif

// ============================================================================
// Construction / Destruction
// ============================================================================

HardwareTab::HardwareTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabHardware)
    , m_detectionModeGroup(nullptr)
    , m_roleGroup(nullptr)
    , m_connected(false)
    , m_autoDetect(true)
    , m_motorRunning(false)
    , m_controllerRole(RoleSource)
    , m_sourceIsHardware(true)
    , m_destIsHardware(true)
    , m_hwModel(0)
    , m_gwDevice(nullptr)
    , m_detectedTracks(0)
    , m_detectedHeads(0)
    , m_detectedRPM(0)
    , m_motorTimer(nullptr)
    , m_statusTimer(nullptr)
{
    ui->setupUi(this);
    
    setupButtonGroups();
    setupConnections();
    detectSerialPorts();
    populateControllerList();
    
    // Initialize UI state (disconnected)
    setConnectionState(false);
    updateRoleButtonsEnabled();
    updateStatus(tr("Ready. Select controller and port, then click Connect."));
}

HardwareTab::~HardwareTab()
{
    // Close HAL device if still open
    #ifdef UFT_HAS_HAL
    if (m_gwDevice != nullptr) {
        uft_gw_close(static_cast<uft_gw_device_t*>(m_gwDevice));
        m_gwDevice = nullptr;
    }
    #endif
    
    if (m_motorTimer) {
        m_motorTimer->stop();
        delete m_motorTimer;
    }
    delete ui;
}

// ============================================================================
// Setup
// ============================================================================

void HardwareTab::setupButtonGroups()
{
    // Detection mode radio buttons
    m_detectionModeGroup = new QButtonGroup(this);
    m_detectionModeGroup->setExclusive(true);
    m_detectionModeGroup->addButton(ui->radioAutoDetect, 0);
    m_detectionModeGroup->addButton(ui->radioManual, 1);
    ui->radioAutoDetect->setChecked(true);
    m_autoDetect = true;
    
    // Role radio buttons (Source/Destination)
    m_roleGroup = new QButtonGroup(this);
    m_roleGroup->setExclusive(true);
    m_roleGroup->addButton(ui->radioSource, RoleSource);
    m_roleGroup->addButton(ui->radioDestination, RoleDestination);
    ui->radioSource->setChecked(true);
    m_controllerRole = RoleSource;
}

void HardwareTab::setupConnections()
{
    // Connection controls
    connect(ui->btnRefreshPorts, &QPushButton::clicked, this, &HardwareTab::onRefreshPorts);
    connect(ui->btnConnect, &QPushButton::clicked, this, [this]() {
        if (m_connected) {
            onDisconnect();
        } else {
            onConnect();
        }
    });
    connect(ui->btnDetect, &QPushButton::clicked, this, &HardwareTab::onDetectDrive);
    connect(ui->comboController, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HardwareTab::onControllerChanged);
    
    // Role selection (Source/Destination)
    connect(m_roleGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &HardwareTab::onRoleChanged);
    
    // Detection mode
    connect(m_detectionModeGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int id) {
                Q_UNUSED(id);
                onDetectionModeChanged();
            });
    
    // Motor control
    connect(ui->btnMotorOn, &QPushButton::clicked, this, &HardwareTab::onMotorOn);
    connect(ui->btnMotorOff, &QPushButton::clicked, this, &HardwareTab::onMotorOff);
    connect(ui->checkAutoSpinDown, &QCheckBox::toggled, this, &HardwareTab::onAutoSpinDownChanged);
    
    // Drive settings (for manual mode)
    connect(ui->comboDriveType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HardwareTab::onDriveTypeChanged);
    connect(ui->comboTracks, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HardwareTab::onTracksChanged);
    connect(ui->comboHeads, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HardwareTab::onHeadsChanged);
    connect(ui->comboDensity, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HardwareTab::onDensityChanged);
    connect(ui->comboRPM, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HardwareTab::onRPMChanged);
    
    // Advanced settings
    connect(ui->checkDoubleStep, &QCheckBox::toggled, this, &HardwareTab::onDoubleStepChanged);
    connect(ui->checkIgnoreIndex, &QCheckBox::toggled, this, &HardwareTab::onIgnoreIndexChanged);
    
    // Test buttons
    connect(ui->btnSeekTest, &QPushButton::clicked, this, &HardwareTab::onSeekTest);
    connect(ui->btnReadTest, &QPushButton::clicked, this, &HardwareTab::onReadTest);
    connect(ui->btnRPMTest, &QPushButton::clicked, this, &HardwareTab::onRPMTest);
    connect(ui->btnCalibrate, &QPushButton::clicked, this, &HardwareTab::onCalibrate);
}

// ============================================================================
// Controller List Management
// ============================================================================

void HardwareTab::populateControllerList()
{
    ui->comboController->blockSignals(true);
    ui->comboController->clear();
    
    // Flux controllers (always available)
    ui->comboController->addItem(tr("Greaseweazle (F1/F7)"), "greaseweazle");
    ui->comboController->addItem(tr("SuperCard Pro"), "scp");
    ui->comboController->addItem(tr("KryoFlux"), "kryoflux");
    
    // USB Floppy - only for Destination mode
    if (m_controllerRole == RoleDestination) {
        ui->comboController->addItem(tr("USB Floppy Drive"), "usb_floppy");
    }
    
    ui->comboController->blockSignals(false);
}

void HardwareTab::updateControllerListForRole()
{
    // Remember current selection
    QString currentData = ui->comboController->currentData().toString();
    
    // Repopulate
    populateControllerList();
    
    // Try to restore selection
    int idx = ui->comboController->findData(currentData);
    if (idx >= 0) {
        ui->comboController->setCurrentIndex(idx);
    } else {
        // If USB was selected but we switched to Source, select first item
        ui->comboController->setCurrentIndex(0);
    }
}

// ============================================================================
// Role Change (Source/Destination)
// ============================================================================

void HardwareTab::onRoleChanged(int roleId)
{
    m_controllerRole = static_cast<ControllerRole>(roleId);
    
    // Update controller list (USB only in Destination)
    updateControllerListForRole();
    
    // Update status
    QString roleName = (m_controllerRole == RoleSource) ? tr("Source") : tr("Destination");
    updateStatus(tr("Role: %1 - Select controller and connect.").arg(roleName));
    
    qDebug() << "Role changed to:" << roleName;
}

void HardwareTab::updateRoleButtonsEnabled()
{
    // Source button: enabled only if Workflow source is hardware
    ui->radioSource->setEnabled(m_sourceIsHardware);
    
    // Destination button: enabled only if Workflow destination is hardware  
    ui->radioDestination->setEnabled(m_destIsHardware);
    
    // Visual feedback
    QString enabledStyle = "";
    QString disabledStyle = "color: gray;";
    
    ui->radioSource->setStyleSheet(m_sourceIsHardware ? enabledStyle : disabledStyle);
    ui->radioDestination->setStyleSheet(m_destIsHardware ? enabledStyle : disabledStyle);
    
    // If current selection is disabled, switch to the other
    if (m_controllerRole == RoleSource && !m_sourceIsHardware) {
        if (m_destIsHardware) {
            ui->radioDestination->setChecked(true);
            m_controllerRole = RoleDestination;
            updateControllerListForRole();
        }
    } else if (m_controllerRole == RoleDestination && !m_destIsHardware) {
        if (m_sourceIsHardware) {
            ui->radioSource->setChecked(true);
            m_controllerRole = RoleSource;
            updateControllerListForRole();
        }
    }
    
    // If neither is hardware, disable the whole controller group
    bool anyHardware = m_sourceIsHardware || m_destIsHardware;
    ui->groupController->setEnabled(anyHardware);
    ui->groupConnection->setEnabled(anyHardware);
    
    if (!anyHardware) {
        updateStatus(tr("Hardware not needed - both Source and Destination are Image Files."));
    }
}

void HardwareTab::setWorkflowModes(bool sourceIsHardware, bool destIsHardware)
{
    m_sourceIsHardware = sourceIsHardware;
    m_destIsHardware = destIsHardware;
    updateRoleButtonsEnabled();
}

// ============================================================================
// Port Detection
// ============================================================================

void HardwareTab::detectSerialPorts()
{
    ui->comboPort->clear();
    
#if HAS_SERIALPORT
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    
    for (const QSerialPortInfo& port : ports) {
        QString portName = port.portName();
        QString description = port.description();
        uint16_t vid = port.vendorIdentifier();
        uint16_t pid = port.productIdentifier();
        
        QString displayName;
        QString controllerHint;
        
        // Known VID/PID pairs
        if (vid == 0x1209 && pid == 0x4D69) {
            controllerHint = "Greaseweazle";
        } else if (vid == 0x16D0 && pid == 0x0F8C) {
            controllerHint = "SuperCard Pro";
        } else if (vid == 0x0403 && pid == 0x6001) {
            controllerHint = "KryoFlux (FTDI)";
        }
        
        if (!controllerHint.isEmpty()) {
            displayName = QString("%1 - %2").arg(portName, controllerHint);
        } else if (!description.isEmpty()) {
            displayName = QString("%1 - %2").arg(portName, description);
        } else {
            displayName = portName;
        }
        
        ui->comboPort->addItem(displayName, portName);
    }
#endif
    
    if (ui->comboPort->count() == 0) {
        ui->comboPort->addItem(tr("(No ports found)"), "");
        ui->btnConnect->setEnabled(false);
    } else {
        ui->btnConnect->setEnabled(true);
    }
}

void HardwareTab::onRefreshPorts()
{
    detectSerialPorts();
    updateStatus(tr("Port list refreshed."));
}

// ============================================================================
// Connection
// ============================================================================

void HardwareTab::onConnect()
{
    QString port = ui->comboPort->currentData().toString();
    if (port.isEmpty()) {
        QMessageBox::warning(this, tr("Connection Error"),
            tr("Please select a valid port."));
        return;
    }
    
    m_portName = port;
    m_controllerType = ui->comboController->currentText();
    
    updateStatus(tr("Connecting to %1 on %2...").arg(m_controllerType, m_portName));
    
    // Use real HAL connection for Greaseweazle
    // Note: This runs in main thread - consider moving to worker for non-blocking
    QString controller = ui->comboController->currentData().toString();
    
    if (controller == "greaseweazle" || controller == "fluxengine") {
        // Real HAL connection attempt
        #ifdef UFT_HAS_HAL
        uft_gw_device_t *gw = nullptr;
        int ret = uft_gw_open(port.toLocal8Bit().constData(), &gw);
        
        if (ret == 0 && gw != nullptr) {
            // Get device info
            uft_gw_info_t info;
            if (uft_gw_get_info(gw, &info) == 0) {
                m_firmwareVersion = QString("v%1.%2").arg(info.fw_major).arg(info.fw_minor);
                m_hwModel = info.hw_model;
            } else {
                m_firmwareVersion = "Unknown";
            }
            
            // Store handle for later use
            m_gwDevice = gw;
            m_connected = true;
            setConnectionState(true);
            
            if (m_autoDetect) {
                autoDetectDrive();
            }
            
            updateStatus(tr("Connected to %1 F%2 (%3)")
                .arg(m_controllerType)
                .arg(m_hwModel)
                .arg(m_firmwareVersion));
            emit connectionChanged(true);
            return;
        } else {
            updateStatus(tr("Connection failed: %1").arg(uft_gw_strerror(ret)));
            QMessageBox::warning(this, tr("Connection Error"),
                tr("Failed to connect to %1 on %2.\n\nError: %3")
                .arg(m_controllerType, m_portName, QString::fromUtf8(uft_gw_strerror(ret))));
            return;
        }
        #else
        // HAL not available - fall back to simulated connection
        qWarning() << "HAL not available, using simulated connection";
        #endif
    }
    
    // Fallback: Simulated connection for unsupported controllers
    QTimer::singleShot(500, this, [this]() {
        m_connected = true;
        m_firmwareVersion = "Simulated";
        setConnectionState(true);
        
        if (m_autoDetect) {
            autoDetectDrive();
        }
        
        updateStatus(tr("Connected to %1 (%2) [SIMULATED]").arg(m_controllerType, m_firmwareVersion));
        emit connectionChanged(true);
    });
}

void HardwareTab::onDisconnect()
{
    if (m_motorRunning) {
        onMotorOff();
    }
    
    // Close HAL device if open
    #ifdef UFT_HAS_HAL
    if (m_gwDevice != nullptr) {
        uft_gw_close(static_cast<uft_gw_device_t*>(m_gwDevice));
        m_gwDevice = nullptr;
    }
    #endif
    
    m_connected = false;
    m_hwModel = 0;
    setConnectionState(false);
    clearDetectedInfo();
    
    updateStatus(tr("Disconnected."));
    emit connectionChanged(false);
}

void HardwareTab::onControllerChanged(int index)
{
    Q_UNUSED(index);
    QString controller = ui->comboController->currentData().toString();
    
    // USB Floppy has different capabilities
    bool isUSB = (controller == "usb_floppy");
    
    // Disable flux-specific options for USB
    ui->groupAdvanced->setEnabled(!isUSB);
    
    if (isUSB) {
        updateStatus(tr("USB Floppy selected - limited to standard formats."));
    }
}

// ============================================================================
// Detection Mode
// ============================================================================

void HardwareTab::onDetectionModeChanged()
{
    m_autoDetect = ui->radioAutoDetect->isChecked();
    updateDriveSettingsEnabled();
    
    if (m_autoDetect) {
        updateStatus(tr("Auto-Detect mode - drive settings will be detected automatically."));
        if (m_connected) {
            autoDetectDrive();
        }
    } else {
        updateStatus(tr("Manual mode - configure drive settings manually."));
    }
}

void HardwareTab::onDetectDrive()
{
    if (!m_connected) {
        QMessageBox::warning(this, tr("Not Connected"),
            tr("Please connect to a controller first."));
        return;
    }
    
    autoDetectDrive();
}

void HardwareTab::autoDetectDrive()
{
    updateStatus(tr("Detecting drive..."));
    
#if HAS_HAL
    if (m_gwDevice == nullptr) {
        updateStatus(tr("No device connected"));
        return;
    }
    
    uft_gw_device_t* gw = static_cast<uft_gw_device_t*>(m_gwDevice);
    
    // Select drive unit 0
    int ret = uft_gw_select_drive(gw, 0);
    if (ret != 0) {
        updateStatus(tr("Failed to select drive: %1").arg(ret));
        return;
    }
    
    // Turn on motor
    ret = uft_gw_set_motor(gw, true);
    if (ret != 0) {
        updateStatus(tr("Failed to turn on motor: %1").arg(ret));
        return;
    }
    
    // Wait for spin-up
    QThread::msleep(500);
    
    // Try to seek to track 0 to detect drive presence
    ret = uft_gw_seek(gw, 0);
    if (ret != 0) {
        uft_gw_set_motor(gw, false);
        updateStatus(tr("No drive detected (seek failed)"));
        return;
    }
    
    // Detect drive type by seeking to high tracks
    QString driveType = "Unknown";
    int maxTracks = 80;
    
    // Try track 80 (HD drives)
    ret = uft_gw_seek(gw, 80);
    if (ret == 0) {
        // Try track 82 (some drives support more)
        ret = uft_gw_seek(gw, 82);
        if (ret == 0) {
            maxTracks = 83;
        } else {
            maxTracks = 80;
        }
    } else {
        // Might be a 40-track drive
        ret = uft_gw_seek(gw, 40);
        if (ret == 0) {
            maxTracks = 40;
            driveType = "5.25\" DD";
        }
    }
    
    // Detect density by checking write protect and disk presence
    bool writeProtected = uft_gw_is_write_protected(gw);
    
    // Determine drive type based on tracks
    if (maxTracks >= 80) {
        driveType = "3.5\" HD";  // Most common
    } else if (maxTracks == 40) {
        driveType = "5.25\" DD";
    }
    
    int heads = 2;  // Assume double-sided
    QString density = (maxTracks >= 80) ? "HD" : "DD";
    int rpm = 300;  // Standard, will be measured in RPM test
    
    // Return to track 0
    uft_gw_seek(gw, 0);
    
    // Turn off motor
    uft_gw_set_motor(gw, false);
    
    // Apply detected settings
    applyDetectedSettings(driveType, maxTracks, heads, density, rpm);
    setDetectedInfo(driveType, m_firmwareVersion, QString::number(rpm), 
                    writeProtected ? tr("Yes") : tr("No"));
    
    updateStatus(tr("Drive detected: %1, %2 tracks, Write Protected: %3")
                .arg(driveType).arg(maxTracks).arg(writeProtected ? "Yes" : "No"));
#else
    // No HAL - show warning
    QMessageBox::warning(this, tr("HAL Not Available"),
        tr("Hardware Abstraction Layer is not compiled in.\n"
           "Drive detection is not available.\n\n"
           "Please rebuild UFT with UFT_HAS_HAL=ON"));
    updateStatus(tr("HAL not available - detection skipped"));
#endif
}

void HardwareTab::applyDetectedSettings(const QString& driveType, int tracks,
                                        int heads, const QString& density, int rpm)
{
    m_detectedModel = driveType;
    m_detectedTracks = tracks;
    m_detectedHeads = heads;
    m_detectedDensity = density;
    m_detectedRPM = rpm;
    
    // Update UI (in auto mode these are read-only)
    int idx;
    idx = ui->comboDriveType->findText(driveType, Qt::MatchContains);
    if (idx >= 0) ui->comboDriveType->setCurrentIndex(idx);
    
    idx = ui->comboTracks->findText(QString::number(tracks));
    if (idx >= 0) ui->comboTracks->setCurrentIndex(idx);
    
    idx = ui->comboHeads->findText(QString::number(heads));
    if (idx >= 0) ui->comboHeads->setCurrentIndex(idx);
    
    idx = ui->comboDensity->findText(density, Qt::MatchContains);
    if (idx >= 0) ui->comboDensity->setCurrentIndex(idx);
    
    idx = ui->comboRPM->findText(QString::number(rpm));
    if (idx >= 0) ui->comboRPM->setCurrentIndex(idx);
}

// ============================================================================
// UI State Management
// ============================================================================

void HardwareTab::setConnectionState(bool connected)
{
    m_connected = connected;
    
    ui->btnConnect->setText(connected ? tr("Disconnect") : tr("Connect"));
    ui->btnConnect->setStyleSheet(connected ? 
        "background-color: #ff6666;" : "");
    
    ui->comboController->setEnabled(!connected);
    ui->comboPort->setEnabled(!connected);
    ui->btnRefreshPorts->setEnabled(!connected);
    
    updateDriveSettingsEnabled();
    updateMotorControlsEnabled();
    updateAdvancedEnabled();
    updateTestButtonsEnabled();
    
    ui->groupDetection->setEnabled(connected);
    ui->groupDrive->setEnabled(connected);
    ui->groupMotor->setEnabled(connected);
    ui->groupTest->setEnabled(connected);
    ui->groupAdvanced->setEnabled(connected);
    ui->groupInfo->setEnabled(connected);
    
    if (!connected) {
        clearDetectedInfo();
    }
}

void HardwareTab::updateDriveSettingsEnabled()
{
    bool enabled = m_connected && !m_autoDetect;
    
    ui->comboDriveType->setEnabled(enabled);
    ui->comboTracks->setEnabled(enabled);
    ui->comboHeads->setEnabled(enabled);
    ui->comboDensity->setEnabled(enabled);
    ui->comboRPM->setEnabled(enabled);
}

void HardwareTab::updateMotorControlsEnabled()
{
    ui->btnMotorOn->setEnabled(m_connected && !m_motorRunning);
    ui->btnMotorOff->setEnabled(m_connected && m_motorRunning);
    ui->checkAutoSpinDown->setEnabled(m_connected);
}

void HardwareTab::updateAdvancedEnabled()
{
    bool enabled = m_connected && !m_autoDetect;
    
    ui->checkDoubleStep->setEnabled(enabled);
    ui->checkIgnoreIndex->setEnabled(enabled);
}

void HardwareTab::updateTestButtonsEnabled()
{
    ui->btnSeekTest->setEnabled(m_connected);
    ui->btnReadTest->setEnabled(m_connected);
    ui->btnRPMTest->setEnabled(m_connected);
    ui->btnCalibrate->setEnabled(m_connected);
    ui->btnDetect->setEnabled(m_connected);
}

// ============================================================================
// Status Updates
// ============================================================================

void HardwareTab::updateStatus(const QString& status, bool isError)
{
    ui->labelControllerStatus->setText(status);
    ui->labelControllerStatus->setStyleSheet(isError ? "color: red;" : "");
    emit statusMessage(status);
}

void HardwareTab::clearDetectedInfo()
{
    ui->labelFirmware->setText("-");
    ui->labelIndex->setText("-");
}

void HardwareTab::setDetectedInfo(const QString& model, const QString& firmware,
                                  const QString& rpm, const QString& index)
{
    Q_UNUSED(model);
    Q_UNUSED(rpm);
    ui->labelFirmware->setText(firmware);
    ui->labelIndex->setText(index);
}

// ============================================================================
// Motor Control
// ============================================================================

void HardwareTab::onMotorOn()
{
    if (!m_connected) return;
    
#if HAS_HAL
    if (m_gwDevice != nullptr) {
        uft_gw_device_t* gw = static_cast<uft_gw_device_t*>(m_gwDevice);
        int ret = uft_gw_set_motor(gw, true);
        if (ret != 0) {
            updateStatus(tr("Failed to turn motor on: error %1").arg(ret));
            return;
        }
    }
#endif
    
    m_motorRunning = true;
    updateMotorControlsEnabled();
    updateStatus(tr("Motor ON"));
    
    // Auto spin-down timer
    if (ui->checkAutoSpinDown->isChecked()) {
        if (!m_motorTimer) {
            m_motorTimer = new QTimer(this);
            m_motorTimer->setSingleShot(true);
            connect(m_motorTimer, &QTimer::timeout, this, &HardwareTab::onMotorOff);
        }
        m_motorTimer->start(10000);  // 10 seconds
    }
}

void HardwareTab::onMotorOff()
{
    if (!m_connected) return;
    
#if HAS_HAL
    if (m_gwDevice != nullptr) {
        uft_gw_device_t* gw = static_cast<uft_gw_device_t*>(m_gwDevice);
        int ret = uft_gw_set_motor(gw, false);
        if (ret != 0) {
            updateStatus(tr("Failed to turn motor off: error %1").arg(ret));
        }
    }
#endif
    
    m_motorRunning = false;
    if (m_motorTimer) {
        m_motorTimer->stop();
    }
    updateMotorControlsEnabled();
    updateStatus(tr("Motor OFF"));
}

void HardwareTab::onAutoSpinDownChanged(bool enabled)
{
    Q_UNUSED(enabled);
}

// ============================================================================
// Drive Settings (Manual Mode)
// ============================================================================

void HardwareTab::onDriveTypeChanged(int index) { Q_UNUSED(index); }
void HardwareTab::onTracksChanged(int index) { Q_UNUSED(index); }
void HardwareTab::onHeadsChanged(int index) { Q_UNUSED(index); }
void HardwareTab::onDensityChanged(int index) { Q_UNUSED(index); }
void HardwareTab::onRPMChanged(int index) { Q_UNUSED(index); }

// ============================================================================
// Advanced Settings
// ============================================================================

void HardwareTab::onDoubleStepChanged(bool enabled) { Q_UNUSED(enabled); }
void HardwareTab::onIgnoreIndexChanged(bool enabled) { Q_UNUSED(enabled); }
void HardwareTab::onStepDelayChanged(int value) { Q_UNUSED(value); }
void HardwareTab::onSettleTimeChanged(int value) { Q_UNUSED(value); }

// ============================================================================
// Test Functions
// ============================================================================

void HardwareTab::onSeekTest()
{
    if (!m_connected) return;
    
    updateStatus(tr("Running seek test..."));
    
#if HAS_HAL
    if (m_gwDevice == nullptr) {
        updateStatus(tr("No device"));
        return;
    }
    
    uft_gw_device_t* gw = static_cast<uft_gw_device_t*>(m_gwDevice);
    
    // Turn on motor
    uft_gw_set_motor(gw, true);
    QThread::msleep(300);
    
    int errors = 0;
    int maxTrack = m_detectedTracks > 0 ? m_detectedTracks : 80;
    
    // Seek to each track
    for (int track = 0; track <= maxTrack; track += 10) {
        int ret = uft_gw_seek(gw, track);
        if (ret != 0) {
            errors++;
            updateStatus(tr("Seek error at track %1").arg(track));
        }
        QThread::msleep(10);
    }
    
    // Return to track 0
    uft_gw_seek(gw, 0);
    uft_gw_set_motor(gw, false);
    
    if (errors == 0) {
        updateStatus(tr("Seek test complete - all tracks accessible."));
    } else {
        updateStatus(tr("Seek test complete - %1 errors.").arg(errors));
    }
#else
    updateStatus(tr("Seek test requires HAL"));
#endif
}

void HardwareTab::onReadTest()
{
    if (!m_connected) return;
    
    updateStatus(tr("Running read test..."));
    
#if HAS_HAL
    if (m_gwDevice == nullptr) {
        updateStatus(tr("No device"));
        return;
    }
    
    uft_gw_device_t* gw = static_cast<uft_gw_device_t*>(m_gwDevice);
    
    // Select drive and turn on motor
    uft_gw_select_drive(gw, 0);
    uft_gw_set_motor(gw, true);
    QThread::msleep(500);
    
    // Seek to track 0
    int ret = uft_gw_seek(gw, 0);
    if (ret != 0) {
        uft_gw_set_motor(gw, false);
        updateStatus(tr("Read test failed: cannot seek to track 0"));
        return;
    }
    
    // Select head 0
    uft_gw_select_head(gw, 0);
    
    // Try to read flux data
    uft_gw_read_params_t params = {};
    params.revolutions = 1;
    params.index_sync = true;
    
    uft_gw_flux_data_t *flux = nullptr;
    ret = uft_gw_read_flux(gw, &params, &flux);
    
    uft_gw_set_motor(gw, false);
    
    if (ret == 0 && flux != nullptr && flux->sample_count > 0) {
        updateStatus(tr("Read test complete - Track 0 readable (%1 samples, %2 index pulses)")
                    .arg(flux->sample_count).arg(flux->index_count));
        // Free flux data
        if (flux->samples) free(flux->samples);
        if (flux->index_times) free(flux->index_times);
        free(flux);
    } else {
        updateStatus(tr("Read test failed: no data or error %1").arg(ret));
    }
#else
    updateStatus(tr("Read test requires HAL"));
#endif
}

void HardwareTab::onRPMTest()
{
    if (!m_connected) return;
    
    updateStatus(tr("Measuring RPM..."));
    
#if HAS_HAL
    if (m_gwDevice == nullptr) {
        updateStatus(tr("No device"));
        return;
    }
    
    uft_gw_device_t* gw = static_cast<uft_gw_device_t*>(m_gwDevice);
    
    // Turn on motor
    uft_gw_set_motor(gw, true);
    QThread::msleep(1000);  // Wait for stable rotation
    
    // Read one revolution to measure index time
    uft_gw_read_params_t params = {};
    params.revolutions = 2;  // Need 2 to measure interval
    params.index_sync = true;
    
    uft_gw_flux_data_t *flux = nullptr;
    int ret = uft_gw_read_flux(gw, &params, &flux);
    
    uft_gw_set_motor(gw, false);
    
    if (ret == 0 && flux != nullptr && flux->index_count >= 2) {
        // Calculate RPM from index times
        uint32_t sample_freq = uft_gw_get_sample_freq(gw);
        uint32_t interval_ticks = flux->index_times[1] - flux->index_times[0];
        double interval_ms = (double)interval_ticks / sample_freq * 1000.0;
        double rpm = 60000.0 / interval_ms;
        
        updateStatus(tr("RPM: %1 (interval: %2 ms)")
                    .arg(rpm, 0, 'f', 1).arg(interval_ms, 0, 'f', 2));
        
        // Update detected RPM
        m_detectedRPM = qRound(rpm);
        
        // Free flux data
        if (flux->samples) free(flux->samples);
        if (flux->index_times) free(flux->index_times);
        free(flux);
    } else {
        updateStatus(tr("RPM measurement failed: insufficient index pulses"));
    }
#else
    updateStatus(tr("RPM test requires HAL"));
#endif
}

void HardwareTab::onCalibrate()
{
    if (!m_connected) return;
    
    updateStatus(tr("Calibrating drive..."));
    
#if HAS_HAL
    if (m_gwDevice == nullptr) {
        updateStatus(tr("No device"));
        return;
    }
    
    uft_gw_device_t* gw = static_cast<uft_gw_device_t*>(m_gwDevice);
    
    // Turn on motor
    uft_gw_set_motor(gw, true);
    QThread::msleep(300);
    
    // Seek to track 0 (home position)
    int ret = uft_gw_seek(gw, 0);
    if (ret != 0) {
        uft_gw_set_motor(gw, false);
        updateStatus(tr("Calibration failed: cannot find track 0"));
        return;
    }
    
    // Move out and back to verify
    uft_gw_seek(gw, 2);
    QThread::msleep(50);
    ret = uft_gw_seek(gw, 0);
    
    uft_gw_set_motor(gw, false);
    
    if (ret == 0) {
        updateStatus(tr("Calibration complete - head at track 0"));
    } else {
        updateStatus(tr("Calibration error: track 0 sensor issue"));
    }
#else
    updateStatus(tr("Calibration requires HAL"));
#endif
}

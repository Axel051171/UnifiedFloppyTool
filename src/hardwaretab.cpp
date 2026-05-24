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

/* MF-157 (P1.4): the V2 provider must be COMPLETE in this TU because:
 *   - HardwareTab's destructor (defined here) must instantiate
 *     unique_ptr<GreaseweazleProviderV2>::~unique_ptr,
 *   - onConnect() constructs an instance,
 *   - the codegen-emitted wire_hardware_tab() function (which lives in
 *     generated/tab_hardware_wiring.gen.cpp) calls do_*() through the
 *     wire_action template. */
/* MF-205 (P1.23): the ProviderV2Variant member holds a unique_ptr to
 * any of the 9 V2 providers — the variant's destructor (run from
 * ~HardwareTab in this TU) needs every alternative's complete type. */
#include "hardware_providers/greaseweazle_provider_v2.h"
#include "hardware_providers/scp_provider_v2.h"
#include "hardware_providers/kryoflux_provider_v2.h"
#include "hardware_providers/fluxengine_provider_v2.h"
#include "hardware_providers/fc5025_provider_v2.h"
#include "hardware_providers/xum1541_provider_v2.h"
#include "hardware_providers/applesauce_provider_v2.h"
#include "hardware_providers/adfcopy_provider_v2.h"
#include "hardware_providers/usbfloppy_provider_v2.h"

/* MF-213 (audit ARCH-7-C): ADF-Copy / Applesauce disambiguation probe. */
#include "hardware_providers/teensy_probe.h"

/* MF-250 (v4.1.5-hardening): QSerialPort-backed Applesauce production
 * transport. Guarded via UFT_HAS_QSERIALPORT (defined by CMake when
 * Qt6::SerialPort is found) so a build without it still compiles. */
#ifdef UFT_HAS_QSERIALPORT
#  include "hardware_providers/qserial_applesauce_transport.h"
#  include "hardware_providers/applesauce_serial_runners.h"
#  include "hardware_providers/qserial_adfcopy_transport.h"
#  include "hardware_providers/adfcopy_serial_runners.h"
#endif

/* MF-210: hardwaretab.cpp now uses the capability concepts directly
 * (`if constexpr (::uft::hal::ControlsMotor<P>)` etc.) in the
 * std::visit-based provider helpers. */
#include "uft/hal/concepts.h"

/* MF-212 (audit ARCH-7-B): the SuperCard Pro USB VID/PID lives in
 * exactly one place — uft_scp_direct.h — and the GUI port-hint below
 * references those macros instead of re-typing the literals. */
#include "uft/hal/uft_scp_direct.h"

#include <type_traits>
#include <variant>

#include <QMessageBox>
#include <QRandomGenerator>
#include <QDebug>
#include <QLoggingCategory>
#include <QThread>

// Serial-port detection fires every few seconds; each call used to emit
// one qDebug line per port (~32 on a typical Linux host), flooding the
// system journal. Route those traces through a category that is off by
// default. Users who want to debug hardware detection can re-enable it
// with e.g.:
//   QT_LOGGING_RULES="uft.hw.serial.debug=true" UnifiedFloppyTool
// See issue #17.
Q_LOGGING_CATEGORY(lcHwSerial, "uft.hw.serial", QtWarningMsg)
#include <QFileInfo>
#include <QStandardItemModel>

#ifdef UFT_HAS_SERIALPORT
#include <QSerialPortInfo>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

/* MF-171 (P1.18): the V1 era pulled `uft/hal/uft_greaseweazle_full.h`
 * here so HardwareTab could call `uft_gw_open/close/get_info/...`
 * directly. After P1.18, every uft_gw_* call lives inside
 * GreaseweazleProviderV2 (which the V2 header already includes
 * transitively). The HardwareTab TU is now completely free of
 * direct C-API references — the V2 provider is its only gateway. */

/* MF-169 (P1.17): the V1 hardware-provider hierarchy
 * (`HardwareManager`, `HardwareProvider`, the 11 V1 provider classes,
 * and `unified_hal_bridge`) was deleted in this commit. The single
 * remaining hardware path is via the V2 mixin-composed providers
 * (`*_provider_v2.{h,cpp}`). For Greaseweazle that path is wired
 * end-to-end through `m_gwProviderV2` (MF-157 / P1.4); for the eight
 * other controllers, the V2 wrappers exist but HardwareTab does not
 * yet route to them — that is P1.18. Until then, selecting a
 * non-Greaseweazle controller and pressing Connect surfaces a clear
 * "no V2 routing wired" message rather than silently no-op'ing or
 * mis-dispatching. */

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
    , m_detectedTracks(0)
    , m_detectedHeads(0)
    , m_detectedRPM(0)
    , m_motorTimer(nullptr)
    , m_statusTimer(nullptr)
    , m_portRefreshTimer(nullptr)
{
    ui->setupUi(this);

    /* MF-169 (P1.17): the V1 `HardwareManager` was deleted with the rest
     * of the V1 hierarchy. The worker-thread routing that MF-147 added
     * lived inside HardwareManager and will be reintroduced in P1.18
     * around the V2 providers (currently only Greaseweazle's V2 is wired
     * via `m_gwProviderV2`; the V2 providers themselves are plain C++
     * classes — moving them onto a worker QThread is a P1.18 follow-up). */

    setupButtonGroups();
    setupConnections();
    detectSerialPorts();
    populateControllerList();
    
    // Initialize UI state (disconnected)
    setConnectionState(false);
    updateRoleButtonsEnabled();
    updateStatus(tr("Ready. Select controller and port, then click Connect."));
    
    // Auto-refresh ports every 2 seconds when not connected
    m_portRefreshTimer = new QTimer(this);
    connect(m_portRefreshTimer, &QTimer::timeout, this, &HardwareTab::autoRefreshPorts);
    m_portRefreshTimer->start(2000);
}

HardwareTab::~HardwareTab()
{
    /* MF-171 (P1.18): m_gwProviderV2's destructor handles uft_gw_close()
     * via its own RAII lifecycle. HardwareTab no longer manages the
     * C-handle directly. */


    if (m_motorTimer) {
        m_motorTimer->stop();
        delete m_motorTimer;
    }
    if (m_portRefreshTimer) {
        m_portRefreshTimer->stop();
        delete m_portRefreshTimer;
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
    /* MF-157 (P1.4) + MF-171 (P1.18): btnDetect is wired through the
     * codegen (forms/tab_hardware.actions.yaml +
     * generated/tab_hardware_wiring.gen.cpp). The V1 `onDetectDrive()`
     * slot that lived here was removed in P1.18 — it touched uft_gw_*
     * directly and was unwired since P1.4. */
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
    
    /* MF-157 (P1.4): btnMotorOn / btnMotorOff are now codegen-wired
     * (ControlsMotor capability). Their V1 slots stay as legacy reachable
     * methods until P1.18. */
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
    
    /* MF-157 (P1.4): btnSeekTest, btnReadTest, btnRPMTest, btnCalibrate
     * are now codegen-wired (SeeksHead / ReadsRawFlux / MeasuresRPM /
     * Recalibrates capabilities). V1 slots stay until P1.18. */

    /* MF-169 (P1.17): the "Unified Capture" button + its `onUnifiedCapture`
     * slot were the V1-era entry point through `uft_hal_bridge` and the V1
     * `HardwareManager`. Both are deleted. The codegen-wired btnReadTest
     * + btnSeekTest + btnCalibrate + btnRPMTest now serve the same role
     * via the type-driven path. */

    /* MF-157 (P1.4): initial codegen wiring pass.
     *
     * No V2 provider is bound yet at construction time. The codegen-emitted
     * `wire_hardware_tab(this)` will run Phase 1 (disconnect — currently no
     * connections to remove) and Phase 2 (disable each capability button)
     * because `currentProviderV2()` returns nullptr. The disable IS the
     * "no controller selected yet" UI affordance. After onConnect() succeeds
     * for Greaseweazle, rewireV2() runs again and Phase 3 takes over. */
    rewireV2();
}

void HardwareTab::rewireV2()
{
    /* The codegen function is the single chokepoint for V2 wiring.
     * Calling it is idempotent: Phase 1 strips any prior wiring before
     * Phase 2/3 makes a fresh decision based on currentProviderV2(). */
    ::uft::gui::generated::wire_hardware_tab(this);
}

const ProviderV2Variant &HardwareTab::currentProviderV2() const noexcept
{
    /* MF-205 (P1.23): returns the ProviderV2Variant by const reference —
     * whichever of the 9 V2 providers is connected, or std::monostate
     * when disconnected. The codegen-emitted wire_hardware_tab() does a
     * std::visit over it; inside the visit lambda the held pointer is a
     * concrete provider type, so wire_action<cap::X>'s capability gating
     * stays structural per concrete type. */
    return m_providerV2;
}

/* ════════════════════════════════════════════════════════════════════════
 *  MF-210 — capability-aware provider helpers
 *
 *  The connected provider is one of 9 unrelated types held in
 *  `m_providerV2` (a std::variant). These three helpers wrap the
 *  `std::visit` + `if constexpr (::uft::hal::Capability<P>)` boilerplate
 *  so the connect / disconnect / detect paths are capability-driven for
 *  ALL providers, not hardcoded to the Greaseweazle alternative (the
 *  bug class the P1.23 routing left behind).
 * ════════════════════════════════════════════════════════════════════════ */

bool HardwareTab::providerControlsMotor() const
{
    return std::visit([](const auto &held) -> bool {
        using HeldT = std::decay_t<decltype(held)>;
        if constexpr (std::is_same_v<HeldT, std::monostate>) {
            return false;
        } else {
            using ProviderT = typename HeldT::element_type;
            return ::uft::hal::ControlsMotor<ProviderT>;
        }
    }, m_providerV2);
}

void HardwareTab::issueMotorOffIfRunning()
{
    if (!m_motorRunning) {
        return;
    }
    /* Issue set_motor(false) to whichever connected provider actually has
     * ControlsMotor. The outcome is intentionally discarded — this only
     * runs on teardown. MUST be called before the variant is cleared to
     * std::monostate, otherwise the command is lost (the P1-2 bug). */
    std::visit([](auto &held) {
        using HeldT = std::decay_t<decltype(held)>;
        if constexpr (!std::is_same_v<HeldT, std::monostate>) {
            using ProviderT = typename HeldT::element_type;
            if constexpr (::uft::hal::ControlsMotor<ProviderT>) {
                if (held) {
                    (void)held->set_motor(false);
                }
            }
        }
    }, m_providerV2);
    m_motorRunning = false;
}

void HardwareTab::runDetectProbe()
{
    if (!m_connected) {
        return;
    }
    /* detect_drive() on whichever provider is connected (all 9 satisfy
     * cap::DetectsDrive), dispatched through the same handler set the
     * codegen-wired btnDetect uses. */
    std::visit([this](auto &held) {
        using HeldT = std::decay_t<decltype(held)>;
        if constexpr (!std::is_same_v<HeldT, std::monostate>) {
            using ProviderT = typename HeldT::element_type;
            if constexpr (::uft::hal::DetectsDrive<ProviderT>) {
                if (!held) {
                    return;
                }
                auto outcome = held->detect_drive();
                std::visit(::uft::hal::overloaded{
                    [this](const ::uft::hal::DriveDetected &v)            { onDetectOutcome(v); },
                    [this](const ::uft::hal::DriveAbsent &v)              { onDetectOutcome(v); },
                    [this](const ::uft::hal::CapabilityRequiresPolicy &v) { showPolicyRequired(v); },
                    [this](const ::uft::hal::HardwareDisconnected &v)     { showHardwareDisconnected(v); },
                    [this](const ::uft::hal::ProviderError &e)            { showProviderError(e); },
                }, outcome);
            }
        }
    }, m_providerV2);
}

/* MF-202 (P1.22): HardwareTab::gwDevice() removed. The legacy `void*`
 * C-handle escape hatch had a single purpose — feeding the raw
 * uft_gw_device_t* to FluxCaptureJob / FluxWriteJob. Both were migrated
 * to the V2 outcome surface (P1.20 / P1.21) and now take a non-owning
 * GreaseweazleProviderV2* via currentProviderV2(). GreaseweazleProviderV2
 * ::raw_handle() is removed in the same commit. */

// ============================================================================
// Controller List Management
// ============================================================================

void HardwareTab::populateControllerList()
{
    ui->comboController->blockSignals(true);
    ui->comboController->clear();
    
    // === Flux Controllers (Universal) ===
    ui->comboController->addItem(tr("── Flux Controllers ──"), "separator_flux");
    ui->comboController->addItem(tr("Greaseweazle (F1/F7)"), "greaseweazle");
    ui->comboController->addItem(tr("SuperCard Pro"), "scp");
    ui->comboController->addItem(tr("KryoFlux"), "kryoflux");
    ui->comboController->addItem(tr("FluxEngine"), "fluxengine");
    ui->comboController->addItem(tr("ADF-Copy (Amiga)"), "adfcopy");
    /* MF-144 / HW-B: expose the two providers that already had real
     * I/O implementations (1311 LOC Applesauce, 1005 LOC FC5025) but
     * weren't selectable in the UI. Both route through HardwareManager
     * dispatch (text-match on display name) — verified MF-143. */
    ui->comboController->addItem(tr("Applesauce"), "applesauce");
    ui->comboController->addItem(tr("FC5025 (5.25\" read-only)"), "fc5025");

    // === Commodore Controllers (IEC/IEEE-488) ===
    ui->comboController->addItem(tr("── Commodore USB ──"), "separator_cbm_usb");
    /* MF-180 (P1.19 follow-up): ZoomFloppy and XUM1541 are one hardware
     * family — ZoomFloppy runs xum1541 firmware and speaks the identical
     * OpenCBM protocol. There is exactly one V2 provider, XUM1541ProviderV2;
     * the former separate "zoomfloppy" controller-key had no provider of
     * its own. Merged into a single entry, analogous to the single
     * "Greaseweazle (F1/F7)" entry that covers multiple GW models. */
    ui->comboController->addItem(tr("XUM1541 / XUM1541-II / ZoomFloppy"), "xum1541");

    /* MF-170 (P1.19): legacy parallel-port X1541 family (XA/XAP/XM/XE/
     * X1541) removed from the controller combo. The five entries had
     * NO matching HardwareProvider class — they were structurally
     * phantom-features that surfaced a "Backend not yet wired"
     * messagebox on Connect. Parallel-port adapters are unreachable
     * on modern desktops anyway; the USB-based XUM1541 / ZoomFloppy
     * above is the only supported Commodore bridge today, and its V2
     * provider (P1.12) passes the 65-section conformance harness. */

    // USB Floppy - only for Destination mode
    if (m_controllerRole == RoleDestination) {
        ui->comboController->addItem(tr("── Standard USB ──"), "separator_usb");
        ui->comboController->addItem(tr("USB Floppy Drive"), "usb_floppy");
    }
    
    // Disable separator items
    for (int i = 0; i < ui->comboController->count(); i++) {
        QString data = ui->comboController->itemData(i).toString();
        if (data.startsWith("separator_")) {
            // Make separator items non-selectable
            QStandardItemModel *model = qobject_cast<QStandardItemModel*>(ui->comboController->model());
            if (model) {
                QStandardItem *item = model->item(i);
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            }
        }
    }
    
    ui->comboController->blockSignals(false);
    
    // Select first valid controller (skip separator)
    if (ui->comboController->count() > 1) {
        ui->comboController->setCurrentIndex(1);
    }
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
    
#ifdef UFT_HAS_SERIALPORT
    qCDebug(lcHwSerial) << "Using QSerialPortInfo for port detection";
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    qCDebug(lcHwSerial) << "Found" << ports.size() << "serial ports";

    for (const QSerialPortInfo& port : ports) {
        QString portName = port.portName();
        QString description = port.description();
        uint16_t vid = port.vendorIdentifier();
        uint16_t pid = port.productIdentifier();

        qCDebug(lcHwSerial) << "  Port:" << portName << "VID:" << Qt::hex << vid << "PID:" << pid << "Desc:" << description;
        
        QString displayName;
        QString controllerHint;
        
        // Known VID/PID pairs
        if (vid == 0x1209 && pid == 0x4D69) {
            controllerHint = "Greaseweazle";
        } else if (vid == UFT_SCP_USB_VID && pid == UFT_SCP_USB_PID) {
            /* ARCH-7-B (MF-212): VID/PID single-sourced from
             * uft_scp_direct.h. Verified 0x16D0:0x0F8C from the real
             * device descriptor — the old header value 0x16C0:0x0753
             * was the wrong one of the two contradicting sites. */
            controllerHint = "SuperCard Pro";
        } else if (vid == 0x0403 && pid == 0x6001) {
            controllerHint = "KryoFlux (FTDI)";
        } else if (vid == 0x16D0 &&
                   (pid == 0x04B2 || pid == 0x0504 || pid == 0x0503)) {
            /* ARCH-7 (audit/ARCH-7_VID_PID.md sub-finding A): PIDs per
             * the authoritative in-repo table in
             * src/hardware_providers/xum1541_usb.h — ZoomFloppy 0x04B2,
             * XUM1541 0x0504, DIY XUM1541 0x0503. All run xum1541
             * firmware and speak the identical OpenCBM protocol, so a
             * single hint matches the single combo entry. The previous
             * code matched only 0x0504 and mislabelled it "ZoomFloppy". */
            controllerHint = "XUM1541 / ZoomFloppy";
        } else if (vid == 0x16C0 && pid == 0x0483) {
            /* ARCH-7-C (MF-213): 0x16C0:0x0483 is the stock, unmodified
             * PJRC Teensy USB-Serial identity — claimed by BOTH ADF-Copy
             * and Applesauce. Verified by device readout: both ship the
             * *stock* Teensy string descriptors too, so the proposal's
             * Tier-1 string-descriptor heuristic cannot tell them apart
             * either. The honest hint is therefore explicit ambiguity —
             * never a silent guess at one device. The authoritative
             * answer is the Tier-2 protocol probe (probe_teensy_serial),
             * run on Connect. */
            controllerHint = "ADF-Copy or Applesauce "
                             "(0x16C0:0x0483 — probe on Connect)";
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
#else
    // Fallback: Read COM ports from Windows Registry
    qCDebug(lcHwSerial) << "UFT_HAS_SERIALPORT not defined - using Windows Registry fallback";
#ifdef Q_OS_WIN
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                      L"HARDWARE\\DEVICEMAP\\SERIALCOMM",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        
        WCHAR valueName[256];
        WCHAR valueData[256];
        DWORD valueNameSize, valueDataSize, valueType;
        DWORD index = 0;
        
        while (true) {
            valueNameSize = sizeof(valueName) / sizeof(WCHAR);
            valueDataSize = sizeof(valueData);
            
            LONG result = RegEnumValueW(hKey, index, valueName, &valueNameSize,
                                        NULL, &valueType, (LPBYTE)valueData, &valueDataSize);
            
            if (result != ERROR_SUCCESS) break;
            
            if (valueType == REG_SZ) {
                QString portName = QString::fromWCharArray(valueData);
                QString deviceName = QString::fromWCharArray(valueName);
                
                QString displayName = portName;
                
                // Try to identify device type from registry path
                if (deviceName.contains("USBSER", Qt::CaseInsensitive) ||
                    deviceName.contains("VCP", Qt::CaseInsensitive)) {
                    displayName = QString("%1 - USB Serial").arg(portName);
                }
                
                qCDebug(lcHwSerial) << "  Found:" << portName << "Device:" << deviceName;
                ui->comboPort->addItem(displayName, portName);
            }
            index++;
        }
        RegCloseKey(hKey);
    } else {
        qCWarning(lcHwSerial) << "Failed to open registry key for COM ports";
    }
#endif
#endif
    
    if (ui->comboPort->count() == 0) {
        ui->comboPort->addItem(tr("(No ports found)"), "");
        ui->btnConnect->setEnabled(false);
    } else {
        ui->btnConnect->setEnabled(true);
    }
}

/* MF-170 (P1.19): `detectParallelPorts()` was the port-enumeration
 * path for the X1541 family. With those controllers removed from the
 * combo it has no callers and is deleted. The Qt-side LPT discovery
 * logic (~/dev/parport*, ~/dev/lp*, Windows LPT1..3) lived only here
 * and disappears with the function. */

void HardwareTab::onRefreshPorts()
{
    detectSerialPorts();
    updateStatus(tr("Port list refreshed."));
}

void HardwareTab::autoRefreshPorts()
{
    // Only auto-refresh when not connected
    if (m_connected) return;
    
    // Save current selection
    QString currentPort = ui->comboPort->currentData().toString();
    
    // Get current port count before refresh
    int oldCount = ui->comboPort->count();
    bool hadNoPorts = (oldCount == 1 && ui->comboPort->itemData(0).toString().isEmpty());
    
    // Refresh ports
    detectSerialPorts();
    
    // Restore selection if still available
    if (!currentPort.isEmpty()) {
        int idx = ui->comboPort->findData(currentPort);
        if (idx >= 0) {
            ui->comboPort->setCurrentIndex(idx);
        }
    }
    
    // Notify user if ports appeared
    int newCount = ui->comboPort->count();
    bool hasNoPorts = (newCount == 1 && ui->comboPort->itemData(0).toString().isEmpty());
    if (hadNoPorts && !hasNoPorts) {
        updateStatus(tr("Port detected: %1").arg(ui->comboPort->currentText()));
    }
}

// ============================================================================
// Connection
// ============================================================================

void HardwareTab::onConnect()
{
    QString port = ui->comboPort->currentData().toString();
    QString controller = ui->comboController->currentData().toString();
    
    qDebug() << "=== onConnect called ===";
    qDebug() << "Port:" << port;
    qDebug() << "Controller data:" << controller;
    qDebug() << "Controller text:" << ui->comboController->currentText();
    
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
    
    qDebug() << "Checking if controller is greaseweazle or fluxengine...";
    
    /* Per-controller dispatch (current architecture — supersedes the
     * MF-143 HardwareManager note that used to sit here).
     *
     * Greaseweazle takes the V2-provider path with a real open() against
     * the production-tested C-HAL (uft_gw_*, now internal behind
     * GreaseweazleProviderV2::do_*).
     *
     * MF-206 (P1.23): the other 8 controllers each construct their V2
     * provider honest-stub (null runners / handle) — see the
     * `controller != "greaseweazle"` branch below. All 9 land in
     * `m_providerV2` and are GUI-reachable; their production transports
     * (QProcess / QSerialPort / OpenCBM / libusb) are M3.x follow-up. */
    /* MF-170 (P1.19): the X1541 family (xa1541/xap1541/xm1541/xe1541/
     * x1541) was removed from `populateControllerList()` — the five
     * combo entries had no matching provider class and only ever
     * surfaced a "not yet wired" messagebox. With the entries gone
     * the previous MF-144/HW-A reject guard here is unreachable and
     * has been deleted. */

    if (controller != "greaseweazle") {
        /* MF-206 (P1.23 commit 2): route the 8 non-GW controllers through
         * their V2 providers. Each provider is constructed honest-stub —
         * null runners / null handle — so it is GUI-reachable and its
         * capability buttons are correctly gated by the codegen
         * (wire_action<cap::X> per concrete type), while every do_*
         * honestly returns a ProviderError ("backend not wired" /
         * "M3.x pending") rather than a silent no-op or a fabricated
         * capability. Real production transports (QProcess / QSerialPort
         * / OpenCBM / libusb runner factories) are follow-up M3.x work —
         * the providers themselves already return forensically-truthful
         * errors when their runner is null, which is the honest-scaffold
         * pattern the hardware-provider audit confirmed (ARCH-0). */
        bool constructed = true;
        if (controller == "scp") {
            m_providerV2 = std::make_unique<::uft::hal::SCPProviderV2>(nullptr);
        } else if (controller == "kryoflux") {
            m_providerV2 = std::make_unique<::uft::hal::KryoFluxProviderV2>(nullptr);
        } else if (controller == "fluxengine") {
            m_providerV2 = std::make_unique<::uft::hal::FluxEngineProviderV2>(nullptr);
        } else if (controller == "fc5025") {
            m_providerV2 = std::make_unique<::uft::hal::FC5025ProviderV2>(
                nullptr, nullptr);
        } else if (controller == "xum1541") {
            m_providerV2 = std::make_unique<::uft::hal::XUM1541ProviderV2>(
                nullptr, nullptr, nullptr);
        } else if (controller == "applesauce") {
#ifdef UFT_HAS_QSERIALPORT
            /* MF-250 (v4.1.5-hardening): if Qt6::SerialPort is built in,
             * try to open the user-selected port and build a real
             * runner-bound provider. On open failure fall back to the
             * honest-stub nullptr-runner path so the user still gets a
             * GUI-routed provider (with the orange "Preview" styling)
             * and a clear error message in the status bar — never a
             * silent connect to nothing. */
            auto _as_tx =
                ::uft::hal::QSerialPortApplesauceTransport::open(port);
            if (_as_tx) {
                ::uft::hal::ApplesauceTransportPtr shared(std::move(_as_tx));
                m_providerV2 =
                    std::make_unique<::uft::hal::ApplesauceProviderV2>(
                        ::uft::hal::make_applesauce_read_runner(shared),
                        ::uft::hal::make_applesauce_write_runner(shared),
                        ::uft::hal::make_applesauce_motor_runner(shared),
                        ::uft::hal::make_applesauce_seek_runner(shared),
                        ::uft::hal::make_applesauce_recal_runner(shared),
                        ::uft::hal::make_applesauce_rpm_runner(shared),
                        ::uft::hal::make_applesauce_detect_runner(shared));
            } else {
                /* Open failed — keep the honest-stub provider so the
                 * GUI still reflects the user's controller choice. */
                m_providerV2 =
                    std::make_unique<::uft::hal::ApplesauceProviderV2>(
                        nullptr, nullptr, nullptr, nullptr, nullptr,
                        nullptr, nullptr);
                updateStatus(tr("Applesauce: cannot open %1 — falling "
                                "back to preview stub. Reason: %2")
                                .arg(port,
                                     ::uft::hal::QSerialPortApplesauceTransport::lastOpenError()),
                            /*isError=*/true);
            }
#else
            m_providerV2 = std::make_unique<::uft::hal::ApplesauceProviderV2>(
                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
#endif
        } else if (controller == "adfcopy") {
#ifdef UFT_HAS_QSERIALPORT
            /* MF-252 (v4.1.5-hardening): parallel to MF-250 Applesauce
             * but with the ADFCopy binary protocol over QSerialPort. */
            auto _ac_tx =
                ::uft::hal::QSerialPortADFCopyTransport::open(port);
            if (_ac_tx) {
                ::uft::hal::ADFCopyTransportPtr shared(std::move(_ac_tx));
                m_providerV2 =
                    std::make_unique<::uft::hal::ADFCopyProviderV2>(
                        ::uft::hal::make_adfcopy_read_runner(shared),
                        ::uft::hal::make_adfcopy_motor_runner(shared),
                        ::uft::hal::make_adfcopy_seek_runner(shared),
                        ::uft::hal::make_adfcopy_recal_runner(shared),
                        ::uft::hal::make_adfcopy_detect_runner(shared));
            } else {
                m_providerV2 =
                    std::make_unique<::uft::hal::ADFCopyProviderV2>(
                        nullptr, nullptr, nullptr, nullptr, nullptr);
                updateStatus(tr("ADFCopy: cannot open %1 — falling back "
                                "to preview stub. Reason: %2")
                                .arg(port,
                                     ::uft::hal::QSerialPortADFCopyTransport::lastOpenError()),
                            /*isError=*/true);
            }
#else
            m_providerV2 = std::make_unique<::uft::hal::ADFCopyProviderV2>(
                nullptr, nullptr, nullptr, nullptr, nullptr);
#endif
        } else if (controller == "usb_floppy") {
            m_providerV2 = std::make_unique<::uft::hal::USBFloppyProviderV2>(
                nullptr, nullptr, nullptr);
        } else {
            constructed = false;
        }

        if (!constructed) {
            updateStatus(tr("Unknown controller key '%1' — no V2 provider.")
                             .arg(controller),
                         /*isError=*/true);
            return;
        }

        /* ARCH-7-C (MF-213): ADF-Copy and Applesauce are both stock-ID
         * Teensy devices (0x16C0:0x0483) and indistinguishable by USB
         * descriptor. When the user connects one of the two, run the
         * authoritative non-destructive protocol probe on the selected
         * port and warn — never silently override — if it contradicts
         * the combo selection. Sending ADF-Copy bytes to an Applesauce
         * (or vice versa) is exactly the "stille Veränderung" the
         * project forbids, so catching a mismatch up front is forensic
         * hygiene. probe_teensy_serial() opens its own port, probes,
         * closes; it returns Unknown if it cannot open the port or the
         * Qt build lacks QtSerialPort — Unknown never triggers a
         * warning (no guess in either direction). */
        if (controller == "adfcopy" || controller == "applesauce") {
            const ::uft::hal::TeensyDevice probed =
                ::uft::hal::probe_teensy_serial(port);
            const bool mismatch =
                (controller == "adfcopy" &&
                 probed == ::uft::hal::TeensyDevice::Applesauce) ||
                (controller == "applesauce" &&
                 probed == ::uft::hal::TeensyDevice::AdfCopy);
            if (mismatch) {
                const QString probedName =
                    (probed == ::uft::hal::TeensyDevice::Applesauce)
                        ? tr("Applesauce") : tr("ADF-Copy");
                QMessageBox::warning(this, tr("Device mismatch"),
                    tr("You selected %1, but the device on %2 answered "
                       "the %3 identify handshake.\n\nADF-Copy and "
                       "Applesauce share the stock Teensy USB ID and can "
                       "only be told apart by this probe. Connecting "
                       "anyway would send %1 commands to a %3 device — "
                       "check the controller selection.")
                        .arg(m_controllerType, port, probedName));
            }
        }

        /* The V2 provider is constructed (honest-stub backend). Wire its
         * capability buttons via the codegen std::visit and mark the tab
         * connected. No open()/detect here — the provider has no
         * production transport yet; the user sees correctly-gated
         * buttons and any click surfaces the provider's honest
         * ProviderError.
         *
         * UFT-003 (v4.1.5-hardening): explicitly distinguish honest-stub
         * connection from a production-wired one. Without this, the user
         * sees a green-stripe "Disconnect" button identical to a real
         * Greaseweazle connection, even though Read/Write will fail with
         * NOT_IMPLEMENTED. Mark the button orange + "Preview" text so
         * the gap is visible at a glance (Prinzip 4: no dangerous
         * defaults). */
        /* MF-250: detect whether this provider has a runner-bound
         * production transport. Currently true for Applesauce when
         * Qt6::SerialPort is available AND port-open succeeded above.
         * Anywhere else we stay on the orange honest-stub track. */
        bool _has_production_transport = false;
#ifdef UFT_HAS_QSERIALPORT
        if (controller == "applesauce" || controller == "adfcopy") {
            /* QSerialPort-runner-bound providers — MF-250 / MF-252. */
            _has_production_transport = true;
        }
#endif
#ifdef UFT_HAS_LIBUSB
        if (controller == "scp") {
            /* MF-254: libusb-based SCP-Direct. The C-API
             * uft_scp_direct_open() now talks to real hardware via
             * libusb; open/close/seek paths are wired. Full flux
             * read/write payload parsing is still pending hardware
             * bench verification (NOT_IMPLEMENTED returned from
             * read_flux until then) — but the device-presence and
             * device-control paths are real. Beta-tier styling
             * reflects this: code path is live, end-to-end flux
             * round-trip pending validation. */
            _has_production_transport = true;
        }
#endif

        rewireV2();
        m_connected = true;
        m_hwModel = 0;
        if (_has_production_transport) {
            m_firmwareVersion = QStringLiteral("V2 provider (QSerialPort, bench-unverified)");
        } else {
            m_firmwareVersion = QStringLiteral("V2 provider (backend scaffold)");
        }
        setConnectionState(true);
        if (_has_production_transport) {
            /* Yellow, not orange: transport real but no hardware bench
             * verification yet (docs/M3_APPLESAUCE_TRANSPORT.md §5). */
            ui->btnConnect->setText(tr("Disconnect (Beta)"));
            ui->btnConnect->setStyleSheet("background-color: #ffd966;");
            ui->btnConnect->setToolTip(
                tr("Production transport connected — but this code path "
                   "has not yet been verified against real hardware. "
                   "Operations may work but should be cross-checked "
                   "against a known-good capture. See "
                   "docs/M3_APPLESAUCE_TRANSPORT.md §5."));
            updateStatus(tr("%1 connected — V2 provider routed via "
                            "QSerialPort (BETA, bench-unverified). "
                            "Capability actions will exercise real "
                            "hardware; verify results.")
                            .arg(m_controllerType));
        } else {
            ui->btnConnect->setText(tr("Disconnect (Preview)"));
            ui->btnConnect->setStyleSheet("background-color: #ffaa55;");
            ui->btnConnect->setToolTip(
                tr("Honest-stub connection — the production transport "
                   "for this controller is not wired yet. Read/Write "
                   "actions will return UFT_ERR_NOT_IMPLEMENTED. See "
                   "docs/MASTER_PLAN.md §M3."));
            updateStatus(tr("%1 connected — V2 provider routed (PREVIEW). "
                            "The production transport for this controller "
                            "is not wired yet; capability actions return a "
                            "diagnostic error until the backend lands.")
                            .arg(m_controllerType));
        }
        emit connectionChanged(true);
        emit deviceInfoChanged(m_controllerType, m_firmwareVersion);
        return;
    }

    if (controller == "greaseweazle") {
        qDebug() << "YES - using HAL connection";
        // Real HAL connection attempt — MF-171 (P1.18) via V2 provider lifecycle
        #ifdef UFT_HAS_HAL
        qDebug() << "HardwareTab: Attempting GW V2 connection to" << port;

        /* MF-205 (P1.23): construct the GW provider in a local
         * unique_ptr, open() it, and only move it into m_providerV2 on
         * success. `gwp` is a stable raw view — a unique_ptr move
         * transfers ownership, not the pointed-to object — so it stays
         * valid after the move into the variant. */
        auto gwOwned = std::make_unique<::uft::hal::GreaseweazleProviderV2>();
        ::uft::hal::GreaseweazleProviderV2 *gwp = gwOwned.get();
        std::string err;
        if (gwp->open(port.toLocal8Bit().constData(), &err)) {
            /* Success: the variant takes ownership. */
            m_providerV2 = std::move(gwOwned);

            /* Pull cached firmware/model out of the V2 provider. */
            m_firmwareVersion = QString::fromStdString(gwp->firmware_version());
            if (m_firmwareVersion.isEmpty())
                m_firmwareVersion = QStringLiteral("Unknown");
            m_hwModel = gwp->hardware_model();

            /* Re-run the codegen wiring so every capability-bound
             * button connects through the type-driven pipeline. */
            rewireV2();

            m_connected = true;
            setConnectionState(true);

            if (m_autoDetect) {
                /* MF-210: auto-detect after a successful connect routes
                 * through runDetectProbe() — the same capability-driven
                 * std::visit path onDetectionModeChanged() uses, so the
                 * GW and the 8 other providers behave identically. */
                runDetectProbe();
            }

            QString deviceName = getDeviceName();
            updateStatus(tr("Connected to %1 (%2)")
                .arg(deviceName)
                .arg(m_firmwareVersion));
            emit connectionChanged(true);
            emit deviceInfoChanged(deviceName, m_firmwareVersion);
            return;
        }

        /* open() failed — `gwOwned` still owns the (unopened) provider
         * and destructs at end of scope; m_providerV2 stays
         * std::monostate. Surface the error. */
        const QString qerr = QString::fromStdString(err);
        qDebug() << "HardwareTab: GW V2 open failed:" << qerr;
        updateStatus(tr("Connection failed: %1").arg(qerr));
        QMessageBox::warning(this, tr("Connection Error"),
            tr("Failed to connect to %1 on %2.\n\nError: %3")
                .arg(m_controllerType, m_portName, qerr));
        return;
        #else
        // HAL not available - fall back to simulated connection
        qWarning() << "HAL not available, using simulated connection";
        #endif
    } else {
        qDebug() << "NO - controller is not greaseweazle, using simulated";
    }
    
    // Fallback: Simulated connection for unsupported controllers
    qDebug() << "Using SIMULATED connection";
    QTimer::singleShot(500, this, [this]() {
        m_connected = true;
        m_firmwareVersion = "Simulated";
        /* MF-210 (P1-4): a simulated connection has NO V2 provider —
         * m_providerV2 stays std::monostate. Run the codegen wiring so
         * its Phase-2 guard disables every capability-bound button;
         * without this the buttons keep whatever state the last
         * rewireV2() left and can appear enabled-but-unwired. */
        rewireV2();
        setConnectionState(true);
        /* MF-171 (P1.18): the old autoDetectDrive() helper that scanned
         * via uft_gw_* directly is gone. There is no V2 provider in
         * the simulated path (no hardware to query), so the drive
         * fields stay at their UI defaults until the user manually
         * configures them in Manual mode. */
        updateStatus(tr("Connected to %1 (%2) [SIMULATED]").arg(m_controllerType, m_firmwareVersion));
        emit connectionChanged(true);
    });
}

void HardwareTab::onDisconnect()
{
    /* MF-210 (P1-1/P1-2): issue a clean motor-off to whichever connected
     * provider actually satisfies cap::ControlsMotor — not just
     * Greaseweazle (Applesauce and ADF-Copy have it too). This MUST run
     * before the variant is cleared: the old code did the motor-off
     * after `m_providerV2 = monostate` in showHardwareDisconnected()'s
     * call path, sending the command to std::monostate where it was
     * lost. issueMotorOffIfRunning() is a no-op when no motor is
     * running and clears m_motorRunning itself. */
    issueMotorOffIfRunning();

    /* MF-171 (P1.18) / MF-205 (P1.23): the connected V2 provider owns
     * its handle/transport — its destructor releases it. Assigning
     * std::monostate destroys whichever alternative was active.
     *
     * Order:
     *   1. m_providerV2 = monostate → destroys the V2 provider
     *   2. rewireV2()               → Phase 1 disconnect, Phase 2 disable
     *                                 (provider now monostate) */
    m_providerV2 = std::monostate{};
    rewireV2();

    m_connected = false;
    m_hwModel = 0;
    setConnectionState(false);
    clearDetectedInfo();

    updateStatus(tr("Disconnected."));
    emit connectionChanged(false);
    emit deviceInfoChanged(QString(), QString());
}

/* ════════════════════════════════════════════════════════════════════════
 *  MF-157 (P1.4) — Type-Driven HAL handler overload set
 *
 *  Each handler corresponds to one alternative of one Sum-Type from
 *  `include/uft/hal/outcomes.h`. They are invoked by std::visit dispatch
 *  inside the codegen-emitted wire_hardware_tab() function — never
 *  directly. Adding a new alternative to a Sum-Type requires adding a
 *  matching overload here, otherwise the std::visit call inside
 *  generated/tab_hardware_wiring.gen.cpp fails to compile. That is the
 *  forensic guarantee turning "we should preserve every case" into a
 *  build break.
 *
 *  Each handler must SURFACE the variant detail, not summarize. Rule F-3
 *  (preserve forensic detail) and F-4 (3-part errors) are both upheld
 *  here.
 * ════════════════════════════════════════════════════════════════════════ */

/* MotorOutcome */
void HardwareTab::onMotorOutcome(const ::uft::hal::MotorRunning &v)
{
    m_motorRunning = true;
    updateMotorControlsEnabled();
    updateStatus(tr("Motor running (RPM=%1)").arg(v.measured_rpm, 0, 'f', 1));
}
void HardwareTab::onMotorOutcome(const ::uft::hal::MotorStopped &)
{
    m_motorRunning = false;
    updateMotorControlsEnabled();
    updateStatus(tr("Motor stopped"));
}
void HardwareTab::onMotorOutcome(const ::uft::hal::MotorStalled &v)
{
    m_motorRunning = false;
    updateMotorControlsEnabled();
    updateStatus(tr("Motor stalled: %1").arg(QString::fromStdString(v.reason)),
                 /*isError=*/true);
}

/* SeekOutcome */
void HardwareTab::onSeekOutcome(const ::uft::hal::SeekArrived &v)
{
    updateStatus(tr("Seek arrived at cylinder %1").arg(v.cylinder));
}
void HardwareTab::onSeekOutcome(const ::uft::hal::SeekOvershot &v)
{
    updateStatus(tr("Seek overshot: requested %1, actual %2")
                     .arg(v.requested).arg(v.actual),
                 /*isError=*/true);
}
void HardwareTab::onSeekOutcome(const ::uft::hal::SeekTrack0Failed &v)
{
    updateStatus(tr("Track-0 calibration failed: %1")
                     .arg(QString::fromStdString(v.reason)),
                 /*isError=*/true);
}

/* RpmOutcome */
void HardwareTab::onRpmOutcome(const ::uft::hal::RpmMeasured &v)
{
    updateStatus(tr("RPM=%1 (jitter=%2%, %3 revs sampled)")
                     .arg(v.rpm, 0, 'f', 2)
                     .arg(v.jitter_pct, 0, 'f', 2)
                     .arg(v.revolutions_sampled));
}

/* DetectOutcome */
void HardwareTab::onDetectOutcome(const ::uft::hal::DriveDetected &v)
{
    m_detectedTracks = v.tracks;
    m_detectedHeads = v.heads;
    updateStatus(tr("Drive detected: %1 (%2 tracks, %3 heads, %4 RPM nominal)")
                     .arg(QString::fromStdString(v.drive_kind))
                     .arg(v.tracks).arg(v.heads)
                     .arg(v.rpm_nominal, 0, 'f', 0));
}
void HardwareTab::onDetectOutcome(const ::uft::hal::DriveAbsent &v)
{
    updateStatus(tr("No drive detected (scanned for %1)")
                     .arg(QString::fromStdString(v.scanned_for)),
                 /*isError=*/true);
}

/* FluxOutcome */
void HardwareTab::onFluxOutcome(const ::uft::hal::FluxCaptured &v)
{
    updateStatus(tr("Flux captured C%1/H%2: %3 transitions, %4 revolutions, %5 ns/sample")
                     .arg(v.position.cylinder).arg(v.position.head)
                     .arg(v.transitions_ns.size())
                     .arg(v.revolutions)
                     .arg(v.sample_ns, 0, 'f', 1));
}
void HardwareTab::onFluxOutcome(const ::uft::hal::FluxMarginal &v)
{
    /* F-3: do NOT collapse the marginal flux — preserve transition count
     * and the anomaly note for the audit trail. */
    updateStatus(tr("Flux MARGINAL at C%1/H%2: %3 transitions, anomaly: %4")
                     .arg(v.position.cylinder).arg(v.position.head)
                     .arg(v.transitions_ns.size())
                     .arg(QString::fromStdString(v.anomaly_note)),
                 /*isError=*/true);
}
void HardwareTab::onFluxOutcome(const ::uft::hal::FluxUnreadable &v)
{
    updateStatus(tr("Flux unreadable at C%1/H%2: %3")
                     .arg(v.position.cylinder).arg(v.position.head)
                     .arg(QString::fromStdString(v.physical_reason)),
                 /*isError=*/true);
}

/* Cross-variant alternatives (every Outcome carries them) */
void HardwareTab::showProviderError(const ::uft::hal::ProviderError &e)
{
    /* F-4: surface ALL three parts. The status bar gets `what`, the log
     * (qWarning) gets all three. A future P1.5 pass can replace the log
     * line with a proper 3-part dialog. */
    updateStatus(tr("Error: %1").arg(QString::fromStdString(e.what)),
                 /*isError=*/true);
    qWarning("[HardwareTab] ProviderError\n  what: %s\n  why : %s\n  fix : %s",
             e.what.c_str(), e.why.c_str(), e.fix.c_str());
}
void HardwareTab::showHardwareDisconnected(const ::uft::hal::HardwareDisconnected &d)
{
    updateStatus(tr("Hardware disconnected (%1)")
                     .arg(QString::fromStdString(d.device_path)),
                 /*isError=*/true);
    /* MF-210 (P1-2): delegate the full teardown to onDisconnect() instead
     * of clearing the variant here first. onDisconnect() now issues the
     * capability-driven motor-off against the still-live provider BEFORE
     * destroying it; the old code cleared the variant to std::monostate
     * up here, so the subsequent onDisconnect() motor-off was lost. */
    if (m_connected) {
        onDisconnect();
    } else {
        /* Defensive: not flagged "connected" but a provider somehow
         * lingers in the variant — clear it and re-gate the UI. */
        m_providerV2 = std::monostate{};
        rewireV2();
    }
}
void HardwareTab::showPolicyRequired(const ::uft::hal::CapabilityRequiresPolicy &p)
{
    /* TODO P1.6/P1.7: open a proper consent dialog. For now we flag it
     * loudly in the status bar so the operation is not silently dropped. */
    updateStatus(tr("Policy gate: %1").arg(QString::fromStdString(p.explain)),
                 /*isError=*/true);
}

void HardwareTab::onControllerChanged(int index)
{
    Q_UNUSED(index);
    QString controller = ui->comboController->currentData().toString();
    
    // Skip separators
    if (controller.startsWith("separator_")) {
        // Move to next valid item
        int nextIdx = ui->comboController->currentIndex() + 1;
        if (nextIdx < ui->comboController->count()) {
            ui->comboController->setCurrentIndex(nextIdx);
        }
        return;
    }
    
    // USB Floppy has different capabilities
    bool isUSB = (controller == "usb_floppy");
    
    /* Check if this is a Commodore controller.
     * MF-170 (P1.19): the LPT-based X1541 family was removed from the
     * combo; `isCommodoreLPT` would always be false and is therefore
     * gone. `isCommodore == isCommodoreUSB` now. */
    bool isCommodoreUSB = (controller == "xum1541");
    bool isCommodore = isCommodoreUSB;
    bool isFlux = (controller == "greaseweazle" || controller == "scp" ||
                   controller == "kryoflux" || controller == "fluxengine");

    // Disable flux-specific options for USB and Commodore
    ui->groupAdvanced->setEnabled(isFlux);

    // Update port list based on controller type
    if (isCommodoreUSB) {
        // Show USB devices for ZoomFloppy/XUM1541
        detectSerialPorts();
        updateStatus(tr("Commodore USB adapter selected - uses OpenCBM."));
    } else if (isUSB) {
        detectSerialPorts();
        updateStatus(tr("USB Floppy selected - limited to standard formats."));
    } else {
        detectSerialPorts();
        updateStatus(tr("Flux controller selected."));
    }
    
    // Update drive selection for Commodore (device 8-15)
    if (isCommodore) {
        ui->comboDriveSelect->clear();
        ui->comboDriveSelect->addItem(tr("Device 8"), 8);
        ui->comboDriveSelect->addItem(tr("Device 9"), 9);
        ui->comboDriveSelect->addItem(tr("Device 10"), 10);
        ui->comboDriveSelect->addItem(tr("Device 11"), 11);
    } else {
        ui->comboDriveSelect->clear();
        ui->comboDriveSelect->addItem(tr("Drive 0"), 0);
        ui->comboDriveSelect->addItem(tr("Drive 1"), 1);
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
        /* MF-177 / MF-210 (P1-3): when the user switches to auto-detect
         * while already connected, probe the drive through
         * runDetectProbe() — a capability-driven std::visit over ALL 9
         * providers (every one satisfies cap::DetectsDrive). The old code
         * was a Greaseweazle-only std::get_if + is_open() check, so the
         * mode-switch probe was a silent no-op for the other 8
         * controllers. runDetectProbe() is a no-op when disconnected. */
        runDetectProbe();
    } else {
        updateStatus(tr("Manual mode - configure drive settings manually."));
    }
}
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
    
    // Stop/start auto-refresh timer based on connection state
    if (m_portRefreshTimer) {
        if (connected) {
            m_portRefreshTimer->stop();
        } else {
            m_portRefreshTimer->start(2000);
        }
    }
    
    updateDriveSettingsEnabled();
    updateMotorControlsEnabled();
    updateAdvancedEnabled();
    /* MF-210 (P1-5): updateTestButtonsEnabled() is gone. The five test
     * buttons (btnSeekTest/btnReadTest/btnRPMTest/btnCalibrate/btnDetect)
     * are capability-bound — their enabled-state is owned solely by the
     * codegen `wire_action<cap::X>` (run from rewireV2()). The old helper
     * unconditionally re-enabled them on `m_connected`, which ran AFTER
     * rewireV2() and clobbered the type-driven gating, leaving
     * dead-but-enabled buttons for every provider with a partial
     * capability set. The codegen is now the single source of truth. */

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
    ui->checkAutoSpinDown->setEnabled(m_connected);

    /* MF-210 (P1-5): btnMotorOn / btnMotorOff are capability-bound. The
     * codegen `wire_action<cap::ControlsMotor>` owns the *capability*
     * gate (connect-or-disable per provider type); this function applies
     * only the orthogonal *running-state* refinement, and ONLY for a
     * provider that actually satisfies ControlsMotor. For a provider
     * without it (e.g. SCP) both buttons stay disabled — matching what
     * the codegen already did, instead of the old code unconditionally
     * re-enabling them on `m_connected`.
     *
     * providerControlsMotor() derives the answer from the provider TYPE,
     * so this is correct regardless of call order (it is also invoked
     * from onMotorOutcome(), with no preceding rewireV2()). */
    const bool hasMotor = m_connected && providerControlsMotor();
    ui->btnMotorOn->setEnabled(hasMotor && !m_motorRunning);
    ui->btnMotorOff->setEnabled(hasMotor && m_motorRunning);
}

void HardwareTab::updateAdvancedEnabled()
{
    bool enabled = m_connected && !m_autoDetect;

    ui->checkDoubleStep->setEnabled(enabled);
    ui->checkIgnoreIndex->setEnabled(enabled);
}

/* MF-210 (P1-5): updateTestButtonsEnabled() removed — see the comment in
 * setConnectionState(). The codegen `wire_action<cap::X>` is the single
 * source of truth for btnSeekTest/btnReadTest/btnRPMTest/btnCalibrate/
 * btnDetect; nothing else may setEnabled() on them. */

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

/* MF-177: restored — the MF-171 (P1.18) zombie-slot deletion script's
 * brace-balance logic mis-cut and ate this function's SIGNATURE line
 * while leaving the body orphaned. `onAutoSpinDownChanged` is still
 * connected in setupConnections() and must exist. */
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
// Device Info
// ============================================================================

/* MF-177: restored — the MF-171 (P1.18) zombie-slot deletion script
 * mis-cut and ate this function's HEAD (signature + opening brace +
 * the `if (!m_connected)` guard line), leaving the body orphaned.
 * `getDeviceName()` is a public method used by the status bar and is
 * declared in hardwaretab.h — it must exist. */
QString HardwareTab::getDeviceName() const
{
    if (!m_connected) {
        return tr("Not connected");
    }

    // Build device name based on controller type and model
    QString name;
    
    if (m_controllerType.contains("Greaseweazle", Qt::CaseInsensitive)) {
        // Greaseweazle models: F1=1, F7=7, V4=4, Plus=5
        switch (m_hwModel) {
            case 1: name = "Greaseweazle F1"; break;
            case 4: name = "Greaseweazle V4"; break;
            case 5: name = "Greaseweazle Plus"; break;
            case 7: name = "Greaseweazle F7"; break;
            default: name = QString("Greaseweazle (Model %1)").arg(m_hwModel); break;
        }
    } else if (m_controllerType.contains("FluxEngine", Qt::CaseInsensitive)) {
        name = "FluxEngine";
    } else if (m_controllerType.contains("SuperCard", Qt::CaseInsensitive)) {
        name = "SuperCard Pro";
    } else if (m_controllerType.contains("KryoFlux", Qt::CaseInsensitive)) {
        name = "KryoFlux";
    } else {
        name = m_controllerType;
    }

    return name;
}


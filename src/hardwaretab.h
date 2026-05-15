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

#include <memory>
#include <variant>

#include "uft/hal/outcomes.h"   /* forensic Sum-Types — handlers take refs */

namespace Ui { class TabHardware; }

class HardwareTab;              /* fwd-decl needed before the codegen
                                 * namespace below references it */
struct DetectedDriveInfo;
struct HardwareInfo;

/* MF-205 (P1.23): all 9 V2 provider types are forward-declared here.
 * They share NO base class (the type-driven design makes capabilities a
 * type-property, not a virtual method), so "the currently-connected
 * provider, whichever of the 9 it is" is modelled as a std::variant of
 * std::unique_ptr — the active alternative IS the "which controller is
 * connected" fact, and the unique_ptr gives RAII + stays movable even
 * though several providers (Greaseweazle, SCP) delete their move ops.
 * std::monostate (first) = "disconnected". The variant's destructor
 * needs the complete types — HardwareTab's destructor is defined in
 * hardwaretab.cpp, which #includes all 9 provider headers. */
namespace uft::hal {
    class GreaseweazleProviderV2; class SCPProviderV2;
    class KryoFluxProviderV2;     class FluxEngineProviderV2;
    class FC5025ProviderV2;       class XUM1541ProviderV2;
    class ApplesauceProviderV2;   class ADFCopyProviderV2;
    class USBFloppyProviderV2;
}

using ProviderV2Variant = std::variant<
    std::monostate,
    std::unique_ptr<::uft::hal::GreaseweazleProviderV2>,
    std::unique_ptr<::uft::hal::SCPProviderV2>,
    std::unique_ptr<::uft::hal::KryoFluxProviderV2>,
    std::unique_ptr<::uft::hal::FluxEngineProviderV2>,
    std::unique_ptr<::uft::hal::FC5025ProviderV2>,
    std::unique_ptr<::uft::hal::XUM1541ProviderV2>,
    std::unique_ptr<::uft::hal::ApplesauceProviderV2>,
    std::unique_ptr<::uft::hal::ADFCopyProviderV2>,
    std::unique_ptr<::uft::hal::USBFloppyProviderV2>>;

/* MF-157 (P1.4): forward-declaration of the codegen-emitted wire-up
 * function. Implemented in generated/tab_hardware_wiring.gen.cpp (output
 * of tools/wiring_codegen.py from forms/tab_hardware.actions.yaml +
 * forms/tab_hardware.ui). The function needs deep access to HardwareTab's
 * private ui-pointer and the GreaseweazleProviderV2 instance — granted
 * via a friend declaration on the class below.
 *
 * `::HardwareTab` is fully qualified so the parameter type cannot be
 * mistaken for a fwd-decl into `uft::gui::generated::`. */
namespace uft::gui::generated {
    void wire_hardware_tab(::HardwareTab *self);
}

class HardwareTab : public QWidget
{
    Q_OBJECT

    /* MF-157 (P1.4): the codegen-emitted wire-up function needs to reach
     * `ui->btnX` (private), `m_gwProviderV2` (private), and the public
     * handler overload set declared below. Granting friendship to that
     * one function is preferable to making `ui` public — the deep access
     * stays narrowly scoped to one auto-generated translation unit that
     * lives in `generated/`. */
    friend void ::uft::gui::generated::wire_hardware_tab(::HardwareTab *self);

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

    /* MF-202 (P1.22): `gwDevice()` — the legacy `void*` C-handle
     * accessor — is removed. FluxCaptureJob (P1.20) and FluxWriteJob
     * (P1.21) now receive the non-owning `GreaseweazleProviderV2*` via
     * `currentProviderV2()` and drive it through the V2 outcome
     * surface; nothing reaches for the raw handle any more. */
    int detectedTracks() const { return m_detectedTracks; }
    int detectedHeads() const { return m_detectedHeads; }
    
    // Device info getters
    QString getDeviceName() const;
    QString getFirmwareVersion() const { return m_firmwareVersion; }
    QString getPortName() const { return m_portName; }
    int getHardwareModel() const { return m_hwModel; }

    /* ──────────────────────────────────────────────────────────────────
     *  MF-157 (P1.4) / MF-205 (P1.23) — Type-Driven HAL V2 surface
     *
     *  `currentProviderV2()` is the single accessor consumed by the
     *  codegen-emitted `wire_hardware_tab(self)` (see
     *  `forms/tab_hardware.actions.yaml`'s `provider_source:` entry).
     *
     *  MF-205 (P1.23): it now returns the `ProviderV2Variant` by const
     *  reference — whichever of the 9 V2 providers is connected, or
     *  `std::monostate` when disconnected. The codegen `std::visit`s it:
     *  inside the visit lambda the pointer is a concrete provider type,
     *  so `wire_action<cap::X>` still instantiates per concrete type and
     *  its capability gating stays 100% structural.
     *
     *  The handler overload set below is invoked by the codegen-emitted
     *  std::visit dispatch (one overload per Outcome-variant alternative).
     *  Each handler is RESPONSIBLE for surfacing the forensic detail of
     *  its variant — never collapse `SectorMarginal`'s divergent_reads to
     *  a single sample (rule F-3), never reduce ProviderError to one line
     *  (rule F-4 — what / why / fix all surfaced).
     *
     *  Adding a new alternative to a Sum-Type in `outcomes.h` will fail
     *  to compile here until a matching overload is added. That is the
     *  forensic contract turning "we should preserve every case" into a
     *  build break.
     * ────────────────────────────────────────────────────────────────── */
    const ProviderV2Variant &currentProviderV2() const noexcept;

    /* MotorOutcome variant — one overload per alternative (excl. shared
     * ProviderError / HardwareDisconnected / CapabilityRequiresPolicy
     * which are routed through the show* methods further below). */
    void onMotorOutcome(const ::uft::hal::MotorRunning &v);
    void onMotorOutcome(const ::uft::hal::MotorStopped &v);
    void onMotorOutcome(const ::uft::hal::MotorStalled &v);

    /* SeekOutcome variant. */
    void onSeekOutcome(const ::uft::hal::SeekArrived &v);
    void onSeekOutcome(const ::uft::hal::SeekOvershot &v);
    void onSeekOutcome(const ::uft::hal::SeekTrack0Failed &v);

    /* RpmOutcome variant. */
    void onRpmOutcome(const ::uft::hal::RpmMeasured &v);

    /* DetectOutcome variant. */
    void onDetectOutcome(const ::uft::hal::DriveDetected &v);
    void onDetectOutcome(const ::uft::hal::DriveAbsent &v);

    /* FluxOutcome variant. */
    void onFluxOutcome(const ::uft::hal::FluxCaptured &v);
    void onFluxOutcome(const ::uft::hal::FluxMarginal &v);
    void onFluxOutcome(const ::uft::hal::FluxUnreadable &v);

    /* Cross-variant alternatives — every Outcome variant carries these
     * three. Single implementation each (F-4 enforced — what/why/fix
     * surfaced verbatim). */
    void showProviderError(const ::uft::hal::ProviderError &e);
    void showHardwareDisconnected(const ::uft::hal::HardwareDisconnected &d);
    void showPolicyRequired(const ::uft::hal::CapabilityRequiresPolicy &p);

signals:
    void connectionChanged(bool connected);
    void statusMessage(const QString& message);
    void deviceInfoChanged(const QString& deviceName, const QString& firmware);

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
    void autoRefreshPorts();
    void onConnect();
    void onDisconnect();
    void onControllerChanged(int index);
    
    // Role selection (Source/Destination)
    void onRoleChanged(int roleId);
    
    // Detection mode
    void onDetectionModeChanged();

    // Motor control
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

    /* MF-169 (P1.17): `onUnifiedCapture()` removed with the V1 hierarchy.
     * MF-171 (P1.18): the legacy direct-C-API test slots (onDetectDrive,
     * onMotorOn / onMotorOff, onSeekTest, onReadTest, onRPMTest,
     * onCalibrate) removed — they were unwired since P1.4 (the
     * codegen-emitted wire_action<cap::X> path replaced their button
     * connections) and existed only as dead V1-shape implementations
     * that still touched uft_gw_* directly. Their behavior is preserved
     * through the V2 outcome handler overload set declared above. */

private:
    void setupConnections();
    void setupButtonGroups();
    void detectSerialPorts();
    /* MF-170 (P1.19): `detectParallelPorts()` removed with the X1541
     * family — no parallel-port path remains in the new architecture. */

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
    /* MF-210: `updateTestButtonsEnabled()` removed. The five test buttons
     * (btnSeekTest / btnReadTest / btnRPMTest / btnCalibrate / btnDetect)
     * are capability-bound — their enabled-state is owned solely by the
     * codegen `wire_action<cap::X>` (single source of truth). The old
     * helper unconditionally re-enabled them on `m_connected`, clobbering
     * the type-driven gating for every non-GW provider. */
    void updateRoleButtonsEnabled();

    /* ── MF-210: capability-aware provider helpers ──────────────────────
     * The connected provider is one of 9 unrelated types in a std::variant;
     * these wrap the std::visit + `if constexpr (cap::X<P>)` boilerplate so
     * the connect/disconnect/detect paths stay capability-driven instead of
     * hardcoded to the Greaseweazle alternative. */

    /* True iff the currently-connected provider satisfies cap::ControlsMotor.
     * Used by updateMotorControlsEnabled() to refine the motor buttons'
     * running-state toggle WITHIN the codegen's capability grant. */
    bool providerControlsMotor() const;

    /* If a motor-capable provider is connected and the motor is running,
     * issue set_motor(false). Outcome discarded — used only on teardown.
     * Must run BEFORE the variant is cleared to monostate. */
    void issueMotorOffIfRunning();

    /* Run detect_drive() on whichever provider is connected (all 9 satisfy
     * cap::DetectsDrive) and dispatch the DetectOutcome through the same
     * handler set the codegen-wired btnDetect uses. No-op if disconnected. */
    void runDetectProbe();
    
    // Status updates
    void updateStatus(const QString& status, bool isError = false);
    void clearDetectedInfo();
    void setDetectedInfo(const QString& model, const QString& firmware, 
                         const QString& rpm, const QString& index);
    
    /* MF-171 (P1.18): the old `autoDetectDrive()` helper (~100 LOC of
     * direct uft_gw_select_drive/set_motor/seek/is_write_protected
     * calls) is gone. Auto-detect after a successful Connect now
     * invokes the V2 surface — `m_gwProviderV2->detect_drive()` →
     * `std::visit` → existing `onDetectOutcome(DriveDetected)` handler.
     * The old `applyDetectedSettings()` is no longer needed; the V2
     * handler updates `m_detectedTracks`/`m_detectedHeads` directly
     * (see MF-157 P1.4 implementation). */

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

    /* MF-157 (P1.4): V2 mixin-composition wrapper around the connected
     * controller. MF-205 (P1.23): the single
     * `unique_ptr<GreaseweazleProviderV2>` member became a
     * `ProviderV2Variant` — the active alternative is whichever of the 9
     * V2 providers is connected (or `std::monostate` = disconnected).
     * The provider OWNS its handle/transport (MF-171); the unique_ptr is
     * the sole owner. The variant's destructor needs the complete
     * provider types — HardwareTab's destructor is defined in
     * hardwaretab.cpp, which #includes all 9 provider headers. */
    ProviderV2Variant m_providerV2;

    /* Re-runs `wire_hardware_tab(this)` so Phase-1 disconnect, Phase-2
     * disable, and Phase-3 wire-up reflect the current `currentProviderV2()`.
     * Called from `setupConnections()` (initial), `onConnect()` (after the
     * V2 wrapper is created), and `onDisconnect()` (before/after the
     * V2 wrapper is destroyed). */
    void rewireV2();

    // Detected drive info
    QString m_detectedModel;
    int m_detectedTracks;
    int m_detectedHeads;
    QString m_detectedDensity;
    int m_detectedRPM;
    
    // Timers
    QTimer *m_motorTimer;
    QTimer *m_statusTimer;
    QTimer *m_portRefreshTimer;
};

#endif // HARDWARETAB_H

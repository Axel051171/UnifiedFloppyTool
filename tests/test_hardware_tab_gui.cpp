/**
 * @file test_hardware_tab_gui.cpp
 * @brief GUI improvement test — HardwareTab capability gating (P3.3 #110).
 *
 * The executable proof of the type-driven HAL refactor's GUI half: the
 * Hardware tab greys out exactly the capability actions a connected
 * provider's TYPE does not satisfy — and enables exactly the ones it
 * does. `gw` has no GUI and no capability model; this is a UFT-only
 * property.
 *
 * Why a C++ QtTest and not pytest-qt: pytest-qt drives Qt apps written
 * in Python (PyQt/PySide). UFT's GUI is C++ Qt with no Python bindings,
 * so pytest-qt cannot reach HardwareTab at all. The right tool is
 * QtTest — same shape as tests/test_wiring_runtime.cpp.
 *
 * How it tests without friend access: the buttons carry objectNames
 * (btnReadTest, btnMotorOn, ...) from forms/tab_hardware.ui, so
 * QObject::findChild<QPushButton*>() reaches them through the public
 * API. The provider is connected by driving the real connect path
 * (select controller + port, click Connect) — onConnect() constructs
 * the honest-stub V2 provider (MF-206) and rewireV2() runs the codegen
 * gating.
 *
 * Self-checking matrix: the "expected enabled" state for each button is
 * derived from the capability CONCEPT applied to the provider TYPE
 * (`uft::hal::ReadsRawFlux<SCPProviderV2>` etc.) — never a hard-coded
 * table. So if a provider's capability set changes, the test
 * automatically expects the matching GUI change, and still fails if the
 * GUI stops tracking the type.
 *
 * Covers MF-205/MF-206 (variant routing of all 9 providers) and MF-210
 * (the updateTestButtonsEnabled/updateMotorControlsEnabled fixes that
 * stopped clobbering the codegen gating).
 */
#include <QtTest/QtTest>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>

#include "hardwaretab.h"

#include "hardware_providers/scp_provider_v2.h"
#include "hardware_providers/kryoflux_provider_v2.h"
#include "hardware_providers/fluxengine_provider_v2.h"
#include "hardware_providers/fc5025_provider_v2.h"
#include "hardware_providers/xum1541_provider_v2.h"
#include "hardware_providers/applesauce_provider_v2.h"
#include "hardware_providers/adfcopy_provider_v2.h"
#include "hardware_providers/usbfloppy_provider_v2.h"

#include "uft/hal/concepts.h"

namespace cap = uft::hal;

/* The 7 capability-bound buttons and the concept each is wired to
 * (forms/tab_hardware.actions.yaml). btnMotorOn/Off both map to
 * ControlsMotor; the rest are 1:1. */
class TestHardwareTabGui : public QObject {
    Q_OBJECT

    /* Select `key` in comboController, give comboPort a non-empty entry,
     * and click Connect — driving the real onConnect() path. */
    static void connect_controller(HardwareTab &tab, const QString &key) {
        /* Switch to the Destination role first: it offers the SUPERSET
         * of controllers — every flux controller PLUS the USB Floppy
         * drive, which Source mode omits (populateControllerList). One
         * role => one code path for all 8 non-GW providers. */
        auto *roleDest = tab.findChild<QRadioButton *>("radioDestination");
        QVERIFY2(roleDest, "radioDestination not found");
        roleDest->click();

        auto *combo = tab.findChild<QComboBox *>("comboController");
        QVERIFY2(combo, "comboController not found");
        int idx = combo->findData(key);
        QVERIFY2(idx >= 0, qPrintable("controller key not in combo: " + key));
        combo->setCurrentIndex(idx);

        auto *port = tab.findChild<QComboBox *>("comboPort");
        QVERIFY2(port, "comboPort not found");
        port->clear();
        port->addItem("Test Port", "test_port");   /* non-empty data */
        port->setCurrentIndex(0);

        auto *btnConnect = tab.findChild<QPushButton *>("btnConnect");
        QVERIFY2(btnConnect, "btnConnect not found");
        btnConnect->click();
        QVERIFY2(tab.isConnected(),
                 qPrintable("onConnect did not connect: " + key));
    }

    /* Assert each capability button's enabled-state == the capability
     * concept applied to the provider TYPE. ProviderT is the concrete
     * V2 type for `key`. */
    /* Resolve a button by objectName, failing the test if it is absent.
     * Kept out of check_gating's QCOMPARE expressions because QVERIFY2
     * expands to `return;` and cannot sit inside a value-returning
     * lambda. */
    static QPushButton *btn(HardwareTab &tab, const char *name) {
        auto *b = tab.findChild<QPushButton *>(name);
        if (!b) {
            qWarning("button missing: %s", name);
        }
        return b;
    }

    template <class ProviderT>
    void check_gating(const QString &key) {
        HardwareTab tab;
        connect_controller(tab, key);

        QPushButton *bRead  = btn(tab, "btnReadTest");
        QPushButton *bDet   = btn(tab, "btnDetect");
        QPushButton *bSeek  = btn(tab, "btnSeekTest");
        QPushButton *bCal   = btn(tab, "btnCalibrate");
        QPushButton *bRpm   = btn(tab, "btnRPMTest");
        QPushButton *bMOn   = btn(tab, "btnMotorOn");
        QPushButton *bMOff  = btn(tab, "btnMotorOff");
        QVERIFY(bRead && bDet && bSeek && bCal && bRpm && bMOn && bMOff);

        /* 1:1 capability buttons — GUI must mirror the concept exactly. */
        QCOMPARE(bRead->isEnabled(), cap::ReadsRawFlux<ProviderT>);
        QCOMPARE(bDet->isEnabled(),  cap::DetectsDrive<ProviderT>);
        QCOMPARE(bSeek->isEnabled(), cap::SeeksHead<ProviderT>);
        QCOMPARE(bCal->isEnabled(),  cap::Recalibrates<ProviderT>);
        QCOMPARE(bRpm->isEnabled(),  cap::MeasuresRPM<ProviderT>);

        /* Motor buttons: btnMotorOn is gated on ControlsMotor AND
         * !running; freshly connected the motor is not running, so
         * btnMotorOn mirrors the concept and btnMotorOff is off
         * regardless (MF-210 updateMotorControlsEnabled). */
        QCOMPARE(bMOn->isEnabled(),  cap::ControlsMotor<ProviderT>);
        QCOMPARE(bMOff->isEnabled(), false);
    }

private slots:

    /* No provider connected (m_providerV2 == monostate): the codegen
     * Phase-2 guard must leave every capability button DISABLED — the
     * "no controller selected" affordance. */
    void initial_state_disables_all_capability_buttons() {
        HardwareTab tab;
        QVERIFY(!tab.isConnected());
        for (const char *name : {"btnReadTest", "btnDetect", "btnSeekTest",
                                 "btnCalibrate", "btnRPMTest",
                                 "btnMotorOn", "btnMotorOff"}) {
            auto *b = tab.findChild<QPushButton *>(name);
            QVERIFY2(b, qPrintable(QString("button missing: ") + name));
            QVERIFY2(!b->isEnabled(),
                     qPrintable(QString(name) +
                                " enabled with no provider connected"));
        }
    }

    /* Each non-GW provider, connected, must have exactly its
     * capability set reflected in the button states. GW is excluded:
     * its onConnect path does a real open() that fails without
     * hardware, so no provider is constructed. */
    void scp_capability_gating()       { check_gating<uft::hal::SCPProviderV2>("scp"); }
    void kryoflux_capability_gating()  { check_gating<uft::hal::KryoFluxProviderV2>("kryoflux"); }
    void fluxengine_capability_gating(){ check_gating<uft::hal::FluxEngineProviderV2>("fluxengine"); }
    void fc5025_capability_gating()    { check_gating<uft::hal::FC5025ProviderV2>("fc5025"); }
    void xum1541_capability_gating()   { check_gating<uft::hal::XUM1541ProviderV2>("xum1541"); }
    void applesauce_capability_gating(){ check_gating<uft::hal::ApplesauceProviderV2>("applesauce"); }
    void adfcopy_capability_gating()   { check_gating<uft::hal::ADFCopyProviderV2>("adfcopy"); }
    void usbfloppy_capability_gating() { check_gating<uft::hal::USBFloppyProviderV2>("usb_floppy"); }

    /* ── #43 D1 (MF-XXXX): the drive selection ─────────────────────────
     *
     * Before #43 nothing read comboDriveSelect and the Greaseweazle always
     * ran unit 0. The connect path now reads the combo's ITEM DATA, so
     * the data must be there: the .ui items carry none, and
     * `QVariant().toInt()` is 0 — the old defect in a new shape.
     *
     * NOT exercised here, and by no other test either: connecting a
     * Greaseweazle and changing the combo while it is connected — i.e.
     * the unit passed to open(port, unit), the combo's connect() to
     * onDriveSelectChanged(), the motor-off before set_drive_unit(), and
     * the refusal while m_fluxJobRunning is set. Test targets are built
     * without UFT_HAS_HAL (HardwareTab's GW connect takes the simulated
     * branch, so no Greaseweazle provider is ever held), and a failed
     * open() raises a modal QMessageBox. Covered elsewhere: the mapping
     * (test_gw_drive_unit_select) and the provider half — unit 1 really
     * selected on the IBM-PC bus (test_gw_rpm_indexzeiten, W1/W2). */

    /* Guard: onControllerChanged() runs during construction (measured
     * here, not assumed) and replaces the data-less .ui items.
     * (The name must not end in "_data": QtTest treats such a slot as the
     * data function of another test and never runs it — measured.) */
    void gw_drive_combo_carries_unit_numbers() {
        HardwareTab tab;
        auto *drive = tab.findChild<QComboBox *>("comboDriveSelect");
        QVERIFY2(drive, "comboDriveSelect not found");
        auto *ctrl = tab.findChild<QComboBox *>("comboController");
        QVERIFY2(ctrl, "comboController not found");
        QCOMPARE(ctrl->currentData().toString(), QStringLiteral("greaseweazle"));
        /* MF-1376: A:/B: (IBM-PC bus) plus Shugart 0..3 — gw's six --drive
         * values. */
        QCOMPARE(drive->count(), 6);
        for (int i = 2; i < 6; ++i) {
            bool ok = false;
            QCOMPARE(drive->itemData(i).toInt(&ok), 16 + (i - 2));
            QVERIFY2(ok, "Shugart item carries no integer data");
        }
        bool ok0 = false, ok1 = false;
        QCOMPARE(drive->itemData(0).toInt(&ok0), 0);
        QCOMPARE(drive->itemData(1).toInt(&ok1), 1);
        QVERIFY2(ok0 && ok1, "drive combo items carry no integer unit data");
    }

    /* The labels follow forms/tab_hardware.ui and gw's `--drive=A/B`
     * (IBM-PC bus), not a bare "Drive 0/1" that reads like `--drive=0/1`
     * (Shugart numbering). */
    void gw_drive_combo_uses_ibm_pc_labels() {
        HardwareTab tab;
        auto *drive = tab.findChild<QComboBox *>("comboDriveSelect");
        QVERIFY2(drive, "comboDriveSelect not found");
        /* MF-1376 (petrkr, #43): "A: (Drive 0)" named ONE drive by two
         * different buses. */
        QCOMPARE(drive->itemText(0), QStringLiteral("A: (IBM-PC bus)"));
        QCOMPARE(drive->itemText(1), QStringLiteral("B: (IBM-PC bus)"));
        QCOMPARE(drive->itemText(2), QStringLiteral("Drive 0 (Shugart bus)"));
        QCOMPARE(drive->itemText(5), QStringLiteral("Drive 3 (Shugart bus)"));
    }

    /* Guard: the same combo carries Commodore device numbers for the
     * XUM1541 — which is why the GW path must check the controller. */
    void commodore_drive_combo_keeps_device_numbers() {
        HardwareTab tab;
        auto *ctrl = tab.findChild<QComboBox *>("comboController");
        auto *drive = tab.findChild<QComboBox *>("comboDriveSelect");
        QVERIFY(ctrl && drive);
        ctrl->setCurrentIndex(ctrl->findData(QStringLiteral("xum1541")));
        QCOMPARE(drive->count(), 4);
        QCOMPARE(drive->itemData(0).toInt(), 8);
        QCOMPARE(drive->itemData(3).toInt(), 11);
    }

    /* A flux capture or write job drives the same provider from a worker
     * thread; switching units underneath it would re-select between two
     * tracks. The combo is locked while such a job runs. */
    void drive_combo_locked_while_flux_job_runs() {
        HardwareTab tab;
        auto *drive = tab.findChild<QComboBox *>("comboDriveSelect");
        QVERIFY2(drive, "comboDriveSelect not found");
        QVERIFY(drive->isEnabled());
        const bool gibt_es = QMetaObject::invokeMethod(
            &tab, "setFluxJobRunning", Q_ARG(bool, true));
        QVERIFY2(gibt_es, "HardwareTab has no slot setFluxJobRunning(bool)");
        QVERIFY2(!drive->isEnabled(), "drive combo still enabled during a flux job");
        QMetaObject::invokeMethod(&tab, "setFluxJobRunning", Q_ARG(bool, false));
        QVERIFY2(drive->isEnabled(), "drive combo stays locked after the job");
    }
};

QTEST_MAIN(TestHardwareTabGui)
#include "test_hardware_tab_gui.moc"

/**
 * @file test_provider_switch.cpp
 * @brief Multi-device improvement test — provider-switch consistency (#110).
 *
 * Greaseweazle has a single fixed device model: it only ever talks to a
 * Greaseweazle. UFT's Hardware tab holds a `ProviderV2Variant` — any of
 * 9 unrelated V2 provider types — and the operator switches between
 * them by connect / disconnect / reconnect. This test proves that
 * switching is CLEAN: each provider's capability gating reflects ITS
 * type, with no state bleeding across a switch, and disconnect always
 * returns to a no-capability baseline.
 *
 * Why a C++ QtTest: this exercises the real HardwareTab + the codegen
 * wiring + the variant — same reasoning as test_hardware_tab_gui.cpp
 * (pytest-qt cannot reach C++ Qt widgets).
 *
 * Self-checking: the expected button state is derived from the
 * capability CONCEPT applied to the provider TYPE, never a hard-coded
 * table — so the test cannot drift out of sync with the providers.
 *
 * Covers the multi-device half of MF-205/MF-206 (variant routing) and
 * MF-210 (clean disconnect teardown — m_providerV2 = monostate,
 * rewireV2, the update*Enabled fixes).
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

/* The 7 capability-bound buttons (forms/tab_hardware.actions.yaml). */
static const char *const kCapabilityButtons[] = {
    "btnReadTest", "btnDetect", "btnSeekTest", "btnCalibrate",
    "btnRPMTest", "btnMotorOn", "btnMotorOff",
};

class TestProviderSwitch : public QObject {
    Q_OBJECT

    static QPushButton *btn(HardwareTab &tab, const char *name) {
        auto *b = tab.findChild<QPushButton *>(name);
        if (!b) qWarning("button missing: %s", name);
        return b;
    }

    /* Connect `key` through the real onConnect() path (Destination role
     * offers the controller superset). */
    static void connect_controller(HardwareTab &tab, const QString &key) {
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
        port->addItem("Test Port", "test_port");
        port->setCurrentIndex(0);

        auto *btnConnect = tab.findChild<QPushButton *>("btnConnect");
        QVERIFY2(btnConnect, "btnConnect not found");
        QVERIFY2(!tab.isConnected(), "expected disconnected before connect");
        btnConnect->click();                           /* -> onConnect() */
        QVERIFY2(tab.isConnected(),
                 qPrintable("onConnect did not connect: " + key));
    }

    /* Disconnect via the same toggle button. */
    static void disconnect_controller(HardwareTab &tab) {
        auto *btnConnect = tab.findChild<QPushButton *>("btnConnect");
        QVERIFY2(btnConnect, "btnConnect not found");
        QVERIFY2(tab.isConnected(), "expected connected before disconnect");
        btnConnect->click();                           /* -> onDisconnect() */
        QVERIFY2(!tab.isConnected(), "onDisconnect did not disconnect");
    }

    /* Assert the 7 capability buttons match the concept set for the
     * concrete provider type currently connected. */
    template <class ProviderT>
    void assert_caps(HardwareTab &tab, const QString &key) {
        QPushButton *bRead  = btn(tab, "btnReadTest");
        QPushButton *bDet   = btn(tab, "btnDetect");
        QPushButton *bSeek  = btn(tab, "btnSeekTest");
        QPushButton *bCal   = btn(tab, "btnCalibrate");
        QPushButton *bRpm   = btn(tab, "btnRPMTest");
        QPushButton *bMOn   = btn(tab, "btnMotorOn");
        QPushButton *bMOff  = btn(tab, "btnMotorOff");
        QVERIFY(bRead && bDet && bSeek && bCal && bRpm && bMOn && bMOff);

        const QByteArray ctx = key.toUtf8();
        QVERIFY2(bRead->isEnabled() == cap::ReadsRawFlux<ProviderT>,
                 ctx.constData());
        QVERIFY2(bDet->isEnabled()  == cap::DetectsDrive<ProviderT>,
                 ctx.constData());
        QVERIFY2(bSeek->isEnabled() == cap::SeeksHead<ProviderT>,
                 ctx.constData());
        QVERIFY2(bCal->isEnabled()  == cap::Recalibrates<ProviderT>,
                 ctx.constData());
        QVERIFY2(bRpm->isEnabled()  == cap::MeasuresRPM<ProviderT>,
                 ctx.constData());
        /* btnMotorOn = ControlsMotor && !running; freshly connected the
         * motor is not running. btnMotorOff = false until it runs. */
        QVERIFY2(bMOn->isEnabled()  == cap::ControlsMotor<ProviderT>,
                 ctx.constData());
        QVERIFY2(!bMOff->isEnabled(), ctx.constData());
    }

    /* Assert every capability button is disabled — the no-provider
     * (monostate) baseline. */
    void assert_no_capabilities(HardwareTab &tab, const char *when) {
        for (const char *name : kCapabilityButtons) {
            QPushButton *b = btn(tab, name);
            QVERIFY(b);
            QVERIFY2(!b->isEnabled(),
                     qPrintable(QString(name) + " still enabled " + when));
        }
    }

private slots:

    /* Switch across unrelated provider types in ONE session: each
     * provider's gating reflects its OWN type, and every disconnect
     * returns to the no-capability baseline — no bleed across switches. */
    void switching_across_provider_types_keeps_capabilities_consistent() {
        HardwareTab tab;
        assert_no_capabilities(tab, "at startup");

        connect_controller(tab, "scp");
        assert_caps<uft::hal::SCPProviderV2>(tab, "scp");
        disconnect_controller(tab);
        assert_no_capabilities(tab, "after scp disconnect");

        /* Applesauce satisfies ControlsMotor; SCP / FC5025 do not. The
         * switch must NOT leave a motor button enabled. */
        connect_controller(tab, "applesauce");
        assert_caps<uft::hal::ApplesauceProviderV2>(tab, "applesauce");
        disconnect_controller(tab);
        assert_no_capabilities(tab, "after applesauce disconnect");

        connect_controller(tab, "fc5025");
        assert_caps<uft::hal::FC5025ProviderV2>(tab, "fc5025");
        disconnect_controller(tab);
        assert_no_capabilities(tab, "after fc5025 disconnect");

        connect_controller(tab, "kryoflux");
        assert_caps<uft::hal::KryoFluxProviderV2>(tab, "kryoflux");
        disconnect_controller(tab);
        assert_no_capabilities(tab, "after kryoflux disconnect");
    }

    /* Disconnect always returns to the clean no-capability state. */
    void disconnect_returns_to_clean_no_capability_state() {
        HardwareTab tab;
        connect_controller(tab, "xum1541");
        QVERIFY(tab.isConnected());
        disconnect_controller(tab);
        QVERIFY(!tab.isConnected());
        assert_no_capabilities(tab, "after disconnect");
    }

    /* Reconnecting the SAME provider type twice yields identical
     * capability gating — the switch is idempotent, no accumulated drift. */
    void reconnecting_same_provider_is_idempotent() {
        HardwareTab tab;

        /* Pure (no QVERIFY — that expands to `return;` and cannot sit
         * in a value-returning lambda). A missing button reads false
         * in both snapshots; assert_caps in the other slots is what
         * guards button existence. */
        auto snapshot = [&]() -> QList<bool> {
            QList<bool> s;
            for (const char *name : kCapabilityButtons) {
                QPushButton *b = btn(tab, name);
                s.append(b && b->isEnabled());
            }
            return s;
        };

        connect_controller(tab, "adfcopy");
        const QList<bool> first = snapshot();
        disconnect_controller(tab);

        connect_controller(tab, "adfcopy");
        const QList<bool> second = snapshot();
        disconnect_controller(tab);

        QCOMPARE(second, first);
    }
};

QTEST_MAIN(TestProviderSwitch)
#include "test_provider_switch.moc"

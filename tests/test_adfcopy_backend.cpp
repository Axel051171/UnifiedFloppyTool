/*
 * test_adfcopy_backend.cpp - Unit tests for ADFCopyHardwareProvider
 *
 * Part of UnifiedFloppyTool (UFT)
 * Copyright (C) 2024-2026 Axel Kramer
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest/QtTest>
#include <QByteArray>
#include <QFile>
#include <QTemporaryFile>
#include <QSignalSpy>
#include <QSet>
#include <memory>

// Include the provider header — all protocol constants come from here
#include "hardware_providers/adfcopyhardwareprovider.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * Derived geometry constants (for cross-checking against the header)
 * ═══════════════════════════════════════════════════════════════════════════════ */
static constexpr int EXPECTED_DD_TRACK_BYTES = ADFC_DD_SECTORS * ADFC_SECTOR_SIZE;   // 5632
static constexpr int EXPECTED_DD_ADF_SIZE    = ADFC_DD_TRACKS  * EXPECTED_DD_TRACK_BYTES; // 901120
static constexpr int EXPECTED_HD_TRACK_BYTES = ADFC_HD_SECTORS * ADFC_SECTOR_SIZE;   // 11264
static constexpr int EXPECTED_HD_ADF_SIZE    = ADFC_DD_TRACKS  * EXPECTED_HD_TRACK_BYTES; // 1802240

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Class
 * ═══════════════════════════════════════════════════════════════════════════════ */

class TestAdfCopyBackend : public QObject
{
    Q_OBJECT

private slots:
    // --- Protocol constant tests ---
    void test_ddAdfSize();
    void test_hdAdfSize();
    void test_commandBytes();
    void test_usbIds();
    void test_responseTokens();
    void test_statusFlags();
    void test_timingParamIds();

    // --- Geometry / offset calculations ---
    void test_trackIndexCalc_dd();
    void test_adfOffsetCalc();

    // --- Provider interface tests ---
    void test_notOpenByDefault();
    void test_closeSafeWhenNotOpen();
    void test_displayName();
    void test_defaultGeometry();
    void test_defaultRPM();
    void test_detectPorts_noHardware();
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protocol Constant Tests
 *
 * These pin the firmware protocol interface values so that accidental
 * changes in the header are caught before they cause wire-level breakage.
 * ═══════════════════════════════════════════════════════════════════════════════ */

void TestAdfCopyBackend::test_ddAdfSize()
{
    // DD geometry: 80 cylinders x 2 heads = 160 tracks
    QCOMPARE(ADFC_DD_TRACKS, 160);

    // 11 sectors per track, 512 bytes per sector
    QCOMPARE(ADFC_DD_SECTORS, 11);
    QCOMPARE(ADFC_SECTOR_SIZE, 512);

    // Track size = 11 * 512 = 5632 bytes
    QCOMPARE(EXPECTED_DD_TRACK_BYTES, 5632);

    // Total ADF size = 160 * 5632 = 901120 bytes
    QCOMPARE(EXPECTED_DD_ADF_SIZE, 901120);

    // Cross-check
    QCOMPARE(ADFC_DD_SECTORS * ADFC_SECTOR_SIZE, EXPECTED_DD_TRACK_BYTES);
    QCOMPARE(ADFC_DD_TRACKS * EXPECTED_DD_TRACK_BYTES, EXPECTED_DD_ADF_SIZE);
}

void TestAdfCopyBackend::test_hdAdfSize()
{
    // HD uses same 160 tracks but 22 sectors per track
    QCOMPARE(ADFC_HD_SECTORS, 22);

    // HD track bytes = 22 * 512 = 11264
    QCOMPARE(EXPECTED_HD_TRACK_BYTES, 11264);

    // HD ADF size = 160 * 11264 = 1802240
    QCOMPARE(EXPECTED_HD_ADF_SIZE, 1802240);

    // HD is exactly 2x DD
    QCOMPARE(EXPECTED_HD_ADF_SIZE, EXPECTED_DD_ADF_SIZE * 2);
    QCOMPARE(ADFC_HD_SECTORS, ADFC_DD_SECTORS * 2);
}

void TestAdfCopyBackend::test_commandBytes()
{
    // Verify command byte values match the Niteto firmware protocol
    QCOMPARE(ADFC_CMD_PING,          static_cast<uint8_t>(0x00));
    QCOMPARE(ADFC_CMD_INIT,          static_cast<uint8_t>(0x01));
    QCOMPARE(ADFC_CMD_SEEK,          static_cast<uint8_t>(0x02));
    QCOMPARE(ADFC_CMD_GET_VOLNAME,   static_cast<uint8_t>(0x03));
    QCOMPARE(ADFC_CMD_READ_TRACK,    static_cast<uint8_t>(0x04));
    QCOMPARE(ADFC_CMD_WRITE_TRACK,   static_cast<uint8_t>(0x05));
    QCOMPARE(ADFC_CMD_READ_FLUX,     static_cast<uint8_t>(0x06));
    QCOMPARE(ADFC_CMD_READ_SCP,      static_cast<uint8_t>(0x07));
    QCOMPARE(ADFC_CMD_FORMAT_TRACK,  static_cast<uint8_t>(0x08));
    QCOMPARE(ADFC_CMD_COMPARE_TRACK, static_cast<uint8_t>(0x09));
    QCOMPARE(ADFC_CMD_DISK_INFO,     static_cast<uint8_t>(0x0A));
    QCOMPARE(ADFC_CMD_GET_STATUS,    static_cast<uint8_t>(0x0B));
    QCOMPARE(ADFC_CMD_SET_TIMING,    static_cast<uint8_t>(0x0C));
    QCOMPARE(ADFC_CMD_SAVE_SETTINGS, static_cast<uint8_t>(0x0D));
    QCOMPARE(ADFC_CMD_CLEANING_MODE, static_cast<uint8_t>(0x0E));

    // Ensure all commands are unique (no accidental duplicates)
    QSet<uint8_t> cmds;
    cmds << ADFC_CMD_PING << ADFC_CMD_INIT << ADFC_CMD_SEEK
         << ADFC_CMD_GET_VOLNAME << ADFC_CMD_READ_TRACK << ADFC_CMD_WRITE_TRACK
         << ADFC_CMD_READ_FLUX << ADFC_CMD_READ_SCP << ADFC_CMD_FORMAT_TRACK
         << ADFC_CMD_COMPARE_TRACK << ADFC_CMD_DISK_INFO << ADFC_CMD_GET_STATUS
         << ADFC_CMD_SET_TIMING << ADFC_CMD_SAVE_SETTINGS << ADFC_CMD_CLEANING_MODE;
    QCOMPARE(cmds.size(), 15);
}

void TestAdfCopyBackend::test_usbIds()
{
    // PJRC Teensy VID/PID used by ADF-Copy
    QCOMPARE(ADFC_USB_VID, static_cast<uint16_t>(0x16C0));
    QCOMPARE(ADFC_USB_PID, static_cast<uint16_t>(0x0483));

    // VID and PID must not be zero
    QVERIFY(ADFC_USB_VID != 0);
    QVERIFY(ADFC_USB_PID != 0);

    // VID and PID must be different from each other
    QVERIFY(ADFC_USB_VID != ADFC_USB_PID);
}

void TestAdfCopyBackend::test_responseTokens()
{
    QCOMPARE(ADFC_RSP_OK,        'O');
    QCOMPARE(ADFC_RSP_ERROR,     'E');
    QCOMPARE(ADFC_RSP_WEAKTRACK, 'W');
    QCOMPARE(ADFC_RSP_NDOS,      'N');
    QCOMPARE(ADFC_RSP_WPROT,     'P');
    QCOMPARE(ADFC_RSP_NODISK,    'D');

    // All response tokens must be unique
    QSet<char> tokens;
    tokens << ADFC_RSP_OK << ADFC_RSP_ERROR << ADFC_RSP_WEAKTRACK
           << ADFC_RSP_NDOS << ADFC_RSP_WPROT << ADFC_RSP_NODISK;
    QCOMPARE(tokens.size(), 6);

    // All tokens must be printable ASCII
    for (char c : tokens) {
        QVERIFY2(c >= 0x20 && c <= 0x7E,
                 qPrintable(QString("Non-printable token: 0x%1")
                            .arg(static_cast<uint8_t>(c), 2, 16, QLatin1Char('0'))));
    }
}

void TestAdfCopyBackend::test_statusFlags()
{
    const uint8_t flags[] = {
        ADFC_STATUS_DISK_PRESENT,
        ADFC_STATUS_WRITE_PROT,
        ADFC_STATUS_MOTOR_ON,
        ADFC_STATUS_FLUX_CAPABLE,
    };
    const int numFlags = sizeof(flags) / sizeof(flags[0]);

    // Verify expected bit positions
    QCOMPARE(ADFC_STATUS_DISK_PRESENT, static_cast<uint8_t>(1 << 0));
    QCOMPARE(ADFC_STATUS_WRITE_PROT,   static_cast<uint8_t>(1 << 1));
    QCOMPARE(ADFC_STATUS_MOTOR_ON,     static_cast<uint8_t>(1 << 2));
    QCOMPARE(ADFC_STATUS_FLUX_CAPABLE, static_cast<uint8_t>(1 << 3));

    // Each flag must be non-zero
    for (int i = 0; i < numFlags; ++i) {
        QVERIFY2(flags[i] != 0,
                 qPrintable(QString("Flag %1 is zero").arg(i)));
    }

    // Each flag must be a single bit (power of two)
    for (int i = 0; i < numFlags; ++i) {
        QVERIFY2((flags[i] & (flags[i] - 1)) == 0,
                 qPrintable(QString("Flag 0x%1 is not a single bit")
                            .arg(flags[i], 2, 16, QLatin1Char('0'))));
    }

    // No two flags may overlap
    for (int i = 0; i < numFlags; ++i) {
        for (int j = i + 1; j < numFlags; ++j) {
            QVERIFY2((flags[i] & flags[j]) == 0,
                     qPrintable(QString("Flags 0x%1 and 0x%2 overlap")
                                .arg(flags[i], 2, 16, QLatin1Char('0'))
                                .arg(flags[j], 2, 16, QLatin1Char('0'))));
        }
    }
}

void TestAdfCopyBackend::test_timingParamIds()
{
    const uint8_t ids[] = {
        ADFC_TIMING_STEP_DELAY,
        ADFC_TIMING_HEAD_SETTLE,
        ADFC_TIMING_MOTOR_ON,
        ADFC_TIMING_MFM_WINDOW,
    };
    const int numIds = sizeof(ids) / sizeof(ids[0]);

    // Verify expected values
    QCOMPARE(ADFC_TIMING_STEP_DELAY,  static_cast<uint8_t>(0x01));
    QCOMPARE(ADFC_TIMING_HEAD_SETTLE, static_cast<uint8_t>(0x02));
    QCOMPARE(ADFC_TIMING_MOTOR_ON,    static_cast<uint8_t>(0x03));
    QCOMPARE(ADFC_TIMING_MFM_WINDOW,  static_cast<uint8_t>(0x04));

    // Each timing parameter ID must be non-zero and unique
    QSet<uint8_t> idSet;
    for (int i = 0; i < numIds; ++i) {
        QVERIFY2(ids[i] != 0,
                 qPrintable(QString("Timing ID %1 is zero").arg(i)));
        idSet.insert(ids[i]);
    }
    QCOMPARE(idSet.size(), numIds);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Geometry / Offset Calculation Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

void TestAdfCopyBackend::test_trackIndexCalc_dd()
{
    // Amiga track index = cylinder * 2 + side
    QCOMPARE(0  * 2 + 0, 0);     // Cylinder 0, side 0 → track 0
    QCOMPARE(0  * 2 + 1, 1);     // Cylinder 0, side 1 → track 1
    QCOMPARE(1  * 2 + 0, 2);     // Cylinder 1, side 0 → track 2
    QCOMPARE(39 * 2 + 1, 79);    // Middle of disk
    QCOMPARE(40 * 2 + 0, 80);    // Root block area
    QCOMPARE(79 * 2 + 1, 159);   // Last track

    // Verify full range: every (cyl, side) pair maps to unique index in [0..159]
    QSet<int> trackIndices;
    for (int cyl = 0; cyl < 80; ++cyl) {
        for (int side = 0; side < 2; ++side) {
            int trackIndex = cyl * 2 + side;
            QVERIFY(trackIndex >= 0 && trackIndex < ADFC_DD_TRACKS);
            trackIndices.insert(trackIndex);
        }
    }
    QCOMPARE(trackIndices.size(), ADFC_DD_TRACKS);
}

void TestAdfCopyBackend::test_adfOffsetCalc()
{
    // File offset = track_index * DD_TRACK_BYTES
    QCOMPARE(0 * EXPECTED_DD_TRACK_BYTES, 0);
    QCOMPARE(1 * EXPECTED_DD_TRACK_BYTES, 5632);

    // Last DD track: offset + track_bytes == ADF_SIZE
    int ddLastOffset = (ADFC_DD_TRACKS - 1) * EXPECTED_DD_TRACK_BYTES;
    QCOMPARE(ddLastOffset + EXPECTED_DD_TRACK_BYTES, EXPECTED_DD_ADF_SIZE);

    // Last HD track: offset + track_bytes == HD ADF_SIZE
    int hdLastOffset = (ADFC_DD_TRACKS - 1) * EXPECTED_HD_TRACK_BYTES;
    QCOMPARE(hdLastOffset + EXPECTED_HD_TRACK_BYTES, EXPECTED_HD_ADF_SIZE);

    // Verify sequential DD offsets cover full file without gaps
    for (int t = 0; t < ADFC_DD_TRACKS; ++t) {
        int offset = t * EXPECTED_DD_TRACK_BYTES;
        QVERIFY(offset >= 0 && offset < EXPECTED_DD_ADF_SIZE);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Provider Interface Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

void TestAdfCopyBackend::test_notOpenByDefault()
{
    ADFCopyHardwareProvider provider;
    QVERIFY(!provider.isConnected());
}

void TestAdfCopyBackend::test_closeSafeWhenNotOpen()
{
    ADFCopyHardwareProvider provider;

    // disconnect() on a never-connected provider must not crash
    provider.disconnect();
    provider.disconnect();  // calling twice must also be safe
    QVERIFY(!provider.isConnected());
}

void TestAdfCopyBackend::test_displayName()
{
    ADFCopyHardwareProvider provider;
    QCOMPARE(provider.displayName(), QStringLiteral("ADF-Copy"));
}

void TestAdfCopyBackend::test_defaultGeometry()
{
    ADFCopyHardwareProvider provider;

    // getGeometry() should report 80 tracks, 2 heads
    int tracks = 0, heads = 0;
    bool ok = provider.getGeometry(tracks, heads);
    QVERIFY(ok);
    QCOMPARE(tracks, 80);
    QCOMPARE(heads, 2);
}

void TestAdfCopyBackend::test_defaultRPM()
{
    ADFCopyHardwareProvider provider;

    // Without hardware connected, measureRPM() returns 0.0
    QCOMPARE(provider.measureRPM(), 0.0);
}

void TestAdfCopyBackend::test_detectPorts_noHardware()
{
    ADFCopyHardwareProvider provider;

    // autoDetectDevice() must not crash or hang without actual hardware.
    // It should emit statusMessage but not crash.
    QSignalSpy spy(&provider, &HardwareProvider::statusMessage);
    QVERIFY(spy.isValid());

    provider.autoDetectDevice();

    // Should have emitted at least one status message
    QVERIFY(spy.count() >= 1);
}

QTEST_MAIN(TestAdfCopyBackend)
#include "test_adfcopy_backend.moc"

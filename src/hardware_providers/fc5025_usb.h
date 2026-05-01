/**
 * @file fc5025_usb.h
 * @brief FC5025 USB Protocol Constants and Structures
 *
 * Device Side Data FC5025 USB 5.25" Floppy Controller.
 * Protocol derived from the FC5025 Command Set Specification (v1309)
 * and the open-source fc5025 driver (kevinmarty/fc5025, mnaberez/fc5025).
 *
 * The FC5025 uses a SCSI-like Command Block Wrapper (CBW) / Command Status
 * Wrapper (CSW) protocol over USB bulk transfers.  The device is READ-ONLY;
 * it cannot write to floppy disks.
 *
 * When UFT_FC5025_SUPPORT is defined the implementation uses libusb-1.0
 * for direct USB communication.  Otherwise a stub is compiled that reports
 * the device as unavailable.
 */

#ifndef FC5025_USB_H
#define FC5025_USB_H

#include <cstdint>

namespace fc5025 {

/* ═══════════════════════════════════════════════════════════════════════════════
 * USB Identifiers
 *
 * The FC5025 uses the shared V-USB / VOTI VID.  The PID is specific to
 * Device Side Data.
 * ═══════════════════════════════════════════════════════════════════════════════ */

static constexpr uint16_t USB_VID          = 0x16C0;   /* Van Ooijen Technische Informatica */
static constexpr uint16_t USB_PID          = 0x06D6;   /* FC5025 floppy controller          */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Bulk Endpoints
 *
 * EP1 OUT  (0x01) — host-to-device: commands (CBW)
 * EP1 IN   (0x81) — device-to-host: data + status (CSW)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static constexpr uint8_t  EP_BULK_OUT      = 0x01;
static constexpr uint8_t  EP_BULK_IN       = 0x81;

/* Interface and configuration */
static constexpr int      USB_INTERFACE    = 0;
static constexpr int      USB_CONFIG       = 1;

/* Timeouts (milliseconds) */
static constexpr int      USB_TIMEOUT_CMD  = 2000;     /* for short commands           */
static constexpr int      USB_TIMEOUT_READ = 10000;    /* for track read (may be slow) */
static constexpr int      USB_TIMEOUT_OPEN = 5000;     /* for device enumeration       */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Command Block Wrapper (CBW)
 *
 * The CBW is a fixed 15-byte packet sent on EP_BULK_OUT.
 *
 * Offset  Length  Description
 * ------  ------  -----------
 *  0       4      Signature: 0x55 0x46 0x43 0x01 ("UFC\x01")
 *  4       1      Opcode  (see CmdOpcode below)
 *  5       1      Disk format code  (see DiskFormat below)
 *  6       1      Track number  (physical cylinder)
 *  7       1      Side  (0 or 1)
 *  8       1      Starting sector number
 *  9       1      Sector count  (0 = read all sectors on track)
 * 10       1      Flags  (see CmdFlags below)
 * 11       2      Transfer length (little-endian, expected data bytes)
 * 13       2      Timeout in ms  (little-endian, 0 = default)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static constexpr uint8_t  CBW_SIG_0        = 0x55;     /* 'U' */
static constexpr uint8_t  CBW_SIG_1        = 0x46;     /* 'F' */
static constexpr uint8_t  CBW_SIG_2        = 0x43;     /* 'C' */
static constexpr uint8_t  CBW_SIG_3        = 0x01;     /* version 1 */
static constexpr int      CBW_SIZE         = 15;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Command Status Wrapper (CSW)
 *
 * After the data phase the device returns a 2-byte CSW on EP_BULK_IN.
 *
 * Offset  Length  Description
 * ------  ------  -----------
 *  0       1      Status byte (see CswStatus below)
 *  1       1      Flags / additional info
 * ═══════════════════════════════════════════════════════════════════════════════ */

static constexpr int      CSW_SIZE         = 2;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Command Opcodes  (CBW byte 4)
 * ═══════════════════════════════════════════════════════════════════════════════ */

enum CmdOpcode : uint8_t {
    CMD_READ_ID         = 0x12,     /* Read sector ID field                      */
    CMD_SEEK            = 0x40,     /* Seek to cylinder                          */
    CMD_MOTOR_ON        = 0x41,     /* Spin up drive motor                       */
    CMD_MOTOR_OFF       = 0x42,     /* Spin down drive motor                     */
    CMD_SELECT_DENSITY  = 0x43,     /* Select FM / MFM density                   */
    CMD_RECALIBRATE     = 0x44,     /* Seek to track 0                           */
    CMD_GET_STATUS      = 0x50,     /* Query drive status / index                */
    CMD_GET_VERSION     = 0x51,     /* Query firmware version                    */
    CMD_FLAGS           = 0x80,     /* Set bitcell / timing flags                */
    CMD_READ_FLEXIBLE   = 0xC0,     /* Read sectors (flexible format)            */
    /* 0xC0..0xCF are READ_FLEXIBLE variants; low nibble = sub-format */
    CMD_READ_TRACK_RAW  = 0xD0,     /* Read raw (unformatted) track data         */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Command Flags  (CBW byte 10)
 * ═══════════════════════════════════════════════════════════════════════════════ */

enum CmdFlags : uint8_t {
    FLAG_NONE           = 0x00,
    FLAG_DOUBLE_STEP    = 0x01,     /* Step 2 cylinders per logical track        */
    FLAG_SIDE1          = 0x02,     /* Select side 1 (head 1)                    */
    FLAG_BITCELL_16US   = 0x00,     /* FM 16 us bit cell (default FM)            */
    FLAG_BITCELL_8US    = 0x10,     /* MFM 8 us bit cell (default MFM)           */
    FLAG_BITCELL_4US    = 0x20,     /* HD 4 us bit cell                          */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * CSW Status Codes  (CSW byte 0)
 * ═══════════════════════════════════════════════════════════════════════════════ */

enum CswStatus : uint8_t {
    CSW_OK              = 0x00,     /* Command completed successfully            */
    CSW_SEEK_ERROR      = 0x02,     /* Head seek failed                          */
    CSW_NO_DATA         = 0x04,     /* Sector not found / no data                */
    CSW_WRITE_PROTECT   = 0x08,     /* Disk is write-protected                   */
    CSW_CRC_ERROR       = 0x10,     /* CRC mismatch on sector read               */
    CSW_DELETED_DATA    = 0x20,     /* Read a deleted-data mark                  */
    CSW_NO_INDEX        = 0x40,     /* No index pulse detected (no disk?)        */
    CSW_NOT_READY       = 0x80,     /* Drive not ready / not present             */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk Format Codes  (CBW byte 5)
 *
 * These identify the on-disk encoding so the FC5025 firmware selects the
 * correct MFM/FM/GCR decoder.
 * ═══════════════════════════════════════════════════════════════════════════════ */

enum DiskFormat : uint8_t {
    FMT_APPLE_DOS32     = 0x01,     /* Apple II DOS 3.2  (13 sectors, GCR 5+3)  */
    FMT_APPLE_DOS33     = 0x02,     /* Apple II DOS 3.3  (16 sectors, GCR 6+2)  */
    FMT_APPLE_PRODOS    = 0x03,     /* Apple ProDOS      (16 sectors, GCR 6+2)  */
    FMT_C1541           = 0x04,     /* Commodore 1541    (GCR)                   */
    FMT_IBM_FM_250K     = 0x10,     /* IBM 3740 FM 250 kbps (8" SSSD 128 b/s)   */
    FMT_IBM_MFM_500K    = 0x11,     /* IBM MFM 500 kbps  (8" DSDD or 5.25" HD)  */
    FMT_IBM_MFM_250K    = 0x12,     /* IBM MFM 250 kbps  (5.25" DD)             */
    FMT_TRS80_SSSD      = 0x20,     /* TRS-80 Model I/III SSSD (10 sect, FM)    */
    FMT_TRS80_SSDD      = 0x21,     /* TRS-80 Model III/4 SSDD (18 sect, MFM)   */
    FMT_TRS80_DSDD      = 0x22,     /* TRS-80 Model 4 DSDD   (18 sect, MFM)     */
    FMT_KAYPRO_II       = 0x30,     /* Kaypro II / IV  (10 sect, 512 b, MFM)    */
    FMT_OSBORNE_I       = 0x31,     /* Osborne 1       (10 sect, 256 b, FM)     */
    FMT_NORTHSTAR       = 0x40,     /* North Star DOS  (10 sect, 256 b, FM)     */
    FMT_TI994A          = 0x50,     /* TI-99/4A        (9 sect, 256 b, FM)      */
    FMT_ATARI_810       = 0x60,     /* Atari 810 FM    (18 sect, 128 b)         */
    FMT_ATARI_1050      = 0x61,     /* Atari 1050 MFM  (26 sect, 128 b)         */
    FMT_RAW_FM          = 0xF0,     /* Raw FM capture  (uninterpreted bitstream) */
    FMT_RAW_MFM         = 0xF1,     /* Raw MFM capture (uninterpreted bitstream) */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sector / Track Geometry
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct FormatInfo {
    DiskFormat  code;
    const char *name;
    int         tracks;
    int         sectors;
    int         sectorSize;     /* bytes per sector    */
    bool        isFm;           /* true = FM, false = MFM/GCR */
    int         sides;          /* typical number of sides */
};

/* Lookup table — matches the FC5025 Command Set Spec v1309 */
static constexpr FormatInfo FORMATS[] = {
    { FMT_APPLE_DOS32,  "Apple DOS 3.2",        35, 13, 256, false, 1 },
    { FMT_APPLE_DOS33,  "Apple DOS 3.3",        35, 16, 256, false, 1 },
    { FMT_APPLE_PRODOS, "Apple ProDOS",          35, 16, 256, false, 1 },
    { FMT_C1541,        "Commodore 1541",        35, 21, 256, false, 1 },
    { FMT_IBM_FM_250K,  "IBM 3740 FM (8\" SSSD)",77, 26, 128, true,  1 },
    { FMT_IBM_MFM_500K, "IBM MFM 500 kbps",      77, 26, 256, false, 2 },
    { FMT_IBM_MFM_250K, "IBM MFM 250 kbps",      40, 18, 256, false, 2 },
    { FMT_TRS80_SSSD,   "TRS-80 SSSD",           35, 10, 256, true,  1 },
    { FMT_TRS80_SSDD,   "TRS-80 SSDD",           40, 18, 256, false, 1 },
    { FMT_TRS80_DSDD,   "TRS-80 DSDD",           40, 18, 256, false, 2 },
    { FMT_KAYPRO_II,    "Kaypro II/IV",           40, 10, 512, false, 1 },
    { FMT_OSBORNE_I,    "Osborne 1",              40, 10, 256, true,  1 },
    { FMT_NORTHSTAR,    "North Star",              35, 10, 256, true,  1 },
    { FMT_TI994A,       "TI-99/4A",               40,  9, 256, true,  1 },
    { FMT_ATARI_810,    "Atari 810 FM",            40, 18, 128, true,  1 },
    { FMT_ATARI_1050,   "Atari 1050 MFM",          40, 26, 128, false, 1 },
    { FMT_RAW_FM,       "Raw FM",                   0,  0,   0, true,  1 },
    { FMT_RAW_MFM,      "Raw MFM",                  0,  0,   0, false, 1 },
};

static constexpr int FORMAT_COUNT = sizeof(FORMATS) / sizeof(FORMATS[0]);

/**
 * @brief Look up format info by code.
 * @return Pointer to static FormatInfo, or nullptr if not found.
 */
inline const FormatInfo *lookupFormat(DiskFormat code) {
    for (int i = 0; i < FORMAT_COUNT; ++i) {
        if (FORMATS[i].code == code)
            return &FORMATS[i];
    }
    return nullptr;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * CBW Builder
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Build a Command Block Wrapper in the caller's buffer.
 *
 * @param buf          Output buffer (must be >= CBW_SIZE bytes).
 * @param opcode       Command opcode (CmdOpcode).
 * @param format       Disk format code (DiskFormat).
 * @param track        Physical track (cylinder) number.
 * @param side         Side (0 or 1).
 * @param sectorStart  First sector to read.
 * @param sectorCount  Number of sectors (0 = entire track).
 * @param flags        Flags byte (CmdFlags, may be ORed together).
 * @param xferLen      Expected data transfer length in bytes.
 * @param timeoutMs    Command timeout in milliseconds (0 = default).
 */
inline void buildCBW(uint8_t *buf,
                     uint8_t opcode,
                     uint8_t format,
                     uint8_t track,
                     uint8_t side,
                     uint8_t sectorStart,
                     uint8_t sectorCount,
                     uint8_t flags,
                     uint16_t xferLen,
                     uint16_t timeoutMs)
{
    buf[0]  = CBW_SIG_0;
    buf[1]  = CBW_SIG_1;
    buf[2]  = CBW_SIG_2;
    buf[3]  = CBW_SIG_3;
    buf[4]  = opcode;
    buf[5]  = format;
    buf[6]  = track;
    buf[7]  = side;
    buf[8]  = sectorStart;
    buf[9]  = sectorCount;
    buf[10] = flags;
    buf[11] = static_cast<uint8_t>(xferLen & 0xFF);         /* LE low  */
    buf[12] = static_cast<uint8_t>((xferLen >> 8) & 0xFF);  /* LE high */
    buf[13] = static_cast<uint8_t>(timeoutMs & 0xFF);
    buf[14] = static_cast<uint8_t>((timeoutMs >> 8) & 0xFF);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Device Handle
 *
 * When UFT_FC5025_SUPPORT is defined the handle wraps a libusb_device_handle.
 * Otherwise it is an opaque stub so the rest of the code compiles.
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct DeviceHandle {
    void    *usbHandle   = nullptr;  /* libusb_device_handle* (or nullptr)       */
    bool     connected   = false;
    uint16_t fwVersion   = 0;        /* firmware version from CMD_GET_VERSION    */
    int      maxTrack    = 0;        /* discovered track count                   */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Convenience API  (thin wrappers compiled in fc5025hardwareprovider.cpp)
 *
 * When UFT_FC5025_SUPPORT is NOT defined every function is a no-op stub.
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_FC5025_SUPPORT

bool    init();
void    cleanup();
bool    open(DeviceHandle &h);
void    close(DeviceHandle &h);
bool    isConnected(const DeviceHandle &h);

int     sendCBW(const DeviceHandle &h, const uint8_t *cbw, int cbwLen);
int     recvData(const DeviceHandle &h, uint8_t *buf, int len, int timeoutMs);
int     recvCSW(const DeviceHandle &h, uint8_t *csw);

bool    seekTrack(const DeviceHandle &h, uint8_t track, uint8_t flags = 0);
bool    motorOn(const DeviceHandle &h);
bool    motorOff(const DeviceHandle &h);
bool    recalibrate(const DeviceHandle &h);
bool    getFirmwareVersion(const DeviceHandle &h, uint16_t &version);
uint8_t getDriveStatus(const DeviceHandle &h);

#else /* UFT_FC5025_SUPPORT not defined — stubs */

inline bool    init()                                    { return false; }
inline void    cleanup()                                 {}
inline bool    open(DeviceHandle &)                      { return false; }
inline void    close(DeviceHandle &)                     {}
inline bool    isConnected(const DeviceHandle &h)        { return h.connected; }

inline int     sendCBW(const DeviceHandle &, const uint8_t *, int)  { return -1; }
inline int     recvData(const DeviceHandle &, uint8_t *, int, int)  { return -1; }
inline int     recvCSW(const DeviceHandle &, uint8_t *)             { return -1; }

inline bool    seekTrack(const DeviceHandle &, uint8_t, uint8_t = 0) { return false; }
inline bool    motorOn(const DeviceHandle &)              { return false; }
inline bool    motorOff(const DeviceHandle &)             { return false; }
inline bool    recalibrate(const DeviceHandle &)          { return false; }
inline bool    getFirmwareVersion(const DeviceHandle &, uint16_t &) { return false; }
inline uint8_t getDriveStatus(const DeviceHandle &)       { return 0xFF; }

#endif /* UFT_FC5025_SUPPORT */

} // namespace fc5025

#endif // FC5025_USB_H

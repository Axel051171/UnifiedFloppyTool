/**
 * @file uft_controller_transport.h
 * @brief How each hardware controller is addressed — SSOT (MF-412).
 *
 * Background: GitHub issue #34. A user with an FC5025 could not connect. The
 * FC5025 is a USB device driven through the Device Side Data `fcimage` CLI and
 * its own libusb-based driver; on Windows it shows up as a `libusb-win32`
 * device and never as a COM port. `HardwareTab::onConnect()` nevertheless
 * required a serial port for EVERY controller:
 *
 *     if (port.isEmpty()) { warn("Please select a valid port."); return; }
 *
 * With no COM port present the port combo held "(No ports found)" and Connect
 * was disabled; with an unrelated COM1 present the status line claimed
 * "Connecting to FC5025 on COM1" — a statement about something the FC5025 path
 * never touches. Five of the nine controllers do not read the port field at
 * all, which is why this table exists instead of a second ad-hoc `if`.
 *
 * The classification is derived from what the connect code actually does with
 * the port string, verified call site by call site in src/hardwaretab.cpp:
 *
 *   greaseweazle  gwp->open(port)                              -> SERIAL
 *   applesauce    QSerialPortApplesauceTransport::open(port)   -> SERIAL
 *   adfcopy       QSerialPortADFCopyTransport::open(port)      -> SERIAL
 *   usb_floppy    make_usbfloppy_detect_runner(port)           -> DEVICE_PATH
 *   scp           libusb, device located by VID/PID            -> USB_DIRECT
 *   xum1541       libusb / OpenCBM, located by VID/PID         -> USB_DIRECT
 *   kryoflux      QProcess `dtc`                               -> CLI
 *   fluxengine    QProcess `fluxengine`                        -> CLI
 *   fc5025        QProcess `fcimage`                           -> CLI
 *
 * Greaseweazle is USB too, but it enumerates as a USB-CDC serial port, so a
 * port name is genuinely what its open() takes. "USB" alone does not decide
 * this — what the code passes does.
 */

#ifndef UFT_HAL_UFT_CONTROLLER_TRANSPORT_H
#define UFT_HAL_UFT_CONTROLLER_TRANSPORT_H

#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/** How a controller is addressed by the connect path. */
typedef enum uft_ctrl_transport {
    UFT_CTRL_TRANSPORT_UNKNOWN = 0,  /**< key not in the table */
    UFT_CTRL_TRANSPORT_SERIAL,       /**< needs a serial port name */
    UFT_CTRL_TRANSPORT_DEVICE_PATH,  /**< needs an OS device path */
    UFT_CTRL_TRANSPORT_USB_DIRECT,   /**< libusb; found by VID/PID */
    UFT_CTRL_TRANSPORT_CLI           /**< external command-line tool */
} uft_ctrl_transport_t;

/**
 * @brief Transport for a controller key from the GUI combo box
 * @param key controller key, e.g. "fc5025"; NULL yields UNKNOWN
 */
static inline uft_ctrl_transport_t
uft_controller_transport(const char *key)
{
    if (!key) return UFT_CTRL_TRANSPORT_UNKNOWN;

    if (strcmp(key, "greaseweazle") == 0 ||
        strcmp(key, "applesauce")   == 0 ||
        strcmp(key, "adfcopy")      == 0)
        return UFT_CTRL_TRANSPORT_SERIAL;

    if (strcmp(key, "usb_floppy") == 0)
        return UFT_CTRL_TRANSPORT_DEVICE_PATH;

    if (strcmp(key, "scp")      == 0 ||
        strcmp(key, "xum1541")  == 0)
        return UFT_CTRL_TRANSPORT_USB_DIRECT;

    if (strcmp(key, "kryoflux")   == 0 ||
        strcmp(key, "fluxengine") == 0 ||
        strcmp(key, "fc5025")     == 0)
        return UFT_CTRL_TRANSPORT_CLI;

    return UFT_CTRL_TRANSPORT_UNKNOWN;
}

/**
 * @brief Does the connect path read the port field for this controller?
 *
 * UNKNOWN returns false on purpose: an unrecognised key must not be blocked
 * behind a port requirement that nothing would use. A wrong key is a
 * separate error, surfaced where the provider is constructed.
 */
static inline bool uft_controller_needs_port(const char *key)
{
    uft_ctrl_transport_t t = uft_controller_transport(key);
    return t == UFT_CTRL_TRANSPORT_SERIAL ||
           t == UFT_CTRL_TRANSPORT_DEVICE_PATH;
}

/**
 * @brief Short, user-facing reason why no port is needed (NULL if one is)
 *
 * Used to fill the port combo with something truthful instead of leaving an
 * unrelated COM port selected.
 */
static inline const char *uft_controller_no_port_reason(const char *key)
{
    switch (uft_controller_transport(key)) {
    case UFT_CTRL_TRANSPORT_USB_DIRECT:
        return "USB device, located by VID/PID - no port needed";
    case UFT_CTRL_TRANSPORT_CLI:
        return "driven by an external tool - no port needed";
    default:
        return NULL;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_HAL_UFT_CONTROLLER_TRANSPORT_H */

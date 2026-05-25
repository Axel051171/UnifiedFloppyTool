/**
 * @file tests/usb_mock/libusb_mock.h
 * @brief Tier-2.5 USB-mock — scripted libusb-1.0 replacement (MF-270).
 *
 * Allows C-HAL files (uft_scp_direct.c, uft_xum1541.c, ...) to be
 * compiled into a TEST executable that exercises the full libusb call
 * sequence WITHOUT touching real hardware.
 *
 * USAGE
 * =====
 *
 * In the test's CMakeLists block (or as compiler args):
 *   add_definitions(-DUFT_HAS_LIBUSB -DUFT_USB_MOCK=1)
 *   target_include_directories(... PRIVATE
 *       ${CMAKE_SOURCE_DIR}/tests/usb_mock)
 *
 * Then the test source includes this header BEFORE the C-HAL is
 * compiled (CMake's target_sources puts both into one TU). The macros
 * here redirect every `libusb_*` call to the corresponding
 * `uft_mock_libusb_*` function, which reads from a scripted-exchange
 * queue that the test populates in its setup phase.
 *
 * NO PRODUCTION CODE CHANGES. The C-HAL file is byte-identical between
 * the production build and the mock-test build.
 *
 * SCRIPTING MODEL
 * ===============
 *
 * A test pre-queues a sequence of expected USB exchanges:
 *
 *   usb_mock_reset();
 *   usb_mock_queue_open(0x16D0, 0x0F8C, 1);        // open succeeds
 *   usb_mock_queue_bulk_out(EP_OUT, tx, tx_len, 0); // 0 = LIBUSB_SUCCESS
 *   usb_mock_queue_bulk_in(EP_IN, rx, rx_len, 0);
 *   usb_mock_queue_close();
 *
 * Then the test calls the C-HAL function (uft_scp_direct_open, ...).
 * Each libusb_* call consumes the next queued exchange and asserts
 * its arguments match. A mismatch fails the test loudly.
 *
 * FORENSIC INVARIANT
 * ==================
 *
 * The mock never fabricates bytes. Every byte the C-HAL receives via
 * libusb_bulk_transfer comes verbatim from a usb_mock_queue_bulk_in()
 * call in the test. If the test did not script a reply, the C-HAL
 * sees LIBUSB_ERROR_TIMEOUT.
 */
#ifndef UFT_USB_MOCK_LIBUSB_MOCK_H
#define UFT_USB_MOCK_LIBUSB_MOCK_H

/* Force the real <libusb-1.0/libusb.h> to skip its body — our shim
 * is the ONLY libusb definition in this TU. The real header guards
 * itself with `#ifndef LIBUSB_H`, so claiming that guard first turns
 * any later #include of the real header into a no-op. */
#ifndef LIBUSB_H
#define LIBUSB_H
#endif

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>   /* ssize_t */

/* The mock provides its own definitions of libusb's enum constants so
 * production C-HAL #ifs continue to compile. We avoid #include <libusb.h>
 * entirely in the test build — even if libusb headers are present on
 * the system, the production C-HAL must compile against our shim
 * exclusively. */

/* libusb_context + libusb_device_handle are opaque to production code.
 * The mock makes them sentinel-pointer types so equality checks work. */
struct libusb_context        { int _id; };
struct libusb_device_handle  { int _id; };
struct libusb_device         { int _id; };
typedef struct libusb_context        libusb_context;
typedef struct libusb_device_handle  libusb_device_handle;
typedef struct libusb_device         libusb_device;

/* Minimal libusb_device_descriptor — only the VID/PID fields the
 * XUM1541 detect path reads. The full real struct has 16+ fields;
 * leaving them undefined keeps the test surface tiny. */
struct libusb_device_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
};

/* Error codes — must match libusb-1.0's enum values where the C-HAL
 * compares against them. */
enum {
    LIBUSB_SUCCESS               = 0,
    LIBUSB_ERROR_IO              = -1,
    LIBUSB_ERROR_INVALID_PARAM   = -2,
    LIBUSB_ERROR_ACCESS          = -3,
    LIBUSB_ERROR_NO_DEVICE       = -4,
    LIBUSB_ERROR_NOT_FOUND       = -5,
    LIBUSB_ERROR_BUSY            = -6,
    LIBUSB_ERROR_TIMEOUT         = -7,
    LIBUSB_ERROR_OVERFLOW        = -8,
    LIBUSB_ERROR_PIPE            = -9,
    LIBUSB_ERROR_NOT_SUPPORTED   = -12,
    LIBUSB_ERROR_OTHER           = -99,
};

/* ─────────────────────────────────────────────────────────────────────
 *  Mock API — test code calls these to script the expected USB
 *  exchange, then drives the C-HAL.
 * ───────────────────────────────────────────────────────────────────── */

#ifdef __cplusplus
extern "C" {
#endif

/** Clear all queued exchanges + recorded transfers. Call at the start
 *  of every test to start fresh. */
void usb_mock_reset(void);

/** Pre-script: libusb_open_device_with_vid_pid(vid, pid) succeeds.
 *  If `open_succeeds` is false, returns NULL → C-HAL takes the
 *  "device not found" branch. */
void usb_mock_queue_open(uint16_t vid, uint16_t pid, int open_succeeds);

/** Pre-script: libusb_bulk_transfer with direction OUT (host→device).
 *  The mock asserts the actual bytes the C-HAL sends match
 *  `expected_tx[expected_len]` (length + content). If `ack_status` is
 *  the LIBUSB_SUCCESS value the transfer succeeds; otherwise the
 *  C-HAL gets the supplied error code. */
void usb_mock_queue_bulk_out(uint8_t endpoint,
                             const uint8_t *expected_tx,
                             size_t expected_len,
                             int ack_status);

/** Pre-script: libusb_bulk_transfer with direction IN (device→host).
 *  The C-HAL receives `scripted_rx[scripted_len]` verbatim. If
 *  `ack_status != LIBUSB_SUCCESS` the C-HAL gets that error and an
 *  unfilled buffer. */
void usb_mock_queue_bulk_in(uint8_t endpoint,
                            const uint8_t *scripted_rx,
                            size_t scripted_len,
                            int ack_status);

/** Pre-script: libusb_close() expected. (close is unconditional in the
 *  C-HALs, so this is mainly for completeness — the mock verifies it
 *  happens once per open.) */
void usb_mock_queue_close(void);

/** Diagnostic: how many exchanges remain unconsumed. The test should
 *  assert this is 0 at the end (otherwise the C-HAL invoked fewer USB
 *  calls than scripted). */
size_t usb_mock_remaining_exchanges(void);

/** Diagnostic: total bulk_transfer calls observed (in + out). */
size_t usb_mock_total_transfers(void);

/* ─────────────────────────────────────────────────────────────────────
 *  Production-side macros — the test_*.c compilation unit includes
 *  this header BEFORE compiling the C-HAL, so every libusb_* call in
 *  the C-HAL is rewritten to the mock variant below.
 * ───────────────────────────────────────────────────────────────────── */

int  uft_mock_libusb_init(libusb_context **ctx);
void uft_mock_libusb_exit(libusb_context *ctx);
libusb_device_handle *uft_mock_libusb_open_device_with_vid_pid(
    libusb_context *ctx, uint16_t vid, uint16_t pid);
int  uft_mock_libusb_set_auto_detach_kernel_driver(
    libusb_device_handle *dev, int enable);
int  uft_mock_libusb_claim_interface(libusb_device_handle *dev,
                                      int interface);
int  uft_mock_libusb_release_interface(libusb_device_handle *dev,
                                        int interface);
void uft_mock_libusb_close(libusb_device_handle *dev);
int  uft_mock_libusb_bulk_transfer(libusb_device_handle *dev,
                                    uint8_t endpoint,
                                    unsigned char *data,
                                    int length,
                                    int *transferred,
                                    unsigned int timeout_ms);
int  uft_mock_libusb_control_transfer(libusb_device_handle *dev,
                                       uint8_t bmRequestType,
                                       uint8_t bRequest,
                                       uint16_t wValue,
                                       uint16_t wIndex,
                                       unsigned char *data,
                                       uint16_t wLength,
                                       unsigned int timeout_ms);

#define libusb_init                          uft_mock_libusb_init
#define libusb_exit                          uft_mock_libusb_exit
#define libusb_open_device_with_vid_pid      uft_mock_libusb_open_device_with_vid_pid
#define libusb_set_auto_detach_kernel_driver uft_mock_libusb_set_auto_detach_kernel_driver
#define libusb_claim_interface               uft_mock_libusb_claim_interface
#define libusb_release_interface             uft_mock_libusb_release_interface
#define libusb_close                         uft_mock_libusb_close
#define libusb_bulk_transfer                 uft_mock_libusb_bulk_transfer
#define libusb_control_transfer              uft_mock_libusb_control_transfer

/* Detect-path stubs — XUM1541 detect uses these. The mock returns
 * "no devices found" by default (n=0), which surfaces to the C-HAL as
 * UFT_OK with device_count=0 — an honest, no-fabrication answer. */
ssize_t uft_mock_libusb_get_device_list(libusb_context *ctx,
                                         libusb_device ***list);
void    uft_mock_libusb_free_device_list(libusb_device **list, int unref);
int     uft_mock_libusb_get_device_descriptor(libusb_device *dev,
                                       struct libusb_device_descriptor *desc);
#define libusb_get_device_list        uft_mock_libusb_get_device_list
#define libusb_free_device_list       uft_mock_libusb_free_device_list
#define libusb_get_device_descriptor  uft_mock_libusb_get_device_descriptor

/* XUM1541-specific: script the data buffer returned from the INIT
 * control transfer. Same queue but device-in direction. */
void usb_mock_queue_control_in(uint8_t bmRequestType,
                                uint8_t bRequest,
                                const uint8_t *scripted_data,
                                size_t scripted_len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_USB_MOCK_LIBUSB_MOCK_H */

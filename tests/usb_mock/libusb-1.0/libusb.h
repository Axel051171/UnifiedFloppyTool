/**
 * @file tests/usb_mock/libusb-1.0/libusb.h
 * @brief Mock stand-in for the system <libusb-1.0/libusb.h> (CI-1).
 *
 * The USB-mock tests (test_scp_direct_usb_mock, test_xum1541_usb_mock)
 * compile the production HAL with UFT_HAS_LIBUSB=1, which makes
 * src/hal/uft_scp_direct.c do `#include <libusb-1.0/libusb.h>`. The mock
 * header (libusb_mock.h) claims the real header's `LIBUSB_H` include
 * guard so that include contributes nothing — BUT the preprocessor still
 * has to physically find the file. On hosts with libusb-dev installed
 * (the Linux CI runner) the real header exists; on the Windows CI runner
 * it does not, which broke the whole test build (KNOWN_ISSUES CI-1).
 *
 * This stub sits on the mock tests' include path (`-I tests/usb_mock`)
 * so `<libusb-1.0/libusb.h>` resolves on every platform. It pulls in the
 * scripted mock, which provides all libusb-1.0 symbols the HAL uses.
 * Only the two usb_mock targets add tests/usb_mock to their includes, so
 * this never shadows the real libusb for any other target.
 */
#ifndef UFT_USB_MOCK_LIBUSB_STUB_H
#define UFT_USB_MOCK_LIBUSB_STUB_H

#include "../libusb_mock.h"

#endif /* UFT_USB_MOCK_LIBUSB_STUB_H */

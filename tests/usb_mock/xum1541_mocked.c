/**
 * @file tests/usb_mock/xum1541_mocked.c
 * @brief Wrapper TU compiling uft_xum1541.c with the libusb mock in scope.
 * See scp_direct_mocked.c header for the rationale.
 */
#include "libusb_mock.h"
#include "../../src/hal/uft_xum1541.c"

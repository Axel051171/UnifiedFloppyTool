/**
 * @file tests/usb_mock/scp_direct_mocked.c
 * @brief Wrapper TU that compiles uft_scp_direct.c with the libusb mock
 *        in scope (MF-270).
 *
 * Including the C-HAL inline like this — instead of via
 * set_source_files_properties — keeps the mock injection local to the
 * mock-test target. Other tests that link uft_scp_direct.c
 * (test_scp_direct_hal, test_hal_conformance) compile it with real
 * libusb headers as before.
 */
#include "libusb_mock.h"
#include "../../src/hal/uft_scp_direct.c"

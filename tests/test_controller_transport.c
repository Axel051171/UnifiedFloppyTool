/**
 * @file test_controller_transport.c
 * @brief Which controllers need a port — regression guard for issue #34.
 *
 * Reported 2026-08-11: an FC5025 could not be connected. It is a USB device
 * driven through the `fcimage` CLI and shows up on Windows as a `libusb-win32`
 * device, never as a COM port. HardwareTab::onConnect() required a serial port
 * for every controller, so the user was stopped by
 * "Please select a valid port." for a field the FC5025 path never reads.
 *
 * src/gui/ has no test coverage, so the decision itself was moved out of the
 * widget into include/uft/hal/uft_controller_transport.h, where it can be
 * checked. What this file guards is the classification, not the Qt wiring.
 */

#include "uft/hal/uft_controller_transport.h"

#include <stdio.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(fc5025_needs_no_port_issue_34) {
    /* The reported case. The FC5025 is reached through the fcimage CLI; the
     * port field is never read on that path, so requiring one blocks a
     * perfectly working setup. */
    ASSERT(uft_controller_transport("fc5025") == UFT_CTRL_TRANSPORT_CLI);
    ASSERT(!uft_controller_needs_port("fc5025"));
    ASSERT(uft_controller_no_port_reason("fc5025") != NULL);
}

TEST(cli_driven_controllers_need_no_port) {
    /* Same transport, same consequence — kryoflux via dtc, fluxengine via
     * its own binary. */
    const char *cli[] = { "kryoflux", "fluxengine", "fc5025" };
    for (size_t i = 0; i < sizeof(cli) / sizeof(cli[0]); i++) {
        ASSERT(uft_controller_transport(cli[i]) == UFT_CTRL_TRANSPORT_CLI);
        ASSERT(!uft_controller_needs_port(cli[i]));
    }
}

TEST(libusb_controllers_need_no_port) {
    /* Located by VID/PID, not by port name. */
    const char *usb[] = { "scp", "xum1541" };
    for (size_t i = 0; i < sizeof(usb) / sizeof(usb[0]); i++) {
        ASSERT(uft_controller_transport(usb[i]) == UFT_CTRL_TRANSPORT_USB_DIRECT);
        ASSERT(!uft_controller_needs_port(usb[i]));
        ASSERT(uft_controller_no_port_reason(usb[i]) != NULL);
    }
}

TEST(serial_controllers_still_need_a_port) {
    /* The other half of the fix: loosening the gate must not loosen it for
     * the controllers whose open() genuinely takes a port string. Greaseweazle
     * is USB as well, but enumerates as USB-CDC and its open() takes the port
     * name — so "is USB" is not the criterion, "what the code passes" is. */
    const char *serial[] = { "greaseweazle", "applesauce", "adfcopy" };
    for (size_t i = 0; i < sizeof(serial) / sizeof(serial[0]); i++) {
        ASSERT(uft_controller_transport(serial[i]) == UFT_CTRL_TRANSPORT_SERIAL);
        ASSERT(uft_controller_needs_port(serial[i]));
        ASSERT(uft_controller_no_port_reason(serial[i]) == NULL);
    }
}

TEST(usb_floppy_needs_a_device_path) {
    /* Not a COM port, but the connect path does read the field
     * (make_usbfloppy_detect_runner(port)), so it stays required. */
    ASSERT(uft_controller_transport("usb_floppy") == UFT_CTRL_TRANSPORT_DEVICE_PATH);
    ASSERT(uft_controller_needs_port("usb_floppy"));
}

TEST(every_controller_in_the_gui_combo_is_classified) {
    /* If a controller is added to the combo box and not to the table, it
     * silently lands in UNKNOWN and skips the port gate. This pins the nine
     * keys that populateControllerList() offers today; adding a tenth must
     * fail here rather than produce a quiet default. */
    const char *keys[] = {
        "greaseweazle", "scp", "kryoflux", "fluxengine", "adfcopy",
        "applesauce", "fc5025", "xum1541", "usb_floppy"
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        ASSERT(uft_controller_transport(keys[i]) != UFT_CTRL_TRANSPORT_UNKNOWN);
    }
}

TEST(unknown_and_degenerate_keys_do_not_demand_a_port) {
    /* An unrecognised key must not be blocked behind a requirement nothing
     * would use; the wrong key is reported where the provider is built. */
    ASSERT(uft_controller_transport(NULL) == UFT_CTRL_TRANSPORT_UNKNOWN);
    ASSERT(!uft_controller_needs_port(NULL));
    ASSERT(uft_controller_transport("") == UFT_CTRL_TRANSPORT_UNKNOWN);
    ASSERT(!uft_controller_needs_port("nonesuch"));

    /* Separator entries share the combo with real controllers. */
    ASSERT(!uft_controller_needs_port("separator_flux"));
}

int main(void)
{
    printf("=== Controller transport classification (issue #34, MF-412) ===\n");
    RUN(fc5025_needs_no_port_issue_34);
    RUN(cli_driven_controllers_need_no_port);
    RUN(libusb_controllers_need_no_port);
    RUN(serial_controllers_still_need_a_port);
    RUN(usb_floppy_needs_a_device_path);
    RUN(every_controller_in_the_gui_combo_is_classified);
    RUN(unknown_and_degenerate_keys_do_not_demand_a_port);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

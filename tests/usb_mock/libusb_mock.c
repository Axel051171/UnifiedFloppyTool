/**
 * @file tests/usb_mock/libusb_mock.c
 * @brief Tier-2.5 USB-mock implementation (MF-270).
 *
 * Scripted libusb-1.0 replacement. Test code pre-queues a sequence of
 * exchanges; each libusb_* call from the C-HAL consumes the next one
 * and validates argv matches. Header is libusb_mock.h.
 */

#include "libusb_mock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Sentinel handles ────────────────────────────────────────────────
 *
 * The mock returns these pointer addresses as the "context" and
 * "device" handles. Production code only treats them as opaque, so
 * the actual sentinel values don't matter — they just need to be
 * distinct, non-NULL, and stable. */
static libusb_context        g_mock_ctx_value    = { .  _id = 0x11 };
static libusb_device_handle  g_mock_dev_value    = { .  _id = 0x22 };

/* ─── Scripted-exchange queue ──────────────────────────────────────── */

typedef enum {
    EXCH_OPEN,         /* libusb_open_device_with_vid_pid */
    EXCH_BULK_OUT,     /* libusb_bulk_transfer, host→device */
    EXCH_BULK_IN,      /* libusb_bulk_transfer, device→host */
    EXCH_CTRL_IN,      /* libusb_control_transfer, device→host */
    EXCH_CLOSE,        /* libusb_close */
} exchange_kind_t;

#define MAX_EXCHANGE_PAYLOAD 4096
#define MAX_QUEUED_EXCHANGES  64

typedef struct {
    exchange_kind_t kind;
    uint16_t  vid;
    uint16_t  pid;
    int       open_succeeds;
    uint8_t   endpoint;
    uint8_t   bmRequestType;
    uint8_t   bRequest;
    uint8_t   payload[MAX_EXCHANGE_PAYLOAD];
    size_t    payload_len;
    int       ack_status;
} scripted_exchange_t;

static scripted_exchange_t g_queue[MAX_QUEUED_EXCHANGES];
static size_t              g_queue_head  = 0;
static size_t              g_queue_count = 0;
static size_t              g_total_transfers = 0;

/* ─── Helpers ──────────────────────────────────────────────────────── */

static scripted_exchange_t *pop_exchange(exchange_kind_t expected_kind,
                                          const char *callsite)
{
    if (g_queue_count == 0) {
        fprintf(stderr,
                "[usb_mock] FAIL: %s called but no scripted exchange queued\n",
                callsite);
        return NULL;
    }
    scripted_exchange_t *e = &g_queue[g_queue_head];
    if (e->kind != expected_kind) {
        fprintf(stderr,
                "[usb_mock] FAIL: %s expected kind=%d but next queued is %d\n",
                callsite, (int)expected_kind, (int)e->kind);
        return NULL;
    }
    g_queue_head  = (g_queue_head + 1) % MAX_QUEUED_EXCHANGES;
    g_queue_count--;
    return e;
}

static scripted_exchange_t *queue_slot(void)
{
    if (g_queue_count >= MAX_QUEUED_EXCHANGES) {
        fprintf(stderr,
                "[usb_mock] FATAL: queue full (%d) — raise MAX_QUEUED_EXCHANGES\n",
                MAX_QUEUED_EXCHANGES);
        abort();
    }
    size_t idx = (g_queue_head + g_queue_count) % MAX_QUEUED_EXCHANGES;
    g_queue_count++;
    scripted_exchange_t *e = &g_queue[idx];
    memset(e, 0, sizeof(*e));
    return e;
}

/* ─── Public API ──────────────────────────────────────────────────── */

void usb_mock_reset(void)
{
    g_queue_head  = 0;
    g_queue_count = 0;
    g_total_transfers = 0;
}

void usb_mock_queue_open(uint16_t vid, uint16_t pid, int open_succeeds)
{
    scripted_exchange_t *e = queue_slot();
    e->kind = EXCH_OPEN;
    e->vid  = vid;
    e->pid  = pid;
    e->open_succeeds = open_succeeds;
}

void usb_mock_queue_bulk_out(uint8_t endpoint,
                              const uint8_t *expected_tx,
                              size_t expected_len,
                              int ack_status)
{
    if (expected_len > MAX_EXCHANGE_PAYLOAD) {
        fprintf(stderr,
                "[usb_mock] FATAL: bulk_out payload %zu > %d\n",
                expected_len, MAX_EXCHANGE_PAYLOAD);
        abort();
    }
    scripted_exchange_t *e = queue_slot();
    e->kind        = EXCH_BULK_OUT;
    e->endpoint    = endpoint;
    e->payload_len = expected_len;
    if (expected_tx && expected_len) {
        memcpy(e->payload, expected_tx, expected_len);
    }
    e->ack_status = ack_status;
}

void usb_mock_queue_bulk_in(uint8_t endpoint,
                             const uint8_t *scripted_rx,
                             size_t scripted_len,
                             int ack_status)
{
    if (scripted_len > MAX_EXCHANGE_PAYLOAD) {
        fprintf(stderr,
                "[usb_mock] FATAL: bulk_in payload %zu > %d\n",
                scripted_len, MAX_EXCHANGE_PAYLOAD);
        abort();
    }
    scripted_exchange_t *e = queue_slot();
    e->kind        = EXCH_BULK_IN;
    e->endpoint    = endpoint;
    e->payload_len = scripted_len;
    if (scripted_rx && scripted_len) {
        memcpy(e->payload, scripted_rx, scripted_len);
    }
    e->ack_status = ack_status;
}

void usb_mock_queue_close(void)
{
    scripted_exchange_t *e = queue_slot();
    e->kind = EXCH_CLOSE;
}

void usb_mock_queue_control_in(uint8_t bmRequestType,
                                uint8_t bRequest,
                                const uint8_t *scripted_data,
                                size_t scripted_len)
{
    if (scripted_len > MAX_EXCHANGE_PAYLOAD) {
        fprintf(stderr,
                "[usb_mock] FATAL: control_in payload %zu > %d\n",
                scripted_len, MAX_EXCHANGE_PAYLOAD);
        abort();
    }
    scripted_exchange_t *e = queue_slot();
    e->kind = EXCH_CTRL_IN;
    e->bmRequestType = bmRequestType;
    e->bRequest      = bRequest;
    e->payload_len   = scripted_len;
    if (scripted_data && scripted_len) {
        memcpy(e->payload, scripted_data, scripted_len);
    }
    e->ack_status = LIBUSB_SUCCESS;
}

size_t usb_mock_remaining_exchanges(void) { return g_queue_count; }
size_t usb_mock_total_transfers(void)     { return g_total_transfers; }

/* ─── libusb function shims ──────────────────────────────────────── */

int uft_mock_libusb_init(libusb_context **ctx)
{
    if (ctx) *ctx = &g_mock_ctx_value;
    return LIBUSB_SUCCESS;
}

void uft_mock_libusb_exit(libusb_context *ctx)
{
    (void)ctx;
    /* No queued exchange consumed — libusb_exit is unconditional. */
}

libusb_device_handle *
uft_mock_libusb_open_device_with_vid_pid(libusb_context *ctx,
                                          uint16_t vid, uint16_t pid)
{
    (void)ctx;
    scripted_exchange_t *e = pop_exchange(EXCH_OPEN, "open_device_with_vid_pid");
    if (!e) return NULL;
    if (e->vid != vid || e->pid != pid) {
        fprintf(stderr,
                "[usb_mock] FAIL: open vid/pid mismatch — "
                "queued %04x:%04x, called %04x:%04x\n",
                e->vid, e->pid, vid, pid);
        return NULL;
    }
    return e->open_succeeds ? &g_mock_dev_value : NULL;
}

int uft_mock_libusb_set_auto_detach_kernel_driver(libusb_device_handle *dev,
                                                    int enable)
{
    (void)dev; (void)enable;
    /* Always-succeed shim — production code ignores return anyway. */
    return LIBUSB_SUCCESS;
}

int uft_mock_libusb_claim_interface(libusb_device_handle *dev, int interface)
{
    (void)dev; (void)interface;
    return LIBUSB_SUCCESS;
}

int uft_mock_libusb_release_interface(libusb_device_handle *dev, int interface)
{
    (void)dev; (void)interface;
    return LIBUSB_SUCCESS;
}

void uft_mock_libusb_close(libusb_device_handle *dev)
{
    (void)dev;
    /* Pop one EXCH_CLOSE if queued, but don't fail if none — close()
     * is sometimes called from cleanup paths that the test didn't
     * explicitly script. */
    if (g_queue_count > 0 && g_queue[g_queue_head].kind == EXCH_CLOSE) {
        (void)pop_exchange(EXCH_CLOSE, "close");
    }
}

int uft_mock_libusb_bulk_transfer(libusb_device_handle *dev,
                                    uint8_t endpoint,
                                    unsigned char *data,
                                    int length,
                                    int *transferred,
                                    unsigned int timeout_ms)
{
    (void)dev; (void)timeout_ms;
    g_total_transfers++;

    /* Direction = endpoint bit 7 (1 = IN, 0 = OUT). */
    int is_in = (endpoint & 0x80) ? 1 : 0;
    exchange_kind_t expected = is_in ? EXCH_BULK_IN : EXCH_BULK_OUT;

    scripted_exchange_t *e = pop_exchange(
        expected, is_in ? "bulk_transfer IN" : "bulk_transfer OUT");
    if (!e) {
        if (transferred) *transferred = 0;
        return LIBUSB_ERROR_TIMEOUT;
    }
    if (e->endpoint != endpoint) {
        fprintf(stderr,
                "[usb_mock] FAIL: bulk_transfer endpoint mismatch — "
                "queued 0x%02x, called 0x%02x\n",
                e->endpoint, endpoint);
        if (transferred) *transferred = 0;
        return LIBUSB_ERROR_IO;
    }
    if (e->ack_status != LIBUSB_SUCCESS) {
        if (transferred) *transferred = 0;
        return e->ack_status;
    }

    if (is_in) {
        /* Device→host: write the scripted bytes into the caller's buffer. */
        size_t to_copy = (e->payload_len < (size_t)length)
                          ? e->payload_len : (size_t)length;
        if (data && to_copy) {
            memcpy(data, e->payload, to_copy);
        }
        if (transferred) *transferred = (int)to_copy;
    } else {
        /* Host→device: verify the bytes the C-HAL sent match the
         * scripted expectation. */
        if ((size_t)length != e->payload_len) {
            fprintf(stderr,
                    "[usb_mock] FAIL: bulk_out length mismatch — "
                    "queued %zu, actual %d\n",
                    e->payload_len, length);
            if (transferred) *transferred = 0;
            return LIBUSB_ERROR_IO;
        }
        if (data && e->payload_len &&
            memcmp(data, e->payload, e->payload_len) != 0) {
            fprintf(stderr,
                    "[usb_mock] FAIL: bulk_out byte mismatch on ep=0x%02x:\n",
                    endpoint);
            fprintf(stderr, "  expected: ");
            for (size_t i = 0; i < e->payload_len && i < 16; i++) {
                fprintf(stderr, "%02x ", e->payload[i]);
            }
            fprintf(stderr, "\n  actual:   ");
            for (size_t i = 0; i < (size_t)length && i < 16; i++) {
                fprintf(stderr, "%02x ", data[i]);
            }
            fprintf(stderr, "\n");
            if (transferred) *transferred = 0;
            return LIBUSB_ERROR_IO;
        }
        if (transferred) *transferred = length;
    }
    return LIBUSB_SUCCESS;
}

ssize_t uft_mock_libusb_get_device_list(libusb_context *ctx,
                                         libusb_device ***list)
{
    (void)ctx;
    /* Honest: report no devices. The detect-path C-HAL caller then
     * reports device_count = 0 — neither fabrication nor stubbed lie. */
    if (list) *list = NULL;
    return 0;
}

void uft_mock_libusb_free_device_list(libusb_device **list, int unref)
{
    (void)list; (void)unref;
}

int uft_mock_libusb_get_device_descriptor(libusb_device *dev,
                                            struct libusb_device_descriptor *desc)
{
    (void)dev;
    if (desc) {
        memset(desc, 0, sizeof(*desc));
    }
    return LIBUSB_SUCCESS;
}

int uft_mock_libusb_control_transfer(libusb_device_handle *dev,
                                       uint8_t bmRequestType,
                                       uint8_t bRequest,
                                       uint16_t wValue,
                                       uint16_t wIndex,
                                       unsigned char *data,
                                       uint16_t wLength,
                                       unsigned int timeout_ms)
{
    (void)dev; (void)wValue; (void)wIndex; (void)timeout_ms;
    g_total_transfers++;

    scripted_exchange_t *e = pop_exchange(EXCH_CTRL_IN, "control_transfer IN");
    if (!e) return LIBUSB_ERROR_TIMEOUT;
    if (e->bmRequestType != bmRequestType || e->bRequest != bRequest) {
        fprintf(stderr,
                "[usb_mock] FAIL: ctrl_in argv mismatch — "
                "queued bmRT=0x%02x bReq=0x%02x, called bmRT=0x%02x bReq=0x%02x\n",
                e->bmRequestType, e->bRequest,
                bmRequestType, bRequest);
        return LIBUSB_ERROR_IO;
    }
    size_t to_copy = (e->payload_len < (size_t)wLength)
                      ? e->payload_len : (size_t)wLength;
    if (data && to_copy) {
        memcpy(data, e->payload, to_copy);
    }
    return (int)to_copy;  /* control_transfer returns bytes transferred */
}

#include "uft/hal/ufi.h"
#include <string.h>

struct uft_ufi_device { int dummy; };

static const uft_ufi_ops_t *g_ops = NULL;

void uft_ufi_set_backend(const uft_ufi_ops_t *ops){ g_ops = ops; }

static uft_rc_t ensure_backend(uft_diag_t *diag)
{
    if (!g_ops || !g_ops->open || !g_ops->close || !g_ops->exec_cdb) {
        uft_diag_set(diag, "ufi: backend not set (NOT_IMPLEMENTED)");
        return UFT_ENOT_IMPLEMENTED;
    }
    return UFT_OK;
}

uft_rc_t uft_ufi_inquiry(const char *path, char vendor[9], char product[17], char rev[5], uft_diag_t *diag)
{
    uft_rc_t rc = ensure_backend(diag);
    if (rc != UFT_OK) return rc;

    uint8_t cdb[6] = { (uint8_t)UFT_UFI_INQUIRY, 0, 0, 0, 36, 0 };
    uint8_t buf[36]; memset(buf, 0, sizeof(buf));

    uft_ufi_device_t *dev = NULL;
    rc = g_ops->open(&dev, path, diag); if (rc != UFT_OK) return rc;

    rc = g_ops->exec_cdb(dev, cdb, sizeof(cdb), buf, sizeof(buf), +1, 2000, diag);
    g_ops->close(dev);
    if (rc != UFT_OK) return rc;

    /* Standard INQUIRY: vendor 8, product 16, rev 4 at offsets 8,16,32 */
    memcpy(vendor,  buf + 8, 8); vendor[8]='\0';
    memcpy(product, buf +16,16); product[16]='\0';
    memcpy(rev,     buf +32, 4); rev[4]='\0';

    uft_diag_set(diag, "ufi: inquiry ok");
    return UFT_OK;
}

uft_rc_t uft_ufi_verify_lba(const char *path, uint32_t lba, uint16_t blocks, uint32_t timeout_ms, uft_diag_t *diag)
{
    uft_rc_t rc = ensure_backend(diag);
    if (rc != UFT_OK) return rc;

    uint8_t cdb[10] = {0};
    cdb[0] = (uint8_t)UFT_UFI_VERIFY_10;
    cdb[2] = (uint8_t)((lba >> 24) & 0xFF);
    cdb[3] = (uint8_t)((lba >> 16) & 0xFF);
    cdb[4] = (uint8_t)((lba >>  8) & 0xFF);
    cdb[5] = (uint8_t)((lba >>  0) & 0xFF);
    cdb[7] = (uint8_t)((blocks >> 8) & 0xFF);
    cdb[8] = (uint8_t)((blocks >> 0) & 0xFF);

    uft_ufi_device_t *dev = NULL;
    rc = g_ops->open(&dev, path, diag); if (rc != UFT_OK) return rc;
    rc = g_ops->exec_cdb(dev, cdb, sizeof(cdb), NULL, 0, 0, timeout_ms, diag);
    g_ops->close(dev);
    if (rc != UFT_OK) return rc;

    uft_diag_set(diag, "ufi: verify ok");
    return UFT_OK;
}

uft_rc_t uft_ufi_format_floppy(const char *path, uint16_t cyl, uint8_t heads, uint8_t spt, uint16_t bps, uft_diag_t *diag)
{
    uft_rc_t rc = ensure_backend(diag);
    if (rc != UFT_OK) return rc;

    /* FORMAT UNIT in UFI typically uses parameter list; details vary by device.
       We provide a conservative NOT_IMPLEMENTED placeholder to avoid fake success. */
    (void)path; (void)cyl; (void)heads; (void)spt; (void)bps;
    uft_diag_set(diag, "ufi: format_floppy NOT_IMPLEMENTED (device-specific parameter list)");
    return UFT_ENOT_IMPLEMENTED;
}

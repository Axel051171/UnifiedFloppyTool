#include <uft/cbm/uft_crt.h>
#include <string.h>

static uint16_t rd_be16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] << 8) | (uint16_t)p[1]; }
static uint32_t rd_be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3]; }

static int memeq(const uint8_t *a, const char *b, size_t n) {
    for (size_t i=0;i<n;i++) if (a[i] != (uint8_t)b[i]) return 0;
    return 1;
}

uft_crt_status_t uft_crt_parse(const uint8_t *blob, size_t blob_len, uft_crt_view_t *out) {
    if (!out) return UFT_CRT_E_INVALID;
    memset(out, 0, sizeof(*out));
    if (!blob) return UFT_CRT_E_INVALID;
    if (blob_len < 0x40) return UFT_CRT_E_TRUNC;

    if (!memeq(blob, UFT_CRT_MAGIC, UFT_CRT_MAGIC_LEN)) return UFT_CRT_E_MAGIC;

    uft_crt_header_t h;
    memset(&h, 0, sizeof(h));
    memcpy(h.magic, blob, UFT_CRT_MAGIC_LEN);
    h.header_len = rd_be32(blob + 0x10);
    h.version    = rd_be16(blob + 0x14);
    h.hw_type    = rd_be16(blob + 0x16);
    h.exrom      = blob[0x18];
    h.game       = blob[0x19];
    memcpy(h.reserved,  blob + 0x1A, 6);
    memcpy(h.name,      blob + 0x20, 32);
    /* we keep a conservative fixed view; header may be >0x40 */
    memcpy(h.reserved2, blob + 0x40 - 32, 32);

    if (h.header_len < 0x40) return UFT_CRT_E_HEADER;
    if (h.header_len > blob_len) return UFT_CRT_E_TRUNC;

    out->blob = blob;
    out->blob_len = blob_len;
    out->hdr = h;
    out->chip_off = (size_t)h.header_len;
    return UFT_CRT_OK;
}

uft_crt_status_t uft_crt_next_chip(const uft_crt_view_t *crt, size_t *cursor, uft_crt_chip_view_t *chip_out) {
    if (!crt || !cursor || !chip_out) return UFT_CRT_E_INVALID;
    memset(chip_out, 0, sizeof(*chip_out));

    size_t off = *cursor;
    if (off == crt->blob_len) return UFT_CRT_E_TRUNC; /* done */
    if (off > crt->blob_len) return UFT_CRT_E_TRUNC;
    if (crt->blob_len - off < 0x10) return UFT_CRT_E_TRUNC;

    const uint8_t *p = crt->blob + off;
    if (!memeq(p, UFT_CRT_CHIP_MAGIC, UFT_CRT_CHIP_MAGIC_LEN)) return UFT_CRT_E_CHIP;

    uft_crt_chip_header_t ch;
    memset(&ch, 0, sizeof(ch));
    memcpy(ch.magic, p, 4);
    ch.packet_len = rd_be32(p + 0x04);
    ch.chip_type  = rd_be16(p + 0x08);
    ch.bank       = rd_be16(p + 0x0A);
    ch.load_addr  = rd_be16(p + 0x0C);
    ch.rom_len    = rd_be16(p + 0x0E);

    if (ch.packet_len < 0x10) return UFT_CRT_E_CHIP;
    if (off + (size_t)ch.packet_len > crt->blob_len) return UFT_CRT_E_TRUNC;
    if ((size_t)ch.rom_len > (size_t)ch.packet_len - 0x10) return UFT_CRT_E_CHIP;

    chip_out->chip_hdr = ch;
    chip_out->packet_off = off;
    chip_out->packet_len = (size_t)ch.packet_len;
    chip_out->data = p + 0x10;
    chip_out->data_len = (size_t)ch.rom_len;

    *cursor = off + (size_t)ch.packet_len;
    return UFT_CRT_OK;
}

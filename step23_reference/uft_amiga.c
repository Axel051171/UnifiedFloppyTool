#include "uft/uft_amiga.h"
#include "uft/uft_endian.h"

#include <string.h>

static inline uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |
           ((uint32_t)p[3]      );
}

uint32_t uft_amiga_bootblock_checksum(const uint8_t *boot1024)
{
    if (!boot1024) return 0;

    uint32_t sum = 0;

    for (int i = 0; i < 256; ++i) {
        const uint8_t *p = boot1024 + (i * 4);
        uint32_t v = be32(p);
        /* checksum field is long #1 (offset 4) */
        if (i == 1) v = 0;
        sum += v;
    }

    return ~sum;
}

int uft_amiga_block_checksum_check(const uint8_t *block512, size_t size, uft_amiga_block_checksum_info_t *out)
{
    if (!block512 || !out || size < 512) return 0;

    uint32_t sum = 0;
    for (size_t i = 0; i < 512; i += 4) {
        sum += uft_be32_read(block512 + i);
    }
    out->sum = sum;
    out->checksum_ok = (sum == 0) ? 1 : 0;
    return 1;
}

int uft_amiga_bootblock_parse(const uint8_t *buf, size_t size, uft_amiga_bootblock_info_t *out)
{
    if (!buf || !out || size < 1024) return 0;
    memset(out, 0, sizeof(*out));

    out->has_dos_magic = (buf[0] == 'D' && buf[1] == 'O' && buf[2] == 'S') ? 1 : 0;
    out->dos_type = -1;
    if (out->has_dos_magic) {
        const uint8_t t = buf[3];
        if (t <= 3) out->dos_type = (int)t;
    }

    out->stored_checksum = be32(buf + 4);
    out->computed_checksum = uft_amiga_bootblock_checksum(buf);
    out->checksum_ok = (out->stored_checksum == out->computed_checksum) ? 1 : 0;
    return 1;
}

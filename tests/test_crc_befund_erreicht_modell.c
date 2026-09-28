/* A CRC error a container records reaches the model as a CRC error — not
 * as "unchecked", indistinguishable from a good sector (P3-659, MF-1488).
 *
 * The disk2 bridge (src/core/uft_disk2_bridge.c) reads a sector's CRC
 * verdict from `status` only (UFT_SECTOR_CRC_ERROR / _ID_CRC_ERROR /
 * _CRC_CHECKED) and deliberately ignores `crc_ok`, because memset-built
 * sectors carry crc_ok=false without any finding (MF-1001). But
 * uft_sector_set_crc(s, false) sets only crc_ok/crc_valid/data_crc_ok —
 * never `status`. Measured: 11 of 14 producers of set_crc(false) leave
 * status at UFT_SECTOR_OK, so their CRC-bad sector came out of the bridge
 * as data_crc_known=0 / UNVERIFIED: DecodeJob counted it "unchecked", the
 * report "crc_unknown", FEAT_BAD_CRC never set. Proven end to end at an
 * IMD sector of type 6 by the P3-659 measurement.
 *
 * A central fix (set_crc(false) sets CRC_ERROR) was measured and REJECTED:
 * producers use set_crc(false) loosely — D64 for every 1541 error code
 * (02 "header not found", 04 "data not found" are no CRC errors), NFD for
 * a truncated 0xE5 fill and for ST1 bit 5 (an ID CRC). A central bit would
 * turn each of those into an invented "data CRC bad". So each producer
 * gets the meaning its own source gives, through two helpers in
 * uft_types.h. This test covers the three whose meaning is exact:
 *
 *   IMD   types 5..8 = "data read with data error" — Dave Dunfield,
 *         ImageDisk 1.20 IMD.SRC, sector records 05..08
 *   D88   status 0xB0 = bad data CRC, 0xA0 = bad address CRC — MAME
 *         d88_dsk.cpp (`bad_data_crc = hs[8] == 0xb0`, `bad_addr_crc =
 *         hs[8] == 0xa0`, "according to hxc")
 *   EDSK  ST2 bit 5 (DD) = data CRC, ST1 bit 5 (DE) without DD = ID CRC —
 *         NEC uPD765 status registers, as the plugin's own comment states
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_core.h"
#include "uft/uft_types.h"
#include "uft/core/uft_disk2.h"
#include "uft/core/uft_disk2_bridge.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_dsk_cpc;
extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_nfd;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

static const char *tmpdir(void)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    return d;
}

static int schreibe(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    int ok = fwrite(b, 1, n, f) == n;
    fclose(f);
    return ok;
}

/* Run the file through uft_disk_open -> bridge; report the model's verdict
 * for sector R of C0/H0: 'B' data CRC bad, 'I' ID CRC bad, 'U' unknown,
 * 'G' known good, '-' absent. */
static void modell(uft_disk_t *disk, const uft_format_plugin_t *p,
                   const uint8_t *rs, char *out, int n)
{
    uft_disk2_t *d = uft_d2_create();
    uft_d2_bridge_stats_t st;
    uft_d2_from_disk(d, disk, p, &st);
    const uft_d2_track_t *dt = uft_d2_track_get(d, 0, 0);
    for (size_t i = 0; dt && i < dt->sectors.count; i++) {
        const uft_d2_sector_t *s = &dt->sectors.items[i];
        for (int k = 0; k < n; k++) {
            if (s->id_sec != rs[k]) continue;
            if (!s->has_data) out[k] = 'M';
            else if (s->id_crc_known && !s->id_crc_ok) out[k] = 'I';
            else if (s->data_crc_known && !s->data_crc_ok) out[k] = 'B';
            else if (s->data_crc_known) out[k] = 'G';
            else out[k] = 'U';
        }
    }
    uft_d2_destroy(d);
}

/* EDSK only: `uft_disk_open()` opens NO extended CPC file — `dsk_cpc` and
 * `edsk` both claim "EXTENDED" at 95 and tie, and a tie opens nothing
 * (P3-402, pinned in test_oeffentliche_api_am_korpus). The producer under
 * test here is `dsk_cpc`, so it is opened by that plugin directly. */
static void urteile_mit(const uft_format_plugin_t *p, const char *pfad,
                        const uint8_t *rs, char *out, int n)
{
    for (int i = 0; i < n; i++) out[i] = '-';
    out[n] = 0;
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) {
        for (int i = 0; i < n; i++) out[i] = '!';
        return;
    }
    modell(&disk, p, rs, out, n);
    p->close(&disk);
}

static void urteile(const char *pfad, const uint8_t *rs, char *out, int n)
{
    for (int i = 0; i < n; i++) out[i] = '-';
    out[n] = 0;
    struct uft_probe_ranking rk;
    memset(&rk, 0, sizeof rk);
    uft_disk_t *disk = uft_disk_open_ranked(pfad, true, &rk);
    if (!disk) {
        /* say WHY instead of a bare NULL: winner, tie, claimants */
        printf("      %s: nicht geoeffnet — Sieger %s (%d), gleichauf %zu, Anspruchsteller %zu\n",
               pfad, rk.winner ? rk.winner->name : "(keiner)", rk.confidence,
               rk.tied, rk.claimants);
        for (int i = 0; i < n; i++) out[i] = '!';
        return;
    }
    modell(disk, uft_disk_plugin(disk), rs, out, n);
    uft_disk_close(disk);
}

int main(void)
{
    char pfad[600], urteil[8], h[160];
    const uint8_t r123[3] = { 1, 2, 3 };
    printf("CRC-Befund erreicht das Modell (MF-1488)\n");
    if (uft_register_all_formats() != UFT_OK) { printf("Registrierung scheiterte\n"); return 1; }

    /* IMD: R1 type 6 (compressed, data error), R2 type 2 (compressed, ok) */
    {
        static const uint8_t trk[] = { 0x05, 0x00, 0x00, 0x02, 0x02, 0x01, 0x02,
                                       0x06, 0xAA, 0x02, 0x55 };
        uint8_t b[128];
        const char kopf[] = "IMD 1.18: 01/01/2026 00:00:00\r\nMF-1488";
        size_t n = sizeof kopf - 1;
        memcpy(b, kopf, n); b[n++] = 0x1A;
        memcpy(b + n, trk, sizeof trk); n += sizeof trk;
        snprintf(pfad, sizeof pfad, "%s/uft_mf1488.imd", tmpdir());
        schreibe(pfad, b, n);
        urteile(pfad, r123, urteil, 2);
        remove(pfad);
        snprintf(h, sizeof h, "Modell R1,R2 = %s (B=Daten-CRC schlecht, U=ungeprueft)", urteil);
        pruefe("IMD Typ 6 (Dunfield: data error) kommt als Daten-CRC-Fehler an, Typ 2 bleibt ungeprueft",
               urteil[0] == 'B' && urteil[1] == 'U', h);
    }

    /* D88 2D: R1 status 0x00, R2 0xB0 (data CRC), R3 0xA0 (address CRC) */
    {
        static uint8_t b[0x2B0 + 3 * (16 + 256)];
        memset(b, 0, sizeof b);
        memcpy(b, "MF1488", 6);
        size_t pos = 0x2B0;
        b[0x20] = (uint8_t)pos; b[0x21] = (uint8_t)(pos >> 8);
        const uint8_t st[3] = { 0x00, 0xB0, 0xA0 };
        for (int i = 0; i < 3; i++) {
            uint8_t *s = b + pos;
            s[0] = 0; s[1] = 0; s[2] = (uint8_t)(i + 1); s[3] = 1;
            s[4] = 3; s[8] = st[i]; s[14] = 0x00; s[15] = 0x01;
            pos += 16;
            memset(b + pos, 0x10 + i, 256);
            pos += 256;
        }
        b[0x1C] = (uint8_t)pos; b[0x1D] = (uint8_t)(pos >> 8);
        snprintf(pfad, sizeof pfad, "%s/uft_mf1488.d88", tmpdir());
        schreibe(pfad, b, pos);
        urteile(pfad, r123, urteil, 3);
        remove(pfad);
        snprintf(h, sizeof h, "Modell R1,R2,R3 = %s (I=ID-CRC schlecht)", urteil);
        pruefe("D88 0xB0 -> Daten-CRC-Fehler, 0xA0 -> ID-CRC-Fehler, 0x00 ungeprueft (MAME)",
               urteil[0] == 'U' && urteil[1] == 'B' && urteil[2] == 'I', h);
    }

    /* EDSK: one track, R1 ST1/ST2 0/0, R2 ST2 0x20 (DD), R3 ST1 0x20 only (DE) */
    {
        static uint8_t b[0x100 + 0x100 + 3 * 512];
        memset(b, 0, sizeof b);
        memcpy(b, "EXTENDED CPC DSK File\r\nDisk-Info\r\n", 34);
        memcpy(b + 0x22, "MF-1488       ", 14);
        b[0x30] = 1; b[0x31] = 1;
        b[0x34] = (uint8_t)((0x100 + 3 * 512) / 256);
        uint8_t *t = b + 0x100;
        memcpy(t, "Track-Info\r\n", 12);
        t[0x10] = 0; t[0x11] = 0; t[0x14] = 2; t[0x15] = 3; t[0x16] = 0x4E; t[0x17] = 0xE5;
        const uint8_t st1[3] = { 0x00, 0x00, 0x20 }, st2[3] = { 0x00, 0x20, 0x00 };
        for (int i = 0; i < 3; i++) {
            uint8_t *si = t + 0x18 + 8 * i;
            si[0] = 0; si[1] = 0; si[2] = (uint8_t)(i + 1); si[3] = 2;
            si[4] = st1[i]; si[5] = st2[i]; si[6] = 0x00; si[7] = 0x02;
            memset(t + 0x100 + 512 * i, 0x20 + i, 512);
        }
        snprintf(pfad, sizeof pfad, "%s/uft_mf1488.dsk", tmpdir());
        schreibe(pfad, b, sizeof b);
        urteile_mit(&uft_format_plugin_dsk_cpc, pfad, r123, urteil, 3);
        remove(pfad);
        snprintf(h, sizeof h, "Modell R1,R2,R3 = %s", urteil);
        pruefe("EDSK ST2 DD -> Daten-CRC-Fehler, ST1 DE allein -> ID-CRC-Fehler (uPD765)",
               urteil[0] == 'U' && urteil[1] == 'B' && urteil[2] == 'I', h);
    }

    /* D64 with error bytes — second slice (MF-1490). Peter Schepers,
     * D64.TXT, error byte table (read 2026-09-28):
     *   01 = 00 no error          05 = 23 checksum error in data block
     *   09 = 27 checksum error in header block
     *   02 = 20 header descriptor byte not found  (seek)
     *   03 = 21 no SYNC,  04 = 22 data descriptor byte not found,
     *   0F = 74 drive not ready  — the sector could not be read at all
     *   0B = 29 disk sector ID mismatch — no CRC statement
     * Track 1 sectors 0..5 carry 01, 05, 09, 02, 04, 0B. */
    {
        enum { D64 = 174848, ERR = 683 };
        static uint8_t b[D64 + ERR];
        memset(b, 0, sizeof b);
        for (int s = 0; s < 21; s++) memset(b + 256 * s, 0x30 + s, 256);
        memset(b + D64, 0x01, ERR);
        const uint8_t code[6] = { 0x01, 0x05, 0x09, 0x02, 0x04, 0x0B };
        memcpy(b + D64, code, 6);
        const uint8_t r012345[6] = { 0, 1, 2, 3, 4, 5 };
        snprintf(pfad, sizeof pfad, "%s/uft_mf1490.d64", tmpdir());
        schreibe(pfad, b, sizeof b);
        char u6[8];
        urteile_mit(&uft_format_plugin_d64, pfad, r012345, u6, 6);
        remove(pfad);
        snprintf(h, sizeof h, "Modell S0..S5 (01,05,09,02,04,0B) = %s (M=nicht lesbar)", u6);
        pruefe("D64: 05 Daten-CRC, 09 Kopf-CRC, 02/04 nicht lesbar, 01 und 0B ohne CRC-Befund (Schepers D64.TXT)",
               strcmp(u6, "UBIMMU") == 0, h);
    }

    /* NFD r0 (PC-98, uPD765 status) — second slice (MF-1490): R1 ST2 bit 5
     * (data CRC), R2 ST1 bit 5 without ST2 (ID CRC), R3 clean, R4 whose data
     * lies past the end of the file (truncated: no CRC statement, the source
     * ended). */
    {
        enum { TAB = 163 * 26 * 16, KOPF = 0x120 + TAB };
        static uint8_t b[KOPF + 3 * 256];
        memset(b, 0, sizeof b);
        memcpy(b, "T98FDDIMAGE.R0", 14);
        b[0x110] = (uint8_t)KOPF; b[0x111] = (uint8_t)(KOPF >> 8);
        b[0x112] = (uint8_t)(KOPF >> 16);
        b[0x115] = 1;
        for (int slot = 0; slot < 163 * 26; slot++) b[0x120 + 16 * slot] = 0xFF;
        const uint8_t st1[4] = { 0x00, 0x20, 0x00, 0x00 }, st2[4] = { 0x20, 0x00, 0x00, 0x00 };
        for (int i = 0; i < 4; i++) {
            uint8_t *e = b + 0x120 + 16 * i;
            e[0] = 0; e[1] = 0; e[2] = (uint8_t)(i + 1); e[3] = 1;
            e[4] = 1; e[8] = st1[i]; e[9] = st2[i]; e[10] = 0x90;
        }
        for (int i = 0; i < 3; i++) memset(b + KOPF + 256 * i, 0x40 + i, 256);
        const uint8_t r1234[4] = { 1, 2, 3, 4 };
        snprintf(pfad, sizeof pfad, "%s/uft_mf1490.nfd", tmpdir());
        schreibe(pfad, b, sizeof b);
        char u4[8];
        urteile_mit(&uft_format_plugin_nfd, pfad, r1234, u4, 4);
        remove(pfad);
        snprintf(h, sizeof h, "Modell R1..R4 = %s", u4);
        pruefe("NFD: ST2 -> Daten-CRC, ST1 allein -> ID-CRC, abgeschnitten -> fehlt (uPD765)",
               strcmp(u4, "BIUM") == 0, h);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

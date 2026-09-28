/* ST probe: a PC boot sector does not score as an Atari ST disk.
 *
 * st_probe() (src/formats/st/uft_st.c) returned 75 as soon as the BPB
 * described the file — before its own x86 check ("x86 JMP: this is far
 * more likely a PC image of the same size") could run. That branch was
 * unreachable for every PC image whose BPB is correct, i.e. for every
 * normal one. docs/SONDEN_DOKTRIN.md caps a claim without a format-specific
 * marker at 45; a PC 720K image got 75 from the ST probe.
 *
 * The sector below is a DOS 5.0 720K boot sector as FORMAT writes it
 * (EB 3C 90, OEM "MSDOS5.0", BPB 512/2/1/2/112/1440/F9/3/9/2, 55 AA) —
 * the layout is the one Microsoft documents for the BIOS parameter block;
 * no foreign file is used. The Atari control is the same BPB with a 68000
 * BRA.S (0x60) in front, which must keep its value.
 *
 * Found by the uft-st-code package review (neue-ideen, t3), P3-653,
 * MF-1478.
 */
#include "uft/uft_format_plugin.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_st;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

static void bpb_720k(uint8_t *b)
{
    b[0x0B] = 0x00; b[0x0C] = 0x02;          /* 512 bytes per sector */
    b[0x0D] = 2;                             /* sectors per cluster  */
    b[0x0E] = 1; b[0x0F] = 0;                /* reserved             */
    b[0x10] = 2;                             /* FATs                 */
    b[0x11] = 112; b[0x12] = 0;              /* root entries         */
    b[0x13] = 0xA0; b[0x14] = 0x05;          /* 1440 sectors         */
    b[0x15] = 0xF9;                          /* media                */
    b[0x16] = 3; b[0x17] = 0;                /* sectors per FAT      */
    b[0x18] = 9; b[0x19] = 0;                /* sectors per track    */
    b[0x1A] = 2; b[0x1B] = 0;                /* heads                */
}

int main(void)
{
    uint8_t b[512];
    int k = -1;
    char h[120];
    const size_t groesse = 737280;

    printf("ST-Sonde: ein PC-Bootsektor ist kein ST-Beleg (MF-1478)\n");

    memset(b, 0, sizeof b);
    b[0] = 0xEB; b[1] = 0x3C; b[2] = 0x90;
    memcpy(b + 3, "MSDOS5.0", 8);
    bpb_720k(b);
    b[510] = 0x55; b[511] = 0xAA;
    bool ja = uft_format_plugin_st.probe(b, sizeof b, groesse, &k);
    snprintf(h, sizeof h, "probe=%d, Konfidenz %d", (int)ja, k);
    pruefe("PC-720K-Bootsektor: ST-Sonde hoechstens 45 (Doktrin ohne Kennung)",
           !ja || k <= 45, h);

    memset(b, 0, sizeof b);
    b[0] = 0x60; b[1] = 0x1C;
    bpb_720k(b);
    k = -1;
    ja = uft_format_plugin_st.probe(b, sizeof b, groesse, &k);
    snprintf(h, sizeof h, "probe=%d, Konfidenz %d", (int)ja, k);
    pruefe("Kontrolle: derselbe BPB hinter BRA.S behaelt 75", ja && k == 75, h);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

/**
 * @file uft_td0.c
 * @brief Teledisk (TD0) Format Implementation for UFT
 * 
 * Based on reverse-engineering work by various authors, Will Krantz,
 * 
 * @copyright UFT Project
 */

#include "uft_td0.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Huffman Decode Tables (from Teledisk reverse-engineering)
 *============================================================================*/

const uint8_t uft_td0_d_code[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
    0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
    0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D,
    0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F,
    0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11,
    0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13,
    0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15,
    0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
    0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1B,
    0x1C, 0x1C, 0x1D, 0x1D, 0x1E, 0x1E, 0x1F, 0x1F,
    0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23,
    0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27,
    0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B,
    0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

const uint8_t uft_td0_d_len[16] = {
    2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7
};

/*============================================================================
 * LZSS-Huffman Decompression
 *============================================================================*/

void uft_td0_lzss_init(uft_td0_lzss_state_t* state,
                       const uint8_t* data, size_t size)
{
    if (!state) return;
    
    memset(state, 0, sizeof(*state));
    state->input = data;
    state->input_size = size;
    state->input_pos = 0;
    state->eof = false;
    
    /* Initialize Huffman tree */
    unsigned i, j;
    for (i = j = 0; i < UFT_TD0_LZSS_N_CHAR; i++) {
        state->freq[i] = 1;
        state->son[i] = i + UFT_TD0_LZSS_TSIZE;
        state->parent[i + UFT_TD0_LZSS_TSIZE] = i;
    }
    
    while (i <= UFT_TD0_LZSS_ROOT) {
        state->freq[i] = state->freq[j] + state->freq[j + 1];
        state->son[i] = j;
        state->parent[j] = state->parent[j + 1] = i++;
        j += 2;
    }
    
    /* Initialize ring buffer with spaces */
    memset(state->ring_buff, ' ', sizeof(state->ring_buff));
    
    state->freq[UFT_TD0_LZSS_TSIZE] = 0xFFFF;
    state->parent[UFT_TD0_LZSS_ROOT] = 0;
    state->bitbuff = 0;
    state->bits = 0;
    state->r = UFT_TD0_LZSS_SBSIZE - UFT_TD0_LZSS_LASIZE;
    state->state = 0;
}

/* Get character from input */
static unsigned lzss_getchar(uft_td0_lzss_state_t* state)
{
    if (state->input_pos >= state->input_size) {
        state->eof = true;
        return 0;
    }
    return state->input[state->input_pos++];
}

/* Get single bit from input stream */
static unsigned lzss_getbit(uft_td0_lzss_state_t* state)
{
    unsigned t;
    
    if (state->bits == 0) {
        state->bitbuff |= lzss_getchar(state) << 8;
        state->bits = 8;
    }
    state->bits--;
    
    t = state->bitbuff >> 15;
    state->bitbuff <<= 1;
    return t;
}

/* Get byte from input stream (not bit-aligned) */
static unsigned lzss_getbyte(uft_td0_lzss_state_t* state)
{
    unsigned t;
    
    if (state->bits < 8) {
        state->bitbuff |= lzss_getchar(state) << (8 - state->bits);
    } else {
        state->bits -= 8;
    }
    
    t = state->bitbuff >> 8;
    state->bitbuff <<= 8;
    return t;
}

/* Update Huffman tree */
static void lzss_update(uft_td0_lzss_state_t* state, int c)
{
    unsigned i, j, k, f, l;
    
    if (state->freq[UFT_TD0_LZSS_ROOT] == UFT_TD0_LZSS_MAX_FREQ) {
        /* Tree is full - rebuild */
        for (i = j = 0; i < UFT_TD0_LZSS_TSIZE; i++) {
            if (state->son[i] >= UFT_TD0_LZSS_TSIZE) {
                state->freq[j] = (state->freq[i] + 1) / 2;
                state->son[j] = state->son[i];
                j++;
            }
        }
        
        for (i = 0, j = UFT_TD0_LZSS_N_CHAR; j < UFT_TD0_LZSS_TSIZE; i += 2, j++) {
            k = i + 1;
            f = state->freq[j] = state->freq[i] + state->freq[k];
            for (k = j - 1; f < state->freq[k]; k--);
            k++;
            l = (j - k) * sizeof(state->freq[0]);
            
            memmove(&state->freq[k + 1], &state->freq[k], l);
            state->freq[k] = f;
            memmove(&state->son[k + 1], &state->son[k], l);
            state->son[k] = i;
        }
        
        for (i = 0; i < UFT_TD0_LZSS_TSIZE; i++) {
            if ((k = state->son[i]) >= UFT_TD0_LZSS_TSIZE) {
                state->parent[k] = i;
            } else {
                state->parent[k] = state->parent[k + 1] = i;
            }
        }
    }
    
    c = state->parent[c + UFT_TD0_LZSS_TSIZE];
    do {
        k = ++state->freq[c];
        if (k > state->freq[l = c + 1]) {
            while (k > state->freq[++l]);
            state->freq[c] = state->freq[--l];
            state->freq[l] = k;
            state->parent[i = state->son[c]] = l;
            if (i < UFT_TD0_LZSS_TSIZE)
                state->parent[i + 1] = l;
            state->parent[j = state->son[l]] = c;
            state->son[l] = i;
            if (j < UFT_TD0_LZSS_TSIZE)
                state->parent[j + 1] = c;
            state->son[c] = j;
            c = l;
        }
    } while ((c = state->parent[c]) != 0);
}

/* Decode character from Huffman tree */
static unsigned lzss_decode_char(uft_td0_lzss_state_t* state)
{
    unsigned c = UFT_TD0_LZSS_ROOT;
    
    while ((c = state->son[c]) < UFT_TD0_LZSS_TSIZE) {
        c += lzss_getbit(state);
    }
    
    lzss_update(state, c -= UFT_TD0_LZSS_TSIZE);
    return c;
}

/* Decode position from input */
static unsigned lzss_decode_position(uft_td0_lzss_state_t* state)
{
    unsigned i, j, c;
    
    i = lzss_getbyte(state);
    c = (unsigned)uft_td0_d_code[i] << 6;
    
    j = uft_td0_d_len[i >> 4];
    while (--j) {
        i = (i << 1) | lzss_getbit(state);
    }
    
    return (i & 0x3F) | c;
}

int uft_td0_lzss_getbyte(uft_td0_lzss_state_t* state)
{
    unsigned c;
    
    if (!state) return -1;
    
    for (;;) {
        if (state->eof) return -1;
        
        if (state->state == 0) {
            /* Not in the middle of a string */
            c = lzss_decode_char(state);
            if (c < 256) {
                /* Direct data extraction */
                state->ring_buff[state->r++] = c;
                state->r &= (UFT_TD0_LZSS_SBSIZE - 1);
                return c;
            }
            /* Begin extracting compressed string */
            state->state = 1;
            state->i = (state->r - lzss_decode_position(state) - 1) & 
                       (UFT_TD0_LZSS_SBSIZE - 1);
            state->j = c - 255 + UFT_TD0_LZSS_THRESHOLD;
            state->k = 0;
        }
        
        if (state->k < state->j) {
            /* Extract compressed string */
            c = state->ring_buff[(state->k++ + state->i) & (UFT_TD0_LZSS_SBSIZE - 1)];
            state->ring_buff[state->r++] = c;
            state->r &= (UFT_TD0_LZSS_SBSIZE - 1);
            return c;
        }
        
        state->state = 0;
    }
}

size_t uft_td0_lzss_read(uft_td0_lzss_state_t* state,
                         uint8_t* buffer, size_t size)
{
    size_t count = 0;
    int c;
    
    while (count < size) {
        c = uft_td0_lzss_getbyte(state);
        if (c < 0) break;
        buffer[count++] = (uint8_t)c;
    }
    
    return count;
}

/*============================================================================
 * TD0 Detection and Initialization
 *============================================================================*/

/*============================================================================
 * Sector Data Decoding
 *============================================================================*/

/*============================================================================
 * Drive Type Names
 *============================================================================*/

const char* uft_td0_drive_name(uft_td0_drive_t type)
{
    switch (type) {
        case UFT_TD0_DRIVE_525_96:  return "5.25\" 96 TPI (1.2MB)";
        case UFT_TD0_DRIVE_525_48:  return "5.25\" 48 TPI (360K)";
        case UFT_TD0_DRIVE_35_HD:   return "3.5\" HD";
        case UFT_TD0_DRIVE_35_DD:   return "3.5\" DD";
        case UFT_TD0_DRIVE_8INCH:   return "8\"";
        case UFT_TD0_DRIVE_35_ED:   return "3.5\" ED";
        default:                    return "Unknown";
    }
}

/*============================================================================
 * TD0 Reading
 *============================================================================*/

/*============================================================================
 * Information Display
 *============================================================================*/

/* MF-1287: hier standen acht Funktionen — `uft_td0_read_mem()` und was
 * an ihm hing (`_init`, `_free`, `_decode_sector`, `_read`,
 * `_print_info`, `_detect`, `_is_compressed`).
 *
 * Sie waren der ZWEITE Leser fuer TD0. Er konnte etwas, das dem Plugin
 * fehlte — entpacken und den Kommentarblock lesen —, und er machte
 * dabei einen Fehler, den das Plugin nicht hatte: sein `READ_BLOCK`
 * brach am Dateiende nur die INNERE Schleife ab, liess den Spurkopf
 * unberuehrt und las ihn uninitialisiert weiter. Die 0xFF-Endmarke
 * wurde deshalb nie erreicht; an einer 1440-Sektoren-Diskette kamen
 * 51 830 Sektoren auf 256 Spuren heraus.
 *
 * Repariert wurde er nicht — er wurde geloescht. Was er KONNTE, ist seit
 * MF-1285 im Strom-Kern (`uft_td0_strom_*`), und der ist von BEIDEN
 * Seiten erreichbar: vom Plugin ueber `uft_disk_open()` und von den
 * Wandlern ueber die blossen Bytes. Zwei Leser fuer ein Format waren
 * der Fehler, nicht der Muell.
 *
 * Was hier BLEIBT, ist der LZH-Entpacker — `uft_td0_lzss_init()`,
 * `uft_td0_lzss_getbyte()` und ihre Helfer. Der
 * Vollstaendigkeit halber: `uft_td0_lzss_read()` steht noch da und hat
 * NULL Aufrufer — schon vor diesem Commit, denn `read_mem` nahm
 * `getbyte`. Eine vorbestehende Waise; sie wird hier BENANNT und nicht
 * mitgeloescht, weil Loeschen eine Eigentuemerentscheidung ist
 * (§MF-1077). Der
 * war richtig, gemessen gegen hxcfe an zwei gepackten Dateien, und er
 * haette den Schnitt nicht ueberlebt: ausserhalb dieser Datei hatte er
 * VOR MF-1285 null Aufrufer. */

#include "uft_c64_prg_analyzer.h"
#include <string.h>
#include <ctype.h>

static uint16_t rd_u16_le(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }

uft_prg_status_t uft_prg_parse(const uint8_t *blob, size_t blob_len, uft_prg_view_t *out) {
    if (!out) return UFT_PRG_E_INVALID;
    memset(out, 0, sizeof(*out));
    if (!blob) return UFT_PRG_E_INVALID;
    if (blob_len < 2) return UFT_PRG_E_TOO_SMALL;
    out->load_address = rd_u16_le(blob);
    out->data = blob + 2;
    out->data_len = blob_len - 2;
    return UFT_PRG_OK;
}

static int is_printable(uint8_t b, char *out_ch) {
    if (b == 0x0D) { *out_ch = '\n'; return 1; }
    if (b >= 0x20 && b <= 0x7E) { *out_ch = (char)b; return 1; }
    return 0;
}

static uint32_t visible_len(const char *s) {
    uint32_t n = 0;
    for (; *s; s++) if (*s != '\n') n++;
    return n;
}

uft_prg_status_t uft_prg_extract_strings(const uft_prg_view_t *prg,
                                         uft_prg_string_t *out_strings, size_t max_strings,
                                         char *text_buf, size_t text_cap,
                                         uint32_t min_visible_len,
                                         size_t *out_count, size_t *out_text_used)
{
    if (out_count) *out_count = 0;
    if (out_text_used) *out_text_used = 0;
    if (!prg || !out_strings || !text_buf || text_cap == 0) return UFT_PRG_E_INVALID;

    size_t sc = 0, tp = 0;
    size_t run_start = (size_t)-1, run_tp = 0;

    for (size_t i = 0; i < prg->data_len; i++) {
        char ch = 0;
        if (is_printable(prg->data[i], &ch)) {
            if (run_start == (size_t)-1) { run_start = i; run_tp = tp; }
            if (tp + 2 > text_cap) return UFT_PRG_E_BUF;
            text_buf[tp++] = ch;
        } else {
            if (run_start != (size_t)-1) {
                text_buf[tp++] = '\0';
                const char *s = &text_buf[run_tp];
                if (visible_len(s) >= min_visible_len) {
                    if (sc >= max_strings) return UFT_PRG_E_BUF;
                    out_strings[sc].offset = (uint32_t)run_start;
                    out_strings[sc].length = (uint32_t)strlen(s);
                    out_strings[sc].text = s;
                    sc++;
                }
                run_start = (size_t)-1;
            }
        }
    }

    if (run_start != (size_t)-1) {
        if (tp + 1 > text_cap) return UFT_PRG_E_BUF;
        text_buf[tp++] = '\0';
        const char *s = &text_buf[run_tp];
        if (visible_len(s) >= min_visible_len) {
            if (sc >= max_strings) return UFT_PRG_E_BUF;
            out_strings[sc].offset = (uint32_t)run_start;
            out_strings[sc].length = (uint32_t)strlen(s);
            out_strings[sc].text = s;
            sc++;
        }
    }

    if (out_count) *out_count = sc;
    if (out_text_used) *out_text_used = tp;
    return UFT_PRG_OK;
}

static int ci_contains(const char *hay, const char *needle) {
    if (!hay || !needle || !*needle) return 0;
    size_t hn = strlen(hay), nn = strlen(needle);
    if (nn > hn) return 0;
    for (size_t i = 0; i + nn <= hn; i++) {
        size_t j = 0;
        for (; j < nn; j++) {
            unsigned char a = (unsigned char)hay[i + j];
            unsigned char b = (unsigned char)needle[j];
            if ((char)tolower(a) != (char)tolower(b)) break;
        }
        if (j == nn) return 1;
    }
    return 0;
}

void uft_prg_score_floppy_keywords(const uft_prg_string_t *strings, size_t count, uft_prg_score_t *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!strings) return;

    for (size_t i = 0; i < count; i++) {
        const char *s = strings[i].text;
        if (!s) continue;

        out->nib      += (uint32_t)(ci_contains(s, "NIB") || ci_contains(s, "NIBBLE") || ci_contains(s, "NIBBLER"));
        out->burst    += (uint32_t)ci_contains(s, "BURST");
        out->gcr      += (uint32_t)ci_contains(s, "GCR");
        out->sync     += (uint32_t)ci_contains(s, "SYNC");
        out->track    += (uint32_t)ci_contains(s, "TRACK");
        out->sector   += (uint32_t)ci_contains(s, "SECTOR");
        out->disk     += (uint32_t)ci_contains(s, "DISK");
        out->drive    += (uint32_t)ci_contains(s, "DRIVE");
        out->dev1541  += (uint32_t)ci_contains(s, "1541");
        out->dev1571  += (uint32_t)ci_contains(s, "1571");
        out->dev1581  += (uint32_t)ci_contains(s, "1581");
        out->copy     += (uint32_t)ci_contains(s, "COPY");
        out->rapid    += (uint32_t)ci_contains(s, "RAPID");
        out->fast     += (uint32_t)ci_contains(s, "FAST");
        out->turbo    += (uint32_t)ci_contains(s, "TURBO");
        out->verify   += (uint32_t)ci_contains(s, "VERIFY");
        out->retry    += (uint32_t)(ci_contains(s, "RETRY") || ci_contains(s, "ATTEMPT"));
        out->error    += (uint32_t)ci_contains(s, "ERROR");
        out->check    += (uint32_t)ci_contains(s, "CHECK");
        out->crc      += (uint32_t)ci_contains(s, "CRC");
        out->checksum += (uint32_t)ci_contains(s, "CHECKSUM");
        out->protect  += (uint32_t)(ci_contains(s, "PROTECT") || ci_contains(s, "PROTECTION"));
        out->weak     += (uint32_t)ci_contains(s, "WEAK");
        out->bits     += (uint32_t)ci_contains(s, "BITS");
        out->density  += (uint32_t)ci_contains(s, "DENSITY");
        out->align    += (uint32_t)ci_contains(s, "ALIGN");
        out->speed    += (uint32_t)ci_contains(s, "SPEED");
        out->head     += (uint32_t)ci_contains(s, "HEAD");
        out->step     += (uint32_t)ci_contains(s, "STEP");
        out->read     += (uint32_t)ci_contains(s, "READ");
        out->write    += (uint32_t)ci_contains(s, "WRITE");
        out->gap      += (uint32_t)ci_contains(s, "GAP");
        out->backup   += (uint32_t)ci_contains(s, "BACKUP");
        out->format   += (uint32_t)ci_contains(s, "FORMAT");
        out->bam      += (uint32_t)ci_contains(s, "BAM");
        out->directory += (uint32_t)ci_contains(s, "DIRECTORY");
    }
}
